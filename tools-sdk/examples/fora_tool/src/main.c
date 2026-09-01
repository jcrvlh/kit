/**
 * @file main.c
 * @brief FORA — Tool de dedução social para o KIT.
 *
 * Jogo para 3–12 jogadores. Todos recebem uma palavra secreta, exceto
 * o FORA, que precisa disfarçar. Após rodadas de perguntas, todos votam
 * em quem acham que é o FORA. Se descoberto, o FORA ainda pode ganhar
 * adivinhando a palavra entre 4 opções.
 *
 * A interface usa a linguagem Brutalist Bauhaus do KIT:
 * - Fundo AMOLED preto (#000)
 * - Tipografia mono CAIXA ALTA (Space Mono)
 * - Accent color: KIT_COLOR_RED (#C6472F)
 * - Botões pill-shape, áreas de toque grandes
 *
 * Toda a lógica do jogo está separada em fora_game.c/h.
 * Este arquivo cuida apenas da apresentação e do ciclo de vida.
 */

#include "kit_tool_api.h"
#include "kit_theme.h"
#include "kit_fonts.h"
#include "fora_game.h"
#include "fora_words.h"
#include <stdio.h>
#include <string.h>

/* -----------------------------------------------------------------------
 * Layout constants (368 × 448 px)
 * ----------------------------------------------------------------------- */

#define F_SCREEN_W     368
#define F_SCREEN_H     448
#define F_PAD          16
#define F_CONTENT      (F_SCREEN_W - 2 * F_PAD)    /* 336 */
#define F_BTN_H        76
#define F_BTN_MARGIN   18
#define F_STEP_SIZE    68
#define F_CHIP_H       54
#define F_CHIP_RAD     15
#define F_CAT_CHIP_H   46
#define F_ACCENT       KIT_COLOR_RED

/* Reveal animation */
#define F_REVEAL_TICKS    12
#define F_REVEAL_MS_MIN   40
#define F_REVEAL_MS_MAX   120

/* Vote reveal animation */
#define F_VOTE_TICKS      16
#define F_VOTE_MS_MIN     50
#define F_VOTE_MS_MAX     180

#ifndef KIT_SDK_STUBS

/* -----------------------------------------------------------------------
 * State
 * ----------------------------------------------------------------------- */

static const kit_api_table_t *s_api = NULL;
static fora_state_t s_game;

/* LVGL objects — destroyed on phase change */
static lv_obj_t *s_screen = NULL;

/* Animation timers */
static lv_timer_t *s_anim_timer = NULL;
static int s_anim_tick = 0;

/* -----------------------------------------------------------------------
 * LVGL Helpers (espelho do estilo das Tools internas)
 * ----------------------------------------------------------------------- */

static uint32_t on_accent(void)
{
    return (F_ACCENT == KIT_COLOR_YELLOW) ? KIT_COLOR_ON_YELLOW : KIT_COLOR_ON_COLOR;
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

static lv_obj_t *add_label_wrap(lv_obj_t *parent, const char *txt, uint32_t color,
                                const lv_font_t *font, int letter_space)
{
    lv_obj_t *l = add_label(parent, txt, color, font, letter_space);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, F_CONTENT);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    return l;
}

static lv_obj_t *plain_box(lv_obj_t *parent)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

/** Botão pill-shape principal no rodapé (como as Tools internas). */
static lv_obj_t *make_pill_btn(lv_obj_t *parent, const char *text,
                                lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_set_size(btn, F_CONTENT, F_BTN_H);
    lv_obj_set_style_radius(btn, F_BTN_H / 2, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(F_ACCENT), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_80, LV_STATE_PRESSED);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(btn, 8);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -F_BTN_MARGIN);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);

    lv_obj_t *l = add_label(btn, text, on_accent(), &kit_mono_26, 3);
    lv_obj_center(l);

    return btn;
}

/** Botão de step [-] / [+] (como kit_coin). */
static lv_obj_t *make_step_btn(lv_obj_t *parent, const char *sym,
                                lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_set_size(b, F_STEP_SIZE, F_STEP_SIZE);
    lv_obj_set_style_bg_color(b, lv_color_hex(KIT_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_radius(b, 20, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(b, 16);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user_data);
    lv_obj_t *l = add_label(b, sym, KIT_COLOR_TEXT, &kit_display_44, 0);
    lv_obj_center(l);
    return b;
}

static void beep_tap(void)
{
    if (s_api && s_api->audio) s_api->audio->beep(1800, 30);
}

static void beep_reveal(void)
{
    if (s_api && s_api->audio) s_api->audio->beep(2200, 60);
}

static void beep_suspense(void)
{
    if (s_api && s_api->audio) s_api->audio->beep(800, 40);
}

static void beep_success(void)
{
    if (s_api && s_api->audio) s_api->audio->beep(2600, 100);
}

static void beep_fail(void)
{
    if (s_api && s_api->audio) s_api->audio->beep(400, 200);
}

/* -----------------------------------------------------------------------
 * Screen lifecycle — cada fase cria uma tela nova
 * ----------------------------------------------------------------------- */

static void kill_anim(void)
{
    if (s_anim_timer) {
        lv_timer_delete(s_anim_timer);
        s_anim_timer = NULL;
    }
    s_anim_tick = 0;
}

static lv_obj_t *new_screen(void)
{
    kill_anim();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(KIT_COLOR_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    return scr;
}

static void show_screen(lv_obj_t *scr)
{
    lv_obj_t *old = s_screen;
    s_screen = scr;
    lv_screen_load(scr);
    if (old) lv_obj_delete(old);
}

/* Forward declarations */
static void build_config(void);
static void build_distribute(void);
static void build_reveal(void);
static void build_all_ready(void);
static void build_question(void);
static void build_round_end(void);
static void build_vote(void);
static void build_vote_reveal(void);
static void build_fora_escaped(void);
static void build_final_guess(void);
static void build_guess_result(void);
static void build_result(void);

/* -----------------------------------------------------------------------
 * Persistência de configuração
 * ----------------------------------------------------------------------- */

static void save_config(void)
{
    if (!s_api || !s_api->storage) return;
    s_api->storage->set_i32("fora_players", s_game.num_players);
    s_api->storage->set_i32("fora_cat",     s_game.category_index);
    s_api->storage->set_i32("fora_rounds",  s_game.num_rounds);
}

static void load_config(void)
{
    if (!s_api || !s_api->storage) return;
    int32_t v;
    if (s_api->storage->get_i32("fora_players", &v) == KIT_OK &&
        v >= FORA_MIN_PLAYERS && v <= FORA_MAX_PLAYERS)
        s_game.num_players = (int)v;
    if (s_api->storage->get_i32("fora_cat", &v) == KIT_OK &&
        v >= FORA_MIX_INDEX && v < FORA_CATEGORY_COUNT)
        s_game.category_index = (int)v;
    if (s_api->storage->get_i32("fora_rounds", &v) == KIT_OK &&
        (v == 1 || v == 2))
        s_game.num_rounds = (int)v;
}

/* =======================================================================
 * PHASE: CONFIG
 * ======================================================================= */

static lv_obj_t *s_cfg_players_lbl = NULL;
static lv_obj_t *s_cfg_cat_selected = NULL;
static lv_obj_t *s_cfg_round_chips[2];
static lv_obj_t *s_cfg_round_lbls[2];

static void cfg_update_players(void)
{
    lv_label_set_text_fmt(s_cfg_players_lbl, "%d", s_game.num_players);
}

static void cfg_update_rounds(void)
{
    for (int i = 0; i < 2; i++) {
        bool sel = (s_game.num_rounds == (i + 1));
        lv_obj_set_style_bg_color(s_cfg_round_chips[i],
            lv_color_hex(sel ? F_ACCENT : KIT_COLOR_SURFACE), 0);
        lv_obj_set_style_text_color(s_cfg_round_lbls[i],
            lv_color_hex(sel ? on_accent() : KIT_COLOR_TEXT), 0);
    }
}

static void cfg_player_step_cb(lv_event_t *e)
{
    int delta = (int)(intptr_t)lv_event_get_user_data(e);
    int n = s_game.num_players + delta;
    if (n < FORA_MIN_PLAYERS || n > FORA_MAX_PLAYERS) return;
    s_game.num_players = n;
    cfg_update_players();
    beep_tap();
}

static void cfg_cat_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    s_game.category_index = idx;  /* -1 = MIX, 0..N-1 = categoria */
    beep_tap();
    /* Rebuild para atualizar seleção visual */
    build_config();
}

static void cfg_round_cb(lv_event_t *e)
{
    int r = (int)(intptr_t)lv_event_get_user_data(e);
    s_game.num_rounds = r;
    cfg_update_rounds();
    beep_tap();
}

static void cfg_start_cb(lv_event_t *e)
{
    (void)e;
    beep_tap();
    save_config();

    /* Inicia o jogo */
    fora_game_reset(&s_game);
    /* Preserva config */
    /* (num_players, category_index, num_rounds já estão setados) */
    s_game.phase = FORA_PHASE_CONFIG; /* será atualizada abaixo */

    /* Sorteia palavra e FORA */
    fora_game_select_word(&s_game, s_api ? s_api->random : NULL);

    /* Inicia distribuição */
    s_game.current_player = 0;
    s_game.phase = FORA_PHASE_DISTRIBUTE;
    build_distribute();
}

static void cfg_exit_cb(lv_event_t *e)
{
    (void)e;
    if (s_api && s_api->system) s_api->system->exit();
}

static void build_config(void)
{
    lv_obj_t *scr = new_screen();

    /* Scrollable container */
    lv_obj_t *cont = lv_obj_create(scr);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, F_SCREEN_W, F_SCREEN_H - F_BTN_H - F_BTN_MARGIN - 10);
    lv_obj_set_pos(cont, 0, 0);
    lv_obj_set_style_pad_left(cont, F_PAD, 0);
    lv_obj_set_style_pad_right(cont, F_PAD, 0);
    lv_obj_set_style_pad_top(cont, 20, 0);
    lv_obj_set_style_pad_bottom(cont, 10, 0);
    lv_obj_set_style_pad_row(cont, 16, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(cont, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_AUTO);

    /* --- Título --- */
    lv_obj_t *title = add_label(cont, "FORA", F_ACCENT, &kit_display_44, 4);
    lv_obj_set_width(title, F_CONTENT);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *sub = add_label(cont, "QUEM EST\xC3\x81 FORA?",
                              KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);
    lv_obj_set_width(sub, F_CONTENT);
    lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_CENTER, 0);

    /* --- JOGADORES --- */
    add_label(cont, "JOGADORES", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);

    lv_obj_t *prow = plain_box(cont);
    lv_obj_set_size(prow, F_CONTENT, F_STEP_SIZE);
    lv_obj_set_flex_flow(prow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(prow, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(prow, 16, 0);

    make_step_btn(prow, "-", cfg_player_step_cb, (void *)(intptr_t)(-1));

    s_cfg_players_lbl = add_label(prow, "5", KIT_COLOR_TEXT, &kit_display_44, 0);
    lv_obj_set_width(s_cfg_players_lbl, 80);
    lv_obj_set_style_text_align(s_cfg_players_lbl, LV_TEXT_ALIGN_CENTER, 0);

    make_step_btn(prow, "+", cfg_player_step_cb, (void *)(intptr_t)(1));
    cfg_update_players();

    /* --- CATEGORIA --- */
    add_label(cont, "CATEGORIA", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);

    /* Grid de categorias (MIX + 20 categorias) scrollável */
    lv_obj_t *catgrid = plain_box(cont);
    lv_obj_set_size(catgrid, F_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(catgrid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(catgrid, 6, 0);
    lv_obj_set_style_pad_row(catgrid, 6, 0);
    lv_obj_add_flag(catgrid, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    /* MIX chip */
    {
        bool sel = (s_game.category_index == FORA_MIX_INDEX);
        lv_obj_t *c = lv_obj_create(catgrid);
        lv_obj_set_size(c, LV_SIZE_CONTENT, F_CAT_CHIP_H);
        lv_obj_set_style_pad_left(c, 14, 0);
        lv_obj_set_style_pad_right(c, 14, 0);
        lv_obj_set_style_pad_top(c, 0, 0);
        lv_obj_set_style_pad_bottom(c, 0, 0);
        lv_obj_set_style_bg_color(c, lv_color_hex(sel ? F_ACCENT : KIT_COLOR_SURFACE), 0);
        lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(c, 0, 0);
        lv_obj_set_style_radius(c, F_CHIP_RAD, 0);
        lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(c, cfg_cat_cb, LV_EVENT_CLICKED, (void *)(intptr_t)FORA_MIX_INDEX);

        lv_obj_t *l = add_label(c, "MIX", sel ? on_accent() : KIT_COLOR_TEXT,
                                &kit_mono_16, 1);
        lv_obj_center(l);
    }

    /* Categorias */
    for (int i = 0; i < FORA_CATEGORY_COUNT; i++) {
        bool sel = (s_game.category_index == i);
        lv_obj_t *c = lv_obj_create(catgrid);
        lv_obj_set_size(c, LV_SIZE_CONTENT, F_CAT_CHIP_H);
        lv_obj_set_style_pad_left(c, 14, 0);
        lv_obj_set_style_pad_right(c, 14, 0);
        lv_obj_set_style_pad_top(c, 0, 0);
        lv_obj_set_style_pad_bottom(c, 0, 0);
        lv_obj_set_style_bg_color(c, lv_color_hex(sel ? F_ACCENT : KIT_COLOR_SURFACE), 0);
        lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(c, 0, 0);
        lv_obj_set_style_radius(c, F_CHIP_RAD, 0);
        lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(c, cfg_cat_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        lv_obj_t *l = add_label(c, FORA_CATEGORIES[i].name,
                                sel ? on_accent() : KIT_COLOR_TEXT,
                                &kit_mono_16, 1);
        lv_obj_center(l);
    }

    /* --- RODADAS --- */
    add_label(cont, "RODADAS", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);

    lv_obj_t *rrow = plain_box(cont);
    lv_obj_set_size(rrow, F_CONTENT, F_CHIP_H);
    lv_obj_set_flex_flow(rrow, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(rrow, 8, 0);

    static const char *R_LABELS[] = { "1", "2" };
    for (int i = 0; i < 2; i++) {
        lv_obj_t *c = lv_obj_create(rrow);
        lv_obj_set_height(c, F_CHIP_H);
        lv_obj_set_flex_grow(c, 1);
        lv_obj_set_style_bg_color(c, lv_color_hex(KIT_COLOR_SURFACE), 0);
        lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(c, 0, 0);
        lv_obj_set_style_radius(c, F_CHIP_RAD, 0);
        lv_obj_set_style_pad_all(c, 0, 0);
        lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(c, cfg_round_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)(i + 1));

        lv_obj_t *l = add_label(c, R_LABELS[i], KIT_COLOR_TEXT, &kit_mono_26, 1);
        lv_obj_center(l);

        s_cfg_round_chips[i] = c;
        s_cfg_round_lbls[i] = l;
    }
    cfg_update_rounds();

    /* --- Botão COMEÇAR --- */
    make_pill_btn(scr, "COME\xC3\x87" "AR", cfg_start_cb, NULL);

    show_screen(scr);
}

/* =======================================================================
 * PHASE: DISTRIBUTE — "PASSE O KIT — JOGADOR X"
 * ======================================================================= */

static void dist_reveal_cb(lv_event_t *e)
{
    (void)e;
    beep_tap();
    s_game.phase = FORA_PHASE_REVEAL;
    build_reveal();
}

static void build_distribute(void)
{
    lv_obj_t *scr = new_screen();

    /* Central column */
    lv_obj_t *col = plain_box(scr);
    lv_obj_set_size(col, F_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, 16, 0);
    lv_obj_center(col);

    /* PASSE O KIT (ou PEGUE O KIT para o primeiro) */
    if (s_game.current_player == 0) {
        add_label(col, "PEGUE O KIT", KIT_COLOR_TEXT_MUTED, &kit_mono_20, 2);
    } else {
        add_label(col, "PASSE O KIT", KIT_COLOR_TEXT_MUTED, &kit_mono_20, 2);
    }

    /* JOGADOR X */
    char buf[20];
    snprintf(buf, sizeof(buf), "JOGADOR %d", s_game.current_player + 1);
    add_label(col, buf, KIT_COLOR_TEXT, &kit_display_44, 2);

    /* Botão REVELAR */
    make_pill_btn(scr, "REVELAR", dist_reveal_cb, NULL);

    show_screen(scr);
}

/* =======================================================================
 * PHASE: REVEAL — Jogador vê sua palavra (ou FORA)
 * ======================================================================= */

static void reveal_hide_cb(lv_event_t *e)
{
    (void)e;
    beep_tap();

    /* Próximo jogador ou ALL_READY */
    s_game.current_player++;
    if (s_game.current_player < s_game.num_players) {
        s_game.phase = FORA_PHASE_DISTRIBUTE;
        build_distribute();
    } else {
        s_game.phase = FORA_PHASE_ALL_READY;
        build_all_ready();
    }
}

static void build_reveal(void)
{
    lv_obj_t *scr = new_screen();

    lv_obj_t *col = plain_box(scr);
    lv_obj_set_size(col, F_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, 12, 0);
    lv_obj_center(col);

    bool is_fora = (s_game.current_player == s_game.fora_player);

    if (is_fora) {
        /* FORA */
        add_label(col, "VOC\xC3\x8A EST\xC3\x81", KIT_COLOR_TEXT_MUTED, &kit_mono_20, 2);

        lv_obj_t *fora_lbl = add_label(col, "FORA", F_ACCENT, &kit_display_72, 4);
        lv_obj_set_width(fora_lbl, F_CONTENT);
        lv_obj_set_style_text_align(fora_lbl, LV_TEXT_ALIGN_CENTER, 0);

        add_label(col, "N\xC3\x83O SABE A PALAVRA", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);
    } else {
        /* Palavra secreta */
        add_label(col, "SUA PALAVRA", KIT_COLOR_TEXT_MUTED, &kit_mono_20, 2);

        const char *word = fora_game_get_word(&s_game);
        lv_obj_t *word_lbl = add_label_wrap(col, word, KIT_COLOR_TEXT, &kit_display_44, 2);
        (void)word_lbl;
    }

    beep_reveal();

    /* Botão OCULTAR */
    make_pill_btn(scr, "OCULTAR", reveal_hide_cb, NULL);

    show_screen(scr);
}

/* =======================================================================
 * PHASE: ALL_READY — "TODOS PRONTOS?"
 * ======================================================================= */

static void ready_start_cb(lv_event_t *e)
{
    (void)e;
    beep_tap();

    /* Gera pares e inicia perguntas */
    s_game.current_round = 0;
    fora_game_generate_pairs(&s_game, s_api ? s_api->random : NULL);
    s_game.current_pair = 0;
    s_game.phase = FORA_PHASE_QUESTION;
    build_question();
}

static void build_all_ready(void)
{
    lv_obj_t *scr = new_screen();

    lv_obj_t *col = plain_box(scr);
    lv_obj_set_size(col, F_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, 20, 0);
    lv_obj_center(col);

    add_label(col, "TODOS", KIT_COLOR_TEXT_MUTED, &kit_mono_26, 3);

    lv_obj_t *tl = add_label(col, "PRONTOS?", KIT_COLOR_TEXT, &kit_display_44, 2);
    lv_obj_set_width(tl, F_CONTENT);
    lv_obj_set_style_text_align(tl, LV_TEXT_ALIGN_CENTER, 0);

    make_pill_btn(scr, "COME\xC3\x87" "AR", ready_start_cb, NULL);

    show_screen(scr);
}

/* =======================================================================
 * PHASE: QUESTION — "JOGADOR X PERGUNTE PARA JOGADOR Y"
 * ======================================================================= */

static void question_next_cb(lv_event_t *e)
{
    (void)e;
    beep_tap();

    s_game.current_pair++;
    if (s_game.current_pair < s_game.num_pairs) {
        /* Próximo par */
        build_question();
    } else {
        /* Fim da rodada */
        s_game.phase = FORA_PHASE_ROUND_END;
        build_round_end();
    }
}

static void build_question(void)
{
    lv_obj_t *scr = new_screen();

    int from = s_game.pairs_from[s_game.current_pair];
    int to   = s_game.pairs_to[s_game.current_pair];

    lv_obj_t *col = plain_box(scr);
    lv_obj_set_size(col, F_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, 12, 0);
    lv_obj_center(col);

    /* Indicador de rodada */
    char round_buf[32];
    snprintf(round_buf, sizeof(round_buf), "RODADA %d  \xC2\xB7  %d/%d",
             s_game.current_round + 1,
             s_game.current_pair + 1, s_game.num_pairs);
    add_label(col, round_buf, KIT_COLOR_TEXT_MUTED, &kit_mono_16, 1);

    /* JOGADOR X */
    char from_buf[20];
    snprintf(from_buf, sizeof(from_buf), "JOGADOR %d", from + 1);
    add_label(col, from_buf, F_ACCENT, &kit_display_44, 2);

    /* PERGUNTE PARA */
    add_label(col, "PERGUNTE PARA", KIT_COLOR_TEXT_MUTED, &kit_mono_20, 2);

    /* JOGADOR Y */
    char to_buf[20];
    snprintf(to_buf, sizeof(to_buf), "JOGADOR %d", to + 1);
    add_label(col, to_buf, KIT_COLOR_TEXT, &kit_display_44, 2);

    make_pill_btn(scr, "CONTINUAR", question_next_cb, NULL);

    show_screen(scr);
}

/* =======================================================================
 * PHASE: ROUND_END — "RODADA X CONCLUÍDA"
 * ======================================================================= */

static void round_end_cb(lv_event_t *e)
{
    (void)e;
    beep_tap();

    /* Salva pares e verifica se há mais rodadas */
    fora_game_save_prev_pairs(&s_game);
    s_game.current_round++;

    if (s_game.current_round < s_game.num_rounds) {
        /* Nova rodada */
        fora_game_generate_pairs(&s_game, s_api ? s_api->random : NULL);
        s_game.current_pair = 0;
        s_game.phase = FORA_PHASE_QUESTION;
        build_question();
    } else {
        /* Votação */
        s_game.phase = FORA_PHASE_VOTE;
        build_vote();
    }
}

static void build_round_end(void)
{
    lv_obj_t *scr = new_screen();

    lv_obj_t *col = plain_box(scr);
    lv_obj_set_size(col, F_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, 16, 0);
    lv_obj_center(col);

    char buf[32];
    snprintf(buf, sizeof(buf), "RODADA %d", s_game.current_round + 1);
    add_label(col, buf, KIT_COLOR_TEXT, &kit_display_44, 2);

    add_label(col, "CONCLU\xC3\x8D" "DA", KIT_COLOR_TEXT_MUTED, &kit_mono_26, 3);

    if (s_game.current_round + 1 < s_game.num_rounds) {
        make_pill_btn(scr, "PR\xC3\x93XIMA RODADA", round_end_cb, NULL);
    } else {
        make_pill_btn(scr, "HORA DE VOTAR", round_end_cb, NULL);
    }

    show_screen(scr);
}

/* =======================================================================
 * PHASE: VOTE — "QUEM RECEBEU A MAIORIA?"
 * ======================================================================= */

static void vote_player_cb(lv_event_t *e)
{
    int player = (int)(intptr_t)lv_event_get_user_data(e);
    beep_tap();

    s_game.voted_player = player;
    s_game.phase = FORA_PHASE_VOTE_REVEAL;
    build_vote_reveal();
}

static void build_vote(void)
{
    lv_obj_t *scr = new_screen();

    /* Header */
    lv_obj_t *header = add_label(scr, "QUEM RECEBEU\nA MAIORIA?",
                                 KIT_COLOR_TEXT_MUTED, &kit_mono_20, 2);
    lv_label_set_long_mode(header, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(header, F_CONTENT);
    lv_obj_set_style_text_align(header, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 20);

    /* Lista de jogadores (scrollável) */
    lv_obj_t *list = lv_obj_create(scr);
    lv_obj_remove_style_all(list);
    int list_top = 90;
    int list_h = F_SCREEN_H - list_top - 16;
    lv_obj_set_size(list, F_CONTENT, list_h);
    lv_obj_set_pos(list, F_PAD, list_top);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, 6, 0);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);

    for (int i = 0; i < s_game.num_players; i++) {
        lv_obj_t *btn = lv_obj_create(list);
        lv_obj_set_size(btn, F_CONTENT, 56);
        lv_obj_set_style_bg_color(btn, lv_color_hex(KIT_COLOR_SURFACE), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_80, LV_STATE_PRESSED);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 16, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(btn, vote_player_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);

        char buf[20];
        snprintf(buf, sizeof(buf), "JOGADOR %d", i + 1);
        lv_obj_t *l = add_label(btn, buf, KIT_COLOR_TEXT, &kit_mono_20, 1);
        lv_obj_center(l);
    }

    show_screen(scr);
}

/* =======================================================================
 * PHASE: VOTE_REVEAL — Animação de suspense + revelação
 * ======================================================================= */

static lv_obj_t *s_vr_result_lbl = NULL;
static lv_obj_t *s_vr_sub_lbl = NULL;

static void vote_reveal_tick_cb(lv_timer_t *t)
{
    (void)t;
    s_anim_tick++;

    if (s_anim_tick < F_VOTE_TICKS) {
        /* Flicker — mostra nomes aleatórios */
        int r = 0;
        if (s_api && s_api->random) r = s_api->random->range(0, s_game.num_players - 1);
#ifdef KIT_SDK_STUBS
        else                        r = rand() % s_game.num_players;
#endif
        char buf[20];
        snprintf(buf, sizeof(buf), "JOGADOR %d", r + 1);
        lv_label_set_text(s_vr_result_lbl, buf);
        beep_suspense();

        /* Desacelera */
        uint32_t p = F_VOTE_MS_MIN +
            (uint32_t)(F_VOTE_MS_MAX - F_VOTE_MS_MIN) * s_anim_tick / (F_VOTE_TICKS - 1);
        lv_timer_set_period(s_anim_timer, p);
        return;
    }

    /* Trava no resultado real */
    kill_anim();

    char buf[20];
    snprintf(buf, sizeof(buf), "JOGADOR %d", s_game.voted_player + 1);
    lv_label_set_text(s_vr_result_lbl, buf);

    bool correct = fora_game_vote_correct(&s_game);

    if (correct) {
        lv_obj_set_style_text_color(s_vr_result_lbl, lv_color_hex(F_ACCENT), 0);
        lv_label_set_text(s_vr_sub_lbl, "ERA O FORA!");
        beep_success();

        /* Após 2s, vai para o chute final */
        s_game.phase = FORA_PHASE_FINAL_GUESS;
        fora_game_generate_guess(&s_game, s_api ? s_api->random : NULL);
        s_anim_timer = lv_timer_create(
            (lv_timer_cb_t)(void *)build_final_guess, 2000, NULL);
        lv_timer_set_repeat_count(s_anim_timer, 1);
    } else {
        lv_obj_set_style_text_color(s_vr_result_lbl, lv_color_hex(KIT_COLOR_TEXT), 0);
        lv_label_set_text(s_vr_sub_lbl, "N\xC3\x83O ERA O FORA");
        beep_fail();

        /* Após 2s, FORA escapou */
        s_game.phase = FORA_PHASE_FORA_ESCAPED;
        s_game.fora_won = true;
        s_anim_timer = lv_timer_create(
            (lv_timer_cb_t)(void *)build_fora_escaped, 2000, NULL);
        lv_timer_set_repeat_count(s_anim_timer, 1);
    }
}

static void build_vote_reveal(void)
{
    lv_obj_t *scr = new_screen();

    lv_obj_t *col = plain_box(scr);
    lv_obj_set_size(col, F_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, 16, 0);
    lv_obj_center(col);

    add_label(col, "A MAIORIA ESCOLHEU", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);

    s_vr_result_lbl = add_label(col, "...", KIT_COLOR_TEXT_MUTED, &kit_display_44, 2);
    lv_obj_set_width(s_vr_result_lbl, F_CONTENT);
    lv_obj_set_style_text_align(s_vr_result_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_vr_result_lbl, LV_LABEL_LONG_WRAP);

    s_vr_sub_lbl = add_label(col, "", KIT_COLOR_TEXT_MUTED, &kit_mono_20, 2);
    lv_obj_set_width(s_vr_sub_lbl, F_CONTENT);
    lv_obj_set_style_text_align(s_vr_sub_lbl, LV_TEXT_ALIGN_CENTER, 0);

    /* Inicia animação de suspense */
    s_anim_tick = 0;
    s_anim_timer = lv_timer_create(vote_reveal_tick_cb, F_VOTE_MS_MIN, NULL);

    show_screen(scr);
}

/* =======================================================================
 * PHASE: FORA_ESCAPED — o FORA venceu (maioria errou)
 * ======================================================================= */

static void fora_escaped_continue_cb(lv_event_t *e)
{
    (void)e;
    beep_tap();
    s_game.phase = FORA_PHASE_RESULT;
    build_result();
}

static void build_fora_escaped(void)
{
    lv_obj_t *scr = new_screen();

    lv_obj_t *col = plain_box(scr);
    lv_obj_set_size(col, F_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, 16, 0);
    lv_obj_center(col);

    add_label(col, "O FORA", KIT_COLOR_TEXT_MUTED, &kit_mono_26, 3);

    lv_obj_t *esc = add_label(col, "ESCAPOU!", F_ACCENT, &kit_display_44, 2);
    lv_obj_set_width(esc, F_CONTENT);
    lv_obj_set_style_text_align(esc, LV_TEXT_ALIGN_CENTER, 0);

    make_pill_btn(scr, "CONTINUAR", fora_escaped_continue_cb, NULL);

    show_screen(scr);
}

/* =======================================================================
 * PHASE: FINAL_GUESS — 4 opções
 * ======================================================================= */

static void guess_option_cb(lv_event_t *e)
{
    int chosen = (int)(intptr_t)lv_event_get_user_data(e);
    beep_tap();

    fora_game_check_guess(&s_game, chosen);
    s_game.phase = FORA_PHASE_GUESS_RESULT;
    build_guess_result();
}

static void build_final_guess(void)
{
    lv_obj_t *scr = new_screen();
    kill_anim();  /* Garante que o timer de transição é limpo */

    /* Header */
    add_label(scr, "\xC3\x9ALTIMA CHANCE", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2)
        ->user_data = NULL;
    lv_obj_align(lv_obj_get_child(scr, 0), LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t *title = add_label(scr, "QUAL ERA\nA PALAVRA?", F_ACCENT, &kit_mono_26, 2);
    lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(title, F_CONTENT);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 44);

    /* 4 opções como botões grandes (2 × 2 grid) */
    lv_obj_t *grid = plain_box(scr);
    lv_obj_set_size(grid, F_CONTENT, 220);
    lv_obj_align(grid, LV_ALIGN_BOTTOM_MID, 0, -F_BTN_MARGIN);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(grid, 8, 0);
    lv_obj_set_style_pad_row(grid, 8, 0);

    for (int i = 0; i < FORA_GUESS_OPTIONS; i++) {
        lv_obj_t *btn = lv_obj_create(grid);
        int btn_w = (F_CONTENT - 8) / 2;
        lv_obj_set_size(btn, btn_w, 104);
        lv_obj_set_style_bg_color(btn, lv_color_hex(KIT_COLOR_SURFACE), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(F_ACCENT), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 20, 0);
        lv_obj_set_style_pad_all(btn, 8, 0);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(btn, guess_option_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);

        const char *word = fora_game_get_guess_word(&s_game, i);
        lv_obj_t *l = add_label(btn, word, KIT_COLOR_TEXT, &kit_mono_20, 1);
        lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(l, btn_w - 16);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(l);
    }

    show_screen(scr);
}

/* =======================================================================
 * PHASE: GUESS_RESULT — Resultado do chute
 * ======================================================================= */

static void guess_result_continue_cb(lv_event_t *e)
{
    (void)e;
    beep_tap();
    s_game.phase = FORA_PHASE_RESULT;
    build_result();
}

static void build_guess_result(void)
{
    lv_obj_t *scr = new_screen();

    lv_obj_t *col = plain_box(scr);
    lv_obj_set_size(col, F_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, 12, 0);
    lv_obj_center(col);

    if (s_game.fora_won) {
        beep_success();
        add_label(col, "VOC\xC3\x8A ACERTOU!", F_ACCENT, &kit_mono_26, 3);
    } else {
        beep_fail();
        add_label(col, "N\xC3\x83O ERA.", KIT_COLOR_TEXT_MUTED, &kit_mono_26, 3);
    }

    add_label(col, "A PALAVRA ERA", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);

    const char *word = fora_game_get_word(&s_game);
    lv_obj_t *wl = add_label_wrap(col, word, KIT_COLOR_TEXT, &kit_display_44, 2);
    (void)wl;

    if (s_game.fora_won) {
        add_label(col, "FORA VENCEU!", F_ACCENT, &kit_mono_20, 2);
    } else {
        add_label(col, "OS JOGADORES\nVENCERAM!", KIT_COLOR_GREEN, &kit_mono_20, 2);
        lv_obj_t *last = lv_obj_get_child(col, -1);
        lv_label_set_long_mode(last, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(last, F_CONTENT);
        lv_obj_set_style_text_align(last, LV_TEXT_ALIGN_CENTER, 0);
    }

    make_pill_btn(scr, "CONTINUAR", guess_result_continue_cb, NULL);

    show_screen(scr);
}

/* =======================================================================
 * PHASE: RESULT — Resumo final
 * ======================================================================= */

static void result_new_game_cb(lv_event_t *e)
{
    (void)e;
    beep_tap();
    /* Mantém configuração, reseta jogo */
    int players = s_game.num_players;
    int cat = s_game.category_index;
    int rounds = s_game.num_rounds;
    fora_game_reset(&s_game);
    s_game.num_players = players;
    s_game.category_index = cat;
    s_game.num_rounds = rounds;
    s_game.phase = FORA_PHASE_CONFIG;
    build_config();
}

static void result_replay_cb(lv_event_t *e)
{
    (void)e;
    beep_tap();
    /* Joga de novo com mesma configuração */
    int players = s_game.num_players;
    int cat = s_game.category_index;
    int rounds = s_game.num_rounds;
    fora_game_reset(&s_game);
    s_game.num_players = players;
    s_game.category_index = cat;
    s_game.num_rounds = rounds;

    fora_game_select_word(&s_game, s_api ? s_api->random : NULL);
    s_game.current_player = 0;
    s_game.phase = FORA_PHASE_DISTRIBUTE;
    build_distribute();
}

static void result_exit_cb(lv_event_t *e)
{
    (void)e;
    if (s_api && s_api->system) s_api->system->exit();
}

static void build_result(void)
{
    lv_obj_t *scr = new_screen();

    /* Scrollable container */
    lv_obj_t *cont = lv_obj_create(scr);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, F_SCREEN_W, F_SCREEN_H);
    lv_obj_set_pos(cont, 0, 0);
    lv_obj_set_style_pad_left(cont, F_PAD, 0);
    lv_obj_set_style_pad_right(cont, F_PAD, 0);
    lv_obj_set_style_pad_top(cont, 30, 0);
    lv_obj_set_style_pad_bottom(cont, 20, 0);
    lv_obj_set_style_pad_row(cont, 12, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(cont, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_AUTO);

    /* RESULTADO */
    add_label(cont, "RESULTADO", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 3);

    /* Quem venceu */
    if (s_game.fora_won) {
        lv_obj_t *wl = add_label(cont, "FORA VENCEU!", F_ACCENT, &kit_display_44, 2);
        lv_obj_set_width(wl, F_CONTENT);
        lv_obj_set_style_text_align(wl, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(wl, LV_LABEL_LONG_WRAP);
    } else {
        lv_obj_t *wl = add_label(cont, "JOGADORES\nVENCERAM!", KIT_COLOR_GREEN, &kit_display_44, 2);
        lv_obj_set_width(wl, F_CONTENT);
        lv_obj_set_style_text_align(wl, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(wl, LV_LABEL_LONG_WRAP);
    }

    /* Separador */
    lv_obj_t *sep = plain_box(cont);
    lv_obj_set_size(sep, F_CONTENT, 1);
    lv_obj_set_style_bg_color(sep, lv_color_hex(KIT_COLOR_LINE), 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);

    /* PALAVRA */
    add_label(cont, "PALAVRA", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);
    lv_obj_t *word_l = add_label_wrap(cont, fora_game_get_word(&s_game),
                                       KIT_COLOR_TEXT, &kit_mono_26, 2);
    (void)word_l;

    /* CATEGORIA */
    add_label(cont, "CATEGORIA", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);
    add_label(cont, fora_game_get_category_name(&s_game), KIT_COLOR_TEXT, &kit_mono_20, 1);

    /* FORA */
    add_label(cont, "FORA", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);
    char fora_buf[20];
    snprintf(fora_buf, sizeof(fora_buf), "JOGADOR %d", s_game.fora_player + 1);
    add_label(cont, fora_buf, F_ACCENT, &kit_mono_26, 2);

    /* Separador */
    lv_obj_t *sep2 = plain_box(cont);
    lv_obj_set_size(sep2, F_CONTENT, 1);
    lv_obj_set_style_bg_color(sep2, lv_color_hex(KIT_COLOR_LINE), 0);
    lv_obj_set_style_bg_opa(sep2, LV_OPA_COVER, 0);

    /* Botão JOGAR DE NOVO (accent pill) */
    lv_obj_t *replay_btn = lv_obj_create(cont);
    lv_obj_set_size(replay_btn, F_CONTENT, F_BTN_H);
    lv_obj_set_style_radius(replay_btn, F_BTN_H / 2, 0);
    lv_obj_set_style_border_width(replay_btn, 0, 0);
    lv_obj_set_style_shadow_width(replay_btn, 0, 0);
    lv_obj_set_style_pad_all(replay_btn, 0, 0);
    lv_obj_set_style_bg_color(replay_btn, lv_color_hex(F_ACCENT), 0);
    lv_obj_set_style_bg_opa(replay_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(replay_btn, LV_OPA_80, LV_STATE_PRESSED);
    lv_obj_clear_flag(replay_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(replay_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(replay_btn, result_replay_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *rl = add_label(replay_btn, "JOGAR DE NOVO", on_accent(), &kit_mono_20, 2);
    lv_obj_center(rl);

    /* Botão NOVA PARTIDA (surface) */
    lv_obj_t *new_btn = lv_obj_create(cont);
    lv_obj_set_size(new_btn, F_CONTENT, 56);
    lv_obj_set_style_radius(new_btn, 56 / 2, 0);
    lv_obj_set_style_border_width(new_btn, 0, 0);
    lv_obj_set_style_shadow_width(new_btn, 0, 0);
    lv_obj_set_style_pad_all(new_btn, 0, 0);
    lv_obj_set_style_bg_color(new_btn, lv_color_hex(KIT_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(new_btn, LV_OPA_COVER, 0);
    lv_obj_clear_flag(new_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(new_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(new_btn, result_new_game_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *nl = add_label(new_btn, "ALTERAR CONFIG", KIT_COLOR_TEXT, &kit_mono_16, 2);
    lv_obj_center(nl);

    /* Botão SAIR (texto simples) */
    lv_obj_t *exit_btn = lv_obj_create(cont);
    lv_obj_set_size(exit_btn, F_CONTENT, 48);
    lv_obj_remove_style_all(exit_btn);
    lv_obj_add_flag(exit_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(exit_btn, result_exit_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *el = add_label(exit_btn, "SAIR", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);
    lv_obj_center(el);

    show_screen(scr);
}

#endif /* KIT_SDK_STUBS */

#ifdef KIT_SDK_STUBS

kit_err_t tool_init(kit_tool_ctx_t *ctx)
{
    (void)ctx;
    return KIT_OK;
}

void tool_destroy(void)
{
}

#else /* KIT_SDK_STUBS */

/* =======================================================================
 * Tool lifecycle (Real)
 * ======================================================================= */

kit_err_t tool_init(kit_tool_ctx_t *ctx)
{
    if (!ctx || !ctx->api) return KIT_ERR_INVALID_ARG;
    s_api = ctx->api;

    printf("[FORA] tool_init (id=%s)\n", ctx->tool_id);

    /* Defaults */
    s_game.num_players    = 5;
    s_game.category_index = FORA_MIX_INDEX;
    s_game.num_rounds     = 2;
    fora_game_reset(&s_game);

    /* Restaura configuração salva */
    load_config();

    /* Mostra tela de configuração */
    s_game.phase = FORA_PHASE_CONFIG;
    build_config();

    return KIT_OK;
}

void tool_destroy(void)
{
    printf("[FORA] tool_destroy\n");
    kill_anim();
    /* O Runtime limpa todos os widgets LVGL automaticamente */
    s_screen = NULL;
    s_api = NULL;
}

#endif /* KIT_SDK_STUBS */
