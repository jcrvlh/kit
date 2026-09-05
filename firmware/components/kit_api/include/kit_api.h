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
} kit_sfx_t;

typedef struct {
    kit_err_t (*beep)(uint16_t freq_hz, uint16_t duration_ms);
    kit_err_t (*set_volume)(uint8_t percentage);
    kit_err_t (*sfx)(kit_sfx_t sfx);
    // "Pavio queimando": tique metronômico gerado na task de áudio, imune ao
    // jitter dos timers da Tool. tension 0..255 acelera o tique de forma
    // contínua; tension < 0 apaga o pavio. Chame a ~10 Hz enquanto queima.
    kit_err_t (*fuse)(int16_t tension);
} kit_audio_api_t;

typedef struct {
    kit_err_t (*keep_awake)(bool enable);
} kit_power_api_t;

typedef struct {
    kit_err_t (*get_info)(kit_system_info_t *info);
    void      (*exit)(void);
} kit_system_api_t;

typedef void (*kit_shake_callback_t)(void *user_data);

// Gesto de inclinar (Tool "Testa" / Heads Up!): o aparelho fica ~vertical na
// testa (eixo normal à tela ~0 g); virar a tela para o chão dispara DOWN, para o
// teto dispara UP. Uma vez por inclinada — só rearma ao voltar ao neutro.
typedef enum {
    KIT_TILT_NONE = 0,
    KIT_TILT_DOWN,   // tela virada para baixo (no Heads Up!: acertou)
    KIT_TILT_UP,     // tela virada para cima  (no Heads Up!: passou)
} kit_tilt_t;

typedef void (*kit_tilt_callback_t)(kit_tilt_t dir, void *user_data);

typedef struct {
    kit_err_t (*register_shake_callback)(kit_shake_callback_t cb, void *user_data);
    // Registra o callback de inclinar. O Runtime só faz o polling do gesto
    // enquanto a Tool ativa o pediu (kit_runtime_set_tool_tilt_enabled).
    kit_err_t (*register_tilt_callback)(kit_tilt_callback_t cb, void *user_data);
} kit_imu_api_t;

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

// Obtenção da Export Table no Runtime
const kit_api_table_t *kit_api_get_table(void);

#ifdef __cplusplus
}
#endif
