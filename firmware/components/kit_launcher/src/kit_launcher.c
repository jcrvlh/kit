#include "kit_launcher.h"
#include "kit_tool_manager.h"
#include "kit_power.h"
#include "kit_display.h"
#include "kit_audio.h"
#include "kit_config.h"
#include "kit_fonts.h"
#include "kit_theme.h"
#include "kit_storage.h"
#include "kit_usb_msc.h"
#include "kit_network.h"
#include "kit_catalog.h"
#include "esp_log.h"
#include "lvgl.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Launcher do KIT — linguagem "Brutalist Bauhaus" (ver docs/design/design-language.md).
// Telas: splash "INICIANDO", Home (sem tools), Ajustes (Tela > brilho/repouso,
// Som > volume/liga-desliga, Armazenamento, Modo pen drive), Sobre, e um
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
static lv_obj_t *s_display_screen = NULL;        // Ajustes > Tela (brilho + repouso)
static lv_obj_t *s_brightness_screen = NULL;
static lv_obj_t *s_volume_screen = NULL;         // Ajustes > Som (volume + liga/desliga)
static lv_obj_t *s_sleep_screen = NULL;
static lv_obj_t *s_battery_screen = NULL;        // Ajustes > Bateria (nível + desligar sozinho)
static lv_obj_t *s_poweroff_screen = NULL;
static lv_obj_t *s_about_screen = NULL;
static lv_obj_t *s_storage_screen = NULL;       // Ajustes > Armazenamento
static lv_obj_t *s_sd_format_screen = NULL;     // confirmação de formatar o cartão
static lv_obj_t *s_usbmsc_screen = NULL;        // Ajustes > Modo pen drive (USB MSC)
static lv_obj_t *s_wifi_screen = NULL;          // Ajustes > Wi-Fi
static lv_obj_t *s_wifi_portal_screen = NULL;   // Wi-Fi > Configurar rede (portal)
static lv_obj_t *s_catalog_screen = NULL;       // Catálogo de Tools
static lv_obj_t *s_catalog_detail_screen = NULL;
static lv_obj_t *s_catalog_busy_screen = NULL;
static lv_obj_t *s_catalog_confirm_screen = NULL;
static lv_obj_t *s_onboarding_screen = NULL;   // introdução do primeiro boot (repetível)
static lv_obj_t *s_home_deck = NULL;       // lv_tileview horizontal — slideshow de Tools
static lv_obj_t *s_home_dots_box = NULL;   // fileira de pontos de página (rodapé da Home)
static lv_obj_t *s_feedback_screen = NULL;

extern const lv_image_dsc_t kit_icon_triangle_a8;

static lv_obj_t *s_brightness_val_lbl = NULL;
static lv_obj_t *s_volume_val_lbl = NULL;
static lv_obj_t *s_sound_val_lbl = NULL;
static lv_obj_t *s_batt_lbl = NULL;
static lv_obj_t *s_batt_fill = NULL;
static lv_obj_t *s_wifi_icon = NULL;
static lv_obj_t *s_toast = NULL;

static bool s_was_charging = false;
static bool s_low_batt_warned = false;   // aviso de "bateria baixa" já mostrado nesta descarga

// Grade de Tools da Home. Cada Tool built-in só entra aqui quando já está
// implementada (o campo `available` cobre o caso de uma Tool em
// desenvolvimento, que aparece esmaecida e responde com "EM BREVE"). Tools
// externas (cartão microSD, Marco 2) entram em runtime — ver build_home_tools().
typedef enum {
    TOOL_ICON_DICE, TOOL_ICON_SPIN, TOOL_ICON_COIN, TOOL_ICON_TRIANGLE,
    TOOL_ICON_BINGO, TOOL_ICON_ORDER, TOOL_ICON_TIMER, TOOL_ICON_FIRST,
    TOOL_ICON_TEAMS, TOOL_ICON_ASK, TOOL_ICON_PAVIO, TOOL_ICON_ADEDONHA,
    TOOL_ICON_PLACAR, TOOL_ICON_VETO, TOOL_ICON_MIMICA, TOOL_ICON_TESTA,
    TOOL_ICON_EXTERNAL
} tool_icon_t;

typedef struct {
    char        id[40];
    char        label[32];   // >= sizeof(kit_tool_entry_t.name), evita truncar
    uint32_t    color;
    tool_icon_t icon;
    bool        available;
    bool        is_game;     // true = mini-jogo; false = ferramenta ("tool")
} home_tool_t;

static const home_tool_t HOME_TOOLS_BUILTIN[] = {
    { "com.kit.dice",    "Dados",   KIT_COLOR_RED,    TOOL_ICON_DICE, true, false },
    { "com.kit.bottle",  "Garrafa", KIT_COLOR_BLUE,   TOOL_ICON_SPIN, true, false },
    { "com.kit.coin",    "Moeda",   KIT_COLOR_YELLOW, TOOL_ICON_COIN, true, false },
    { "com.kit.timer",   "Timer",   KIT_COLOR_GREEN,  TOOL_ICON_TIMER, true, false },
    { "com.kit.primeiro","Primeiro",KIT_COLOR_RED, TOOL_ICON_FIRST, true, false },
    { "com.kit.times",   "Times",   KIT_COLOR_BLUE,   TOOL_ICON_TEAMS, true, false },
    { "com.kit.bingo",   "Bingo",   KIT_COLOR_GREEN,  TOOL_ICON_BINGO, true, true },
    // Quebra-Gelo, Pavio, Adedonha, Veto, Mímica e Testa saíram do Core — vivem no
    // catálogo (io.github.jcrvlh.*). TOOL_ICON_ASK / PAVIO / ADEDONHA / VETO /
    // MIMICA / TESTA e seus mapas em icon_from_name ficam pra Tool do cartão reusar
    // via "home_icon" no manifest.
    { "com.kit.placar", "Placar", KIT_COLOR_GREEN, TOOL_ICON_PLACAR, true, false },
};
#define HOME_TOOLS_BUILTIN_N ((int)(sizeof(HOME_TOOLS_BUILTIN) / sizeof(HOME_TOOLS_BUILTIN[0])))

// Grade efetiva da Home: built-ins + o catálogo dinâmico do cartão SD
// (kit_tool_manager_get_count/get_entry), montada uma vez em build_home_tools().
#define KIT_HOME_TOOLS_MAX (HOME_TOOLS_BUILTIN_N + 16)
static home_tool_t s_home_tools[KIT_HOME_TOOLS_MAX];
static int         s_home_tools_n = 0;

// -- Slideshow da Home ----------------------------------------------------
// A Home é um lv_tileview horizontal: os HOME_MRU_SLOTS primeiros slides são as
// Tools usadas mais recentemente (a mais recente primeiro) e o último slide é
// "VER TODOS" — a grade completa. A ordem de recência é persistida em NVS
// (kit_config, chaves "home_mru0".."home_mru2", valor = índice em s_home_tools + 1).
#define HOME_MRU_SLOTS 3
#define HOME_STATUS_H  72
#define HOME_DOTS_H    40
#define HOME_DECK_H    (KIT_DISPLAY_HEIGHT - HOME_STATUS_H - HOME_DOTS_H)

static int      s_mru[KIT_HOME_TOOLS_MAX];           // índices em s_home_tools, recente primeiro
static int      s_mru_n = 0;
static lv_obj_t *s_home_tiles[HOME_MRU_SLOTS + 1];
static lv_obj_t *s_home_dots[HOME_MRU_SLOTS + 1];
static int      s_home_slides = 0;
static bool     s_home_deck_dirty = false;           // pediu rebuild ao voltar pra Home

// Nome do ícone no manifest ("home_icon") -> glifo geométrico da Home. Uma Tool
// do cartão pode reusar um dos desenhos das Tools oficiais; sem isso (ou nome
// desconhecido) cai no cartão genérico.
static tool_icon_t icon_from_name(const char *name)
{
    if (!name || !name[0]) return TOOL_ICON_EXTERNAL;
    static const struct { const char *n; tool_icon_t i; } kMap[] = {
        { "dice", TOOL_ICON_DICE },     { "spin", TOOL_ICON_SPIN },
        { "coin", TOOL_ICON_COIN },     { "triangle", TOOL_ICON_TRIANGLE },
        { "bingo", TOOL_ICON_BINGO },   { "order", TOOL_ICON_ORDER },
        { "timer", TOOL_ICON_TIMER },   { "first", TOOL_ICON_FIRST },
        { "teams", TOOL_ICON_TEAMS },   { "ask", TOOL_ICON_ASK },
        { "pavio", TOOL_ICON_PAVIO },   { "adedonha", TOOL_ICON_ADEDONHA },
        { "placar", TOOL_ICON_PLACAR }, { "veto", TOOL_ICON_VETO },
        { "mimica", TOOL_ICON_MIMICA }, { "testa", TOOL_ICON_TESTA },
        { "card", TOOL_ICON_EXTERNAL },
    };
    for (size_t i = 0; i < sizeof(kMap) / sizeof(kMap[0]); i++)
        if (strcmp(name, kMap[i].n) == 0) return kMap[i].i;
    return TOOL_ICON_EXTERNAL;
}

// Monta a grade efetiva da Home: as Tools built-in (compiladas no firmware)
// seguidas do catálogo dinâmico que o Tool Manager varreu em /sdcard/tools.
// A Tool do cartão pode declarar `accent` (cor do card) e `home_icon` no
// manifest; sem isso, gira pela paleta e usa o ícone genérico de cartão.
static void build_home_tools(void)
{
    s_home_tools_n = 0;
    for (int i = 0; i < HOME_TOOLS_BUILTIN_N && s_home_tools_n < KIT_HOME_TOOLS_MAX; i++) {
        s_home_tools[s_home_tools_n++] = HOME_TOOLS_BUILTIN[i];
    }

    static const uint32_t kExtPalette[] = {
        KIT_COLOR_RED, KIT_COLOR_BLUE, KIT_COLOR_YELLOW, KIT_COLOR_GREEN,
    };
    uint32_t ext_n = kit_tool_manager_get_count();
    for (uint32_t i = 0; i < ext_n && s_home_tools_n < KIT_HOME_TOOLS_MAX; i++) {
        kit_tool_entry_t e;
        if (kit_tool_manager_get_entry(i, &e) != KIT_OK) continue;

        home_tool_t *t = &s_home_tools[s_home_tools_n++];
        snprintf(t->id, sizeof(t->id), "%s", e.id);
        snprintf(t->label, sizeof(t->label), "%s", e.name);
        t->color     = e.accent ? e.accent
                     : kExtPalette[i % (sizeof(kExtPalette) / sizeof(kExtPalette[0]))];
        t->icon      = icon_from_name(e.icon);
        t->available = true;
        t->is_game   = e.is_game;
    }

    ESP_LOGI(TAG, "Home: %d Tool(s) built-in + %d do cartão SD.",
             HOME_TOOLS_BUILTIN_N, s_home_tools_n - HOME_TOOLS_BUILTIN_N);
}

// Reconstrói a lista de recência a partir do NVS; Tools sem histórico entram no
// fim, na ordem de s_home_tools.
static void home_mru_load(void)
{
    bool seen[KIT_HOME_TOOLS_MAX] = { 0 };
    s_mru_n = 0;
    for (int slot = 0; slot < HOME_MRU_SLOTS; slot++) {
        char key[16];
        snprintf(key, sizeof(key), "home_mru%d", slot);
        uint8_t v = 0;
        kit_config_get_u8(key, &v, 0);
        int idx = (int)v - 1;
        if (idx >= 0 && idx < s_home_tools_n && !seen[idx]) {
            seen[idx] = true;
            s_mru[s_mru_n++] = idx;
        }
    }
    for (int i = 0; i < s_home_tools_n; i++)
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
static void open_display_cb(lv_event_t *e);
static void close_display_cb(lv_event_t *e);
static void open_brightness_cb(lv_event_t *e);
static void close_brightness_cb(lv_event_t *e);
static void open_sound_cb(lv_event_t *e);
static void close_sound_cb(lv_event_t *e);
static void open_sleep_cb(lv_event_t *e);
static void open_battery_cb(lv_event_t *e);
static void close_battery_cb(lv_event_t *e);
static void open_poweroff_cb(lv_event_t *e);
static void open_about_cb(lv_event_t *e);
static void close_about_cb(lv_event_t *e);
static void open_storage_cb(lv_event_t *e);
static void close_storage_cb(lv_event_t *e);
static void sd_scan_cb(lv_event_t *e);
static void open_sd_format_cb(lv_event_t *e);
static void close_sd_format_cb(lv_event_t *e);
static void do_sd_format_cb(lv_event_t *e);
static void open_usbmsc_cb(lv_event_t *e);
static void close_usbmsc_cb(lv_event_t *e);
static void open_wifi_cb(lv_event_t *e);
static void close_wifi_cb(lv_event_t *e);
static void open_catalog_cb(lv_event_t *e);
static void close_catalog_cb(lv_event_t *e);
static void wifi_toggle_cb(lv_event_t *e);
static void wifi_forget_cb(lv_event_t *e);
static void wifi_portal_open_cb(lv_event_t *e);
static void wifi_portal_close_cb(lv_event_t *e);
static void usbmsc_activate_cb(lv_event_t *e);
static void usbmsc_exit_cb(lv_event_t *e);
static void usbmsc_do_exit_cb(lv_event_t *e);
static void usbmsc_confirm_back_cb(lv_event_t *e);
static void brightness_slider_cb(lv_event_t *e);
static void brightness_released_cb(lv_event_t *e);
static void volume_slider_cb(lv_event_t *e);
static void volume_released_cb(lv_event_t *e);
static void sound_toggle_cb(lv_event_t *e);
static void run_test_tool_cb(lv_event_t *e);
static void home_tile_cb(lv_event_t *e);
static void onboarding_show(int step);
static void onboarding_start_if_needed(void);
static void repeat_onboarding_cb(lv_event_t *e);

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

static lv_obj_t *make_row(lv_obj_t *parent, const char *shape, uint32_t shape_color,
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
    return row;
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

    // Ícone de sinal Wi-Fi (0xF012) — só aparece com o rádio ligado.
    s_wifi_icon = add_label(g, KIT_ICON_BARS, KIT_COLOR_TEXT_MUTED, &kit_mono_16, 0);
    lv_obj_add_flag(s_wifi_icon, LV_OBJ_FLAG_HIDDEN);

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

// Ícone de Wi-Fi da barra de status: escondido com o rádio desligado; âmbar
// enquanto procura/associa; verde conectado; apagado ligado-sem-rede.
static void update_wifi_icon(void)
{
    if (!s_wifi_icon) return;
    uint32_t c = KIT_COLOR_TEXT_MUTED;
    bool show = true;
    switch (kit_network_get_state()) {
    case KIT_NET_CONNECTED:    c = KIT_COLOR_GREEN;  break;
    case KIT_NET_CONNECTING:   c = KIT_COLOR_YELLOW; break;
    case KIT_NET_PROVISIONING: c = KIT_COLOR_BLUE;   break;
    case KIT_NET_DISCONNECTED: c = KIT_COLOR_TEXT_MUTED; break;
    default:                   show = false; break;   // KIT_NET_OFF
    }
    if (show) {
        lv_obj_set_style_text_color(s_wifi_icon, lv_color_hex(c), 0);
        lv_obj_clear_flag(s_wifi_icon, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_wifi_icon, LV_OBJ_FLAG_HIDDEN);
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
    onboarding_start_if_needed();   // primeiro boot: entra a introdução
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
    case TOOL_ICON_PAVIO: {
        // bomba: corpo redondo + pavio curto no topo com uma faísca
        lv_obj_t *body = icon_shape(ic, 18, 18, color, LV_RADIUS_CIRCLE, 0);
        lv_obj_align(body, LV_ALIGN_CENTER, 0, 3);
        lv_obj_align(icon_shape(ic, 3, 6, color, 1, 0), LV_ALIGN_CENTER, 3, -7);
        lv_obj_align(icon_shape(ic, 5, 5, color, 2, 0), LV_ALIGN_CENTER, 6, -10);
        break;
    }
    case TOOL_ICON_ADEDONHA: {
        // folha de cartela: moldura + três linhas (as colunas a preencher)
        lv_obj_t *sheet = icon_shape(ic, 20, 24, color, 4, 3);
        lv_obj_center(sheet);
        lv_obj_align(icon_shape(ic, 11, 3, color, 1, 0), LV_ALIGN_CENTER, 0, -6);
        lv_obj_align(icon_shape(ic, 11, 3, color, 1, 0), LV_ALIGN_CENTER, 0,  0);
        lv_obj_align(icon_shape(ic, 11, 3, color, 1, 0), LV_ALIGN_CENTER, 0,  6);
        break;
    }
    case TOOL_ICON_PLACAR: {
        // três colunas de placar em alturas diferentes (pódio / barras)
        lv_obj_align(icon_shape(ic, 6, 12, color, 1, 0), LV_ALIGN_BOTTOM_LEFT,  1, 0);
        lv_obj_align(icon_shape(ic, 6, 22, color, 1, 0), LV_ALIGN_BOTTOM_MID,   0, 0);
        lv_obj_align(icon_shape(ic, 6, 16, color, 1, 0), LV_ALIGN_BOTTOM_RIGHT, -1, 0);
        break;
    }
    case TOOL_ICON_VETO: {
        // carta: a palavra-alvo (barra grossa no topo) e as proibidas abaixo,
        // cada uma com um ponto (o marcador quadrado da lista)
        lv_obj_t *card = icon_shape(ic, 22, 24, color, 4, 3);
        lv_obj_center(card);
        lv_obj_align(icon_shape(ic, 13, 5, color, 1, 0), LV_ALIGN_CENTER, 0, -6);
        lv_obj_align(icon_shape(ic, 3, 3, color, 1, 0), LV_ALIGN_CENTER, -5, 2);
        lv_obj_align(icon_shape(ic, 8, 2, color, 1, 0), LV_ALIGN_CENTER,  2, 2);
        lv_obj_align(icon_shape(ic, 3, 3, color, 1, 0), LV_ALIGN_CENTER, -5, 8);
        lv_obj_align(icon_shape(ic, 8, 2, color, 1, 0), LV_ALIGN_CENTER,  2, 8);
        break;
    }
    case TOOL_ICON_MIMICA: {
        // figura gesticulando: cabeça + tronco + braços erguidos
        lv_obj_align(icon_shape(ic, 9, 9, color, LV_RADIUS_CIRCLE, 0), LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_align(icon_shape(ic, 4, 12, color, 2, 0), LV_ALIGN_CENTER, 0, 4);
        lv_obj_align(icon_shape(ic, 11, 3, color, 1, 0), LV_ALIGN_CENTER, -6, 0);
        lv_obj_align(icon_shape(ic, 11, 3, color, 1, 0), LV_ALIGN_CENTER,  6, 0);
        break;
    }
    case TOOL_ICON_TESTA: {
        // cabeça + aparelho encostado na testa + setas de inclinar (↑/↓)
        lv_obj_align(icon_shape(ic, 13, 13, color, LV_RADIUS_CIRCLE, 0), LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_align(icon_shape(ic, 10, 18, color, 2, 2), LV_ALIGN_CENTER, 4, 0);
        lv_obj_align(icon_shape(ic, 8, 3, color, 1, 0), LV_ALIGN_CENTER, 12, -7);
        lv_obj_align(icon_shape(ic, 8, 3, color, 1, 0), LV_ALIGN_CENTER, 12,  7);
        break;
    }
    case TOOL_ICON_EXTERNAL: {
        // cartão (Tool vinda do microSD): moldura com 3 pinos — sem glifo
        // dedicado por Tool ainda (o manifest não traz ícone próprio).
        lv_obj_t *card = icon_shape(ic, 20, 20, color, 4, 3);
        lv_obj_center(card);
        lv_obj_align(icon_shape(ic, 3, 8, color, 1, 0), LV_ALIGN_CENTER, -5, 0);
        lv_obj_align(icon_shape(ic, 3, 8, color, 1, 0), LV_ALIGN_CENTER,  0, 0);
        lv_obj_align(icon_shape(ic, 3, 8, color, 1, 0), LV_ALIGN_CENTER,  5, 0);
        break;
    }
    }
}

static void make_tool_tile(lv_obj_t *grid, int index)
{
    const home_tool_t *tool = &s_home_tools[index];
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

// -- Card de Catálogo: ao lado de Ajustes na seção SISTEMA da grade. --
static void make_catalog_tile(lv_obj_t *grid)
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
    lv_obj_add_event_cb(tile, open_catalog_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *badge = lv_obj_create(tile);
    lv_obj_set_size(badge, 42, 42);
    lv_obj_set_style_bg_color(badge, lv_color_hex(KIT_COLOR_TEXT_MUTED), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_20, 0);
    lv_obj_set_style_border_width(badge, 0, 0);
    lv_obj_set_style_radius(badge, 12, 0);
    lv_obj_set_style_pad_all(badge, 0, 0);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(badge, LV_ALIGN_TOP_LEFT, 14, 14);
    // kit_mono_26 (não kit_display_44): o "+" no tamanho grande passava bem dos
    // ícones desenhados (~24 px) das outras Tools e do anel do Ajustes.
    lv_obj_t *g = add_label(badge, KIT_ICON_PLUS, KIT_COLOR_TEXT_MUTED, &kit_mono_26, 0);
    lv_obj_center(g);

    lv_obj_t *lbl = add_label(tile, "Cat\xC3\xA1logo", KIT_COLOR_TEXT_MUTED, &kit_sans_22, 0);
    lv_obj_align(lbl, LV_ALIGN_BOTTOM_LEFT, 14, -14);
}

// -- Slide de destaque: uma Tool por tela, ocupando tudo na cor dela. `slot` é
//    a posição no slideshow (marca-d'água "01".."04"). --
static void make_tool_slide(lv_obj_t *tile, int index, int slot)
{
    const home_tool_t *tool = &s_home_tools[index];
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

// Cabeçalho de seção dentro da grade (ocupa a linha inteira do flex-wrap).
static void make_grid_header(lv_obj_t *grid, const char *txt, bool first)
{
    lv_obj_t *h = add_label(grid, txt, KIT_COLOR_TEXT_MUTED, &kit_mono_16, 3);
    lv_obj_set_width(h, lv_pct(100));
    lv_obj_set_style_pad_top(h, first ? 0 : 10, 0);
    lv_obj_set_style_pad_bottom(h, 2, 0);
}

// -- Último slide: "VER TODOS" — a grade completa, rola na vertical.
//    Ferramentas em cima, mini-jogos embaixo (separação visual). --
static void make_all_slide(lv_obj_t *tile)
{
    lv_obj_t *hdr = add_label(tile, "TUDO", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 3);
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

    int n_games = 0;
    for (int i = 0; i < s_home_tools_n; i++)
        if (s_home_tools[i].is_game) n_games++;

    // Ferramentas em cima, mini-jogos no meio, sistema (Ajustes) embaixo —
    // cada bloco com seu cabeçalho de seção.
    make_grid_header(grid, "FERRAMENTAS", true);
    for (int i = 0; i < s_home_tools_n; i++)
        if (!s_home_tools[i].is_game) make_tool_tile(grid, i);

    if (n_games > 0) {
        make_grid_header(grid, "MINI-JOGOS", false);
        for (int i = 0; i < s_home_tools_n; i++)
            if (s_home_tools[i].is_game) make_tool_tile(grid, i);
    }

    make_grid_header(grid, "SISTEMA", false);
    make_settings_tile(grid);
    make_catalog_tile(grid);
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

static void home_build_deck(void);

// Reconstrução do deck adiada (via lv_async_call). Ao sair de uma Tool,
// kit_system_exit_impl chama kit_launcher_go_home() e SÓ DEPOIS
// kit_tool_manager_stop_current() (que libera a árvore LVGL da Tool). Se o deck
// fosse remontado dentro do go_home, a Tool ainda viva + a Home nova não cabem
// no pool de 64 KB do LVGL: lv_malloc devolve NULL, o LV_ASSERT dispara e cai
// num while(1) (a placa "trava" no voltar). Adiar um tick deixa a Tool ser
// liberada primeiro.
static void home_build_deck_async_cb(void *unused)
{
    (void)unused;
    if (!s_launcher_screen || s_home_deck) return;   // já saiu de novo, ou já montado
    home_build_deck();
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

    build_home_tools();   // built-ins + catálogo do cartão SD (kit_tool_manager)
    home_mru_load();
    home_build_deck();
}

// Há alguma tela (overlay) por cima da Home agora?
static bool home_is_covered(void)
{
    return s_settings_screen || s_display_screen || s_brightness_screen || s_volume_screen ||
           s_sleep_screen || s_battery_screen || s_poweroff_screen || s_about_screen ||
           s_storage_screen || s_sd_format_screen || s_usbmsc_screen ||
           s_wifi_screen || s_wifi_portal_screen || s_catalog_screen ||
           s_catalog_detail_screen || s_catalog_busy_screen ||
           s_catalog_confirm_screen || s_onboarding_screen || s_feedback_screen;
}

// Recallback do Tool Manager: o catálogo do cartão mudou (instalou/removeu Tool,
// formatou, montou um cartão novo). Roda sempre na task LVGL (via lv_async_call).
static void launcher_catalog_changed_impl(void *unused)
{
    (void)unused;
    ESP_LOGI(TAG, "Cat\xC3\xA1logo de Tools mudou.");
    build_home_tools();   // só dados — sem objetos LVGL
    home_mru_load();

    // O deck da Home (tileview + dots) é filho do s_launcher_screen; recriá-lo
    // agora joga objetos novos POR CIMA de qualquer overlay aberto (o usuário
    // instalou pelo Catálogo => a tela "TOOL"/"CATÁLOGO" está na frente). Adia:
    // o kit_launcher_go_home() reconstrói o deck quando o usuário voltar.
    if (home_is_covered()) {
        s_home_deck_dirty = true;
        return;
    }
    home_clear_deck();
    home_build_deck();
    s_home_deck_dirty = false;
}

// O kit_tool_manager pode chamar isso de qualquer task (kit_catalog na
// instalação pelo catálogo, kit_comms no upload serial) — o refaz da Home
// mexe em dezenas de objetos LVGL, então empurra pra thread do LVGL.
static void launcher_catalog_changed(void)
{
    lv_async_call(launcher_catalog_changed_impl, NULL);
}

// ---------------------------------------------------------------------------
// Introdução (onboarding)
// ---------------------------------------------------------------------------
// Quatro telas no primeiro boot: o KIT se apresenta, diz pra que serve, mostra
// o que tem dentro e devolve o usuário na Home. A flag "onboarded" (NVS) marca
// que já rodou; os Ajustes têm um botão pra repetir.

#define ONB_STEPS 4

static int s_onb_step = 0;

static void onboarding_next_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_sfx_impl(KIT_SFX_CLICK);   // toque leve pra avançar
    onboarding_show(s_onb_step + 1);
}

static void onboarding_finish_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_sfx_impl(KIT_SFX_ONBOARD_DONE);   // fanfarra feliz de boas-vindas
    kit_config_set_u8("onboarded", 1);
    if (s_onboarding_screen) { lv_obj_delete(s_onboarding_screen); s_onboarding_screen = NULL; }
    kit_launcher_go_home();
}

// Frase grande, branca e centralizada. Archivo Bold é a única fonte proporcional
// do sistema — o "grande e negrito" possível sem quebrar a regra de que as
// fontes display são só números. `font` escolhe o tamanho (kit_sans_28 na tela
// de abertura, kit_sans_22 nas mais longas).
static lv_obj_t *onb_sentence(lv_obj_t *parent, const char *txt, const lv_font_t *font)
{
    lv_obj_t *l = add_label(parent, txt, KIT_COLOR_TEXT, font, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, KIT_CONTENT);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    return l;
}

// Fileira dos selos das Tools (tela 3), na cor de cada uma.
static void onb_tool_reel(lv_obj_t *parent)
{
    lv_obj_t *reel = lv_obj_create(parent);
    lv_obj_remove_style_all(reel);
    lv_obj_set_size(reel, KIT_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(reel, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(reel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(reel, 10, 0);
    lv_obj_set_style_pad_column(reel, 10, 0);
    lv_obj_clear_flag(reel, LV_OBJ_FLAG_SCROLLABLE);

    // Sempre as Tools built-in — a introdução roda no primeiro boot, antes de
    // qualquer varredura de cartão fazer sentido para o usuário.
    for (int i = 0; i < HOME_TOOLS_BUILTIN_N; i++) {
        lv_obj_t *badge = lv_obj_create(reel);
        lv_obj_set_size(badge, 46, 46);
        lv_obj_set_style_bg_color(badge, lv_color_hex(KIT_COLOR_SURFACE_ALT), 0);
        lv_obj_set_style_border_width(badge, 0, 0);
        lv_obj_set_style_radius(badge, 14, 0);
        lv_obj_set_style_pad_all(badge, 0, 0);
        lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
        make_tool_icon(badge, HOME_TOOLS_BUILTIN[i].icon, HOME_TOOLS_BUILTIN[i].color);
    }
}

static void onboarding_show(int step)
{
    if (step < 0) step = 0;
    if (step >= ONB_STEPS) { onboarding_finish_cb(NULL); return; }

    if (s_onboarding_screen) { lv_obj_delete(s_onboarding_screen); s_onboarding_screen = NULL; }
    s_onb_step = step;
    s_onboarding_screen = make_overlay(KIT_COLOR_BG);

    lv_obj_t *col = make_group(s_onboarding_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, 18, 0);
    lv_obj_set_width(col, KIT_CONTENT);
    lv_obj_align(col, LV_ALIGN_CENTER, 0, -28);

    const char *btn_txt = "CONTINUAR";
    bool finish = false;

    switch (step) {
    case 0:
        kit_audio_sfx_impl(KIT_SFX_WELCOME);   // aceno de boas-vindas, só aqui
        make_logo_trio(col);
        add_label(col, "OL\xC3\x81, EU SOU O", KIT_COLOR_TEXT_MUTED, &kit_mono_20, 3);
        add_label(col, "KIT", KIT_COLOR_TEXT, &kit_display_72, 0);
        btn_txt = "COME\xC3\x87""AR";
        break;
    case 1:
        onb_sentence(col, "Sou um kit de ferramentas para a sua game night.", &kit_sans_28);
        btn_txt = "VEM VER";
        break;
    case 2:
        onb_sentence(col, "Aqui tem dados, garrafa, moeda, timer, quebra-gelo e muito mais.", &kit_sans_22);
        onb_tool_reel(col);
        btn_txt = "CONTINUAR";
        break;
    case 3: {
        lv_obj_t *badge = lv_obj_create(col);
        lv_obj_set_size(badge, 96, 96);
        lv_obj_set_style_bg_color(badge, lv_color_hex(KIT_COLOR_SURFACE_ALT), 0);
        lv_obj_set_style_border_width(badge, 0, 0);
        lv_obj_set_style_radius(badge, 28, 0);
        lv_obj_set_style_pad_all(badge, 0, 0);
        lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *chk = add_label(badge, KIT_ICON_CHECK, KIT_COLOR_GREEN, &kit_display_44, 0);
        lv_obj_center(chk);
        add_label(col, "PRONTO", KIT_COLOR_TEXT, &kit_mono_26, 3);
        onb_sentence(col, "Boa game night! O KIT foi feito com carinho para pessoas como voc\xC3\xAA.", &kit_sans_22);
        btn_txt = "COME\xC3\x87""AR";
        finish = true;
        break;
    }
    }

    lv_obj_t *btn = make_button(s_onboarding_screen, btn_txt,
                                finish ? onboarding_finish_cb : onboarding_next_cb, true);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    if (finish) {
        lv_obj_set_style_bg_color(btn, lv_color_hex(KIT_COLOR_GREEN), 0);
        lv_obj_set_style_text_color(lv_obj_get_child(btn, 0), lv_color_hex(KIT_COLOR_ON_COLOR), 0);
    }
}

static void onboarding_start_if_needed(void)
{
    uint8_t done = 0;
    kit_config_get_u8("onboarded", &done, 0);
    if (!done) onboarding_show(0);
}

static void onboarding_replay_shutdown_cb(lv_timer_t *t)
{
    lv_timer_delete(t);
    kit_power_shutdown();   // AXP2101 soft power-off; a introdução volta no próximo boot
}

// "Repetir introdução": limpa a flag e desliga o aparelho. Quem religa cai
// direto na introdução, como se fosse o primeiro boot.
static void repeat_onboarding_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_sfx_impl(KIT_SFX_BACK);
    kit_config_set_u8("onboarded", 0);
    if (s_settings_screen) {
        lv_obj_delete(s_settings_screen);
        s_settings_screen = NULL;
    }

    lv_obj_t *bye = make_overlay(KIT_COLOR_BG);
    lv_obj_t *l = add_label(bye, "AT\xC3\x89 J\xC3\x81", KIT_COLOR_TEXT_MUTED, &kit_mono_26, 4);
    lv_obj_center(l);

    lv_timer_t *t = lv_timer_create(onboarding_replay_shutdown_cb, 900, NULL);
    lv_timer_set_repeat_count(t, 1);
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

static lv_obj_t *make_sound_row(lv_obj_t *parent)
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
    return row;
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
    kit_audio_sfx_impl(KIT_SFX_CLICK);
    if (s_settings_screen) return;

    s_settings_screen = make_overlay(KIT_COLOR_BG);
    make_titlebar(s_settings_screen, "AJUSTES", close_settings_cb);

    lv_obj_t *body = make_scroll_body(s_settings_screen, 0);
    make_row(body, NULL, 0, "Tela",             false, open_display_cb,   NULL);
    make_row(body, NULL, 0, "Som",              false, open_sound_cb,     NULL);
    make_row(body, NULL, 0, "Wi-Fi",            false, open_wifi_cb,      NULL);
    make_row(body, NULL, 0, "Armazenamento",    false, open_storage_cb,   NULL);
    make_row(body, NULL, 0, "Modo pen drive",   false, open_usbmsc_cb,    NULL);
    make_row(body, NULL, 0, "Bateria",          false, open_battery_cb,   NULL);
    make_row(body, NULL, 0, "Repetir introdu\xC3\xA7\xC3\xA3o", false, repeat_onboarding_cb, NULL);
    make_row(body, NULL, 0, "Sobre o KIT",      false, open_about_cb,     NULL);
}

static void close_settings_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_sfx_impl(KIT_SFX_BACK);
    if (s_settings_screen) {
        lv_obj_delete(s_settings_screen);
        s_settings_screen = NULL;
    }
}

// ---------------------------------------------------------------------------
// Tela  (Ajustes > Tela: brilho + repouso da tela)
// ---------------------------------------------------------------------------

static void open_display_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_sfx_impl(KIT_SFX_CLICK);
    if (s_display_screen) return;

    s_display_screen = make_overlay(KIT_COLOR_BG);
    make_titlebar(s_display_screen, "TELA", close_display_cb);

    lv_obj_t *body = make_scroll_body(s_display_screen, 0);
    make_row(body, NULL, 0, "Brilho",          false, open_brightness_cb, NULL);
    make_row(body, NULL, 0, "Repouso da tela", false, open_sleep_cb,      NULL);
}

static void close_display_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_sfx_impl(KIT_SFX_BACK);
    if (s_display_screen) {
        lv_obj_delete(s_display_screen);
        s_display_screen = NULL;
    }
}

// ---------------------------------------------------------------------------
// Brilho
// ---------------------------------------------------------------------------

static void open_brightness_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_sfx_impl(KIT_SFX_CLICK);
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
    kit_audio_sfx_impl(KIT_SFX_BACK);
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
// Som  (Ajustes > Som: liga/desliga + volume)
// ---------------------------------------------------------------------------

static void open_sound_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_sfx_impl(KIT_SFX_CLICK);
    if (s_volume_screen) return;

    s_volume_screen = make_overlay(KIT_COLOR_BG);
    make_titlebar(s_volume_screen, "SOM", close_sound_cb);

    // Liga/desliga: mesma linha usada antes nos Ajustes.
    lv_obj_t *srow = make_sound_row(s_volume_screen);
    lv_obj_align(srow, LV_ALIGN_TOP_MID, 0, KIT_TITLEBAR + 12);

    uint8_t cur = kit_config_get_volume();

    lv_obj_t *slider = lv_slider_create(s_volume_screen);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, cur, LV_ANIM_OFF);
    lv_obj_set_size(slider, KIT_DISPLAY_WIDTH - 64, 22);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, 0);
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
    lv_obj_align(s_volume_val_lbl, LV_ALIGN_CENTER, 0, 92);

    lv_obj_t *back = make_button(s_volume_screen, "VOLTAR", close_sound_cb, false);
    lv_obj_align(back, LV_ALIGN_BOTTOM_MID, 0, -14);
}

static void close_sound_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_sfx_impl(KIT_SFX_BACK);
    if (s_volume_screen) {
        lv_obj_delete(s_volume_screen);
        s_volume_screen = NULL;
        s_volume_val_lbl = NULL;
        s_sound_val_lbl = NULL;
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
    kit_audio_sfx_impl(KIT_SFX_BACK);
    if (s_sleep_screen) { lv_obj_delete(s_sleep_screen); s_sleep_screen = NULL; }
}

static void close_poweroff_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_sfx_impl(KIT_SFX_BACK);
    if (s_poweroff_screen) { lv_obj_delete(s_poweroff_screen); s_poweroff_screen = NULL; }
}

static void sleep_pick_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i >= SLEEP_OPTS_N) return;
    kit_config_set_screen_sleep_s(SLEEP_OPTS[i].seconds);
    kit_audio_sfx_impl(KIT_SFX_CLICK);
    close_sleep_cb(NULL);
}

static void poweroff_pick_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i >= POWEROFF_OPTS_N) return;
    kit_config_set_auto_poweroff_s(POWEROFF_OPTS[i].seconds);
    kit_audio_sfx_impl(KIT_SFX_CLICK);
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
    kit_audio_sfx_impl(KIT_SFX_CLICK);
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
    kit_audio_sfx_impl(KIT_SFX_CLICK);
    if (s_poweroff_screen) return;
    s_poweroff_screen = make_overlay(KIT_COLOR_BG);
    make_titlebar(s_poweroff_screen, "DESLIGAR", close_poweroff_cb);
    build_timeout_list(s_poweroff_screen,
                       "Tempo sem uso at\xC3\xA9 o KIT desligar sozinho. N\xC3\xA3o vale na tomada.",
                       POWEROFF_OPTS, POWEROFF_OPTS_N,
                       kit_config_get_auto_poweroff_s(), poweroff_pick_cb);
}

// ---------------------------------------------------------------------------
// Bateria  (Ajustes > Bateria: nível + "Desligar sozinho")
// ---------------------------------------------------------------------------

static void open_battery_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_sfx_impl(KIT_SFX_CLICK);
    if (s_battery_screen) return;

    s_battery_screen = make_overlay(KIT_COLOR_BG);
    make_titlebar(s_battery_screen, "BATERIA", close_battery_cb);

    lv_obj_t *body = make_scroll_body(s_battery_screen, 0);

    char pct[8];
    snprintf(pct, sizeof(pct), "%u%%", kit_power_get_battery_percentage());
    make_spec(body, "N\xC3\x8DVEL", pct);
    make_spec(body, "ESTADO",
              kit_power_is_charging()      ? "carregando"
              : kit_power_is_usb_connected() ? "na tomada"
                                             : "na bateria");

    make_row(body, NULL, 0, "Desligar sozinho", false, open_poweroff_cb, NULL);
}

static void close_battery_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_sfx_impl(KIT_SFX_BACK);
    if (s_battery_screen) {
        lv_obj_delete(s_battery_screen);
        s_battery_screen = NULL;
    }
}

// ---------------------------------------------------------------------------
// Sobre
// ---------------------------------------------------------------------------

static void open_about_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_sfx_impl(KIT_SFX_CLICK);
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

    make_row(body, NULL, 0, "Testes", false, run_test_tool_cb, NULL);

    lv_obj_t *sig = add_label(body, "JCRVLH EXPERIMENT", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 4);
    lv_obj_set_style_pad_top(sig, 14, 0);

    lv_obj_t *back = make_button(s_about_screen, "VOLTAR", close_about_cb, false);
    lv_obj_align(back, LV_ALIGN_BOTTOM_MID, 0, -14);
}

static void close_about_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_sfx_impl(KIT_SFX_BACK);
    if (s_about_screen) {
        lv_obj_delete(s_about_screen);
        s_about_screen = NULL;
    }
}

// ---------------------------------------------------------------------------
// Armazenamento (Ajustes > Armazenamento)
// ---------------------------------------------------------------------------

static void fmt_size(char *buf, size_t n, uint64_t bytes)
{
    double mb = (double)bytes / (1024.0 * 1024.0);
    if (mb >= 1000.0) snprintf(buf, n, "%.1f GB", mb / 1024.0);
    else              snprintf(buf, n, "%.0f MB", mb);
}

// Linha de armazenamento em duas alturas: rótulo miúdo em cima, valor em
// fonte maior embaixo. Antes era uma linha só (make_spec) e o valor longo
// batia no rótulo.
static void storage_line(lv_obj_t *body, const char *key, const char *val)
{
    lv_obj_t *r = lv_obj_create(body);
    lv_obj_set_size(r, KIT_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(r, 1, 0);
    lv_obj_set_style_border_side(r, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(r, lv_color_hex(KIT_COLOR_LINE), 0);
    lv_obj_set_style_radius(r, 0, 0);
    lv_obj_set_style_pad_all(r, 0, 0);
    lv_obj_set_style_pad_top(r, 4, 0);
    lv_obj_set_style_pad_bottom(r, 12, 0);
    lv_obj_set_style_pad_row(r, 5, 0);
    lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    add_label(r, key, KIT_COLOR_TEXT_MUTED, &kit_mono_16, 1);
    lv_obj_t *lv_val = add_label(r, val, KIT_COLOR_TEXT, &kit_sans_22, 0);
    lv_label_set_long_mode(lv_val, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lv_val, KIT_CONTENT);
}

static void storage_spec(lv_obj_t *body, const char *key, uint64_t total, uint64_t freeb)
{
    char t[16], f[16], v[48];
    fmt_size(t, sizeof(t), total);
    fmt_size(f, sizeof(f), freeb);
    snprintf(v, sizeof(v), "%s livres de %s", f, t);
    storage_line(body, key, v);
}

static void build_storage_body(void)
{
    uint64_t sd_total = 0, sd_free = 0;
    bool sd = kit_storage_sd_is_mounted() &&
              kit_storage_sd_info(&sd_total, &sd_free) == KIT_OK;

    // Cartão montado: duas pílulas empilhadas (recarregar + formatar).
    lv_obj_t *body = make_scroll_body(s_storage_screen,
                                      sd ? (2 * KIT_BTN_H + 40) : (KIT_BTN_H + 28));

    uint32_t lfs_total = 0, lfs_free = 0;
    if (kit_storage_get_info(&lfs_total, &lfs_free) == KIT_OK)
        storage_spec(body, "INTERNO", lfs_total, lfs_free);
    else
        storage_line(body, "INTERNO", "indispon\xC3\xADvel");

    if (sd) {
        storage_spec(body, "CART\xC3\x83O SD", sd_total, sd_free);
        char tv[16];
        snprintf(tv, sizeof(tv), "%lu", (unsigned long)kit_tool_manager_get_count());
        storage_line(body, "TOOLS NO CART\xC3\x83O", tv);

        lv_obj_t *rl = make_button(s_storage_screen, "RECARREGAR TOOLS", sd_scan_cb, false);
        lv_obj_align(rl, LV_ALIGN_BOTTOM_MID, 0, -(14 + KIT_BTN_H + 12));

        lv_obj_t *fmt = make_button(s_storage_screen, "FORMATAR CART\xC3\x83O", open_sd_format_cb, false);
        lv_obj_set_style_border_color(fmt, lv_color_hex(KIT_COLOR_RED), 0);
        lv_obj_align(fmt, LV_ALIGN_BOTTOM_MID, 0, -14);
        lv_obj_t *lbl = lv_obj_get_child(fmt, 0);
        if (lbl) lv_obj_set_style_text_color(lbl, lv_color_hex(KIT_COLOR_RED), 0);
    } else {
        storage_line(body, "CART\xC3\x83O SD", "n\xC3\xA3o encontrado");
        // Cartão inserido depois do boot: monta agora e recarrega o catálogo,
        // sem exigir reiniciar o KIT.
        lv_obj_t *scan = make_button(s_storage_screen, "PROCURAR CART\xC3\x83O", sd_scan_cb, false);
        lv_obj_align(scan, LV_ALIGN_BOTTOM_MID, 0, -14);
    }
}

static void sd_scan_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_sfx_impl(KIT_SFX_CLICK);
    bool was_mounted = kit_storage_sd_is_mounted();
    bool ok = (kit_storage_sd_mount() == KIT_OK);
    if (ok) kit_tool_manager_reload_catalog();   // dispara launcher_catalog_changed
    if (s_storage_screen) {
        lv_obj_clean(s_storage_screen);
        make_titlebar(s_storage_screen, "ARMAZENAMENTO", close_storage_cb);
        build_storage_body();
    }
    show_toast(!ok ? "SEM CART\xC3\x83O"
                   : was_mounted ? "TOOLS RECARREGADAS" : "CART\xC3\x83O MONTADO");
}

static void open_storage_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_sfx_impl(KIT_SFX_CLICK);
    if (s_storage_screen) return;
    s_storage_screen = make_overlay(KIT_COLOR_BG);
    make_titlebar(s_storage_screen, "ARMAZENAMENTO", close_storage_cb);
    build_storage_body();
}

static void close_storage_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_sfx_impl(KIT_SFX_BACK);
    if (s_storage_screen) { lv_obj_delete(s_storage_screen); s_storage_screen = NULL; }
}

// -- Confirmação de formatar o cartão --------------------------------------

static void open_sd_format_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_sfx_impl(KIT_SFX_CLICK);
    if (s_sd_format_screen) return;
    s_sd_format_screen = make_overlay(KIT_COLOR_BG);
    make_titlebar(s_sd_format_screen, "FORMATAR", close_sd_format_cb);

    lv_obj_t *body = make_scroll_body(s_sd_format_screen, 2 * KIT_BTN_H + 40);
    lv_obj_t *warn = add_label(body,
        "Isso APAGA TUDO no cart\xC3\xA3o e recria a pasta tools/ do KIT. "
        "N\xC3\xA3o d\xC3\xA1 pra desfazer.", KIT_COLOR_TEXT, &kit_sans_22, 0);
    lv_label_set_long_mode(warn, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(warn, KIT_CONTENT);

    lv_obj_t *ok = make_button(s_sd_format_screen, "FORMATAR AGORA", do_sd_format_cb, true);
    lv_obj_set_style_bg_color(ok, lv_color_hex(KIT_COLOR_RED), 0);
    lv_obj_t *okl = lv_obj_get_child(ok, 0);
    if (okl) lv_obj_set_style_text_color(okl, lv_color_hex(KIT_COLOR_ON_COLOR), 0);
    lv_obj_align(ok, LV_ALIGN_BOTTOM_MID, 0, -(14 + KIT_BTN_H + 12));

    lv_obj_t *no = make_button(s_sd_format_screen, "CANCELAR", close_sd_format_cb, false);
    lv_obj_align(no, LV_ALIGN_BOTTOM_MID, 0, -14);
}

static void close_sd_format_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_sfx_impl(KIT_SFX_BACK);
    if (s_sd_format_screen) { lv_obj_delete(s_sd_format_screen); s_sd_format_screen = NULL; }
}

static void do_sd_format_cb(lv_event_t *e)
{
    (void)e;

    // "FORMATANDO..." precisa aparecer ANTES da chamada bloqueante — força o
    // desenho imediato (a task LVGL fica presa durante o format).
    lv_obj_t *busy = make_overlay(KIT_COLOR_BG);
    lv_obj_t *msg = add_label(busy, "FORMATANDO O CART\xC3\x83O...", KIT_COLOR_TEXT, &kit_mono_20, 3);
    lv_obj_center(msg);
    lv_refr_now(NULL);

    kit_err_t err = kit_storage_sd_format();
    kit_tool_manager_reload_catalog();   // catálogo agora está vazio

    lv_obj_delete(busy);
    if (s_sd_format_screen) { lv_obj_delete(s_sd_format_screen); s_sd_format_screen = NULL; }

    if (s_storage_screen) {   // recarrega com os números novos
        lv_obj_clean(s_storage_screen);
        make_titlebar(s_storage_screen, "ARMAZENAMENTO", close_storage_cb);
        build_storage_body();
    }

    if (err == KIT_OK) {
        kit_audio_sfx_impl(KIT_SFX_CONFIRM);
        show_toast("CART\xC3\x83O PRONTO");
    } else {
        kit_audio_beep_impl(400, 60);
        show_toast("FALHOU");
    }
}

// ---------------------------------------------------------------------------
// Modo pen drive (USB Mass Storage)  —  Ajustes > Modo pen drive
// ---------------------------------------------------------------------------

static lv_obj_t  *s_usbmsc_diag_lbl = NULL;
static lv_timer_t *s_usbmsc_poll = NULL;

static void usbmsc_poll_cb(lv_timer_t *t)
{
    (void)t;
    if (!s_usbmsc_diag_lbl) return;
    uint32_t mb = kit_usb_msc_card_mb();
    bool usb = kit_usb_msc_usb_ready();
    bool host = kit_usb_msc_host_connected();
    char buf[96];
    snprintf(buf, sizeof(buf), "CART\xC3\x83O %lu MB\nUSB %s\nPC %s",
             (unsigned long)mb,
             usb  ? "conectado" : "aguardando...",
             host ? "leu o cart\xC3\xA3o" : "-");
    lv_label_set_text(s_usbmsc_diag_lbl, buf);
    lv_obj_set_style_text_color(s_usbmsc_diag_lbl,
        lv_color_hex(usb ? KIT_COLOR_GREEN : KIT_COLOR_TEXT_MUTED), 0);
}

static void usbmsc_stop_poll(void)
{
    if (s_usbmsc_poll) { lv_timer_delete(s_usbmsc_poll); s_usbmsc_poll = NULL; }
    s_usbmsc_diag_lbl = NULL;
}

// Tela "ativo": o cartão está no computador. A única saída é reiniciar.
static void usbmsc_show_active(void)
{
    usbmsc_stop_poll();
    lv_obj_clean(s_usbmsc_screen);

    lv_obj_t *col = make_group(s_usbmsc_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, 14, 0);
    lv_obj_set_width(col, KIT_CONTENT);
    lv_obj_align(col, LV_ALIGN_CENTER, 0, -30);

    add_label(col, "MODO PEN DRIVE", KIT_COLOR_TEXT_MUTED, &kit_mono_20, 4);
    add_label(col, "ATIVO", KIT_COLOR_GREEN, &kit_display_44, 0);

    lv_obj_t *hint = add_label(col,
        "Copie os arquivos .kit pelo computador.",
        KIT_COLOR_TEXT_MUTED, &kit_sans_22, 0);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(hint, KIT_CONTENT);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *warn = add_label(col,
        "EJETE O CART\xC3\x83O NO COMPUTADOR ANTES DE SAIR",
        KIT_COLOR_YELLOW, &kit_mono_16, 2);
    lv_label_set_long_mode(warn, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(warn, KIT_CONTENT);
    lv_obj_set_style_text_align(warn, LV_TEXT_ALIGN_CENTER, 0);

    s_usbmsc_diag_lbl = add_label(col, "", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 1);
    lv_obj_set_style_text_align(s_usbmsc_diag_lbl, LV_TEXT_ALIGN_CENTER, 0);
    s_usbmsc_poll = lv_timer_create(usbmsc_poll_cb, 500, NULL);
    usbmsc_poll_cb(NULL);

    lv_obj_t *out = make_button(s_usbmsc_screen, "SAIR (REINICIA)", usbmsc_exit_cb, true);
    lv_obj_set_style_bg_color(out, lv_color_hex(KIT_COLOR_RED), 0);
    lv_obj_t *ol = lv_obj_get_child(out, 0);
    if (ol) lv_obj_set_style_text_color(ol, lv_color_hex(KIT_COLOR_ON_COLOR), 0);
    lv_obj_align(out, LV_ALIGN_BOTTOM_MID, 0, -14);
}

static void open_usbmsc_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_sfx_impl(KIT_SFX_CLICK);
    if (s_usbmsc_screen) return;
    s_usbmsc_screen = make_overlay(KIT_COLOR_BG);
    make_titlebar(s_usbmsc_screen, "PEN DRIVE", close_usbmsc_cb);

    if (kit_usb_msc_is_active()) { usbmsc_show_active(); return; }

    lv_obj_t *body = make_scroll_body(s_usbmsc_screen, KIT_BTN_H + 28);
    lv_obj_t *tx = add_label(body,
        "Liga o cart\xC3\xA3o do KIT no computador como um pen drive, pra voc\xC3\xAA "
        "copiar e organizar os arquivos .kit sem tirar o cart\xC3\xA3o.\n\n"
        "Enquanto estiver ligado o KIT fica em espera e o cabo n\xC3\xA3o serve "
        "de console. Ao terminar, ejete no computador e toque em SAIR: "
        "o KIT reinicia e l\xC3\xAA as Tools novas.",
        KIT_COLOR_TEXT, &kit_sans_22, 0);
    lv_label_set_long_mode(tx, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(tx, KIT_CONTENT);

    bool sd = kit_storage_sd_is_mounted();
    lv_obj_t *btn = make_button(s_usbmsc_screen, sd ? "ATIVAR" : "SEM CART\xC3\x83O",
                                usbmsc_activate_cb, true);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -14);
    if (!sd) lv_obj_add_state(btn, LV_STATE_DISABLED);
}

static void close_usbmsc_cb(lv_event_t *e)
{
    (void)e;
    // Com o modo ativo não há volta limpa — sair = reiniciar.
    if (kit_usb_msc_is_active()) { usbmsc_exit_cb(NULL); return; }
    kit_audio_sfx_impl(KIT_SFX_BACK);
    if (s_usbmsc_screen) { lv_obj_delete(s_usbmsc_screen); s_usbmsc_screen = NULL; }
}

static void usbmsc_activate_cb(lv_event_t *e)
{
    (void)e;
    if (!kit_storage_sd_is_mounted()) { show_toast("SEM CART\xC3\x83O"); return; }

    lv_obj_t *busy = make_overlay(KIT_COLOR_BG);
    lv_obj_t *m = add_label(busy, "ATIVANDO...", KIT_COLOR_TEXT, &kit_mono_20, 3);
    lv_obj_center(m);
    lv_refr_now(NULL);

    kit_err_t err = kit_usb_msc_enter();
    lv_obj_delete(busy);

    if (err != KIT_OK) {
        kit_audio_beep_impl(400, 60);
        show_toast("N\xC3\x83O ATIVOU");
        return;
    }
    kit_audio_sfx_impl(KIT_SFX_CONFIRM);
    if (s_usbmsc_screen) usbmsc_show_active();
}

static void usbmsc_confirm_back_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_sfx_impl(KIT_SFX_BACK);
    if (s_usbmsc_screen) usbmsc_show_active();
}

static void usbmsc_do_exit_cb(lv_event_t *e)
{
    (void)e;
    lv_obj_t *busy = make_overlay(KIT_COLOR_BG);
    lv_obj_t *m = add_label(busy, "REINICIANDO...", KIT_COLOR_TEXT, &kit_mono_20, 3);
    lv_obj_center(m);
    lv_refr_now(NULL);
    kit_usb_msc_exit_reboot();   // não retorna
}

// SAIR não reinicia direto: pergunta se o cartão já foi ejetado no computador
// (sair com o disco ainda montado pode corromper os arquivos .kit).
static void usbmsc_exit_cb(lv_event_t *e)
{
    (void)e;
    if (!s_usbmsc_screen) { usbmsc_do_exit_cb(NULL); return; }
    kit_audio_sfx_impl(KIT_SFX_CLICK);
    usbmsc_stop_poll();
    lv_obj_clean(s_usbmsc_screen);
    make_titlebar(s_usbmsc_screen, "PEN DRIVE", usbmsc_confirm_back_cb);

    lv_obj_t *body = make_scroll_body(s_usbmsc_screen, 2 * KIT_BTN_H + 40);
    lv_obj_t *q = add_label(body,
        "J\xC3\xA1 ejetou o cart\xC3\xA3o no computador?\n\n"
        "Sair com o disco ainda montado pode corromper os arquivos.",
        KIT_COLOR_TEXT, &kit_sans_22, 0);
    lv_label_set_long_mode(q, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(q, KIT_CONTENT);

    lv_obj_t *ok = make_button(s_usbmsc_screen, "J\xC3\x81 EJETEI, SAIR", usbmsc_do_exit_cb, true);
    lv_obj_set_style_bg_color(ok, lv_color_hex(KIT_COLOR_RED), 0);
    lv_obj_t *okl = lv_obj_get_child(ok, 0);
    if (okl) lv_obj_set_style_text_color(okl, lv_color_hex(KIT_COLOR_ON_COLOR), 0);
    lv_obj_align(ok, LV_ALIGN_BOTTOM_MID, 0, -(14 + KIT_BTN_H + 12));

    lv_obj_t *no = make_button(s_usbmsc_screen, "AINDA N\xC3\x83O", usbmsc_confirm_back_cb, false);
    lv_obj_align(no, LV_ALIGN_BOTTOM_MID, 0, -14);
}

// ---------------------------------------------------------------------------
// Wi-Fi  (Ajustes > Wi-Fi)  +  portal de provisionamento
// ---------------------------------------------------------------------------

static lv_obj_t  *s_wifi_status_lbl = NULL;
static lv_obj_t  *s_wifi_toggle_lbl = NULL;
static lv_timer_t *s_wifi_poll = NULL;

static lv_obj_t  *s_wifi_portal_status_lbl = NULL;
static lv_timer_t *s_wifi_portal_poll = NULL;
static int        s_wifi_portal_ticks = 0;
static bool       s_wifi_portal_done = false;

static lv_obj_t  *s_wifi_forget_screen = NULL;
static char       s_wifi_forget_ssid[KIT_NET_SSID_MAX] = {0};

static void wifi_screen_reload(void);
static void wifi_forget_close_cb(lv_event_t *e);
static void wifi_forget_do_cb(lv_event_t *e);

static void wifi_poll_cb(lv_timer_t *t)
{
    (void)t;
    if (!s_wifi_screen) return;

    kit_net_state_t st = kit_network_get_state();

    if (s_wifi_toggle_lbl) {
        bool on = (st != KIT_NET_OFF);
        lv_label_set_text(s_wifi_toggle_lbl, on ? "LIGADO" : "DESLIGADO");
        lv_obj_set_style_text_color(s_wifi_toggle_lbl,
            lv_color_hex(on ? KIT_COLOR_GREEN : KIT_COLOR_TEXT_MUTED), 0);
    }

    if (s_wifi_status_lbl) {
        char buf[112];
        switch (st) {
        case KIT_NET_CONNECTED: {
            char ssid[KIT_NET_SSID_MAX] = {0}, ip[16] = {0};
            kit_network_get_ssid(ssid, sizeof(ssid));
            kit_network_get_ip(ip, sizeof(ip));
            snprintf(buf, sizeof(buf), "Conectado a %s\n%s", ssid, ip);
            break;
        }
        case KIT_NET_CONNECTING:
            snprintf(buf, sizeof(buf), "Procurando rede...");
            break;
        case KIT_NET_PROVISIONING:
            snprintf(buf, sizeof(buf), "Portal de configura\xC3\xA7\xC3\xA3o ativo...");
            break;
        case KIT_NET_DISCONNECTED:
            snprintf(buf, sizeof(buf), kit_network_saved_count() > 0
                     ? "Nenhuma rede conhecida por perto."
                     : "Nenhuma rede salva. Toque em CONFIGURAR REDE.");
            break;
        default:
            snprintf(buf, sizeof(buf), "R\xC3\xA1""dio desligado.");
            break;
        }
        lv_label_set_text(s_wifi_status_lbl, buf);
    }
}

static void wifi_toggle_cb(lv_event_t *e)
{
    (void)e;
    bool turn_on = (kit_network_get_state() == KIT_NET_OFF);
    kit_config_set_u8("wifi_en", turn_on ? 1 : 0);
    if (turn_on) {
        kit_network_start();
        kit_audio_sfx_impl(KIT_SFX_CLICK);
    } else {
        kit_network_stop();
        kit_audio_sfx_impl(KIT_SFX_BACK);
    }
    wifi_poll_cb(NULL);
}

// Tocar numa rede salva abre a confirmação de esquecer (não esquece de cara).
static void wifi_forget_cb(lv_event_t *e)
{
    size_t idx = (size_t)(intptr_t)lv_event_get_user_data(e);
    if (kit_network_saved_ssid(idx, s_wifi_forget_ssid,
                               sizeof(s_wifi_forget_ssid)) != KIT_OK) return;
    kit_audio_sfx_impl(KIT_SFX_CLICK);
    if (s_wifi_forget_screen) return;

    s_wifi_forget_screen = make_overlay(KIT_COLOR_BG);
    make_titlebar(s_wifi_forget_screen, "ESQUECER", wifi_forget_close_cb);

    lv_obj_t *body = make_scroll_body(s_wifi_forget_screen, 2 * KIT_BTN_H + 40);
    lv_obj_t *q = add_label(body,
        "Esquecer esta rede? O KIT n\xC3\xA3o vai mais conectar nela sozinho.",
        KIT_COLOR_TEXT, &kit_sans_22, 0);
    lv_label_set_long_mode(q, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(q, KIT_CONTENT);

    lv_obj_t *name = add_label(body, s_wifi_forget_ssid, KIT_COLOR_YELLOW, &kit_sans_28, 1);
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    lv_obj_set_width(name, KIT_CONTENT);
    lv_obj_set_style_pad_top(name, 6, 0);

    lv_obj_t *ok = make_button(s_wifi_forget_screen, "ESQUECER", wifi_forget_do_cb, true);
    lv_obj_set_style_bg_color(ok, lv_color_hex(KIT_COLOR_RED), 0);
    lv_obj_t *okl = lv_obj_get_child(ok, 0);
    if (okl) lv_obj_set_style_text_color(okl, lv_color_hex(KIT_COLOR_ON_COLOR), 0);
    lv_obj_align(ok, LV_ALIGN_BOTTOM_MID, 0, -(14 + KIT_BTN_H + 12));

    lv_obj_t *no = make_button(s_wifi_forget_screen, "CANCELAR", wifi_forget_close_cb, false);
    lv_obj_align(no, LV_ALIGN_BOTTOM_MID, 0, -14);
}

static void wifi_forget_close_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_sfx_impl(KIT_SFX_BACK);
    if (s_wifi_forget_screen) { lv_obj_delete(s_wifi_forget_screen); s_wifi_forget_screen = NULL; }
}

static void wifi_forget_do_cb(lv_event_t *e)
{
    (void)e;
    kit_network_forget(s_wifi_forget_ssid);
    kit_audio_sfx_impl(KIT_SFX_CONFIRM);
    if (s_wifi_forget_screen) { lv_obj_delete(s_wifi_forget_screen); s_wifi_forget_screen = NULL; }
    show_toast("REDE ESQUECIDA");
    wifi_screen_reload();
}

// Linha de liga/desliga do Wi-Fi (mesma pegada da linha "Som").
static void make_wifi_toggle_row(lv_obj_t *parent)
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
    lv_obj_add_event_cb(row, wifi_toggle_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *l = add_label(row, "Wi-Fi", KIT_COLOR_TEXT, &kit_sans_22, 0);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 24, 0);

    s_wifi_toggle_lbl = add_label(row, "", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 1);
    lv_obj_align(s_wifi_toggle_lbl, LV_ALIGN_RIGHT_MID, -20, 0);
}

static void wifi_screen_populate(void)
{
    lv_obj_t *body = make_scroll_body(s_wifi_screen, KIT_BTN_H + 28);

    make_wifi_toggle_row(body);

    s_wifi_status_lbl = add_label(body, "", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 1);
    lv_label_set_long_mode(s_wifi_status_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_wifi_status_lbl, KIT_CONTENT);
    lv_obj_set_style_pad_left(s_wifi_status_lbl, 6, 0);
    lv_obj_set_style_pad_bottom(s_wifi_status_lbl, 4, 0);

    size_t n = kit_network_saved_count();
    if (n > 0) {
        lv_obj_t *hdr = add_label(body, "REDES SALVAS", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 3);
        lv_obj_set_style_pad_left(hdr, 6, 0);
        lv_obj_set_style_pad_top(hdr, 8, 0);
        for (size_t i = 0; i < n; i++) {
            char ssid[KIT_NET_SSID_MAX] = {0};
            if (kit_network_saved_ssid(i, ssid, sizeof(ssid)) != KIT_OK) continue;
            make_row(body, NULL, 0, ssid, false, wifi_forget_cb, (void *)(intptr_t)i);
        }
        lv_obj_t *tip = add_label(body, "Toque numa rede para esquec\xC3\xAA-la.",
                                  KIT_COLOR_TEXT_MUTED, &kit_mono_16, 1);
        lv_label_set_long_mode(tip, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(tip, KIT_CONTENT);
        lv_obj_set_style_pad_left(tip, 6, 0);
    }

    lv_obj_t *cfg = make_button(s_wifi_screen, "CONFIGURAR REDE", wifi_portal_open_cb, true);
    lv_obj_align(cfg, LV_ALIGN_BOTTOM_MID, 0, -14);

    wifi_poll_cb(NULL);
}

static void wifi_screen_reload(void)
{
    if (!s_wifi_screen) return;
    s_wifi_status_lbl = NULL;
    s_wifi_toggle_lbl = NULL;
    lv_obj_clean(s_wifi_screen);
    make_titlebar(s_wifi_screen, "WI-FI", close_wifi_cb);
    wifi_screen_populate();
}

static void open_wifi_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_sfx_impl(KIT_SFX_CLICK);
    if (s_wifi_screen) return;

    s_wifi_screen = make_overlay(KIT_COLOR_BG);
    make_titlebar(s_wifi_screen, "WI-FI", close_wifi_cb);
    wifi_screen_populate();

    s_wifi_poll = lv_timer_create(wifi_poll_cb, 1500, NULL);
}

static void close_wifi_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_sfx_impl(KIT_SFX_BACK);
    if (s_wifi_poll) { lv_timer_delete(s_wifi_poll); s_wifi_poll = NULL; }
    if (s_wifi_screen) {
        lv_obj_delete(s_wifi_screen);
        s_wifi_screen = NULL;
        s_wifi_status_lbl = NULL;
        s_wifi_toggle_lbl = NULL;
    }
}

// Wi-Fi associou pelo portal: derruba o portal + a tela de Wi-Fi, solta o
// keep-awake, volta para a Home e mostra o mesmo overlay curto do "carregando".
static void wifi_portal_finish_connected(void)
{
    kit_audio_sfx_impl(KIT_SFX_CONFIRM);

    if (s_wifi_portal_poll) { lv_timer_delete(s_wifi_portal_poll); s_wifi_portal_poll = NULL; }
    if (s_wifi_poll)        { lv_timer_delete(s_wifi_poll);        s_wifi_poll = NULL; }

    kit_network_portal_stop();
    kit_power_keep_awake_impl(false);

    if (s_wifi_portal_screen) {
        lv_obj_delete(s_wifi_portal_screen);
        s_wifi_portal_screen = NULL;
        s_wifi_portal_status_lbl = NULL;
    }
    if (s_wifi_screen) {
        lv_obj_delete(s_wifi_screen);
        s_wifi_screen = NULL;
        s_wifi_status_lbl = NULL;
        s_wifi_toggle_lbl = NULL;
    }

    kit_launcher_go_home();
    show_feedback(KIT_COLOR_GREEN, KIT_ICON_BARS, "CONECTADO");
}

static void wifi_portal_poll_cb(lv_timer_t *t)
{
    (void)t;
    if (!s_wifi_portal_screen || !s_wifi_portal_status_lbl) return;

    s_wifi_portal_ticks++;

    if (kit_network_is_connected()) {
        if (!s_wifi_portal_done) {
            s_wifi_portal_done = true;
            wifi_portal_finish_connected();
        }
        return;
    }

    lv_label_set_text(s_wifi_portal_status_lbl,
        kit_network_get_state() == KIT_NET_CONNECTING
            ? "Tentando conectar..."
            : "Aguardando o celular...");

    // Tempo limite: 4 min segurando a tela acesa é o bastante.
    if (s_wifi_portal_ticks > 240) wifi_portal_close_cb(NULL);
}

static void wifi_portal_open_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_sfx_impl(KIT_SFX_CLICK);
    if (s_wifi_portal_screen) return;

    kit_config_set_u8("wifi_en", 1);
    kit_power_keep_awake_impl(true);
    kit_network_portal_start(kit_power_get_device_id());

    s_wifi_portal_ticks = 0;
    s_wifi_portal_done = false;

    s_wifi_portal_screen = make_overlay(KIT_COLOR_BG);
    make_titlebar(s_wifi_portal_screen, "CONFIGURAR", wifi_portal_close_cb);

    lv_obj_t *body = make_scroll_body(s_wifi_portal_screen, 0);

    add_label(body, "1  No celular, entre na rede Wi-Fi:",
              KIT_COLOR_TEXT, &kit_sans_22, 0);
    lv_obj_t *ap = add_label(body, kit_power_get_device_id(),
                             KIT_COLOR_YELLOW, &kit_sans_28, 1);
    lv_obj_set_style_pad_bottom(ap, 6, 0);

    lv_obj_t *s2 = add_label(body,
        "2  A tela de configura\xC3\xA7\xC3\xA3o abre sozinha. Escolha a sua rede e "
        "digite a senha.",
        KIT_COLOR_TEXT, &kit_sans_22, 0);
    lv_label_set_long_mode(s2, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s2, KIT_CONTENT);

    lv_obj_t *s3 = add_label(body,
        "N\xC3\xA3o abriu? V\xC3\xA1 no navegador em 192.168.4.1",
        KIT_COLOR_TEXT_MUTED, &kit_mono_16, 1);
    lv_label_set_long_mode(s3, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s3, KIT_CONTENT);
    lv_obj_set_style_pad_bottom(s3, 10, 0);

    s_wifi_portal_status_lbl = add_label(body, "Aguardando o celular...",
                                         KIT_COLOR_TEXT_MUTED, &kit_mono_16, 1);
    lv_label_set_long_mode(s_wifi_portal_status_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_wifi_portal_status_lbl, KIT_CONTENT);

    s_wifi_portal_poll = lv_timer_create(wifi_portal_poll_cb, 1000, NULL);
}

static void wifi_portal_close_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_sfx_impl(KIT_SFX_BACK);
    if (s_wifi_portal_poll) { lv_timer_delete(s_wifi_portal_poll); s_wifi_portal_poll = NULL; }

    kit_network_portal_stop();
    kit_power_keep_awake_impl(false);

    if (s_wifi_portal_screen) {
        lv_obj_delete(s_wifi_portal_screen);
        s_wifi_portal_screen = NULL;
        s_wifi_portal_status_lbl = NULL;
    }
    if (s_wifi_screen) wifi_screen_reload();
}

// ---------------------------------------------------------------------------
// Catálogo de Tools  (Home > VER TODOS > SISTEMA > Catálogo)
// ---------------------------------------------------------------------------

static lv_timer_t *s_catalog_poll = NULL;
static int         s_catalog_last_state = -1;
static char        s_catalog_sel_id[40] = {0};
static bool        s_catalog_op_was_install = false;   // último trabalho: baixar (true) x remover

static void catalog_list_rebuild(void);
static void catalog_detail_rebuild(void);
static void catalog_row_cb(lv_event_t *e);
static void catalog_refresh_cb(lv_event_t *e);
static void catalog_action_cb(lv_event_t *e);
static void catalog_remove_cb(lv_event_t *e);
static void catalog_detail_close_cb(lv_event_t *e);
static void catalog_confirm_close_cb(lv_event_t *e);
static void catalog_confirm_do_cb(lv_event_t *e);
static void catalog_goto_wifi_cb(lv_event_t *e);

// -- Overlay de progresso (baixando / instalando) -------------------------
static void catalog_busy_show(const char *title)
{
    if (s_catalog_busy_screen) return;
    // Segura o repouso: um download em curso não pode ver a tela apagar (e o
    // rádio Wi-Fi cair junto — ver kit_network_suspend).
    kit_power_keep_awake_impl(true);
    s_catalog_busy_screen = make_overlay(KIT_COLOR_BG);
    lv_obj_t *col = make_group(s_catalog_busy_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, 18, 0);
    lv_obj_center(col);
    add_label(col, title, KIT_COLOR_TEXT_MUTED, &kit_mono_20, 4);
    // kit_display_72 só tem dígitos + A-Z; nada de "%" ou "." (viram retângulo).
    lv_obj_t *pct = add_label(col, "", KIT_COLOR_TEXT, &kit_display_72, 0);
    lv_obj_set_user_data(s_catalog_busy_screen, pct);
}

static void catalog_busy_update(void)
{
    if (!s_catalog_busy_screen) return;
    lv_obj_t *pct = lv_obj_get_user_data(s_catalog_busy_screen);
    if (!pct) return;
    int p = kit_catalog_progress();
    if (p < 0) lv_label_set_text(pct, "");
    else       lv_label_set_text_fmt(pct, "%d", p);
}

static void catalog_busy_hide(void)
{
    if (s_catalog_busy_screen) { lv_obj_delete(s_catalog_busy_screen); s_catalog_busy_screen = NULL; }
    kit_power_keep_awake_impl(false);
}

// -- Linha do catálogo: nome + versão + pílula de estado ------------------
static const char *cat_status_text(kit_cat_install_t s)
{
    switch (s) {
    case KIT_CAT_UPDATE:    return "ATUALIZAR";
    case KIT_CAT_INSTALLED: return "INSTALADA";
    default:                return "INSTALAR";
    }
}
static uint32_t cat_status_color(kit_cat_install_t s)
{
    switch (s) {
    case KIT_CAT_UPDATE:    return KIT_COLOR_GREEN;
    case KIT_CAT_INSTALLED: return KIT_COLOR_TEXT_MUTED;
    default:                return KIT_COLOR_YELLOW;
    }
}

static void catalog_make_row(lv_obj_t *body, const kit_catalog_entry_t *e, int idx)
{
    lv_obj_t *row = lv_obj_create(body);
    lv_obj_set_size(row, KIT_CONTENT, KIT_ROW_H);
    lv_obj_set_style_bg_color(row, lv_color_hex(KIT_COLOR_SURFACE), 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, 24, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(row, 6);
    lv_obj_add_event_cb(row, catalog_row_cb, LV_EVENT_CLICKED, (void *)(intptr_t)idx);

    lv_obj_t *nm = add_label(row, e->name, KIT_COLOR_TEXT, &kit_sans_22, 0);
    lv_obj_align(nm, LV_ALIGN_LEFT_MID, 20, -10);
    lv_obj_t *vs = add_label(row, e->version, KIT_COLOR_TEXT_MUTED, &kit_mono_16, 1);
    lv_obj_align(vs, LV_ALIGN_LEFT_MID, 20, 14);

    lv_obj_t *chip = add_label(row, cat_status_text(e->install),
                               cat_status_color(e->install), &kit_mono_16, 2);
    lv_obj_align(chip, LV_ALIGN_RIGHT_MID, -18, 0);
}

// Reconstrói o corpo da tela de lista conforme o estado do kit_catalog.
static void catalog_list_rebuild(void)
{
    if (!s_catalog_screen) return;
    lv_obj_clean(s_catalog_screen);
    make_titlebar(s_catalog_screen, "CAT\xC3\x81LOGO", close_catalog_cb);

    kit_catalog_state_t st = kit_catalog_get_state();

    if (!kit_network_is_connected() || st == KIT_CAT_OFFLINE) {
        lv_obj_t *body = make_scroll_body(s_catalog_screen, KIT_BTN_H + 28);
        lv_obj_t *m = add_label(body,
            "O cat\xC3\xA1logo precisa de Wi-Fi. Conecte o KIT a uma rede para ver e "
            "baixar Tools.", KIT_COLOR_TEXT, &kit_sans_22, 0);
        lv_label_set_long_mode(m, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(m, KIT_CONTENT);
        lv_obj_t *b = make_button(s_catalog_screen, "IR PARA WI-FI", catalog_goto_wifi_cb, true);
        lv_obj_align(b, LV_ALIGN_BOTTOM_MID, 0, -14);
        return;
    }

    if (st == KIT_CAT_FETCHING) {
        lv_obj_t *m = add_label(s_catalog_screen, "Buscando cat\xC3\xA1logo...",
                                KIT_COLOR_TEXT_MUTED, &kit_mono_20, 3);
        lv_obj_center(m);
        return;
    }

    if (st == KIT_CAT_FETCH_ERR) {
        lv_obj_t *body = make_scroll_body(s_catalog_screen, KIT_BTN_H + 28);
        lv_obj_t *m = add_label(body, kit_catalog_last_error(), KIT_COLOR_TEXT, &kit_sans_22, 0);
        lv_label_set_long_mode(m, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(m, KIT_CONTENT);
        lv_obj_t *b = make_button(s_catalog_screen, "TENTAR DE NOVO", catalog_refresh_cb, true);
        lv_obj_align(b, LV_ALIGN_BOTTOM_MID, 0, -14);
        return;
    }

    // READY (ou WORK_OK/ERR — a lista segue visível)
    lv_obj_t *body = make_scroll_body(s_catalog_screen, KIT_BTN_H + 28);
    uint32_t n = kit_catalog_get_count();
    if (n == 0) {
        add_label(body, "Nenhuma Tool no cat\xC3\xA1logo ainda.",
                  KIT_COLOR_TEXT_MUTED, &kit_mono_16, 1);
    }
    for (uint32_t i = 0; i < n; i++) {
        kit_catalog_entry_t e;
        if (kit_catalog_get_entry(i, &e) == KIT_OK) catalog_make_row(body, &e, (int)i);
    }
    lv_obj_t *b = make_button(s_catalog_screen, "ATUALIZAR LISTA", catalog_refresh_cb, false);
    lv_obj_align(b, LV_ALIGN_BOTTOM_MID, 0, -14);
}

static void catalog_poll_cb(lv_timer_t *t)
{
    (void)t;
    if (!s_catalog_screen) return;

    kit_catalog_state_t st = kit_catalog_get_state();
    catalog_busy_update();

    if ((int)st == s_catalog_last_state) return;
    int prev = s_catalog_last_state;
    s_catalog_last_state = (int)st;

    switch (st) {
    case KIT_CAT_WORKING:
        if (!s_catalog_busy_screen) catalog_busy_show("BAIXANDO");
        break;
    case KIT_CAT_WORK_OK:
        catalog_busy_hide();
        kit_audio_sfx_impl(s_catalog_op_was_install ? KIT_SFX_CATALOG_DONE : KIT_SFX_CONFIRM);
        show_toast("PRONTO");
        catalog_list_rebuild();
        if (s_catalog_detail_screen) catalog_detail_rebuild();
        s_home_deck_dirty = true;   // a Home muda quando a lista de Tools muda
        break;
    case KIT_CAT_WORK_ERR:
        catalog_busy_hide();
        kit_audio_beep_impl(400, 60);
        show_toast(kit_catalog_last_error());
        catalog_list_rebuild();
        if (s_catalog_detail_screen) catalog_detail_rebuild();
        break;
    case KIT_CAT_READY:
    case KIT_CAT_FETCH_ERR:
    case KIT_CAT_OFFLINE:
        if (prev == KIT_CAT_FETCHING || prev == -1 || prev == KIT_CAT_WORK_OK ||
            prev == KIT_CAT_WORK_ERR) {
            if (!s_catalog_detail_screen) catalog_list_rebuild();
        }
        break;
    case KIT_CAT_FETCHING:
        if (!s_catalog_detail_screen) catalog_list_rebuild();
        break;
    default: break;
    }
}

static void open_catalog_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_sfx_impl(KIT_SFX_CLICK);
    if (s_catalog_screen) return;

    s_catalog_screen = make_overlay(KIT_COLOR_BG);
    s_catalog_last_state = -1;
    catalog_list_rebuild();

    kit_catalog_state_t st = kit_catalog_get_state();
    if (kit_network_is_connected() &&
        (st == KIT_CAT_IDLE || st == KIT_CAT_FETCH_ERR || st == KIT_CAT_OFFLINE)) {
        kit_catalog_refresh();
    }
    s_catalog_poll = lv_timer_create(catalog_poll_cb, 400, NULL);
}

static void close_catalog_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_sfx_impl(KIT_SFX_BACK);
    if (s_catalog_poll) { lv_timer_delete(s_catalog_poll); s_catalog_poll = NULL; }
    catalog_busy_hide();
    if (s_catalog_confirm_screen) { lv_obj_delete(s_catalog_confirm_screen); s_catalog_confirm_screen = NULL; }
    if (s_catalog_detail_screen)  { lv_obj_delete(s_catalog_detail_screen);  s_catalog_detail_screen = NULL; }
    if (s_catalog_screen)         { lv_obj_delete(s_catalog_screen);         s_catalog_screen = NULL; }

    // Instalou/removeu Tool com o Catálogo aberto? O deck da Home ficou pendente
    // (ver launcher_catalog_changed_impl) — reconstrói agora que a grade "VER
    // TODOS" volta a aparecer, senão a Tool nova/removida só aparece no próximo
    // go_home.
    if (s_home_deck_dirty && !home_is_covered()) {
        s_home_deck_dirty = false;
        home_clear_deck();
        home_build_deck();
    }
}

static void catalog_refresh_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_sfx_impl(KIT_SFX_CLICK);
    s_catalog_last_state = -1;
    kit_catalog_refresh();
    catalog_list_rebuild();
}

static void catalog_goto_wifi_cb(lv_event_t *e)
{
    close_catalog_cb(NULL);
    open_wifi_cb(NULL);
}

// -- Tela de detalhe ----------------------------------------------------
static bool catalog_find(const char *id, kit_catalog_entry_t *out)
{
    uint32_t n = kit_catalog_get_count();
    for (uint32_t i = 0; i < n; i++) {
        if (kit_catalog_get_entry(i, out) == KIT_OK && strcmp(out->id, id) == 0) return true;
    }
    return false;
}

static void catalog_detail_rebuild(void)
{
    if (!s_catalog_detail_screen) return;
    lv_obj_clean(s_catalog_detail_screen);

    kit_catalog_entry_t e;
    if (!catalog_find(s_catalog_sel_id, &e)) { catalog_detail_close_cb(NULL); return; }

    make_titlebar(s_catalog_detail_screen, "TOOL", catalog_detail_close_cb);

    bool installed = (e.install != KIT_CAT_NOT_INSTALLED);
    int reserve = KIT_BTN_H + 28 + (installed ? KIT_BTN_H + 12 : 0);
    lv_obj_t *body = make_scroll_body(s_catalog_detail_screen, reserve);

    lv_obj_t *nm = add_label(body, e.name, KIT_COLOR_TEXT, &kit_sans_28, 1);
    lv_obj_set_width(nm, KIT_CONTENT);
    lv_label_set_long_mode(nm, LV_LABEL_LONG_DOT);

    char meta[80];
    snprintf(meta, sizeof(meta), "%s%s\xC2\xB7 v%s",
             e.author[0] ? e.author : "", e.author[0] ? " " : "", e.version);
    lv_obj_t *mt = add_label(body, meta, KIT_COLOR_TEXT_MUTED, &kit_mono_16, 1);
    lv_obj_set_style_pad_bottom(mt, 6, 0);

    if (e.description[0]) {
        lv_obj_t *ds = add_label(body, e.description, KIT_COLOR_TEXT, &kit_sans_22, 0);
        lv_label_set_long_mode(ds, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(ds, KIT_CONTENT);
    }

    if (e.size) {
        char sz[32];
        snprintf(sz, sizeof(sz), "Download ~%lu KB", (unsigned long)((e.size + 512) / 1024));
        lv_obj_t *szl = add_label(body, sz, KIT_COLOR_TEXT_MUTED, &kit_mono_16, 1);
        lv_obj_set_style_pad_top(szl, 8, 0);
    }
    if (e.install == KIT_CAT_UPDATE) {
        lv_obj_t *u = add_label(body, "Atualiza\xC3\xA7\xC3\xA3o dispon\xC3\xADvel",
                                KIT_COLOR_GREEN, &kit_mono_16, 1);
        lv_obj_set_style_pad_top(u, 8, 0);
    }

    const char *act = (e.install == KIT_CAT_UPDATE) ? "ATUALIZAR"
                    : (e.install == KIT_CAT_INSTALLED) ? "REINSTALAR" : "INSTALAR";
    lv_obj_t *b = make_button(s_catalog_detail_screen, act, catalog_action_cb, true);
    lv_obj_align(b, LV_ALIGN_BOTTOM_MID, 0, installed ? -(14 + KIT_BTN_H + 12) : -14);

    if (installed) {
        lv_obj_t *rm = make_button(s_catalog_detail_screen, "REMOVER", catalog_remove_cb, false);
        lv_obj_set_style_border_color(rm, lv_color_hex(KIT_COLOR_RED), 0);
        lv_obj_t *rl = lv_obj_get_child(rm, 0);
        if (rl) lv_obj_set_style_text_color(rl, lv_color_hex(KIT_COLOR_RED), 0);
        lv_obj_align(rm, LV_ALIGN_BOTTOM_MID, 0, -14);
    }
}

static void catalog_row_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    kit_catalog_entry_t ent;
    if (kit_catalog_get_entry((uint32_t)i, &ent) != KIT_OK) return;
    kit_audio_sfx_impl(KIT_SFX_CLICK);
    strlcpy(s_catalog_sel_id, ent.id, sizeof(s_catalog_sel_id));
    if (s_catalog_detail_screen) return;
    s_catalog_detail_screen = make_overlay(KIT_COLOR_BG);
    catalog_detail_rebuild();
}

static void catalog_detail_close_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_sfx_impl(KIT_SFX_BACK);
    if (s_catalog_detail_screen) { lv_obj_delete(s_catalog_detail_screen); s_catalog_detail_screen = NULL; }
    catalog_list_rebuild();
}

static void catalog_action_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_sfx_impl(KIT_SFX_CLICK);
    s_catalog_last_state = -1;
    s_catalog_op_was_install = true;
    if (kit_catalog_install(s_catalog_sel_id) != KIT_OK) {
        show_toast("OCUPADO");
        return;
    }
    catalog_busy_show("BAIXANDO");
}

// -- Confirmação de remover -------------------------------------------
static void catalog_remove_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_sfx_impl(KIT_SFX_CLICK);
    if (s_catalog_confirm_screen) return;

    kit_catalog_entry_t ent;
    if (!catalog_find(s_catalog_sel_id, &ent)) return;

    s_catalog_confirm_screen = make_overlay(KIT_COLOR_BG);
    make_titlebar(s_catalog_confirm_screen, "REMOVER", catalog_confirm_close_cb);

    lv_obj_t *body = make_scroll_body(s_catalog_confirm_screen, 2 * KIT_BTN_H + 40);
    lv_obj_t *q = add_label(body,
        "Remover esta Tool do KIT? Os dados dela no cart\xC3\xA3o s\xC3\xA3o apagados.",
        KIT_COLOR_TEXT, &kit_sans_22, 0);
    lv_label_set_long_mode(q, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(q, KIT_CONTENT);
    lv_obj_t *nm = add_label(body, ent.name, KIT_COLOR_YELLOW, &kit_sans_28, 1);
    lv_obj_set_style_pad_top(nm, 6, 0);

    lv_obj_t *ok = make_button(s_catalog_confirm_screen, "REMOVER", catalog_confirm_do_cb, true);
    lv_obj_set_style_bg_color(ok, lv_color_hex(KIT_COLOR_RED), 0);
    lv_obj_t *okl = lv_obj_get_child(ok, 0);
    if (okl) lv_obj_set_style_text_color(okl, lv_color_hex(KIT_COLOR_ON_COLOR), 0);
    lv_obj_align(ok, LV_ALIGN_BOTTOM_MID, 0, -(14 + KIT_BTN_H + 12));

    lv_obj_t *no = make_button(s_catalog_confirm_screen, "CANCELAR", catalog_confirm_close_cb, false);
    lv_obj_align(no, LV_ALIGN_BOTTOM_MID, 0, -14);
}

static void catalog_confirm_close_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_sfx_impl(KIT_SFX_BACK);
    if (s_catalog_confirm_screen) { lv_obj_delete(s_catalog_confirm_screen); s_catalog_confirm_screen = NULL; }
}

static void catalog_confirm_do_cb(lv_event_t *e)
{
    (void)e;
    if (s_catalog_confirm_screen) { lv_obj_delete(s_catalog_confirm_screen); s_catalog_confirm_screen = NULL; }
    s_catalog_last_state = -1;
    s_catalog_op_was_install = false;
    if (kit_catalog_uninstall(s_catalog_sel_id) != KIT_OK) { show_toast("OCUPADO"); return; }
    catalog_busy_show("REMOVENDO");
}

// ---------------------------------------------------------------------------
// Ações
// ---------------------------------------------------------------------------

// Id da Tool a abrir no próximo ciclo do LVGL (ver launch_pending_tool_cb).
static char s_pending_tool_id[40];

// Abre a Tool pedida DEPOIS de liberar o slideshow da Home. O deck (tileview +
// slides com fontes grandes e ícones) ocupa uma fatia gorda do pool de 64 KB
// do LVGL; enquanto ele está montado, o tool_init de uma Tool pesada pode não
// achar memória e falhar ("não abre até reiniciar a placa"). Roda via
// lv_async_call porque home_tile_cb é o evento de um filho do próprio deck —
// apagá-lo ali dentro seria use-after-free. A Home reconstrói o deck ao voltar.
static void launch_pending_tool_cb(void *unused)
{
    (void)unused;
    if (!s_home_deck) return;   // deck já liberado: um launch já está em curso

    home_clear_deck();
    s_home_deck_dirty = true;

    if (kit_tool_manager_start(s_pending_tool_id) == KIT_OK) return;

    // Não abriu: reconstrói o deck aqui mesmo (ainda estamos na Home).
    kit_audio_beep_impl(400, 60);
    home_build_deck();
    s_home_deck_dirty = false;
    show_toast("NAO ABRIU");
}

static void home_tile_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i >= s_home_tools_n) return;
    const home_tool_t *tool = &s_home_tools[i];

    if (!tool->available) {
        kit_audio_beep_impl(500, 30);
        show_toast("EM BREVE");
        return;
    }

    kit_audio_sfx_impl(KIT_SFX_TOOL_OPEN);
    ESP_LOGI(TAG, "Abrindo Tool '%s'...", tool->id);
    if (s_toast) { lv_obj_delete(s_toast); s_toast = NULL; }
    home_mru_touch(i);   // sobe pro topo da recência (deck reconstrói ao voltar)
    snprintf(s_pending_tool_id, sizeof s_pending_tool_id, "%s", tool->id);
    lv_async_call(launch_pending_tool_cb, NULL);
}

static void run_test_tool_cb(lv_event_t *e)
{
    (void)e;
    kit_audio_beep_impl(2000, 50);
    // "Testes" agora vive dentro de Sobre; fecha os dois overlays antes de
    // entrar na Tool pra não deixar ponteiro solto pro go_home.
    if (s_about_screen)    { lv_obj_delete(s_about_screen);    s_about_screen = NULL; }
    if (s_settings_screen) { lv_obj_delete(s_settings_screen); s_settings_screen = NULL; }
    ESP_LOGI(TAG, "Iniciando Test Tool...");
    kit_tool_manager_start("com.kit.test");
}

// ---------------------------------------------------------------------------
// Navegação e polling da bateria
// ---------------------------------------------------------------------------

void kit_launcher_release_home_deck(void)
{
    if (s_home_deck) home_clear_deck();
    s_home_deck_dirty = true;   // kit_launcher_go_home reconstrói ao voltar
}

void kit_launcher_go_home(void)
{
    // Modo pen drive ativo: o cartão está no PC e o USB não é console —
    // a única saída coerente é reiniciar.
    if (kit_usb_msc_is_active()) { kit_usb_msc_exit_reboot(); return; }

    if (s_feedback_screen)   { lv_obj_delete(s_feedback_screen);   s_feedback_screen = NULL; }
    if (s_wifi_portal_poll)  { lv_timer_delete(s_wifi_portal_poll); s_wifi_portal_poll = NULL; }
    if (s_wifi_poll)         { lv_timer_delete(s_wifi_poll);        s_wifi_poll = NULL; }
    if (s_wifi_portal_screen) {
        if (kit_network_portal_is_active()) kit_network_portal_stop();
        kit_power_keep_awake_impl(false);
        lv_obj_delete(s_wifi_portal_screen); s_wifi_portal_screen = NULL;
        s_wifi_portal_status_lbl = NULL;
    }
    if (s_wifi_forget_screen) { lv_obj_delete(s_wifi_forget_screen); s_wifi_forget_screen = NULL; }
    if (s_wifi_screen) { lv_obj_delete(s_wifi_screen); s_wifi_screen = NULL;
                         s_wifi_status_lbl = NULL; s_wifi_toggle_lbl = NULL; }
    if (s_catalog_poll)   { lv_timer_delete(s_catalog_poll); s_catalog_poll = NULL; }
    if (s_catalog_busy_screen)    { lv_obj_delete(s_catalog_busy_screen);    s_catalog_busy_screen = NULL;
                                    kit_power_keep_awake_impl(false); }
    if (s_catalog_confirm_screen) { lv_obj_delete(s_catalog_confirm_screen); s_catalog_confirm_screen = NULL; }
    if (s_catalog_detail_screen)  { lv_obj_delete(s_catalog_detail_screen);  s_catalog_detail_screen = NULL; }
    if (s_catalog_screen)         { lv_obj_delete(s_catalog_screen);         s_catalog_screen = NULL; }
    if (s_usbmsc_screen)     { lv_obj_delete(s_usbmsc_screen);     s_usbmsc_screen = NULL; }
    if (s_about_screen)      { lv_obj_delete(s_about_screen);      s_about_screen = NULL; }
    if (s_sd_format_screen)  { lv_obj_delete(s_sd_format_screen);  s_sd_format_screen = NULL; }
    if (s_storage_screen)    { lv_obj_delete(s_storage_screen);    s_storage_screen = NULL; }
    if (s_sleep_screen)      { lv_obj_delete(s_sleep_screen);      s_sleep_screen = NULL; }
    if (s_poweroff_screen)   { lv_obj_delete(s_poweroff_screen);   s_poweroff_screen = NULL; }
    if (s_battery_screen)    { lv_obj_delete(s_battery_screen);    s_battery_screen = NULL; }
    if (s_brightness_screen) { lv_obj_delete(s_brightness_screen); s_brightness_screen = NULL;
                               s_brightness_val_lbl = NULL; }
    if (s_volume_screen)     { lv_obj_delete(s_volume_screen);     s_volume_screen = NULL;
                               s_volume_val_lbl = NULL; s_sound_val_lbl = NULL; }
    if (s_display_screen)    { lv_obj_delete(s_display_screen);    s_display_screen = NULL; }
    if (s_settings_screen)   { lv_obj_delete(s_settings_screen);   s_settings_screen = NULL; }
    if (s_onboarding_screen) { lv_obj_delete(s_onboarding_screen); s_onboarding_screen = NULL;
                               kit_config_set_u8("onboarded", 1); }  // saiu pelo BOTÃO físico: não repete no próximo boot
    if (s_splash_screen)     { lv_obj_delete(s_splash_screen);     s_splash_screen = NULL; }
    if (s_toast)             { lv_obj_delete(s_toast);             s_toast = NULL; }

    // Usou uma Tool? A ordem de recência mudou — reconstrói o slideshow.
    // Senão, só volta o slideshow para o primeiro slide.
    if (s_home_deck_dirty) {
        s_home_deck_dirty = false;
        home_clear_deck();
        // Remonta FORA daqui — depois que a Tool que está saindo for liberada
        // (ver home_build_deck_async_cb). A Home aparece ~1 frame sem o
        // slideshow; o deck entra no tick seguinte.
        lv_async_call(home_build_deck_async_cb, NULL);
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
    update_wifi_icon();

    bool charging = kit_power_is_charging();
    if (charging && !s_was_charging) {
        kit_audio_beep_impl(1600, 60);
        show_feedback(KIT_COLOR_GREEN, KIT_ICON_BOLT, "CARREGANDO");
    }
    s_was_charging = charging;

    // Aviso de bateria baixa: dispara uma vez quando cai a <= 20% descarregando.
    // Rearma só depois de carregar ou voltar acima de 25% (histerese).
    uint8_t p = kit_power_get_battery_percentage();
    if (charging || p > 25) {
        s_low_batt_warned = false;
    } else if (!s_low_batt_warned && p <= 20) {
        s_low_batt_warned = true;
        kit_audio_beep_impl(500, 90);
        show_feedback(KIT_COLOR_YELLOW, KIT_ICON_TRIANGLE, "BATERIA BAIXA");
    }
}

// ---------------------------------------------------------------------------

kit_err_t kit_launcher_init(void)
{
    ESP_LOGI(TAG, "Montando UI do Launcher (Brutalist Bauhaus) em LVGL v9...");

    s_launcher_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_launcher_screen, lv_color_hex(KIT_COLOR_BG), 0);
    lv_obj_clear_flag(s_launcher_screen, LV_OBJ_FLAG_SCROLLABLE);

    build_home(s_launcher_screen);
    kit_tool_manager_set_catalog_changed_cb(launcher_catalog_changed);
    update_battery();
    update_wifi_icon();
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
