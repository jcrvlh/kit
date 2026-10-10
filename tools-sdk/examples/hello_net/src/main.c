/**
 * @file main.c
 * @brief "Olá Rede" — exemplo da API de rede das Tools (runtime >= 0.16.0).
 *
 * Toque na metade de cima: GET no manifesto de firmware do KIT (domínio
 * declarado em "network_domains"). Toque na metade de baixo: GET num domínio
 * fora da lista, para ver o KIT recusar (KIT_ERR_PERMISSION_DENIED).
 *
 * O callback chega no contexto do LVGL: dá pra atualizar a tela direto.
 */

#include "kit_tool_api.h"
#include "kit_fonts.h"

/* Primeira constante global alinhada (ver o README do hello_sd). */
static const char URL_OK[] __attribute__((aligned(4))) =
    "https://jcrvlh.github.io/kit/firmware.json";
static const char URL_FORA[] = "https://example.com/";

static const kit_api_table_t *s_api;
static lv_obj_t *s_screen;
static lv_obj_t *s_label;

static void show(const char *txt)
{
    lv_label_set_text(s_label, txt);
}

static void on_response(kit_err_t result, int http_status,
                        const char *body, size_t len, void *user_data)
{
    (void)user_data;
    if (result != KIT_OK) {
        lv_label_set_text_fmt(s_label, "FALHOU\n\nerro %d", (int)result);
        return;
    }
    /* Mostra o status e o começo do corpo (o body só vale aqui dentro). */
    char preview[121];
    size_t n = len < sizeof(preview) - 1 ? len : sizeof(preview) - 1;
    for (size_t i = 0; i < n; i++) preview[i] = body[i];
    preview[n] = '\0';
    lv_label_set_text_fmt(s_label, "HTTP %d  (%d B)\n\n%s", http_status, (int)len, preview);
}

static void fetch(const char *url)
{
    if (!s_api->net) {
        show("SEM PERMISSAO DE REDE\n\n(manifest)");
        return;
    }
    if (!s_api->net->is_online()) {
        show("SEM WI-FI");
        return;
    }
    kit_err_t r = s_api->net->http_get(url, on_response, 0);
    if (r == KIT_OK)                         show("BUSCANDO...");
    else if (r == KIT_ERR_PERMISSION_DENIED) show("RECUSADO\n\ndominio fora do manifest");
    else if (r == KIT_FAIL)                  show("OCUPADO\n\num pedido por vez");
    else lv_label_set_text_fmt(s_label, "ERRO %d", (int)r);
}

/* O runtime entrega TOUCH_DOWN contínuo enquanto o dedo está na tela (não há
 * TAP). Um toque novo = um DOWN depois de ~250 ms sem nenhum. */
static uint64_t s_last_down_ms;

static void on_input(const kit_input_event_t *ev, void *user_data)
{
    (void)user_data;
    if (ev->type != KIT_INPUT_TOUCH_DOWN) return;
    uint64_t now = s_api->time ? s_api->time->get_millis() : 0;
    bool novo = now - s_last_down_ms > 250;
    s_last_down_ms = now;
    if (novo) fetch(ev->y < 224 ? URL_OK : URL_FORA);
}

KIT_TOOL_EXPORT kit_err_t tool_init(kit_tool_ctx_t *ctx)
{
    if (!ctx || !ctx->api) return KIT_ERR_INVALID_ARG;
    s_api = ctx->api;

    s_screen = lv_obj_create(0);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x000000), 0);
    lv_screen_load(s_screen);

    s_label = lv_label_create(s_screen);
    lv_obj_set_width(s_label, 320);
    lv_label_set_long_mode(s_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(s_label, lv_color_hex(0xF5F0E6), 0);
    lv_obj_set_style_text_font(s_label, &kit_mono_20, 0);
    lv_obj_align(s_label, LV_ALIGN_CENTER, 0, 0);
    show("TOQUE EM CIMA: BUSCAR\n\nTOQUE EMBAIXO: DOMINIO\nFORA DA LISTA");

    if (s_api->input) s_api->input->register_callback(on_input, 0);
    return KIT_OK;
}

KIT_TOOL_EXPORT void tool_destroy(void)
{
    if (s_api && s_api->net) s_api->net->cancel();
    if (s_api && s_api->input) s_api->input->register_callback(0, 0);
    if (s_screen) {
        lv_obj_delete(s_screen);
        s_screen = 0;
    }
}
