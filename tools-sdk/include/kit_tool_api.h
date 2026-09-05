/**
 * @file kit_tool_api.h
 * @brief Header público do SDK de desenvolvimento de Tools para o KIT.
 *
 * Este é o header principal que toda Tool externa deve incluir. Ele
 * declara a tabela de APIs do Runtime (Display, Input, Storage, Random,
 * Time, Audio, Power, System e IMU), os tipos compartilhados e os
 * protótipos de ciclo de vida que toda Tool deve implementar.
 *
 * A Tool **nunca** acessa o hardware diretamente — toda interação com o
 * KIT é feita por meio de ponteiros de função na @ref kit_api_table_t,
 * recebida em @ref tool_init.
 *
 * @copyright GNU General Public License v3.0 (GPL-3.0)
 * @see https://github.com/jcrvlh/kit
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Versão do SDK e nível de API
 * ----------------------------------------------------------------------- */

/** Versão do SDK no formato string "MAJOR.MINOR.PATCH". */
#define KIT_SDK_VERSION       "0.2.0"

/** Versão do SDK como inteiro (major * 10000 + minor * 100 + patch). */
#define KIT_SDK_VERSION_CODE  200

/**
 * Nível da API do Runtime com o qual este SDK é compatível.
 * O campo `api_level` do manifest.json deve coincidir com este valor.
 */
#define KIT_API_LEVEL         1

/* -----------------------------------------------------------------------
 * Códigos de Retorno Padronizados
 * ----------------------------------------------------------------------- */

/**
 * @brief Códigos de retorno usados por toda a API do KIT.
 *
 * Toda função que retorna @ref kit_err_t segue a convenção:
 * - @ref KIT_OK (0) para sucesso;
 * - Valores negativos para erros específicos.
 */
typedef enum {
    KIT_OK                  =  0,  /**< Operação concluída com sucesso. */
    KIT_FAIL                = -1,  /**< Falha genérica. */
    KIT_ERR_NO_MEM          = -2,  /**< Memória insuficiente (PSRAM/heap). */
    KIT_ERR_INVALID_ARG     = -3,  /**< Argumento inválido ou NULL inesperado. */
    KIT_ERR_NOT_FOUND       = -4,  /**< Recurso (chave, arquivo, Tool) não encontrado. */
    KIT_ERR_TIMEOUT         = -5,  /**< Tempo limite excedido na operação. */
    KIT_ERR_PERMISSION_DENIED = -6, /**< Permissão negada (API não declarada no manifest). */
    KIT_ERR_STORAGE         = -7,  /**< Erro de I/O no sistema de arquivos (Flash/LittleFS). */
    KIT_ERR_NOT_SUPPORTED   = -8   /**< Funcionalidade indisponível neste nível de API. */
} kit_err_t;

/* -----------------------------------------------------------------------
 * Eventos de Entrada / Toque
 * ----------------------------------------------------------------------- */

/**
 * @brief Tipos de eventos de entrada reconhecidos pelo KIT.
 *
 * O CST820 (touch capacitivo) gera esses eventos na camada LVGL. A Tool
 * recebe-os via callback registrado em @ref kit_input_api_t.
 */
typedef enum {
    KIT_INPUT_TOUCH_DOWN = 0,  /**< Dedo encostou na tela. */
    KIT_INPUT_TOUCH_UP,        /**< Dedo levantou da tela. */
    KIT_INPUT_TAP,             /**< Toque rápido (< 300 ms) — o evento mais comum. */
    KIT_INPUT_LONG_PRESS,      /**< Toque longo (≥ 800 ms sem mover). */
    KIT_INPUT_SWIPE_UP,        /**< Arrastar para cima. */
    KIT_INPUT_SWIPE_DOWN,      /**< Arrastar para baixo. */
    KIT_INPUT_SWIPE_LEFT,      /**< Arrastar para a esquerda (avança página). */
    KIT_INPUT_SWIPE_RIGHT      /**< Arrastar para a direita (volta página). */
} kit_input_event_type_t;

/**
 * @brief Dados de um evento de entrada.
 *
 * Coordenadas (x, y) são relativas à tela (0,0 = canto superior esquerdo,
 * max 367×447 no display AMOLED 368×448 do KIT).
 */
typedef struct {
    kit_input_event_type_t type;  /**< Tipo do evento. */
    int16_t x;                    /**< Coordenada X (pixels). */
    int16_t y;                    /**< Coordenada Y (pixels). */
    uint32_t duration_ms;         /**< Duração do toque em milissegundos. */
} kit_input_event_t;

/**
 * @brief Callback de eventos de entrada.
 * @param event Ponteiro para a estrutura do evento (válido apenas durante a chamada).
 * @param user_data Ponteiro opaco passado no registro do callback.
 */
typedef void (*kit_input_callback_t)(const kit_input_event_t *event, void *user_data);

/* -----------------------------------------------------------------------
 * Data e Hora
 * ----------------------------------------------------------------------- */

/**
 * @brief Estrutura de data e hora lida do RTC (PCF85063A).
 *
 * Campos seguem calendário civil; year é o ano completo (ex: 2026).
 */
typedef struct {
    uint16_t year;    /**< Ano (ex: 2026). */
    uint8_t  month;   /**< Mês (1–12). */
    uint8_t  day;     /**< Dia do mês (1–31). */
    uint8_t  hour;    /**< Hora (0–23). */
    uint8_t  minute;  /**< Minuto (0–59). */
    uint8_t  second;  /**< Segundo (0–59). */
} kit_datetime_t;

/* -----------------------------------------------------------------------
 * Telemetria do Sistema
 * ----------------------------------------------------------------------- */

/**
 * @brief Informações do dispositivo e do estado de energia.
 *
 * Obtida via @ref kit_system_api_t::get_info.
 */
typedef struct {
    uint8_t  battery_percentage;   /**< Nível da bateria (0–100%). */
    bool     is_charging;          /**< `true` se conectado a USB/carregador. */
    uint32_t free_psram_bytes;     /**< Bytes livres de PSRAM. */
    uint32_t free_flash_bytes;     /**< Bytes livres na partição LittleFS. */
    char     device_id[16];        /**< Identificador único "KIT-XXXX" (derivado do eFuse MAC). */
    char     runtime_version[16];  /**< Versão do firmware em execução (ex: "0.1.0"). */
} kit_system_info_t;

/* -----------------------------------------------------------------------
 * Integração gráfica LVGL
 * ----------------------------------------------------------------------- */

/*
 * `lv_obj_t`, `lv_color_t`, `lv_font_t` e (no build real) todo o LVGL vêm de
 * kit_lvgl.h — opacos no build nativo, `lvgl.h` de verdade no build Xtensa.
 * Ver kit_lvgl.h e tools-sdk/docs/tool_lvgl_runtime.md.
 */
#include "kit_lvgl.h"

/* -----------------------------------------------------------------------
 * Callback de chacoalhar (IMU)
 * ----------------------------------------------------------------------- */

/**
 * @brief Callback invocado quando o KIT é chacoalhado (shake).
 * @param user_data Ponteiro opaco passado no registro.
 */
typedef void (*kit_shake_callback_t)(void *user_data);

/**
 * @brief Direção de uma inclinada deliberada do aparelho (gesto "Heads Up!").
 *
 * O KIT fica ~vertical na testa (eixo normal à tela ~0 g). Virar a tela para o
 * chão dispara @ref KIT_TILT_DOWN; para o teto, @ref KIT_TILT_UP. Dispara uma
 * vez por inclinada — só rearma quando o aparelho volta a ~vertical.
 */
typedef enum {
    KIT_TILT_NONE = 0,
    KIT_TILT_DOWN,   /**< tela virada para baixo (no Heads Up!: acertou). */
    KIT_TILT_UP,     /**< tela virada para cima  (no Heads Up!: passou). */
} kit_tilt_t;

/**
 * @brief Callback invocado a cada inclinada deliberada do aparelho.
 * @param dir       Direção da inclinada.
 * @param user_data Ponteiro opaco passado no registro.
 */
typedef void (*kit_tilt_callback_t)(kit_tilt_t dir, void *user_data);

/* -----------------------------------------------------------------------
 * Tabelas de APIs Individuais
 * ----------------------------------------------------------------------- */

/**
 * @brief API de Display (AMOLED CO5300 368×448, QSPI).
 *
 * Controla a tela e fornece o container raiz LVGL para a Tool.
 * Requer permissão: `"display"` no manifest.
 */
typedef struct {
    /**
     * Retorna o objeto LVGL que serve de container raiz da Tool.
     * Todos os widgets devem ser criados como filhos deste objeto.
     * @return Ponteiro para lv_obj_t (nunca NULL enquanto a Tool estiver ativa).
     */
    lv_obj_t *(*get_screen)(void);

    /**
     * Força um refresh imediato do framebuffer (raramente necessário —
     * o LVGL já atualiza automaticamente a 60 FPS quando há animações).
     * @return KIT_OK em caso de sucesso.
     */
    kit_err_t (*refresh)(void);

    /**
     * Define o brilho do display AMOLED.
     * @param percentage Brilho de 0 (desligado) a 100 (máximo).
     * @return KIT_OK em caso de sucesso.
     */
    kit_err_t (*set_brightness)(uint8_t percentage);

    /**
     * Retorna o brilho atual do display.
     * @return Valor de 0 a 100.
     */
    uint8_t   (*get_brightness)(void);
} kit_display_api_t;

/**
 * @brief API de Entrada (Touch CST820, I2C).
 *
 * Permite à Tool receber notificações de eventos de toque e gestos.
 * Requer permissão: `"input"` no manifest.
 */
typedef struct {
    /**
     * Registra um callback para receber eventos de entrada.
     * Apenas um callback por Tool é suportado; uma nova chamada substitui o anterior.
     * @param cb        Função de callback.
     * @param user_data Ponteiro opaco repassado ao callback.
     * @return KIT_OK em caso de sucesso.
     */
    kit_err_t (*register_callback)(kit_input_callback_t cb, void *user_data);
} kit_input_api_t;

/**
 * @brief API de Armazenamento Persistente (LittleFS sobre Flash).
 *
 * Cada Tool tem acesso isolado ao seu subdiretório privado
 * `/tools/<tool_id>/data/`. Chaves e arquivos são persisti-dos entre
 * sessões e sobrevivem a reinicializações.
 *
 * Requer permissão: `"storage"` no manifest.
 */
typedef struct {
    /**
     * Persiste uma string associada a uma chave.
     * @param key   Chave (até 15 caracteres, ASCII alfanumérico + '_').
     * @param value String UTF-8 (até 512 bytes incluindo '\0').
     * @return KIT_OK ou KIT_ERR_STORAGE.
     */
    kit_err_t (*set_str)(const char *key, const char *value);

    /**
     * Lê uma string previamente salva.
     * @param key     Chave a buscar.
     * @param buffer  Buffer de destino (preenchido com '\0' se não encontrada).
     * @param max_len Tamanho máximo do buffer.
     * @return KIT_OK ou KIT_ERR_NOT_FOUND.
     */
    kit_err_t (*get_str)(const char *key, char *buffer, size_t max_len);

    /**
     * Persiste um inteiro de 32 bits associado a uma chave.
     * @param key   Chave (até 15 caracteres).
     * @param value Valor inteiro.
     * @return KIT_OK ou KIT_ERR_STORAGE.
     */
    kit_err_t (*set_i32)(const char *key, int32_t value);

    /**
     * Lê um inteiro previamente salvo.
     * @param key       Chave a buscar.
     * @param out_value Ponteiro para receber o valor (inalterado se não encontrado).
     * @return KIT_OK ou KIT_ERR_NOT_FOUND.
     */
    kit_err_t (*get_i32)(const char *key, int32_t *out_value);

    /**
     * Abre um arquivo no diretório de dados da Tool.
     * O caminho é relativo ao subdiretório da Tool (`/tools/<id>/data/`).
     * @param filename Nome do arquivo (ex: "scores.json").
     * @param mode     Modo fopen: "r", "w", "a", "rb", "wb".
     * @return Ponteiro FILE* ou NULL em caso de erro.
     */
    FILE     *(*open_file)(const char *filename, const char *mode);
} kit_storage_api_t;

/**
 * @brief API de Aleatoriedade (TRNG de hardware do ESP32-S3).
 *
 * Números gerados por True Random Number Generator — criptograficamente
 * seguros e sem necessidade de seed.
 *
 * Requer permissão: `"random"` no manifest.
 */
typedef struct {
    /**
     * Gera um inteiro aleatório sem sinal de 32 bits (0 a UINT32_MAX).
     * @return Valor aleatório.
     */
    uint32_t  (*u32)(void);

    /**
     * Gera um inteiro aleatório no intervalo [min, max] (inclusivo).
     * @param min Limite inferior.
     * @param max Limite superior (deve ser >= min).
     * @return Valor aleatório no intervalo.
     */
    int32_t   (*range)(int32_t min, int32_t max);

    /**
     * Preenche um buffer com bytes aleatórios.
     * @param buffer Ponteiro para o buffer de destino.
     * @param length Quantidade de bytes a gerar.
     * @return KIT_OK em caso de sucesso.
     */
    kit_err_t (*bytes)(uint8_t *buffer, size_t length);

    /**
     * Gera um número float aleatório no intervalo [0.0, 1.0).
     * @return Valor float aleatório.
     */
    float     (*get_float)(void);
} kit_random_api_t;

/**
 * @brief API de Tempo (RTC PCF85063A + relógio interno do FreeRTOS).
 *
 * Requer permissão: `"time"` no manifest.
 */
typedef struct {
    /**
     * Retorna milissegundos desde o boot do dispositivo (monotônico).
     * @return Milissegundos (wraps a cada ~49.7 dias).
     */
    uint64_t  (*get_millis)(void);

    /**
     * Lê a data e hora atual do RTC.
     * @param dt Ponteiro para estrutura de destino.
     * @return KIT_OK ou KIT_ERR_NOT_SUPPORTED se RTC indisponível.
     */
    kit_err_t (*get_datetime)(kit_datetime_t *dt);

    /**
     * Bloqueia a execução pelo número de milissegundos especificado.
     * Use com cautela — bloqueia a task da Tool e impede atualização da UI.
     * @param ms Milissegundos para esperar.
     */
    void      (*delay_ms)(uint32_t ms);
} kit_time_api_t;

/**
 * @brief Efeitos sonoros prontos do KIT.
 *
 * Sequências curtas renderizadas pela task de áudio do Runtime; respeitam a
 * flag "Som" dos Ajustes. Espelha o enum do firmware — não reordene.
 */
typedef enum {
    KIT_SFX_CLICK = 0,     /**< toque sutil — navegação, abrir um app */
    KIT_SFX_BACK,          /**< voltar / fechar */
    KIT_SFX_CONFIRM,       /**< confirmação positiva */
    KIT_SFX_DICE_ROLL,     /**< "tombo" de rolagem de dados (~0,5 s) */
    KIT_SFX_ROULETTE,      /**< catraca de roleta desacelerando (~1,4 s) */
    KIT_SFX_COIN,          /**< giro de moeda no ar terminando num "ding" */
    KIT_SFX_TIMER_DONE,    /**< alarme do fim do timer */
    KIT_SFX_REVEAL,        /**< sorteio revelado */
    KIT_SFX_BINGO_BALL,    /**< bolinha do bingo saindo (curto, clicado em série) */
    KIT_SFX_TOOL_OPEN,     /**< abrir uma Tool — escalinha pentatônica subindo */
    KIT_SFX_WELCOME,       /**< Introdução: abertura curta e alegre (onboarding) */
    KIT_SFX_ONBOARD_DONE,  /**< Introdução: fanfarra de boas-vindas ao concluir */
    KIT_SFX_TIMER_TICK,    /**< contagem regressiva: tique nos últimos 5 s */
    KIT_SFX_LOCK,          /**< tela apagada: cadeado fechando */
    KIT_SFX_UNLOCK,        /**< tela ligada: cadeado abrindo */
    KIT_SFX_BOTTLE_SPIN,   /**< Garrafa: catraca de madeira desacelerando (~2,2 s) */
    KIT_SFX_CATALOG_DONE,  /**< Catálogo: download de uma Tool concluído — arpejo alegre */
    KIT_SFX_ADEDONHA_CARD, /**< Adedonha: sorteio da cartela — folhear cartas + "tap" */
    KIT_SFX_ADEDONHA_LETTER,/**< Adedonha: letra travou — folheio + carimbo + "VALENDO!" */
    KIT_SFX_ADEDONHA_STOP, /**< Adedonha: apertou STOP — buzina amigável descendo */
    KIT_SFX_ADEDONHA_TIMEUP,/**< Adedonha: tempo esgotado — klaxon bi-tom + resolução grave */
    KIT_SFX_VETO_HIT,      /**< Veto: acertou — duas notas rápidas subindo, curtas */
    KIT_SFX_VETO_FOUL,     /**< Veto: falou uma proibida — buzina dupla áspera descendo */
    KIT_SFX_PAVIO_TICK,    /**< Pavio: tique do pavio — "tec" seco, clicado em série acelerando */
    KIT_SFX_PAVIO_TICK_HOT,/**< Pavio: tique quase estourando — mesmo "tec", mais agudo */
    KIT_SFX_PAVIO_BOOM,    /**< Pavio: explodiu — estalo agudo + cascata caindo (~0,35 s) */
    KIT_SFX_TELEFONEMA_RING_A, /**< Telefonema: toque de verdade, variante A — o "brrring" clássico */
    KIT_SFX_TELEFONEMA_RING_B, /**< Telefonema: toque de verdade, variante B — mais grave, ritmo em 3 */
    KIT_SFX_TELEFONEMA_RING_C, /**< Telefonema: toque de verdade, variante C — mais aguda e arrastada */
    KIT_SFX_TELEFONEMA_FAKE,   /**< Telefonema: toque falso (trote) — início de uma das variantes, cortado */
    KIT_SFX_TELEFONEMA_PICKUP, /**< Telefonema: atendeu certo — "clique" de secretária + nota subindo */
    KIT_SFX_TELEFONEMA_MISS,   /**< Telefonema: errou (cedo, trote ou não atendeu) — buzina curta descendo */
    KIT_SFX_ESTOURO_POP,       /**< Estouro: estalo agudo + fuga de ar curtíssima (~0,1 s, sem cascata) */
    KIT_SFX_ESTOURO_SHAKE,     /**< Estouro: "thump" curto e forte a cada chacoalhada registrada */
} kit_sfx_t;

/**
 * @brief API de Áudio (Codec ES8311, I2S + amplificador onboard).
 *
 * Requer permissão: `"audio"` no manifest.
 */
typedef struct {
    /**
     * Emite um bipe (tom senoidal) no alto-falante.
     * @param freq_hz     Frequência em Hz (recomendado: 200–8000).
     * @param duration_ms Duração em milissegundos (recomendado: 20–500).
     * @return KIT_OK ou KIT_ERR_NOT_SUPPORTED se áudio desabilitado.
     */
    kit_err_t (*beep)(uint16_t freq_hz, uint16_t duration_ms);

    /**
     * Define o volume do alto-falante.
     * @param percentage Volume de 0 (mudo) a 100 (máximo).
     * @return KIT_OK em caso de sucesso.
     */
    kit_err_t (*set_volume)(uint8_t percentage);

    /**
     * Toca um efeito sonoro pronto do KIT (ver @ref kit_sfx_t).
     * @param sfx Identificador do efeito.
     * @return KIT_OK ou KIT_ERR_NOT_SUPPORTED se áudio desabilitado.
     */
    kit_err_t (*sfx)(kit_sfx_t sfx);

    /**
     * "Pavio queimando": um tique metronômico renderizado pela task de áudio do
     * Runtime — o ritmo é constante mesmo com a Tool ocupada repintando a tela
     * (um `lv_timer` tocando o tique treme). A Tool empurra só a "tensão".
     *
     * @param tension 0..255 acelera o tique de forma contínua (grave e
     *                espaçado → agudo e frenético). Valor negativo apaga o
     *                pavio (silêncio).
     * @return KIT_OK ou KIT_ERR_NOT_SUPPORTED se áudio desabilitado.
     *
     * Chame periodicamente (~10 Hz basta) enquanto o pavio queima e uma vez
     * com valor negativo ao terminar. Respeita a flag "Som".
     */
    kit_err_t (*fuse)(int16_t tension);
} kit_audio_api_t;

/**
 * @brief API de Energia (PMIC AXP2101).
 *
 * Requer permissão: `"power"` no manifest.
 */
typedef struct {
    /**
     * Mantém o dispositivo acordado enquanto a Tool estiver ativa,
     * impedindo repouso automático e desligamento por timer.
     * Essencial para Tools como Timer/Cronômetro.
     * @param enable `true` para manter acordado, `false` para restaurar comportamento normal.
     * @return KIT_OK em caso de sucesso.
     */
    kit_err_t (*keep_awake)(bool enable);
} kit_power_api_t;

/**
 * @brief API do Sistema (informações do dispositivo e controle de ciclo de vida).
 *
 * Sempre disponível — não requer permissão especial.
 */
typedef struct {
    /**
     * Preenche a estrutura com informações do dispositivo (bateria, memória, versão).
     * @param info Ponteiro para a estrutura de destino.
     * @return KIT_OK em caso de sucesso.
     */
    kit_err_t (*get_info)(kit_system_info_t *info);

    /**
     * Encerra a Tool e retorna ao Launcher.
     * Esta função **não retorna** — o Runtime chamará `tool_destroy()` e
     * descarregará a Tool da memória.
     */
    void      (*exit)(void);
} kit_system_api_t;

/**
 * @brief API do IMU (Acelerômetro/Giroscópio QMI8658).
 *
 * Permite detectar quando o dispositivo é chacoalhado (shake).
 * Requer permissão: `"imu"` no manifest.
 */
typedef struct {
    /**
     * Registra um callback que será chamado quando o KIT for chacoalhado.
     * Apenas um callback por Tool é suportado; uma nova chamada substitui o anterior.
     * A sensibilidade do shake é definida pelo Runtime (não configurável pela Tool).
     * @param cb        Função de callback (chamada no contexto da task LVGL).
     * @param user_data Ponteiro opaco repassado ao callback.
     * @return KIT_OK em caso de sucesso.
     */
    kit_err_t (*register_shake_callback)(kit_shake_callback_t cb, void *user_data);

    /**
     * Registra um callback para o gesto de inclinar (ver @ref kit_tilt_t).
     * Feito para o jogo estilo "Heads Up!": inclina para baixo = acertou,
     * para cima = passou. Um callback por Tool; nova chamada substitui o
     * anterior; NULL remove. O Runtime só faz o polling do gesto enquanto
     * houver um callback registrado. Requer `min_runtime` >= "0.2.0".
     * @param cb        Função de callback (chamada no contexto da task LVGL).
     * @param user_data Ponteiro opaco repassado ao callback.
     * @return KIT_OK em caso de sucesso.
     */
    kit_err_t (*register_tilt_callback)(kit_tilt_callback_t cb, void *user_data);
} kit_imu_api_t;

/* -----------------------------------------------------------------------
 * Tabela Consolidada de Export do KIT Runtime
 * ----------------------------------------------------------------------- */

/**
 * @brief Tabela mestra de APIs exportadas pelo Runtime.
 *
 * Recebida pela Tool em @ref kit_tool_ctx_t::api. Cada ponteiro é `NULL`
 * se a permissão correspondente não foi declarada no manifest.json.
 * Sempre verifique antes de usar:
 * @code
 * if (ctx->api->audio) {
 *     ctx->api->audio->beep(1500, 50);
 * }
 * @endcode
 */
typedef struct {
    const kit_display_api_t *display;  /**< API de Display (requer "display"). */
    const kit_input_api_t   *input;    /**< API de Entrada (requer "input"). */
    const kit_storage_api_t *storage;  /**< API de Storage (requer "storage"). */
    const kit_random_api_t  *random;   /**< API de Random (requer "random"). */
    const kit_time_api_t    *time;     /**< API de Tempo (requer "time"). */
    const kit_audio_api_t   *audio;    /**< API de Áudio (requer "audio"). */
    const kit_power_api_t   *power;    /**< API de Energia (requer "power"). */
    const kit_system_api_t  *system;   /**< API do Sistema (sempre disponível). */
    const kit_imu_api_t     *imu;      /**< API do IMU/Shake (requer "imu"). */
} kit_api_table_t;

/* -----------------------------------------------------------------------
 * Contexto da Tool
 * ----------------------------------------------------------------------- */

/**
 * @brief Contexto recebido pela Tool na chamada de @ref tool_init.
 *
 * Contém o ID da Tool, o caminho para seu diretório de dados e a
 * tabela de APIs do Runtime.
 */
typedef struct {
    const char *tool_id;            /**< ID único da Tool (ex: "com.kit.dice"). */
    const char *data_path;          /**< Caminho absoluto para o diretório de dados da Tool. */
    const kit_api_table_t *api;     /**< Tabela de APIs do Runtime. */
} kit_tool_ctx_t;

/* -----------------------------------------------------------------------
 * Protótipos de Ciclo de Vida (a Tool deve implementar)
 * ----------------------------------------------------------------------- */

/**
 * Marca os únicos símbolos que o objeto compartilhado da Tool exporta.
 * A Tool é compilada com `-fvisibility=hidden`; sem isto, `tool_init` e
 * `tool_destroy` seriam ocultados e removidos pelo `--gc-sections`.
 * No build nativo (stubs) é vazio.
 */
#if defined(KIT_SDK_STUBS)
#define KIT_TOOL_EXPORT
#else
#define KIT_TOOL_EXPORT __attribute__((visibility("default"), used))
#endif

/**
 * @brief Ponto de entrada da Tool — chamado pelo Runtime ao iniciar.
 *
 * A Tool deve:
 * 1. Salvar `ctx->api` para uso posterior.
 * 2. Obter a tela raiz via `ctx->api->display->get_screen()`.
 * 3. Criar seus widgets LVGL como filhos dessa tela.
 * 4. Registrar callbacks de input/shake se necessário.
 * 5. Retornar @ref KIT_OK para indicar sucesso.
 *
 * Se retornar erro, o Runtime aborta o carregamento e volta ao Launcher.
 *
 * @param ctx Contexto com APIs e metadados (válido durante toda a vida da Tool).
 * @return KIT_OK em caso de sucesso, código de erro caso contrário.
 */
KIT_TOOL_EXPORT kit_err_t tool_init(kit_tool_ctx_t *ctx);

/**
 * @brief Destrutor da Tool — chamado pelo Runtime ao encerrar.
 *
 * A Tool deve liberar toda memória alocada dinamicamente. Não é necessário
 * destruir widgets LVGL individualmente — o Runtime limpa a tela inteira.
 * Porém, timers LVGL (`lv_timer_t`) e alocações de heap devem ser
 * explicitamente liberados.
 */
KIT_TOOL_EXPORT void tool_destroy(void);

#ifdef __cplusplus
}
#endif
