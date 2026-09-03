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

// Repouso: com a tela apagada o Runtime chama kit_audio_suspend(true) e nenhum
// efeito novo é enfileirado. O que já estava na fila (ex.: o "cadeado" ao
// apagar) toca até o fim; depois a audio_task fecha o codec/PA sozinha por
// ociosidade (AUDIO_IDLE_MS), zerando a corrente de repouso do amplificador.
static volatile bool s_suspended = false;

static i2s_chan_handle_t s_tx_handle = NULL;
static esp_codec_dev_handle_t s_speaker = NULL;

// O codec/PA fica fechado quando não há som: aberto, o amplificador do ES8311
// segura o PA_EN e injeta um chiado contínuo no alto-falante (e gasta a
// corrente de repouso do PA de 5 V). audio_task abre sob demanda e fecha de
// novo depois de AUDIO_IDLE_MS parado. Só a audio_task mexe nesse estado.
static bool s_codec_open = false;
static esp_codec_dev_sample_info_t s_fs;

#define AUDIO_SAMPLE_RATE   16000
#define AUDIO_FRAME_COUNT   256
#define AUDIO_TWO_PI        6.28318530717958647692f
#define AUDIO_IDLE_MS       3000

// Depois de religar o codec, o PA_EN do ES8311 leva alguns ms pra subir e
// estabilizar. Sem essa folga o primeiro efeito curto (um CLICK tem ~20 ms)
// sai enquanto o amplificador ainda acorda e some — só o segundo toque em
// diante era ouvido. audio_codec_wake() empurra esse silêncio antes do 1º tom.
#define AUDIO_PA_SETTLE_MS  80

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

// "Pavio queimando" (Pavio Tool): um tique metronômico gerado AQUI, na task de
// áudio, e não por um lv_timer do lado da Tool — o ritmo do lv_timer treme
// quando a placa está repintando (era o "travando"). A Tool só empurra a
// "tensão" 0..255 a ~10 Hz; a task tateia s_fuse_tension a cada tique e ajusta
// o intervalo. render_silence entre os tiques mantém o DMA cheio (sem estalo) e
// o compasso exato. s_fuse_on liga/desliga o modo (só a Tool escreve tensão; a
// task só lê). Curva: quase parado no começo (suspense), aperta no meio,
// disparada no fim.
static volatile bool    s_fuse_on      = false;
static volatile uint8_t s_fuse_tension = 0;

#define FUSE_GAP_CALM_MS    430.0f   // intervalo entre tiques com tensão 0
#define FUSE_GAP_SPAN_MS    366.0f   // quanto o intervalo encurta até a tensão 255
#define FUSE_FREQ_BASE_HZ   1400.0f
#define FUSE_FREQ_SPAN_HZ    620.0f
#define FUSE_AMP_BASE       11800.0f // alto, mas ainda com folga pro fundo de escala (32767)
#define FUSE_AMP_SPAN        2700.0f // (o silêncio ativo entre os tiques é que tira o estouro)

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

// Um tique do pavio + o silêncio até o próximo, dimensionados pela tensão
// corrente (0..255). Roda na task de áudio, então o compasso é firme. O
// silêncio é fatiado pra ceder a vez na hora a um efeito que entre na fila (o
// "toc" do passe, o BUM) e ao fuse(-1) — nesse caso o ritmo cede a vez por um
// tique só e retoma.
static void render_fuse_tick(void)
{
    float t = s_fuse_tension / 255.0f;
    // Encurtamento do intervalo: mistura linear + quadrática — segura o começo
    // (dread) e desaba no fim (pânico).
    float shape = 0.30f * t + 0.70f * t * t;

    uint16_t freq = (uint16_t)(FUSE_FREQ_BASE_HZ + FUSE_FREQ_SPAN_HZ * t);
    uint16_t dur  = (uint16_t)(12.0f - 4.0f * t);           // 12 ms -> 8 ms
    float    amp  = FUSE_AMP_BASE + FUSE_AMP_SPAN * t;
    uint16_t gap  = (uint16_t)(FUSE_GAP_CALM_MS - FUSE_GAP_SPAN_MS * shape);

    render_tone(freq, dur, amp);

    uint16_t done = 0;
    while (done < gap) {
        if (!s_fuse_on || s_suspended) return;
        if (uxQueueMessagesWaiting(s_beep_queue) > 0) return;
        uint16_t chunk = (uint16_t)(gap - done) < 20 ? (uint16_t)(gap - done) : 20;
        render_silence(chunk);
        done += chunk;
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
        // Duas notas subindo (G5 -> C6), bem macias e baixas — é só um
        // "pronto", não uma fanfarra. Amplitude ~metade do resto.
        render_tone(784,  60,  4200.0f);   // G5
        render_silence(16);
        render_tone(1047, 110, 4600.0f);   // C6
        break;

    case KIT_SFX_BINGO_BALL: {
        // Sutil de propósito: é clicado muitas vezes seguidas. A animação de
        // sorteio dura ~600 ms (10 trocas de número a 60 ms), então acompanha
        // no mesmo compasso: um estalinho baixo e grave por troca, e no fim
        // uma notinha macia pra "assentar" — nada de brilho agudo nem volume.
        for (int i = 0; i < 9; i++) {
            render_tone((uint16_t)(760 + rnd(360)), 8, 2400.0f);
            render_silence(52);   // 8 + 52 ≈ 60 ms, no passo da animação
        }
        render_tone(784, 44, 3800.0f);   // ...e a bolinha saindo (G5, suave)
        render_tone(659, 44, 3000.0f);   // pequena queda (E5) pra "assentar"
        break;
    }

    case KIT_SFX_TOOL_OPEN: {
        // "Entrando num mundo novo": escalinha pentatônica de Dó subindo,
        // leve e rápida, terminando numa notinha brilhante que fica no ar.
        // Pentatônica = sempre alegre, sem nota torta. Toca toda vez que
        // uma Tool abre, então é leve de propósito.
        static const uint16_t run[] = { 523, 587, 659, 784, 880 };
        for (int i = 0; i < 5; i++) {
            render_tone(run[i], 40, 4200.0f + i * 320.0f);
            render_silence(12);
        }
        render_tone(1047, 200, 5400.0f);  // C6 — chega no oitavado, redondo
        break;
    }

    case KIT_SFX_WELCOME:
        // "Oi": arpejo de Lá maior subindo, baixo e arejado — a introdução
        // começa com um aceno, não com um anúncio.
        render_tone(659,  90, 4800.0f);   // E5
        render_silence(34);
        render_tone(880,  90, 5200.0f);   // A5
        render_silence(34);
        render_tone(1109, 120, 5600.0f);  // C#6
        render_silence(24);
        render_tone(1319, 150, 4600.0f);  // E6 — resolução macia, mais baixa
        break;

    case KIT_SFX_ONBOARD_DONE:
        // "Bem-vindo!": tríade de Dó subindo até o oitavado e um floreio
        // brilhante no fim. Mais cheia e feliz que o CONFIRM — fecha a
        // introdução em festa.
        render_tone(523,  95, 9000.0f);   // C5
        render_silence(22);
        render_tone(659,  95, 9500.0f);   // E5
        render_silence(22);
        render_tone(784,  95, 10000.0f);  // G5
        render_silence(22);
        render_tone(1047, 170, 11000.0f); // C6 — chegada
        render_silence(40);
        render_tone(1319, 70,  9000.0f);  // E6
        render_tone(1047, 70,  8500.0f);  // C6
        render_tone(1568, 200, 11000.0f); // G6 — final alto e alegre
        break;

    case KIT_SFX_TIMER_TICK:
        // Contagem regressiva dos últimos 5 s: um tique único, seco e baixo —
        // só um lembrete de que o tempo está acabando, sem assustar. O toque
        // do fim (TIMER_DONE) é que é o alarme.
        render_tone(1900, 7,  4200.0f);
        render_tone(1400, 9,  3000.0f);
        break;

    case KIT_SFX_LOCK:
        // Cadeado fechando: dois estalinhos secos e agudos (a lingueta entrando)
        // e um trinco grave e curto no fim — "trancou". Baixo de propósito, é só
        // um aviso de que a tela apagou.
        render_tone(2100, 9,  3200.0f);
        render_silence(8);
        render_tone(1500, 11, 3600.0f);
        render_silence(6);
        render_tone(430,  70, 7000.0f);   // trinco grave — fechado
        break;

    case KIT_SFX_UNLOCK:
        // Cadeado abrindo: o trinco grave solta e a lingueta sai em dois
        // estalinhos subindo — o oposto do LOCK, dá sensação de "liberou".
        render_tone(430,  45, 6000.0f);   // o trinco cede
        render_silence(10);
        render_tone(1500, 11, 3600.0f);
        render_silence(6);
        render_tone(2100, 12, 3400.0f);   // lingueta fora — aberto
        break;

    case KIT_SFX_BOTTLE_SPIN: {
        // Catraca da Garrafa: um "toc" seco por entalhe, o intervalo abrindo em
        // ease-out conforme o giro perde força (~2 s). Frequência média (o
        // falantinho não reproduz grave) e amplitude ~2/3 da cheia — audível,
        // mas com folga pra não estourar no volume máximo. Fecha com uma nota
        // um pouco mais grave e longa: "assentou".
        uint16_t gap = 14;
        for (int i = 0; i < 20; i++) {
            render_tone((uint16_t)(1050 + rnd(260)), 8, 8000.0f);  // "toc" seco
            render_silence(gap);
            uint32_t next = (uint32_t)gap + 3u + (uint32_t)i;
            gap = next > 140 ? 140 : (uint16_t)next;
        }
        render_tone(760, 80, 9000.0f);   // parou
        break;
    }

    case KIT_SFX_CATALOG_DONE:
        // Download concluído: arpejo de Dó subindo depressa, um saltinho no
        // oitavado e pouso no Sol agudo — comemora sem virar fanfarra. Toca
        // raramente (só quando um .kit termina de baixar), então pode brilhar.
        render_tone(523,  42, 8500.0f);   // C5
        render_silence(10);
        render_tone(659,  42, 9000.0f);   // E5
        render_silence(10);
        render_tone(784,  42, 9500.0f);   // G5
        render_silence(10);
        render_tone(1047, 75, 10500.0f);  // C6
        render_silence(24);
        render_tone(1319, 48, 9500.0f);   // E6  — saltinho
        render_tone(1047, 48, 8800.0f);   // C6
        render_tone(1568, 165, 11000.0f); // G6  — pouso alegre, fica no ar
        break;

    case KIT_SFX_ADEDONHA_CARD: {
        // Sorteio da cartela: folhear cartas (estalinhos de papel) desacelerando
        // em ease-out, terminando num "tap" seco — a cartela assentou. É setup,
        // energia baixa; o brilho fica pro sorteio da letra.
        int gap = 26;
        for (int i = 0; i < 12; i++) {
            render_tone((uint16_t)(1400 + rnd(500)), 6, 3200.0f);
            render_silence((uint16_t)gap);
            gap += 4 + i;
            if (gap > 150) gap = 150;
        }
        render_tone(520, 60, 7000.0f);   // tap — assentou
        break;
    }

    case KIT_SFX_ADEDONHA_LETTER: {
        // Letra travando — o som-assinatura. Folheio rápido que desacelera,
        // um "carimbo" seco (grave curto sobre agudo), e duas notas subindo
        // (C6 -> G6): "VALENDO!". Elétrico, curto, azul.
        int gap = 16;
        for (int i = 0; i < 10; i++) {
            render_tone((uint16_t)(1600 + rnd(700)), 6, 4200.0f);
            render_silence((uint16_t)gap);
            gap += 6 + i;
        }
        render_tone(280, 26, 12000.0f);   // carimbo — corpo grave
        render_tone(1500, 10, 6000.0f);   // ...e o "tec" agudo do carimbo
        render_silence(24);
        render_tone(1047, 70, 10000.0f);  // C6
        render_tone(1568, 150, 11000.0f); // G6 — valendo!
        break;
    }

    case KIT_SFX_ADEDONHA_STOP: {
        // STOP: buzina amigável (não um erro) — três notas descendo depressa e
        // um assento grave. "Lápis pra cima."
        render_tone(1245, 70, 10000.0f);  // D#6
        render_tone(988,  70, 10000.0f);  // B5
        render_tone(740,  90, 10500.0f);  // F#5
        render_silence(24);
        render_tone(392, 150, 9500.0f);   // G4 — assentou
        break;
    }

    case KIT_SFX_ADEDONHA_TIMEUP: {
        // Tempo esgotado: klaxon de game-show, bi-tom alternado (A5/F5) três
        // vezes e uma resolução grave e longa. Mais urgente que o alarme do
        // Timer — re-toca no overlay a cada ~3,4 s.
        for (int i = 0; i < 3; i++) {
            render_tone(880, 120, 12000.0f);   // A5
            render_tone(698, 120, 12000.0f);   // F5
            render_silence(50);
        }
        render_tone(392, 320, 12000.0f);       // G4 — "acabou"
        break;
    }

    case KIT_SFX_VETO_HIT:
        // Veto - Acertou: duas notas subindo depressa (E6 -> B6), curtas e brilhantes.
        // É clicado muitas vezes numa vez, então nada de fanfarra.
        render_tone(1319, 34, 7000.0f);   // E6
        render_tone(1976, 60, 8000.0f);   // B6
        break;

    case KIT_SFX_VETO_FOUL:
        // A cigarra do Veto: buzina de game-show — dois "honks" ásperos descendo
        // e um assento grave e longo. Rude de propósito: "falou a proibida".
        // Ajustar as graves no HW se a corneta chacoalhar (< ~200 Hz).
        render_tone(262,  95, 12000.0f);  // C4
        render_tone(208, 110, 12000.0f);  // ~G#3
        render_silence(22);
        render_tone(233,  90, 12000.0f);  // ~A#3
        render_tone(196, 240, 11000.0f);  // ~G3 — assentou
        break;

    case KIT_SFX_PAVIO_TICK:
        // A "fagulha" inicial do pavio: um "tec" seco de bancada, casado com o
        // primeiro tique do motor `fuse()` (mesma freq/amp), e um silêncio ativo
        // logo depois pra o DMA não estalar.
        render_tone(1450, 11, 12800.0f);
        render_silence(10);
        break;

    case KIT_SFX_PAVIO_TICK_HOT:
        // O mesmo "tec", mais agudo e mais curto (órfão desde que o tique virou
        // o motor `fuse()`; mantido pela ABI).
        render_tone(2050, 9, 13200.0f);
        render_silence(10);
        break;

    case KIT_SFX_PAVIO_BOOM: {
        // Explodiu: um estalo agudo e uma cascata caindo, tudo no registro que a
        // corneta reproduz limpo (nada de sub-grave) e com silêncios pro DMA.
        // Fica acima do pico do `fuse()` — a explosão é o som mais alto do jogo.
        render_tone(2600, 32, 15500.0f);
        render_silence(12);
        static const uint16_t f[7] = { 2200, 1750, 1350, 1050, 850, 720, 640 };
        for (int i = 0; i < 7; i++) {
            render_tone(f[i], (uint16_t)(26 - i * 2), 14800.0f - i * 1000.0f);
            render_silence((uint16_t)(12 + i * 4));
        }
        break;
    }

    default:
        break;
    }
}

// Liga o codec/PA (abre o dispositivo e reaplica o volume salvo). Só chamada
// pela audio_task. Um "toc" baixo pode acontecer aqui quando o PA sobe.
static void audio_codec_wake(void)
{
    if (s_codec_open || !s_speaker) return;
    int oret = esp_codec_dev_open(s_speaker, &s_fs);
    if (oret != ESP_CODEC_DEV_OK) {
        ESP_LOGW(TAG, "audio_codec_wake: open falhou (código %d)", oret);
        return;
    }
    esp_codec_dev_set_out_vol(s_speaker, s_volume);
    s_codec_open = true;

    // Dá tempo do PA assentar antes de qualquer tom, alimentando zeros (o DMA
    // não estala). Só roda nessa transição fechado -> aberto, então não pesa
    // nos toques seguidos.
    render_silence(AUDIO_PA_SETTLE_MS);
}

// Desliga o codec/PA depois de um tempo parado: derruba o PA_EN, o que
// elimina o chiado e a corrente de repouso do amplificador.
static void audio_codec_sleep(void)
{
    if (!s_codec_open || !s_speaker) return;
    esp_codec_dev_close(s_speaker);
    s_codec_open = false;
}

static void audio_task(void *arg)
{
    (void)arg;
    kit_beep_req_t req;
    while (1) {
        // Com o pavio queimando não bloqueia: entre um efeito e outro a task
        // fica emitindo os tiques (render_fuse_tick), que é o metrônomo. Se o
        // áudio está suspenso (tela em repouso), volta a bloquear — não fica
        // girando à toa.
        bool burning = s_fuse_on && !s_suspended;
        TickType_t wait = burning ? 0 : pdMS_TO_TICKS(AUDIO_IDLE_MS);
        if (xQueueReceive(s_beep_queue, &req, wait) == pdTRUE) {
            if (req.sfx == -2) continue;   // "kick": só acorda a task pro pavio
            audio_codec_wake();
            if (req.sfx == -1) {
                // Bipes muito curtos (<= 14 ms) são "ticks" de textura — saem
                // mais baixos pra não estourar quando disparados em rajada
                // (catraca da Garrafa, animação de sorteio).
                float amp = (req.duration_ms <= 14) ? AUDIO_AMP_FULL * 0.34f
                                                    : AUDIO_AMP_FULL;
                render_tone(req.freq_hz, req.duration_ms, amp);
            } else {
                render_sfx((kit_sfx_t)req.sfx);
            }
            continue;
        }
        // Fila vazia.
        if (burning) {
            audio_codec_wake();
            render_fuse_tick();
            continue;
        }
        audio_codec_sleep();   // ocioso: desliga o PA
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

    s_fs = (esp_codec_dev_sample_info_t){
        .bits_per_sample = 16,
        .channel = 1,
        .channel_mask = 0,
        .sample_rate = AUDIO_SAMPLE_RATE,
        .mclk_multiple = 256,
    };
    s_volume = kit_config_get_volume();   // volume salvo (kit_config_init roda antes)

    // Abre uma vez para validar o pipeline, aplica o volume e fecha de novo:
    // o codec nasce ocioso (PA_EN baixo, sem chiado). audio_task religa no
    // primeiro som e desliga sozinho depois de AUDIO_IDLE_MS parado.
    int oret = esp_codec_dev_open(s_speaker, &s_fs);
    if (oret != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "Falha ao abrir dispositivo de reprodução (código %d)", oret);
        return KIT_FAIL;
    }
    int vret = esp_codec_dev_set_out_vol(s_speaker, s_volume);
    if (vret != ESP_CODEC_DEV_OK) {
        ESP_LOGW(TAG, "esp_codec_dev_set_out_vol retornou %d", vret);
    }
    esp_codec_dev_close(s_speaker);
    s_codec_open = false;

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

void kit_audio_suspend(bool suspend)
{
    if (suspend == s_suspended) return;
    s_suspended = suspend;
    ESP_LOGI(TAG, "Áudio %s", suspend ? "suspenso (repouso)" : "reativado");
}

kit_err_t kit_audio_beep_impl(uint16_t freq_hz, uint16_t duration_ms)
{
    if (!s_speaker || !s_beep_queue) {
        ESP_LOGW(TAG, "Áudio não inicializado, ignorando bipe.");
        return KIT_FAIL;
    }
    if (s_suspended || !kit_config_get_sound_enabled()) return KIT_OK;

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
    if (s_suspended || !kit_config_get_sound_enabled()) return KIT_OK;

    kit_beep_req_t req = { .sfx = (int16_t)sfx, .freq_hz = 0, .duration_ms = 0 };
    if (xQueueSend(s_beep_queue, &req, 0) != pdTRUE) {
        return KIT_FAIL;
    }
    return KIT_OK;
}

kit_err_t kit_audio_fuse_impl(int16_t tension)
{
    if (!s_speaker || !s_beep_queue) return KIT_FAIL;

    if (tension < 0) {              // apaga o pavio
        s_fuse_on = false;
        return KIT_OK;
    }
    if (tension > 255) tension = 255;
    s_fuse_tension = (uint8_t)tension;

    if (s_suspended || !kit_config_get_sound_enabled()) {
        s_fuse_on = false;
        return KIT_OK;
    }
    if (!s_fuse_on) {
        s_fuse_on = true;
        // A task pode estar bloqueada até AUDIO_IDLE_MS no xQueueReceive —
        // um "kick" a acorda pra começar a emitir os tiques agora.
        kit_beep_req_t kick = { .sfx = -2, .freq_hz = 0, .duration_ms = 0 };
        xQueueSend(s_beep_queue, &kick, 0);
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
    // Se o codec estiver dormindo, o novo volume é aplicado no próximo wake.
    if (s_speaker && s_codec_open) {
        esp_codec_dev_set_out_vol(s_speaker, s_volume);
    }
    return KIT_OK;
}
