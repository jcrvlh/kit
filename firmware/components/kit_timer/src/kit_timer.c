#include "kit_timer.h"
#include "kit_api.h"
#include "kit_display.h"
#include "kit_fonts.h"
#include "kit_theme.h"
#include "kit_imu.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>

// Timer Tool — linguagem "Brutalist Bauhaus" (ver docs/design/design-language.md).
// Titlebar fixa + tileview de 2 páginas (arrasta na horizontal):
//   0 AJUSTE   — modo (CRONÔMETRO ↑ / REGRESSIVO ↓); no regressivo, tempos
//                fixos + roleta de arraste MM:SS (time_wheel_t) + Modo Ampulheta.
//   1 RELÓGIO  — só o mostrador MM:SS e os botões PARAR / COMEÇAR (a página
//                inicial). O botão COMEÇAR alterna COMEÇAR → PAUSAR → CONTINUAR.
// PWR físico (e chacoalhar) fazem a mesma coisa que o botão COMEÇAR
// (kit_timer_toggle -> Runtime). A saída é feita pela API (system->exit).
//
// Modo Ampulheta (opcional, no AJUSTE): com o KIT de cabeça pra baixo o timer
// corre e a tela gira 180°; qualquer outra posição pausa e guarda o restante.
// A orientação vem do acelerômetro (kit_imu_poll_orientation).
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

// Roleta de arraste do tempo (mesmo gesto da sigla do Placar/Fora): arrasta ↕
// pra girar o número, toca pra +1.
#define WHEEL_DRAG_PX  24    // px de arraste por passo
#define WHEEL_SLOP     12    // até aqui ainda conta como toque, não arraste
#define WHEEL_JUMP     64    // salto de coordenada acima disto = lixo, ignora

// Modo Ampulheta — KIT de cabeça pra baixo corre um timer; qualquer outra
// posição pausa (guarda o restante). Como uma ampulheta.
#define FLIP_GUARD_US 800000LL   // ignora "chacoalhar" logo após uma virada

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
static lv_timer_t *s_orient_timer = NULL;  // 200 ms — lê a orientação (só no Modo Ampulheta)

// Modo Ampulheta
static bool    s_flip_on   = false;      // persistido ("timer_flip")
static int     s_flip_set  = 300;        // tempo cheio (persist "timer_flip_s")
static int     s_flip_rem  = 300;        // restante (runtime)
static bool    s_flip_spent = false;     // zerou; só re-arma ao sair da posição
static bool    s_flip_pos   = false;     // KIT está na posição de correr agora? (ver flip_run_orient)
static bool    s_flip_touched = false;   // já correu ao menos uma vez (mostra valor pausado)
static int64_t s_flip_last_change_us = 0;

// ---------------------------------------------------------------------------
// Objetos LVGL
// ---------------------------------------------------------------------------
static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_tv     = NULL;
static lv_obj_t *s_tiles[PAGES];
static lv_obj_t *s_dots[PAGES];

// Roleta de arraste: um mostrador de número (00..mod-1, com wrap).
typedef struct {
    lv_obj_t *box, *lbl;
    int  val, mod;
    int  accum, gross;
    bool moved;
    void (*on_change)(void);
} time_wheel_t;

// 2 do tempo regressivo + 2x2 do Modo Ampulheta. Instâncias estáticas: sem
// lv_malloc (o pool do LVGL já é apertado nesta board).
static time_wheel_t s_wheels[6];
static int          s_wheel_n = 0;
static bool         s_wheel_locked = false;
static lv_dir_t     s_pg_sdir = LV_DIR_VER;
static lv_dir_t     s_tv_sdir = LV_DIR_HOR;

// Página 0 — Ajuste
static lv_obj_t *s_adjust_page = NULL;   // container rolável do tile 0 (roleta congela o scroll)
static lv_obj_t *s_mode_pills[2];
static lv_obj_t *s_mode_pill_lbls[2];
static lv_obj_t *s_down_cnt   = NULL;
static lv_obj_t *s_preset_pills[PRESET_COUNT];
static lv_obj_t *s_preset_pill_lbls[PRESET_COUNT];
static time_wheel_t *s_wheel_mm = NULL;
static time_wheel_t *s_wheel_ss = NULL;
static lv_obj_t *s_hint_lbl   = NULL;
static lv_obj_t *s_flip_pills[2];
static lv_obj_t *s_flip_pill_lbls[2];
static lv_obj_t *s_flip_cfg   = NULL;    // container da roleta (oculto quando desligado)
static time_wheel_t *s_wheel_flip[2] = { NULL, NULL };   // MM, SS

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
    if (t->storage->get_i32("timer_flip", &v) == KIT_OK)
        s_flip_on = (v != 0);
    if (t->storage->get_i32("timer_flip_s", &v) == KIT_OK && v >= SECS_MIN && v <= SECS_MAX)
        s_flip_set = (int)v;
}

static void save_prefs(void)
{
    const kit_api_table_t *t = api();
    if (!t || !t->storage) return;
    t->storage->set_i32("timer_mode", s_mode);
    t->storage->set_i32("timer_secs", s_set_secs);
    t->storage->set_i32("timer_flip", s_flip_on ? 1 : 0);
    t->storage->set_i32("timer_flip_s", s_flip_set);
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
    // Modo Ampulheta segura a tela acesa: com ela apagada o acelerômetro
    // desliga (kit_imu) e não dá pra detectar a virada.
    t->power->keep_awake(s_run != RUN_IDLE || finishing || s_flip_on);
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

static void wheel_paint(time_wheel_t *w)
{
    if (!w || !w->lbl) return;
    char b[4];
    snprintf(b, sizeof b, "%02d", w->val);
    lv_label_set_text(w->lbl, b);
}

static void sync_wheels(void)
{
    if (s_wheel_mm) { s_wheel_mm->val = s_set_secs / 60; wheel_paint(s_wheel_mm); }
    if (s_wheel_ss) { s_wheel_ss->val = s_set_secs % 60; wheel_paint(s_wheel_ss); }
    if (s_wheel_flip[0]) { s_wheel_flip[0]->val = s_flip_set / 60; wheel_paint(s_wheel_flip[0]); }
    if (s_wheel_flip[1]) { s_wheel_flip[1]->val = s_flip_set % 60; wheel_paint(s_wheel_flip[1]); }
}

static void sync_flip_pills(void)
{
    uint32_t sel_txt = on_accent();
    for (int i = 0; i < 2; i++) {
        bool sel = (i == 0) == s_flip_on;   // pílula 0 = LIGADO
        lv_obj_set_style_bg_color(s_flip_pills[i],
            lv_color_hex(sel ? s_accent : KIT_COLOR_SURFACE), 0);
        lv_obj_set_style_text_color(s_flip_pill_lbls[i],
            lv_color_hex(sel ? sel_txt : KIT_COLOR_TEXT), 0);
    }
    if (s_flip_cfg) {
        if (s_flip_on) lv_obj_clear_flag(s_flip_cfg, LV_OBJ_FLAG_HIDDEN);
        else           lv_obj_add_flag(s_flip_cfg, LV_OBJ_FLAG_HIDDEN);
    }
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

    int t;
    const char *tag;
    if (s_flip_on && s_flip_pos) {
        t = s_flip_rem;                              // correndo — só o número
        tag = "";
    } else if (s_flip_on && s_flip_touched) {
        t = s_flip_rem;                              // pausado — mostra onde parou
        tag = "PAUSADO";
    } else if (s_flip_on) {
        t = s_flip_set;                              // ainda não virou
        tag = "PRONTO";
    } else {
        t = s_cur_secs;
        tag = (s_mode == MODE_UP) ? "CRON\xC3\x94METRO" : "REGRESSIVO";
    }
    if (t < 0) t = 0;
    if (t > SECS_MAX) t = SECS_MAX;
    lv_label_set_text_fmt(s_mm_lbl, "%02d", t / 60);
    lv_label_set_text_fmt(s_ss_lbl, "%02d", t % 60);
    if (s_modetag_lbl) lv_label_set_text(s_modetag_lbl, tag);
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

    // Modo Ampulheta: a orientação é o controle. Some com o COMEÇAR, mostra a
    // dica, e deixa o PARAR só pra zerar o que já rodou.
    if (s_flip_on) {
        lv_obj_add_flag(s_go_btn, LV_OBJ_FLAG_HIDDEN);
        bool stop_on = !s_flip_pos && (s_flip_touched || s_flip_rem != s_flip_set);
        lv_opa_t o = stop_on ? LV_OPA_COVER : LV_OPA_40;
        lv_obj_set_style_border_opa(s_stop_btn, o, 0);
        lv_obj_set_style_text_opa(s_stop_btn, o, 0);
        if (stop_on) { lv_obj_clear_flag(s_stop_btn, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(s_stop_btn, LV_OBJ_FLAG_CLICKABLE); }
        else         lv_obj_add_flag(s_stop_btn, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(s_go_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_stop_btn, LV_OBJ_FLAG_HIDDEN);

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

    // Modo Ampulheta: conta o restante enquanto o KIT está de cabeça pra baixo.
    if (s_flip_on && s_flip_pos) {
        if (s_flip_rem > 0) s_flip_rem--;
        if (s_flip_rem <= 0) {
            s_flip_rem = 0;
            s_flip_spent = true;
            sync_clock();
            stop_counting();
            trigger_finish();
            return;
        }
        if (s_flip_rem <= 5) {
            const kit_api_table_t *at = api();
            if (at && at->audio) at->audio->sfx(KIT_SFX_TIMER_TICK);
        }
        sync_clock();
        return;
    }

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
        // Últimos 5 s: um tique curtíssimo por segundo pra sinalizar que
        // está acabando. O alarme (TIMER_DONE) toca só no zero.
        if (s_cur_secs <= 5) {
            const kit_api_table_t *at = api();
            if (at && at->audio) at->audio->sfx(KIT_SFX_TIMER_TICK);
        }
    }
    sync_clock();
}

// Alterna COMEÇAR → PAUSAR → CONTINUAR (botão primário / PWR / chacoalhar).
static void toggle(void)
{
    if (!s_screen) return;
    if (s_finish && !lv_obj_has_flag(s_finish, LV_OBJ_FLAG_HIDDEN)) return;

    // Modo Ampulheta: só a orientação controla — PWR e chacoalhar não fazem nada
    // (evita o falso disparo do tranco da virada).
    if (s_flip_on) return;

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
    if (s_flip_on) {
        s_flip_rem = s_flip_set;
        s_flip_spent = false;
        s_flip_touched = false;
    } else {
        s_run = RUN_IDLE;
        s_cur_secs = (s_mode == MODE_UP) ? 0 : s_set_secs;
    }
    show_colon(true);
    sync_wheels();
    sync_clock();
    sync_buttons();
    sync_keep_awake();
}

void kit_timer_toggle(void)
{
    toggle();
}

// ---------------------------------------------------------------------------
// Modo Ampulheta — KIT de cabeça pra baixo corre; qualquer outra posição pausa
// ---------------------------------------------------------------------------

// A posição em que o timer corre é sempre a OPOSTA à de leitura — e a de leitura
// depende do Modo canhoto (Ajustes > Tela). Normal: lê de pé, corre de cabeça
// pra baixo. Canhoto (base 180°): lê de cabeça pra baixo, corre de pé. A tela
// gira pra continuar legível enquanto corre.
static kit_orient_t flip_run_orient(void)
{
    return (kit_display_base_rotation() == 180) ? KIT_ORIENT_UPRIGHT
                                                : KIT_ORIENT_INVERTED;
}
static int flip_run_rotation(void)
{
    return (kit_display_base_rotation() == 180) ? 0 : 180;
}

static void orient_tick_cb(lv_timer_t *t)
{
    (void)t;
    if (!s_flip_on || !s_screen) return;
    if (s_finish && !lv_obj_has_flag(s_finish, LV_OBJ_FLAG_HIDDEN)) return;

    bool pos = (kit_imu_poll_orientation() == flip_run_orient());
    if (pos == s_flip_pos) return;

    s_flip_last_change_us = esp_timer_get_time();
    s_flip_pos = pos;
    stop_counting();

    if (!pos) {
        s_flip_spent = false;   // saiu da posição -> re-arma pro próximo giro
    } else {
        s_flip_touched = true;
        wake_display();
        if (!s_flip_spent) {
            if (s_flip_rem <= 0) s_flip_rem = s_flip_set;
            start_counting();
        }
        if (s_tv) lv_tileview_set_tile_by_index(s_tv, 1, 0, LV_ANIM_OFF);
    }

    // Entrar na posição de correr gira a tela — e ela FICA assim mesmo depois de
    // pausar (só volta ao desligar o modo ou sair da Tool). Assim a pessoa lê o
    // valor pausado do mesmo lado de onde estava olhando.
    if (pos && kit_display_rotation() != flip_run_rotation()) {
        kit_display_set_rotation_impl(flip_run_rotation());
        lv_obj_invalidate(s_screen);
    }

    show_colon(true);
    sync_clock();
    sync_buttons();
    sync_keep_awake();
}

static void sync_orient_timer(void)
{
    bool want = s_flip_on && s_screen;
    if (want && !s_orient_timer)
        s_orient_timer = lv_timer_create(orient_tick_cb, 200, NULL);
    else if (!want && s_orient_timer) {
        lv_timer_delete(s_orient_timer);
        s_orient_timer = NULL;
    }
}

// ---------------------------------------------------------------------------
// Animação de 200 ms — pisca o "dois pontos" + gerencia o brilho reduzido
// ---------------------------------------------------------------------------

static void anim_tick_cb(lv_timer_t *t)
{
    (void)t;

    // "dois pontos" pisca só enquanto conta (manual OU posição do Modo Ampulheta)
    bool counting = (s_count_timer != NULL);
    static int blink_acc = 0;
    if (counting) {
        if (++blink_acc >= 3) { blink_acc = 0; show_colon(!s_colon_vis); }
    } else if (!s_colon_vis) {
        blink_acc = 0;
        show_colon(true);
    }

    // brilho reduzido depois de ~15 s sem toque, apenas contando
    uint32_t idle = lv_display_get_inactive_time(NULL);
    bool want_dim = counting && (idle >= DIM_AFTER_MS);
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
    // No Modo Ampulheta o timer que zerou fica "gasto" (rem = 0) — só re-conta
    // depois de sair da posição e voltar; não mexe no timer manual.
    if (!s_flip_on) {
        s_cur_secs = (s_mode == MODE_UP) ? 0 : s_set_secs;
        s_run = RUN_IDLE;
    }
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
    sync_wheels();
    sync_clock();
    sync_buttons();
    sync_hint();
    save_prefs();
}

// on_change da roleta do tempo regressivo.
static void cd_wheel_changed(void)
{
    if (s_run != RUN_IDLE || !s_wheel_mm || !s_wheel_ss) return;
    int secs = s_wheel_mm->val * 60 + s_wheel_ss->val;
    if (secs < SECS_MIN) secs = SECS_MIN;
    if (secs > SECS_MAX) secs = SECS_MAX;
    s_set_secs = secs;
    if (s_mode == MODE_DOWN) s_cur_secs = s_set_secs;
    sync_presets();
    sync_clock();
    sync_buttons();
    sync_hint();
    save_prefs();
}

// on_change das roletas do Modo Ampulheta (as duas posições).
static void flip_wheel_changed(void)
{
    if (!s_wheel_flip[0] || !s_wheel_flip[1]) return;
    int secs = s_wheel_flip[0]->val * 60 + s_wheel_flip[1]->val;
    if (secs < SECS_MIN) secs = SECS_MIN;
    if (secs > SECS_MAX) secs = SECS_MAX;
    s_flip_set = secs;
    if (!s_flip_pos) s_flip_rem = s_flip_set;   // não mexe no que está correndo
    save_prefs();
}

// -- Roleta de arraste (mesmo gesto da sigla do Placar) --

static void wheel_lock_scroll(bool lock)
{
    if (lock == s_wheel_locked) return;
    s_wheel_locked = lock;
    if (lock) {
        if (s_adjust_page) { s_pg_sdir = lv_obj_get_scroll_dir(s_adjust_page);
                             lv_obj_set_scroll_dir(s_adjust_page, LV_DIR_NONE); }
        if (s_tv) { s_tv_sdir = lv_obj_get_scroll_dir(s_tv);
                    lv_obj_set_scroll_dir(s_tv, LV_DIR_NONE); }
    } else {
        if (s_adjust_page) lv_obj_set_scroll_dir(s_adjust_page, s_pg_sdir);
        if (s_tv)          lv_obj_set_scroll_dir(s_tv, s_tv_sdir);
    }
}

static void wheel_press_cb(lv_event_t *e)
{
    time_wheel_t *w = lv_event_get_user_data(e);
    if (!w) return;
    w->accum = 0; w->gross = 0; w->moved = false;
    wheel_lock_scroll(true);
}

static void wheel_pressing_cb(lv_event_t *e)
{
    time_wheel_t *w = lv_event_get_user_data(e);
    if (!w) return;
    lv_point_t v = { 0, 0 };
    lv_indev_get_vect(lv_indev_active(), &v);
    int dy = (int)v.y;
    if (dy > WHEEL_JUMP || dy < -WHEEL_JUMP) return;   // salto de coordenada = lixo

    w->gross += dy < 0 ? -dy : dy;
    if (w->gross >= WHEEL_SLOP) w->moved = true;

    w->accum += dy;
    bool changed = false;
    // arrastar pra CIMA (y diminui) aumenta o número
    while (w->accum <= -WHEEL_DRAG_PX) { w->val = (w->val + 1) % w->mod; w->accum += WHEEL_DRAG_PX; changed = true; }
    while (w->accum >=  WHEEL_DRAG_PX) { w->val = (w->val - 1 + w->mod) % w->mod; w->accum -= WHEEL_DRAG_PX; changed = true; }
    if (changed) { wheel_paint(w); if (w->on_change) w->on_change(); }
}

static void wheel_release_cb(lv_event_t *e)
{
    (void)e;
    wheel_lock_scroll(false);
}

static void wheel_tap_cb(lv_event_t *e)
{
    time_wheel_t *w = lv_event_get_user_data(e);
    if (!w || w->moved) return;   // foi arraste, não toque
    w->val = (w->val + 1) % w->mod;
    wheel_paint(w);
    if (w->on_change) w->on_change();
}

static void flip_pill_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    bool on = (i == 0);   // pílula 0 = LIGADO
    if (on == s_flip_on) return;
    s_flip_on = on;
    if (!on) {   // desligou: volta pro timer manual
        s_flip_pos = false;
        s_flip_touched = false;
        stop_counting();
        s_run = RUN_IDLE;
        s_cur_secs = (s_mode == MODE_UP) ? 0 : s_set_secs;
        if (kit_display_rotation() != kit_display_base_rotation()) {
            kit_display_restore_rotation_impl();
            lv_obj_invalidate(s_screen);
        }
    } else {
        s_flip_rem = s_flip_set;
        s_flip_spent = false;
        s_flip_touched = false;
    }
    sync_flip_pills();
    sync_orient_timer();
    sync_keep_awake();
    show_colon(true);
    sync_clock();
    sync_buttons();
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

static time_wheel_t *make_wheel(lv_obj_t *parent, int mod, void (*on_change)(void))
{
    if (s_wheel_n >= (int)(sizeof(s_wheels) / sizeof(s_wheels[0]))) return NULL;
    time_wheel_t *w = &s_wheels[s_wheel_n++];
    w->val = 0; w->mod = mod; w->accum = 0; w->gross = 0; w->moved = false;
    w->on_change = on_change;

    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_set_size(box, 96, 104);
    lv_obj_set_style_bg_color(box, lv_color_hex(KIT_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_radius(box, 16, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(box, 6);
    lv_obj_add_event_cb(box, wheel_tap_cb,      LV_EVENT_CLICKED,    w);
    lv_obj_add_event_cb(box, wheel_press_cb,    LV_EVENT_PRESSED,    w);
    lv_obj_add_event_cb(box, wheel_pressing_cb, LV_EVENT_PRESSING,   w);
    lv_obj_add_event_cb(box, wheel_release_cb,  LV_EVENT_RELEASED,   w);
    lv_obj_add_event_cb(box, wheel_release_cb,  LV_EVENT_PRESS_LOST, w);

    w->box = box;
    w->lbl = add_label(box, "00", KIT_COLOR_TEXT, &kit_display_72, 0);
    lv_obj_center(w->lbl);
    return w;
}

// Coluna [roleta] + [rótulo], pra as 3 colunas do par MM:SS ficarem alinhadas.
static lv_obj_t *wheel_col(lv_obj_t *parent)
{
    lv_obj_t *c = plain_box(parent);
    lv_obj_set_size(c, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(c, 5, 0);
    return c;
}

// Par MM : SS de roletas de arraste, com o ":" desenhado (dois quadrados).
static void make_wheel_pair(lv_obj_t *parent, time_wheel_t **mm, time_wheel_t **ss,
                            void (*on_change)(void))
{
    lv_obj_t *row = plain_box(parent);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 8, 0);

    lv_obj_t *cm = wheel_col(row);
    *mm = make_wheel(cm, 100, on_change);
    add_label(cm, "MIN", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);

    lv_obj_t *cc = wheel_col(row);
    lv_obj_t *colon = plain_box(cc);
    lv_obj_set_size(colon, 12, 104);
    lv_obj_set_flex_flow(colon, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(colon, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(colon, 16, 0);
    for (int i = 0; i < 2; i++) {
        lv_obj_t *sq = shape(colon, 10, 10, 2, 0);
        lv_obj_set_style_bg_color(sq, lv_color_hex(KIT_COLOR_TEXT), 0);
    }
    add_label(cc, "", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 0);   // espaçador (altura do rótulo)

    lv_obj_t *cs = wheel_col(row);
    *ss = make_wheel(cs, 60, on_change);
    add_label(cs, "SEG", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);
}

// Página 0 — AJUSTE
static void build_page_adjust(lv_obj_t *tile)
{
    lv_obj_t *p = page_scroll(tile);
    s_adjust_page = p;   // a roleta de arraste congela o scroll deste container

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

    field_label(s_down_cnt, "OU DEFINA \xC2\xB7 ARRASTA / TOCA");
    make_wheel_pair(s_down_cnt, &s_wheel_mm, &s_wheel_ss, cd_wheel_changed);

    s_hint_lbl = add_label(s_down_cnt, "", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 1);
    lv_label_set_long_mode(s_hint_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_hint_lbl, lv_pct(100));
    lv_obj_set_style_text_align(s_hint_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(s_hint_lbl, 4, 0);

    // -------- MODO AMPULHETA --------
    lv_obj_t *sec_flip = plain_box(p);
    lv_obj_set_size(sec_flip, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(sec_flip, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(sec_flip, 9, 0);
    lv_obj_set_style_pad_top(sec_flip, 8, 0);
    field_label(sec_flip, "MODO AMPULHETA");

    lv_obj_t *flip_row = plain_box(sec_flip);
    lv_obj_set_size(flip_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(flip_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(flip_row, 8, 0);
    static const char *FLIP_LABELS[] = { "LIGADO", "DESLIGADO" };
    for (int i = 0; i < 2; i++)
        s_flip_pills[i] = make_pill(flip_row, FLIP_LABELS[i], 54, flip_pill_cb, i,
                                    &s_flip_pill_lbls[i]);

    s_flip_cfg = plain_box(p);
    lv_obj_set_size(s_flip_cfg, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_flip_cfg, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_flip_cfg, 9, 0);

    // No Modo canhoto a posição de correr é "de pé" (a base já é 180°).
    const char *fx_txt = (kit_display_base_rotation() == 180)
        ? "Ponha o KIT de p\xC3\xA9 pra correr o timer; a tela gira junto. "
          "Qualquer outra posi\xC3\xA7\xC3\xA3o pausa e guarda onde parou. A "
          "tela fica acesa neste modo."
        : "Vire o KIT de cabe\xC3\xA7""a pra baixo pra correr o timer; a tela gira "
          "junto. Qualquer outra posi\xC3\xA7\xC3\xA3o pausa e guarda onde parou. A "
          "tela fica acesa neste modo.";
    lv_obj_t *fx = add_label(s_flip_cfg, fx_txt,
        KIT_COLOR_TEXT_MUTED, &kit_mono_16, 1);
    lv_label_set_long_mode(fx, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(fx, lv_pct(100));

    field_label(s_flip_cfg, "TEMPO");
    make_wheel_pair(s_flip_cfg, &s_wheel_flip[0], &s_wheel_flip[1], flip_wheel_changed);
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
    s_flip_on   = false;
    s_flip_set  = 300;
    s_flip_pos  = false;
    s_flip_spent = false;
    s_flip_touched = false;
    s_flip_last_change_us = 0;
    s_wheel_n = 0;
    s_wheel_locked = false;
    load_prefs();
    s_cur_secs  = (s_mode == MODE_UP) ? 0 : s_set_secs;
    s_flip_rem  = s_flip_set;
    kit_display_restore_rotation_impl();

    const kit_api_table_t *t = api();
    s_bright_normal = (t && t->display) ? t->display->get_brightness() : 80;

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
    sync_wheels();
    sync_flip_pills();
    sync_hint();
    sync_clock();
    sync_buttons();
    sync_dots();
    sync_orient_timer();
    sync_keep_awake();

    s_anim_timer = lv_timer_create(anim_tick_cb, 200, NULL);

    lv_screen_load(s_screen);
    return KIT_OK;
}

void kit_timer_destroy(void)
{
    ESP_LOGI(TAG, "Encerrando Timer Tool.");

    if (s_count_timer)  { lv_timer_delete(s_count_timer);  s_count_timer  = NULL; }
    if (s_anim_timer)   { lv_timer_delete(s_anim_timer);   s_anim_timer   = NULL; }
    if (s_fin_timer)    { lv_timer_delete(s_fin_timer);    s_fin_timer    = NULL; }
    if (s_orient_timer) { lv_timer_delete(s_orient_timer); s_orient_timer = NULL; }

    const kit_api_table_t *t = api();
    if (t && t->power)   t->power->keep_awake(false);
    if (t && t->display && s_dimmed) t->display->set_brightness(s_bright_normal);
    kit_display_restore_rotation_impl();
    s_dimmed = false;
    s_run    = RUN_IDLE;
    s_flip_pos = false;

    if (s_screen) { lv_obj_delete(s_screen); s_screen = NULL; }

    s_tv = s_adjust_page = NULL;
    s_down_cnt = s_hint_lbl = NULL;
    s_wheel_mm = s_wheel_ss = NULL;
    s_wheel_flip[0] = s_wheel_flip[1] = NULL;
    s_flip_cfg = NULL;
    s_modetag_lbl = s_mm_lbl = s_ss_lbl = s_colon = NULL;
    s_colon_sq[0] = s_colon_sq[1] = NULL;
    s_go_btn = s_go_lbl = s_stop_btn = NULL;
    s_finish = s_fin_disc = s_fin_icon = s_fin_word = s_fin_hint = NULL;
}
