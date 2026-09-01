#include "kit_input.h"
#include "kit_power.h"
#include "kit_display.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "KIT_INPUT";

static lv_indev_t *s_indev = NULL;
static kit_input_callback_t s_user_cb = NULL;
static void *s_user_data = NULL;

static uint8_t s_touch_addr = CST820_I2C_ADDR;

static void lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    // Lê o CST820 a cada polling do LVGL (config original — o sensor no I2C
    // compartilhado é sensível a mudanças de cadência). O gate por pino INT
    // GPIO21 foi tentado e derrubou o touch nesta placa; se o travamento
    // durante a rolagem voltar, mover a leitura para uma task dedicada.
    uint8_t buf[7] = {0};

    // Lê 7 bytes a partir do registrador 0x00 conforme especificação do CST820/CST816
    // buf[0] = Gesture ID
    // buf[1] = Status/Action
    // buf[2] = Touch points (0 ou 1)
    // buf[3] = X_HIGH (bits 3:0)
    // buf[4] = X_LOW
    // buf[5] = Y_HIGH (bits 3:0)
    // buf[6] = Y_LOW
    if (kit_i2c_read_bytes(s_touch_addr, 0x00, buf, sizeof(buf)) == ESP_OK) {
        uint8_t num_points = buf[2] & 0x0F;

        if (num_points == 1 && buf[2] != 0xFF) {
            uint16_t x = ((buf[3] & 0x0F) << 8) | buf[4];
            uint16_t y = ((buf[5] & 0x0F) << 8) | buf[6];

            // Mapeia coordenadas para o display (368 x 448)
            if (x < KIT_DISPLAY_WIDTH && y < KIT_DISPLAY_HEIGHT) {
                data->point.x = x;
                data->point.y = y;
                data->state = LV_INDEV_STATE_PRESSED;

                // Notifica callback customizado se registrado
                if (s_user_cb) {
                    kit_input_event_t ev = {
                        .type = KIT_INPUT_TOUCH_DOWN,
                        .x = x,
                        .y = y,
                        .duration_ms = 0,
                    };
                    s_user_cb(&ev, s_user_data);
                }
                return;
            }
        }
    }

    data->state = LV_INDEV_STATE_RELEASED;
}


kit_err_t kit_input_init(void)
{
    ESP_LOGI(TAG, "Inicializando subsistema de Touch...");

    // Tenta encontrar o controlador de toque nos endereços comuns (0x15, 0x5A, 0x38, 0x24)
    uint8_t probe_addrs[] = {0x15, 0x5A, 0x38, 0x24, 0x5D};
    bool found = false;
    for (size_t i = 0; i < sizeof(probe_addrs); i++) {
        uint8_t test_val = 0;
        if (kit_i2c_read_reg(probe_addrs[i], 0x00, &test_val) == ESP_OK) {
            s_touch_addr = probe_addrs[i];
            ESP_LOGI(TAG, "Touch detectado no endereco I2C: 0x%02X", s_touch_addr);
            found = true;
            break;
        }
    }

    if (!found) {
        ESP_LOGW(TAG, "Nenhum touch respondeu no I2C, usando endereco padrao 0x15");
    }

    // O CST820 entra em modo de auto-sleep poucos segundos após o boot caso não
    // detecte toque, deixando de responder de forma confiável ao polling do LVGL.
    // Desativa o auto-sleep do CST820 escrevendo 0x01 no registrador 0xFE.
    esp_err_t sleep_err = kit_i2c_write_reg(s_touch_addr, CST820_REG_DIS_AUTOSLEEP, 0x01);
    if (sleep_err != ESP_OK) {
        ESP_LOGW(TAG, "Falha ao desativar auto-sleep do CST820: %s", esp_err_to_name(sleep_err));
    } else {
        ESP_LOGI(TAG, "Auto-sleep do CST820 desativado (reg 0xFE).");
    }

    // Configura o pino de interrupção GPIO 21
    gpio_config_t int_conf = {
        .pin_bit_mask = (1ULL << KIT_TOUCH_INT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&int_conf);

    // Cria o dispositivo de entrada pointer no LVGL v9
    s_indev = lv_indev_create();
    if (!s_indev) {
        ESP_LOGE(TAG, "Falha ao criar dispositivo de entrada LVGL!");
        return KIT_ERR_NO_MEM;
    }

    lv_indev_set_type(s_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(s_indev, lvgl_touch_read_cb);

    ESP_LOGI(TAG, "Touch CST820 (0x%02X) registrado com sucesso no LVGL.", s_touch_addr);
    return KIT_OK;
}


kit_err_t kit_input_register_callback_impl(kit_input_callback_t cb, void *user_data)
{
    s_user_cb = cb;
    s_user_data = user_data;
    return KIT_OK;
}

kit_err_t kit_input_set_enabled_impl(bool enabled)
{
    if (s_indev) {
        lv_indev_enable(s_indev, enabled);
    }
    return KIT_OK;
}

bool kit_input_touch_present_impl(void)
{
    // Leitura crua do CST820, sem passar pelo indev do LVGL. Usada com a tela
    // em repouso (touch do LVGL desligado) para detectar o toque que deve
    // acordar o aparelho.
    uint8_t buf[7] = {0};
    if (kit_i2c_read_bytes(s_touch_addr, 0x00, buf, sizeof(buf)) != ESP_OK) {
        return false;
    }
    uint8_t num_points = buf[2] & 0x0F;
    if (num_points != 1 || buf[2] == 0xFF) {
        return false;
    }
    uint16_t x = ((buf[3] & 0x0F) << 8) | buf[4];
    uint16_t y = ((buf[5] & 0x0F) << 8) | buf[6];
    return (x < KIT_DISPLAY_WIDTH && y < KIT_DISPLAY_HEIGHT);
}
