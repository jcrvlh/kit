/**
 * @file main.c
 * @brief Hello KIT — Exemplo oficial de referência para o SDK de Tools.
 *
 * Demonstra:
 * - Ciclo de vida (tool_init / tool_destroy)
 * - Acesso ao Display (get_screen)
 * - Random API (range)
 * - Audio API (beep)
 * - Storage API (persistência de estado)
 * - Tema e cores (kit_theme.h)
 *
 * No hardware real, cria uma tela LVGL com um número aleatório (D20),
 * que muda a cada toque. No modo stub (desktop), executa a lógica sem UI.
 */

#include "kit_tool_api.h"
#include "kit_theme.h"
#include <stdio.h>

/* Estado global da Tool */
static const kit_api_table_t *s_api = NULL;
static lv_obj_t *s_screen = NULL;
static int32_t s_roll_value = 1;

/**
 * Callback de toque — rola um D20 e emite bipe.
 */
static void on_input(const kit_input_event_t *event, void *user_data)
{
    (void)user_data;
    if (!s_api || event->type != KIT_INPUT_TAP) return;

    /* Gera um número aleatório de 1 a 20 (D20) usando a Random API (TRNG) */
    s_roll_value = s_api->random->range(1, 20);
    printf("[Hello KIT] D20 = %d\n", (int)s_roll_value);

    /* Emite som de bipe */
    if (s_api->audio) {
        s_api->audio->beep(1800, 40);
    }

    /* Persiste a última rolagem na Storage API */
    if (s_api->storage) {
        s_api->storage->set_i32("last_d20_roll", s_roll_value);
    }
}

/**
 * Ponto de entrada da Tool.
 */
kit_err_t tool_init(kit_tool_ctx_t *ctx)
{
    if (!ctx || !ctx->api) return KIT_ERR_INVALID_ARG;
    s_api = ctx->api;

    printf("[Hello KIT] tool_init (id=%s, data=%s)\n", ctx->tool_id, ctx->data_path);

    /* Recupera última rolagem se disponível */
    if (s_api->storage) {
        s_api->storage->get_i32("last_d20_roll", &s_roll_value);
        printf("[Hello KIT] Última rolagem restaurada: %d\n", (int)s_roll_value);
    }

    /* Obtém o container de tela fornecido pelo Runtime */
    s_screen = s_api->display->get_screen();

    /*
     * Em um build para o hardware real, aqui você criaria widgets LVGL:
     *
     *   lv_obj_t *label = lv_label_create(s_screen);
     *   lv_label_set_text(label, "HELLO KIT");
     *   lv_obj_set_style_text_font(label, &kit_mono_26, 0);
     *   lv_obj_set_style_text_color(label, lv_color_hex(KIT_COLOR_TEXT), 0);
     *   lv_obj_center(label);
     *
     * No modo stub (desktop), s_screen é NULL e nenhum widget é criado.
     */

    /* Registra callback de toque */
    if (s_api->input) {
        s_api->input->register_callback(on_input, NULL);
    }

    return KIT_OK;
}

/**
 * Destrutor da Tool.
 */
void tool_destroy(void)
{
    printf("[Hello KIT] tool_destroy\n");
    s_screen = NULL;
    s_api = NULL;
}
