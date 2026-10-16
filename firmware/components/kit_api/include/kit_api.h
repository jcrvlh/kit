#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Códigos de Retorno Padronizados do KIT
typedef enum {
    KIT_OK = 0,
    KIT_FAIL = -1,
    KIT_ERR_NO_MEM = -2,
    KIT_ERR_INVALID_ARG = -3,
    KIT_ERR_NOT_FOUND = -4,
    KIT_ERR_TIMEOUT = -5,
    KIT_ERR_PERMISSION_DENIED = -6,
    KIT_ERR_STORAGE = -7,
    KIT_ERR_NOT_SUPPORTED = -8
} kit_err_t;

// Tipos de Eventos de Entrada
typedef enum {
    KIT_INPUT_TOUCH_DOWN,
    KIT_INPUT_TOUCH_UP,
    KIT_INPUT_TAP,
    KIT_INPUT_LONG_PRESS,
    KIT_INPUT_SWIPE_UP,
    KIT_INPUT_SWIPE_DOWN,
    KIT_INPUT_SWIPE_LEFT,
    KIT_INPUT_SWIPE_RIGHT
} kit_input_event_type_t;

typedef struct {
    kit_input_event_type_t type;
    int16_t x;
    int16_t y;
    uint32_t duration_ms;
} kit_input_event_t;

typedef void (*kit_input_callback_t)(const kit_input_event_t *event, void *user_data);

// Estrutura de Data e Hora
typedef struct {
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
} kit_datetime_t;

// Informações do Sistema
typedef struct {
    uint8_t  battery_percentage;
    bool     is_charging;
    uint32_t free_psram_bytes;
    uint32_t free_flash_bytes;
    char     device_id[16];
    char     runtime_version[16];
} kit_system_info_t;

// Tabelas de APIs Individuais
typedef struct {
    lv_obj_t *(*get_screen)(void);
    kit_err_t (*refresh)(void);
    kit_err_t (*set_brightness)(uint8_t percentage);
    uint8_t   (*get_brightness)(void);
} kit_display_api_t;

typedef struct {
    kit_err_t (*register_callback)(kit_input_callback_t cb, void *user_data);
} kit_input_api_t;

typedef struct {
    kit_err_t (*set_str)(const char *key, const char *value);
    kit_err_t (*get_str)(const char *key, char *buffer, size_t max_len);
    kit_err_t (*set_i32)(const char *key, int32_t value);
    kit_err_t (*get_i32)(const char *key, int32_t *out_value);
    FILE     *(*open_file)(const char *filename, const char *mode);
} kit_storage_api_t;

typedef struct {
    uint32_t  (*u32)(void);
    int32_t   (*range)(int32_t min, int32_t max);
    kit_err_t (*bytes)(uint8_t *buffer, size_t length);
    float     (*get_float)(void);
} kit_random_api_t;

typedef struct {
    uint64_t  (*get_millis)(void);
    kit_err_t (*get_datetime)(kit_datetime_t *dt);
    void      (*delay_ms)(uint32_t ms);
} kit_time_api_t;

// Efeitos sonoros prontos do KIT (sequências curtas renderizadas pela task de
// áudio, respeitam a flag "Som" dos Ajustes).
typedef enum {
    KIT_SFX_CLICK = 0,     // toque sutil — navegação, abrir um app
    KIT_SFX_BACK,          // voltar / fechar
    KIT_SFX_CONFIRM,       // confirmação positiva
    KIT_SFX_DICE_ROLL,     // "tombo" de rolagem de dados (~0,5 s)
    KIT_SFX_ROULETTE,      // catraca de roleta desacelerando (~1,4 s)
    KIT_SFX_COIN,          // giro de moeda no ar terminando num "ding"
    KIT_SFX_TIMER_DONE,    // alarme do fim do timer
    KIT_SFX_REVEAL,        // sorteio revelado (Primeiro, Quebra-gelo)
    KIT_SFX_BINGO_BALL,    // bolinha do bingo saindo (curto e discreto, clicado em série)
    KIT_SFX_TOOL_OPEN,     // abrir uma Tool — escalinha pentatônica feliz subindo
    KIT_SFX_WELCOME,       // Introdução: abertura curta, sutil e alegre (só no onboarding)
    KIT_SFX_ONBOARD_DONE,  // Introdução: fanfarra feliz de boas-vindas ao concluir
    KIT_SFX_TIMER_TICK,    // contagem regressiva: tique curtíssimo nos últimos 5 s
    KIT_SFX_LOCK,          // tela apagada: cadeado fechando (estalo + trinco grave)
    KIT_SFX_UNLOCK,        // tela ligada: cadeado abrindo (trinco solta + estalo subindo)
    KIT_SFX_BOTTLE_SPIN,   // Garrafa: catraca de madeira desacelerando (~2,2 s) e um "assentou"
    KIT_SFX_CATALOG_DONE,  // Catálogo: download de uma Tool concluído — arpejo alegre e saltitante
    KIT_SFX_ADEDONHA_CARD, // Adedonha: sorteio da cartela — folhear cartas desacelerando + "tap"
    KIT_SFX_ADEDONHA_LETTER,// Adedonha: letra travou — folheio + carimbo + duas notas "VALENDO!"
    KIT_SFX_ADEDONHA_STOP, // Adedonha: alguém apertou STOP — buzina amigável descendo + assento
    KIT_SFX_ADEDONHA_TIMEUP,// Adedonha: tempo esgotado — klaxon bi-tom alternado + resolução grave
    KIT_SFX_VETO_HIT,      // Veto: acertou — duas notas rápidas subindo, curtas (clicado em série)
    KIT_SFX_VETO_FOUL,     // Veto: falou uma proibida — a "cigarra", buzina dupla áspera descendo
    KIT_SFX_PAVIO_TICK,    // Pavio: tique do pavio — um "tec" seco (com silêncio pro DMA), clicado em série acelerando
    KIT_SFX_PAVIO_TICK_HOT,// Pavio: tique do pavio quase estourando — mesmo "tec", mais agudo e aflito
    KIT_SFX_PAVIO_BOOM,    // Pavio: explodiu — estalo agudo + cascata caindo (~0,35 s)
    KIT_SFX_TELEFONEMA_RING_A, // Telefonema: toque de verdade, variante A — o "brrring" clássico
    KIT_SFX_TELEFONEMA_RING_B, // Telefonema: toque de verdade, variante B — mais grave, ritmo em 3
    KIT_SFX_TELEFONEMA_RING_C, // Telefonema: toque de verdade, variante C — mais aguda e arrastada
    KIT_SFX_TELEFONEMA_FAKE, // Telefonema: toque falso (trote) — início de uma das variantes, cortado
    KIT_SFX_TELEFONEMA_PICKUP, // Telefonema: atendeu certo — "clique" de secretária + nota subindo
    KIT_SFX_TELEFONEMA_MISS,  // Telefonema: errou (cedo, trote ou não atendeu) — buzina curta descendo
    KIT_SFX_ESTOURO_POP,      // Estouro: estalo agudo + fuga de ar curtíssima (~0,1 s, sem cascata)
    KIT_SFX_ESTOURO_SHAKE,    // Estouro: "thump" curto e forte a cada chacoalhada registrada
    KIT_SFX_TAP,              // toque de botão sutil: "tic" de madeira (>= runtime 0.15.0)
} kit_sfx_t;

typedef struct {
    kit_err_t (*beep)(uint16_t freq_hz, uint16_t duration_ms);
    kit_err_t (*set_volume)(uint8_t percentage);
    kit_err_t (*sfx)(kit_sfx_t sfx);
    // "Pavio queimando": tique metronômico gerado na task de áudio, imune ao
    // jitter dos timers da Tool. tension 0..255 acelera o tique de forma
    // contínua; tension < 0 apaga o pavio. Chame a ~10 Hz enquanto queima.
    kit_err_t (*fuse)(int16_t tension);
    // Toca um WAV PCM 16-bit mono do cartão SD (`path` absoluto sob /sdcard/,
    // ex.: "<ctx->data_path>/assets/x.wav" ou "/sdcard/soundbox/<banco>/x.wav").
    // 16 kHz direto, 8 kHz com upsample 2x, estéreo rebaixado pra mono; outros
    // formatos são recusados. Assíncrono; um novo play corta o anterior
    // (retrigger). Respeita a flag "Som". Requer o cartão presente.
    kit_err_t (*play_sample)(const char *path);
    // Corta o WAV em reprodução, se houver.
    kit_err_t (*stop_sample)(void);
} kit_audio_api_t;

typedef struct {
    kit_err_t (*keep_awake)(bool enable);
} kit_power_api_t;

typedef struct {
    kit_err_t (*get_info)(kit_system_info_t *info);
    void      (*exit)(void);
} kit_system_api_t;

typedef void (*kit_shake_callback_t)(void *user_data);

// Gesto de inclinar (Tool "Testa"): o aparelho fica ~vertical na
// testa (eixo normal à tela ~0 g); virar a tela para o chão dispara DOWN, para o
// teto dispara UP. Uma vez por inclinada — só rearma ao voltar ao neutro.
typedef enum {
    KIT_TILT_NONE = 0,
    KIT_TILT_DOWN,   // tela virada para baixo (na Testa: acertou)
    KIT_TILT_UP,     // tela virada para cima  (na Testa: passou)
} kit_tilt_t;

typedef void (*kit_tilt_callback_t)(kit_tilt_t dir, void *user_data);

typedef struct {
    kit_err_t (*register_shake_callback)(kit_shake_callback_t cb, void *user_data);
    // Registra o callback de inclinar. O Runtime só faz o polling do gesto
    // enquanto a Tool ativa o pediu (kit_runtime_set_tool_tilt_enabled).
    kit_err_t (*register_tilt_callback)(kit_tilt_callback_t cb, void *user_data);

    // --- Giroscópio sob demanda (>= runtime 0.4.0) --------------------------
    // Ângulo relativo ao instante em que a Tool zerou (sem magnetômetro não há
    // "norte"). Ligue só com o aparelho parado — o bias é calibrado na hora.
    // Consome mais que o acelerômetro sozinho: chame gyro_stop() ao terminar.
    // Tudo em CENTIGRAUS (grau * 100) pra não expor float ao .so da Tool.

    // Liga o giroscópio e zera bias + ângulo acumulado.
    kit_err_t (*gyro_start)(void);
    // Re-zera bias e ângulo sem religar o sensor (liga se estiver desligado).
    // Use entre tentativas: religar o sensor a cada vez pisca o AMOLED.
    kit_err_t (*gyro_rezero)(void);
    // Integra desde o último start/rezero. yaw/pitch/roll em centigraus (sem
    // wrap — passar de 360° soma normal); rate = módulo da velocidade angular
    // atual em centigraus/s (pra detectar "parou de girar"). Qualquer ponteiro
    // pode ser NULL. false se o I2C falhou (mantém os últimos valores).
    bool (*gyro_poll)(int32_t *yaw_cdeg, int32_t *pitch_cdeg,
                      int32_t *roll_cdeg, int32_t *rate_cdps);
    // Desliga o giroscópio (economia de energia).
    void (*gyro_stop)(void);

    // --- Acelerômetro (>= runtime 0.15.0) -----------------------------------
    // Eixos da TELA: x pra direita, y pra baixo, z saindo da tela — já com a
    // rotação do Modo canhoto. Parado, o vetor aponta pra cima (deitado de
    // tela pra cima: z ~ +1000). Sempre ligado com a tela acesa; false no
    // repouso ou se o I2C falhou. Inteiros pra não expor float ao .so.

    // Aceleração em mili-g (qualquer ponteiro pode ser NULL).
    bool (*accel_poll)(int32_t *x_mg, int32_t *y_mg, int32_t *z_mg);
    // Inclinação absoluta pela gravidade: ângulo dos eixos x/y da tela acima
    // do horizontal, centigraus (-9000..9000). x > 0 = borda direita mais
    // alta; y > 0 = borda de baixo mais alta. Sem drift, sem calibrar.
    bool (*accel_tilt)(int32_t *x_cdeg, int32_t *y_cdeg);
} kit_imu_api_t;

// --- Rede (>= runtime 0.16.0) ----------------------------------------------
// Só HTTPS GET, um pedido por vez (no máximo 1/s), para os domínios listados em
// "network_domains" no manifest, sem redirect automático, resposta até 16 KB.
// O callback roda no contexto do LVGL; body (terminado em '\0') só vale
// durante a chamada. result: KIT_OK (veio resposta HTTP — confira o status),
// KIT_ERR_TIMEOUT, KIT_ERR_NO_MEM (resposta > 16 KB) ou KIT_FAIL (conexão/TLS).
typedef void (*kit_net_callback_t)(kit_err_t result, int http_status,
                                   const char *body, size_t len, void *user_data);

typedef struct {
    // Wi-Fi conectado agora?
    bool      (*is_online)(void);
    // Dispara o GET. KIT_OK = enfileirado (o callback vem depois);
    // KIT_ERR_PERMISSION_DENIED = domínio fora do manifest ou não é https://;
    // KIT_ERR_NOT_SUPPORTED = sem Wi-Fi; KIT_FAIL = ocupado (pedido em
    // andamento ou < 1 s do anterior); KIT_ERR_INVALID_ARG = url/cb inválidos.
    kit_err_t (*http_get)(const char *url, kit_net_callback_t cb, void *user_data);
    // Descarta o pedido em andamento: o callback não é mais chamado.
    void      (*cancel)(void);
} kit_net_api_t;

// --- Scan de Wi-Fi (>= runtime 0.18.0) -------------------------------------
// Varre as redes 2.4 GHz em volta e devolve cada AP (um por BSSID, oculta
// incluída), mais forte primeiro, até KIT_WIFI_SCAN_MAX. Só olha: não conecta
// em nada. Com o Wi-Fi do KIT desligado o rádio liga só durante o scan. Um
// scan por vez, no máximo um a cada KIT_WIFI_SCAN_MIN_GAP_MS (contando do
// início do anterior). O callback roda no contexto do LVGL; aps só vale
// durante a chamada. result: KIT_OK, KIT_ERR_NO_MEM (a pilha Wi-Fi não subiu
// por falta de RAM), KIT_ERR_NOT_SUPPORTED (a tela entrou em repouso) ou
// KIT_FAIL (o driver recusou, ex.: associando a uma rede agora — tente de novo).
#define KIT_WIFI_SCAN_MAX         32
#define KIT_WIFI_SCAN_MIN_GAP_MS  5000
#define KIT_WIFI_SSID_MAX         33   // 32 octetos + NUL

typedef enum {
    KIT_WIFI_AUTH_OPEN = 0,     // aberta (inclui OWE)
    KIT_WIFI_AUTH_WEP,
    KIT_WIFI_AUTH_WPA,
    KIT_WIFI_AUTH_WPA2,         // inclui WPA/WPA2 misto
    KIT_WIFI_AUTH_WPA3,         // inclui WPA2/WPA3 misto
    KIT_WIFI_AUTH_ENTERPRISE,   // 802.1X (WPA2/WPA3 Enterprise)
    KIT_WIFI_AUTH_OTHER,        // WAPI, DPP, ...
} kit_wifi_auth_t;

typedef struct {
    uint8_t bssid[6];                // endereço do AP: único por rádio
    int8_t  rssi;                    // dBm (mais perto de 0 = mais forte)
    uint8_t channel;                 // 1..13
    uint8_t auth;                    // kit_wifi_auth_t
    char    ssid[KIT_WIFI_SSID_MAX]; // "" = rede oculta
} kit_wifi_ap_t;

typedef void (*kit_wifi_scan_cb_t)(kit_err_t result, const kit_wifi_ap_t *aps,
                                   size_t count, void *user_data);

typedef struct {
    // Dispara o scan (~2 s). KIT_OK = aceito (o callback vem depois);
    // KIT_FAIL = ocupado (scan em andamento ou < KIT_WIFI_SCAN_MIN_GAP_MS do
    // anterior); KIT_ERR_INVALID_ARG = cb NULL; KIT_ERR_NO_MEM = sem RAM.
    kit_err_t (*scan)(kit_wifi_scan_cb_t cb, void *user_data);
    // Descarta o scan em andamento: o callback não é mais chamado.
    void      (*cancel)(void);
} kit_wifi_api_t;

// Export Table Consolidada
typedef struct {
    const kit_display_api_t *display;
    const kit_input_api_t   *input;
    const kit_storage_api_t *storage;
    const kit_random_api_t  *random;
    const kit_time_api_t    *time;
    const kit_audio_api_t   *audio;
    const kit_power_api_t   *power;
    const kit_system_api_t  *system;
    const kit_imu_api_t     *imu;
    // >= runtime 0.16.0. NULL se a Tool não declarou "network" + "network_domains".
    const kit_net_api_t     *net;
    // >= runtime 0.18.0. NULL se a Tool não declarou "wifi_scan".
    const kit_wifi_api_t    *wifi;
} kit_api_table_t;

// Contexto passado para cada Tool em tool_init
typedef struct {
    const char *tool_id;
    const char *data_path;
    const kit_api_table_t *api;
} kit_tool_ctx_t;

// Protótipos obrigatórios que a Tool deve exportar
typedef kit_err_t (*kit_tool_init_fn)(kit_tool_ctx_t *ctx);
typedef void (*kit_tool_destroy_fn)(void);

// Obtenção da Export Table no Runtime. kit_api_get_table() não tem rede nem
// scan (net = wifi = NULL); kit_api_get_table_for() é a mesma tabela com o que
// o manifest da Tool liberou. Uma Tool externa por vez: a tabela devolvida
// vale até a próxima chamada.
const kit_api_table_t *kit_api_get_table(void);
const kit_api_table_t *kit_api_get_table_for(bool with_network, bool with_wifi_scan);

#ifdef __cplusplus
}
#endif
