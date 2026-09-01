#include "kit_launcher.h"
#include "kit_tool_manager.h"
#include "kit_power.h"
#include "kit_display.h"
#include "kit_audio.h"
#include "kit_config.h"
#include "kit_fonts.h"
#include "kit_theme.h"
#include "esp_log.h"
#include "lvgl.h"
#include <stdio.h>

// Launcher do KIT — linguagem "Brutalist Bauhaus" (ver docs/design/design-language.md).
// Telas: splash "INICIANDO", Home (sem tools), Ajustes, Brilho, Sobre, e um
// overlay de feedback reutilizável (ex: carga iniciada).
//
// Os botões físicos (PWR liga/desliga a tela, BOOT volta para a Home) são
// tratados em kit_runtime; aqui só expomos kit_launcher_go_home().

static const char *TAG = "KIT_LAUNCHER";

#define KIT_PAD       16
#define KIT_CONTENT   (KIT_DISPLAY_WIDTH - 2 * KIT_PAD)
#define KIT_CHIP      64
#define KIT_TITLEBAR  88
#define KIT_BTN_H     KIT_TOUCH_TARGET_COMFORTABLE   // 80
#define KIT_ROW_H     88

static lv_obj_t *s_launcher_screen = NULL;
static lv_obj_t *s_splash_screen = NULL;
static lv_obj_t *s_settings_screen = NULL;
static lv_obj_t *s_brightness_screen = NULL;
static lv_obj_t *s_volume_screen = NULL;
static lv_obj_t *s_sleep_screen = NULL;
static lv_obj_t *s_poweroff_screen = NULL;
static lv_obj_t *s_about_screen = NULL;
static lv_obj_t *s_home_deck = NULL;       // lv_tileview horizontal — slideshow de Tools
static lv_obj_t *s_home_dots_box = NULL;   // fileira de pontos de página (rodapé da Home)
static lv_obj_t *s_feedback_screen = NULL;

extern const lv_image_dsc_t kit_icon_triangle_a8;

static lv_obj_t *s_brightness_val_lbl = NULL;
static lv_obj_t *s_volume_val_lbl = NULL;
static lv_obj_t *s_sound_val_lbl = NULL;
static lv_obj_t *s_batt_lbl = NULL;
static lv_obj_t *s_batt_fill = NULL;
static lv_obj_t *s_toast = NULL;

static bool s_was_charging = false;

// Grade de Tools da Home. Cada Tool só entra aqui quando já está implementada
// (o campo `available` cobre o caso de uma Tool em desenvolvimento, que aparece
// esmaecida e responde com "EM BREVE"). As demais Tools da Fase 2 entram nesta
// lista à medida que forem feitas.
typedef enum {
    TOOL_ICON_DICE, TOOL_ICON_SPIN, TOOL_ICON_COIN, TOOL_ICON_TRIANGLE,
    TOOL_ICON_BINGO, TOOL_ICON_ORDER, TOOL_ICON_TIMER, TOOL_ICON_FIRST,
    TOOL_ICON_TEAMS, TOOL_ICON_ASK
} tool_icon_t;

typedef struct {
    const char *id;
    const char *label;
    uint32_t    color;
    tool_icon_t icon;
    bool        available;
} home_tool_t;

static const home_tool_t HOME_TOOLS[] = {
    { "com.kit.dice",    "Dados",   KIT_COLOR_RED,    TOOL_ICON_DICE, true },
    { "com.kit.bottle",  "Garrafa", KIT_COLOR_BLUE,   TOOL_ICON_SPIN, true },
    { "com.kit.coin",    "Moeda",   KIT_COLOR_YELLOW, TOOL_ICON_COIN, true },
    { "com.kit.timer",   "Timer",   KIT_COLOR_GREEN,  TOOL_ICON_TIMER, true },
    { "com.kit.primeiro","Primeiro",KIT_COLOR_YELLOW, TOOL_ICON_FIRST, true },
    { "com.kit.times",   "Times",   KIT_COLOR_BLUE,   TOOL_ICON_TEAMS, true },
    { "com.kit.bingo",   "Bingo",   KIT_COLOR_GREEN,  TOOL_ICON_BINGO, true },
    { "com.kit.quebragelo", "Quebra-Gelo", KIT_COLOR_RED, TOOL_ICON_ASK, true },
};
#define HOME_TOOLS_N ((int)(sizeof(HOME_TOOLS) / sizeof(HOME_TOOLS[0])))

// -- Slideshow da Home ----------------------------------------------------
// A Home é um lv_tileview horizontal: os HOME_MRU_SLOTS primeiros slides são as
// Tools usadas mais recentemente (a mais recente primeiro) e o último slide é
// "VER TODOS" — a grade completa. A ordem de recência é persistida em NVS
// (kit_config, chaves "home_mru0".."home_mru2", valor = índice em HOME_TOOLS + 1).
#define HOME_MRU_SLOTS 3
#define HOME_STATUS_H  72
#define HOME_DOTS_H    40
#define HOME_DECK_H    (KIT_DISPLAY_HEIGHT - HOME_STATUS_H - HOME_DOTS_H)

static int      s_mru[HOME_TOOLS_N];                 // índices em HOME_TOOLS, recente primeiro
static int      s_mru_n = 0;
static lv_obj_t *s_home_tiles[HOME_MRU_SLOTS + 1];
static lv_obj_t *s_home_dots[HOME_MRU_SLOTS + 1];
static int      s_home_slides = 0;
static bool     s_home_deck_dirty = false;           // pediu rebuild ao voltar pra Home

// Reconstrói a lista de recência a partir do NVS; Tools sem histórico entram no
// fim, na ordem de declaração de HOME_TOOLS.
static void home_mru_load(void)
{
    bool seen[HOME_TOOLS_N] = { 0 };
    s_mru_n = 0;
    for (int slot = 0; slot < HOME_MRU_SLOTS; slot++) {
        char key[16];
        snprintf(key, sizeof(key), "home_mru%d", slot);
        uint8_t v = 0;
        kit_config_get_u8(key, &v, 0);
        int idx = (int)v - 1;
        if (idx >= 0 && idx < HOME_TOOLS_N && !seen[idx]) {
            seen[idx] = true;
            s_mru[s_mru_n++] = idx;
        }
    }
    for (int i = 0; i < HOME_TOOLS_N; i++)
        if (!seen[i]) s_mru[s_mru_n++] = i;
}

static void home_mru_save(void)
{
    for (int slot = 0; slot < HOME_MRU_SLOTS; slot++) {
        char key[16];
        snprintf(key, sizeof(key), "home_mru%d", slot);
        uint8_t v = (slot < s_mru_n) ? (uint8_t)(s_mru[slot] + 1) : 0;
        kit_config_set_u8(key, v);
    }
}

// Sobe a Tool `idx` para o topo da recência e persiste. Marca o deck para
// reconstruir quando o usuário voltar para a Home.
static void home_mru_touch(int idx)
{
    int pos = -1;
    for (int i = 0; i < s_mru_n; i++)
        if (s_mru[i] == idx) { pos = i; break; }
    if (pos < 0) return;
    for (int i = pos; i > 0; i--) s_mru[i] = s_mru[i - 1];
    s_mru[0] = idx;
    home_mru_save();
    s_home_deck_dirty = true;
}

static void open_settings_cb(lv_event_t *e);
static void close_settings_cb(lv_event_t *e);
static void open_brightness_cb(lv_event_t *e);
static void close_brightness_cb(lv_event_t *e);
static void open_volume_cb(lv_event_t *e);
static void close_volume_cb(lv_event_t *e);
static void open_sleep_cb(lv_event_t *e);
static void open_poweroff_cb(lv_event_t *e);
static void open_about_cb(lv_event_t *e);
static void close_about_cb(lv_event_t *e);
static void brightness_slider_cb(lv_event_t *e);
static void brightness_released_cb(lv_event_t *e);
static void volume_slider_cb(lv_event_t *e);
static void volume_released_cb(lv_event_t *e);
static void sound_toggle_cb(lv_event_t *e);
static void run_test_tool_cb(lv_event_t *e);
static void home_tile_cb(lv_event_t *e);

// ---------------------------------------------------------------------------
// Blocos reutilizáveis
// ---------------------------------------------------------------------------

static lv_obj_t *add_label(lv_obj_t *parent, const char *txt, uint32_t color,
                           const lv_font_t *font, int letter_space)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(l, font, 0);
    if (letter_space) lv_obj_set_style_text_letter_space(l, letter_space, 0);
    return l;
}

static lv_obj_t *make_group(lv_obj_t *parent, lv_flex_flow_t flow)
{
    lv_obj_t *g = lv_obj_create(parent);
    lv_obj_remove_style_all(g);
    lv_obj_set_size(g, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(g, flow);
    lv_obj_set_flex_align(g, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(g, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    return g;
}

// Trio de primitivas do KIT (a logo). Todas do mesmo tamanho visual.
// O triângulo é o glifo caret-up (KIT_ICON_TRIANGLE) direto — SEM
// transform_rotation: objeto transformado faz o LVGL alocar layer buffer por
// frame no CO5300/PSRAM, render > 5 s e o task watchdog reinicia a board
// (mesma regra da Dice/Bottle/Decisor).
static lv_obj_t *make_logo_trio(lv_obj_t *parent)
{
    lv_obj_t *trio = make_group(parent, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(trio, 16, 0);
    add_label(trio, KIT_ICON_SQUARE,   KIT_COLOR_RED,    &kit_display_44, 0);
    add_label(trio, KIT_ICON_CIRCLE,   KIT_COLOR_BLUE,   &kit_display_44, 0);
    
    // Triângulo (Amarelo) - Agora usando imagem gerada para ser equilátero e não distorcido
    lv_obj_t *img_tri = lv_image_create(trio);
    lv_image_set_src(img_tri, &kit_icon_triangle_a8);
    // Para colorir uma imagem A8, ativamos o recolor no LVGL
    lv_obj_set_style_image_recolor_opa(img_tri, LV_OPA_COVER, 0);
    lv_obj_set_style_image_recolor(img_tri, lv_color_hex(KIT_COLOR_YELLOW), 0);
    return trio;
}

static lv_obj_t *make_overlay(uint32_t bg)
{
    lv_obj_t *o = lv_obj_create(s_launcher_screen);
    lv_obj_set_size(o, KIT_DISPLAY_WIDTH, KIT_DISPLAY_HEIGHT);
    lv_obj_set_pos(o, 0, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static lv_obj_t *make_scroll_body(lv_obj_t *overlay, int bottom_reserve)
{
    lv_obj_t *b = lv_obj_create(overlay);
    lv_obj_set_size(b, KIT_DISPLAY_WIDTH, KIT_DISPLAY_HEIGHT - KIT_TITLEBAR - bottom_reserve);
    lv_obj_set_pos(b, 0, KIT_TITLEBAR);
    lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_radius(b, 0, 0);
    lv_obj_set_style_pad_left(b, KIT_PAD, 0);
    lv_obj_set_style_pad_right(b, KIT_PAD, 0);
    lv_obj_set_style_pad_top(b, 6, 0);
    lv_obj_set_style_pad_bottom(b, 20, 0);
    lv_obj_set_style_pad_row(b, 12, 0);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(b, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(b, LV_SCROLLBAR_MODE_AUTO);
    return b;
}

static lv_obj_t *make_chip(lv_obj_t *parent, lv_event_cb_t cb)
{
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_set_size(b, KIT_CHIP, KIT_CHIP);
    lv_obj_set_style_bg_color(b, lv_color_hex(KIT_COLOR_SURFACE), 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_radius(b, 18, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    if (cb) {
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(b, 12);
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    }
    return b;
}

static void make_titlebar(lv_obj_t *screen, const char *title, lv_event_cb_t back_cb)
{
    lv_obj_t *chip = make_chip(screen, back_cb);
    lv_obj_align(chip, LV_ALIGN_TOP_LEFT, KIT_PAD, 16);
    lv_obj_t *g = add_label(chip, KIT_ICON_BACK, KIT_COLOR_TEXT, &kit_display_44, 0);
    lv_obj_center(g);

    lv_obj_t *t = add_label(screen, title, KIT_COLOR_TEXT, &kit_mono_26, 3);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, KIT_PAD + KIT_CHIP + 12, 32);
}

static lv_obj_t *make_button(lv_obj_t *parent, const char *txt, lv_event_cb_t cb, bool primary)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_size(b, KIT_CONTENT, KIT_BTN_H);
    lv_obj_set_style_radius(b, KIT_BTN_H / 2, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_ext_click_area(b, 8);
    if (primary) {
        lv_obj_set_style_bg_color(b, lv_color_hex(KIT_COLOR_YELLOW), 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    } else {
        lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(b, 2, 0);
        lv_obj_set_style_border_color(b, lv_color_hex(KIT_COLOR_TEXT), 0);
    }
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l = add_label(b, txt, primary ? KIT_COLOR_ON_YELLOW : KIT_COLOR_TEXT, &kit_mono_20, 2);
    lv_obj_center(l);
    return b;
}

static void make_row(lv_obj_t *parent, const char *shape, uint32_t shape_color,
                     const char *label, bool selected, lv_event_cb_t cb,
                     void *user_data)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, KIT_CONTENT, KIT_ROW_H);
    lv_obj_set_style_bg_color(row, lv_color_hex(selected ? KIT_COLOR_TEXT : KIT_COLOR_SURFACE), 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, 24, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(row, 6);
    lv_obj_add_event_cb(row, cb, LV_EVENT_CLICKED, user_data);

    if (shape) {
        lv_obj_t *badge = lv_obj_create(row);
        lv_obj_set_size(badge, 52, 52);
        lv_obj_set_style_bg_color(badge, lv_color_hex(selected ? KIT_COLOR_BG : KIT_COLOR_SURFACE_ALT), 0);
        lv_obj_set_style_border_width(badge, 0, 0);
        lv_obj_set_style_radius(badge, 14, 0);
        lv_obj_set_style_pad_all(badge, 0, 0);
        lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(badge, LV_ALIGN_LEFT_MID, 16, 0);
        lv_obj_t *sh = add_label(badge, shape, shape_color, &kit_display_44, 0);
        lv_obj_center(sh);
    }

    lv_obj_t *l = add_label(row, label, selected ? KIT_COLOR_BG : KIT_COLOR_TEXT, &kit_sans_22, 0);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, shape ? 84 : 24, 0);

    lv_obj_t *ch = add_label(row, KIT_ICON_CHEVRON,
                             selected ? KIT_COLOR_BG : KIT_COLOR_TEXT_MUTED, &kit_mono_26, 0);
    lv_obj_align(ch, LV_ALIGN_RIGHT_MID, -20, 0);
}

static void make_spec(lv_obj_t *parent, const char *k, const char *v)
{
    lv_obj_t *r = lv_obj_create(parent);
    lv_obj_set_size(r, KIT_CONTENT, 46);
    lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(r, 1, 0);
    lv_obj_set_style_border_side(r, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(r, lv_color_hex(KIT_COLOR_LINE), 0);
    lv_obj_set_style_radius(r, 0, 0);
    lv_obj_set_style_pad_all(r, 0, 0);
    lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lk = add_label(r, k, KIT_COLOR_TEXT_MUTED, &kit_mono_16, 1);
    lv_obj_align(lk, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t *lv = add_label(r, v, KIT_COLOR_TEXT, &kit_mono_16, 0);
    lv_obj_align(lv, LV_ALIGN_RIGHT_MID, 0, 0);
}

// Indicador de bateria: [ 82% ][███  ]▐  — desenhado, sem glifo.
static void make_battery(lv_obj_t *parent)
{
    lv_obj_t *g = make_group(parent, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(g, 6, 0);
    lv_obj_align(g, LV_ALIGN_TOP_RIGHT, -KIT_PAD, 34);

    s_batt_lbl = add_label(g, "--%", KIT_COLOR_TEXT, &kit_mono_16, 1);

    lv_obj_t *body = lv_obj_create(g);
    lv_obj_set_size(body, 30, 15);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 2, 0);
    lv_obj_set_style_border_color(body, lv_color_hex(KIT_COLOR_TEXT_MUTED), 0);
    lv_obj_set_style_radius(body, 3, 0);
    lv_obj_set_style_pad_all(body, 2, 0);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);

    s_batt_fill = lv_obj_create(body);
    lv_obj_set_style_bg_color(s_batt_fill, lv_color_hex(KIT_COLOR_TEXT), 0);
    lv_obj_set_style_border_width(s_batt_fill, 0, 0);
    lv_obj_set_style_radius(s_batt_fill, 1, 0);
    lv_obj_set_style_pad_all(s_batt_fill, 0, 0);
    lv_obj_clear_flag(s_batt_fill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_height(s_batt_fill, lv_pct(100));
    lv_obj_set_width(s_batt_fill, lv_pct(60));
    lv_obj_align(s_batt_fill, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *nub = lv_obj_create(g);
    lv_obj_set_size(nub, 3, 8);
    lv_obj_set_style_bg_color(nub, lv_color_hex(KIT_COLOR_TEXT_MUTED), 0);
    lv_obj_set_style_border_width(nub, 0, 0);
    lv_obj_set_style_radius(nub, 1, 0);
    lv_obj_clear_flag(nub, LV_OBJ_FLAG_SCROLLABLE);
}

static void update_battery(void)
{
    if (!s_batt_lbl) return;
    uint8_t p = kit_power_get_battery_percentage();
    bool charging = kit_power_is_charging();

    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", p);
    lv_label_set_text(s_batt_lbl, buf);

    if (s_batt_fill) {
        lv_obj_set_width(s_batt_fill, lv_pct(p < 8 ? 8 : p));
        uint32_t c = charging ? KIT_COLOR_GREEN : (p <= 15 ? KIT_COLOR_RED : KIT_COLOR_TEXT);
        lv_obj_set_style_bg_color(s_batt_fill, lv_color_hex(c), 0);
    }
}

// ---------------------------------------------------------------------------
// Overlay de feedback (ex: carga iniciada) — disco escuro + ícone + rótulo,
// some sozinho.
// ---------------------------------------------------------------------------

static void feedback_timer_cb(lv_timer_t *t)
{
    if (s_feedback_screen) {
        lv_obj_delete(s_feedback_screen);
        s_feedback_screen = NULL;
    }
    lv_timer_delete(t);
}

static void show_feedback(uint32_t bg, const char *icon, const char *label)
{
    if (s_feedback_screen) return;
    s_feedback_screen = make_overlay(bg);

    lv_obj_t *col = make_group(s_feedback_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, 24, 0);
    lv_obj_center(col);

    lv_obj_t *disc = lv_obj_create(col);
    lv_obj_set_size(disc, 132, 132);
    lv_obj_set_style_bg_color(disc, lv_color_hex(KIT_COLOR_BG), 0);
    lv_obj_set_style_border_width(disc, 0, 0);
    lv_obj_set_style_radius(disc, 66, 0);
    lv_obj_set_style_pad_all(disc, 0, 0);
    lv_obj_clear_flag(disc, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(disc, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    // Ícone no tamanho nativo (kit_display_44) — SEM transform_scale: escalar
    // um objeto faz o LVGL alocar layer buffer por frame no CO5300/PSRAM e o
    // task watchdog reinicia a board (mesma regra da Dice/Bottle/Decisor).
    lv_obj_t *ic = add_label(disc, icon, bg, &kit_display_44, 0);
    lv_obj_center(ic);

    add_label(col, label, KIT_COLOR_BG, &kit_mono_20, 5);

    lv_timer_t *t = lv_timer_create(feedback_timer_cb, 1700, NULL);
    lv_timer_set_repeat_count(t, 1);
}

// ---------------------------------------------------------------------------
// Splash "INICIANDO"
// ---------------------------------------------------------------------------

static void splash_done_cb(lv_timer_t *t)
{
    if (s_splash_screen) {
        lv_obj_delete(s_splash_screen);
        s_splash_screen = NULL;
    }
    lv_timer_delete(t);
}

static void build_splash(void)
{
    s_splash_screen = make_overlay(KIT_COLOR_BG);

    lv_obj_t *col = make_group(s_splash_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, 18, 0);
    lv_obj_center(col);

    make_logo_trio(col);
    add_label(col, "KIT", KIT_COLOR_TEXT, &kit_display_44, 4);
    add_label(col, "INICIANDO", KIT_COLOR_TEXT_MUTED, &kit_mono_20, 6);
}

// ---------------------------------------------------------------------------
// Home
// ---------------------------------------------------------------------------

// -- Toast transitório (aviso curto no rodapé da Home) --

static void toast_timer_cb(lv_timer_t *t)
{
    if (s_toast) { lv_obj_delete(s_toast); s_toast = NULL; }
    lv_timer_delete(t);
}

static void show_toast(const char *msg)
{
    if (s_toast) { lv_obj_delete(s_toast); s_toast = NULL; }
    s_toast = add_label(s_launcher_screen, msg, KIT_COLOR_BG, &kit_mono_20, 3);
    lv_obj_set_style_bg_color(s_toast, lv_color_hex(KIT_COLOR_TEXT), 0);
    lv_obj_set_style_bg_opa(s_toast, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_toast, 16, 0);
    lv_obj_set_style_pad_hor(s_toast, 18, 0);
    lv_obj_set_style_pad_ver(s_toast, 12, 0);
    lv_obj_align(s_toast, LV_ALIGN_BOTTOM_MID, 0, -24);
    lv_timer_t *t = lv_timer_create(toast_timer_cb, 1400, NULL);
    lv_timer_set_repeat_count(t, 1);
}

// -- Ícones das Tools: composições geométricas simples (linha, sem glifo
//    dedicado), na cor de contraste do card. --

static lv_obj_t *icon_shape(lv_obj_t *p, int w, int h, uint32_t color,
                            int radius, int border)
{
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    if (border) {
        lv_obj_set_style_border_width(o, border, 0);
        lv_obj_set_style_border_color(o, lv_color_hex(color), 0);
    } else {
        lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
        lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    }
    return o;
}

static void make_tool_icon(lv_obj_t *badge, tool_icon_t kind, uint32_t color)
{
    lv_obj_t *ic = lv_obj_create(badge);
    lv_obj_remove_style_all(ic);
    lv_obj_set_size(ic, 24, 24);
    lv_obj_clear_flag(ic, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ic, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_center(ic);

    switch (kind) {
    case TOOL_ICON_DICE: {
        lv_obj_t *sq = icon_shape(ic, 20, 20, color, 5, 3);
        lv_obj_center(sq);
        lv_obj_align(icon_shape(ic, 4, 4, color, 2, 0), LV_ALIGN_CENTER, -4, -4);
        lv_obj_align(icon_shape(ic, 4, 4, color, 2, 0), LV_ALIGN_CENTER,  4,  4);
        break;
    }
    case TOOL_ICON_SPIN: {
        lv_obj_t *ring = icon_shape(ic, 20, 20, color, LV_RADIUS_CIRCLE, 3);
        lv_obj_center(ring);
        lv_obj_align(icon_shape(ic, 3, 8, color, 2, 0), LV_ALIGN_CENTER, 0, -4);
        lv_obj_align(icon_shape(ic, 5, 5, color, 3, 0), LV_ALIGN_CENTER, 0, 0);
        break;
    }
    case TOOL_ICON_COIN: {
        lv_obj_t *disc = icon_shape(ic, 20, 20, color, LV_RADIUS_CIRCLE, 3);
        lv_obj_center(disc);
        lv_obj_align(icon_shape(ic, 3, 22, color, 1, 0), LV_ALIGN_CENTER, 0, 0);
        break;
    }
    case TOOL_ICON_TRIANGLE: {
        lv_obj_t *tr = add_label(ic, KIT_ICON_TRIANGLE, color, &kit_mono_26, 0);
        lv_obj_center(tr);
        break;
    }
    case TOOL_ICON_BINGO: {
        lv_obj_align(icon_shape(ic, 7, 7, color, 4, 0), LV_ALIGN_CENTER, -5, -5);
        lv_obj_align(icon_shape(ic, 7, 7, color, 4, 0), LV_ALIGN_CENTER,  5, -5);
        lv_obj_align(icon_shape(ic, 7, 7, color, 4, 0), LV_ALIGN_CENTER, -5,  5);
        lv_obj_align(icon_shape(ic, 7, 7, color, 4, 0), LV_ALIGN_CENTER,  5,  5);
        break;
    }
    case TOOL_ICON_ORDER: {
        lv_obj_align(icon_shape(ic, 24, 4, color, 2, 0), LV_ALIGN_TOP_LEFT, 0, 3);
        lv_obj_align(icon_shape(ic, 16, 4, color, 2, 0), LV_ALIGN_TOP_LEFT, 0, 11);
        lv_obj_align(icon_shape(ic,  9, 4, color, 2, 0), LV_ALIGN_TOP_LEFT, 0, 19);
        break;
    }
    case TOOL_ICON_TIMER: {
        lv_obj_t *ring = icon_shape(ic, 20, 20, color, LV_RADIUS_CIRCLE, 3);
        lv_obj_align(ring, LV_ALIGN_CENTER, 0, 2);
        lv_obj_align(icon_shape(ic, 6, 3, color, 1, 0), LV_ALIGN_CENTER, 0, -10);
        lv_obj_align(icon_shape(ic, 2, 7, color, 1, 0), LV_ALIGN_CENTER, 0, -1);
        lv_obj_align(icon_shape(ic, 6, 2, color, 1, 0), LV_ALIGN_CENTER, 3, 3);
        break;
    }
    case TOOL_ICON_FIRST: {
        // seta apontando para a pessoa escolhida
        lv_obj_t *car = add_label(ic, KIT_ICON_CHEVRON, color, &kit_mono_26, 0);
        lv_obj_align(car, LV_ALIGN_LEFT_MID, -2, 0);
        lv_obj_align(icon_shape(ic, 13, 13, color, LV_RADIUS_CIRCLE, 0),
                     LV_ALIGN_RIGHT_MID, 0, 0);
        break;
    }
    case TOOL_ICON_TEAMS: {
        // quadrado dividido em dois — dois times
        lv_obj_align(icon_shape(ic, 10, 22, color, 3, 0), LV_ALIGN_CENTER, -6, 0);
        lv_obj_align(icon_shape(ic, 10, 22, color, 3, 3), LV_ALIGN_CENTER,  6, 0);
        break;
    }
    case TOOL_ICON_ASK: {
        // balão de fala com reticências — conversa
        lv_obj_align(icon_shape(ic, 24, 16, color, 6, 3), LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_align(icon_shape(ic, 6, 6, color, 1, 0), LV_ALIGN_BOTTOM_LEFT, 3, 0);
        lv_obj_align(icon_shape(ic, 3, 3, color, 2, 0), LV_ALIGN_TOP_MID, -6, 6);
        lv_obj_align(icon_shape(ic, 3, 3, color, 2, 0), LV_ALIGN_TOP_MID,  0, 6);
        lv_obj_align(icon_shape(ic, 3, 3, color, 2, 0), LV_ALIGN_TOP_MID,  6, 6);
        break;
    }
    }
}

static void make_tool_tile(lv_obj_t *grid, int index)
{
    const home_tool_t *tool = &HOME_TOOLS[index];
    bool on = tool->color == KIT_COLOR_YELLOW;
    uint32_t ink = on ? KIT_COLOR_ON_YELLOW : KIT_COLOR_ON_COLOR;

    lv_obj_t *tile = lv_obj_create(grid);
    lv_obj_set_size(tile, 162, 118);
    lv_obj_set_style_bg_color(tile, lv_color_hex(tool->color), 0);
    lv_obj_set_style_bg_opa(tile, tool->available ? LV_OPA_COVER : LV_OPA_40, 0);
    lv_obj_set_style_border_width(tile, 0, 0);
    lv_obj_set_style_radius(tile, 20, 0);
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(tile, 4);
    lv_obj_add_event_cb(tile, home_tile_cb, LV_EVENT_CLICKED, (void *)(intptr_t)index);

    lv_obj_t *badge = lv_obj_create(tile);
    lv_obj_set_size(badge, 42, 42);
    lv_obj_set_style_bg_color(badge, lv_color_hex(ink), 0);
    lv_obj_set_style_bg_opa(badge, on ? LV_OPA_20 : LV_OPA_30, 0);
    lv_obj_set_style_border_width(badge, 0, 0);
    lv_obj_set_style_radius(badge, 12, 0);
    lv_obj_set_style_pad_all(badge, 0, 0);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(badge, LV_ALIGN_TOP_LEFT, 14, 14);
    make_tool_icon(badge, tool->icon, ink);

    char num[8];
    snprintf(num, sizeof(num), "%02d", (index + 1) % 100);
    lv_obj_t *n = add_label(tile, num, ink, &kit_mono_26, 1);
    lv_obj_set_style_text_opa(n, LV_OPA_40, 0);
    lv_obj_align(n, LV_ALIGN_TOP_RIGHT, -14, 16);

    lv_obj_t *lbl = add_label(tile, tool->label, ink, &kit_sans_22, 0);
    lv_obj_align(lbl, LV_ALIGN_BOTTOM_LEFT, 14, -14);
}

// -- Card de Ajustes: sempre o último da grade "VER TODOS", em cinza. --
static void make_settings_tile(lv_obj_t *grid)
{
    lv_obj_t *tile = lv_obj_create(grid);
    lv_obj_set_size(tile, 162, 118);
    lv_obj_set_style_bg_color(tile, lv_color_hex(KIT_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tile, 0, 0);
    lv_obj_set_style_radius(tile, 20, 0);
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(tile, 4);
    lv_obj_add_event_cb(tile, open_settings_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *badge = lv_obj_create(tile);
    lv_obj_set_size(badge, 42, 42);
    lv_obj_set_style_bg_color(badge, lv_color_hex(KIT_COLOR_TEXT_MUTED), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_20, 0);
    lv_obj_set_style_border_width(badge, 0, 0);
    lv_obj_set_style_radius(badge, 12, 0);
    lv_obj_set_style_pad_all(badge, 0, 0);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(badge, LV_ALIGN_TOP_LEFT, 14, 14);

    lv_obj_t *ring = lv_obj_create(badge);
    lv_obj_set_size(ring, 22, 22);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ring, 3, 0);
    lv_obj_set_style_border_color(ring, lv_color_hex(KIT_COLOR_TEXT_MUTED), 0);
    lv_obj_set_style_radius(ring, 11, 0);
    lv_obj_set_style_pad_all(ring, 0, 0);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(ring);

    lv_obj_t *lbl = add_label(tile, "Ajustes", KIT_COLOR_TEXT_MUTED, &kit_sans_22, 0);
    lv_obj_align(lbl, LV_ALIGN_BOTTOM_LEFT, 14, -14);
}

// -- Slide de destaque: uma Tool por tela, ocupando tudo na cor dela. `slot` é
//    a posição no slideshow (marca-d'água "01".."04"). --
static void make_tool_slide(lv_obj_t *tile, int index, int slot)
{
    const home_tool_t *tool = &HOME_TOOLS[index];
    bool on = tool->color == KIT_COLOR_YELLOW;
    uint32_t ink = on ? KIT_COLOR_ON_YELLOW : KIT_COLOR_ON_COLOR;

    lv_obj_t *card = lv_obj_create(tile);
    lv_obj_set_size(card, KIT_CONTENT, HOME_DECK_H - 12);
    lv_obj_center(card);
    lv_obj_set_style_bg_color(card, lv_color_hex(tool->color), 0);
    lv_obj_set_style_bg_opa(card, tool->available ? LV_OPA_COVER : LV_OPA_40, 0);
    lv_obj_set_style_bg_opa(card, tool->available ? LV_OPA_90 : LV_OPA_40, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 30, 0);
    lv_obj_set_style_pad_all(card, 22, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(card, 4);
    lv_obj_add_event_cb(card, home_tile_cb, LV_EVENT_CLICKED, (void *)(intptr_t)index);

    lv_obj_t *badge = lv_obj_create(card);
    lv_obj_set_size(badge, 52, 52);
    lv_obj_set_style_bg_color(badge, lv_color_hex(ink), 0);
    lv_obj_set_style_bg_opa(badge, on ? LV_OPA_20 : LV_OPA_30, 0);
    lv_obj_set_style_border_width(badge, 0, 0);
    lv_obj_set_style_radius(badge, 15, 0);
    lv_obj_set_style_pad_all(badge, 0, 0);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(badge, LV_ALIGN_TOP_LEFT, 0, 0);
    make_tool_icon(badge, tool->icon, ink);

    char num[8];
    snprintf(num, sizeof(num), "%02d", (slot + 1) % 100);
    lv_obj_t *n = add_label(card, num, ink, &kit_display_72, 0);
    lv_obj_set_style_text_opa(n, LV_OPA_30, 0);
    lv_obj_align(n, LV_ALIGN_TOP_RIGHT, 2, -4);

    lv_obj_t *lbl = add_label(card, tool->label, ink, &kit_sans_22, 0);
    lv_obj_align(lbl, LV_ALIGN_BOTTOM_LEFT, 0, -26);

    lv_obj_t *hint = add_label(card, tool->available ? "TOQUE PARA ABRIR" : "EM BREVE",
                               ink, &kit_mono_16, 2);
    lv_obj_set_style_text_opa(hint, LV_OPA_60, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_LEFT, 0, 0);
}

// -- Último slide: "VER TODOS" — a grade completa, rola na vertical. --
static void make_all_slide(lv_obj_t *tile)
{
    lv_obj_t *hdr = add_label(tile, "TODAS AS TOOLS", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 3);
    lv_obj_align(hdr, LV_ALIGN_TOP_LEFT, KIT_PAD, 4);

    lv_obj_t *grid = lv_obj_create(tile);
    lv_obj_set_size(grid, KIT_DISPLAY_WIDTH, HOME_DECK_H - 34);
    lv_obj_set_pos(grid, 0, 30);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_radius(grid, 0, 0);
    lv_obj_set_style_pad_left(grid, KIT_PAD, 0);
    lv_obj_set_style_pad_right(grid, KIT_PAD, 0);
    lv_obj_set_style_pad_top(grid, 2, 0);
    lv_obj_set_style_pad_bottom(grid, 16, 0);
    lv_obj_set_style_pad_row(grid, 12, 0);
    lv_obj_set_style_pad_column(grid, 12, 0);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scroll_dir(grid, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(grid, LV_SCROLLBAR_MODE_AUTO);

    for (int i = 0; i < HOME_TOOLS_N; i++) make_tool_tile(grid, i);
    make_settings_tile(grid);
}

// Ponto de página ativo = traço claro; os demais = fio.
static void home_sync_dots(void)
{
    if (!s_home_deck || !s_home_dots_box) return;   // ainda montando o deck
    lv_obj_t *act = lv_tileview_get_tile_active(s_home_deck);
    for (int i = 0; i < s_home_slides; i++) {
        bool cur = (act == s_home_tiles[i]);
        lv_obj_set_size(s_home_dots[i], cur ? 20 : 8, 8);
        lv_obj_set_style_bg_color(s_home_dots[i],
            lv_color_hex(cur ? KIT_COLOR_TEXT : KIT_COLOR_LINE), 0);
    }
}

static void home_deck_changed_cb(lv_event_t *e)
{
    (void)e;
    home_sync_dots();
}

static void home_clear_deck(void)
{
    if (s_home_deck)     { lv_obj_delete(s_home_deck);     s_home_deck = NULL; }
    if (s_home_dots_box) { lv_obj_delete(s_home_dots_box); s_home_dots_box = NULL; }
    s_home_slides = 0;
}

// Monta o slideshow (tileview horizontal) + os pontos de página, a partir da
// ordem de recência atual em s_mru.
static void home_build_deck(void)
{
    int n_tools = s_mru_n < HOME_MRU_SLOTS ? s_mru_n : HOME_MRU_SLOTS;
    s_home_slides = n_tools + 1;   // + "VER TODOS"

    s_home_deck = lv_tileview_create(s_launcher_screen);
    lv_obj_set_size(s_home_deck, KIT_DISPLAY_WIDTH, HOME_DECK_H);
    lv_obj_set_pos(s_home_deck, 0, HOME_STATUS_H);
    lv_obj_set_style_bg_opa(s_home_deck, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_home_deck, 0, 0);
    lv_obj_set_scrollbar_mode(s_home_deck, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(s_home_deck, home_deck_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    for (int i = 0; i < n_tools; i++) {
        s_home_tiles[i] = lv_tileview_add_tile(s_home_deck, i, 0, LV_DIR_HOR);
        make_tool_slide(s_home_tiles[i], s_mru[i], i);
    }
    s_home_tiles[n_tools] = lv_tileview_add_tile(s_home_deck, n_tools, 0, LV_DIR_HOR);
    make_all_slide(s_home_tiles[n_tools]);

    s_home_dots_box = lv_obj_create(s_launcher_screen);
    lv_obj_remove_style_all(s_home_dots_box);
    lv_obj_set_size(s_home_dots_box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_home_dots_box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_home_dots_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_home_dots_box, 8, 0);
    lv_obj_clear_flag(s_home_dots_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(s_home_dots_box, LV_ALIGN_BOTTOM_MID, 0, -14);

    for (int i = 0; i < s_home_slides; i++) {
        lv_obj_t *d = lv_obj_create(s_home_dots_box);
        lv_obj_remove_style_all(d);
        lv_obj_set_size(d, 8, 8);
        lv_obj_set_style_radius(d, 4, 0);
        lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(d, lv_color_hex(KIT_COLOR_LINE), 0);
        s_home_dots[i] = d;
    }
    home_sync_dots();
}

static void build_home(lv_obj_t *s)
{
    // Topo da Home: só a marca "KIT" à esquerda e a bateria à direita, alinhadas
    // uma de cada lado. Ajustes agora vive no fim da grade "VER TODOS".
    lv_obj_t *brand = add_label(s, "KIT", KIT_COLOR_TEXT, &kit_mono_26, 4);
    lv_obj_align(brand, LV_ALIGN_TOP_LEFT, KIT_PAD, 30);

    make_battery(s);

    home_mru_load();
    home_build_deck();
}

// ---------------------------------------------------------------------------
// Ajustes
// ---------------------------------------------------------------------------

// Linha de liga/desliga do som: rótulo à esquerda, estado à direita (sem
// chevron). Toca no próprio rótulo para alternar, sem trocar de tela.
static void sync_sound_row(void)
{
    if (!s_sound_val_lbl) return;
    bool on = kit_config_get_sound_enabled();
    lv_label_set_text(s_sound_val_lbl, on ? "LIGADO" : "DESLIGADO");
    lv_obj_set_style_text_color(s_sound_val_lbl,
        lv_color_hex(on ? KIT_COLOR_GREEN : KIT_COLOR_TEXT_MUTED), 0);
}

static void make_sound_row(lv_obj_t *parent)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, KIT_CONTENT, KIT_ROW_H);
    lv_obj_set_style_bg_color(row, lv_color_hex(KIT_COLOR_SURFACE), 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, 24, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(row, 6);
    lv_obj_add_event_cb(row, sound_toggle_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *l = add_label(row, "Som", KIT_COLOR_TEXT, &kit_sans_22, 0);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 24, 0);

    s_sound_val_lbl = add_label(row, "", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 1);
    lv_obj_align(s_sound_val_lbl, LV_ALIGN_RIGHT_MID, -20, 0);
    sync_sound_row();
}

static void sound_toggle_cb(lv_event_t *e)
{
    (void)e;
    bool now_on = !kit_config_get_sound_enabled();
    kit_config_set_sound_enabled(now_on);
    sync_sound_row();
    if (now_on) kit_audio_sfx_impl(KIT_SFX_CLICK);   // confirma que voltou
}

static void open_settings_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_beep_impl(1200, 40);
    if (s_settings_screen) return;

    s_settings_screen = make_overlay(KIT_COLOR_BG);
    make_titlebar(s_settings_screen, "AJUSTES", close_settings_cb);

    lv_obj_t *body = make_scroll_body(s_settings_screen, 0);
    make_row(body, NULL, 0, "Brilho",           false, open_brightness_cb, NULL);
    make_row(body, NULL, 0, "Volume",           false, open_volume_cb,     NULL);
    make_row(body, NULL, 0, "Repouso da tela",  false, open_sleep_cb,      NULL);
    make_row(body, NULL, 0, "Desligar sozinho", false, open_poweroff_cb,   NULL);
    make_sound_row(body);
    make_row(body, NULL, 0, "Testes",           false, run_test_tool_cb,   NULL);
    make_row(body, NULL, 0, "Sobre o KIT",      false, open_about_cb,      NULL);
}

static void close_settings_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_beep_impl(800, 30);
    if (s_settings_screen) {
        lv_obj_delete(s_settings_screen);
        s_settings_screen = NULL;
        s_sound_val_lbl = NULL;
    }
}

// ---------------------------------------------------------------------------
// Brilho
// ---------------------------------------------------------------------------

static void open_brightness_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_beep_impl(1200, 40);
    if (s_brightness_screen) return;

    s_brightness_screen = make_overlay(KIT_COLOR_BG);
    make_titlebar(s_brightness_screen, "BRILHO", close_brightness_cb);

    lv_obj_t *lamp = lv_obj_create(s_brightness_screen);
    lv_obj_set_size(lamp, 112, 112);
    lv_obj_set_style_bg_color(lamp, lv_color_hex(KIT_COLOR_YELLOW), 0);
    lv_obj_set_style_border_width(lamp, 0, 0);
    lv_obj_set_style_radius(lamp, 28, 0);
    lv_obj_set_style_pad_all(lamp, 0, 0);
    lv_obj_clear_flag(lamp, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(lamp, LV_ALIGN_CENTER, 0, -84);

    lv_obj_t *dot = lv_obj_create(lamp);
    lv_obj_set_size(dot, 42, 42);
    lv_obj_set_style_bg_color(dot, lv_color_hex(KIT_COLOR_BG), 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_radius(dot, 21, 0);
    lv_obj_set_style_pad_all(dot, 0, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(dot);

    uint8_t cur = kit_display_get_brightness_impl();

    lv_obj_t *slider = lv_slider_create(s_brightness_screen);
    lv_slider_set_range(slider, 10, 100);
    lv_slider_set_value(slider, cur, LV_ANIM_OFF);
    lv_obj_set_size(slider, KIT_DISPLAY_WIDTH - 64, 22);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, 14);
    lv_obj_set_ext_click_area(slider, 24);
    lv_obj_set_style_bg_color(slider, lv_color_hex(KIT_COLOR_SURFACE), LV_PART_MAIN);
    lv_obj_set_style_radius(slider, 11, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(KIT_COLOR_YELLOW), LV_PART_INDICATOR);
    lv_obj_set_style_radius(slider, 11, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(KIT_COLOR_TEXT), LV_PART_KNOB);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 13, LV_PART_KNOB);
    lv_obj_add_event_cb(slider, brightness_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(slider, brightness_released_cb, LV_EVENT_RELEASED, NULL);

    lv_obj_t *lmin = add_label(s_brightness_screen, "MIN", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);
    lv_obj_align_to(lmin, slider, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 16);
    lv_obj_t *lmax = add_label(s_brightness_screen, "MAX", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);
    lv_obj_align_to(lmax, slider, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 16);

    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", cur);
    s_brightness_val_lbl = add_label(s_brightness_screen, buf, KIT_COLOR_YELLOW, &kit_display_44, 0);
    lv_obj_align(s_brightness_val_lbl, LV_ALIGN_CENTER, 0, 84);

    lv_obj_t *back = make_button(s_brightness_screen, "VOLTAR", close_brightness_cb, false);
    lv_obj_align(back, LV_ALIGN_BOTTOM_MID, 0, -14);
}

static void close_brightness_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_beep_impl(800, 30);
    if (s_brightness_screen) {
        lv_obj_delete(s_brightness_screen);
        s_brightness_screen = NULL;
        s_brightness_val_lbl = NULL;
    }
}

static void brightness_slider_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t val = lv_slider_get_value(slider);
    kit_display_set_brightness_impl((uint8_t)val);   // aplica ao vivo enquanto arrasta
    if (s_brightness_val_lbl) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", (int)val);
        lv_label_set_text(s_brightness_val_lbl, buf);
    }
}

// Só persiste ao soltar o dedo — evita uma gravação em NVS por passo do arraste.
static void brightness_released_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    kit_config_set_brightness((uint8_t)lv_slider_get_value(slider));
}

// ---------------------------------------------------------------------------
// Volume
// ---------------------------------------------------------------------------

static void open_volume_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_beep_impl(1200, 40);
    if (s_volume_screen) return;

    s_volume_screen = make_overlay(KIT_COLOR_BG);
    make_titlebar(s_volume_screen, "VOLUME", close_volume_cb);

    lv_obj_t *spk = lv_obj_create(s_volume_screen);
    lv_obj_set_size(spk, 112, 112);
    lv_obj_set_style_bg_color(spk, lv_color_hex(KIT_COLOR_GREEN), 0);
    lv_obj_set_style_border_width(spk, 0, 0);
    lv_obj_set_style_radius(spk, 28, 0);
    lv_obj_set_style_pad_all(spk, 0, 0);
    lv_obj_clear_flag(spk, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(spk, LV_ALIGN_CENTER, 0, -84);
    lv_obj_t *ico = add_label(spk, KIT_ICON_BARS, KIT_COLOR_BG, &kit_display_44, 0);
    lv_obj_center(ico);

    uint8_t cur = kit_config_get_volume();

    lv_obj_t *slider = lv_slider_create(s_volume_screen);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, cur, LV_ANIM_OFF);
    lv_obj_set_size(slider, KIT_DISPLAY_WIDTH - 64, 22);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, 14);
    lv_obj_set_ext_click_area(slider, 24);
    lv_obj_set_style_bg_color(slider, lv_color_hex(KIT_COLOR_SURFACE), LV_PART_MAIN);
    lv_obj_set_style_radius(slider, 11, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(KIT_COLOR_GREEN), LV_PART_INDICATOR);
    lv_obj_set_style_radius(slider, 11, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(KIT_COLOR_TEXT), LV_PART_KNOB);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 13, LV_PART_KNOB);
    lv_obj_add_event_cb(slider, volume_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(slider, volume_released_cb, LV_EVENT_RELEASED, NULL);

    lv_obj_t *lmin = add_label(s_volume_screen, "MIN", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);
    lv_obj_align_to(lmin, slider, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 16);
    lv_obj_t *lmax = add_label(s_volume_screen, "MAX", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);
    lv_obj_align_to(lmax, slider, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 16);

    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", cur);
    s_volume_val_lbl = add_label(s_volume_screen, buf, KIT_COLOR_GREEN, &kit_display_44, 0);
    lv_obj_align(s_volume_val_lbl, LV_ALIGN_CENTER, 0, 84);

    lv_obj_t *back = make_button(s_volume_screen, "VOLTAR", close_volume_cb, false);
    lv_obj_align(back, LV_ALIGN_BOTTOM_MID, 0, -14);
}

static void close_volume_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_beep_impl(800, 30);
    if (s_volume_screen) {
        lv_obj_delete(s_volume_screen);
        s_volume_screen = NULL;
        s_volume_val_lbl = NULL;
    }
}

static void volume_slider_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t val = lv_slider_get_value(slider);
    kit_audio_set_volume_impl((uint8_t)val);   // aplica ao vivo
    if (s_volume_val_lbl) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", (int)val);
        lv_label_set_text(s_volume_val_lbl, buf);
    }
    // Prévia audível durante o arraste, no máximo ~6x/s pra não engasgar.
    static uint32_t s_last_preview = 0;
    uint32_t now = lv_tick_get();
    if (now - s_last_preview >= 160) {
        s_last_preview = now;
        kit_audio_beep_impl(1320, 45);
    }
}

// Só persiste ao soltar o dedo.
static void volume_released_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    uint8_t v = (uint8_t)lv_slider_get_value(slider);
    kit_config_set_volume(v);
    kit_audio_set_volume_impl(v);
    kit_audio_beep_impl(1320, 90);   // toque final no volume escolhido
}

// ---------------------------------------------------------------------------
// Repouso da tela  ·  Desligar sozinho  (listas de opção estilo rádio)
// ---------------------------------------------------------------------------

typedef struct { const char *label; uint32_t seconds; } timeout_opt_t;

// A tela apaga por inatividade e volta com o botão PWR.
static const timeout_opt_t SLEEP_OPTS[] = {
    { "15 segundos", 15 },  { "30 segundos", 30 }, { "1 minuto", 60 },
    { "2 minutos", 120 },   { "5 minutos", 300 },  { "Nunca", 0 },
};
// O aparelho desliga sozinho — só quando não está carregando.
static const timeout_opt_t POWEROFF_OPTS[] = {
    { "2 minutos", 120 },   { "5 minutos", 300 },  { "10 minutos", 600 },
    { "30 minutos", 1800 }, { "Nunca", 0 },
};
#define SLEEP_OPTS_N    ((int)(sizeof(SLEEP_OPTS) / sizeof(SLEEP_OPTS[0])))
#define POWEROFF_OPTS_N ((int)(sizeof(POWEROFF_OPTS) / sizeof(POWEROFF_OPTS[0])))

static void close_sleep_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_beep_impl(800, 30);
    if (s_sleep_screen) { lv_obj_delete(s_sleep_screen); s_sleep_screen = NULL; }
}

static void close_poweroff_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_beep_impl(800, 30);
    if (s_poweroff_screen) { lv_obj_delete(s_poweroff_screen); s_poweroff_screen = NULL; }
}

static void sleep_pick_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i >= SLEEP_OPTS_N) return;
    kit_config_set_screen_sleep_s(SLEEP_OPTS[i].seconds);
    kit_audio_beep_impl(1200, 40);
    close_sleep_cb(NULL);
}

static void poweroff_pick_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i >= POWEROFF_OPTS_N) return;
    kit_config_set_auto_poweroff_s(POWEROFF_OPTS[i].seconds);
    kit_audio_beep_impl(1200, 40);
    close_poweroff_cb(NULL);
}

static void build_timeout_list(lv_obj_t *screen, const char *hint,
                               const timeout_opt_t *opts, int n,
                               uint32_t current, lv_event_cb_t pick_cb)
{
    lv_obj_t *body = make_scroll_body(screen, 0);

    lv_obj_t *cap = add_label(body, hint, KIT_COLOR_TEXT_MUTED, &kit_mono_16, 1);
    lv_label_set_long_mode(cap, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(cap, KIT_CONTENT);
    lv_obj_set_style_pad_bottom(cap, 6, 0);

    for (int i = 0; i < n; i++) {
        bool sel = (opts[i].seconds == current);
        make_row(body, KIT_ICON_CIRCLE,
                 sel ? KIT_COLOR_GREEN : KIT_COLOR_TEXT_MUTED,
                 opts[i].label, sel, pick_cb, (void *)(intptr_t)i);
    }
}

static void open_sleep_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_beep_impl(1200, 40);
    if (s_sleep_screen) return;
    s_sleep_screen = make_overlay(KIT_COLOR_BG);
    make_titlebar(s_sleep_screen, "REPOUSO", close_sleep_cb);
    build_timeout_list(s_sleep_screen,
                       "Tempo sem toque at\xC3\xA9 a tela apagar. O bot\xC3\xA3o PWR volta a ligar.",
                       SLEEP_OPTS, SLEEP_OPTS_N,
                       kit_config_get_screen_sleep_s(), sleep_pick_cb);
}

static void open_poweroff_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_beep_impl(1200, 40);
    if (s_poweroff_screen) return;
    s_poweroff_screen = make_overlay(KIT_COLOR_BG);
    make_titlebar(s_poweroff_screen, "DESLIGAR", close_poweroff_cb);
    build_timeout_list(s_poweroff_screen,
                       "Tempo sem uso at\xC3\xA9 o KIT desligar sozinho. N\xC3\xA3o vale na tomada.",
                       POWEROFF_OPTS, POWEROFF_OPTS_N,
                       kit_config_get_auto_poweroff_s(), poweroff_pick_cb);
}

// ---------------------------------------------------------------------------
// Sobre
// ---------------------------------------------------------------------------

static void open_about_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_beep_impl(1200, 40);
    if (s_about_screen) return;

    s_about_screen = make_overlay(KIT_COLOR_BG);
    make_titlebar(s_about_screen, "SOBRE", close_about_cb);

    lv_obj_t *body = make_scroll_body(s_about_screen, KIT_BTN_H + 28);

    lv_obj_t *logo = make_group(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(logo, 12, 0);
    lv_obj_set_style_pad_top(logo, 6, 0);
    lv_obj_set_style_pad_bottom(logo, 10, 0);
    make_logo_trio(logo);
    add_label(logo, "KIT", KIT_COLOR_TEXT, &kit_display_44, 3);

    make_spec(body, "DISPOSITIVO", kit_power_get_device_id());
    make_spec(body, "RUNTIME", "v0.1.0");
    make_spec(body, "HARDWARE", "ESP32-S3 V2");
    make_spec(body, "FLASH", "16 MB \xC2\xB7 7 MB LFS");
    make_spec(body, "LICEN\xC3\x87""A", "GPL-3.0");

    lv_obj_t *sig = add_label(body, "JCRVLH EXPERIMENT", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 4);
    lv_obj_set_style_pad_top(sig, 14, 0);

    lv_obj_t *back = make_button(s_about_screen, "VOLTAR", close_about_cb, false);
    lv_obj_align(back, LV_ALIGN_BOTTOM_MID, 0, -14);
}

static void close_about_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_beep_impl(800, 30);
    if (s_about_screen) {
        lv_obj_delete(s_about_screen);
        s_about_screen = NULL;
    }
}

// ---------------------------------------------------------------------------
// Ações
// ---------------------------------------------------------------------------

static void home_tile_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i >= HOME_TOOLS_N) return;
    const home_tool_t *tool = &HOME_TOOLS[i];

    if (!tool->available) {
        kit_audio_beep_impl(500, 30);
        show_toast("EM BREVE");
        return;
    }

    kit_audio_sfx_impl(KIT_SFX_CLICK);
    ESP_LOGI(TAG, "Abrindo Tool '%s'...", tool->id);
    if (s_toast) { lv_obj_delete(s_toast); s_toast = NULL; }
    home_mru_touch(i);   // sobe pro topo da recência (deck reconstrói ao voltar)
    kit_tool_manager_start(tool->id);
}

static void run_test_tool_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_beep_impl(2000, 50);
    if (s_settings_screen) {
        lv_obj_delete(s_settings_screen);
        s_settings_screen = NULL;
    }
    ESP_LOGI(TAG, "Iniciando Test Tool...");
    kit_tool_manager_start("com.kit.test");
}

// ---------------------------------------------------------------------------
// Navegação e polling da bateria
// ---------------------------------------------------------------------------

void kit_launcher_go_home(void)
{
    if (s_feedback_screen)   { lv_obj_delete(s_feedback_screen);   s_feedback_screen = NULL; }
    if (s_about_screen)      { lv_obj_delete(s_about_screen);      s_about_screen = NULL; }
    if (s_sleep_screen)      { lv_obj_delete(s_sleep_screen);      s_sleep_screen = NULL; }
    if (s_poweroff_screen)   { lv_obj_delete(s_poweroff_screen);   s_poweroff_screen = NULL; }
    if (s_brightness_screen) { lv_obj_delete(s_brightness_screen); s_brightness_screen = NULL;
                               s_brightness_val_lbl = NULL; }
    if (s_volume_screen)     { lv_obj_delete(s_volume_screen);     s_volume_screen = NULL;
                               s_volume_val_lbl = NULL; }
    if (s_settings_screen)   { lv_obj_delete(s_settings_screen);   s_settings_screen = NULL;
                               s_sound_val_lbl = NULL; }
    if (s_splash_screen)     { lv_obj_delete(s_splash_screen);     s_splash_screen = NULL; }
    if (s_toast)             { lv_obj_delete(s_toast);             s_toast = NULL; }

    // Usou uma Tool? A ordem de recência mudou — reconstrói o slideshow.
    // Senão, só volta o slideshow para o primeiro slide.
    if (s_home_deck_dirty) {
        s_home_deck_dirty = false;
        home_clear_deck();
        home_build_deck();
    } else if (s_home_deck && s_home_slides > 0) {
        lv_tileview_set_tile_by_index(s_home_deck, 0, 0, LV_ANIM_OFF);
        home_sync_dots();
    }

    if (s_launcher_screen)   { lv_screen_load(s_launcher_screen); }
}

static void batt_tick_cb(lv_timer_t *t)
{
    (void)t;
    update_battery();

    bool charging = kit_power_is_charging();
    if (charging && !s_was_charging) {
        kit_audio_beep_impl(1600, 60);
        show_feedback(KIT_COLOR_GREEN, KIT_ICON_BOLT, "CARREGANDO");
    }
    s_was_charging = charging;
}

// ---------------------------------------------------------------------------

kit_err_t kit_launcher_init(void)
{
    ESP_LOGI(TAG, "Montando UI do Launcher (Brutalist Bauhaus) em LVGL v9...");

    s_launcher_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_launcher_screen, lv_color_hex(KIT_COLOR_BG), 0);
    lv_obj_clear_flag(s_launcher_screen, LV_OBJ_FLAG_SCROLLABLE);

    build_home(s_launcher_screen);
    update_battery();
    build_splash();

    kit_launcher_show();

    lv_timer_t *splash_t = lv_timer_create(splash_done_cb, 1400, NULL);
    lv_timer_set_repeat_count(splash_t, 1);
    lv_timer_create(batt_tick_cb, 2000, NULL);

    return KIT_OK;
}

void kit_launcher_show(void)
{
    if (s_launcher_screen) {
        lv_screen_load(s_launcher_screen);
    }
}
