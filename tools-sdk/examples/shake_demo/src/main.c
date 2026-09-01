/**
 * @file main.c
 * @brief Shake Demo — Exemplo de uso da IMU API do SDK.
 *
 * Demonstra:
 * - IMU API (register_shake_callback)
 * - Random API (geração de números aleatórios)
 * - Audio API (beep ao chacoalhar)
 * - Display API (get_screen, get_brightness)
 * - Tema e cores (kit_theme.h)
 *
 * Ao chacoalhar o KIT, gera um número aleatório de 1 a 100 e emite bipe.
 * No hardware real, o número seria mostrado em destaque na tela.
 */

#include "kit_tool_api.h"
#include "kit_theme.h"
#include <stdio.h>

static const kit_api_table_t *s_api = NULL;
static lv_obj_t *s_screen = NULL;
static int32_t s_last_value = 0;
static uint32_t s_shake_count = 0;

/**
 * Callback chamado pelo Runtime quando o KIT é chacoalhado.
 */
static void on_shake(void *user_data)
{
    (void)user_data;
    if (!s_api) return;

    s_shake_count++;
    s_last_value = s_api->random->range(1, 100);

    printf("[Shake Demo] Chacoalhada #%u → valor = %d\n",
           (unsigned)s_shake_count, (int)s_last_value);

    /* Bipe de feedback */
    if (s_api->audio) {
        s_api->audio->beep(2000, 60);
    }

    /* Persiste contagem total de chacoalhadas */
    if (s_api->storage) {
        s_api->storage->set_i32("shake_count", (int32_t)s_shake_count);
    }
}

/**
 * Callback de toque — toque manual também gera um valor.
 */
static void on_input(const kit_input_event_t *event, void *user_data)
{
    (void)user_data;
    if (!s_api || event->type != KIT_INPUT_TAP) return;

    s_last_value = s_api->random->range(1, 100);
    printf("[Shake Demo] Toque → valor = %d\n", (int)s_last_value);

    if (s_api->audio) {
        s_api->audio->beep(1500, 40);
    }
}

kit_err_t tool_init(kit_tool_ctx_t *ctx)
{
    if (!ctx || !ctx->api) return KIT_ERR_INVALID_ARG;
    s_api = ctx->api;

    printf("[Shake Demo] tool_init (id=%s)\n", ctx->tool_id);

    /* Restaura contagem anterior */
    if (s_api->storage) {
        int32_t v = 0;
        if (s_api->storage->get_i32("shake_count", &v) == KIT_OK) {
            s_shake_count = (uint32_t)v;
            printf("[Shake Demo] Histórico: %u chacoalhadas anteriores\n",
                   (unsigned)s_shake_count);
        }
    }

    s_screen = s_api->display->get_screen();

    /*
     * No hardware real, a UI seria:
     * - Fundo KIT_COLOR_BG
     * - Título "SHAKE" em kit_mono_26 com KIT_COLOR_TEXT
     * - Número grande em kit_display_120 (centralizado)
     * - Subtítulo "CHACOALHE O KIT" em kit_mono_16 com KIT_COLOR_TEXT_MUTED
     * - Contagem de shakes no rodapé
     */

    /* Registra callback de shake (IMU) */
    if (s_api->imu) {
        s_api->imu->register_shake_callback(on_shake, NULL);
        printf("[Shake Demo] Callback de shake registrado.\n");
    }

    /* Registra callback de toque */
    if (s_api->input) {
        s_api->input->register_callback(on_input, NULL);
    }

    return KIT_OK;
}

void tool_destroy(void)
{
    printf("[Shake Demo] tool_destroy (total shakes=%u)\n", (unsigned)s_shake_count);
    s_screen = NULL;
    s_api = NULL;
}
