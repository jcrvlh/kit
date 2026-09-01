#include "kit_timer.h"
#include "kit_api.h"
#include "kit_display.h"
#include "kit_fonts.h"
#include "kit_theme.h"
#include "esp_log.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>

// Timer Tool — linguagem "Brutalist Bauhaus" (ver docs/design/design-language.md).
// Titlebar fixa + tileview de 2 páginas (arrasta na horizontal):
//   0 AJUSTE   — modo (CRONÔMETRO ↑ / REGRESSIVO ↓); no regressivo, tempos
//                fixos + roda MM:SS (a mesma lv_roller da Coin Tool).
//   1 RELÓGIO  — só o mostrador MM:SS e os botões PARAR / COMEÇAR (a página
//                inicial). O botão COMEÇAR alterna COMEÇAR → PAUSAR → CONTINUAR.
// PWR físico (e chacoalhar) fazem a mesma coisa que o botão COMEÇAR
// (kit_timer_toggle -> Runtime). A saída é feita pela API (system->exit).
//
// Enquanto conta, a Tool segura o repouso/desligamento (kit_power keep-awake) e
// só escurece o painel após ~15 s sem toque — sem apagar. Ao zerar a contagem
// regressiva, roda uma animação de anéis + "TEMPO". Sem áudio por enquanto.

static const char *TAG = "KIT_TIMER";

// ---------------------------------------------------------------------------
// Layout (espelha as métricas da Dice / Coin)
// ---------------------------------------------------------------------------
#define T_PAD        16
#define T_CONTENT    (KIT_DISPLAY_WIDTH - 2 * T_PAD)              // 336
#define T_TITLEBAR   88
#define T_PAGE_H     (KIT_DISPLAY_HEIGHT - T_TITLEBAR)            // 360
#define T_CHIP       56
#define T_BTN_H      76
#define T_BTN_W      ((T_CONTENT - 12) / 2)                       // 162
#define T_BTN_MARGIN 18
#define PAGES        2

// Contagem
#define MODE_UP      0    // cronômetro (conta para cima)
#define MODE_DOWN    1    // regressivo (conta para baixo)
#define SECS_MIN     1
#define SECS_MAX     (99 * 60 + 59)   // 99:59

#define RUN_IDLE     0
#define RUN_RUNNING  1
#define RUN_PAUSED   2

// Brilho reduzido
#define DIM_AFTER_MS 15000
#define DIM_BRIGHT   5

// Animação de fim — pisca fundo verde ↔ preto (igual em espírito à tela de
// "CARREGANDO"): disco + "TEMPO", alternando até tocar na tela.
#define FIN_TICK_MS  420

static const int PRESET_MIN[] = { 3, 5, 10, 15, 30 };
#define PRESET_COUNT ((int)(sizeof(PRESET_MIN) / sizeof(PRESET_MIN[0])))

// ---------------------------------------------------------------------------
// Estado
// ---------------------------------------------------------------------------
static int      s_mode      = MODE_DOWN;
static int      s_set_secs  = 300;        // tempo configurado (regressivo)
static int      s_cur_secs  = 300;        // valor no mostrador
static int      s_run       = RUN_IDLE;
static uint32_t s_accent    = KIT_COLOR_GREEN;
static uint8_t  s_bright_normal = 80;
static bool     s_dimmed    = false;
static bool     s_colon_vis = true;

static lv_timer_t *s_count_timer = NULL;   // 1 s — conta
static lv_timer_t *s_anim_timer  = NULL;   // 200 ms — pisca o "dois pontos" + brilho
static lv_timer_t *s_fin_timer   = NULL;   // ~420 ms — pisca o fundo da tela de fim

static char s_min_opts[420];
static char s_sec_opts[240];

// ---------------------------------------------------------------------------
// Objetos LVGL
// ---------------------------------------------------------------------------
static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_tv     = NULL;
static lv_obj_t *s_tiles[PAGES];
static lv_obj_t *s_dots[PAGES];

// Página 0 — Ajuste
static lv_obj_t *s_mode_pills[2];
static lv_obj_t *s_mode_pill_lbls[2];
static lv_obj_t *s_down_cnt   = NULL;
static lv_obj_t *s_preset_pills[PRESET_COUNT];
static lv_obj_t *s_preset_pill_lbls[PRESET_COUNT];
static lv_obj_t *s_roller_min = NULL;
static lv_obj_t *s_roller_sec = NULL;
static lv_obj_t *s_hint_lbl   = NULL;

// Página 1 — Relógio
static lv_obj_t *s_modetag_lbl = NULL;
static lv_obj_t *s_mm_lbl      = NULL;
static lv_obj_t *s_ss_lbl      = NULL;
static lv_obj_t *s_colon       = NULL;
static lv_obj_t *s_colon_sq[2] = { NULL, NULL };
static lv_obj_t *s_go_btn      = NULL;
static lv_obj_t *s_go_lbl      = NULL;
static lv_obj_t *s_stop_btn    = NULL;

// Overlay de fim
static lv_obj_t *s_finish     = NULL;
static lv_obj_t *s_fin_disc   = NULL;
static lv_obj_t *s_fin_icon   = NULL;
static lv_obj_t *s_fin_word   = NULL;
static lv_obj_t *s_fin_hint   = NULL;
static bool     s_fin_on      = false;
static uint32_t s_fin_ticks   = 0;

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

// Retângulo/círculo cru para montar ícones geométricos (a cor é aplicada depois).
static lv_obj_t *shape(lv_obj_t *parent, int w, int h, int radius, int border)
{
    lv_obj_t *o = plain_box(parent);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, radius, 0);
    if (border) lv_obj_set_style_border_width(o, border, 0);
    else        lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    return o;
}

static lv_obj_t *field_label(lv_obj_t *parent, const char *txt)
{
    return add_label(parent, txt, KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);
}

static void fill_num_opts(char *buf, size_t n, int count)
{
    char *p = buf;
    for (int i = 0; i < count; i++)
        p += snprintf(p, n - (size_t)(p - buf), "%s%02d", i ? "\n" : "", i);
}

// ---------------------------------------------------------------------------
// Persistência (Storage API)
// ---------------------------------------------------------------------------

static void load_prefs(void)
{
    const kit_api_table_t *t = api();
    if (!t || !t->storage) return;
    int32_t v;
    if (t->storage->get_i32("timer_mode", &v) == KIT_OK && (v == MODE_UP || v == MODE_DOWN))
        s_mode = (int)v;
    if (t->storage->get_i32("timer_secs", &v) == KIT_OK && v >= SECS_MIN && v <= SECS_MAX)
        s_set_secs = (int)v;
}

static void save_prefs(void)
{
    const kit_api_table_t *t = api();
    if (!t || !t->storage) return;
    t->storage->set_i32("timer_mode", s_mode);
    t->storage->set_i32("timer_secs", s_set_secs);
}

// ---------------------------------------------------------------------------
// Brilho / keep-awake
// ---------------------------------------------------------------------------

static void set_brightness(uint8_t pct)
{
    const kit_api_table_t *t = api();
    if (t && t->display) t->display->set_brightness(pct);
}

static void wake_display(void)
{
    if (!s_dimmed) return;
    s_dimmed = false;
    set_brightness(s_bright_normal);
}

static void sync_keep_awake(void)
{
    const kit_api_table_t *t = api();
    if (!t || !t->power) return;
    bool finishing = (s_finish && !lv_obj_has_flag(s_finish, LV_OBJ_FLAG_HIDDEN));
    t->power->keep_awake(s_run != RUN_IDLE || finishing);
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

static void sync_mode_pills(void)
{
    uint32_t sel_txt = on_accent();
    for (int i = 0; i < 2; i++) {
        bool sel = (i == s_mode);
        lv_obj_set_style_bg_color(s_mode_pills[i],
            lv_color_hex(sel ? s_accent : KIT_COLOR_SURFACE), 0);
        lv_obj_set_style_text_color(s_mode_pill_lbls[i],
            lv_color_hex(sel ? sel_txt : KIT_COLOR_TEXT), 0);
    }
    if (s_down_cnt) {
        if (s_mode == MODE_DOWN) lv_obj_clear_flag(s_down_cnt, LV_OBJ_FLAG_HIDDEN);
        else                     lv_obj_add_flag(s_down_cnt, LV_OBJ_FLAG_HIDDEN);
    }
}

static void sync_presets(void)
{
    uint32_t sel_txt = on_accent();
    for (int i = 0; i < PRESET_COUNT; i++) {
        bool sel = (PRESET_MIN[i] * 60 == s_set_secs);
        lv_obj_set_style_bg_color(s_preset_pills[i],
            lv_color_hex(sel ? s_accent : KIT_COLOR_SURFACE), 0);
        lv_obj_set_style_text_color(s_preset_pill_lbls[i],
            lv_color_hex(sel ? sel_txt : KIT_COLOR_TEXT), 0);
    }
}

static void sync_rollers(void)
{
    if (s_roller_min) lv_roller_set_selected(s_roller_min, s_set_secs / 60, LV_ANIM_OFF);
    if (s_roller_sec) lv_roller_set_selected(s_roller_sec, s_set_secs % 60, LV_ANIM_OFF);
}

static void sync_hint(void)
{
    if (!s_hint_lbl) return;
    if (s_mode == MODE_UP) {
        lv_label_set_text(s_hint_lbl, "O CRON\xC3\x94METRO COME\xC3\x87""A EM 00:00.");
    } else {
        lv_label_set_text_fmt(s_hint_lbl, "VAI COME\xC3\x87""AR EM %02d:%02d.",
                              s_set_secs / 60, s_set_secs % 60);
    }
}

static void sync_clock(void)
{
    if (!s_mm_lbl) return;
    int t = s_cur_secs;
    if (t < 0) t = 0;
    if (t > SECS_MAX) t = SECS_MAX;
    lv_label_set_text_fmt(s_mm_lbl, "%02d", t / 60);
    lv_label_set_text_fmt(s_ss_lbl, "%02d", t % 60);
    if (s_modetag_lbl)
        lv_label_set_text(s_modetag_lbl, s_mode == MODE_UP ? "CRON\xC3\x94METRO" : "REGRESSIVO");
}

static void show_colon(bool on)
{
    s_colon_vis = on;
    if (!s_colon_sq[0]) return;
    // bg_opa nos dois quadrados (não `opa` no container — `opa` intermediário
    // força layer buffer, regra da board). COVER/TRANSP mantém o espaço no flex,
    // então os dígitos não pulam.
    for (int i = 0; i < 2; i++)
        lv_obj_set_style_bg_opa(s_colon_sq[i], on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
}

static void sync_buttons(void)
{
    if (!s_go_lbl) return;
    const char *go = (s_run == RUN_IDLE)   ? "COME\xC3\x87""AR"
                   : (s_run == RUN_PAUSED) ? "CONTINUAR"
                                           : "PAUSAR";
    lv_label_set_text(s_go_lbl, go);
    // O texto do botão muda de comprimento; repinta o botão inteiro (fundo +
    // rótulo) para não deixar "fantasma" do texto anterior.
    lv_obj_invalidate(s_go_btn);

    bool stop_on = (s_run != RUN_IDLE) ||
                   (s_mode == MODE_UP ? s_cur_secs != 0 : s_cur_secs != s_set_secs);
    // esmaece via border/text opa (parte), não `opa` do objeto — `opa`
    // intermediário força layer buffer (regra da board).
    lv_opa_t o = stop_on ? LV_OPA_COVER : LV_OPA_40;
    lv_obj_set_style_border_opa(s_stop_btn, o, 0);
    lv_obj_set_style_text_opa(s_stop_btn, o, 0);
    if (stop_on) lv_obj_add_flag(s_stop_btn, LV_OBJ_FLAG_CLICKABLE);
    else         lv_obj_clear_flag(s_stop_btn, LV_OBJ_FLAG_CLICKABLE);
}

// ---------------------------------------------------------------------------
// Contagem
// ---------------------------------------------------------------------------

static void trigger_finish(void);
static void count_tick_cb(lv_timer_t *t);

static void start_counting(void)
{
    if (s_count_timer) return;
    s_count_timer = lv_timer_create(count_tick_cb, 1000, NULL);
}

static void stop_counting(void)
{
    if (s_count_timer) { lv_timer_delete(s_count_timer); s_count_timer = NULL; }
}

static void count_tick_cb(lv_timer_t *t)
{
    (void)t;
    if (s_mode == MODE_UP) {
        if (s_cur_secs < SECS_MAX) s_cur_secs++;
    } else {
        if (s_cur_secs > 0) s_cur_secs--;
        if (s_cur_secs <= 0) {
            s_cur_secs = 0;
            sync_clock();
            stop_counting();
            s_run = RUN_IDLE;
            trigger_finish();
            return;
        }
    }
    sync_clock();
}

// Alterna COMEÇAR → PAUSAR → CONTINUAR (botão primário / PWR / chacoalhar).
static void toggle(void)
{
    if (!s_screen) return;
    if (s_finish && !lv_obj_has_flag(s_finish, LV_OBJ_FLAG_HIDDEN)) return;

    wake_display();

    if (s_run == RUN_IDLE) {
        if (s_mode == MODE_DOWN) {
            if (s_set_secs < SECS_MIN) s_set_secs = SECS_MIN;
            s_cur_secs = s_set_secs;
        } else {
            s_cur_secs = 0;
        }
        s_run = RUN_RUNNING;
        start_counting();
        if (s_tv) lv_tileview_set_tile_by_index(s_tv, 1, 0, LV_ANIM_OFF);
    } else if (s_run == RUN_RUNNING) {
        s_run = RUN_PAUSED;
        stop_counting();
    } else { // RUN_PAUSED
        s_run = RUN_RUNNING;
        start_counting();
    }

    show_colon(true);
    sync_clock();
    sync_buttons();
    sync_keep_awake();
}

static void stop_reset(void)
{
    wake_display();
    stop_counting();
    s_run = RUN_IDLE;
    s_cur_secs = (s_mode == MODE_UP) ? 0 : s_set_secs;
    show_colon(true);
    sync_clock();
    sync_buttons();
    sync_keep_awake();
}

void kit_timer_toggle(void)
{
    toggle();
}

// ---------------------------------------------------------------------------
// Animação de 200 ms — pisca o "dois pontos" + gerencia o brilho reduzido
// ---------------------------------------------------------------------------

static void anim_tick_cb(lv_timer_t *t)
{
    (void)t;

    // "dois pontos" pisca só enquanto conta
    static int blink_acc = 0;
    if (s_run == RUN_RUNNING) {
        if (++blink_acc >= 3) { blink_acc = 0; show_colon(!s_colon_vis); }
    } else if (!s_colon_vis) {
        blink_acc = 0;
        show_colon(true);
    }

    // brilho reduzido depois de ~15 s sem toque, apenas contando
    uint32_t idle = lv_display_get_inactive_time(NULL);
    bool want_dim = (s_run == RUN_RUNNING) && (idle >= DIM_AFTER_MS);
    if (want_dim && !s_dimmed) {
        s_dimmed = true;
        set_brightness(DIM_BRIGHT);
    } else if (!want_dim && s_dimmed) {
        s_dimmed = false;
        set_brightness(s_bright_normal);
    }
}

// ---------------------------------------------------------------------------
// Animação de fim — mesma família visual da tela "CARREGANDO" (disco + rótulo),
// piscando o fundo verde ↔ preto até tocar na tela. Sem áudio, sem transform.
// ---------------------------------------------------------------------------

static void fin_paint(bool green)
{
    // fundo verde: miolo preto (disco preto, ícone/rótulos pretos).
    // fundo preto: miolo verde. Sempre alto contraste nas duas fases.
    uint32_t bg = green ? s_accent : KIT_COLOR_BG;
    uint32_t fg = green ? KIT_COLOR_BG : s_accent;

    lv_obj_set_style_bg_color(s_finish, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_color(s_fin_disc, lv_color_hex(fg), 0);
    lv_obj_set_style_text_color(s_fin_word, lv_color_hex(fg), 0);
    lv_obj_set_style_text_color(s_fin_hint, lv_color_hex(fg), 0);

    // ícone de timer (formas geométricas) — cor de contraste com o disco
    uint32_t nchild = lv_obj_get_child_count(s_fin_icon);
    for (uint32_t i = 0; i < nchild; i++) {
        lv_obj_t *c = lv_obj_get_child(s_fin_icon, i);
        lv_obj_set_style_bg_color(c, lv_color_hex(bg), 0);
        lv_obj_set_style_border_color(c, lv_color_hex(bg), 0);
    }
}

static void play_alarm(void)
{
    const kit_api_table_t *t = api();
    if (t && t->audio) t->audio->sfx(KIT_SFX_TIMER_DONE);
}

static void fin_tick_cb(lv_timer_t *t)
{
    (void)t;
    s_fin_on = !s_fin_on;
    fin_paint(s_fin_on);

    // Re-toca o alarme a cada ~8 ticks (~3,4 s) enquanto a tela de fim está no ar.
    if (++s_fin_ticks % 8 == 0) play_alarm();
}

static void trigger_finish(void)
{
    if (!s_finish) return;
    wake_display();
    show_colon(true);
    sync_clock();
    sync_buttons();

    if (s_tv) lv_tileview_set_tile_by_index(s_tv, 1, 0, LV_ANIM_OFF);
    s_fin_on = true;
    s_fin_ticks = 0;
    fin_paint(true);
    lv_obj_clear_flag(s_finish, LV_OBJ_FLAG_HIDDEN);
    sync_keep_awake();
    play_alarm();

    if (s_fin_timer) lv_timer_delete(s_fin_timer);
    s_fin_timer = lv_timer_create(fin_tick_cb, FIN_TICK_MS, NULL);
}

static void finish_dismiss(void)
{
    if (s_fin_timer) { lv_timer_delete(s_fin_timer); s_fin_timer = NULL; }
    if (s_finish) lv_obj_add_flag(s_finish, LV_OBJ_FLAG_HIDDEN);
    s_cur_secs = (s_mode == MODE_UP) ? 0 : s_set_secs;
    s_run = RUN_IDLE;
    show_colon(true);
    sync_clock();
    sync_buttons();
    sync_keep_awake();
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

static void go_cb(lv_event_t *e)      { (void)e; toggle(); }
static void stop_cb(lv_event_t *e)    { (void)e; if (s_run != RUN_IDLE || s_cur_secs) stop_reset(); }
static void finish_cb(lv_event_t *e)  { (void)e; finish_dismiss(); }

static void mode_pill_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i != MODE_UP && i != MODE_DOWN) return;
    s_mode = i;
    stop_reset();
    sync_mode_pills();
    sync_hint();
    save_prefs();
}

static void preset_pill_cb(lv_event_t *e)
{
    if (s_run != RUN_IDLE) return;
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i >= PRESET_COUNT) return;
    s_set_secs = PRESET_MIN[i] * 60;
    if (s_mode == MODE_DOWN) s_cur_secs = s_set_secs;
    sync_presets();
    sync_rollers();
    sync_clock();
    sync_buttons();
    sync_hint();
    save_prefs();
}

static void roller_cb(lv_event_t *e)
{
    if (s_run != RUN_IDLE) return;
    int secs = (int)lv_roller_get_selected(s_roller_min) * 60 +
               (int)lv_roller_get_selected(s_roller_sec);
    if (secs < SECS_MIN) secs = SECS_MIN;
    s_set_secs = secs;
    if (s_mode == MODE_DOWN) s_cur_secs = s_set_secs;
    sync_presets();
    sync_clock();
    sync_buttons();
    sync_hint();
    save_prefs();
}

// ---------------------------------------------------------------------------
// Construção da tela
// ---------------------------------------------------------------------------

static void build_titlebar(void)
{
    lv_obj_t *chip = lv_obj_create(s_screen);
    lv_obj_set_size(chip, T_CHIP, T_CHIP);
    lv_obj_set_style_bg_color(chip, lv_color_hex(KIT_COLOR_SURFACE), 0);
    lv_obj_set_style_border_width(chip, 0, 0);
    lv_obj_set_style_radius(chip, 18, 0);
    lv_obj_set_style_pad_all(chip, 0, 0);
    lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(chip, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(chip, 12);
    lv_obj_add_event_cb(chip, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(chip, LV_ALIGN_TOP_LEFT, T_PAD, 16);

    lv_obj_t *g = add_label(chip, KIT_ICON_BACK, KIT_COLOR_TEXT, &kit_display_44, 0);
    lv_obj_center(g);

    lv_obj_t *title = add_label(s_screen, "TIMER", KIT_COLOR_TEXT, &kit_mono_26, 3);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, T_PAD + T_CHIP + 12, 30);

    lv_obj_t *dots = plain_box(s_screen);
    lv_obj_set_size(dots, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(dots, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dots, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(dots, 6, 0);
    lv_obj_align(dots, LV_ALIGN_TOP_RIGHT, -T_PAD, 40);
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
    lv_obj_set_style_pad_left(p, T_PAD, 0);
    lv_obj_set_style_pad_right(p, T_PAD, 0);
    lv_obj_set_style_pad_top(p, 8, 0);
    lv_obj_set_style_pad_bottom(p, 32, 0);
    lv_obj_set_style_pad_row(p, 12, 0);
    lv_obj_set_flex_flow(p, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(p, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(p, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(p, LV_SCROLLBAR_MODE_AUTO);
    return p;
}

static lv_obj_t *make_pill(lv_obj_t *parent, const char *txt, int h,
                           lv_event_cb_t cb, int code, lv_obj_t **out_lbl)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_set_height(c, h);
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
    lv_obj_t *l = add_label(c, txt, KIT_COLOR_TEXT, &kit_mono_16, 1);
    lv_obj_center(l);
    if (out_lbl) *out_lbl = l;
    return c;
}

static lv_obj_t *make_roller(lv_obj_t *parent, const char *opts, int code)
{
    lv_obj_t *r = lv_roller_create(parent);
    lv_roller_set_options(r, opts, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(r, 3);
    lv_obj_set_width(r, 92);
    lv_obj_set_style_bg_color(r, lv_color_hex(KIT_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_color(r, lv_color_hex(s_accent), LV_PART_SELECTED);
    lv_obj_set_style_text_color(r, lv_color_hex(KIT_COLOR_TEXT), 0);
    lv_obj_set_style_text_color(r, lv_color_hex(on_accent()), LV_PART_SELECTED);
    lv_obj_set_style_text_font(r, &kit_mono_26, 0);
    lv_obj_set_style_border_width(r, 0, 0);
    lv_obj_set_style_pad_all(r, 0, 0);
    lv_obj_add_event_cb(r, roller_cb, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)code);
    return r;
}

// Página 0 — AJUSTE
static void build_page_adjust(lv_obj_t *tile)
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
    static const char *MODE_LABELS[] = { "CRON\xC3\x94METRO", "REGRESSIVO" };
    for (int i = 0; i < 2; i++)
        s_mode_pills[i] = make_pill(mode_row, MODE_LABELS[i], 54, mode_pill_cb, i,
                                    &s_mode_pill_lbls[i]);

    // -------- SÓ NO REGRESSIVO --------
    s_down_cnt = plain_box(p);
    lv_obj_set_size(s_down_cnt, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_down_cnt, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_down_cnt, 9, 0);
    lv_obj_set_style_pad_top(s_down_cnt, 4, 0);

    field_label(s_down_cnt, "TEMPOS FIXOS \xC2\xB7 MIN");
    lv_obj_t *preset_row = plain_box(s_down_cnt);
    lv_obj_set_size(preset_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(preset_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(preset_row, 6, 0);
    for (int i = 0; i < PRESET_COUNT; i++) {
        char n[4];
        snprintf(n, sizeof(n), "%d", PRESET_MIN[i]);
        s_preset_pills[i] = make_pill(preset_row, n, 56, preset_pill_cb, i,
                                      &s_preset_pill_lbls[i]);
    }

    field_label(s_down_cnt, "OU DEFINA");
    lv_obj_t *wheels = plain_box(s_down_cnt);
    lv_obj_set_size(wheels, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(wheels, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(wheels, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(wheels, 10, 0);

    lv_obj_t *col_m = plain_box(wheels);
    lv_obj_set_size(col_m, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col_m, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col_m, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col_m, 6, 0);
    s_roller_min = make_roller(col_m, s_min_opts, 0);
    add_label(col_m, "MIN", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);

    lv_obj_t *col_s = plain_box(wheels);
    lv_obj_set_size(col_s, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col_s, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col_s, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col_s, 6, 0);
    s_roller_sec = make_roller(col_s, s_sec_opts, 1);
    add_label(col_s, "SEG", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);

    s_hint_lbl = add_label(s_down_cnt, "", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 1);
    lv_label_set_long_mode(s_hint_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_hint_lbl, lv_pct(100));
    lv_obj_set_style_text_align(s_hint_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(s_hint_lbl, 4, 0);
}

// Página 1 — RELÓGIO
static lv_obj_t *make_footer_btn(lv_obj_t *parent, const char *txt,
                                 lv_event_cb_t cb, bool primary, lv_obj_t **out_lbl)
{
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_set_size(b, T_BTN_W, T_BTN_H);
    lv_obj_set_style_radius(b, T_BTN_H / 2, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(b, 8);
    if (primary) {
        lv_obj_set_style_border_width(b, 0, 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(s_accent), 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_80, LV_STATE_PRESSED);
    } else {
        lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(b, 2, 0);
        lv_obj_set_style_border_color(b, lv_color_hex(KIT_COLOR_TEXT), 0);
    }
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l = add_label(b, txt, primary ? on_accent() : KIT_COLOR_TEXT, &kit_mono_20, 2);
    lv_obj_center(l);
    if (out_lbl) *out_lbl = l;
    return b;
}

static void build_page_clock(lv_obj_t *tile)
{
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_t *box = lv_obj_create(tile);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    s_modetag_lbl = add_label(box, "REGRESSIVO", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 4);
    lv_obj_align(s_modetag_lbl, LV_ALIGN_TOP_MID, 0, 16);

    // Mostrador MM:SS — dígitos em kit_display_120 (só " - + 0-9"); o "dois
    // pontos" são dois quadrados desenhados (a fonte não tem ':').
    lv_obj_t *row = plain_box(box);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 6, 0);
    lv_obj_align(row, LV_ALIGN_CENTER, 0, -24);

    // Largura automática: "%02d" tem sempre 2 dígitos e todo dígito do
    // kit_display_120 avança igual, então a caixa nunca muda de tamanho (sem
    // "fantasma") — e nada de travar largura, que cortava o número.
    s_mm_lbl = add_label(row, "05", KIT_COLOR_TEXT, &kit_display_120, 0);

    s_colon = plain_box(row);
    lv_obj_set_size(s_colon, 14, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_colon, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_colon, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_colon, 18, 0);
    for (int i = 0; i < 2; i++) {
        lv_obj_t *sq = lv_obj_create(s_colon);
        lv_obj_remove_style_all(sq);
        lv_obj_set_size(sq, 12, 12);
        lv_obj_set_style_bg_color(sq, lv_color_hex(KIT_COLOR_TEXT), 0);
        lv_obj_set_style_bg_opa(sq, LV_OPA_COVER, 0);
        lv_obj_clear_flag(sq, LV_OBJ_FLAG_SCROLLABLE);
        s_colon_sq[i] = sq;
    }

    s_ss_lbl = add_label(row, "00", KIT_COLOR_TEXT, &kit_display_120, 0);

    s_stop_btn = make_footer_btn(box, "PARAR", stop_cb, false, NULL);
    lv_obj_align(s_stop_btn, LV_ALIGN_BOTTOM_LEFT, T_PAD, -T_BTN_MARGIN);

    s_go_btn = make_footer_btn(box, "COME\xC3\x87""AR", go_cb, true, &s_go_lbl);
    lv_obj_align(s_go_btn, LV_ALIGN_BOTTOM_RIGHT, -T_PAD, -T_BTN_MARGIN);
}

static void build_tileview(void)
{
    s_tv = lv_tileview_create(s_screen);
    lv_obj_set_size(s_tv, KIT_DISPLAY_WIDTH, T_PAGE_H);
    lv_obj_set_pos(s_tv, 0, T_TITLEBAR);
    lv_obj_set_style_bg_opa(s_tv, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_tv, 0, 0);
    lv_obj_set_scrollbar_mode(s_tv, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(s_tv, tv_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_tiles[0] = lv_tileview_add_tile(s_tv, 0, 0, LV_DIR_HOR);
    s_tiles[1] = lv_tileview_add_tile(s_tv, 1, 0, LV_DIR_HOR);
    build_page_adjust(s_tiles[0]);
    build_page_clock(s_tiles[1]);
}

static void build_finish(void)
{
    s_finish = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_finish);
    lv_obj_set_size(s_finish, KIT_DISPLAY_WIDTH, KIT_DISPLAY_HEIGHT);
    lv_obj_set_pos(s_finish, 0, 0);
    lv_obj_set_style_bg_color(s_finish, lv_color_hex(s_accent), 0);
    lv_obj_set_style_bg_opa(s_finish, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_finish, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_finish, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_finish, finish_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(s_finish, LV_OBJ_FLAG_HIDDEN);

    // Coluna central: disco + rótulo — mesmo desenho da tela "CARREGANDO".
    lv_obj_t *col = plain_box(s_finish);
    lv_obj_set_size(col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, 24, 0);
    lv_obj_center(col);

    s_fin_disc = lv_obj_create(col);
    lv_obj_set_size(s_fin_disc, 132, 132);
    lv_obj_set_style_bg_color(s_fin_disc, lv_color_hex(KIT_COLOR_BG), 0);
    lv_obj_set_style_border_width(s_fin_disc, 0, 0);
    lv_obj_set_style_radius(s_fin_disc, 66, 0);
    lv_obj_set_style_pad_all(s_fin_disc, 0, 0);
    lv_obj_clear_flag(s_fin_disc, LV_OBJ_FLAG_SCROLLABLE);

    // Ícone de timer geométrico (mesmo idioma do card da Home): anel + botão
    // em cima + dois ponteiros. A cor é aplicada por fin_paint().
    s_fin_icon = plain_box(s_fin_disc);
    lv_obj_set_size(s_fin_icon, 64, 64);
    lv_obj_add_flag(s_fin_icon, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_center(s_fin_icon);
    lv_obj_align(shape(s_fin_icon, 54, 54, LV_RADIUS_CIRCLE, 6), LV_ALIGN_CENTER, 0,  4);
    lv_obj_align(shape(s_fin_icon, 18,  9, 2, 0),                LV_ALIGN_CENTER, 0, -25);
    lv_obj_align(shape(s_fin_icon,  5, 17, 2, 0),                LV_ALIGN_CENTER, 0, -2);
    lv_obj_align(shape(s_fin_icon, 12,  5, 2, 0),                LV_ALIGN_CENTER, 6,  6);

    s_fin_word = add_label(col, "TEMPO", KIT_COLOR_BG, &kit_mono_26, 5);

    s_fin_hint = add_label(s_finish, "TOQUE PARA PARAR", KIT_COLOR_BG, &kit_mono_16, 3);
    lv_obj_align(s_fin_hint, LV_ALIGN_BOTTOM_MID, 0, -40);
}

// ---------------------------------------------------------------------------
// Ciclo de vida
// ---------------------------------------------------------------------------

kit_err_t kit_timer_start(uint32_t accent)
{
    if (s_screen) kit_timer_destroy();

    ESP_LOGI(TAG, "Montando Timer Tool...");
    s_accent    = accent ? accent : KIT_COLOR_GREEN;
    s_mode      = MODE_DOWN;
    s_set_secs  = 300;
    s_run       = RUN_IDLE;
    s_dimmed    = false;
    s_colon_vis = true;
    s_fin_on    = false;
    load_prefs();
    s_cur_secs  = (s_mode == MODE_UP) ? 0 : s_set_secs;

    const kit_api_table_t *t = api();
    s_bright_normal = (t && t->display) ? t->display->get_brightness() : 80;

    fill_num_opts(s_min_opts, sizeof(s_min_opts), 100);
    fill_num_opts(s_sec_opts, sizeof(s_sec_opts), 60);

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(KIT_COLOR_BG), 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    build_titlebar();
    build_tileview();
    build_finish();

    lv_tileview_set_tile_by_index(s_tv, 1, 0, LV_ANIM_OFF);   // começa no RELÓGIO
    lv_obj_update_layout(s_screen);

    sync_mode_pills();
    sync_presets();
    sync_rollers();
    sync_hint();
    sync_clock();
    sync_buttons();
    sync_dots();

    s_anim_timer = lv_timer_create(anim_tick_cb, 200, NULL);

    lv_screen_load(s_screen);
    return KIT_OK;
}

void kit_timer_destroy(void)
{
    ESP_LOGI(TAG, "Encerrando Timer Tool.");

    if (s_count_timer) { lv_timer_delete(s_count_timer); s_count_timer = NULL; }
    if (s_anim_timer)  { lv_timer_delete(s_anim_timer);  s_anim_timer  = NULL; }
    if (s_fin_timer)   { lv_timer_delete(s_fin_timer);   s_fin_timer   = NULL; }

    const kit_api_table_t *t = api();
    if (t && t->power)   t->power->keep_awake(false);
    if (t && t->display && s_dimmed) t->display->set_brightness(s_bright_normal);
    s_dimmed = false;
    s_run    = RUN_IDLE;

    if (s_screen) { lv_obj_delete(s_screen); s_screen = NULL; }

    s_tv = NULL;
    s_down_cnt = s_hint_lbl = NULL;
    s_roller_min = s_roller_sec = NULL;
    s_modetag_lbl = s_mm_lbl = s_ss_lbl = s_colon = NULL;
    s_colon_sq[0] = s_colon_sq[1] = NULL;
    s_go_btn = s_go_lbl = s_stop_btn = NULL;
    s_finish = s_fin_disc = s_fin_icon = s_fin_word = s_fin_hint = NULL;
}
