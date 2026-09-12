/**
 * @file kit_ui.h
 * @brief Galeria de componentes de UI do KIT — widgets prontos no padrão
 *        "Brutalist Bauhaus" para reaproveitar entre Tools.
 *
 * Header-only, sem estado global escondido: cada componente com estado
 * (grade de chips, seletor de sigla, botão de ação, "shell" da tela) vive
 * numa struct que a Tool declara como `static` e zera no `tool_destroy` —
 * como já se faz com todo ponteiro LVGL (ver tool_lvgl_runtime.md, erro nº9).
 *
 * ┌─ Como usar ───────────────────────────────────────────────────────────┐
 * │ #include "kit_ui.h"                                                    │
 * │                                                                       │
 * │ static kit_ui_shell_t  s_shell;                                        │
 * │ static kit_ui_chips_t  s_dur;                                          │
 * │                                                                       │
 * │ kit_err_t tool_init(kit_tool_ctx_t *ctx) {                             │
 * │     s_api = ctx->api;                                                  │
 * │     kit_ui_bind(s_api);                       // liga áudio/rng/exit   │
 * │     lv_obj_t *scr = lv_obj_create(NULL);                               │
 * │     ...                                                                │
 * │     kit_ui_shell_begin(&s_shell, scr, "MINHA TOOL", KIT_COLOR_BLUE, 3);│
 * │     kit_ui_shell_tiles(&s_shell, on_page, NULL);                       │
 * │     build_ajuste(s_shell.tiles[0]);                                    │
 * │     build_jogo  (s_shell.tiles[1]);                                    │
 * │     kit_ui_help_page(s_shell.tiles[2], "COMO JOGA", RULES);            │
 * │     kit_ui_shell_open(&s_shell, 1);           // abre no JOGO          │
 * │     lv_screen_load(scr);                                               │
 * │ }                                                                      │
 * └───────────────────────────────────────────────────────────────────────┘
 *
 * ── Restrições que este header respeita (Tool .so) ──────────────────────
 *  - só chama funções LVGL da tabela de símbolos do firmware
 *    (tools-sdk/docs/tool_lvgl_runtime.md);
 *  - inteiro puro — nada de float nem divisão de 64 bits;
 *  - toda a implementação fica atrás de `#ifndef KIT_SDK_STUBS`: no build
 *    nativo (CI/lógica) este header é vazio de propósito, igual à UI da Tool.
 *
 * @copyright GNU General Public License v3.0 (GPL-3.0)
 */
#pragma once

#include "kit_tool_api.h"   /* -> kit_lvgl.h (lvgl.h real no Xtensa) */
#include "kit_theme.h"
#include "kit_fonts.h"

#ifndef KIT_SDK_STUBS

/* Tudo `static inline`: um header incluído por vários .c não colide, e o
 * `--gc-sections` descarta o que a Tool não usa (sem warning de "unused"). */
#define KIT_UI_DEF static inline

/* -----------------------------------------------------------------------
 * Métricas do AMOLED 368 × 448 (espelham as constantes que toda Tool
 * redefine à mão: B_PAD/T_PAD, B_CONTENT, B_TITLEBAR, ...).
 * ----------------------------------------------------------------------- */
#define KIT_UI_SCREEN_W    368
#define KIT_UI_SCREEN_H    448
#define KIT_UI_PAD         16
#define KIT_UI_CONTENT     (KIT_UI_SCREEN_W - 2 * KIT_UI_PAD)   /* 336 */
#define KIT_UI_TITLEBAR    88
#define KIT_UI_PAGE_H      (KIT_UI_SCREEN_H - KIT_UI_TITLEBAR)   /* 360 */
#define KIT_UI_CHIP        56
#define KIT_UI_BTN_H       76
#define KIT_UI_BTN_MARGIN  18

#define KIT_UI_QR_SIZE     216   /* QR inline — legível de perto, ainda cabe com texto */
#define KIT_UI_QR_BIG      332   /* QR expandido — quase a largura inteira da tela */
#define KIT_UI_QR_BRIGHT   100   /* brilho do overlay expandido (%) */
#define KIT_UI_QR_MAX      192   /* maior link que o componente guarda */

#define KIT_UI_MAX_PAGES     6
#define KIT_UI_MAX_CHIPS     8
#define KIT_UI_SIGLA_DRAG_PX  24   /* px de arraste por letra — sensação de roleta */
#define KIT_UI_SIGLA_TAP_SLOP 12   /* movimento até aqui ainda conta como toque, não arraste */
#define KIT_UI_SIGLA_JUMP_MAX 64   /* delta de 1 amostra acima disto = lixo de coordenada, ignora */
#define KIT_UI_SIGLA_LOCKS     3   /* nº de containers cujo scroll congela durante o arraste */

/* =======================================================================
 * TIER 1 — primitivas
 * ===================================================================== */

/** API do Runtime, ligada uma vez em `kit_ui_bind()`. Usada pelos helpers
 *  de áudio, RNG, energia e `kit_ui_exit()`. */
static const kit_api_table_t *kit_ui__api = NULL;

/** Liga a tabela de APIs (chame no início do `tool_init`). */
KIT_UI_DEF void kit_ui_bind(const kit_api_table_t *api) { kit_ui__api = api; }

/** Cor de texto/glifo sobre uma superfície cheia `accent`
 *  (preto sobre amarelo, off-white sobre vermelho/azul/verde). */
KIT_UI_DEF uint32_t kit_ui_on(uint32_t accent)
{
    return (accent == KIT_COLOR_YELLOW) ? KIT_COLOR_ON_YELLOW : KIT_COLOR_ON_COLOR;
}

/** Container de layout invisível: sem estilo, sem scroll. NÃO usar quando
 *  o conteúdo precisa rolar — nesse caso crie um `lv_obj_create` e ligue
 *  `lv_obj_set_scroll_dir(o, LV_DIR_VER)`. */
KIT_UI_DEF lv_obj_t *kit_ui_box(lv_obj_t *parent)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

/** Label numa chamada: cor + fonte + tracking. `letter_space` 0 = sem
 *  tracking (use 2–4 em mono CAIXA ALTA; 0 em Archivo caixa normal). */
KIT_UI_DEF lv_obj_t *kit_ui_label(lv_obj_t *parent, const char *txt, uint32_t color,
                                  const lv_font_t *font, int letter_space)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(l, font, 0);
    if (letter_space) lv_obj_set_style_text_letter_space(l, letter_space, 0);
    return l;
}

/** Bloco de texto de leitura: quebra em `width` px, centrado. Para a
 *  página COMO JOGA, prefira `kit_ui_help_page()`. */
KIT_UI_DEF lv_obj_t *kit_ui_text(lv_obj_t *parent, const char *txt, uint32_t color,
                                 const lv_font_t *font, int width)
{
    lv_obj_t *l = kit_ui_label(parent, txt, color, font, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, width);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    return l;
}

/** Retângulo arredondado opaco (superfície de card, alvo de toque, forma). */
KIT_UI_DEF lv_obj_t *kit_ui_rect(lv_obj_t *parent, int w, int h, uint32_t bg, int radius)
{
    lv_obj_t *o = kit_ui_box(parent);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, radius, 0);
    return o;
}

/** Torna `o` um alvo de toque: CLICKABLE + feedback de pressão + zona de
 *  clique esticada 8 px pra fora + `cb` no CLICKED, com `code` inteiro
 *  como user_data (`(int)(intptr_t)lv_event_get_user_data(e)`). */
KIT_UI_DEF lv_obj_t *kit_ui_tap(lv_obj_t *o, lv_event_cb_t cb, int code)
{
    lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(o, LV_OPA_80, LV_STATE_PRESSED);
    lv_obj_set_ext_click_area(o, 8);
    lv_obj_add_event_cb(o, cb, LV_EVENT_CLICKED, (void *)(intptr_t)code);
    return o;
}

/** Flex numa chamada: `flow` + alinhamento do eixo principal + gaps.
 *  Cruzado e o `track` ficam sempre CENTER (o caso comum das Tools). */
KIT_UI_DEF void kit_ui_flex(lv_obj_t *o, lv_flex_flow_t flow, lv_flex_align_t main_align,
                            int pad_row, int pad_col)
{
    lv_obj_set_flex_flow(o, flow);
    lv_obj_set_flex_align(o, main_align, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    if (pad_row) lv_obj_set_style_pad_row(o, pad_row, 0);
    if (pad_col) lv_obj_set_style_pad_column(o, pad_col, 0);
}

/* --- áudio: presets prontos + o "erro" padrão (duas notas descendo) ----- */

KIT_UI_DEF void kit_ui_sfx(kit_sfx_t s)
{
    if (kit_ui__api && kit_ui__api->audio) kit_ui__api->audio->sfx(s);
}
KIT_UI_DEF void kit_ui_beep(uint16_t hz, uint16_t ms)
{
    if (kit_ui__api && kit_ui__api->audio) kit_ui__api->audio->beep(hz, ms);
}
KIT_UI_DEF void kit_ui_click(void)   { kit_ui_sfx(KIT_SFX_CLICK); }
KIT_UI_DEF void kit_ui_confirm(void) { kit_ui_sfx(KIT_SFX_CONFIRM); }

/** Feedback de erro sem bipe estridente: A4 -> D4, curtas, resolução suave. */
KIT_UI_DEF void kit_ui_miss(void)
{
    if (!kit_ui__api || !kit_ui__api->audio) return;
    kit_ui__api->audio->beep(440, 40);
    kit_ui__api->audio->beep(294, 90);
}

/* --- RNG inteiro (o loader não resolve rand/srand — use a Random API) --- */

/** Inteiro em [lo, hi] inclusivo. Sem a Random API, devolve `lo`. */
KIT_UI_DEF int kit_ui_rnd(int lo, int hi)
{
    if (!kit_ui__api || !kit_ui__api->random) return lo;
    return (int)kit_ui__api->random->range(lo, hi);
}
/** Cara ou coroa. */
KIT_UI_DEF bool kit_ui_coin(void) { return kit_ui_rnd(0, 1) == 1; }

/* --- energia / ciclo de vida ------------------------------------------- */

KIT_UI_DEF void kit_ui_keep_awake(bool on)
{
    if (kit_ui__api && kit_ui__api->power) kit_ui__api->power->keep_awake(on);
}
/** Encerra a Tool e volta ao Launcher (não retorna). */
KIT_UI_DEF void kit_ui_exit(void)
{
    if (kit_ui__api && kit_ui__api->system) kit_ui__api->system->exit();
}
KIT_UI_DEF void kit_ui__exit_cb(lv_event_t *e) { (void)e; kit_ui_exit(); }

/* =======================================================================
 * TIER 2 — esqueleto Ajuste / Jogo / Como Joga
 * ===================================================================== */

/**
 * Shell da tela de uma Tool: titlebar (chip ← + título + N dots) + um
 * `lv_tileview` horizontal por baixo. A Tool preenche `tiles[i]`.
 */
typedef struct {
    lv_obj_t *screen;
    lv_obj_t *tv;
    lv_obj_t *dots_box;                    /* container dos dots — esconde ao travar */
    lv_obj_t *tiles[KIT_UI_MAX_PAGES];
    lv_obj_t *dots[KIT_UI_MAX_PAGES];
    int       page_count;
    uint32_t  accent;
    void    (*on_page)(int page, void *user);
    void     *user;
} kit_ui_shell_t;

KIT_UI_DEF void kit_ui__dots_sync(kit_ui_shell_t *sh, int active)
{
    for (int i = 0; i < sh->page_count; i++) {
        bool on = (i == active);
        lv_obj_set_style_bg_color(sh->dots[i],
            lv_color_hex(on ? sh->accent : KIT_COLOR_LINE), 0);
        lv_obj_set_size(sh->dots[i], on ? 20 : 8, 8);
    }
}

/** Reavalia qual tile está ativo e sincroniza os dots (chame de fora se
 *  mexer no tileview por conta própria). */
KIT_UI_DEF int kit_ui_shell_active(kit_ui_shell_t *sh)
{
    lv_obj_t *t = lv_tileview_get_tile_active(sh->tv);
    for (int i = 0; i < sh->page_count; i++) if (sh->tiles[i] == t) return i;
    return 0;
}

KIT_UI_DEF void kit_ui__page_cb(lv_event_t *e)
{
    kit_ui_shell_t *sh = (kit_ui_shell_t *)lv_event_get_user_data(e);
    int act = kit_ui_shell_active(sh);
    kit_ui__dots_sync(sh, act);
    if (sh->on_page) sh->on_page(act, sh->user);
}

/**
 * Monta a titlebar como filha de `screen`: chip ← (sai da Tool), título em
 * `kit_mono_26` e `page_count` dots à direita. Guarda `accent`/`screen`
 * no shell. Chame `kit_ui_shell_tiles()` em seguida.
 */
KIT_UI_DEF void kit_ui_shell_begin(kit_ui_shell_t *sh, lv_obj_t *screen,
                                   const char *title, uint32_t accent, int page_count)
{
    if (page_count > KIT_UI_MAX_PAGES) page_count = KIT_UI_MAX_PAGES;
    sh->screen = screen;
    sh->accent = accent;
    sh->page_count = page_count;
    sh->tv = NULL;
    sh->on_page = NULL;
    sh->user = NULL;

    lv_obj_t *chip = lv_obj_create(screen);
    lv_obj_set_size(chip, KIT_UI_CHIP, KIT_UI_CHIP);
    lv_obj_set_style_bg_color(chip, lv_color_hex(KIT_COLOR_SURFACE), 0);
    lv_obj_set_style_border_width(chip, 0, 0);
    lv_obj_set_style_radius(chip, 18, 0);
    lv_obj_set_style_pad_all(chip, 0, 0);
    lv_obj_remove_flag(chip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(chip, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(chip, 12);
    lv_obj_add_event_cb(chip, kit_ui__exit_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(chip, LV_ALIGN_TOP_LEFT, KIT_UI_PAD, 16);
    lv_obj_center(kit_ui_label(chip, KIT_ICON_BACK, KIT_COLOR_TEXT, &kit_display_44, 0));

    lv_obj_t *ttl = kit_ui_label(screen, title, KIT_COLOR_TEXT, &kit_mono_26, 3);
    lv_obj_align(ttl, LV_ALIGN_TOP_LEFT, KIT_UI_PAD + KIT_UI_CHIP + 12, 30);

    lv_obj_t *dots = kit_ui_box(screen);
    lv_obj_set_size(dots, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(dots, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dots, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(dots, 6, 0);
    lv_obj_align(dots, LV_ALIGN_TOP_RIGHT, -KIT_UI_PAD, 40);
    sh->dots_box = dots;
    for (int i = 0; i < page_count; i++) {
        lv_obj_t *d = lv_obj_create(dots);
        lv_obj_remove_style_all(d);
        lv_obj_set_size(d, 8, 8);
        lv_obj_set_style_radius(d, 4, 0);
        lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(d, lv_color_hex(KIT_COLOR_LINE), 0);
        sh->dots[i] = d;
    }
}

/**
 * Cria o `lv_tileview` por baixo da titlebar e `page_count` tiles
 * horizontais. Preenche `sh->tiles[]`. `on_page(idx, user)` é chamado a
 * cada troca de página (e por `kit_ui_shell_open`), depois dos dots.
 */
KIT_UI_DEF void kit_ui_shell_tiles(kit_ui_shell_t *sh,
                                   void (*on_page)(int, void *), void *user)
{
    sh->on_page = on_page;
    sh->user = user;

    lv_obj_t *tv = lv_tileview_create(sh->screen);
    lv_obj_set_size(tv, KIT_UI_SCREEN_W, KIT_UI_PAGE_H);
    lv_obj_set_pos(tv, 0, KIT_UI_TITLEBAR);
    lv_obj_set_style_bg_opa(tv, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tv, 0, 0);
    lv_obj_set_scrollbar_mode(tv, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(tv, kit_ui__page_cb, LV_EVENT_VALUE_CHANGED, sh);
    sh->tv = tv;

    for (int i = 0; i < sh->page_count; i++)
        sh->tiles[i] = lv_tileview_add_tile(tv, (uint8_t)i, 0, LV_DIR_HOR);
}

/** Pula pro tile `page` sem animação e sincroniza dots + `on_page`.
 *  Toda Tool abre no JOGO: `kit_ui_shell_open(&sh, 1)`. */
KIT_UI_DEF void kit_ui_shell_open(kit_ui_shell_t *sh, int page)
{
    lv_tileview_set_tile_by_index(sh->tv, (uint8_t)page, 0, LV_ANIM_OFF);
    kit_ui__dots_sync(sh, page);
    if (sh->on_page) sh->on_page(page, sh->user);
}

/**
 * Trava/destrava a navegação por swipe — pra Tool que prende o jogador no
 * JOGO durante a partida (ex: Fora). `locked`: pula pro tile do JOGO,
 * desliga o swipe e esconde os dots. `unlocked`: religa tudo. Origem:
 * `set_swipe()` de `io.github.jcrvlh.fora`.
 */
KIT_UI_DEF void kit_ui_shell_lock(kit_ui_shell_t *sh, bool locked)
{
    if (locked) {
        lv_tileview_set_tile_by_index(sh->tv, 1, 0, LV_ANIM_OFF);
        lv_obj_remove_flag(sh->tv, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(sh->dots_box, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(sh->tv, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(sh->dots_box, LV_OBJ_FLAG_HIDDEN);
        kit_ui__dots_sync(sh, kit_ui_shell_active(sh));
    }
}

/**
 * Monta a página COMO JOGA num tile: cabeçalho em `kit_mono_26` + corpo de
 * leitura em `kit_sans_28`, rolando na vertical. `body` pode ter `\n`.
 */
KIT_UI_DEF void kit_ui_help_page(lv_obj_t *tile, const char *heading, const char *body)
{
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_t *p = lv_obj_create(tile);
    lv_obj_remove_style_all(p);
    lv_obj_set_size(p, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(p, KIT_UI_PAD, 0);
    lv_obj_set_style_pad_top(p, 16, 0);
    lv_obj_set_style_pad_bottom(p, 32, 0);
    lv_obj_set_style_pad_row(p, 14, 0);
    lv_obj_set_flex_flow(p, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(p, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scroll_dir(p, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(p, LV_SCROLLBAR_MODE_AUTO);

    kit_ui_label(p, heading, KIT_COLOR_TEXT, &kit_mono_26, 3);
    lv_obj_t *b = kit_ui_label(p, body, KIT_COLOR_TEXT, &kit_sans_28, 0);
    lv_label_set_long_mode(b, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(b, KIT_UI_CONTENT);
}

/* --- botão de ação (rodapé do tile do JOGO) --------------------------- */

/**
 * Botão primário fixo no rodapé, na cor da Tool. DEVE ser filho do tile do
 * JOGO (não da screen — senão flutua sobre as outras páginas, erro nº5).
 * Compartilhado entre estados (COMEÇAR / SALVAR / JOGAR DE NOVO): troque o
 * rótulo com `kit_ui_action_set` e esconda no meio da rodada com
 * `kit_ui_action_show(&a, false)`.
 */
typedef struct { lv_obj_t *btn; lv_obj_t *lbl; uint32_t accent; } kit_ui_action_t;

KIT_UI_DEF void kit_ui_action_button(kit_ui_action_t *a, lv_obj_t *game_tile,
                                     uint32_t accent, lv_event_cb_t cb)
{
    a->accent = accent;
    lv_obj_t *b = lv_obj_create(game_tile);
    lv_obj_set_size(b, KIT_UI_CONTENT, KIT_UI_BTN_H);
    lv_obj_set_style_radius(b, KIT_UI_BTN_H / 2, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(accent), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_80, LV_STATE_PRESSED);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(b, 8);
    lv_obj_align(b, LV_ALIGN_BOTTOM_MID, 0, -KIT_UI_BTN_MARGIN);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    a->btn = b;
    a->lbl = kit_ui_label(b, "", kit_ui_on(accent), &kit_mono_26, 3);
    lv_obj_center(a->lbl);
}
KIT_UI_DEF void kit_ui_action_set(kit_ui_action_t *a, const char *label)
{
    lv_label_set_text(a->lbl, label);
}
KIT_UI_DEF void kit_ui_action_show(kit_ui_action_t *a, bool show)
{
    if (show) lv_obj_remove_flag(a->btn, LV_OBJ_FLAG_HIDDEN);
    else      lv_obj_add_flag(a->btn, LV_OBJ_FLAG_HIDDEN);
}

/* =======================================================================
 * TIER 2 — grade de chips (seletor segmentado da página AJUSTE)
 * ===================================================================== */

/**
 * Grade de "pílulas" de opção — no máximo 2 por linha, 84 px de altura
 * (piso de toque no AJUSTE). O chip ativo pega a cor da Tool; os outros
 * ficam em `KIT_COLOR_SURFACE`. Origem: io.github.jcrvlh.telefonema,
 * copiado idêntico em Vira Certo e Quadrado.
 *
 * `on_select(idx, user)` é chamado no toque (depois de o visual e o
 * `kit_ui_click()` já terem rolado) — faça ali só a persistência/efeito.
 */
typedef struct kit_ui_chips_s kit_ui_chips_t;
typedef struct { kit_ui_chips_t *owner; int idx; } kit_ui__chip_slot_t;

struct kit_ui_chips_s {
    lv_obj_t *chip[KIT_UI_MAX_CHIPS];
    lv_obj_t *lbl[KIT_UI_MAX_CHIPS];
    kit_ui__chip_slot_t slot[KIT_UI_MAX_CHIPS];
    int       count;
    int       selected;
    uint32_t  accent;
    void    (*on_select)(int idx, void *user);
    void     *user;
};

/** Aplica o estado visual de `selected` sem tocar áudio nem chamar o
 *  callback — seguro no init e em qualquer re-sync. */
KIT_UI_DEF void kit_ui_chips_select(kit_ui_chips_t *c, int selected)
{
    c->selected = selected;
    uint32_t sel_txt = kit_ui_on(c->accent);
    for (int i = 0; i < c->count; i++) {
        bool s = (i == selected);
        lv_obj_set_style_bg_color(c->chip[i],
            lv_color_hex(s ? c->accent : KIT_COLOR_SURFACE), 0);
        lv_obj_set_style_text_color(c->lbl[i],
            lv_color_hex(s ? sel_txt : KIT_COLOR_TEXT), 0);
    }
}

KIT_UI_DEF void kit_ui__chip_cb(lv_event_t *e)
{
    kit_ui__chip_slot_t *sl = (kit_ui__chip_slot_t *)lv_event_get_user_data(e);
    kit_ui_chips_t *c = sl->owner;
    kit_ui_chips_select(c, sl->idx);
    kit_ui_click();
    if (c->on_select) c->on_select(sl->idx, c->user);
}

/**
 * Cria a grade em `parent` (tipicamente logo abaixo de um rótulo em
 * `kit_mono_16`). `labels` deve durar tanto quanto a tela (use um
 * `static const char *const[]`). `initial` é o índice pré-selecionado.
 */
KIT_UI_DEF void kit_ui_chips(kit_ui_chips_t *c, lv_obj_t *parent,
                             const char *const *labels, int count, int initial,
                             uint32_t accent,
                             void (*on_select)(int, void *), void *user)
{
    if (count > KIT_UI_MAX_CHIPS) count = KIT_UI_MAX_CHIPS;
    c->count = count;
    c->accent = accent;
    c->on_select = on_select;
    c->user = user;

    lv_obj_t *wrap = kit_ui_box(parent);
    lv_obj_set_size(wrap, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(wrap, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(wrap, 10, 0);

    int idx = 0;
    while (idx < count) {
        lv_obj_t *row = kit_ui_box(wrap);
        lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(row, 10, 0);

        int in_row = (count - idx < 2) ? (count - idx) : 2;
        for (int k = 0; k < in_row; k++, idx++) {
            lv_obj_t *ch = lv_obj_create(row);
            lv_obj_set_height(ch, 84);
            lv_obj_set_flex_grow(ch, 1);
            lv_obj_set_style_bg_color(ch, lv_color_hex(KIT_COLOR_SURFACE), 0);
            lv_obj_set_style_bg_opa(ch, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(ch, 0, 0);
            lv_obj_set_style_radius(ch, 18, 0);
            lv_obj_set_style_pad_all(ch, 0, 0);
            lv_obj_remove_flag(ch, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(ch, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_ext_click_area(ch, 6);

            c->slot[idx].owner = c;
            c->slot[idx].idx = idx;
            lv_obj_add_event_cb(ch, kit_ui__chip_cb, LV_EVENT_CLICKED, &c->slot[idx]);

            c->chip[idx] = ch;
            c->lbl[idx] = kit_ui_label(ch, labels[idx], KIT_COLOR_TEXT, &kit_mono_20, 1);
            lv_obj_center(c->lbl[idx]);
        }
    }
    kit_ui_chips_select(c, initial);
}

/* =======================================================================
 * TIER 3 (referência) — seletor de SIGLA (3 iniciais, roleta por arraste)
 * ===================================================================== */

/**
 * Três caixas de letra A–Z. Toque numa caixa avança uma letra; arraste
 * pra cima/baixo gira como uma roleta. Botão REDEFINIR volta pra "AAA".
 * Origem: io.github.jcrvlh.bolaquadrado (entrada de iniciais no highscore).
 *
 * O arraste usa o stream de toque bruto — o SDK não expõe widget de
 * rolagem. A Tool registra UM callback de input (`api->input->
 * register_callback`) e repassa cada evento pra `kit_ui_sigla_feed_touch`.
 * Enquanto nenhuma caixa está sendo arrastada, `feed_touch` ignora tudo.
 *
 * As caixas não encadeiam scroll pro pai (chain HOR+VER removidos), então o
 * arraste na letra não vaza pra nenhum ancestral. Como cinto-e-suspensório,
 * registre em `kit_ui_sigla_scroll_lock()` os containers que a Tool ainda
 * quer congelar durante o toque — a página que rola na vertical E o
 * `tileview` que desliza na horizontal — cada um com a direção pra restaurar
 * ao soltar.
 *
 * Tolerância: um micro-arraste (< KIT_UI_SIGLA_TAP_SLOP px) ainda conta como
 * toque; acima disso, o `LV_EVENT_CLICKED` do soltar é ignorado (era arraste,
 * não toque). Saltos de coordenada de uma amostra só — o lixo que chega no
 * instante em que o dedo sai da tela — são descartados.
 *
 * `kit_ui_sigla_opts_t` (pode ser NULL → tudo default):
 *  - `allow_blank`: a letra pode ser ' ' (mostrada "-", apagada). O ciclo
 *    vira ' '→A→…→Z→' '. Pra "sigla opcional" (Fora: sem sigla = "JOGADOR N").
 *  - `on_change(letters, user)`: chamado a cada letra e no REDEFINIR — pra
 *    persistir na hora. `letters` é a string de 3 chars ("AB " no modo blank).
 *  - `reset_label`: rótulo do botão (default "REDEFINIR", ou "APAGAR" se blank).
 */
typedef struct {
    bool        allow_blank;
    const char *reset_label;
    void      (*on_change)(const char *letters, void *user);
    void       *user;
} kit_ui_sigla_opts_t;

typedef struct {
    lv_obj_t *row;
    lv_obj_t *box[3];
    lv_obj_t *lbl[3];
    lv_obj_t *reset_btn;
    struct { lv_obj_t *obj; lv_dir_t restore; } lock[KIT_UI_SIGLA_LOCKS];
    int       nlock;
    char      letters[4];          /* "ABC\0" — corrente ("AB \0" no modo blank) */
    uint32_t  accent;
    bool      allow_blank;
    void    (*on_change)(const char *letters, void *user);
    void     *user;
    int       drag_box;            /* -1 = ninguém arrastando */
    int       drag_last_y;
    int       drag_accum;          /* resto pro próximo passo de letra */
    int       drag_gross;          /* movimento total do toque (px), pra distinguir toque de arraste */
    bool      drag_moved;          /* já passou do slop → o CLICKED do soltar não conta */
    struct { void *owner; int idx; } slot[3];
} kit_ui_sigla_t;

KIT_UI_DEF void kit_ui__sigla_paint(kit_ui_sigla_t *s)
{
    for (int k = 0; k < 3; k++) {
        bool blank = (s->letters[k] == ' ');
        char t[2] = { blank ? '-' : s->letters[k], 0 };
        lv_label_set_text(s->lbl[k], t);
        lv_obj_set_style_text_color(s->lbl[k],
            lv_color_hex(blank ? KIT_COLOR_TEXT_MUTED : KIT_COLOR_TEXT), 0);
    }
}

KIT_UI_DEF void kit_ui__sigla_notify(kit_ui_sigla_t *s)
{
    if (s->on_change) s->on_change(s->letters, s->user);
}

/** Define as 3 letras de fora (ex: pré-preencher com a última sigla). Não
 *  dispara `on_change`. */
KIT_UI_DEF void kit_ui_sigla_set(kit_ui_sigla_t *s, const char *letters)
{
    char fill = s->allow_blank ? ' ' : 'A';
    for (int k = 0; k < 3; k++) {
        char c = (letters && letters[k]) ? letters[k] : fill;
        if (!(c >= 'A' && c <= 'Z') && !(c == ' ' && s->allow_blank)) c = fill;
        s->letters[k] = c;
    }
    s->letters[3] = 0;
    if (s->lbl[0]) kit_ui__sigla_paint(s);
}

/** As 3 letras correntes, como string ("ABC" / "AB "). */
KIT_UI_DEF const char *kit_ui_sigla_get(kit_ui_sigla_t *s) { return s->letters; }

KIT_UI_DEF void kit_ui__sigla_step(kit_ui_sigla_t *s, int k, int dir)
{
    char c = s->letters[k];
    if (dir > 0) {
        if      (c == ' ') c = 'A';
        else if (c == 'Z') c = s->allow_blank ? ' ' : 'A';
        else               c = (char)(c + 1);
    } else {
        if      (c == ' ') c = 'Z';
        else if (c == 'A') c = s->allow_blank ? ' ' : 'Z';
        else               c = (char)(c - 1);
    }
    s->letters[k] = c;
    kit_ui__sigla_paint(s);
    kit_ui_click();
    kit_ui__sigla_notify(s);
}

KIT_UI_DEF void kit_ui__sigla_tap_cb(lv_event_t *e)
{
    struct { void *owner; int idx; } *sl = lv_event_get_user_data(e);
    kit_ui_sigla_t *s = (kit_ui_sigla_t *)sl->owner;
    if (s->drag_moved) return;   /* foi arraste, não toque */
    kit_ui__sigla_step(s, sl->idx, +1);
}
KIT_UI_DEF void kit_ui__sigla_press_cb(lv_event_t *e)
{
    struct { void *owner; int idx; } *sl = lv_event_get_user_data(e);
    kit_ui_sigla_t *s = (kit_ui_sigla_t *)sl->owner;
    s->drag_box = sl->idx;
    s->drag_accum = 0;
    s->drag_gross = 0;
    s->drag_moved = false;
    s->drag_last_y = -1;   /* a próxima amostra vira a referência */
    for (int i = 0; i < s->nlock; i++)
        lv_obj_set_scroll_dir(s->lock[i].obj, LV_DIR_NONE);
}
KIT_UI_DEF void kit_ui__sigla_release_cb(lv_event_t *e)
{
    struct { void *owner; int idx; } *sl = lv_event_get_user_data(e);
    kit_ui_sigla_t *s = (kit_ui_sigla_t *)sl->owner;
    s->drag_box = -1;   /* drag_moved fica — o CLICKED vem depois do RELEASED */
    for (int i = 0; i < s->nlock; i++)
        lv_obj_set_scroll_dir(s->lock[i].obj, s->lock[i].restore);
}
KIT_UI_DEF void kit_ui__sigla_reset_cb(lv_event_t *e)
{
    kit_ui_sigla_t *s = (kit_ui_sigla_t *)lv_event_get_user_data(e);
    kit_ui_sigla_set(s, s->allow_blank ? "   " : "AAA");
    kit_ui_click();
    kit_ui__sigla_notify(s);
}

/** Repasse do toque bruto: chame de dentro do seu `kit_input_callback_t`. */
KIT_UI_DEF void kit_ui_sigla_feed_touch(kit_ui_sigla_t *s, const kit_input_event_t *ev)
{
    if (!ev || ev->type != KIT_INPUT_TOUCH_DOWN || s->drag_box < 0) return;
    if (s->drag_last_y < 0) { s->drag_last_y = ev->y; return; }

    int dy = (int)ev->y - s->drag_last_y;
    s->drag_last_y = ev->y;

    /* salto de uma amostra só acima do limite = lixo de coordenada no
       instante em que o dedo sai da tela — era o que trocava a letra sozinho */
    if (dy > KIT_UI_SIGLA_JUMP_MAX || dy < -KIT_UI_SIGLA_JUMP_MAX) return;

    s->drag_gross += (dy < 0 ? -dy : dy);
    if (s->drag_gross >= KIT_UI_SIGLA_TAP_SLOP) s->drag_moved = true;

    s->drag_accum += dy;
    /* arrastar pra CIMA (y diminui) avança a letra; pra BAIXO, volta */
    while (s->drag_accum <= -KIT_UI_SIGLA_DRAG_PX) {
        kit_ui__sigla_step(s, s->drag_box, +1); s->drag_accum += KIT_UI_SIGLA_DRAG_PX;
    }
    while (s->drag_accum >= KIT_UI_SIGLA_DRAG_PX) {
        kit_ui__sigla_step(s, s->drag_box, -1); s->drag_accum -= KIT_UI_SIGLA_DRAG_PX;
    }
}

/**
 * Registra um container cujo scroll CONGELA (LV_DIR_NONE) enquanto uma
 * letra é arrastada, voltando a `restore` ao soltar. Chame um por
 * eixo/ancestral que pode bagunçar a navegação — normalmente a página que
 * rola na vertical (`restore = LV_DIR_VER`) e o `tileview` que desliza na
 * horizontal (`restore = LV_DIR_HOR`). Até KIT_UI_SIGLA_LOCKS.
 */
KIT_UI_DEF void kit_ui_sigla_scroll_lock(kit_ui_sigla_t *s, lv_obj_t *obj, lv_dir_t restore)
{
    if (obj && s->nlock < KIT_UI_SIGLA_LOCKS) {
        s->lock[s->nlock].obj = obj;
        s->lock[s->nlock].restore = restore;
        s->nlock++;
    }
}

/**
 * Cria a fileira (3 caixas + REDEFINIR abaixo) em `parent`. `initial` = 3
 * letras maiúsculas ou NULL. `opts` pode ser NULL. Depois registre o input
 * da Tool e chame `kit_ui_sigla_feed_touch` de lá.
 */
KIT_UI_DEF void kit_ui_sigla(kit_ui_sigla_t *s, lv_obj_t *parent, uint32_t accent,
                             const char *initial, const kit_ui_sigla_opts_t *opts)
{
    s->accent = accent;
    s->nlock = 0;
    s->drag_box = -1;
    s->drag_accum = 0;
    s->drag_gross = 0;
    s->drag_moved = false;
    s->drag_last_y = 0;
    s->allow_blank = opts && opts->allow_blank;
    s->on_change   = opts ? opts->on_change : NULL;
    s->user        = opts ? opts->user : NULL;
    for (int k = 0; k < 3; k++) { s->box[k] = s->lbl[k] = NULL; }
    kit_ui_sigla_set(s, initial ? initial : (s->allow_blank ? "   " : "AAA"));

    s->row = kit_ui_box(parent);
    lv_obj_set_size(s->row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s->row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(s->row, 12, 0);

    for (int k = 0; k < 3; k++) {
        lv_obj_t *box = lv_obj_create(s->row);
        lv_obj_set_size(box, 92, 96);
        lv_obj_set_style_bg_color(box, lv_color_hex(KIT_COLOR_SURFACE), 0);
        lv_obj_set_style_border_width(box, 0, 0);
        lv_obj_set_style_radius(box, 16, 0);
        lv_obj_set_style_pad_all(box, 0, 0);
        /* SCROLLABLE (mesmo sem conteúdo) faz a caixa absorver o arraste; sem
         * os chain flags, ele não vaza pra NENHUM ancestral (nem o tileview
         * na horizontal). */
        lv_obj_add_flag(box, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_SCROLL_CHAIN_VER);
        lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);

        s->slot[k].owner = s;
        s->slot[k].idx = k;
        lv_obj_add_event_cb(box, kit_ui__sigla_tap_cb,     LV_EVENT_CLICKED,  &s->slot[k]);
        lv_obj_add_event_cb(box, kit_ui__sigla_press_cb,   LV_EVENT_PRESSED,  &s->slot[k]);
        lv_obj_add_event_cb(box, kit_ui__sigla_release_cb, LV_EVENT_RELEASED, &s->slot[k]);

        s->box[k] = box;
        s->lbl[k] = kit_ui_label(box, "A", KIT_COLOR_TEXT, &kit_display_72, 0);
        lv_obj_center(s->lbl[k]);
    }
    kit_ui__sigla_paint(s);

    s->reset_btn = lv_obj_create(parent);
    lv_obj_set_size(s->reset_btn, LV_SIZE_CONTENT, 56);
    lv_obj_set_style_pad_hor(s->reset_btn, 24, 0);
    lv_obj_set_style_bg_color(s->reset_btn, lv_color_hex(KIT_COLOR_SURFACE), 0);
    lv_obj_set_style_border_width(s->reset_btn, 0, 0);
    lv_obj_set_style_radius(s->reset_btn, 28, 0);
    lv_obj_remove_flag(s->reset_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s->reset_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s->reset_btn, 10);
    lv_obj_add_event_cb(s->reset_btn, kit_ui__sigla_reset_cb, LV_EVENT_CLICKED, s);
    const char *rl = (opts && opts->reset_label) ? opts->reset_label
                     : (s->allow_blank ? "APAGAR" : "REDEFINIR");
    lv_obj_center(kit_ui_label(s->reset_btn, rl, KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2));
}

/** Mostra/esconde a fileira e o REDEFINIR (ex: só quando entra no top-5). */
KIT_UI_DEF void kit_ui_sigla_show(kit_ui_sigla_t *s, bool show)
{
    if (show) { lv_obj_remove_flag(s->row, LV_OBJ_FLAG_HIDDEN);
                lv_obj_remove_flag(s->reset_btn, LV_OBJ_FLAG_HIDDEN); }
    else      { lv_obj_add_flag(s->row, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(s->reset_btn, LV_OBJ_FLAG_HIDDEN); }
}

/* --- QR code tocável (padrão do KIT) ---------------------------------- */

/**
 * QR code com a legenda "Toque para expandir" embaixo. Tocar abre um overlay
 * de tela cheia com o código no maior tamanho que a tela comporta e o brilho
 * no máximo — numa tela de 1,8" é o que faz a câmera do celular enganchar de
 * primeira. Fechar (toque em qualquer ponto) devolve o brilho anterior.
 *
 * O QR é a única exceção ao preto AMOLED: fundo branco, código em
 * `KIT_COLOR_BG` (ver design-language.md, "Contraste").
 *
 *   static kit_ui_qr_t s_qr;
 *   kit_ui_qr(&s_qr, parent, "https://exemplo/x");   // monta
 *   kit_ui_qr_set(&s_qr, url);                        // troca o link
 *   kit_ui_qr_close(&s_qr);                           // no tool_destroy
 */
typedef struct {
    lv_obj_t *qr;          /* o lv_qrcode inline */
    lv_obj_t *hint;        /* "Toque para expandir" */
    lv_obj_t *overlay;     /* tela cheia enquanto expandido (NULL fechado) */
    char      data[KIT_UI_QR_MAX];
    uint32_t  len;         /* bytes do link (sem o terminador) */
    uint8_t   saved_bright;
} kit_ui_qr_t;

KIT_UI_DEF void kit_ui__qr_style(lv_obj_t *q)
{
    lv_qrcode_set_dark_color(q, lv_color_hex(KIT_COLOR_BG));
    lv_qrcode_set_light_color(q, lv_color_hex(0xFFFFFF));
    lv_qrcode_set_quiet_zone(q, true);
    lv_obj_set_style_border_width(q, 8, 0);
    lv_obj_set_style_border_color(q, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(q, 3, 0);
}

KIT_UI_DEF void kit_ui_qr_close(kit_ui_qr_t *q)
{
    if (!q->overlay) return;
    lv_obj_delete(q->overlay);
    q->overlay = NULL;
    if (kit_ui__api && kit_ui__api->display)
        kit_ui__api->display->set_brightness(q->saved_bright);
}

/** Esquece o QR sem deletar nada — para o `tool_destroy`, onde a screen
 *  inteira já foi apagada (e com ela o overlay). Devolve o brilho. */
KIT_UI_DEF void kit_ui_qr_reset(kit_ui_qr_t *q)
{
    if (q->overlay && kit_ui__api && kit_ui__api->display)
        kit_ui__api->display->set_brightness(q->saved_bright);
    q->overlay = NULL;
    q->qr = NULL;
    q->hint = NULL;
    q->data[0] = '\0';
    q->len = 0;
}

KIT_UI_DEF void kit_ui__qr_close_cb(lv_event_t *e)
{
    kit_ui_qr_close((kit_ui_qr_t *)lv_event_get_user_data(e));
}

KIT_UI_DEF void kit_ui__qr_open_cb(lv_event_t *e)
{
    kit_ui_qr_t *q = (kit_ui_qr_t *)lv_event_get_user_data(e);
    if (q->overlay || !q->data[0]) return;
    if (!kit_ui__api || !kit_ui__api->display) return;   /* sem display API não expande */
    kit_ui_click();

    q->saved_bright = kit_ui__api->display->get_brightness();
    kit_ui__api->display->set_brightness(KIT_UI_QR_BRIGHT);

    lv_obj_t *root = kit_ui__api->display->get_screen();
    lv_obj_t *ov = kit_ui_rect(root, KIT_UI_SCREEN_W, KIT_UI_SCREEN_H, KIT_COLOR_BG, 0);
    lv_obj_set_pos(ov, 0, 0);
    lv_obj_add_flag(ov, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ov, kit_ui__qr_close_cb, LV_EVENT_CLICKED, q);
    q->overlay = ov;

    lv_obj_t *big = lv_qrcode_create(ov);
    lv_qrcode_set_size(big, KIT_UI_QR_BIG);
    kit_ui__qr_style(big);
    lv_qrcode_update(big, q->data, q->len);
    lv_obj_align(big, LV_ALIGN_CENTER, 0, -18);

    lv_obj_t *l = kit_ui_label(ov, "Toque para fechar", KIT_COLOR_TEXT_MUTED,
                               &kit_sans_22, 0);
    lv_obj_align(l, LV_ALIGN_BOTTOM_MID, 0, -22);
}

KIT_UI_DEF void kit_ui_qr_set(kit_ui_qr_t *q, const char *url)
{
    uint32_t n = 0;
    while (url[n] && n < (uint32_t)(KIT_UI_QR_MAX - 1)) { q->data[n] = url[n]; n++; }
    q->data[n] = '\0';
    q->len = n;
    lv_qrcode_update(q->qr, q->data, n);
    if (q->overlay) { kit_ui_qr_close(q); }   /* o expandido some: link mudou */
}

KIT_UI_DEF void kit_ui_qr(kit_ui_qr_t *q, lv_obj_t *parent, const char *url)
{
    q->overlay = NULL;
    q->saved_bright = KIT_UI_QR_BRIGHT;

    q->qr = lv_qrcode_create(parent);
    lv_qrcode_set_size(q->qr, KIT_UI_QR_SIZE);
    kit_ui__qr_style(q->qr);
    lv_obj_add_flag(q->qr, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(q->qr, 8);
    lv_obj_add_event_cb(q->qr, kit_ui__qr_open_cb, LV_EVENT_CLICKED, q);

    q->hint = kit_ui_label(parent, "Toque para expandir", KIT_COLOR_TEXT_MUTED,
                           &kit_sans_22, 0);
    lv_obj_add_flag(q->hint, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(q->hint, 8);
    lv_obj_add_event_cb(q->hint, kit_ui__qr_open_cb, LV_EVENT_CLICKED, q);

    kit_ui_qr_set(q, url);
}

#undef KIT_UI_DEF

#endif /* !KIT_SDK_STUBS — no build nativo este header é vazio de propósito */
