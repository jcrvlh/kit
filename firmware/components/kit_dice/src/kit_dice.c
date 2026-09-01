#include "kit_dice.h"
#include "kit_api.h"
#include "kit_display.h"
#include "kit_fonts.h"
#include "kit_theme.h"
#include "esp_log.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Dice Tool — linguagem "Brutalist Bauhaus" (ver docs/design/design-language.md).
// Titlebar fixa + tileview de 3 páginas (arrasta na horizontal):
//   0 AJUSTE     — seletor de dado + QUANTIDADE + MODIFICADOR
//   1 RESULTADO  — o número sorteado em destaque (página inicial)
//   2 HISTÓRICO  — últimas rolagens
// Botão ROLAR fixo no rodapé; rolar de qualquer página leva para a página 1.
// Também dispara por PWR e por chacoalhar (kit_dice_roll -> Runtime).
// A saída é feita pela API (system->exit).

static const char *TAG = "KIT_DICE";

#define D_PAD        16
#define D_CONTENT    (KIT_DISPLAY_WIDTH - 2 * D_PAD)          // 336
#define D_TITLEBAR   88
#define D_FOOT       104
#define D_CHIP       56
#define D_STEP       56
#define D_ROLL_H     76
#define D_ROLL_MARGIN 18                                          // botão -> base da tela
#define D_PAGE_H     (KIT_DISPLAY_HEIGHT - D_TITLEBAR - D_FOOT)   // 256
// deslocamento p/ o número ficar no centro EXATO entre a base da titlebar e o
// topo do botão ROLAR (não no centro do tile): (região/2) - (tile/2)
#define D_NUM_OFFSET (((KIT_DISPLAY_HEIGHT - D_ROLL_MARGIN - D_ROLL_H - D_TITLEBAR) / 2) - (D_PAGE_H / 2))

#define ROLL_TICKS   6
#define ROLL_TICK_MS 70

#define COUNT_MIN    1
#define COUNT_MAX    5
#define MOD_LIMIT    10
#define HIST_MAX     6
#define PAGES        3

static const int DICE_FACES[] = { 4, 6, 8, 10, 12, 20, 100 };
#define DICE_KINDS ((int)(sizeof(DICE_FACES) / sizeof(DICE_FACES[0])))

// --- estado ---------------------------------------------------------------
static int      s_die   = 6;
static int      s_count = 1;
static int      s_mod   = 0;
static bool     s_rolling = false;
static uint32_t s_accent = KIT_COLOR_RED;

static int  s_final_vals[COUNT_MAX];
static int  s_final_total = 0;
static int  s_roll_tick = 0;
static lv_timer_t *s_roll_timer = NULL;

static char s_hist[HIST_MAX][40];
static int  s_hist_n = 0;

// --- objetos LVGL --------------------------------------------------------
static lv_obj_t *s_screen        = NULL;
static lv_obj_t *s_tv             = NULL;
static lv_obj_t *s_tiles[PAGES];
static lv_obj_t *s_dots[PAGES];
static lv_obj_t *s_die_chips[DICE_KINDS];
static lv_obj_t *s_die_chip_lbls[DICE_KINDS];
static lv_obj_t *s_count_lbl     = NULL;
static lv_obj_t *s_mod_lbl       = NULL;
static lv_obj_t *s_count_minus   = NULL;
static lv_obj_t *s_count_plus    = NULL;
static lv_obj_t *s_mod_minus     = NULL;
static lv_obj_t *s_mod_plus      = NULL;
static lv_obj_t *s_notation_lbl  = NULL;
static lv_obj_t *s_hint_lbl      = NULL;
static lv_obj_t *s_faces_box     = NULL;
static lv_obj_t *s_total_lbl     = NULL;
static lv_obj_t *s_history_rows  = NULL;
static lv_obj_t *s_history_empty = NULL;
static lv_obj_t *s_roll_btn      = NULL;

static lv_obj_t *s_face_lbls[COUNT_MAX];
static int       s_face_built = 0;

// --- helpers ------------------------------------------------------------

static const kit_api_table_t *api(void) { return kit_api_get_table(); }

// Sem áudio nesta Tool: kit_audio->beep é síncrono (bloqueia a task do LVGL) e
// travava a rolagem. Reavaliar quando o áudio tiver um caminho assíncrono.

static int roll_one(int faces)
{
    const kit_api_table_t *t = api();
    if (t && t->random) return (int)t->random->range(1, faces);
    return 1 + (rand() % faces);
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
    int len = snprintf(buf, n, "%dD%d", s_count, s_die);
    if (len < 0 || (size_t)len >= n) return;
    if (s_mod > 0)      snprintf(buf + len, n - len, "+%d", s_mod);
    else if (s_mod < 0) snprintf(buf + len, n - len, "%d", s_mod);
}

// --- persistência (Storage API) --------------------------------------------

static void load_prefs(void)
{
    const kit_api_table_t *t = api();
    if (!t || !t->storage) return;
    int32_t v;
    if (t->storage->get_i32("dice_faces", &v) == KIT_OK) {
        for (int i = 0; i < DICE_KINDS; i++) if (DICE_FACES[i] == v) s_die = v;
    }
    if (t->storage->get_i32("dice_count", &v) == KIT_OK && v >= COUNT_MIN && v <= COUNT_MAX)
        s_count = (int)v;
    if (t->storage->get_i32("dice_mod", &v) == KIT_OK && v >= -MOD_LIMIT && v <= MOD_LIMIT)
        s_mod = (int)v;
}

static void save_prefs(void)
{
    const kit_api_table_t *t = api();
    if (!t || !t->storage) return;
    t->storage->set_i32("dice_faces", s_die);
    t->storage->set_i32("dice_count", s_count);
    t->storage->set_i32("dice_mod", s_mod);
}

// --- sincronização de UI -------------------------------------------------

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

static void sync_chips(void)
{
    for (int i = 0; i < DICE_KINDS; i++) {
        bool sel = (DICE_FACES[i] == s_die);
        lv_obj_set_style_bg_color(s_die_chips[i],
            lv_color_hex(sel ? s_accent : KIT_COLOR_SURFACE), 0);
        lv_obj_set_style_text_color(s_die_chip_lbls[i],
            lv_color_hex(sel ? KIT_COLOR_ON_COLOR : KIT_COLOR_TEXT), 0);
    }
}

static void dim_step(lv_obj_t *btn, bool disabled)
{
    lv_obj_set_style_opa(btn, disabled ? LV_OPA_30 : LV_OPA_COVER, 0);
}

static void sync_steppers(void)
{
    lv_label_set_text_fmt(s_count_lbl, "%d", s_count);
    if (s_mod > 0) lv_label_set_text_fmt(s_mod_lbl, "+%d", s_mod);
    else           lv_label_set_text_fmt(s_mod_lbl, "%d", s_mod);
    dim_step(s_count_minus, s_count <= COUNT_MIN);
    dim_step(s_count_plus,  s_count >= COUNT_MAX);
    dim_step(s_mod_minus,   s_mod <= -MOD_LIMIT);
    dim_step(s_mod_plus,    s_mod >= MOD_LIMIT);
}

static lv_obj_t *make_face(lv_obj_t *parent)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, "0");
    lv_obj_set_style_text_font(l, &kit_mono_26, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(KIT_COLOR_TEXT), 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_color(l, lv_color_hex(KIT_COLOR_SURFACE_ALT), 0);
    lv_obj_set_style_bg_opa(l, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(l, 11, 0);
    lv_obj_set_style_pad_hor(l, 11, 0);
    lv_obj_set_style_pad_ver(l, 8, 0);
    lv_obj_set_style_min_width(l, 30, 0);
    return l;
}

static void ensure_faces(int n)
{
    if (s_face_built == n) return;
    lv_obj_clean(s_faces_box);
    for (int i = 0; i < n && i < COUNT_MAX; i++) s_face_lbls[i] = make_face(s_faces_box);
    s_face_built = n;
}

// Página 1 volta ao estado "pronto para rolar".
static void reset_result(void)
{
    if (s_rolling) return;
    char buf[24];
    build_notation(buf, sizeof(buf));
    lv_label_set_text(s_notation_lbl, buf);
    lv_label_set_text(s_total_lbl, "-");
    lv_obj_set_style_text_color(s_total_lbl, lv_color_hex(KIT_COLOR_TEXT_MUTED), 0);
    lv_obj_clear_flag(s_hint_lbl, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_faces_box, LV_OBJ_FLAG_HIDDEN);
}

// --- histórico ---------------------------------------------------------

static void push_history(const char *notation, int total)
{
    for (int i = HIST_MAX - 1; i > 0; i--)
        memcpy(s_hist[i], s_hist[i - 1], sizeof(s_hist[0]));
    snprintf(s_hist[0], sizeof(s_hist[0]), "%s|%d", notation, total);
    if (s_hist_n < HIST_MAX) s_hist_n++;

    lv_obj_add_flag(s_history_empty, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clean(s_history_rows);
    for (int i = 0; i < s_hist_n; i++) {
        char *bar = strchr(s_hist[i], '|');
        if (!bar) continue;
        *bar = '\0';

        lv_obj_t *row = plain_box(s_history_rows);
        lv_obj_set_size(row, lv_pct(100), 58);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(KIT_COLOR_LINE), 0);

        lv_obj_t *k = add_label(row, s_hist[i], KIT_COLOR_TEXT_MUTED, &kit_mono_20, 1);
        lv_obj_align(k, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_t *v = add_label(row, bar + 1,
                                i == 0 ? s_accent : KIT_COLOR_TEXT, &kit_mono_26, 0);
        lv_obj_align(v, LV_ALIGN_RIGHT_MID, 0, 0);

        *bar = '|';
    }
}

// --- rolagem ---------------------------------------------------------

static void roll_tick_cb(lv_timer_t *t);
static void align_result_meta(void);

static void do_roll(void)
{
    if (s_rolling) return;
    s_rolling = true;

    int sum = 0;
    for (int i = 0; i < s_count; i++) {
        s_final_vals[i] = roll_one(s_die);
        sum += s_final_vals[i];
    }
    s_final_total = sum + s_mod;
    bool multi = (s_count > 1 || s_mod != 0);

    ensure_faces(s_count);
    lv_obj_add_flag(s_hint_lbl, LV_OBJ_FLAG_HIDDEN);
    if (multi) lv_obj_clear_flag(s_faces_box, LV_OBJ_FLAG_HIDDEN);
    else       lv_obj_add_flag(s_faces_box, LV_OBJ_FLAG_HIDDEN);
    align_result_meta();

    lv_obj_set_style_text_color(s_total_lbl, lv_color_hex(s_accent), 0);

    // rolar de qualquer página leva para a página do resultado
    lv_tileview_set_tile_by_index(s_tv, 1, 0, LV_ANIM_OFF);

    s_roll_tick = 0;
    s_roll_timer = lv_timer_create(roll_tick_cb, ROLL_TICK_MS, NULL);
}

// Animação leve: durante o "tombo" mexemos SÓ no número grande (1 label/tick).
static void roll_tick_cb(lv_timer_t *t)
{
    (void)t;
    s_roll_tick++;

    if (s_roll_tick < ROLL_TICKS) {
        int lo = s_count + s_mod;
        int hi = s_count * s_die + s_mod;
        lv_label_set_text_fmt(s_total_lbl, "%d", lo + (int)(rand() % (hi - lo + 1)));
        return;
    }

    for (int i = 0; i < s_count; i++)
        lv_label_set_text_fmt(s_face_lbls[i], "%d", s_final_vals[i]);
    lv_label_set_text_fmt(s_total_lbl, "%d", s_final_total);
    lv_obj_set_style_text_color(s_total_lbl, lv_color_hex(KIT_COLOR_TEXT), 0);

    if (s_roll_timer) { lv_timer_delete(s_roll_timer); s_roll_timer = NULL; }
    s_rolling = false;

    char buf[24];
    build_notation(buf, sizeof(buf));
    push_history(buf, s_final_total);
}

// --- callbacks -------------------------------------------------------

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

static void die_chip_cb(lv_event_t *e)
{
    if (s_rolling) return;
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i >= DICE_KINDS) return;
    s_die = DICE_FACES[i];
    sync_chips();
    reset_result();
    save_prefs();
}

static void step_cb(lv_event_t *e)
{
    if (s_rolling) return;
    int code = (int)(intptr_t)lv_event_get_user_data(e);
    switch (code) {
        case 0: if (s_count > COUNT_MIN)  s_count--; break;
        case 1: if (s_count < COUNT_MAX)  s_count++; break;
        case 2: if (s_mod > -MOD_LIMIT)   s_mod--;   break;
        case 3: if (s_mod < MOD_LIMIT)    s_mod++;   break;
    }
    sync_steppers();
    reset_result();
    save_prefs();
}

static void roll_cb(lv_event_t *e)
{
    (void)e;
    do_roll();
}

void kit_dice_roll(void)
{
    if (!s_screen) return;
    do_roll();
}

// --- construção da tela ---------------------------------------------

static void build_titlebar(void)
{
    lv_obj_t *chip = lv_obj_create(s_screen);
    lv_obj_set_size(chip, D_CHIP, D_CHIP);
    lv_obj_set_style_bg_color(chip, lv_color_hex(KIT_COLOR_SURFACE), 0);
    lv_obj_set_style_border_width(chip, 0, 0);
    lv_obj_set_style_radius(chip, 18, 0);
    lv_obj_set_style_pad_all(chip, 0, 0);
    lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(chip, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(chip, 12);
    lv_obj_add_event_cb(chip, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(chip, LV_ALIGN_TOP_LEFT, D_PAD, 16);

    lv_obj_t *g = add_label(chip, KIT_ICON_BACK, KIT_COLOR_TEXT, &kit_display_44, 0);
    lv_obj_center(g);

    lv_obj_t *title = add_label(s_screen, "DADOS", KIT_COLOR_TEXT, &kit_mono_26, 3);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, D_PAD + D_CHIP + 12, 30);

    // indicador de página (3 pontos) no canto direito da titlebar
    lv_obj_t *dots = plain_box(s_screen);
    lv_obj_set_size(dots, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(dots, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dots, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(dots, 6, 0);
    lv_obj_align(dots, LV_ALIGN_TOP_RIGHT, -D_PAD, 40);
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
    lv_obj_set_style_pad_left(p, D_PAD, 0);
    lv_obj_set_style_pad_right(p, D_PAD, 0);
    lv_obj_set_style_pad_top(p, 10, 0);
    lv_obj_set_style_pad_bottom(p, 16, 0);
    lv_obj_set_style_pad_row(p, 14, 0);
    lv_obj_set_flex_flow(p, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(p, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(p, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(p, LV_SCROLLBAR_MODE_AUTO);
    return p;
}

static lv_obj_t *make_step_btn(lv_obj_t *parent, const char *sym, int code)
{
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_set_size(b, D_STEP, D_STEP);
    lv_obj_set_style_bg_color(b, lv_color_hex(KIT_COLOR_SURFACE), 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_radius(b, 16, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(b, 8);
    lv_obj_add_event_cb(b, step_cb, LV_EVENT_CLICKED, (void *)(intptr_t)code);
    lv_obj_t *l = add_label(b, sym, KIT_COLOR_TEXT, &kit_display_44, 0);
    lv_obj_center(l);
    return b;
}

static void make_stepper_row(lv_obj_t *parent, const char *label,
                             int code_minus, int code_plus,
                             lv_obj_t **valref, lv_obj_t **minusref, lv_obj_t **plusref)
{
    lv_obj_t *row = plain_box(parent);
    lv_obj_set_size(row, lv_pct(100), D_STEP);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 8, 0);

    lv_obj_t *lbl = add_label(row, label, KIT_COLOR_TEXT_MUTED, &kit_mono_16, 1);
    lv_obj_set_flex_grow(lbl, 1);

    *minusref = make_step_btn(row, "-", code_minus);

    lv_obj_t *val = add_label(row, "0", KIT_COLOR_TEXT, &kit_display_44, 0);
    lv_obj_set_width(val, 76);
    lv_obj_set_style_text_align(val, LV_TEXT_ALIGN_CENTER, 0);
    *valref = val;

    *plusref = make_step_btn(row, "+", code_plus);
}

// Página 0 — AJUSTE
static void build_page_setup(lv_obj_t *tile)
{
    lv_obj_t *p = page_scroll(tile);

    lv_obj_t *sec = plain_box(p);
    lv_obj_set_size(sec, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(sec, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(sec, 9, 0);
    field_label(sec, "DADO");

    lv_obj_t *chips = plain_box(sec);
    lv_obj_set_size(chips, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(chips, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(chips, 4, 0);
    lv_obj_set_style_pad_row(chips, 8, 0);

    for (int i = 0; i < DICE_KINDS; i++) {
        lv_obj_t *c = lv_obj_create(chips);
        lv_obj_set_size(c, 80, 54);
        lv_obj_set_style_bg_color(c, lv_color_hex(KIT_COLOR_SURFACE), 0);
        lv_obj_set_style_border_width(c, 0, 0);
        lv_obj_set_style_radius(c, 15, 0);
        lv_obj_set_style_pad_all(c, 0, 0);
        lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(c, 4);
        lv_obj_add_event_cb(c, die_chip_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        char t[8];
        snprintf(t, sizeof(t), "D%d", DICE_FACES[i]);
        lv_obj_t *l = add_label(c, t, KIT_COLOR_TEXT, &kit_mono_26, 1);
        lv_obj_center(l);

        s_die_chips[i] = c;
        s_die_chip_lbls[i] = l;
    }

    make_stepper_row(p, "QUANTIDADE",  0, 1, &s_count_lbl, &s_count_minus, &s_count_plus);
    make_stepper_row(p, "MODIFICADOR", 2, 3, &s_mod_lbl,   &s_mod_minus,   &s_mod_plus);
}

// Página 1 — RESULTADO. O número fica no centro exato entre a titlebar e o
// botão ROLAR (padding de cima = padding de baixo); a notação flutua logo
// acima dele e as faces logo abaixo — simétrico e sem depender dos parâmetros.
static void build_page_result(lv_obj_t *tile)
{
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_t *box = lv_obj_create(tile);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    // número (fonte bitmap dedicada ~85 px, largura fixa e centrado)
    s_total_lbl = add_label(box, "-", KIT_COLOR_TEXT_MUTED, &kit_display_120, 0);
    lv_obj_set_width(s_total_lbl, D_CONTENT);
    lv_obj_set_style_text_align(s_total_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_total_lbl, LV_ALIGN_CENTER, 0, D_NUM_OFFSET);

    // notação, largura fixa e centrada
    s_notation_lbl = add_label(box, "1D6", KIT_COLOR_TEXT_MUTED, &kit_mono_20, 3);
    lv_obj_set_width(s_notation_lbl, D_CONTENT);
    lv_obj_set_style_text_align(s_notation_lbl, LV_TEXT_ALIGN_CENTER, 0);

    // dica (antes de rolar) e faces (depois) ocupam o mesmo lugar
    s_hint_lbl = add_label(box, "ROLAR \xC2\xB7 PWR \xC2\xB7 CHACOALHAR",
                           KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);
    lv_obj_set_width(s_hint_lbl, D_CONTENT);
    lv_obj_set_style_text_align(s_hint_lbl, LV_TEXT_ALIGN_CENTER, 0);

    s_faces_box = plain_box(box);
    lv_obj_set_size(s_faces_box, D_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_faces_box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_faces_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_faces_box, 6, 0);
    lv_obj_add_flag(s_faces_box, LV_OBJ_FLAG_HIDDEN);
}

// notação 14 px acima do número; dica/faces 14 px abaixo. Chamado depois do
// layout estar resolvido (align_to é one-shot e precisa das coords válidas).
static void align_result_meta(void)
{
    lv_obj_align_to(s_notation_lbl, s_total_lbl, LV_ALIGN_OUT_TOP_MID, 0, -14);
    lv_obj_align_to(s_hint_lbl,     s_total_lbl, LV_ALIGN_OUT_BOTTOM_MID, 0, 14);
    lv_obj_align_to(s_faces_box,    s_total_lbl, LV_ALIGN_OUT_BOTTOM_MID, 0, 14);
}

// Página 2 — HISTÓRICO
static void build_page_history(lv_obj_t *tile)
{
    lv_obj_t *p = page_scroll(tile);

    field_label(p, "HIST\xC3\x93RICO");

    s_history_empty = add_label(p, "AINDA SEM ROLAGENS", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);

    s_history_rows = plain_box(p);
    lv_obj_set_size(s_history_rows, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_history_rows, LV_FLEX_FLOW_COLUMN);
}

static void build_tileview(void)
{
    s_tv = lv_tileview_create(s_screen);
    lv_obj_set_size(s_tv, KIT_DISPLAY_WIDTH, D_PAGE_H);
    lv_obj_set_pos(s_tv, 0, D_TITLEBAR);
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
    s_roll_btn = lv_obj_create(s_screen);
    lv_obj_set_size(s_roll_btn, D_CONTENT, D_ROLL_H);
    lv_obj_set_style_radius(s_roll_btn, D_ROLL_H / 2, 0);
    lv_obj_set_style_border_width(s_roll_btn, 0, 0);
    lv_obj_set_style_shadow_width(s_roll_btn, 0, 0);
    lv_obj_set_style_pad_all(s_roll_btn, 0, 0);
    lv_obj_set_style_bg_color(s_roll_btn, lv_color_hex(s_accent), 0);
    lv_obj_set_style_bg_opa(s_roll_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(s_roll_btn, LV_OPA_80, LV_STATE_PRESSED);
    lv_obj_clear_flag(s_roll_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_roll_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_roll_btn, 8);
    lv_obj_align(s_roll_btn, LV_ALIGN_BOTTOM_MID, 0, -18);
    lv_obj_add_event_cb(s_roll_btn, roll_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *l = add_label(s_roll_btn, "ROLAR", KIT_COLOR_ON_COLOR, &kit_mono_26, 3);
    lv_obj_center(l);
}

// --- ciclo de vida ---------------------------------------------------

kit_err_t kit_dice_start(uint32_t accent)
{
    if (s_screen) kit_dice_destroy();

    ESP_LOGI(TAG, "Montando Dice Tool...");
    s_accent = accent ? accent : KIT_COLOR_RED;
    s_die = 6; s_count = 1; s_mod = 0;
    s_rolling = false;
    s_hist_n = 0;
    s_face_built = 0;
    load_prefs();

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(KIT_COLOR_BG), 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    build_titlebar();
    build_tileview();
    build_footer();

    // começa na página do resultado (a principal)
    lv_tileview_set_tile_by_index(s_tv, 1, 0, LV_ANIM_OFF);

    lv_obj_update_layout(s_screen);   // resolve tamanhos antes dos align_to
    align_result_meta();

    sync_chips();
    sync_steppers();
    reset_result();
    sync_dots();

    lv_screen_load(s_screen);
    return KIT_OK;
}

void kit_dice_destroy(void)
{
    ESP_LOGI(TAG, "Encerrando Dice Tool.");
    if (s_roll_timer) { lv_timer_delete(s_roll_timer); s_roll_timer = NULL; }
    s_rolling = false;

    if (s_screen) {
        lv_obj_delete(s_screen);
        s_screen = NULL;
    }
    s_tv = NULL;
    s_notation_lbl = s_hint_lbl = s_faces_box = s_total_lbl = NULL;
    s_history_rows = s_history_empty = s_roll_btn = NULL;
    s_count_lbl = s_mod_lbl = NULL;
    s_count_minus = s_count_plus = s_mod_minus = s_mod_plus = NULL;
    s_face_built = 0;
}
