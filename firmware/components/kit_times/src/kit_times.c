#include "kit_times.h"
#include "kit_api.h"
#include "kit_display.h"
#include "kit_fonts.h"
#include "kit_theme.h"
#include "esp_log.h"
#include "lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Sortear Times — linguagem "Brutalist Bauhaus" (ver docs/design/design-language.md).
// Titlebar fixa + tileview de 2 páginas (arrasta na horizontal):
//   0 AJUSTE   — PESSOAS (4..16, botões -/+) e TIMES (2/3/4).
//   1 SORTEIO  — o palco: botão SORTEAR (+ toque no palco, PWR físico, chacoalhar).
//                É a página inicial.
//
// A divisão é sempre equilibrada (⌈n/t⌉ / ⌊n/t⌋) e o embaralhamento sai da
// Random API (TRNG). Jogadores são posições numeradas 01..N — quem senta na
// roda se conta. Sem histórico e sem "melhor de": um sorteio de times se usa
// na hora ou refaz.
//
// Card azul na Home (a Bottle Tool também é azul — escolha do usuário; a paleta
// Bauhaus só tem quatro primárias, repetição entre Tools é aceita).
//
// O resultado é sempre revelado **um a um**: um overlay de tela cheia na cor do
// time, uma pessoa por vez. "PESSOA X" grande (kit_display_72) é o que muda a
// cada toque; o nome do time vai em kit_mono_26 (palavra = mono, nunca display —
// e o Archivo Black COM kerning se sobrepõe, ver nota abaixo). Cada toque/PWR
// avança.
//
// FONTES: o número da pessoa e "PRONTO" usam kit_display_72 — a única fonte
// Archivo Black do projeto gerada com `--no-kerning`. kit_display_44 tem os
// pares de kerning do Archivo Black, que se **sobrepõem** e distorcem palavras
// (mesmo bug que a Decisor Tool teve com a versão antiga da display_72). Por
// isso o nome do time fica em kit_mono_26.
//
// Animação = o padrão validado da Dice/Coin/Primeiro: um único lv_timer curto
// que só troca texto/atributos por tick. Nada de transform_scale/rotation —
// o layer transformado animado estoura o render no CO5300/PSRAM e reinicia
// a board.

static const char *TAG = "KIT_TIMES";

// ---------------------------------------------------------------------------
// Layout (espelha as métricas da Timer / Dice)
// ---------------------------------------------------------------------------
#define X_PAD        16
#define X_CONTENT    (KIT_DISPLAY_WIDTH - 2 * X_PAD)              // 336
#define X_TITLEBAR   88
#define X_PAGE_H     (KIT_DISPLAY_HEIGHT - X_TITLEBAR)            // 360
#define X_CHIP       56
#define X_GO_H       76
#define X_GO_MARGIN  18
#define PAGES        2

#define PEOPLE_MIN   4
#define PEOPLE_MAX   16
#define TEAMS_MAX    4

// Suspense curto antes da revelação (só o bipe + um respiro).
#define DRAW_TICK_MS   55
#define DRAW_TICKS     5

static const int TEAMS_OPTS[] = { 2, 3, 4 };
#define TEAMS_OPT_COUNT ((int)(sizeof(TEAMS_OPTS) / sizeof(TEAMS_OPTS[0])))

// ---------------------------------------------------------------------------
// Estado
// ---------------------------------------------------------------------------
static int      s_people = PEOPLE_MIN;
static int      s_teams  = 2;
static uint32_t s_accent = KIT_COLOR_BLUE;

static int      s_team_of[PEOPLE_MAX + 1];   // 1-indexado: time (0..s_teams-1) de cada pessoa

static bool     s_drawing   = false;
static int      s_tick      = 0;
static int      s_reveal_ix = 0;             // 0 = sem revelação; 1..N; N+1 = "PRONTO"
static lv_timer_t *s_timer  = NULL;

// ---------------------------------------------------------------------------
// Objetos LVGL
// ---------------------------------------------------------------------------
static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_tv     = NULL;
static lv_obj_t *s_tiles[PAGES];
static lv_obj_t *s_dots[PAGES];

// Página 0 — Ajuste
static lv_obj_t *s_people_lbl = NULL;
static lv_obj_t *s_teams_pills[TEAMS_OPT_COUNT];
static lv_obj_t *s_teams_pill_lbls[TEAMS_OPT_COUNT];

// Página 1 — Sorteio (palco ocioso)
static lv_obj_t *s_headline = NULL;
static lv_obj_t *s_go_btn   = NULL;
static lv_obj_t *s_go_lbl   = NULL;

// Overlay da revelação (um a um)
static lv_obj_t *s_reveal   = NULL;
static lv_obj_t *s_rv_who    = NULL;   // rótulo "PESSOA" (ou a frase no "PRONTO")
static lv_obj_t *s_rv_count  = NULL;   // número da pessoa, grande — muda a cada avanço (kit_display_72)
static lv_obj_t *s_rv_of     = NULL;   // "DE N"
static lv_obj_t *s_rv_tag    = NULL;   // rótulo "TIME"
static lv_obj_t *s_rv_name   = NULL;   // nome do time (kit_mono_26)
static lv_obj_t *s_rv_next   = NULL;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const kit_api_table_t *api(void) { return kit_api_get_table(); }

static uint32_t on_accent(void)
{
    return (s_accent == KIT_COLOR_YELLOW) ? KIT_COLOR_ON_YELLOW : KIT_COLOR_ON_COLOR;
}

static uint32_t team_color(int i)
{
    static const uint32_t C[TEAMS_MAX] = {
        KIT_COLOR_RED, KIT_COLOR_BLUE, KIT_COLOR_YELLOW, KIT_COLOR_GREEN,
    };
    return C[i & 3];
}

static const char *team_name(int i)
{
    static const char *const N[TEAMS_MAX] = { "VERMELHO", "AZUL", "AMARELO", "VERDE" };
    return N[i & 3];
}

// Texto sobre a cor de um time (preto no amarelo, paper no resto).
static uint32_t on_team(int i)
{
    return (team_color(i) == KIT_COLOR_YELLOW) ? KIT_COLOR_ON_YELLOW : KIT_COLOR_ON_COLOR;
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

static void beep(uint16_t freq, uint16_t ms)
{
    const kit_api_table_t *t = api();
    if (t && t->audio) t->audio->beep(freq, ms);
}

// Cada revelação toca a próxima nota de uma escala maior, subindo em oitavas.
static void reveal_note(void)
{
    static const uint16_t SCALE[] = {
        523, 587, 659, 698, 784, 880, 988,          // C5 D5 E5 F5 G5 A5 B5
        1047, 1175, 1319, 1397, 1568, 1760, 1976,   // C6..B6
        2093, 2349, 2637, 2794, 3136,               // C7..G7
    };
    int n = s_reveal_ix - 1;
    if (n < 0) n = 0;
    if (n >= (int)(sizeof(SCALE) / sizeof(SCALE[0])))
        n = (int)(sizeof(SCALE) / sizeof(SCALE[0])) - 1;
    beep(SCALE[n], 95);
}

// ---------------------------------------------------------------------------
// Persistência (Storage API)
// ---------------------------------------------------------------------------

static void load_prefs(void)
{
    const kit_api_table_t *t = api();
    if (!t || !t->storage) return;
    int32_t v;
    if (t->storage->get_i32("times_people", &v) == KIT_OK && v >= PEOPLE_MIN && v <= PEOPLE_MAX)
        s_people = (int)v;
    if (t->storage->get_i32("times_count", &v) == KIT_OK && v >= 2 && v <= TEAMS_MAX)
        s_teams = (int)v;
    if (s_teams > s_people) s_teams = s_people;
}

static void save_prefs(void)
{
    const kit_api_table_t *t = api();
    if (!t || !t->storage) return;
    t->storage->set_i32("times_people", s_people);
    t->storage->set_i32("times_count", s_teams);
}

// ---------------------------------------------------------------------------
// O sorteio
// ---------------------------------------------------------------------------

static int rnd(int min, int max)
{
    const kit_api_table_t *t = api();
    if (t && t->random) return (int)t->random->range(min, max);
    return min + rand() % (max - min + 1);
}

// Fisher-Yates das posições 1..N e distribuição equilibrada nos times.
static void do_split(void)
{
    int ids[PEOPLE_MAX];
    for (int i = 0; i < s_people; i++) ids[i] = i + 1;
    for (int i = s_people - 1; i > 0; i--) {
        int j = rnd(0, i);
        int tmp = ids[i]; ids[i] = ids[j]; ids[j] = tmp;
    }

    int base = s_people / s_teams;
    int rem  = s_people % s_teams;
    int k = 0;
    for (int ti = 0; ti < s_teams; ti++) {
        int size = base + (ti < rem ? 1 : 0);
        for (int m = 0; m < size; m++) s_team_of[ids[k++]] = ti;
    }
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

static void sync_teams_pills(void)
{
    uint32_t sel_txt = on_accent();
    for (int i = 0; i < TEAMS_OPT_COUNT; i++) {
        bool sel = (TEAMS_OPTS[i] == s_teams);
        lv_obj_set_style_bg_color(s_teams_pills[i],
            lv_color_hex(sel ? s_accent : KIT_COLOR_SURFACE), 0);
        lv_obj_set_style_text_color(s_teams_pill_lbls[i],
            lv_color_hex(sel ? sel_txt : KIT_COLOR_TEXT), 0);
    }
}

static void sync_people(void)
{
    if (s_people_lbl) lv_label_set_text_fmt(s_people_lbl, "%d", s_people);
}

static void sync_headline(void)
{
    if (!s_headline) return;
    lv_label_set_text_fmt(s_headline, "%d TIMES\n%d PESSOAS", s_teams, s_people);
}

// ---------------------------------------------------------------------------
// Revelação — um a um
// ---------------------------------------------------------------------------

static void paint_reveal(void)
{
    if (s_reveal_ix > s_people) {          // tela "PRONTO"
        lv_obj_set_style_bg_color(s_reveal, lv_color_hex(KIT_COLOR_BG), 0);
        lv_label_set_text(s_rv_who, "TODO MUNDO TEM TIME");
        lv_obj_set_style_text_color(s_rv_who, lv_color_hex(KIT_COLOR_TEXT_MUTED), 0);
        lv_label_set_text(s_rv_count, "PRONTO");
        lv_obj_set_style_text_color(s_rv_count, lv_color_hex(s_accent), 0);
        lv_obj_add_flag(s_rv_of, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_rv_tag, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_rv_name, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_rv_next, "TOQUE PARA FECHAR");
        lv_obj_set_style_text_color(s_rv_next, lv_color_hex(KIT_COLOR_TEXT_MUTED), 0);
        return;
    }

    int ti = s_team_of[s_reveal_ix];
    uint32_t fg = on_team(ti);
    lv_obj_set_style_bg_color(s_reveal, lv_color_hex(team_color(ti)), 0);

    // "PESSOA X" é o que mais muda a cada toque (duas pessoas seguidas podem cair
    // no mesmo time, então a cor de fundo não denuncia a troca) → o número vai
    // grande em kit_display_72, com "PESSOA" pequeno em cima e "DE N" embaixo.
    lv_obj_clear_flag(s_rv_of, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_rv_tag, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_rv_name, LV_OBJ_FLAG_HIDDEN);

    lv_label_set_text(s_rv_who, "PESSOA");
    lv_obj_set_style_text_color(s_rv_who, lv_color_hex(fg), 0);
    lv_label_set_text_fmt(s_rv_count, "%d", s_reveal_ix);
    lv_obj_set_style_text_color(s_rv_count, lv_color_hex(fg), 0);
    lv_label_set_text_fmt(s_rv_of, "DE %d", s_people);
    lv_obj_set_style_text_color(s_rv_of, lv_color_hex(fg), 0);
    lv_obj_set_style_text_color(s_rv_tag, lv_color_hex(fg), 0);
    lv_label_set_text(s_rv_name, team_name(ti));
    lv_obj_set_style_text_color(s_rv_name, lv_color_hex(fg), 0);
    lv_label_set_text(s_rv_next, s_reveal_ix == s_people ? "TOQUE PARA TERMINAR"
                                                        : "TOQUE PARA A PR\xC3\x93XIMA");
    lv_obj_set_style_text_color(s_rv_next, lv_color_hex(fg), 0);
}

static void start_reveal(void)
{
    s_reveal_ix = 1;
    paint_reveal();
    lv_obj_clear_flag(s_reveal, LV_OBJ_FLAG_HIDDEN);
    reveal_note();
}

static void advance_reveal(void)
{
    if (s_reveal_ix > s_people) {          // fecha
        lv_obj_add_flag(s_reveal, LV_OBJ_FLAG_HIDDEN);
        s_reveal_ix = 0;
        return;
    }
    s_reveal_ix++;
    paint_reveal();
    if (s_reveal_ix <= s_people) reveal_note();   // nota subindo por revelação
    else                         beep(1046, 70);  // chegou no "PRONTO"
}

// ---------------------------------------------------------------------------
// Timer do sorteio (suspense curto → revelação)
// ---------------------------------------------------------------------------

static void tick_cb(lv_timer_t *t)
{
    (void)t;
    if (++s_tick < DRAW_TICKS) return;

    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    s_drawing = false;
    start_reveal();   // já toca a 1ª nota da escala
}

static void start_draw(void)
{
    if (s_drawing || !s_screen) return;
    if (s_tv) lv_tileview_set_tile_by_index(s_tv, 1, 0, LV_ANIM_OFF);

    s_drawing = true;
    s_tick = 0;
    do_split();
    s_timer = lv_timer_create(tick_cb, DRAW_TICK_MS, NULL);
}

void kit_times_draw(void)
{
    if (s_reveal && !lv_obj_has_flag(s_reveal, LV_OBJ_FLAG_HIDDEN)) {
        advance_reveal();
        return;
    }
    start_draw();
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

static void tv_changed_cb(lv_event_t *e) { (void)e; sync_dots(); }

static void stage_cb(lv_event_t *e)  { (void)e; kit_times_draw(); }
static void go_cb(lv_event_t *e)     { (void)e; kit_times_draw(); }
static void reveal_cb(lv_event_t *e) { (void)e; advance_reveal(); }

static void people_cb(lv_event_t *e)
{
    int delta = (int)(intptr_t)lv_event_get_user_data(e);
    int v = s_people + delta;
    if (v < PEOPLE_MIN) v = PEOPLE_MIN;
    if (v > PEOPLE_MAX) v = PEOPLE_MAX;
    if (v == s_people) return;
    s_people = v;
    if (s_teams > s_people) s_teams = s_people;
    sync_people();
    sync_teams_pills();
    sync_headline();
    save_prefs();
}

static void teams_pill_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i >= TEAMS_OPT_COUNT) return;
    int v = TEAMS_OPTS[i];
    if (v > s_people) return;
    s_teams = v;
    sync_teams_pills();
    sync_headline();
    save_prefs();
}

// ---------------------------------------------------------------------------
// Construção da tela
// ---------------------------------------------------------------------------

static void build_titlebar(void)
{
    lv_obj_t *chip = lv_obj_create(s_screen);
    lv_obj_set_size(chip, X_CHIP, X_CHIP);
    lv_obj_set_style_bg_color(chip, lv_color_hex(KIT_COLOR_SURFACE), 0);
    lv_obj_set_style_border_width(chip, 0, 0);
    lv_obj_set_style_radius(chip, 18, 0);
    lv_obj_set_style_pad_all(chip, 0, 0);
    lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(chip, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(chip, 12);
    lv_obj_add_event_cb(chip, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(chip, LV_ALIGN_TOP_LEFT, X_PAD, 16);

    lv_obj_t *g = add_label(chip, KIT_ICON_BACK, KIT_COLOR_TEXT, &kit_display_44, 0);
    lv_obj_center(g);

    lv_obj_t *title = add_label(s_screen, "TIMES", KIT_COLOR_TEXT, &kit_mono_26, 3);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, X_PAD + X_CHIP + 12, 30);

    lv_obj_t *dots = plain_box(s_screen);
    lv_obj_set_size(dots, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(dots, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dots, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(dots, 6, 0);
    lv_obj_align(dots, LV_ALIGN_TOP_RIGHT, -X_PAD, 40);
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

static lv_obj_t *make_pill(lv_obj_t *parent, const char *txt, int h, bool grow,
                           lv_event_cb_t cb, int code, lv_obj_t **out_lbl)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_set_height(c, h);
    if (grow) lv_obj_set_flex_grow(c, 1);
    lv_obj_set_style_bg_color(c, lv_color_hex(KIT_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(c, 0, 0);
    lv_obj_set_style_radius(c, 15, 0);
    lv_obj_set_style_pad_all(c, 0, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(c, 4);
    lv_obj_add_event_cb(c, cb, LV_EVENT_CLICKED, (void *)(intptr_t)code);
    lv_obj_t *l = add_label(c, txt, KIT_COLOR_TEXT, &kit_mono_20, 1);
    lv_obj_center(l);
    if (out_lbl) *out_lbl = l;
    return c;
}

// Página 0 — AJUSTE
static void build_page_adjust(lv_obj_t *tile)
{
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_t *p = lv_obj_create(tile);
    lv_obj_remove_style_all(p);
    lv_obj_set_size(p, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_left(p, X_PAD, 0);
    lv_obj_set_style_pad_right(p, X_PAD, 0);
    lv_obj_set_style_pad_top(p, 18, 0);
    lv_obj_set_style_pad_bottom(p, 32, 0);
    lv_obj_set_style_pad_row(p, 22, 0);
    lv_obj_set_flex_flow(p, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(p, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(p, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(p, LV_SCROLLBAR_MODE_AUTO);

    // -------- PESSOAS --------
    lv_obj_t *sec_p = plain_box(p);
    lv_obj_set_size(sec_p, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(sec_p, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(sec_p, 10, 0);
    field_label(sec_p, "PESSOAS");

    lv_obj_t *step = plain_box(sec_p);
    lv_obj_set_size(step, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(step, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(step, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    make_pill(step, "-", 60, false, people_cb, -1, NULL);
    s_people_lbl = add_label(step, "4", KIT_COLOR_TEXT, &kit_display_44, 0);
    make_pill(step, "+", 60, false, people_cb, 1, NULL);
    lv_obj_set_width(lv_obj_get_child(step, 0), 72);
    lv_obj_set_width(lv_obj_get_child(step, 2), 72);

    // -------- TIMES --------
    lv_obj_t *sec_t = plain_box(p);
    lv_obj_set_size(sec_t, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(sec_t, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(sec_t, 10, 0);
    field_label(sec_t, "TIMES");

    lv_obj_t *trow = plain_box(sec_t);
    lv_obj_set_size(trow, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(trow, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(trow, 8, 0);
    for (int i = 0; i < TEAMS_OPT_COUNT; i++) {
        char n[4];
        snprintf(n, sizeof(n), "%d", TEAMS_OPTS[i]);
        s_teams_pills[i] = make_pill(trow, n, 58, true, teams_pill_cb, i, &s_teams_pill_lbls[i]);
    }
}

// Página 1 — SORTEIO
static void build_page_stage(lv_obj_t *tile)
{
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_t *box = lv_obj_create(tile);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    // palco tocável (entre a titlebar e o botão)
    int stage_h = X_PAGE_H - (X_GO_H + 2 * X_GO_MARGIN);
    lv_obj_t *stage = plain_box(box);
    lv_obj_set_size(stage, KIT_DISPLAY_WIDTH, stage_h);
    lv_obj_set_pos(stage, 0, 0);
    lv_obj_add_flag(stage, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(stage, stage_cb, LV_EVENT_CLICKED, NULL);

    // estado ocioso — só tipografia, sem "wrap box"
    lv_obj_t *idle = plain_box(stage);
    lv_obj_set_size(idle, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(idle, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(idle, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(idle, 14, 0);
    lv_obj_clear_flag(idle, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(idle);

    s_headline = add_label(idle, "2 TIMES\n4 PESSOAS", KIT_COLOR_TEXT, &kit_mono_26, 2);
    lv_obj_set_style_text_align(s_headline, LV_TEXT_ALIGN_CENTER, 0);
    add_label(idle, "TOQUE PARA SORTEAR", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 3);

    // botão primário
    s_go_btn = lv_obj_create(box);
    lv_obj_set_size(s_go_btn, X_CONTENT, X_GO_H);
    lv_obj_set_style_radius(s_go_btn, X_GO_H / 2, 0);
    lv_obj_set_style_border_width(s_go_btn, 0, 0);
    lv_obj_set_style_shadow_width(s_go_btn, 0, 0);
    lv_obj_set_style_pad_all(s_go_btn, 0, 0);
    lv_obj_set_style_bg_color(s_go_btn, lv_color_hex(s_accent), 0);
    lv_obj_set_style_bg_opa(s_go_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(s_go_btn, LV_OPA_80, LV_STATE_PRESSED);
    lv_obj_clear_flag(s_go_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_go_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_go_btn, 8);
    lv_obj_align(s_go_btn, LV_ALIGN_BOTTOM_MID, 0, -X_GO_MARGIN);
    lv_obj_add_event_cb(s_go_btn, go_cb, LV_EVENT_CLICKED, NULL);
    s_go_lbl = add_label(s_go_btn, "SORTEAR", on_accent(), &kit_mono_26, 3);
    lv_obj_center(s_go_lbl);
}

static void build_tileview(void)
{
    s_tv = lv_tileview_create(s_screen);
    lv_obj_set_size(s_tv, KIT_DISPLAY_WIDTH, X_PAGE_H);
    lv_obj_set_pos(s_tv, 0, X_TITLEBAR);
    lv_obj_set_style_bg_opa(s_tv, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_tv, 0, 0);
    lv_obj_set_scrollbar_mode(s_tv, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(s_tv, tv_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_tiles[0] = lv_tileview_add_tile(s_tv, 0, 0, LV_DIR_HOR);
    s_tiles[1] = lv_tileview_add_tile(s_tv, 1, 0, LV_DIR_HOR);
    build_page_adjust(s_tiles[0]);
    build_page_stage(s_tiles[1]);
}

static void build_reveal(void)
{
    s_reveal = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_reveal);
    lv_obj_set_size(s_reveal, KIT_DISPLAY_WIDTH, KIT_DISPLAY_HEIGHT);
    lv_obj_set_pos(s_reveal, 0, 0);
    lv_obj_set_style_bg_color(s_reveal, lv_color_hex(team_color(0)), 0);
    lv_obj_set_style_bg_opa(s_reveal, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_reveal, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_reveal, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_reveal, reveal_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(s_reveal, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *col = plain_box(s_reveal);
    lv_obj_set_size(col, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, 2, 0);
    lv_obj_add_flag(col, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_center(col);

    s_rv_who   = add_label(col, "PESSOA", KIT_COLOR_ON_COLOR, &kit_mono_16, 3);
    s_rv_count = add_label(col, "1", KIT_COLOR_ON_COLOR, &kit_display_72, 0);
    s_rv_of    = add_label(col, "DE 4", KIT_COLOR_ON_COLOR, &kit_mono_16, 2);
    lv_obj_set_style_pad_bottom(s_rv_of, 26, 0);
    s_rv_tag   = add_label(col, "TIME", KIT_COLOR_ON_COLOR, &kit_mono_16, 3);
    s_rv_name  = add_label(col, "VERMELHO", KIT_COLOR_ON_COLOR, &kit_mono_26, 2);

    s_rv_next = add_label(s_reveal, "TOQUE PARA A PR\xC3\x93XIMA", KIT_COLOR_ON_COLOR, &kit_mono_16, 3);
    lv_obj_align(s_rv_next, LV_ALIGN_BOTTOM_MID, 0, -40);
}

// ---------------------------------------------------------------------------
// Ciclo de vida
// ---------------------------------------------------------------------------

kit_err_t kit_times_start(uint32_t accent)
{
    if (s_screen) kit_times_destroy();

    ESP_LOGI(TAG, "Montando Sortear Times...");
    s_accent    = accent ? accent : KIT_COLOR_BLUE;
    s_people    = PEOPLE_MIN;
    s_teams     = 2;
    s_drawing   = false;
    s_tick      = 0;
    s_reveal_ix = 0;
    load_prefs();

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(KIT_COLOR_BG), 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    build_titlebar();
    build_tileview();
    build_reveal();

    lv_tileview_set_tile_by_index(s_tv, 1, 0, LV_ANIM_OFF);   // começa no SORTEIO
    lv_obj_update_layout(s_screen);

    sync_people();
    sync_teams_pills();
    sync_headline();
    sync_dots();

    lv_screen_load(s_screen);
    return KIT_OK;
}

void kit_times_destroy(void)
{
    ESP_LOGI(TAG, "Encerrando Sortear Times.");
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    s_drawing   = false;
    s_reveal_ix = 0;

    if (s_screen) { lv_obj_delete(s_screen); s_screen = NULL; }

    s_tv = NULL;
    s_people_lbl = NULL;
    s_headline = s_go_btn = s_go_lbl = NULL;
    s_reveal = s_rv_who = s_rv_count = s_rv_of = s_rv_tag = s_rv_name = s_rv_next = NULL;
}
