#include "kit_bingo.h"
#include "kit_api.h"
#include "kit_display.h"
#include "kit_fonts.h"
#include "kit_theme.h"
#include "esp_log.h"
#include "lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Globo de Bingo — linguagem "Brutalist Bauhaus" (ver docs/design/design-language.md).
// Titlebar fixa + tileview de 4 páginas (arrasta na horizontal):
//   0 AJUSTE   — FAIXA (1–75 com letra B/I/N/G/O, ou 1–90) + REINICIAR SORTEIO
//                (dois toques para confirmar).
//   1 GLOBO    — o palco: número grande (kit_display_120) + letra da coluna +
//                contador. Botão SORTEAR (+ toque no palco, PWR físico,
//                chacoalhar). É a página inicial.
//   2 CHAMADAS — o painel inteiro da faixa: números já sorteados acesos na cor
//                da Tool, o último com um anel. Rola na vertical.
//   3 CARTELAS — QR pro gerador de cartelas (web-installer/bingo.html no GitHub
//                Pages, com ?bolas= conforme a FAIXA). Cada celular pega a sua
//                cartela. Incentiva papel e caneta, não mais tempo de tela.
//
// Sorteio sem reposição via Random API (TRNG). Diferente da Sortear Times, a
// **rodada persiste**: faixa e sorteados vão para o Storage (bingo_range /
// bingo_drawn) e voltam ao reabrir — um jogo de bingo dura. Trocar a faixa ou
// tocar REINICIAR zera o painel.
//
// Card verde na Home (TOOL_ICON_BINGO — quatro pontos). Texto sobre o verde =
// paper (KIT_COLOR_ON_COLOR).
//
// FONTES: o número vai em kit_display_120 (só dígitos + "-" "+"), que anima
// trocando texto — nunca transform_scale (o layer transformado animado estoura
// o render no CO5300/PSRAM e reinicia a board). O estado "FIM" cai em
// kit_display_72 (essa cobre A-Z). A letra da coluna e os rótulos vão em mono.
//
// Animação = o padrão validado da Dice/Times: um único lv_timer curto que só
// troca texto por tick.

static const char *TAG = "KIT_BINGO";

// ---------------------------------------------------------------------------
// Layout (espelha as métricas da Times / Dice)
// ---------------------------------------------------------------------------
#define X_PAD        16
#define X_CONTENT    (KIT_DISPLAY_WIDTH - 2 * X_PAD)              // 336
#define X_TITLEBAR   88
#define X_PAGE_H     (KIT_DISPLAY_HEIGHT - X_TITLEBAR)            // 360
#define X_CHIP       56
#define X_GO_H       76
#define X_GO_MARGIN  18
#define PAGES        4

#define BINGO_MAX    90

// Gerador de cartelas — página CARTELAS (QR). ?bolas= é preenchido com a FAIXA.
#define CARDS_URL    "https://jcrvlh.github.io/kit/bingo.html?bolas="

// Suspense curto do sorteio (só troca o texto do número).
#define SHUF_TICKS     10
#define SHUF_TICK_MS   60

#define RESET_ARM_MS   4000

#define K_RANGE  "bingo_range"
#define K_DRAWN  "bingo_drawn"
#define K_VIEW   "bingo_view"

// Página CHAMADAS: LISTA (só os sorteados, agrupados, fonte grande — melhor de
// ler numa tela de 1,8") ou GRADE (o painel inteiro da faixa, lv_table).
#define VIEW_LIST  0
#define VIEW_GRID  1

static const char LETTERS[5] = { 'B', 'I', 'N', 'G', 'O' };

// ---------------------------------------------------------------------------
// Estado
// ---------------------------------------------------------------------------
static int      s_range  = 75;                  // 75 ou 90
static int      s_view   = VIEW_LIST;            // CHAMADAS: LISTA ou GRADE
static uint32_t s_accent = KIT_COLOR_GREEN;

static int      s_drawn[BINGO_MAX];             // números na ordem em que saíram
static int      s_ndrawn = 0;
static bool     s_in[BINGO_MAX + 1];            // 1-indexado: já saiu?

static bool     s_drawing = false;
static int      s_tick    = 0;
static int      s_pick    = 0;
static lv_timer_t *s_timer = NULL;

static bool     s_reset_armed = false;
static lv_timer_t *s_reset_timer = NULL;

// ---------------------------------------------------------------------------
// Objetos LVGL
// ---------------------------------------------------------------------------
static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_tv     = NULL;
static lv_obj_t *s_tiles[PAGES];
static lv_obj_t *s_dots[PAGES];

// Página 0 — Ajuste
static lv_obj_t *s_range_pills[2];
static lv_obj_t *s_range_pill_lbls[2];
static lv_obj_t *s_reset_btn = NULL;
static lv_obj_t *s_reset_lbl = NULL;

// Página 1 — Globo
static lv_obj_t *s_letter_lbl = NULL;
static lv_obj_t *s_num_lbl    = NULL;
static lv_obj_t *s_prev_lbl   = NULL;
static lv_obj_t *s_count_lbl  = NULL;
static lv_obj_t *s_hint_lbl   = NULL;
static lv_obj_t *s_go_btn     = NULL;
static lv_obj_t *s_go_lbl     = NULL;

// Página 2 — Chamadas
static lv_obj_t *s_board_wrap = NULL;   // coluna: cabeçalho + toggle + as duas visões
static lv_obj_t *s_board_head = NULL;   // "SORTEADOS N / TOTAL"
static lv_obj_t *s_view_seg   = NULL;   // toggle LISTA / GRADE
static lv_obj_t *s_view_pills[2];
static lv_obj_t *s_view_pill_lbls[2];
static lv_obj_t *s_bingo_hdr  = NULL;   // linha de rótulos B/I/N/G/O (só GRADE + 1–75)
static lv_obj_t *s_table      = NULL;   // GRADE — 1 widget lv_table (células desenhadas)
static lv_obj_t *s_list       = NULL;   // LISTA — coluna rolável de linhas por grupo
static lv_obj_t *s_list_val[9];         // rótulo com os números sorteados de cada grupo

// Página 3 — Cartelas
static lv_obj_t *s_qr      = NULL;      // lv_qrcode com o link do gerador
static lv_obj_t *s_qr_tag  = NULL;      // "BINGO 1-75" / "1-90" abaixo do QR

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const kit_api_table_t *api(void) { return kit_api_get_table(); }

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

static void beep(uint16_t freq, uint16_t ms)
{
    const kit_api_table_t *t = api();
    if (t && t->audio) t->audio->beep(freq, ms);
}

static int rnd(int min, int max)
{
    const kit_api_table_t *t = api();
    if (t && t->random) return (int)t->random->range(min, max);
    return min + rand() % (max - min + 1);
}

// Letra da coluna do número (só na faixa 1–75). out precisa de 2 bytes.
static void letter_for(int n, char *out)
{
    if (s_range != 75 || n < 1 || n > 75) { out[0] = '\0'; return; }
    out[0] = LETTERS[(n - 1) / 15];
    out[1] = '\0';
}

// ---------------------------------------------------------------------------
// Persistência (Storage API) — a rodada inteira sobrevive a fechar a Tool
// ---------------------------------------------------------------------------

static void save_prefs(void)
{
    const kit_api_table_t *t = api();
    if (!t || !t->storage) return;
    t->storage->set_i32(K_RANGE, s_range);
    t->storage->set_i32(K_VIEW, s_view);

    char buf[400];
    buf[0] = '\0';
    size_t off = 0;
    for (int i = 0; i < s_ndrawn; i++) {
        int w = snprintf(buf + off, sizeof(buf) - off, i ? ",%d" : "%d", s_drawn[i]);
        if (w < 0 || (size_t)w >= sizeof(buf) - off) break;
        off += (size_t)w;
    }
    t->storage->set_str(K_DRAWN, buf);
}

static void load_prefs(void)
{
    const kit_api_table_t *t = api();
    if (!t || !t->storage) return;

    int32_t v;
    if (t->storage->get_i32(K_RANGE, &v) == KIT_OK && (v == 75 || v == 90))
        s_range = (int)v;
    if (t->storage->get_i32(K_VIEW, &v) == KIT_OK && (v == VIEW_LIST || v == VIEW_GRID))
        s_view = (int)v;

    char buf[400];
    if (t->storage->get_str(K_DRAWN, buf, sizeof(buf)) != KIT_OK) return;

    const char *p = buf;
    while (*p) {
        while (*p == ',' || *p == ' ') p++;
        if (*p < '0' || *p > '9') break;
        int n = atoi(p);
        while (*p >= '0' && *p <= '9') p++;
        if (n >= 1 && n <= s_range && !s_in[n] && s_ndrawn < s_range) {
            s_in[n] = true;
            s_drawn[s_ndrawn++] = n;
        }
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

// Página CARTELAS — refaz o QR e o rótulo com a FAIXA atual.
static void cards_sync(void)
{
    if (!s_qr) return;
    char url[sizeof(CARDS_URL) + 4];
    int n = snprintf(url, sizeof(url), CARDS_URL "%d", s_range);
    lv_qrcode_update(s_qr, url, n);
    if (s_qr_tag) lv_label_set_text_fmt(s_qr_tag, "BINGO 1-%d", s_range);
}

static void sync_seg(void)
{
    uint32_t sel_txt = on_accent();
    for (int i = 0; i < 2; i++) {
        bool sel = ((i == 0 ? 75 : 90) == s_range);
        lv_obj_set_style_bg_color(s_range_pills[i],
            lv_color_hex(sel ? s_accent : KIT_COLOR_SURFACE), 0);
        lv_obj_set_style_text_color(s_range_pill_lbls[i],
            lv_color_hex(sel ? sel_txt : KIT_COLOR_TEXT), 0);
    }
}

static void sync_globo(void)
{
    if (!s_num_lbl) return;

    int total = s_range;
    int n     = s_ndrawn;
    bool done = (n >= total);
    int cur   = n >= 1 ? s_drawn[n - 1] : 0;
    int prev  = n >= 2 ? s_drawn[n - 2] : 0;

    char lt[2];

    if (done) {
        lv_obj_set_style_text_font(s_num_lbl, &kit_display_72, 0);
        lv_label_set_text(s_num_lbl, "FIM");
        lv_obj_set_style_text_color(s_num_lbl, lv_color_hex(KIT_COLOR_TEXT_MUTED), 0);
        lv_label_set_text(s_letter_lbl, "");
    } else if (cur == 0) {
        lv_obj_set_style_text_font(s_num_lbl, &kit_display_120, 0);
        lv_label_set_text(s_num_lbl, "-");
        lv_obj_set_style_text_color(s_num_lbl, lv_color_hex(KIT_COLOR_TEXT_MUTED), 0);
        lv_label_set_text(s_letter_lbl, "");
    } else {
        lv_obj_set_style_text_font(s_num_lbl, &kit_display_120, 0);
        lv_label_set_text_fmt(s_num_lbl, "%d", cur);
        lv_obj_set_style_text_color(s_num_lbl, lv_color_hex(s_accent), 0);
        letter_for(cur, lt);
        lv_label_set_text(s_letter_lbl, lt);
    }

    if (prev) {
        letter_for(prev, lt);
        if (lt[0]) lv_label_set_text_fmt(s_prev_lbl, "ANTERIOR \xE2\x80\xA2 %s %d", lt, prev);
        else       lv_label_set_text_fmt(s_prev_lbl, "ANTERIOR \xE2\x80\xA2 %d", prev);
    } else {
        lv_label_set_text(s_prev_lbl, "");
    }

    lv_label_set_text_fmt(s_count_lbl, "%d / %d", n, total);

    if (done)
        lv_label_set_text(s_hint_lbl, "FAIXA COMPLETA \xE2\x80\xA2 REINICIE NO AJUSTE");
    else if (cur == 0)
        lv_label_set_text(s_hint_lbl, "SORTEAR \xE2\x80\xA2 PWR \xE2\x80\xA2 CHACOALHAR");
    else
        lv_label_set_text(s_hint_lbl, "TOQUE PARA SORTEAR");

    if (s_go_btn) {
        lv_obj_set_style_bg_opa(s_go_btn, done ? LV_OPA_40 : LV_OPA_COVER, 0);
        if (done) lv_obj_add_state(s_go_btn, LV_STATE_DISABLED);
        else      lv_obj_remove_state(s_go_btn, LV_STATE_DISABLED);
    }

    if (s_board_head)
        lv_label_set_text_fmt(s_board_head, "SORTEADOS %d / %d", n, total);
}

// ---------------------------------------------------------------------------
// Painel de chamadas — 1 widget `lv_table` (células desenhadas, não objetos).
// Montar a grade de 75/90 células como árvore de `lv_obj` (linhas flex +
// célula + label) estourava o layout do LVGL no CO5300/PSRAM: render > 5 s no
// primeiro desenho → task_wdt em loop, a placa travava ao abrir a Tool. O
// `lv_table` desenha as células direto, sem um objeto por célula; a aparência
// de cada uma sai do hook `LV_EVENT_DRAW_TASK_ADDED`, que lê `s_in[]` ao vivo.
// ---------------------------------------------------------------------------

static int cell_value(int r, int c)
{
    return (s_range == 75) ? c * 15 + r + 1 : c * 10 + r + 1;
}

static void table_draw_cb(lv_event_t *e)
{
    lv_draw_task_t *t = lv_event_get_draw_task(e);
    lv_draw_dsc_base_t *b = (lv_draw_dsc_base_t *)lv_draw_task_get_draw_dsc(t);
    if (!b || b->part != LV_PART_ITEMS) return;

    int val = cell_value((int)b->id1, (int)b->id2);
    if (val < 1 || val > s_range) return;

    bool hit  = s_in[val];
    int  last = s_ndrawn ? s_drawn[s_ndrawn - 1] : 0;

    lv_draw_fill_dsc_t *fd = lv_draw_task_get_fill_dsc(t);
    if (fd) {
        fd->color = lv_color_hex(hit ? s_accent : KIT_COLOR_SURFACE);
        fd->opa   = LV_OPA_COVER;
    }
    lv_draw_label_dsc_t *ld = lv_draw_task_get_label_dsc(t);
    if (ld) {
        // Número marcado = preto sobre a cor da Tool (contraste alto numa tela
        // de 1,8"); número ainda não sorteado = apagado.
        ld->color = lv_color_hex(hit ? KIT_COLOR_BG : KIT_COLOR_TEXT_MUTED);
        ld->align = LV_TEXT_ALIGN_CENTER;
    }
    lv_draw_border_dsc_t *bd = lv_draw_task_get_border_dsc(t);
    if (bd) {
        bool ring = (last && val == last);
        bd->color = lv_color_hex(ring ? KIT_COLOR_TEXT : KIT_COLOR_LINE);
        bd->width = ring ? 3 : 1;
        bd->opa   = LV_OPA_COVER;
    }
}

// ---- LISTA — só os números sorteados, agrupados, em fonte grande ----------
// 1–75: 5 linhas B/I/N/G/O. 1–90: 9 linhas por dezena (01·, 11·, …). Mostra só
// o que interessa e num corpo bem maior — é a visão de conferência pensada pra
// tela de 1,8".
static int list_groups(void) { return (s_range == 75) ? 5 : 9; }
static int list_span(void)   { return (s_range == 75) ? 15 : 10; }

static void paint_list(void)
{
    if (!s_list) return;
    int last = s_ndrawn ? s_drawn[s_ndrawn - 1] : 0;
    int span = list_span();

    for (int g = 0; g < list_groups(); g++) {
        char buf[128];
        size_t off = 0;
        bool has_last = false;
        buf[0] = '\0';
        for (int v = g * span + 1; v <= g * span + span && v <= s_range; v++) {
            if (!s_in[v]) continue;
            if (v == last) has_last = true;
            int w = snprintf(buf + off, sizeof(buf) - off, off ? "  %d" : "%d", v);
            if (w < 0 || (size_t)w >= sizeof(buf) - off) break;
            off += (size_t)w;
        }
        bool empty = (off == 0);
        if (empty) strcpy(buf, "\xC2\xB7");   // ·
        lv_label_set_text(s_list_val[g], buf);
        lv_obj_set_style_text_color(s_list_val[g],
            lv_color_hex(has_last ? s_accent
                                  : (empty ? KIT_COLOR_TEXT_MUTED : KIT_COLOR_TEXT)), 0);
    }
}

// GRADE: o hook da tabela lê s_in[] ao vivo → repintar é só invalidar.
// LISTA: os números estão no texto dos rótulos → recompor.
static void paint_board(void)
{
    if (s_table) lv_obj_invalidate(s_table);
    paint_list();
}

static void apply_view(void)
{
    bool grid = (s_view == VIEW_GRID);
    for (int i = 0; i < 2; i++) {
        bool sel = (i == s_view);
        lv_obj_set_style_bg_color(s_view_pills[i],
            lv_color_hex(sel ? s_accent : KIT_COLOR_SURFACE), 0);
        lv_obj_set_style_text_color(s_view_pill_lbls[i],
            lv_color_hex(sel ? KIT_COLOR_BG : KIT_COLOR_TEXT), 0);
    }
    if (s_table) {
        if (grid) lv_obj_clear_flag(s_table, LV_OBJ_FLAG_HIDDEN);
        else      lv_obj_add_flag(s_table, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_bingo_hdr) {
        if (grid && s_range == 75) lv_obj_clear_flag(s_bingo_hdr, LV_OBJ_FLAG_HIDDEN);
        else                       lv_obj_add_flag(s_bingo_hdr, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_list) {
        if (grid) lv_obj_add_flag(s_list, LV_OBJ_FLAG_HIDDEN);
        else      lv_obj_clear_flag(s_list, LV_OBJ_FLAG_HIDDEN);
    }
}

// (Re)constrói as duas visões — só ao abrir a Tool, trocar a faixa ou reiniciar.
static void board_build(void)
{
    if (!s_board_wrap || !s_table) return;

    int cols = (s_range == 75) ? 5 : 9;
    int rows = (s_range == 75) ? 15 : 10;
    int cw   = (X_CONTENT - (cols - 1)) / cols;   // -1: as bordas de 1 px somam ~1 col

    // -- GRADE: rótulos B/I/N/G/O acima da tabela (só na faixa 1–75) --
    if (s_bingo_hdr) { lv_obj_delete(s_bingo_hdr); s_bingo_hdr = NULL; }
    if (s_range == 75) {
        s_bingo_hdr = plain_box(s_board_wrap);
        lv_obj_set_size(s_bingo_hdr, X_CONTENT, 22);
        lv_obj_set_flex_flow(s_bingo_hdr, LV_FLEX_FLOW_ROW);
        for (int c = 0; c < cols; c++) {
            char s[2] = { LETTERS[c], '\0' };
            lv_obj_t *cap = plain_box(s_bingo_hdr);
            lv_obj_set_size(cap, cw, 22);
            lv_obj_t *l = add_label(cap, s, KIT_COLOR_TEXT, &kit_mono_20, 1);
            lv_obj_center(l);
        }
        lv_obj_move_to_index(s_bingo_hdr, 2);   // depois do cabeçalho e do toggle
    }

    lv_table_set_column_count(s_table, cols);
    lv_table_set_row_count(s_table, rows);
    for (int c = 0; c < cols; c++)
        lv_table_set_column_width(s_table, c, cw);
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++)
            lv_table_set_cell_value_fmt(s_table, r, c, "%d", cell_value(r, c));

    // -- LISTA: uma linha por grupo --
    if (s_list) { lv_obj_delete(s_list); s_list = NULL; }
    s_list = plain_box(s_board_wrap);
    // plain_box tira o LV_OBJ_FLAG_SCROLLABLE — a LISTA precisa rolar de volta.
    lv_obj_add_flag(s_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(s_list, X_CONTENT);
    lv_obj_set_flex_grow(s_list, 1);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_list, 16, 0);
    lv_obj_set_style_pad_bottom(s_list, 8, 0);   // respiro no fim da rolagem
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_AUTO);

    int span = list_span();
    for (int g = 0; g < list_groups(); g++) {
        lv_obj_t *row = plain_box(s_list);
        lv_obj_set_size(row, X_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_column(row, 12, 0);

        lv_obj_t *pre;
        if (s_range == 75) {
            char s[2] = { LETTERS[g], '\0' };
            pre = add_label(row, s, s_accent, &kit_mono_26, 2);
        } else {
            char s[8];
            snprintf(s, sizeof(s), "%02d", (g * span + 1) % 100);
            pre = add_label(row, s, KIT_COLOR_TEXT_MUTED, &kit_mono_16, 1);
        }
        lv_obj_set_width(pre, 34);

        s_list_val[g] = add_label(row, "\xC2\xB7", KIT_COLOR_TEXT_MUTED, &kit_mono_26, 2);
        lv_label_set_long_mode(s_list_val[g], LV_LABEL_LONG_WRAP);
        lv_obj_set_flex_grow(s_list_val[g], 1);
    }

    apply_view();
    paint_board();
}

// ---------------------------------------------------------------------------
// Sorteio
// ---------------------------------------------------------------------------

static void tick_cb(lv_timer_t *t)
{
    (void)t;
    if (++s_tick < SHUF_TICKS) {
        lv_label_set_text_fmt(s_num_lbl, "%d", rnd(1, s_range));
        return;
    }

    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    s_drawing = false;

    s_in[s_pick] = true;
    s_drawn[s_ndrawn++] = s_pick;
    save_prefs();
    sync_globo();
    paint_board();
}

static void start_draw(void)
{
    if (s_drawing || !s_screen) return;
    if (s_ndrawn >= s_range) { beep(300, 60); return; }

    if (s_tv) lv_tileview_set_tile_by_index(s_tv, 1, 0, LV_ANIM_OFF);

    // sorteio sem reposição
    int pool[BINGO_MAX];
    int np = 0;
    for (int v = 1; v <= s_range; v++) if (!s_in[v]) pool[np++] = v;
    s_pick = pool[rnd(0, np - 1)];

    s_drawing = true;
    s_tick = 0;
    lv_obj_set_style_text_font(s_num_lbl, &kit_display_120, 0);
    lv_obj_set_style_text_color(s_num_lbl, lv_color_hex(KIT_COLOR_TEXT), 0);
    lv_label_set_text(s_letter_lbl, "");

    const kit_api_table_t *t = api();
    if (t && t->audio) t->audio->sfx(KIT_SFX_BINGO_BALL);   // estalinho discreto → número saindo

    s_timer = lv_timer_create(tick_cb, SHUF_TICK_MS, NULL);
}

void kit_bingo_draw(void)
{
    start_draw();
}

// ---------------------------------------------------------------------------
// REINICIAR — dois toques
// ---------------------------------------------------------------------------

static void reset_disarm(void)
{
    s_reset_armed = false;
    if (s_reset_timer) { lv_timer_delete(s_reset_timer); s_reset_timer = NULL; }
    if (s_reset_lbl) {
        lv_label_set_text(s_reset_lbl, "REINICIAR SORTEIO");
        lv_obj_set_style_text_color(s_reset_lbl, lv_color_hex(KIT_COLOR_RED), 0);
    }
    if (s_reset_btn)
        lv_obj_set_style_bg_opa(s_reset_btn, LV_OPA_TRANSP, 0);
}

static void reset_disarm_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (s_reset_timer) { lv_timer_delete(s_reset_timer); s_reset_timer = NULL; }
    reset_disarm();
}

static void do_reset(void)
{
    s_ndrawn = 0;
    memset(s_in, 0, sizeof(s_in));
    reset_disarm();
    save_prefs();
    sync_globo();
    board_build();
}

static void reset_cb(lv_event_t *e)
{
    (void)e;
    if (!s_reset_armed) {
        if (s_ndrawn == 0) return;              // nada para zerar
        s_reset_armed = true;
        lv_label_set_text(s_reset_lbl, "TOCAR DE NOVO PARA ZERAR");
        lv_obj_set_style_text_color(s_reset_lbl, lv_color_hex(KIT_COLOR_ON_COLOR), 0);
        lv_obj_set_style_bg_opa(s_reset_btn, LV_OPA_COVER, 0);
        s_reset_timer = lv_timer_create(reset_disarm_timer_cb, RESET_ARM_MS, NULL);
        return;
    }
    do_reset();
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
    reset_disarm();
}

static void stage_cb(lv_event_t *e) { (void)e; start_draw(); }
static void go_cb(lv_event_t *e)    { (void)e; start_draw(); }

static void view_cb(lv_event_t *e)
{
    int v = (int)(intptr_t)lv_event_get_user_data(e);
    if (v == s_view) return;
    s_view = v;
    apply_view();
    save_prefs();
}

static void range_cb(lv_event_t *e)
{
    int r = (int)(intptr_t)lv_event_get_user_data(e);
    if (r == s_range) return;
    s_range  = r;
    s_ndrawn = 0;
    memset(s_in, 0, sizeof(s_in));
    reset_disarm();
    save_prefs();
    sync_seg();
    sync_globo();
    board_build();
    cards_sync();
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

    lv_obj_t *title = add_label(s_screen, "BINGO", KIT_COLOR_TEXT, &kit_mono_26, 3);
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
    lv_obj_set_style_pad_row(p, 26, 0);
    lv_obj_set_flex_flow(p, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(p, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(p, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(p, LV_SCROLLBAR_MODE_AUTO);

    // -------- FAIXA --------
    lv_obj_t *sec_r = plain_box(p);
    lv_obj_set_size(sec_r, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(sec_r, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(sec_r, 12, 0);
    field_label(sec_r, "FAIXA");

    lv_obj_t *rrow = plain_box(sec_r);
    lv_obj_set_size(rrow, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(rrow, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(rrow, 10, 0);
    s_range_pills[0] = make_pill(rrow, "1-75", 58, true, range_cb, 75, &s_range_pill_lbls[0]);
    s_range_pills[1] = make_pill(rrow, "1-90", 58, true, range_cb, 90, &s_range_pill_lbls[1]);
    add_label(sec_r, "1-75 MOSTRA A LETRA DA COLUNA.\n1-90 \xC3\x89 O BING\xC3\x83O, S\xC3\x93 N\xC3\x9AMERO.",
              KIT_COLOR_TEXT_MUTED, &kit_mono_16, 1);

    // -------- RODADA --------
    lv_obj_t *sec_x = plain_box(p);
    lv_obj_set_size(sec_x, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(sec_x, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(sec_x, 12, 0);
    field_label(sec_x, "RODADA");

    s_reset_btn = lv_obj_create(sec_x);
    lv_obj_set_size(s_reset_btn, lv_pct(100), 58);
    lv_obj_set_style_radius(s_reset_btn, 15, 0);
    lv_obj_set_style_bg_color(s_reset_btn, lv_color_hex(KIT_COLOR_RED), 0);
    lv_obj_set_style_bg_opa(s_reset_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_reset_btn, 2, 0);
    lv_obj_set_style_border_color(s_reset_btn, lv_color_hex(KIT_COLOR_RED), 0);
    lv_obj_set_style_pad_all(s_reset_btn, 0, 0);
    lv_obj_clear_flag(s_reset_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_reset_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_reset_btn, 6);
    lv_obj_add_event_cb(s_reset_btn, reset_cb, LV_EVENT_CLICKED, NULL);
    s_reset_lbl = add_label(s_reset_btn, "REINICIAR SORTEIO", KIT_COLOR_RED, &kit_mono_20, 2);
    lv_obj_center(s_reset_lbl);

    add_label(sec_x, "ZERA O PAINEL DE CHAMADAS E VOLTA\nO GLOBO PRO COME\xC3\x87O. TROCAR A\nFAIXA TAMB\xC3\x89M ZERA.",
              KIT_COLOR_TEXT_MUTED, &kit_mono_16, 1);
}

// Página 1 — GLOBO
static void build_page_stage(lv_obj_t *tile)
{
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_t *box = lv_obj_create(tile);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    int stage_h = X_PAGE_H - (X_GO_H + 2 * X_GO_MARGIN);
    lv_obj_t *stage = plain_box(box);
    lv_obj_set_size(stage, KIT_DISPLAY_WIDTH, stage_h);
    lv_obj_set_pos(stage, 0, 0);
    lv_obj_add_flag(stage, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(stage, stage_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *col = plain_box(stage);
    lv_obj_set_size(col, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, 4, 0);
    lv_obj_add_flag(col, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(col);

    s_letter_lbl = add_label(col, "", s_accent, &kit_mono_26, 6);
    lv_obj_set_height(s_letter_lbl, 30);
    s_num_lbl = add_label(col, "-", KIT_COLOR_TEXT_MUTED, &kit_display_120, 0);
    s_prev_lbl = add_label(col, "", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);
    lv_obj_set_style_pad_top(s_prev_lbl, 6, 0);
    s_count_lbl = add_label(col, "0 / 75", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);
    s_hint_lbl = add_label(col, "SORTEAR \xE2\x80\xA2 PWR \xE2\x80\xA2 CHACOALHAR",
                           KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);
    lv_obj_set_style_text_align(s_hint_lbl, LV_TEXT_ALIGN_CENTER, 0);

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

// Página 2 — CHAMADAS
static void build_page_board(lv_obj_t *tile)
{
    lv_obj_set_style_pad_all(tile, 0, 0);
    s_board_wrap = lv_obj_create(tile);
    lv_obj_remove_style_all(s_board_wrap);
    lv_obj_set_size(s_board_wrap, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_left(s_board_wrap, X_PAD, 0);
    lv_obj_set_style_pad_right(s_board_wrap, X_PAD, 0);
    lv_obj_set_style_pad_top(s_board_wrap, 6, 0);
    lv_obj_set_style_pad_bottom(s_board_wrap, 16, 0);
    lv_obj_set_style_pad_row(s_board_wrap, 10, 0);
    lv_obj_set_flex_flow(s_board_wrap, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(s_board_wrap, LV_OBJ_FLAG_SCROLLABLE);

    s_board_head = add_label(s_board_wrap, "SORTEADOS 0 / 75", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);

    // toggle LISTA / GRADE
    s_view_seg = plain_box(s_board_wrap);
    lv_obj_set_size(s_view_seg, X_CONTENT, 44);
    lv_obj_set_flex_flow(s_view_seg, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(s_view_seg, 8, 0);
    s_view_pills[0] = make_pill(s_view_seg, "LISTA", 44, true, view_cb, VIEW_LIST, &s_view_pill_lbls[0]);
    s_view_pills[1] = make_pill(s_view_seg, "GRADE", 44, true, view_cb, VIEW_GRID, &s_view_pill_lbls[1]);

    s_table = lv_table_create(s_board_wrap);
    lv_obj_set_width(s_table, X_CONTENT);
    lv_obj_set_flex_grow(s_table, 1);
    lv_obj_set_scroll_dir(s_table, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_table, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_border_width(s_table, 0, 0);
    lv_obj_set_style_bg_opa(s_table, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(s_table, 0, 0);

    // Células (LV_PART_ITEMS) — a cor final sai do hook table_draw_cb.
    lv_obj_set_style_text_font(s_table, &kit_mono_16, LV_PART_ITEMS);
    lv_obj_set_style_text_color(s_table, lv_color_hex(KIT_COLOR_TEXT_MUTED), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(s_table, lv_color_hex(KIT_COLOR_SURFACE), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(s_table, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_pad_top(s_table, 7, LV_PART_ITEMS);
    lv_obj_set_style_pad_bottom(s_table, 7, LV_PART_ITEMS);
    lv_obj_set_style_pad_left(s_table, 0, LV_PART_ITEMS);
    lv_obj_set_style_pad_right(s_table, 0, LV_PART_ITEMS);
    lv_obj_set_style_border_width(s_table, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_color(s_table, lv_color_hex(KIT_COLOR_LINE), LV_PART_ITEMS);
    lv_obj_set_style_border_opa(s_table, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_border_side(s_table, LV_BORDER_SIDE_FULL, LV_PART_ITEMS);
    lv_obj_set_style_radius(s_table, 0, LV_PART_ITEMS);

    lv_obj_add_flag(s_table, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
    lv_obj_add_event_cb(s_table, table_draw_cb, LV_EVENT_DRAW_TASK_ADDED, NULL);
}

// Página 3 — CARTELAS
// QR pro gerador de cartelas (web-installer/bingo.html). O QR precisa de fundo
// claro pra ler numa câmera, então aqui é a exceção ao preto AMOLED: quadrado
// branco com o código preto. O link carrega ?bolas= com a FAIXA atual.
static void build_page_cards(lv_obj_t *tile)
{
    lv_obj_set_style_pad_all(tile, 0, 0);

    lv_obj_t *p = lv_obj_create(tile);
    lv_obj_remove_style_all(p);
    lv_obj_set_size(p, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_left(p, X_PAD, 0);
    lv_obj_set_style_pad_right(p, X_PAD, 0);
    lv_obj_set_style_pad_top(p, 16, 0);
    lv_obj_set_style_pad_bottom(p, 24, 0);
    lv_obj_set_style_pad_row(p, 16, 0);
    lv_obj_set_flex_flow(p, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(p, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(p, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(p, LV_SCROLLBAR_MODE_AUTO);

    lv_obj_t *cap = add_label(p, "ESCANEIE COM O CELULAR", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);
    lv_obj_set_style_text_align(cap, LV_TEXT_ALIGN_CENTER, 0);

    s_qr = lv_qrcode_create(p);
    lv_qrcode_set_size(s_qr, 188);
    lv_qrcode_set_dark_color(s_qr, lv_color_hex(KIT_COLOR_BG));
    lv_qrcode_set_light_color(s_qr, lv_color_white());
    lv_qrcode_set_quiet_zone(s_qr, true);
    lv_obj_set_style_border_width(s_qr, 10, 0);
    lv_obj_set_style_border_color(s_qr, lv_color_white(), 0);
    lv_obj_set_style_radius(s_qr, 4, 0);

    s_qr_tag = add_label(p, "BINGO 1-75", s_accent, &kit_mono_20, 2);

    lv_obj_t *note = add_label(p,
        "NO SITE D\xC3\x81 PRA BAIXAR, IMPRIMIR OU MARCAR POR L\xC3\x81. "
        "MAS O KIT TORCE POR PAPEL E CANETA, N\xC3\x83O POR MAIS TEMPO DE TELA.",
        KIT_COLOR_TEXT_MUTED, &kit_mono_16, 1);
    lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(note, X_CONTENT);
    lv_obj_set_style_text_align(note, LV_TEXT_ALIGN_CENTER, 0);
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
    s_tiles[3] = lv_tileview_add_tile(s_tv, 3, 0, LV_DIR_HOR);
    build_page_adjust(s_tiles[0]);
    build_page_stage(s_tiles[1]);
    build_page_board(s_tiles[2]);
    build_page_cards(s_tiles[3]);
}

// ---------------------------------------------------------------------------
// Ciclo de vida
// ---------------------------------------------------------------------------

kit_err_t kit_bingo_start(uint32_t accent)
{
    if (s_screen) kit_bingo_destroy();

    ESP_LOGI(TAG, "Montando Globo de Bingo...");
    s_accent  = accent ? accent : KIT_COLOR_GREEN;
    s_range   = 75;
    s_view    = VIEW_LIST;
    s_ndrawn  = 0;
    s_drawing = false;
    s_tick    = 0;
    s_reset_armed = false;
    memset(s_in, 0, sizeof(s_in));
    load_prefs();

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(KIT_COLOR_BG), 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    build_titlebar();
    build_tileview();

    lv_tileview_set_tile_by_index(s_tv, 1, 0, LV_ANIM_OFF);   // começa no GLOBO
    lv_obj_update_layout(s_screen);

    board_build();
    sync_seg();
    sync_globo();
    sync_dots();
    cards_sync();

    lv_screen_load(s_screen);
    return KIT_OK;
}

void kit_bingo_destroy(void)
{
    ESP_LOGI(TAG, "Encerrando Globo de Bingo.");
    if (s_timer)       { lv_timer_delete(s_timer);       s_timer = NULL; }
    if (s_reset_timer) { lv_timer_delete(s_reset_timer); s_reset_timer = NULL; }
    s_drawing = false;
    s_reset_armed = false;

    if (s_screen) { lv_obj_delete(s_screen); s_screen = NULL; }

    s_tv = NULL;
    s_range_pills[0] = s_range_pills[1] = NULL;
    s_range_pill_lbls[0] = s_range_pill_lbls[1] = NULL;
    s_reset_btn = s_reset_lbl = NULL;
    s_letter_lbl = s_num_lbl = s_prev_lbl = s_count_lbl = s_hint_lbl = NULL;
    s_go_btn = s_go_lbl = NULL;
    s_board_wrap = s_board_head = s_bingo_hdr = s_table = NULL;
    s_view_seg = s_list = NULL;
    s_view_pills[0] = s_view_pills[1] = NULL;
    s_view_pill_lbls[0] = s_view_pill_lbls[1] = NULL;
    for (int i = 0; i < 9; i++) s_list_val[i] = NULL;
    s_qr = s_qr_tag = NULL;
}
