#include "kit_placar.h"
#include "kit_api.h"
#include "kit_display.h"
#include "kit_fonts.h"
#include "kit_theme.h"
#include "esp_log.h"
#include "lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Placar — linguagem "Brutalist Bauhaus" (ver docs/design/design-language.md).
// Placar de mesa para qualquer jogo. Três páginas espelhando a Pavio/Bingo:
//   0 AJUSTE   — JOGADORES (2/3/4) · INICIAIS (opcional, padrão da Tool Fora:
//                seletor JOGADOR N + 3 caixas girando a letra) · META
//                (SEM/10/21/50/100). Tudo em Storage.
//   1 PLACAR   — o palco: 2 a 4 colunas, cada uma com a inicial/№ do jogador na
//                cor dele e a pontuação grande. TOQUE = +1, SEGURAR = -1. Barra
//                de progresso da meta no rodapé de cada coluna. Botão ZERAR
//                (dois toques) no rodapé. É a página inicial.
//   2 COMO USA — as regras curtas.
//
// A partida em andamento persiste (pl_score) — reabriu, continua de onde parou.
// Ao bater a meta, um overlay VENCEU anuncia UMA vez por jogador (s_announced);
// a mesa segue jogando (toque fora do botão) ou começa de novo (NOVA PARTIDA).
//
// Sem transform_scale/rotation e sem `opa` intermediário em container (força
// layer buffer e estoura o render no CO5300/PSRAM — regra da board). O flash da
// coluna ao pontuar usa só bg_opa na própria coluna (não cria layer).

static const char *TAG = "KIT_PLACAR";

// ---------------------------------------------------------------------------
// Layout (espelha as métricas da Pavio)
// ---------------------------------------------------------------------------
#define X_PAD         16
#define X_CONTENT     (KIT_DISPLAY_WIDTH - 2 * X_PAD)              // 336
#define X_CHIP        56
#define X_TITLEBAR    88
#define X_PAGE_H      (KIT_DISPLAY_HEIGHT - X_TITLEBAR)            // 360
#define X_FOOT_H      64
#define X_FOOT_MARGIN 14
#define X_BOARD_H     (X_PAGE_H - (X_FOOT_H + 2 * X_FOOT_MARGIN)) // 268
#define PAGES         3
#define NP_MIN        2
#define NP_MAX        4

#define FLASH_MS      150      // brilho da coluna ao pontuar
#define ARM_MS        2500     // janela do "toca de novo" do ZERAR

#define K_PLAYERS  "pl_players"
#define K_META     "pl_meta"
#define K_NAMES    "pl_names"
#define K_SCORE    "pl_score"

#define PL_NAME_LEN 4   // 3 letras + terminador

static const int32_t META_VALUES[] = { 0, 3, 5, 10, 21, 50 };
#define META_N ((int)(sizeof(META_VALUES) / sizeof(META_VALUES[0])))
static const char *const META_LBL[META_N]  = { "SEM", "3", "5", "10", "21", "50" };
static const char *const PLAYERS_LBL[3]     = { "2", "3", "4" };

// Uma cor Bauhaus por jogador (identidade da coluna) — as quatro primárias, sem
// repetir, mesmo com 4 jogadores.
static const uint32_t PLAYER_COLOR[NP_MAX] = {
    KIT_COLOR_RED, KIT_COLOR_GREEN, KIT_COLOR_YELLOW, KIT_COLOR_BLUE,
};

// ---------------------------------------------------------------------------
// Estado
// ---------------------------------------------------------------------------
static uint32_t s_accent = KIT_COLOR_GREEN;

static int  s_nplayers = NP_MIN;
static int  s_meta_idx = 0;                     // índice em META_VALUES
static int  s_score[NP_MAX];
static char s_name[NP_MAX][PL_NAME_LEN];        // "" → usar "#N"
static bool s_announced[NP_MAX];                // meta já anunciada p/ este jogador

static int  s_name_sel = 0;                     // jogador em edição no AJUSTE

static bool        s_zerar_armed = false;
static lv_timer_t *s_zerar_timer = NULL;
static lv_timer_t *s_flash_timer = NULL;

// ---------------------------------------------------------------------------
// Objetos LVGL
// ---------------------------------------------------------------------------
static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_tv     = NULL;
static lv_obj_t *s_tiles[PAGES];
static lv_obj_t *s_dots[PAGES];

// Página 0 — AJUSTE
static lv_obj_t *s_players_pills[3];  static lv_obj_t *s_players_lbls[3];
static lv_obj_t *s_meta_pills[META_N]; static lv_obj_t *s_meta_lbls[META_N];
static lv_obj_t *s_name_hdr   = NULL;   // "JOGADOR N"
static lv_obj_t *s_slot_lbl[3];
static lv_obj_t *s_name_clear = NULL;

// Página 1 — PLACAR
static lv_obj_t *s_board      = NULL;
static lv_obj_t *s_col[NP_MAX];
static lv_obj_t *s_col_hdr[NP_MAX];
static lv_obj_t *s_col_score[NP_MAX];
static lv_obj_t *s_col_bar[NP_MAX];     // trilho da meta
static lv_obj_t *s_col_fill[NP_MAX];    // preenchimento da meta
static lv_obj_t *s_zerar_btn = NULL;
static lv_obj_t *s_zerar_lbl = NULL;

// Overlay — VENCEU
static lv_obj_t *s_win_ov    = NULL;
static lv_obj_t *s_win_title = NULL;
static lv_obj_t *s_win_who   = NULL;
static lv_obj_t *s_win_btn   = NULL;
static lv_obj_t *s_win_btn_lbl = NULL;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const kit_api_table_t *api(void) { return kit_api_get_table(); }

static void beep(uint16_t freq, uint16_t ms)
{
    const kit_api_table_t *t = api();
    if (t && t->audio) t->audio->beep(freq, ms);
}

static void sfx(kit_sfx_t s)
{
    const kit_api_table_t *t = api();
    if (t && t->audio) t->audio->sfx(s);
}

static uint64_t millis(void)
{
    const kit_api_table_t *t = api();
    return (t && t->time) ? t->time->get_millis() : 0;
}

// Som de pontuar: os SFX prontos do kit_audio (envelope de 2 ms, dois tons,
// amplitude baixa) em vez de um beep() cru — o beep cru clicado em série
// estoura (fila de áudio de 6 lugares + ataque seco). CLICK sobe, BACK desce:
// já são um par "avança / volta". Rate-limitado a CLICK_GAP_MS para o som não
// atrasar do dedo nem formar backlog na fila; o feedback rápido de verdade é o
// brilho da coluna.
#define CLICK_GAP_MS 60
static uint64_t s_last_click = 0;

static void click_feedback(bool up)
{
    uint64_t now = millis();
    if (now - s_last_click < CLICK_GAP_MS) return;
    s_last_click = now;
    sfx(up ? KIT_SFX_CLICK : KIT_SFX_BACK);
}

static void keep_awake(bool on)
{
    const kit_api_table_t *t = api();
    if (t && t->power) t->power->keep_awake(on);
}

static int32_t meta_value(void) { return META_VALUES[s_meta_idx]; }

static uint32_t on_color(uint32_t c)
{
    return (c == KIT_COLOR_YELLOW) ? KIT_COLOR_ON_YELLOW : KIT_COLOR_ON_COLOR;
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

// Fonte da pontuação: quanto menos colunas, maior a fonte. Números negativos
// caem em mono_* (glifo '-' garantido; as fontes de display cobrem só " - 0-9
// A-Z Ã Ç Õ, mas nem toda tem o traço — mono sempre tem).
static const lv_font_t *score_font(int digits, bool neg)
{
    if (neg) return (s_nplayers <= 3) ? &kit_display_72 : &kit_mono_26;
    switch (s_nplayers) {
    case 2:  return (digits >= 3) ? &kit_display_72 : &kit_display_120;
    case 3:  return (digits >= 3) ? &kit_display_44 : &kit_display_72;
    default: return (digits >= 3) ? &kit_mono_26    : &kit_display_44;
    }
}

// ---------------------------------------------------------------------------
// Iniciais (padrão da Tool Fora)
// ---------------------------------------------------------------------------

// As 3 caixas do jogador `pl` como um array de char com ' ' onde está vazio.
static void name_slots(int pl, char o[3])
{
    const char *nm = s_name[pl];
    int L = (int)strlen(nm);
    for (int k = 0; k < 3; k++) o[k] = (k < L) ? nm[k] : ' ';
}

// Grava as 3 caixas de volta na sigla, aparando ' ' das pontas.
static void name_store(int pl, const char in[3])
{
    int a = 0, b = 2;
    while (a < 3 && in[a] == ' ') a++;
    while (b >= a && in[b] == ' ') b--;
    char *d = s_name[pl];
    int n = 0;
    for (int k = a; k <= b && k >= 0; k++) d[n++] = in[k];
    d[n] = 0;
}

static bool has_name(int pl) { return s_name[pl][0] != '\0'; }

// ---------------------------------------------------------------------------
// Persistência
// ---------------------------------------------------------------------------

static void save_prefs(void)
{
    const kit_api_table_t *t = api();
    if (!t || !t->storage) return;
    t->storage->set_i32(K_PLAYERS, s_nplayers);
    t->storage->set_i32(K_META, s_meta_idx);

    char buf[64];
    int w = 0;
    for (int i = 0; i < NP_MAX; i++)
        w += snprintf(buf + w, sizeof(buf) - w, "%s%s", i ? "/" : "", s_name[i]);
    t->storage->set_str(K_NAMES, buf);

    w = 0;
    for (int i = 0; i < NP_MAX; i++)
        w += snprintf(buf + w, sizeof(buf) - w, "%s%d", i ? "/" : "", s_score[i]);
    t->storage->set_str(K_SCORE, buf);
}

static void load_prefs(void)
{
    const kit_api_table_t *t = api();
    if (!t || !t->storage) return;

    int32_t v;
    if (t->storage->get_i32(K_PLAYERS, &v) == KIT_OK && v >= NP_MIN && v <= NP_MAX)
        s_nplayers = (int)v;
    if (t->storage->get_i32(K_META, &v) == KIT_OK && v >= 0 && v < META_N)
        s_meta_idx = (int)v;

    char buf[64];
    if (t->storage->get_str(K_NAMES, buf, sizeof(buf)) == KIT_OK) {
        int pl = 0, n = 0;
        for (const char *c = buf; pl < NP_MAX; c++) {
            if (*c == '/' || *c == '\0') {
                s_name[pl][n] = 0;
                pl++; n = 0;
                if (*c == '\0') break;
            } else if (n < PL_NAME_LEN - 1 && *c != ' ') {
                s_name[pl][n++] = *c;
            }
        }
    }
    if (t->storage->get_str(K_SCORE, buf, sizeof(buf)) == KIT_OK) {
        int pl = 0;
        char *p = buf;
        while (pl < NP_MAX && *p) {
            s_score[pl++] = (int)strtol(p, &p, 10);
            if (*p == '/') p++;
        }
        // Pontuação carregada já pode estar na meta — não reanuncia ao abrir.
        for (int i = 0; i < NP_MAX; i++)
            s_announced[i] = (meta_value() > 0 && s_score[i] >= meta_value());
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
        lv_obj_set_style_bg_color(s_dots[i], lv_color_hex(on ? s_accent : KIT_COLOR_LINE), 0);
        lv_obj_set_size(s_dots[i], on ? 20 : 8, 8);
    }
}

static void sync_seg(lv_obj_t **pills, lv_obj_t **lbls, int n, int sel)
{
    for (int i = 0; i < n; i++) {
        bool on = (i == sel);
        lv_obj_set_style_bg_color(pills[i], lv_color_hex(on ? s_accent : KIT_COLOR_SURFACE), 0);
        lv_obj_set_style_text_color(lbls[i],
            lv_color_hex(on ? on_color(s_accent) : KIT_COLOR_TEXT), 0);
    }
}

static void sync_name_editor(void)
{
    if (!s_name_hdr) return;
    if (s_name_sel >= s_nplayers) s_name_sel = 0;

    lv_label_set_text_fmt(s_name_hdr, "JOGADOR %d", s_name_sel + 1);

    char sl[3];
    name_slots(s_name_sel, sl);
    for (int k = 0; k < 3; k++) {
        char t[2] = { sl[k] == ' ' ? '-' : sl[k], 0 };
        lv_label_set_text(s_slot_lbl[k], t);
        lv_obj_set_style_text_color(s_slot_lbl[k],
            lv_color_hex(sl[k] == ' ' ? KIT_COLOR_TEXT_MUTED : KIT_COLOR_TEXT), 0);
    }
    if (has_name(s_name_sel)) lv_obj_clear_flag(s_name_clear, LV_OBJ_FLAG_HIDDEN);
    else                      lv_obj_add_flag(s_name_clear, LV_OBJ_FLAG_HIDDEN);
}

static void sync_segs(void)
{
    sync_seg(s_players_pills, s_players_lbls, 3, s_nplayers - NP_MIN);
    sync_seg(s_meta_pills, s_meta_lbls, META_N, s_meta_idx);
    sync_name_editor();
}

// Uma coluna do placar.
static void sync_col(int i)
{
    if (!s_col[i]) return;

    bool shown = (i < s_nplayers);
    if (shown) lv_obj_clear_flag(s_col[i], LV_OBJ_FLAG_HIDDEN);
    else       { lv_obj_add_flag(s_col[i], LV_OBJ_FLAG_HIDDEN); return; }

    uint32_t col = PLAYER_COLOR[i];

    if (has_name(i)) lv_label_set_text(s_col_hdr[i], s_name[i]);
    else             lv_label_set_text_fmt(s_col_hdr[i], "#%d", i + 1);
    lv_obj_set_style_text_color(s_col_hdr[i], lv_color_hex(col), 0);

    char num[12];
    snprintf(num, sizeof(num), "%d", s_score[i]);
    bool neg = (s_score[i] < 0);
    int digits = (int)strlen(num) - (neg ? 1 : 0);
    lv_label_set_text(s_col_score[i], num);
    lv_obj_set_style_text_font(s_col_score[i], score_font(digits, neg), 0);

    int32_t m = meta_value();
    if (m > 0) {
        lv_obj_clear_flag(s_col_bar[i], LV_OBJ_FLAG_HIDDEN);
        int pct = s_score[i] <= 0 ? 0 : (s_score[i] >= m ? 100 : (int)((int64_t)s_score[i] * 100 / m));
        lv_obj_set_width(s_col_fill[i], lv_pct(pct));
        lv_obj_set_style_bg_color(s_col_fill[i], lv_color_hex(col), 0);
    } else {
        lv_obj_add_flag(s_col_bar[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static void sync_board(void)
{
    for (int i = 0; i < NP_MAX; i++) sync_col(i);
    if (s_board) lv_obj_update_layout(s_board);
}

// ---------------------------------------------------------------------------
// Trava do arraste entre páginas (só durante o overlay VENCEU) — travado =
// tileview sem SCROLLABLE, como na Pavio.
// ---------------------------------------------------------------------------
static void tv_lock(bool locked)
{
    if (!s_tv) return;
    if (locked) lv_obj_clear_flag(s_tv, LV_OBJ_FLAG_SCROLLABLE);
    else        lv_obj_add_flag(s_tv, LV_OBJ_FLAG_SCROLLABLE);
}

// ---------------------------------------------------------------------------
// ZERAR — dois toques
// ---------------------------------------------------------------------------

static void zerar_disarm(void)
{
    s_zerar_armed = false;
    if (s_zerar_timer) { lv_timer_delete(s_zerar_timer); s_zerar_timer = NULL; }
    if (s_zerar_lbl) {
        lv_label_set_text(s_zerar_lbl, "ZERAR");
        lv_obj_set_style_text_color(s_zerar_lbl, lv_color_hex(KIT_COLOR_RED), 0);
    }
    if (s_zerar_btn) lv_obj_set_style_bg_opa(s_zerar_btn, LV_OPA_TRANSP, 0);
}

static void zerar_disarm_cb(lv_timer_t *t) { (void)t; zerar_disarm(); }

static void do_zerar(void)
{
    for (int i = 0; i < NP_MAX; i++) { s_score[i] = 0; s_announced[i] = false; }
    zerar_disarm();
    save_prefs();
    sync_board();
}

static void zerar_cb(lv_event_t *e)
{
    (void)e;
    bool empty = true;
    for (int i = 0; i < s_nplayers; i++) if (s_score[i] != 0) empty = false;
    if (empty) return;

    if (!s_zerar_armed) {
        s_zerar_armed = true;
        lv_label_set_text(s_zerar_lbl, "TOCA DE NOVO");
        lv_obj_set_style_text_color(s_zerar_lbl, lv_color_hex(KIT_COLOR_ON_COLOR), 0);
        lv_obj_set_style_bg_opa(s_zerar_btn, LV_OPA_COVER, 0);
        s_zerar_timer = lv_timer_create(zerar_disarm_cb, ARM_MS, NULL);
        lv_timer_set_repeat_count(s_zerar_timer, 1);
        beep(520, 14);
        return;
    }
    do_zerar();
    sfx(KIT_SFX_BACK);
}

// ---------------------------------------------------------------------------
// Overlay VENCEU
// ---------------------------------------------------------------------------

static void announce(int i)
{
    s_announced[i] = true;
    uint32_t col = PLAYER_COLOR[i];
    uint32_t ink = on_color(col);

    lv_obj_set_style_bg_color(s_win_ov, lv_color_hex(col), 0);
    lv_obj_set_style_text_color(s_win_title, lv_color_hex(ink), 0);
    lv_obj_set_style_text_color(s_win_who, lv_color_hex(ink), 0);
    lv_obj_set_style_border_color(s_win_btn, lv_color_hex(ink), 0);
    lv_obj_set_style_text_color(s_win_btn_lbl, lv_color_hex(ink), 0);

    if (has_name(i)) lv_label_set_text(s_win_who, s_name[i]);
    else             lv_label_set_text_fmt(s_win_who, "JOGADOR %d", i + 1);

    lv_obj_clear_flag(s_win_ov, LV_OBJ_FLAG_HIDDEN);
    tv_lock(true);
    sfx(KIT_SFX_CONFIRM);
}

// Toque fora do botão: fecha o aviso e segue jogando (pontuação intacta; não
// reanuncia este jogador até um ZERAR).
static void win_bg_cb(lv_event_t *e)
{
    (void)e;   // sem EVENT_BUBBLE: só dispara no fundo, não no botão
    lv_obj_add_flag(s_win_ov, LV_OBJ_FLAG_HIDDEN);
    tv_lock(false);
}

static void win_newgame_cb(lv_event_t *e)
{
    (void)e;
    lv_obj_add_flag(s_win_ov, LV_OBJ_FLAG_HIDDEN);
    tv_lock(false);
    do_zerar();
    sfx(KIT_SFX_BACK);
}

// ---------------------------------------------------------------------------
// Pontuar
// ---------------------------------------------------------------------------

static void unflash_cb(lv_timer_t *t)
{
    (void)t;
    s_flash_timer = NULL;
    for (int i = 0; i < NP_MAX; i++)
        if (s_col[i]) lv_obj_set_style_bg_opa(s_col[i], LV_OPA_TRANSP, 0);
}

static void add_point(int i, int delta)
{
    if (i < 0 || i >= s_nplayers) return;

    int v = s_score[i] + delta;
    if (v > 999)  v = 999;
    if (v < -99)  v = -99;
    if (v == s_score[i]) return;
    s_score[i] = v;

    // brilho curto na coluna
    lv_obj_set_style_bg_color(s_col[i], lv_color_hex(PLAYER_COLOR[i]), 0);
    lv_obj_set_style_bg_opa(s_col[i], LV_OPA_30, 0);
    if (s_flash_timer) lv_timer_delete(s_flash_timer);
    s_flash_timer = lv_timer_create(unflash_cb, FLASH_MS, NULL);
    lv_timer_set_repeat_count(s_flash_timer, 1);

    click_feedback(delta > 0);

    sync_col(i);
    lv_obj_update_layout(s_board);
    save_prefs();

    int32_t m = meta_value();
    if (delta > 0 && m > 0 && s_score[i] >= m && !s_announced[i])
        announce(i);
}

static void col_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_SHORT_CLICKED)      add_point(i, +1);
    else if (code == LV_EVENT_LONG_PRESSED)  add_point(i, -1);
}

// ---------------------------------------------------------------------------
// Callbacks — titlebar / tileview / AJUSTE
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
    if (s_win_ov && !lv_obj_has_flag(s_win_ov, LV_OBJ_FLAG_HIDDEN) &&
        s_tv && lv_tileview_get_tile_active(s_tv) != s_tiles[1])
        lv_tileview_set_tile_by_index(s_tv, 1, 0, LV_ANIM_OFF);
    if (s_zerar_armed) zerar_disarm();
    sync_dots();
}

static void players_cb(lv_event_t *e)
{
    int v = (int)(intptr_t)lv_event_get_user_data(e) + NP_MIN;
    if (v == s_nplayers) return;
    s_nplayers = v;
    if (s_name_sel >= s_nplayers) s_name_sel = 0;
    sync_segs();
    sync_board();
    save_prefs();
}

static void meta_cb(lv_event_t *e)
{
    int v = (int)(intptr_t)lv_event_get_user_data(e);
    if (v == s_meta_idx) return;
    s_meta_idx = v;
    // Meta nova: revalida o "já anunciado" de cada jogador.
    for (int i = 0; i < NP_MAX; i++)
        s_announced[i] = (meta_value() > 0 && s_score[i] >= meta_value());
    sync_segs();
    sync_board();
    save_prefs();
}

static void name_sel_cb(lv_event_t *e)
{
    int d = (int)(intptr_t)lv_event_get_user_data(e);
    s_name_sel = (s_name_sel + d + s_nplayers) % s_nplayers;
    sync_name_editor();
    beep(660, 12);
}

static void slot_cb(lv_event_t *e)
{
    int k = (int)(intptr_t)lv_event_get_user_data(e);
    char sl[3];
    name_slots(s_name_sel, sl);
    char c = sl[k];
    sl[k] = (c == ' ') ? 'A' : (c == 'Z') ? ' ' : (char)(c + 1);
    name_store(s_name_sel, sl);
    sync_name_editor();
    sync_col(s_name_sel);
    lv_obj_update_layout(s_board);
    save_prefs();
    beep(760, 12);
}

static void name_clear_cb(lv_event_t *e)
{
    (void)e;
    s_name[s_name_sel][0] = 0;
    sync_name_editor();
    sync_col(s_name_sel);
    lv_obj_update_layout(s_board);
    save_prefs();
    beep(480, 12);
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

    lv_obj_t *title = add_label(s_screen, "PLACAR", KIT_COLOR_TEXT, &kit_mono_26, 2);
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

static lv_obj_t *make_pill(lv_obj_t *parent, const char *txt, lv_event_cb_t cb,
                           int code, lv_obj_t **out_lbl, int height)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_set_height(c, height);
    lv_obj_set_flex_grow(c, 1);
    lv_obj_set_style_bg_color(c, lv_color_hex(KIT_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(c, 0, 0);
    lv_obj_set_style_radius(c, 15, 0);
    lv_obj_set_style_pad_all(c, 0, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(c, 4);
    lv_obj_add_event_cb(c, cb, LV_EVENT_CLICKED, (void *)(intptr_t)code);
    lv_obj_t *l = add_label(c, txt, KIT_COLOR_TEXT, &kit_mono_26, 1);
    lv_obj_center(l);
    if (out_lbl) *out_lbl = l;
    return c;
}

// Seletor de pílula. Com per_row < n as pílulas quebram em várias linhas (assim
// dá pra deixá-las maiores). height é a altura de cada pílula.
static void seg_grid(lv_obj_t *parent, const char *title, const char *const *opts,
                     int n, int per_row, int height, lv_event_cb_t cb,
                     lv_obj_t **pills, lv_obj_t **lbls)
{
    lv_obj_t *sec = plain_box(parent);
    lv_obj_set_size(sec, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(sec, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(sec, 12, 0);
    field_label(sec, title);

    lv_obj_t *rows = plain_box(sec);
    lv_obj_set_size(rows, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(rows, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(rows, 10, 0);

    lv_obj_t *row = NULL;
    for (int i = 0; i < n; i++) {
        if (i % per_row == 0) {
            row = plain_box(rows);
            lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
            lv_obj_set_style_pad_column(row, 10, 0);
        }
        pills[i] = make_pill(row, opts[i], cb, i, &lbls[i], height);
    }
}

static void seg_row(lv_obj_t *parent, const char *title, const char *const *opts,
                    int n, lv_event_cb_t cb, lv_obj_t **pills, lv_obj_t **lbls)
{
    seg_grid(parent, title, opts, n, n, 58, cb, pills, lbls);
}

static void build_name_editor(lv_obj_t *parent)
{
    lv_obj_t *sec = plain_box(parent);
    lv_obj_set_size(sec, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(sec, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(sec, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(sec, 14, 0);
    field_label(sec, "INICIAIS (OPCIONAL)");

    // Linha: [◄]  JOGADOR N  [►]
    lv_obj_t *pick = plain_box(sec);
    lv_obj_set_size(pick, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(pick, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pick, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (int s = 0; s < 2; s++) {
        lv_obj_t *b = lv_obj_create(pick);
        lv_obj_remove_style_all(b);
        lv_obj_set_size(b, 48, 48);
        lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(b, 10);
        lv_obj_add_event_cb(b, name_sel_cb, LV_EVENT_CLICKED, (void *)(intptr_t)(s ? 1 : -1));
        lv_obj_t *g = add_label(b, s ? KIT_ICON_CHEVRON : KIT_ICON_BACK,
                                KIT_COLOR_TEXT, &kit_display_44, 0);
        lv_obj_center(g);
        if (s == 0) {
            s_name_hdr = add_label(pick, "JOGADOR 1", KIT_COLOR_TEXT, &kit_mono_20, 1);
        }
    }

    // 3 caixas de letra
    lv_obj_t *slots = plain_box(sec);
    lv_obj_set_size(slots, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(slots, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(slots, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(slots, 12, 0);
    for (int k = 0; k < 3; k++) {
        lv_obj_t *box = lv_obj_create(slots);
        lv_obj_set_size(box, 92, 96);
        lv_obj_set_style_bg_color(box, lv_color_hex(KIT_COLOR_SURFACE), 0);
        lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(box, 0, 0);
        lv_obj_set_style_radius(box, 16, 0);
        lv_obj_set_style_pad_all(box, 0, 0);
        lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(box, 4);
        lv_obj_add_event_cb(box, slot_cb, LV_EVENT_CLICKED, (void *)(intptr_t)k);
        s_slot_lbl[k] = add_label(box, "-", KIT_COLOR_TEXT_MUTED, &kit_display_72, 0);
        lv_obj_center(s_slot_lbl[k]);
    }

    // APAGAR
    s_name_clear = lv_obj_create(sec);
    lv_obj_set_size(s_name_clear, lv_pct(100), 56);
    lv_obj_set_style_bg_color(s_name_clear, lv_color_hex(KIT_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(s_name_clear, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_name_clear, 0, 0);
    lv_obj_set_style_radius(s_name_clear, 15, 0);
    lv_obj_set_style_pad_all(s_name_clear, 0, 0);
    lv_obj_clear_flag(s_name_clear, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_name_clear, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_name_clear, name_clear_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_center(add_label(s_name_clear, "APAGAR", KIT_COLOR_TEXT_MUTED, &kit_mono_20, 2));
}

static void build_page_adjust(lv_obj_t *tile)
{
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_t *p = lv_obj_create(tile);
    lv_obj_remove_style_all(p);
    lv_obj_set_size(p, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_left(p, X_PAD, 0);
    lv_obj_set_style_pad_right(p, X_PAD, 0);
    lv_obj_set_style_pad_top(p, 22, 0);
    lv_obj_set_style_pad_bottom(p, 32, 0);
    lv_obj_set_style_pad_row(p, 30, 0);
    lv_obj_set_flex_flow(p, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(p, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(p, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(p, LV_SCROLLBAR_MODE_AUTO);

    seg_row(p, "JOGADORES", PLAYERS_LBL, 3, players_cb, s_players_pills, s_players_lbls);
    build_name_editor(p);
    // META tem 6 opções — quebra em 2 linhas de 3 pra caber botão maior.
    seg_grid(p, "META", META_LBL, META_N, 3, 68, meta_cb, s_meta_pills, s_meta_lbls);
}

static void build_page_board(lv_obj_t *tile)
{
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_t *box = lv_obj_create(tile);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    // Faixa de colunas
    s_board = plain_box(box);
    lv_obj_set_size(s_board, KIT_DISPLAY_WIDTH, X_BOARD_H);
    lv_obj_set_pos(s_board, 0, 0);
    lv_obj_set_flex_flow(s_board, LV_FLEX_FLOW_ROW);

    for (int i = 0; i < NP_MAX; i++) {
        lv_obj_t *c = lv_obj_create(s_board);
        lv_obj_remove_style_all(c);
        lv_obj_set_height(c, lv_pct(100));
        lv_obj_set_flex_grow(c, 1);
        lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(c, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(c, 10, 0);
        lv_obj_set_style_pad_ver(c, 16, 0);
        lv_obj_set_style_bg_color(c, lv_color_hex(PLAYER_COLOR[i]), 0);
        lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
        lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
        if (i > 0) {
            lv_obj_set_style_border_side(c, LV_BORDER_SIDE_LEFT, 0);
            lv_obj_set_style_border_width(c, 2, 0);
            lv_obj_set_style_border_color(c, lv_color_hex(KIT_COLOR_LINE), 0);
        }
        lv_obj_add_event_cb(c, col_cb, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(c, col_cb, LV_EVENT_LONG_PRESSED, (void *)(intptr_t)i);
        s_col[i] = c;

        s_col_hdr[i] = add_label(c, "#1", PLAYER_COLOR[i], &kit_mono_20, 1);

        s_col_score[i] = add_label(c, "0", KIT_COLOR_TEXT, &kit_display_72, 0);
        lv_obj_set_style_pad_ver(s_col_score[i], 6, 0);

        // Barra da meta (trilho + preenchimento)
        s_col_bar[i] = plain_box(c);
        lv_obj_set_size(s_col_bar[i], lv_pct(78), 5);
        lv_obj_set_style_radius(s_col_bar[i], 3, 0);
        lv_obj_set_style_bg_color(s_col_bar[i], lv_color_hex(KIT_COLOR_LINE), 0);
        lv_obj_set_style_bg_opa(s_col_bar[i], LV_OPA_COVER, 0);
        lv_obj_add_flag(s_col_bar[i], LV_OBJ_FLAG_HIDDEN);
        s_col_fill[i] = plain_box(s_col_bar[i]);
        lv_obj_set_size(s_col_fill[i], lv_pct(0), lv_pct(100));
        lv_obj_set_style_radius(s_col_fill[i], 3, 0);
        lv_obj_set_style_bg_color(s_col_fill[i], lv_color_hex(PLAYER_COLOR[i]), 0);
        lv_obj_set_style_bg_opa(s_col_fill[i], LV_OPA_COVER, 0);
        lv_obj_align(s_col_fill[i], LV_ALIGN_LEFT_MID, 0, 0);
    }

    // ZERAR — dois toques
    s_zerar_btn = lv_obj_create(box);
    lv_obj_set_size(s_zerar_btn, X_CONTENT, X_FOOT_H);
    lv_obj_set_style_radius(s_zerar_btn, X_FOOT_H / 2, 0);
    lv_obj_set_style_border_width(s_zerar_btn, 2, 0);
    lv_obj_set_style_border_color(s_zerar_btn, lv_color_hex(KIT_COLOR_RED), 0);
    lv_obj_set_style_bg_color(s_zerar_btn, lv_color_hex(KIT_COLOR_RED), 0);
    lv_obj_set_style_bg_opa(s_zerar_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(s_zerar_btn, 0, 0);
    lv_obj_set_style_pad_all(s_zerar_btn, 0, 0);
    lv_obj_clear_flag(s_zerar_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_zerar_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_zerar_btn, 8);
    lv_obj_align(s_zerar_btn, LV_ALIGN_BOTTOM_MID, 0, -X_FOOT_MARGIN);
    lv_obj_add_event_cb(s_zerar_btn, zerar_cb, LV_EVENT_CLICKED, NULL);
    s_zerar_lbl = add_label(s_zerar_btn, "ZERAR", KIT_COLOR_RED, &kit_mono_20, 3);
    lv_obj_center(s_zerar_lbl);
}

static void help_step(lv_obj_t *p, const char *title, const char *body)
{
    lv_obj_t *sec = plain_box(p);
    lv_obj_set_size(sec, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(sec, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(sec, 4, 0);

    add_label(sec, title, KIT_COLOR_TEXT, &kit_mono_26, 1);
    lv_obj_t *b = add_label(sec, body, KIT_COLOR_TEXT, &kit_mono_20, 1);
    lv_label_set_long_mode(b, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(b, X_CONTENT);
}

static void build_page_help(lv_obj_t *tile)
{
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_t *p = lv_obj_create(tile);
    lv_obj_remove_style_all(p);
    lv_obj_set_size(p, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_left(p, X_PAD, 0);
    lv_obj_set_style_pad_right(p, X_PAD, 0);
    lv_obj_set_style_pad_top(p, 20, 0);
    lv_obj_set_style_pad_bottom(p, 36, 0);
    lv_obj_set_style_pad_row(p, 24, 0);
    lv_obj_set_flex_flow(p, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(p, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scroll_dir(p, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(p, LV_SCROLLBAR_MODE_AUTO);

    add_label(p, "COMO USA", KIT_COLOR_TEXT_MUTED, &kit_mono_20, 3);

    help_step(p, "TOQUE = +1",
              "UM TOQUE NA COLUNA DO JOGADOR SOMA 1 PONTO.");
    help_step(p, "SEGURE = -1",
              "SEGURE A COLUNA PARA TIRAR 1 PONTO. VAI AT\xC3\x89 -99.");
    help_step(p, "META (OPCIONAL)",
              "NO AJUSTE: SEM META, 3, 5, 10, 21 OU 50. AO BATER, APARECE \"VENCEU\". "
              "TOQUE FORA DO BOT\xC3\x83O PARA SEGUIR JOGANDO.");
    help_step(p, "INICIAIS (OPCIONAL)",
              "NO AJUSTE: ESCOLHA O JOGADOR E TOQUE CADA CAIXA. CADA TOQUE AVAN\xC3\x87""A A "
              "LETRA (VAZIO, A, B ... Z E VOLTA). SEM ISSO, A COLUNA MOSTRA #1 A #4.");
    help_step(p, "ZERAR",
              "DOIS TOQUES NO BOT\xC3\x83O ZERAR LIMPAM O PLACAR. O N\xC3\x9AMERO DE "
              "JOGADORES, A META E AS INICIAIS FICAM.");
    help_step(p, "A PARTIDA N\xC3\x83O SOME",
              "FECHOU E ABRIU DE NOVO, O PLACAR CONTINUA DE ONDE PAROU.");
}

static void build_win_overlay(void)
{
    s_win_ov = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_win_ov);
    lv_obj_set_size(s_win_ov, KIT_DISPLAY_WIDTH, X_PAGE_H);
    lv_obj_set_pos(s_win_ov, 0, X_TITLEBAR);
    lv_obj_set_style_bg_color(s_win_ov, lv_color_hex(KIT_COLOR_GREEN), 0);
    lv_obj_set_style_bg_opa(s_win_ov, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_win_ov, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_win_ov, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_win_ov, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_win_ov, win_bg_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *col = plain_box(s_win_ov);
    lv_obj_set_size(col, KIT_DISPLAY_WIDTH, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, 8, 0);
    lv_obj_align(col, LV_ALIGN_CENTER, 0, -28);

    s_win_title = add_label(col, "VENCEU", KIT_COLOR_ON_COLOR, &kit_display_72, 0);
    s_win_who   = add_label(col, "JOGADOR 1", KIT_COLOR_ON_COLOR, &kit_mono_26, 2);

    s_win_btn = lv_obj_create(s_win_ov);
    lv_obj_set_size(s_win_btn, X_CONTENT, X_FOOT_H);
    lv_obj_set_style_radius(s_win_btn, X_FOOT_H / 2, 0);
    lv_obj_set_style_border_width(s_win_btn, 2, 0);
    lv_obj_set_style_border_color(s_win_btn, lv_color_hex(KIT_COLOR_ON_COLOR), 0);
    lv_obj_set_style_bg_opa(s_win_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(s_win_btn, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_set_style_pad_all(s_win_btn, 0, 0);
    lv_obj_clear_flag(s_win_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_win_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_win_btn, 8);
    lv_obj_align(s_win_btn, LV_ALIGN_BOTTOM_MID, 0, -X_FOOT_MARGIN);
    lv_obj_add_event_cb(s_win_btn, win_newgame_cb, LV_EVENT_CLICKED, NULL);
    s_win_btn_lbl = add_label(s_win_btn, "NOVA PARTIDA", KIT_COLOR_ON_COLOR, &kit_mono_20, 3);
    lv_obj_center(s_win_btn_lbl);
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
    s_tiles[2] = lv_tileview_add_tile(s_tv, 2, 0, LV_DIR_HOR);
    build_page_adjust(s_tiles[0]);
    build_page_board(s_tiles[1]);
    build_page_help(s_tiles[2]);
}

// ---------------------------------------------------------------------------
// Ciclo de vida
// ---------------------------------------------------------------------------

kit_err_t kit_placar_start(uint32_t accent)
{
    if (s_screen) kit_placar_destroy();

    ESP_LOGI(TAG, "Montando Placar...");
    s_accent   = accent ? accent : KIT_COLOR_GREEN;
    s_nplayers = NP_MIN;
    s_meta_idx = 0;
    s_name_sel = 0;
    s_zerar_armed = false;
    memset(s_score, 0, sizeof(s_score));
    memset(s_name, 0, sizeof(s_name));
    memset(s_announced, 0, sizeof(s_announced));
    load_prefs();

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(KIT_COLOR_BG), 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    build_titlebar();
    build_tileview();
    build_win_overlay();

    lv_tileview_set_tile_by_index(s_tv, 1, 0, LV_ANIM_OFF);   // começa no PLACAR
    lv_obj_update_layout(s_screen);

    sync_segs();
    sync_board();
    sync_dots();

    keep_awake(true);   // fica na mesa a partida inteira
    lv_screen_load(s_screen);
    return KIT_OK;
}

void kit_placar_destroy(void)
{
    ESP_LOGI(TAG, "Encerrando Placar.");
    if (s_zerar_timer) { lv_timer_delete(s_zerar_timer); s_zerar_timer = NULL; }
    if (s_flash_timer) { lv_timer_delete(s_flash_timer); s_flash_timer = NULL; }
    keep_awake(false);

    if (s_screen) { lv_obj_delete(s_screen); s_screen = NULL; }

    s_tv = NULL;
    for (int i = 0; i < PAGES; i++) { s_tiles[i] = NULL; s_dots[i] = NULL; }
    for (int i = 0; i < 3; i++) { s_players_pills[i] = NULL; s_players_lbls[i] = NULL; s_slot_lbl[i] = NULL; }
    for (int i = 0; i < META_N; i++) { s_meta_pills[i] = NULL; s_meta_lbls[i] = NULL; }
    for (int i = 0; i < NP_MAX; i++) {
        s_col[i] = s_col_hdr[i] = s_col_score[i] = NULL;
        s_col_bar[i] = s_col_fill[i] = NULL;
    }
    s_name_hdr = s_name_clear = NULL;
    s_board = s_zerar_btn = s_zerar_lbl = NULL;
    s_win_ov = s_win_title = s_win_who = s_win_btn = s_win_btn_lbl = NULL;
}
