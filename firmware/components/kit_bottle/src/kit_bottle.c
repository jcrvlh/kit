#include "kit_bottle.h"
#include "kit_api.h"
#include "kit_display.h"
#include "kit_fonts.h"
#include "kit_theme.h"
#include "esp_log.h"
#include "lvgl.h"
#include <stdio.h>
#include <stdlib.h>

// Bottle Tool (Garrafa) — linguagem "Brutalist Bauhaus" (ver
// docs/design/design-language.md). Titlebar fixa + tileview de 2 páginas
// (arrasta na horizontal):
//   0 AJUSTE  — quantas setas giram (1 a 3)
//   1 PALCO   — o anel com as setas que giram (página inicial)
// Botão fixo GIRAR no rodapé; girar de qualquer página leva para o PALCO.
// Também dispara por toque no palco e pelo botão físico PWR. Sem histórico:
// a graça é só o giro. A saída é feita pela API (system->exit).
//
// As setas NÃO usam transform_rotation (o layer transformado animado travava
// a board no CO5300/PSRAM). Em vez disso são desenhadas à mão a cada frame
// num LV_EVENT_DRAW_MAIN: uma linha (haste) + um triângulo (bico) girados
// por trigonometria inteira do próprio LVGL. Cada tick só invalida a área
// do palco da seta — redraw parcial normal, sem layers. Com até 3 setas são
// 3 linhas + 3 triângulos por frame; cada seta tem seu próprio sorteio de
// força/atrito, então param apontando direções diferentes.

static const char *TAG = "KIT_BOTTLE";

#define B_PAD        16
#define B_CONTENT    (KIT_DISPLAY_WIDTH - 2 * B_PAD)          // 336
#define B_TITLEBAR   88
#define B_FOOT       104
#define B_CHIP       56
#define B_STEP       56
#define B_GO_H       76

#define B_PAGE_H     (KIT_DISPLAY_HEIGHT - B_TITLEBAR - B_FOOT) // 256
#define B_RING       224   // diâmetro do anel do "palco"
#define B_ARROW_BOX  184   // lado da área de desenho das setas (centrada no palco)
#define PAGES        2

// Geometria da seta (distâncias a partir do centro, em px).
#define B_R_TIP      76    // ponta do bico
#define B_R_TAIL     48    // cauda da haste (lado oposto)
#define B_HEAD_LEN   30    // comprimento do bico
#define B_HEAD_HW    19    // meia-largura da base do bico
#define B_SHAFT_W    10    // espessura da haste
#define B_HUB        9     // raio do disco central

// Física do giro (unidades: centésimos de grau; tick a cada B_TICK_MS).
// s_omega parte alto e aleatório, decai geometricamente (s_decay/1024 por
// tick) com um atrito constante por cima pra garantir que sempre pare.
#define B_TICK_MS       30
#define B_OMEGA_MIN     4200   // ~42 graus/tick  (~3,9 rev/s)
#define B_OMEGA_SPAN    3200   // +0..32 graus/tick
#define B_DECAY_MIN     984    // /1024  (~0,961)
#define B_DECAY_SPAN    12
#define B_FRICTION      40     // atrito constante (centésimos de grau/tick)
#define B_OMEGA_STOP    80     // 0,8 grau/tick

#define B_ARROWS_MIN    1
#define B_ARROWS_MAX    3

// --- estado ---------------------------------------------------------------
static uint32_t s_accent   = KIT_COLOR_BLUE;
static bool     s_spinning = false;
static int      s_count    = 1;                    // setas ativas (1..3)
static int32_t  s_angle_c[B_ARROWS_MAX];           // 0..35999 (centésimos de grau)
static int32_t  s_omega[B_ARROWS_MAX];
static int32_t  s_decay[B_ARROWS_MAX];
static lv_timer_t *s_timer = NULL;

// --- objetos LVGL -------------------------------------------------------
static lv_obj_t *s_screen     = NULL;
static lv_obj_t *s_tv         = NULL;
static lv_obj_t *s_tiles[PAGES];
static lv_obj_t *s_dots[PAGES];
static lv_obj_t *s_count_lbl  = NULL;
static lv_obj_t *s_count_minus = NULL;
static lv_obj_t *s_count_plus  = NULL;
static lv_obj_t *s_arrow      = NULL;
static lv_obj_t *s_hint_lbl   = NULL;
static lv_obj_t *s_go_btn     = NULL;

// --- helpers ----------------------------------------------------------

static const kit_api_table_t *api(void) { return kit_api_get_table(); }

static int32_t rnd_range(int32_t min, int32_t max)
{
    const kit_api_table_t *t = api();
    if (t && t->random) return t->random->range(min, max);
    return min + (rand() % (max - min + 1));
}

// cor de cada seta: a 0 é o accent da Tool; as demais, primitivas Bauhaus.
static uint32_t arrow_color(int i)
{
    switch (i) {
        case 0:  return s_accent;
        case 1:  return KIT_COLOR_YELLOW;
        default: return KIT_COLOR_GREEN;
    }
}

// espalha as setas em ângulos distintos pra não nascerem sobrepostas.
static void spread_arrows(void)
{
    int n = s_count < 1 ? 1 : s_count;
    for (int i = 0; i < B_ARROWS_MAX; i++) s_angle_c[i] = (int32_t)i * (36000 / n);
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

// --- persistência (Storage API) --------------------------------------------

static void load_prefs(void)
{
    const kit_api_table_t *t = api();
    if (!t || !t->storage) return;
    int32_t v;
    if (t->storage->get_i32("bottle_count", &v) == KIT_OK &&
        v >= B_ARROWS_MIN && v <= B_ARROWS_MAX)
        s_count = (int)v;
}

static void save_prefs(void)
{
    const kit_api_table_t *t = api();
    if (!t || !t->storage) return;
    t->storage->set_i32("bottle_count", s_count);
}

// --- desenho das setas ------------------------------------------------

static void draw_one_arrow(lv_layer_t *layer, int32_t cx, int32_t cy,
                           int32_t angle_c, lv_color_t c)
{
    int16_t deg = (int16_t)((angle_c / 100) % 360);
    int32_t si = lv_trigo_sin(deg);          // seno   * 32768
    int32_t co = lv_trigo_cos(deg);          // cosseno * 32768

    // heading unitário = (si, -co)/32768 (grau 0 = pra cima, gira no sentido horário)
    #define B_PROJ(dist, ox, oy) do {                              \
        (ox) = cx + (int32_t)(((int64_t)(dist) * si) >> 15);       \
        (oy) = cy - (int32_t)(((int64_t)(dist) * co) >> 15);       \
    } while (0)

    int32_t tipx, tipy, tailx, taily, neckx, necky;
    B_PROJ(B_R_TIP,               tipx,  tipy);
    B_PROJ(-B_R_TAIL,             tailx, taily);
    B_PROJ(B_R_TIP - B_HEAD_LEN,  neckx, necky);

    // perpendicular ao heading = (co, si)/32768
    int32_t px = (int32_t)(((int64_t)B_HEAD_HW * co) >> 15);
    int32_t py = (int32_t)(((int64_t)B_HEAD_HW * si) >> 15);

    lv_draw_line_dsc_t ld;
    lv_draw_line_dsc_init(&ld);
    ld.color = c;
    ld.width = B_SHAFT_W;
    ld.round_start = 1;
    ld.round_end = 1;
    ld.p1.x = tailx; ld.p1.y = taily;
    ld.p2.x = neckx; ld.p2.y = necky;
    lv_draw_line(layer, &ld);

    lv_draw_triangle_dsc_t td;
    lv_draw_triangle_dsc_init(&td);
    td.color = c;
    td.opa = LV_OPA_COVER;
    td.p[0].x = tipx;        td.p[0].y = tipy;
    td.p[1].x = neckx + px;  td.p[1].y = necky + py;
    td.p[2].x = neckx - px;  td.p[2].y = necky - py;
    lv_draw_triangle(layer, &td);

    #undef B_PROJ
}

static void arrow_draw_cb(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    if (!layer) return;

    lv_area_t a;
    lv_obj_get_coords(s_arrow, &a);
    int32_t cx = (a.x1 + a.x2) / 2;
    int32_t cy = (a.y1 + a.y2) / 2;

    for (int i = 0; i < s_count; i++)
        draw_one_arrow(layer, cx, cy, s_angle_c[i], lv_color_hex(arrow_color(i)));

    // disco central por cima de todas as hastes
    lv_draw_rect_dsc_t rd;
    lv_draw_rect_dsc_init(&rd);
    rd.bg_color = lv_color_hex(s_accent);
    rd.bg_opa = LV_OPA_COVER;
    rd.radius = LV_RADIUS_CIRCLE;
    lv_area_t hub = { cx - B_HUB, cy - B_HUB, cx + B_HUB, cy + B_HUB };
    lv_draw_rect(layer, &rd, &hub);
}

// --- giro -----------------------------------------------------------

static void spin_tick_cb(lv_timer_t *t);

static void do_spin(void)
{
    if (s_spinning || !s_arrow) return;
    s_spinning = true;

    for (int i = 0; i < s_count; i++) {
        s_omega[i] = B_OMEGA_MIN + rnd_range(0, B_OMEGA_SPAN);
        s_decay[i] = B_DECAY_MIN + rnd_range(0, B_DECAY_SPAN);
    }

    lv_obj_add_flag(s_hint_lbl, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(s_go_btn, LV_OPA_60, 0);   // "ocupado"
    if (s_tv) lv_tileview_set_tile_by_index(s_tv, 1, 0, LV_ANIM_OFF);

    // Catraca desacelerando + "assentou": um SFX só, de amplitude baixa, casado
    // com a duração típica do giro (~2 s). Renderizado na task de áudio, então
    // não segura a animação; e por ser SFX (não bipes em rajada) não estoura no
    // volume máximo.
    const kit_api_table_t *a = api();
    if (a && a->audio) a->audio->sfx(KIT_SFX_BOTTLE_SPIN);

    s_timer = lv_timer_create(spin_tick_cb, B_TICK_MS, NULL);
}

static void spin_tick_cb(lv_timer_t *t)
{
    (void)t;

    bool any = false;
    for (int i = 0; i < s_count; i++) {
        if (s_omega[i] <= B_OMEGA_STOP) continue;
        s_angle_c[i] = (s_angle_c[i] + s_omega[i]) % 36000;
        s_omega[i] = ((s_omega[i] * s_decay[i]) >> 10) - B_FRICTION;
        any = true;
    }

    lv_obj_invalidate(s_arrow);
    if (any) return;

    // pararam: cada seta ficou apontando uma direção qualquer
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    s_spinning = false;
    lv_obj_set_style_opa(s_go_btn, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_hint_lbl, LV_OBJ_FLAG_HIDDEN);
}

void kit_bottle_spin(void)
{
    do_spin();
}

// --- callbacks ------------------------------------------------------

static void back_cb(lv_event_t *e)
{
    (void)e;
    const kit_api_table_t *t = api();
    if (t && t->system) t->system->exit();
}

static void spin_cb(lv_event_t *e)
{
    (void)e;
    do_spin();
}

static void tv_changed_cb(lv_event_t *e);
static void sync_steppers(void);

static void step_cb(lv_event_t *e)
{
    if (s_spinning) return;
    int delta = (int)(intptr_t)lv_event_get_user_data(e);
    int next = s_count + delta;
    if (next < B_ARROWS_MIN || next > B_ARROWS_MAX) return;
    s_count = next;
    spread_arrows();
    sync_steppers();
    if (s_arrow) lv_obj_invalidate(s_arrow);
    save_prefs();
}

// --- sincronização de UI -------------------------------------------

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

static void tv_changed_cb(lv_event_t *e)
{
    (void)e;
    sync_dots();
}

static void sync_steppers(void)
{
    lv_label_set_text_fmt(s_count_lbl, "%d", s_count);
    lv_obj_set_style_opa(s_count_minus, s_count <= B_ARROWS_MIN ? LV_OPA_30 : LV_OPA_COVER, 0);
    lv_obj_set_style_opa(s_count_plus,  s_count >= B_ARROWS_MAX ? LV_OPA_30 : LV_OPA_COVER, 0);
}

// --- construção da tela -------------------------------------------

static void build_titlebar(void)
{
    lv_obj_t *chip = lv_obj_create(s_screen);
    lv_obj_set_size(chip, B_CHIP, B_CHIP);
    lv_obj_set_style_bg_color(chip, lv_color_hex(KIT_COLOR_SURFACE), 0);
    lv_obj_set_style_border_width(chip, 0, 0);
    lv_obj_set_style_radius(chip, 18, 0);
    lv_obj_set_style_pad_all(chip, 0, 0);
    lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(chip, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(chip, 12);
    lv_obj_add_event_cb(chip, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(chip, LV_ALIGN_TOP_LEFT, B_PAD, 16);

    lv_obj_t *g = add_label(chip, KIT_ICON_BACK, KIT_COLOR_TEXT, &kit_display_44, 0);
    lv_obj_center(g);

    lv_obj_t *title = add_label(s_screen, "GARRAFA", KIT_COLOR_TEXT, &kit_mono_26, 3);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, B_PAD + B_CHIP + 12, 30);

    // indicador de página (2 pontos) no canto direito da titlebar
    lv_obj_t *dots = plain_box(s_screen);
    lv_obj_set_size(dots, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(dots, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dots, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(dots, 6, 0);
    lv_obj_align(dots, LV_ALIGN_TOP_RIGHT, -B_PAD, 40);
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

static lv_obj_t *make_step_btn(lv_obj_t *parent, const char *sym, int delta)
{
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_set_size(b, B_STEP, B_STEP);
    lv_obj_set_style_bg_color(b, lv_color_hex(KIT_COLOR_SURFACE), 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_radius(b, 16, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(b, 8);
    lv_obj_add_event_cb(b, step_cb, LV_EVENT_CLICKED, (void *)(intptr_t)delta);
    lv_obj_t *l = add_label(b, sym, KIT_COLOR_TEXT, &kit_display_44, 0);
    lv_obj_center(l);
    return b;
}

// Página 0 — AJUSTE: quantas setas giram.
static void build_page_setup(lv_obj_t *tile)
{
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_t *p = lv_obj_create(tile);
    lv_obj_remove_style_all(p);
    lv_obj_set_size(p, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_hor(p, B_PAD, 0);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(p, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(p, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(p, 18, 0);

    add_label(p, "SETAS", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);

    lv_obj_t *row = plain_box(p);
    lv_obj_set_size(row, lv_pct(100), B_STEP);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 8, 0);

    s_count_minus = make_step_btn(row, "-", -1);

    s_count_lbl = add_label(row, "1", KIT_COLOR_TEXT, &kit_display_44, 0);
    lv_obj_set_width(s_count_lbl, 76);
    lv_obj_set_style_text_align(s_count_lbl, LV_TEXT_ALIGN_CENTER, 0);

    s_count_plus = make_step_btn(row, KIT_ICON_PLUS, +1);

    add_label(p, "PARA ESCOLHER PARES OU TRIOS", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 1);
}

// Página 1 — PALCO: anel discreto (as pessoas ficam em volta) + as setas
// desenhadas no centro.
static void build_page_stage(lv_obj_t *tile)
{
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_t *stage = lv_obj_create(tile);
    lv_obj_remove_style_all(stage);
    lv_obj_set_size(stage, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(stage, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(stage, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(stage, spin_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *ring = lv_obj_create(stage);
    lv_obj_remove_style_all(ring);
    lv_obj_set_size(ring, B_RING, B_RING);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ring, 2, 0);
    lv_obj_set_style_border_color(ring, lv_color_hex(KIT_COLOR_LINE), 0);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(ring);

    s_hint_lbl = add_label(stage, "TOQUE PARA GIRAR", KIT_COLOR_TEXT_MUTED,
                           &kit_mono_16, 2);
    lv_obj_align(s_hint_lbl, LV_ALIGN_CENTER, 0, B_PAGE_H / 2 - 24);

    s_arrow = lv_obj_create(stage);
    lv_obj_remove_style_all(s_arrow);
    lv_obj_set_size(s_arrow, B_ARROW_BOX, B_ARROW_BOX);
    lv_obj_clear_flag(s_arrow, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_arrow, LV_OBJ_FLAG_CLICKABLE);   // toque cai no palco
    lv_obj_center(s_arrow);
    lv_obj_add_event_cb(s_arrow, arrow_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
}

static void build_tileview(void)
{
    s_tv = lv_tileview_create(s_screen);
    lv_obj_set_size(s_tv, KIT_DISPLAY_WIDTH, B_PAGE_H);
    lv_obj_set_pos(s_tv, 0, B_TITLEBAR);
    lv_obj_set_style_bg_opa(s_tv, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_tv, 0, 0);
    lv_obj_set_scrollbar_mode(s_tv, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(s_tv, tv_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_tiles[0] = lv_tileview_add_tile(s_tv, 0, 0, LV_DIR_HOR);
    s_tiles[1] = lv_tileview_add_tile(s_tv, 1, 0, LV_DIR_HOR);

    build_page_setup(s_tiles[0]);
    build_page_stage(s_tiles[1]);
}

static void build_footer(void)
{
    // lv_obj puro (sem tema) pelo mesmo motivo da Dice Tool: o lv_button
    // aplica transform no estado PRESSED e desalinha o rótulo.
    s_go_btn = lv_obj_create(s_screen);
    lv_obj_set_size(s_go_btn, B_CONTENT, B_GO_H);
    lv_obj_set_style_radius(s_go_btn, B_GO_H / 2, 0);
    lv_obj_set_style_border_width(s_go_btn, 0, 0);
    lv_obj_set_style_shadow_width(s_go_btn, 0, 0);
    lv_obj_set_style_pad_all(s_go_btn, 0, 0);
    lv_obj_set_style_bg_color(s_go_btn, lv_color_hex(s_accent), 0);
    lv_obj_set_style_bg_opa(s_go_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(s_go_btn, LV_OPA_80, LV_STATE_PRESSED);
    lv_obj_clear_flag(s_go_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_go_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_go_btn, 8);
    lv_obj_align(s_go_btn, LV_ALIGN_BOTTOM_MID, 0, -18);
    lv_obj_add_event_cb(s_go_btn, spin_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *l = add_label(s_go_btn, "GIRAR", KIT_COLOR_ON_COLOR, &kit_mono_26, 3);
    lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
    lv_obj_center(l);
}

// --- ciclo de vida ------------------------------------------------

kit_err_t kit_bottle_start(uint32_t accent)
{
    if (s_screen) kit_bottle_destroy();

    ESP_LOGI(TAG, "Montando Bottle Tool...");
    s_accent = accent ? accent : KIT_COLOR_BLUE;
    s_spinning = false;
    s_count = 1;
    for (int i = 0; i < B_ARROWS_MAX; i++) s_omega[i] = 0;
    load_prefs();
    spread_arrows();

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(KIT_COLOR_BG), 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    build_titlebar();
    build_tileview();
    build_footer();

    // começa no palco (a página principal)
    lv_tileview_set_tile_by_index(s_tv, 1, 0, LV_ANIM_OFF);

    sync_steppers();
    sync_dots();

    lv_screen_load(s_screen);
    return KIT_OK;
}

void kit_bottle_destroy(void)
{
    ESP_LOGI(TAG, "Encerrando Bottle Tool.");
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    s_spinning = false;

    if (s_screen) {
        lv_obj_delete(s_screen);
        s_screen = NULL;
    }
    s_tv = s_arrow = s_hint_lbl = s_go_btn = NULL;
    s_count_lbl = s_count_minus = s_count_plus = NULL;
}
