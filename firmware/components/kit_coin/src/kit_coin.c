#include "kit_coin.h"
#include "kit_api.h"
#include "kit_display.h"
#include "kit_fonts.h"
#include "kit_theme.h"
#include "esp_log.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Coin Tool — linguagem "Brutalist Bauhaus" (ver docs/design/design-language.md).
// Titlebar fixa + tileview de 3 páginas (arrasta na horizontal):
//   0 AJUSTE     — modo (Moeda / Sim·Não), melhor-de, peso
//   1 RESULTADO  — CARA / COROA / SIM / NÃO em destaque (página inicial)
//   2 HISTÓRICO  — últimas jogadas
// Botão SORTEAR fixo no rodapé; sortear de qualquer página leva para a página 1.
// Também dispara por PWR (kit_coin_flip -> Runtime).
// A saída é feita pela API (system->exit).

static const char *TAG = "KIT_COIN";

// ---------------------------------------------------------------------------
// Layout (espelha as métricas da Dice Tool)
// ---------------------------------------------------------------------------
#define C_PAD        16
#define C_CONTENT    (KIT_DISPLAY_WIDTH - 2 * C_PAD)          // 336
#define C_TITLEBAR   88
#define C_FOOT       104
#define C_CHIP       56
#define C_STEP       56
#define C_FLIP_H     76
#define C_FLIP_MARGIN 18
#define C_PAGE_H     (KIT_DISPLAY_HEIGHT - C_TITLEBAR - C_FOOT)
#define C_TXT_OFFSET (((KIT_DISPLAY_HEIGHT - C_FLIP_MARGIN - C_FLIP_H - C_TITLEBAR) / 2) - (C_PAGE_H / 2))

#define FLIP_TICKS   8
#define FLIP_TICK_MS 60

#define HIST_MAX     10
#define PAGES        3

// ---------------------------------------------------------------------------
// Modos e opções
// ---------------------------------------------------------------------------
#define MODE_COIN    0
#define MODE_YESNO   1
#define MODE_CUSTOM  2
#define MODE_COUNT   3

static const int BESTOF_OPTS[] = { 1, 3, 5, 7 };
#define BESTOF_COUNT 4

#define WEIGHT_MIN   10
#define WEIGHT_MAX   90
#define WEIGHT_STEP  5

// ---------------------------------------------------------------------------
// Estado
// ---------------------------------------------------------------------------
static int      s_mode     = MODE_COIN;
static int      s_best_of  = 1;
static int      s_weight   = 50;       // % para opção A (CARA / SIM)
static bool     s_flipping = false;
static uint32_t s_accent   = KIT_COLOR_YELLOW;
static int      s_result   = -1;       // -1 = sem resultado; 0 = A; 1 = B
static int      s_final_result = 0;
static char     s_custom_a[4] = "A  ";
static char     s_custom_b[4] = "B  ";

// Série (Melhor De)
static int  s_score_a     = 0;         // CARA / SIM
static int  s_score_b     = 0;         // COROA / NÃO
static bool s_series_done = false;

static int  s_flip_tick   = 0;
static lv_timer_t *s_flip_timer = NULL;

// Histórico
static int  s_hist_result[HIST_MAX];
static int  s_hist_mode[HIST_MAX];
static int  s_hist_n = 0;

// ---------------------------------------------------------------------------
// Objetos LVGL
// ---------------------------------------------------------------------------
static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_tv     = NULL;
static lv_obj_t *s_tiles[PAGES];
static lv_obj_t *s_dots[PAGES];

// Página 0 — Ajuste
static lv_obj_t *s_mode_chips[MODE_COUNT];
static lv_obj_t *s_mode_chip_lbls[MODE_COUNT];
static lv_obj_t *s_custom_cnt   = NULL;
static lv_obj_t *s_rollers_a[3];
static lv_obj_t *s_rollers_b[3];
static lv_obj_t *s_bestof_chips[BESTOF_COUNT];
static lv_obj_t *s_bestof_chip_lbls[BESTOF_COUNT];
static lv_obj_t *s_weight_lbl   = NULL;
static lv_obj_t *s_weight_minus = NULL;
static lv_obj_t *s_weight_plus  = NULL;

// Página 1 — Resultado
static lv_obj_t *s_notation_lbl = NULL;
static lv_obj_t *s_score_lbl    = NULL;
static lv_obj_t *s_result_lbl   = NULL;
static lv_obj_t *s_winner_lbl   = NULL;
static lv_obj_t *s_hint_lbl     = NULL;
static lv_obj_t *s_flip_btn     = NULL;

// Página 2 — Histórico
static lv_obj_t *s_history_rows  = NULL;
static lv_obj_t *s_history_empty = NULL;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const kit_api_table_t *api(void) { return kit_api_get_table(); }

static int flip_one(void)
{
    const kit_api_table_t *t = api();
    int roll;
    if (t && t->random) roll = (int)t->random->range(1, 100);
    else                roll = 1 + (rand() % 100);
    return (roll <= s_weight) ? 0 : 1;
}

static const char *result_text(int r)
{
    if (s_mode == MODE_COIN)  return r == 0 ? "CARA"  : "COROA";
    if (s_mode == MODE_YESNO) return r == 0 ? "SIM"   : "N\xC3\x83O";
    return r == 0 ? s_custom_a : s_custom_b;
}

static const char *result_text_for(int r, int mode)
{
    if (mode == MODE_COIN)  return r == 0 ? "CARA"  : "COROA";
    if (mode == MODE_YESNO) return r == 0 ? "SIM"   : "N\xC3\x83O";
    return r == 0 ? s_custom_a : s_custom_b;
}

static uint32_t on_accent(void)
{
    return (s_accent == KIT_COLOR_YELLOW) ? KIT_COLOR_ON_YELLOW : KIT_COLOR_ON_COLOR;
}

static lv_obj_t *add_label(lv_obj_t *parent, const char *txt, uint32_t color,
                           const lv_font_t *font, int letter_space)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(l, font, 0);
    if (letter_space) lv_obj_set_style_text_letter_space(l, letter_space, 0);
    return l;
}

static lv_obj_t *plain_box(lv_obj_t *parent)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static lv_obj_t *field_label(lv_obj_t *parent, const char *txt)
{
    return add_label(parent, txt, KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);
}

static void build_notation(char *buf, size_t n)
{
    const char *m = (s_mode == MODE_COIN) ? "MOEDA" : 
                    (s_mode == MODE_YESNO) ? "SIM/N\xC3\x83O" : "CUSTOM";
    snprintf(buf, n, "%s \xC2\xB7 %d/%d", m, s_weight, 100 - s_weight);
}

// ---------------------------------------------------------------------------
// Persistência (Storage API)
// ---------------------------------------------------------------------------

static void load_prefs(void)
{
    const kit_api_table_t *t = api();
    if (!t || !t->storage) return;
    int32_t v;
    if (t->storage->get_i32("coin_mode", &v) == KIT_OK && v >= 0 && v < MODE_COUNT)
        s_mode = (int)v;
    if (t->storage->get_i32("coin_bestof", &v) == KIT_OK) {
        for (int i = 0; i < BESTOF_COUNT; i++)
            if (BESTOF_OPTS[i] == v) s_best_of = (int)v;
    }
    if (t->storage->get_i32("coin_weight", &v) == KIT_OK
        && v >= WEIGHT_MIN && v <= WEIGHT_MAX)
        s_weight = (int)v;
        
    if (t->storage->get_str("coin_cust_a", s_custom_a, 4) != KIT_OK) strcpy(s_custom_a, "A  ");
    if (t->storage->get_str("coin_cust_b", s_custom_b, 4) != KIT_OK) strcpy(s_custom_b, "B  ");
}

static void save_prefs(void)
{
    const kit_api_table_t *t = api();
    if (!t || !t->storage) return;
    t->storage->set_i32("coin_mode",   s_mode);
    t->storage->set_i32("coin_bestof", s_best_of);
    t->storage->set_i32("coin_weight", s_weight);
    t->storage->set_str("coin_cust_a", s_custom_a);
    t->storage->set_str("coin_cust_b", s_custom_b);
}

// ---------------------------------------------------------------------------
// Sincronização de UI
// ---------------------------------------------------------------------------

static void sync_dots(void)
{
    lv_obj_t *act = s_tv ? lv_tileview_get_tile_active(s_tv) : NULL;
    for (int i = 0; i < PAGES; i++) {
        bool on = (act == s_tiles[i]);
        lv_obj_set_style_bg_color(s_dots[i],
            lv_color_hex(on ? s_accent : KIT_COLOR_LINE), 0);
        lv_obj_set_size(s_dots[i], on ? 20 : 8, 8);
    }
}

static void sync_mode_chips(void)
{
    uint32_t sel_txt = on_accent();
    for (int i = 0; i < MODE_COUNT; i++) {
        bool sel = (i == s_mode);
        lv_obj_set_style_bg_color(s_mode_chips[i],
            lv_color_hex(sel ? s_accent : KIT_COLOR_SURFACE), 0);
        lv_obj_set_style_text_color(s_mode_chip_lbls[i],
            lv_color_hex(sel ? sel_txt : KIT_COLOR_TEXT), 0);
    }
    if (s_custom_cnt) {
        if (s_mode == MODE_CUSTOM) lv_obj_clear_flag(s_custom_cnt, LV_OBJ_FLAG_HIDDEN);
        else                       lv_obj_add_flag(s_custom_cnt, LV_OBJ_FLAG_HIDDEN);
    }
}

static void sync_bestof_chips(void)
{
    uint32_t sel_txt = on_accent();
    for (int i = 0; i < BESTOF_COUNT; i++) {
        bool sel = (BESTOF_OPTS[i] == s_best_of);
        lv_obj_set_style_bg_color(s_bestof_chips[i],
            lv_color_hex(sel ? s_accent : KIT_COLOR_SURFACE), 0);
        lv_obj_set_style_text_color(s_bestof_chip_lbls[i],
            lv_color_hex(sel ? sel_txt : KIT_COLOR_TEXT), 0);
    }
}

static void dim_step(lv_obj_t *btn, bool disabled)
{
    lv_obj_set_style_opa(btn, disabled ? LV_OPA_30 : LV_OPA_COVER, 0);
}

static void sync_weight(void)
{
    lv_label_set_text_fmt(s_weight_lbl, "%d/%d", s_weight, 100 - s_weight);
    dim_step(s_weight_minus, s_weight <= WEIGHT_MIN);
    dim_step(s_weight_plus,  s_weight >= WEIGHT_MAX);
}

static void sync_score(void)
{
    if (s_best_of <= 1) {
        lv_obj_add_flag(s_score_lbl, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(s_score_lbl, LV_OBJ_FLAG_HIDDEN);
    
    const char *opt_a = (s_mode == MODE_COIN) ? "CARA" : (s_mode == MODE_YESNO) ? "SIM" : s_custom_a;
    const char *opt_b = (s_mode == MODE_COIN) ? "COROA" : (s_mode == MODE_YESNO) ? "N\xC3\x83O" : s_custom_b;
    
    lv_label_set_text_fmt(s_score_lbl, "%s %d x %d %s", opt_a, s_score_a, s_score_b, opt_b);
}

static void align_result_meta(void)
{
    if (s_best_of > 1) {
        lv_obj_align_to(s_score_lbl,    s_result_lbl, LV_ALIGN_OUT_TOP_MID, 0, -10);
        lv_obj_align_to(s_notation_lbl, s_score_lbl,  LV_ALIGN_OUT_TOP_MID, 0, -6);
    } else {
        lv_obj_align_to(s_notation_lbl, s_result_lbl, LV_ALIGN_OUT_TOP_MID, 0, -14);
    }
    lv_obj_align_to(s_hint_lbl,   s_result_lbl, LV_ALIGN_OUT_BOTTOM_MID, 0, 14);
    lv_obj_align_to(s_winner_lbl, s_result_lbl, LV_ALIGN_OUT_BOTTOM_MID, 0, 14);
}

// Página 1 volta ao estado "pronto para sortear".
static void reset_result(void)
{
    if (s_flipping) return;

    s_score_a = 0;
    s_score_b = 0;
    s_series_done = false;
    s_result = -1;

    char buf[32];
    build_notation(buf, sizeof(buf));
    lv_label_set_text(s_notation_lbl, buf);

    lv_label_set_text(s_result_lbl, "-");
    lv_obj_set_style_text_color(s_result_lbl, lv_color_hex(KIT_COLOR_TEXT_MUTED), 0);

    lv_obj_clear_flag(s_hint_lbl, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_winner_lbl, LV_OBJ_FLAG_HIDDEN);

    sync_score();
    align_result_meta();
}

// ---------------------------------------------------------------------------
// Histórico
// ---------------------------------------------------------------------------

static void push_history(int result)
{
    for (int i = HIST_MAX - 1; i > 0; i--) {
        s_hist_result[i] = s_hist_result[i - 1];
        s_hist_mode[i]   = s_hist_mode[i - 1];
    }
    s_hist_result[0] = result;
    s_hist_mode[0]   = s_mode;
    if (s_hist_n < HIST_MAX) s_hist_n++;

    lv_obj_add_flag(s_history_empty, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clean(s_history_rows);
    for (int i = 0; i < s_hist_n; i++) {
        lv_obj_t *row = plain_box(s_history_rows);
        lv_obj_set_size(row, lv_pct(100), 58);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(KIT_COLOR_LINE), 0);

        char num[8];
        snprintf(num, sizeof(num), "#%d", s_hist_n - i);
        lv_obj_t *k = add_label(row, num, KIT_COLOR_TEXT_MUTED, &kit_mono_16, 1);
        lv_obj_align(k, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t *v = add_label(row, result_text_for(s_hist_result[i], s_hist_mode[i]),
                                i == 0 ? s_accent : KIT_COLOR_TEXT, &kit_mono_26, 0);
        lv_obj_align(v, LV_ALIGN_RIGHT_MID, 0, 0);
    }
}

// ---------------------------------------------------------------------------
// Flip (animação de sorteio)
// ---------------------------------------------------------------------------

static void flip_tick_cb(lv_timer_t *t);
static void align_result_meta(void);

static void do_flip(void)
{
    if (s_flipping) return;

    // Série completada → reseta para nova série
    if (s_series_done) {
        s_score_a = 0;
        s_score_b = 0;
        s_series_done = false;
        lv_obj_add_flag(s_winner_lbl, LV_OBJ_FLAG_HIDDEN);
    }

    s_flipping = true;
    s_final_result = flip_one();

    lv_obj_add_flag(s_hint_lbl, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_winner_lbl, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_color(s_result_lbl, lv_color_hex(s_accent), 0);

    // Sortear de qualquer página leva para a página do resultado
    lv_tileview_set_tile_by_index(s_tv, 1, 0, LV_ANIM_OFF);

    s_flip_tick = 0;
    s_flip_timer = lv_timer_create(flip_tick_cb, FLIP_TICK_MS, NULL);
}

// Animação leve: durante o "flip" alterna CARA/COROA (ou SIM/NÃO).
static void flip_tick_cb(lv_timer_t *t)
{
    (void)t;
    s_flip_tick++;

    if (s_flip_tick < FLIP_TICKS) {
        lv_label_set_text(s_result_lbl, result_text(s_flip_tick % 2));
        return;
    }

    // Resultado final
    lv_label_set_text(s_result_lbl, result_text(s_final_result));
    lv_obj_set_style_text_color(s_result_lbl, lv_color_hex(KIT_COLOR_TEXT), 0);

    if (s_flip_timer) { lv_timer_delete(s_flip_timer); s_flip_timer = NULL; }
    s_flipping = false;
    s_result = s_final_result;

    // Atualiza placar da série
    if (s_final_result == 0) s_score_a++;
    else                     s_score_b++;

    // Verifica vencedor (Melhor De)
    if (s_best_of > 1) {
        int need = (s_best_of + 1) / 2;
        if (s_score_a >= need || s_score_b >= need) {
            s_series_done = true;
            lv_obj_clear_flag(s_winner_lbl, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_text_color(s_result_lbl, lv_color_hex(s_accent), 0);
        }
    }

    sync_score();
    align_result_meta();
    push_history(s_final_result);
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------

static void back_cb(lv_event_t *e)
{
    (void)e;
    const kit_api_table_t *t = api();
    if (t && t->system) t->system->exit();
}

static void tv_changed_cb(lv_event_t *e)
{
    (void)e;
    sync_dots();
}

static void mode_chip_cb(lv_event_t *e)
{
    if (s_flipping) return;
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i >= MODE_COUNT) return;
    s_mode = i;
    sync_mode_chips();
    reset_result();
    save_prefs();
}

static void bestof_chip_cb(lv_event_t *e)
{
    if (s_flipping) return;
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i >= BESTOF_COUNT) return;
    s_best_of = BESTOF_OPTS[i];
    sync_bestof_chips();
    reset_result();
    save_prefs();
}

static void weight_step_cb(lv_event_t *e)
{
    if (s_flipping) return;
    int code = (int)(intptr_t)lv_event_get_user_data(e);
    switch (code) {
        case 0: if (s_weight > WEIGHT_MIN) s_weight -= WEIGHT_STEP; break;
        case 1: if (s_weight < WEIGHT_MAX) s_weight += WEIGHT_STEP; break;
    }
    sync_weight();
    reset_result();
    save_prefs();
}

static void flip_cb(lv_event_t *e)
{
    (void)e;
    do_flip();
}

void kit_coin_flip(void)
{
    if (!s_screen) return;
    do_flip();
}

// ---------------------------------------------------------------------------
// Construção da tela
// ---------------------------------------------------------------------------

static void build_titlebar(void)
{
    lv_obj_t *chip = lv_obj_create(s_screen);
    lv_obj_set_size(chip, C_CHIP, C_CHIP);
    lv_obj_set_style_bg_color(chip, lv_color_hex(KIT_COLOR_SURFACE), 0);
    lv_obj_set_style_border_width(chip, 0, 0);
    lv_obj_set_style_radius(chip, 18, 0);
    lv_obj_set_style_pad_all(chip, 0, 0);
    lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(chip, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(chip, 12);
    lv_obj_add_event_cb(chip, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(chip, LV_ALIGN_TOP_LEFT, C_PAD, 16);

    lv_obj_t *g = add_label(chip, KIT_ICON_BACK, KIT_COLOR_TEXT, &kit_display_44, 0);
    lv_obj_center(g);

    lv_obj_t *title = add_label(s_screen, "MOEDA", KIT_COLOR_TEXT, &kit_mono_26, 3);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, C_PAD + C_CHIP + 12, 30);

    // Indicador de página (3 pontos)
    lv_obj_t *dots = plain_box(s_screen);
    lv_obj_set_size(dots, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(dots, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dots, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(dots, 6, 0);
    lv_obj_align(dots, LV_ALIGN_TOP_RIGHT, -C_PAD, 40);
    for (int i = 0; i < PAGES; i++) {
        lv_obj_t *d = lv_obj_create(dots);
        lv_obj_remove_style_all(d);
        lv_obj_set_size(d, 8, 8);
        lv_obj_set_style_radius(d, 4, 0);
        lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(d, lv_color_hex(KIT_COLOR_LINE), 0);
        s_dots[i] = d;
    }
}

static lv_obj_t *page_scroll(lv_obj_t *tile)
{
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_t *p = lv_obj_create(tile);
    lv_obj_remove_style_all(p);
    lv_obj_set_size(p, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_left(p, C_PAD, 0);
    lv_obj_set_style_pad_right(p, C_PAD, 0);
    lv_obj_set_style_pad_top(p, 10, 0);
    lv_obj_set_style_pad_bottom(p, 36, 0);
    lv_obj_set_style_pad_row(p, 14, 0);
    lv_obj_set_flex_flow(p, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(p, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(p, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(p, LV_SCROLLBAR_MODE_AUTO);
    return p;
}

static lv_obj_t *make_step_btn(lv_obj_t *parent, const char *sym,
                                lv_event_cb_t cb, int code)
{
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_set_size(b, 68, 68);
    lv_obj_set_style_bg_color(b, lv_color_hex(KIT_COLOR_SURFACE), 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_radius(b, 20, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(b, 16);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void *)(intptr_t)code);
    lv_obj_t *l = add_label(b, sym, KIT_COLOR_TEXT, &kit_display_44, 0);
    lv_obj_center(l);
    return b;
}

static void roller_event_cb(lv_event_t *e)
{
    lv_obj_t *r = lv_event_get_target(e);
    int id = (int)(intptr_t)lv_event_get_user_data(e);
    char buf[8];
    lv_roller_get_selected_str(r, buf, sizeof(buf));
    char c = buf[0];
    
    if (id < 3) s_custom_a[id] = c;
    else        s_custom_b[id - 3] = c;
    
    save_prefs();
    reset_result();
    sync_score();
}

// Página 0 — AJUSTE
static void build_page_setup(lv_obj_t *tile)
{
    lv_obj_t *p = page_scroll(tile);

    // -------- MODO --------
    lv_obj_t *sec_mode = plain_box(p);
    lv_obj_set_size(sec_mode, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(sec_mode, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(sec_mode, 9, 0);
    field_label(sec_mode, "MODO");

    lv_obj_t *mode_row = plain_box(sec_mode);
    lv_obj_set_size(mode_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(mode_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(mode_row, 8, 0);

    static const char *MODE_LABELS[] = { "MOEDA", "SIM/N\xC3\x83O", "CUSTOM" };
    for (int i = 0; i < MODE_COUNT; i++) {
        lv_obj_t *c = lv_obj_create(mode_row);
        lv_obj_set_height(c, 54);
        lv_obj_set_flex_grow(c, 1);
        lv_obj_set_style_bg_color(c, lv_color_hex(KIT_COLOR_SURFACE), 0);
        lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(c, 0, 0);
        lv_obj_set_style_radius(c, 15, 0);
        lv_obj_set_style_pad_all(c, 0, 0);
        lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(c, 4);
        lv_obj_add_event_cb(c, mode_chip_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        lv_obj_t *l = add_label(c, MODE_LABELS[i], KIT_COLOR_TEXT, &kit_mono_16, 1);
        lv_obj_center(l);

        s_mode_chips[i]     = c;
        s_mode_chip_lbls[i] = l;
    }

    // -------- CUSTOM SETUP --------
    s_custom_cnt = plain_box(p);
    lv_obj_set_size(s_custom_cnt, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_custom_cnt, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_custom_cnt, 9, 0);
    lv_obj_set_style_pad_top(s_custom_cnt, 4, 0);
    
    const char *opts = " \nA\nB\nC\nD\nE\nF\nG\nH\nI\nJ\nK\nL\nM\nN\nO\nP\nQ\nR\nS\nT\nU\nV\nW\nX\nY\nZ";
    
    for (int opt = 0; opt < 2; opt++) {
        field_label(s_custom_cnt, opt == 0 ? "OP\xC3\x87\xC3\x83O 1" : "OP\xC3\x87\xC3\x83O 2");
        lv_obj_t *row = plain_box(s_custom_cnt);
        lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(row, 8, 0);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        
        for (int i = 0; i < 3; i++) {
            lv_obj_t *r = lv_roller_create(row);
            lv_roller_set_options(r, opts, LV_ROLLER_MODE_INFINITE);
            lv_roller_set_visible_row_count(r, 3);
            lv_obj_set_width(r, 94);
            lv_obj_set_style_bg_color(r, lv_color_hex(KIT_COLOR_SURFACE), 0);
            lv_obj_set_style_bg_color(r, lv_color_hex(s_accent), LV_PART_SELECTED);
            lv_obj_set_style_text_color(r, lv_color_hex(KIT_COLOR_TEXT), 0);
            lv_obj_set_style_text_color(r, lv_color_hex(on_accent()), LV_PART_SELECTED);
            lv_obj_set_style_text_font(r, &kit_mono_26, 0);
            lv_obj_set_style_border_width(r, 0, 0);
            lv_obj_set_style_pad_all(r, 0, 0);
            
            int id = opt * 3 + i;
            char c = (opt == 0) ? s_custom_a[i] : s_custom_b[i];
            int sel = (c == ' ') ? 0 : (c - 'A' + 1);
            lv_roller_set_selected(r, sel, LV_ANIM_OFF);
            lv_obj_add_event_cb(r, roller_event_cb, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)id);
            
            if (opt == 0) s_rollers_a[i] = r;
            else          s_rollers_b[i] = r;
        }
    }

    // -------- MELHOR DE --------
    lv_obj_t *sec_bo = plain_box(p);
    lv_obj_set_size(sec_bo, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(sec_bo, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(sec_bo, 9, 0);
    field_label(sec_bo, "MELHOR DE");

    lv_obj_t *bo_row = plain_box(sec_bo);
    lv_obj_set_size(bo_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bo_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(bo_row, 4, 0);

    static const char *BO_LABELS[] = { "1", "3", "5", "7" };
    for (int i = 0; i < BESTOF_COUNT; i++) {
        lv_obj_t *c = lv_obj_create(bo_row);
        lv_obj_set_size(c, 78, 54);
        lv_obj_set_style_bg_color(c, lv_color_hex(KIT_COLOR_SURFACE), 0);
        lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(c, 0, 0);
        lv_obj_set_style_radius(c, 15, 0);
        lv_obj_set_style_pad_all(c, 0, 0);
        lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(c, 4);
        lv_obj_add_event_cb(c, bestof_chip_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        lv_obj_t *l = add_label(c, BO_LABELS[i], KIT_COLOR_TEXT, &kit_mono_26, 1);
        lv_obj_center(l);

        s_bestof_chips[i]     = c;
        s_bestof_chip_lbls[i] = l;
    }

    // -------- PESO --------
    lv_obj_t *weight_row = plain_box(p);
    lv_obj_set_size(weight_row, lv_pct(100), 68);
    lv_obj_set_flex_flow(weight_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(weight_row, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(weight_row, 8, 0);

    lv_obj_t *wlbl = add_label(weight_row, "PESO", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 1);
    lv_obj_set_flex_grow(wlbl, 1);

    s_weight_minus = make_step_btn(weight_row, "-", weight_step_cb, 0);

    s_weight_lbl = add_label(weight_row, "50/50", KIT_COLOR_TEXT, &kit_mono_26, 0);
    lv_obj_set_width(s_weight_lbl, 96);
    lv_obj_set_style_text_align(s_weight_lbl, LV_TEXT_ALIGN_CENTER, 0);

    s_weight_plus = make_step_btn(weight_row, "+", weight_step_cb, 1);
}

// Página 1 — RESULTADO
static void build_page_result(lv_obj_t *tile)
{
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_t *box = lv_obj_create(tile);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    // Resultado grande (kit_display_72: A-Z Ã Ç Õ)
    s_result_lbl = add_label(box, "-", KIT_COLOR_TEXT_MUTED, &kit_display_72, 0);
    lv_obj_set_width(s_result_lbl, C_CONTENT);
    lv_obj_set_style_text_align(s_result_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_result_lbl, LV_ALIGN_CENTER, 0, C_TXT_OFFSET);

    // Notação (modo · peso)
    s_notation_lbl = add_label(box, "MOEDA \xC2\xB7 50/50",
                               KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);
    lv_obj_set_width(s_notation_lbl, C_CONTENT);
    lv_obj_set_style_text_align(s_notation_lbl, LV_TEXT_ALIGN_CENTER, 0);

    // Placar da série (visível apenas em Melhor De > 1)
    s_score_lbl = add_label(box, "0 x 0", KIT_COLOR_TEXT, &kit_mono_20, 1);
    lv_obj_set_width(s_score_lbl, C_CONTENT);
    lv_obj_set_style_text_align(s_score_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(s_score_lbl, LV_OBJ_FLAG_HIDDEN);

    // Dica (antes de sortear)
    s_hint_lbl = add_label(box, "SORTEAR \xC2\xB7 PWR",
                           KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);
    lv_obj_set_width(s_hint_lbl, C_CONTENT);
    lv_obj_set_style_text_align(s_hint_lbl, LV_TEXT_ALIGN_CENTER, 0);

    // Banner de vitória (oculto por padrão)
    s_winner_lbl = add_label(box, "VENCEU!", s_accent, &kit_mono_26, 3);
    lv_obj_set_width(s_winner_lbl, C_CONTENT);
    lv_obj_set_style_text_align(s_winner_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(s_winner_lbl, LV_OBJ_FLAG_HIDDEN);
}

// Página 2 — HISTÓRICO
static void build_page_history(lv_obj_t *tile)
{
    lv_obj_t *p = page_scroll(tile);

    field_label(p, "HIST\xC3\x93RICO");

    s_history_empty = add_label(p, "AINDA SEM SORTEIOS",
                                KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);

    s_history_rows = plain_box(p);
    lv_obj_set_size(s_history_rows, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_history_rows, LV_FLEX_FLOW_COLUMN);
}

static void build_tileview(void)
{
    s_tv = lv_tileview_create(s_screen);
    lv_obj_set_size(s_tv, KIT_DISPLAY_WIDTH, C_PAGE_H);
    lv_obj_set_pos(s_tv, 0, C_TITLEBAR);
    lv_obj_set_style_bg_opa(s_tv, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_tv, 0, 0);
    lv_obj_set_scrollbar_mode(s_tv, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(s_tv, tv_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_tiles[0] = lv_tileview_add_tile(s_tv, 0, 0, LV_DIR_HOR);
    s_tiles[1] = lv_tileview_add_tile(s_tv, 1, 0, LV_DIR_HOR);
    s_tiles[2] = lv_tileview_add_tile(s_tv, 2, 0, LV_DIR_HOR);

    build_page_setup(s_tiles[0]);
    build_page_result(s_tiles[1]);
    build_page_history(s_tiles[2]);
}

static void build_footer(void)
{
    s_flip_btn = lv_obj_create(s_screen);
    lv_obj_set_size(s_flip_btn, C_CONTENT, C_FLIP_H);
    lv_obj_set_style_radius(s_flip_btn, C_FLIP_H / 2, 0);
    lv_obj_set_style_border_width(s_flip_btn, 0, 0);
    lv_obj_set_style_shadow_width(s_flip_btn, 0, 0);
    lv_obj_set_style_pad_all(s_flip_btn, 0, 0);
    lv_obj_set_style_bg_color(s_flip_btn, lv_color_hex(s_accent), 0);
    lv_obj_set_style_bg_opa(s_flip_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(s_flip_btn, LV_OPA_80, LV_STATE_PRESSED);
    lv_obj_clear_flag(s_flip_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_flip_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_flip_btn, 8);
    lv_obj_align(s_flip_btn, LV_ALIGN_BOTTOM_MID, 0, -18);
    lv_obj_add_event_cb(s_flip_btn, flip_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *l = add_label(s_flip_btn, "SORTEAR", on_accent(), &kit_mono_26, 3);
    lv_obj_center(l);
}

// ---------------------------------------------------------------------------
// Ciclo de vida
// ---------------------------------------------------------------------------

kit_err_t kit_coin_start(uint32_t accent)
{
    if (s_screen) kit_coin_destroy();

    ESP_LOGI(TAG, "Montando Coin Tool...");
    s_accent   = accent ? accent : KIT_COLOR_YELLOW;
    s_mode     = MODE_COIN;
    s_best_of  = 1;
    s_weight   = 50;
    s_flipping = false;
    s_result   = -1;
    s_score_a  = 0;
    s_score_b  = 0;
    s_series_done = false;
    s_hist_n   = 0;
    load_prefs();

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(KIT_COLOR_BG), 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    build_titlebar();
    build_tileview();
    build_footer();

    // Começa na página do resultado (a principal)
    lv_tileview_set_tile_by_index(s_tv, 1, 0, LV_ANIM_OFF);

    lv_obj_update_layout(s_screen);
    align_result_meta();

    sync_mode_chips();
    sync_bestof_chips();
    sync_weight();
    sync_score();
    reset_result();
    sync_dots();

    lv_screen_load(s_screen);
    return KIT_OK;
}

void kit_coin_destroy(void)
{
    ESP_LOGI(TAG, "Encerrando Coin Tool.");
    if (s_flip_timer) { lv_timer_delete(s_flip_timer); s_flip_timer = NULL; }
    s_flipping = false;

    if (s_screen) {
        lv_obj_delete(s_screen);
        s_screen = NULL;
    }
    s_tv = NULL;
    s_notation_lbl = s_score_lbl = s_result_lbl = NULL;
    s_winner_lbl = s_hint_lbl = s_flip_btn = NULL;
    s_history_rows = s_history_empty = NULL;
    s_weight_lbl = s_weight_minus = s_weight_plus = NULL;
}
