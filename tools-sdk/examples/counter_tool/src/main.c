/**
 * @file main.c
 * @brief Counter Tool — Exemplo intermediário do SDK.
 *
 * Demonstra:
 * - Storage API (persistência do contador entre sessões)
 * - Audio API (beep ao incrementar/decrementar)
 * - Input API (callback para gestos)
 * - Padrão de múltiplas páginas (conceito de tileview)
 * - Uso do tema e cores (kit_theme.h)
 *
 * O contador é persistido via Storage API — ao reabrir a Tool, o valor
 * é restaurado. Swipe left/right alterna entre página de ajuste e
 * página principal (conceito de tileview no hardware real).
 */

#include "kit_tool_api.h"
#include "kit_theme.h"
#include <stdio.h>

static const kit_api_table_t *s_api = NULL;
static lv_obj_t *s_screen = NULL;
static int32_t s_counter = 0;
static int32_t s_step = 1;  /* Incremento configurável */

static void counter_increment(void)
{
    s_counter += s_step;
    printf("[Counter] valor = %d\n", (int)s_counter);

    if (s_api->audio) {
        s_api->audio->beep(1200, 30);
    }

    if (s_api->storage) {
        s_api->storage->set_i32("counter_val", s_counter);
    }
}

static void counter_decrement(void)
{
    s_counter -= s_step;
    printf("[Counter] valor = %d\n", (int)s_counter);

    if (s_api->audio) {
        s_api->audio->beep(800, 30);
    }

    if (s_api->storage) {
        s_api->storage->set_i32("counter_val", s_counter);
    }
}

static void counter_reset(void)
{
    s_counter = 0;
    printf("[Counter] resetado para 0\n");

    if (s_api->storage) {
        s_api->storage->set_i32("counter_val", 0);
    }
}

static void on_input(const kit_input_event_t *event, void *user_data)
{
    (void)user_data;
    if (!s_api) return;

    switch (event->type) {
        case KIT_INPUT_TAP:
            counter_increment();
            break;
        case KIT_INPUT_LONG_PRESS:
            counter_reset();
            break;
        case KIT_INPUT_SWIPE_UP:
            counter_increment();
            break;
        case KIT_INPUT_SWIPE_DOWN:
            counter_decrement();
            break;
        default:
            break;
    }
}

kit_err_t tool_init(kit_tool_ctx_t *ctx)
{
    if (!ctx || !ctx->api) return KIT_ERR_INVALID_ARG;
    s_api = ctx->api;

    printf("[Counter] tool_init (id=%s)\n", ctx->tool_id);

    /* Restaura valor e step do storage */
    if (s_api->storage) {
        s_api->storage->get_i32("counter_val", &s_counter);
        s_api->storage->get_i32("counter_step", &s_step);
        if (s_step < 1) s_step = 1;
        printf("[Counter] restaurado: valor=%d, step=%d\n", (int)s_counter, (int)s_step);
    }

    s_screen = s_api->display->get_screen();

    /*
     * No hardware real, aqui se criaria:
     * - lv_tileview com 2 tiles (Ajuste | Contador)
     * - Tile 0 (Ajuste): roller para o step (1, 5, 10, 50, 100)
     * - Tile 1 (Contador): número grande (kit_display_120) + botões +/-
     *
     * Padrão de cores:
     * - Fundo: KIT_COLOR_BG
     * - Número: KIT_COLOR_TEXT, fonte kit_display_120
     * - Botão +: KIT_COLOR_GREEN
     * - Botão −: KIT_COLOR_RED
     * - Rótulos: KIT_COLOR_TEXT_MUTED, fonte kit_mono_16
     */

    if (s_api->input) {
        s_api->input->register_callback(on_input, NULL);
    }

    return KIT_OK;
}

void tool_destroy(void)
{
    printf("[Counter] tool_destroy\n");
    s_screen = NULL;
    s_api = NULL;
}
