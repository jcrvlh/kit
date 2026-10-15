/**
 * @file main.c
 * @brief "Olá Wi-Fi" — exemplo da API de scan de Wi-Fi (runtime >= 0.18.0).
 *
 * Toque na tela: varre as redes em volta e lista as mais fortes — sinal,
 * canal, segurança e nome (oculta = "(oculta)"). Um scan por vez, no máximo
 * um a cada 5 s: tocar antes disso mostra "AGUARDE".
 *
 * O callback chega no contexto do LVGL: dá pra atualizar a tela direto.
 */

#include "kit_tool_api.h"
#include "kit_fonts.h"
#include <stdio.h>

/* Primeira constante global alinhada (ver o README do hello_sd). */
static const char AUTH[][6] __attribute__((aligned(4))) = {
    "ABERT", "WEP", "WPA", "WPA2", "WPA3", "ENT", "?"
};

#define LINHAS 8

static const kit_api_table_t *s_api;
static lv_obj_t *s_screen;
static lv_obj_t *s_title;
static lv_obj_t *s_list;

static void on_scan(kit_err_t result, const kit_wifi_ap_t *aps, size_t count,
                    void *user_data)
{
    (void)user_data;
    if (result != KIT_OK) {
        lv_label_set_text_fmt(s_title, "FALHOU (%d)", (int)result);
        lv_label_set_text(s_list, "TOQUE PRA TENTAR\nDE NOVO");
        return;
    }
    lv_label_set_text_fmt(s_title, "%d REDES", (int)count);

    /* aps só vale aqui dentro: monta o texto agora. */
    static char buf[LINHAS * 48];
    size_t off = 0;
    for (size_t i = 0; i < count && i < LINHAS; i++) {
        const kit_wifi_ap_t *a = &aps[i];
        const char *nome = a->ssid[0] ? a->ssid : "(oculta)";
        uint8_t au = a->auth < 7 ? a->auth : 6;
        int n = snprintf(buf + off, sizeof(buf) - off, "%d %2u %-5s %.16s\n",
                            (int)a->rssi, (unsigned)a->channel, AUTH[au], nome);
        if (n < 0 || (size_t)n >= sizeof(buf) - off) break;
        off += (size_t)n;
    }
    buf[off] = '\0';
    lv_label_set_text(s_list, off ? buf : "NENHUMA REDE");
}

static void scan(void)
{
    if (!s_api->wifi) {
        lv_label_set_text(s_title, "SEM PERMISSAO");
        lv_label_set_text(s_list, "declare \"wifi_scan\"\nno manifest");
        return;
    }
    kit_err_t r = s_api->wifi->scan(on_scan, 0);
    if (r == KIT_OK)        lv_label_set_text(s_title, "PROCURANDO...");
    else if (r == KIT_FAIL) lv_label_set_text(s_title, "AGUARDE");
    else                    lv_label_set_text_fmt(s_title, "ERRO %d", (int)r);
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
    if (novo) scan();
}

KIT_TOOL_EXPORT kit_err_t tool_init(kit_tool_ctx_t *ctx)
{
    if (!ctx || !ctx->api) return KIT_ERR_INVALID_ARG;
    s_api = ctx->api;

    s_screen = lv_obj_create(0);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x000000), 0);
    lv_screen_load(s_screen);

    s_title = lv_label_create(s_screen);
    lv_obj_set_style_text_color(s_title, lv_color_hex(0xEFEADD), 0);
    lv_obj_set_style_text_font(s_title, &kit_mono_26, 0);
    lv_obj_set_style_text_letter_space(s_title, 3, 0);
    lv_obj_align(s_title, LV_ALIGN_TOP_LEFT, 16, 24);
    lv_label_set_text(s_title, "TOQUE PRA VARRER");

    s_list = lv_label_create(s_screen);
    lv_obj_set_width(s_list, 336);
    lv_obj_set_style_text_color(s_list, lv_color_hex(0xEFEADD), 0);
    lv_obj_set_style_text_font(s_list, &kit_mono_16, 0);
    lv_obj_set_style_text_line_space(s_list, 10, 0);
    lv_obj_align(s_list, LV_ALIGN_TOP_LEFT, 16, 80);
    lv_label_set_text(s_list, "");

    if (s_api->input) s_api->input->register_callback(on_input, 0);
    scan();
    return KIT_OK;
}

KIT_TOOL_EXPORT void tool_destroy(void)
{
    if (s_api && s_api->wifi) s_api->wifi->cancel();
    if (s_api && s_api->input) s_api->input->register_callback(0, 0);
    if (s_screen) {
        lv_obj_delete(s_screen);
        s_screen = 0;
    }
    s_title = 0;
    s_list = 0;
}
