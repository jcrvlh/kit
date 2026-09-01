#include "kit_audio.h"
#include "kit_power.h"
#include "kit_config.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/i2s_std.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include <math.h>

static const char *TAG = "KIT_AUDIO";
static uint8_t s_volume = 80;

static i2s_chan_handle_t s_tx_handle = NULL;
static esp_codec_dev_handle_t s_speaker = NULL;

#define AUDIO_SAMPLE_RATE   16000
#define AUDIO_FRAME_COUNT   256
#define AUDIO_TWO_PI        6.28318530717958647692f

// O som roda numa task própria: escrever no I2S é síncrono (bloqueia até o
// codec drenar o buffer) e, chamado direto do callback do LVGL, travava a task
// `main` (a rolagem da Dice Tool, a navegação do Launcher). kit_audio_beep_impl
// e kit_audio_sfx_impl só enfileiram e voltam na hora; audio_task renderiza.
typedef struct {
    int16_t  sfx;         // <0 = tom puro (usa freq/dur); >=0 = kit_sfx_t
    uint16_t freq_hz;
    uint16_t duration_ms;
} kit_beep_req_t;

static QueueHandle_t s_beep_queue = NULL;

// Amplitude "cheia" de um tom (~73% do fundo de escala 16-bit).
#define AUDIO_AMP_FULL   12000.0f

static void render_tone(uint16_t freq_hz, uint16_t duration_ms, float amp)
{
    if (!s_speaker || duration_ms == 0) return;

    int16_t samples[AUDIO_FRAME_COUNT];
    float phase = 0.0f;
    float step = AUDIO_TWO_PI * (float)freq_hz / (float)AUDIO_SAMPLE_RATE;

    uint32_t total_samples = (AUDIO_SAMPLE_RATE * (uint32_t)duration_ms) / 1000;
    uint32_t sent = 0;
    int write_fails = 0;
    int last_err = ESP_CODEC_DEV_OK;
    while (sent < total_samples) {
        uint32_t remaining = total_samples - sent;
        int chunk = remaining < AUDIO_FRAME_COUNT ? (int)remaining : AUDIO_FRAME_COUNT;
        for (int i = 0; i < chunk; i++) {
            // Envelope curto nas pontas (~2 ms) para não estalar o alto-falante.
            float env = 1.0f;
            uint32_t idx = sent + (uint32_t)i;
            const uint32_t ramp = AUDIO_SAMPLE_RATE / 500; // ~2 ms
            if (idx < ramp)                      env = (float)idx / (float)ramp;
            else if (idx > total_samples - ramp) env = (float)(total_samples - idx) / (float)ramp;
            samples[i] = (int16_t)(sinf(phase) * amp * env);
            phase += step;
            if (phase >= AUDIO_TWO_PI) {
                phase -= AUDIO_TWO_PI;
            }
        }
        int w = esp_codec_dev_write(s_speaker, samples, (size_t)chunk * sizeof(int16_t));
        if (w != ESP_CODEC_DEV_OK) {
            write_fails++;
            last_err = w;
        }
        sent += (uint32_t)chunk;
    }
    if (write_fails > 0) {
        ESP_LOGW(TAG, "render_tone: %d escrita(s) I2S falharam (último código %d)",
                 write_fails, last_err);
    }
}

// Silêncio ativo entre os "ticks" de um SFX: alimenta zeros para o DMA não
// esvaziar (evita estalos) e mantém a temporização firme.
static void render_silence(uint16_t duration_ms)
{
    if (!s_speaker || duration_ms == 0) return;
    static const int16_t zeros[AUDIO_FRAME_COUNT] = { 0 };
    uint32_t total = (AUDIO_SAMPLE_RATE * (uint32_t)duration_ms) / 1000;
    uint32_t sent = 0;
    while (sent < total) {
        uint32_t remaining = total - sent;
        int chunk = remaining < AUDIO_FRAME_COUNT ? (int)remaining : AUDIO_FRAME_COUNT;
        esp_codec_dev_write(s_speaker, (void *)zeros, (size_t)chunk * sizeof(int16_t));
        sent += (uint32_t)chunk;
    }
}

static inline uint32_t rnd(uint32_t n) { return esp_random() % n; }

static void render_sfx(kit_sfx_t sfx)
{
    switch (sfx) {
    case KIT_SFX_CLICK:
        render_tone(2400, 8,  6500.0f);
        render_tone(1500, 12, 4500.0f);
        break;

    case KIT_SFX_BACK:
        render_tone(1300, 12, 6500.0f);
        render_tone(850,  16, 5000.0f);
        break;

    case KIT_SFX_CONFIRM:
        render_tone(1200, 45, 10000.0f);
        render_tone(1800, 65, 10000.0f);
        break;

    case KIT_SFX_DICE_ROLL: {
        // "Tombo": ~8 batidinhas de tom variável, desacelerando, e um assento.
        static const uint16_t base[] = { 900, 1300, 780, 1500, 1000, 720, 1250, 940 };
        for (int i = 0; i < 8; i++) {
            render_tone((uint16_t)(base[i] + rnd(140)), 22, 7000.0f);
            render_silence((uint16_t)(14 + i * 5));
        }
        render_tone(1150, 60, 11000.0f);
        break;
    }

    case KIT_SFX_ROULETTE: {
        // Catraca desacelerando: o intervalo entre "ticks" cresce em ease-out.
        int gap = 22;
        for (int i = 0; i < 24; i++) {
            render_tone(2600, 6, 5500.0f);
            render_silence((uint16_t)gap);
            gap += 3 + i;
            if (gap > 175) gap = 175;
        }
        render_tone(680, 75, 9500.0f);   // parou
        break;
    }

    case KIT_SFX_COIN: {
        // Moeda girando no ar: tremular rápido entre dois tons, depois um "ding".
        for (int i = 0; i < 12; i++) {
            render_tone((i & 1) ? 1650 : 2150, 14, 5500.0f);
            render_silence((uint16_t)(6 + i));
        }
        render_tone(2637, 110, 11000.0f);   // E7
        render_tone(3520, 70,  9000.0f);    // A7
        break;
    }

    case KIT_SFX_TIMER_DONE:
        // Alarme: três batidas de dois tons e um toque final mais longo.
        for (int i = 0; i < 3; i++) {
            render_tone(1200, 90, 12000.0f);
            render_tone(1650, 90, 12000.0f);
            render_silence(70);
        }
        render_tone(1650, 220, 12000.0f);
        break;

    case KIT_SFX_REVEAL:
        // Duas notas subindo, macio.
        render_tone(880,  70,  8500.0f);
        render_tone(1320, 120, 9500.0f);
        break;

    case KIT_SFX_BINGO_BALL: {
        // Chocalho: ~14 estalos curtos de tom aleatório (bolinhas quicando)...
        for (int i = 0; i < 14; i++) {
            render_tone((uint16_t)(1200 + rnd(1400)), 12, 6000.0f);
            render_silence((uint16_t)(8 + rnd(22)));
        }
        render_tone(1568, 150, 11000.0f);   // ...e a bolinha saindo (G6)
        break;
    }

    default:
        break;
    }
}

static void audio_task(void *arg)
{
    (void)arg;
    kit_beep_req_t req;
    while (1) {
        if (xQueueReceive(s_beep_queue, &req, portMAX_DELAY) == pdTRUE) {
            if (req.sfx < 0) {
                // Bipes muito curtos (<= 14 ms) são "ticks" de textura — saem
                // mais baixos pra não estourar quando disparados em rajada
                // (catraca da Garrafa, animação de sorteio).
                float amp = (req.duration_ms <= 14) ? AUDIO_AMP_FULL * 0.34f
                                                    : AUDIO_AMP_FULL;
                render_tone(req.freq_hz, req.duration_ms, amp);
            } else {
                render_sfx((kit_sfx_t)req.sfx);
            }
        }
    }
}

kit_err_t kit_audio_init(void)
{
    ESP_LOGI(TAG, "Inicializando subsistema de áudio (ES8311 I2S + Speaker PA)...");

    // Sondagem de diagnóstico: confirma que o codec responde no barramento
    // compartilhado antes de montar o pipeline I2S completo.
    uint8_t chip_id = 0;
    if (kit_i2c_read_reg(ES8311_I2C_ADDR, 0x00, &chip_id) == ESP_OK) {
        ESP_LOGI(TAG, "Codec ES8311 detectado (Chip ID: 0x%02X)", chip_id);
    } else {
        ESP_LOGW(TAG, "ES8311 não respondeu em 0x18.");
    }

    // 1. Canal I2S mestre: o ESP32-S3 gera MCLK/BCLK/WS e o ES8311 opera como escravo.
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    esp_err_t err = i2s_new_channel(&chan_cfg, &s_tx_handle, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao criar canal I2S: %s", esp_err_to_name(err));
        return KIT_FAIL;
    }

    i2s_std_config_t i2s_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = KIT_I2S_MCK_PIN,
            .bclk = KIT_I2S_BCK_PIN,
            .ws = KIT_I2S_WS_PIN,
            .dout = KIT_I2S_DO_PIN,
            .din = KIT_I2S_DI_PIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    err = i2s_channel_init_std_mode(s_tx_handle, &i2s_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao configurar I2S std mode: %s", esp_err_to_name(err));
        return KIT_FAIL;
    }

    // 2. Interface de dados I2S exposta ao framework de codec.
    audio_codec_i2s_cfg_t codec_i2s_cfg = {
        .port = I2S_NUM_0,
        .tx_handle = s_tx_handle,
        .rx_handle = NULL,
    };
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&codec_i2s_cfg);
    if (!data_if) {
        ESP_LOGE(TAG, "Falha ao criar interface de dados I2S do codec");
        return KIT_FAIL;
    }

    // 3. Interface de controle I2C do ES8311 (reaproveita o barramento
    // i2c_master já instalado e compartilhado por kit_power).
    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = KIT_I2C_PORT,
        .addr = ES8311_CODEC_DEFAULT_ADDR,
        .bus_handle = kit_power_get_i2c_bus_handle(),
    };
    const audio_codec_ctrl_if_t *i2c_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    if (!i2c_ctrl_if) {
        ESP_LOGE(TAG, "Falha ao criar interface de controle I2C do codec");
        return KIT_FAIL;
    }

    // 4. Driver do ES8311 em modo DAC (só reprodução). O próprio driver
    // controla o pino PA_EN (amplificador) conforme o estado de mute/abertura.
    esp_codec_dev_hw_gain_t gain = {
        .pa_voltage = 5.0,
        .codec_dac_voltage = 3.3,
    };
    es8311_codec_cfg_t es8311_cfg = {
        .ctrl_if = i2c_ctrl_if,
        .gpio_if = gpio_if,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
        .pa_pin = KIT_AUDIO_PA_PIN,
        .pa_reverted = false,
        .master_mode = false,
        .use_mclk = true,
        .digital_mic = false,
        .invert_mclk = false,
        .invert_sclk = false,
        .hw_gain = gain,
    };
    const audio_codec_if_t *es8311_dev = es8311_codec_new(&es8311_cfg);
    if (!es8311_dev) {
        ESP_LOGE(TAG, "Falha ao criar driver do codec ES8311");
        return KIT_FAIL;
    }

    esp_codec_dev_cfg_t codec_dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = es8311_dev,
        .data_if = data_if,
    };
    s_speaker = esp_codec_dev_new(&codec_dev_cfg);
    if (!s_speaker) {
        ESP_LOGE(TAG, "Falha ao criar dispositivo de reprodução");
        return KIT_FAIL;
    }

    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 16,
        .channel = 1,
        .channel_mask = 0,
        .sample_rate = AUDIO_SAMPLE_RATE,
        .mclk_multiple = 256,
    };
    int oret = esp_codec_dev_open(s_speaker, &fs);
    if (oret != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "Falha ao abrir dispositivo de reprodução (código %d)", oret);
        return KIT_FAIL;
    }
    s_volume = kit_config_get_volume();   // volume salvo (kit_config_init roda antes)
    int vret = esp_codec_dev_set_out_vol(s_speaker, s_volume);
    if (vret != ESP_CODEC_DEV_OK) {
        ESP_LOGW(TAG, "esp_codec_dev_set_out_vol retornou %d", vret);
    }

    // Task + fila do bipe assíncrono.
    s_beep_queue = xQueueCreate(6, sizeof(kit_beep_req_t));
    if (!s_beep_queue ||
        xTaskCreate(audio_task, "kit_audio", 4096, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Falha ao criar a task de áudio");
        return KIT_FAIL;
    }

    ESP_LOGI(TAG, "Codec ES8311 e I2S prontos para reprodução (vol %d%%).", s_volume);
    return KIT_OK;
}

kit_err_t kit_audio_beep_impl(uint16_t freq_hz, uint16_t duration_ms)
{
    if (!s_speaker || !s_beep_queue) {
        ESP_LOGW(TAG, "Áudio não inicializado, ignorando bipe.");
        return KIT_FAIL;
    }
    if (!kit_config_get_sound_enabled()) return KIT_OK;

    kit_beep_req_t req = { .sfx = -1, .freq_hz = freq_hz, .duration_ms = duration_ms };
    // Não espera: se a fila estiver cheia (bipes muito seguidos), descarta.
    if (xQueueSend(s_beep_queue, &req, 0) != pdTRUE) {
        return KIT_FAIL;
    }
    return KIT_OK;
}

kit_err_t kit_audio_sfx_impl(kit_sfx_t sfx)
{
    if (!s_speaker || !s_beep_queue) return KIT_FAIL;
    if (!kit_config_get_sound_enabled()) return KIT_OK;

    kit_beep_req_t req = { .sfx = (int16_t)sfx, .freq_hz = 0, .duration_ms = 0 };
    if (xQueueSend(s_beep_queue, &req, 0) != pdTRUE) {
        return KIT_FAIL;
    }
    return KIT_OK;
}

kit_err_t kit_audio_selftest_impl(void)
{
    if (!s_speaker || !s_beep_queue) {
        ESP_LOGE(TAG, "Self-test: subsistema de áudio não inicializado.");
        return KIT_FAIL;
    }

    // Identidade do ES8311 (esperado ID1=0x83, ID2=0x11) e trilha analógica.
    uint8_t id1 = 0, id2 = 0, aldo1 = 0;
    kit_i2c_read_reg(ES8311_I2C_ADDR, 0xFD, &id1);
    kit_i2c_read_reg(ES8311_I2C_ADDR, 0xFE, &id2);
    kit_i2c_read_reg(AXP2101_I2C_ADDR, 0x92, &aldo1); // AXP2101 ALDO1 voltage
    ESP_LOGI(TAG, "Self-test: ES8311 ID=0x%02X%02X%s, AVDD(ALDO1)~%d mV",
             id1, id2, (id1 == 0x83 && id2 == 0x11) ? " (OK)" : " (?)",
             500 + (aldo1 & 0x1F) * 100);

    kit_beep_req_t tone = { .sfx = -1, .freq_hz = 1000, .duration_ms = 600 };
    if (xQueueSend(s_beep_queue, &tone, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Self-test: fila cheia, tom descartado.");
        return KIT_FAIL;
    }
    return KIT_OK;
}

kit_err_t kit_audio_set_volume_impl(uint8_t percentage)
{
    if (percentage > 100) percentage = 100;
    if (percentage == s_volume) return KIT_OK;
    s_volume = percentage;
    ESP_LOGD(TAG, "Volume do áudio ajustado para %d%%", percentage);
    if (s_speaker) {
        esp_codec_dev_set_out_vol(s_speaker, s_volume);
    }
    return KIT_OK;
}
