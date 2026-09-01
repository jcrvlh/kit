#include "kit_primeiro.h"
#include "kit_api.h"
#include "kit_display.h"
#include "kit_fonts.h"
#include "kit_theme.h"
#include "esp_log.h"
#include "lvgl.h"
#include <stdio.h>
#include <stdlib.h>

// Quem Vai Primeiro — linguagem "Brutalist Bauhaus" (ver
// docs/design/design-language.md). Página única (sem tileview, sem ajuste,
// sem histórico), no mesmo espírito minimalista da Bottle Tool:
//   - titlebar fixa (chip de voltar + "PRIMEIRO");
//   - a característica sorteada no centro, em kit_mono_26 CAIXA ALTA,
//     quebrando em até quatro linhas — sem "wrap box" em volta;
//   - "A PESSOA QUE" acima e "COMEÇA O JOGO" abaixo, em mono apagado;
//   - botão SORTEAR fixo no rodapé (cor da Tool).
// Sortear dispara pelo botão, por toque em qualquer lugar do palco e pelo
// botão físico PWR / chacoalhar (kit_primeiro_draw -> Runtime). A saída é
// feita pela API (system->exit) / botão BOOT.
//
// Animação = o padrão validado da Dice/Coin Tool: um único lv_timer curto
// que só faz lv_label_set_text por tick, embaralhando entre características
// e travando na sorteada (escolhida ANTES da animação) no último tick.
// Nada de transform_scale/transform_rotation — o layer transformado
// animado estoura o render no CO5300/PSRAM e reinicia a board.

static const char *TAG = "KIT_PRIMEIRO";

#define P_PAD        16
#define P_CONTENT    (KIT_DISPLAY_WIDTH - 2 * P_PAD)            // 336
#define P_TITLEBAR   88
#define P_FOOT       104
#define P_CHIP       56
#define P_GO_H       76
#define P_GO_MARGIN  18
#define P_PAGE_H     (KIT_DISPLAY_HEIGHT - P_TITLEBAR - P_FOOT) // 256

#define P_DRAW_TICKS   13
#define P_DRAW_TICK_MS 55

// Lista fixa de características. Todas em CAIXA ALTA (regra da tipografia
// mono). Pares opostos entram de propósito, pra dar mais variação.
static const char *const TRAITS[] = {
    "É MAIS ALTA",
    "É MAIS BAIXA",
    "É MAIS NOVA",
    "É MAIS VELHA",
    "ACORDOU MAIS CEDO HOJE",
    "DORMIU MAIS TARDE ONTEM",
    "COMEU FEIJÃO POR ÚLTIMO",
    "FAZ ANIVERSÁRIO PRIMEIRO",
    "TEM O ANIVERSÁRIO MAIS LONGE",
    "CHEGOU POR ÚLTIMO",
    "CHEGOU PRIMEIRO",
    "TEM O CABELO MAIS COMPRIDO",
    "TEM O CABELO MAIS CURTO",
    "MORA MAIS LONGE DAQUI",
    "MORA MAIS PERTO DAQUI",
    "TOMOU BANHO MAIS CEDO",
    "ESTÁ COM MENOS BATERIA NO CELULAR",
    "ESTÁ COM MAIS BATERIA NO CELULAR",
    "VIAJOU MAIS RECENTEMENTE",
    "TEM MAIS IRMÃOS",
    "TEM MAIS PRIMOS",
    "CALÇA O PÉ MAIOR",
    "CALÇA O PÉ MENOR",
    "ESTÁ COM A ROUPA MAIS COLORIDA",
    "ESTÁ COM A ROUPA MAIS ESCURA",
    "FALOU MENOS ATÉ AGORA",
    "FALOU MAIS ATÉ AGORA",
    "BEBEU ÁGUA POR ÚLTIMO",
    "TEM O NOME MAIS CURTO",
    "TEM O NOME MAIS COMPRIDO",
    "RIU POR ÚLTIMO",
    "TEM MAIS NOTIFICAÇÕES NO CELULAR",
    "USOU O BANHEIRO POR ÚLTIMO",
    "COMEU DOCE MAIS RECENTEMENTE",
    "ESTÁ MAIS PERTO DA PORTA",
    "TEM MAIS MOEDAS NO BOLSO",
    "CORTOU O CABELO MAIS RECENTEMENTE",
    "NASCEU MAIS LONGE DAQUI",
    "DORMIU MENOS ESSA NOITE",
    "MANDOU A ÚLTIMA MENSAGEM NO GRUPO",
    "TEM A UNHA MAIS COMPRIDA",
    "PISCOU POR ÚLTIMO",
    "ESTÁ COM O PÉ MAIS GELADO",
    "BEBEU CAFÉ HOJE",
    "DIRIGIU POR ÚLTIMO",
    "MORA NO ANDAR MAIS ALTO",
    "TEM MAIS CHAVES NO CHAVEIRO",
    "VIU O MAR MAIS RECENTEMENTE",
    "FEZ EXERCÍCIO HOJE",
    "TEM MAIS ABAS ABERTAS NO CELULAR",
    "ESTÁ COM A UNHA PINTADA",
    "ACORDOU SEM ALARME HOJE",
    "COMEU PIZZA POR ÚLTIMO",
    "ESTÁ SENTADA MAIS PERTO DA JANELA",
    "TROCOU DE CELULAR MAIS RECENTEMENTE",
    "CANTOU ALGUMA MÚSICA HOJE",
    "ESTÁ COM MAIS FOME AGORA",
};
#define TRAITS_N ((int)(sizeof(TRAITS) / sizeof(TRAITS[0])))

// --- estado --------------------------------------------------------------
static uint32_t s_accent  = KIT_COLOR_YELLOW;
static bool     s_drawing = false;
static bool     s_drawn   = false;    // já houve ao menos um sorteio
static int      s_last    = -1;       // índice sorteado anterior
static int      s_target  = -1;       // índice sorteado da rodada atual
static int      s_tick    = 0;
static lv_timer_t *s_timer = NULL;

// --- objetos LVGL -------------------------------------------------------
static lv_obj_t *s_screen   = NULL;
static lv_obj_t *s_lead_in  = NULL;
static lv_obj_t *s_phrase   = NULL;
static lv_obj_t *s_lead_out = NULL;
static lv_obj_t *s_go_btn   = NULL;

// --- helpers ----------------------------------------------------------

static const kit_api_table_t *api(void) { return kit_api_get_table(); }

// Texto sobre a cor da Tool: preto no amarelo (contraste alto), paper no resto.
static uint32_t on_accent(void)
{
    return (s_accent == KIT_COLOR_YELLOW) ? KIT_COLOR_ON_YELLOW : KIT_COLOR_ON_COLOR;
}

static int rnd_index(void)
{
    const kit_api_table_t *t = api();
    int i;
    do {
        if (t && t->random) i = (int)t->random->range(0, TRAITS_N - 1);
        else                i = rand() % TRAITS_N;
    } while (i == s_last && TRAITS_N > 1);
    return i;
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

// --- sorteio -----------------------------------------------------------

static void draw_tick_cb(lv_timer_t *t);

static void do_draw(void)
{
    if (s_drawing || !s_phrase) return;
    s_drawing = true;
    s_target = rnd_index();

    if (!s_drawn) {
        s_drawn = true;
        lv_obj_clear_flag(s_lead_in, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_lead_out, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_style_text_color(s_phrase, lv_color_hex(KIT_COLOR_TEXT_MUTED), 0);
    lv_obj_set_style_opa(s_go_btn, LV_OPA_60, 0);   // "ocupado"

    s_tick = 0;
    s_timer = lv_timer_create(draw_tick_cb, P_DRAW_TICK_MS, NULL);
}

static void draw_tick_cb(lv_timer_t *t)
{
    (void)t;
    s_tick++;

    if (s_tick < P_DRAW_TICKS) {
        int i;
        if (api() && api()->random) i = (int)api()->random->range(0, TRAITS_N - 1);
        else                        i = rand() % TRAITS_N;
        lv_label_set_text(s_phrase, TRAITS[i]);
        // Tique curtinho e baixo subindo de tom — "dando corda" até a revelação.
        const kit_api_table_t *a = api();
        if (a && a->audio) a->audio->beep((uint16_t)(1200 + s_tick * 80), 9);
        return;
    }

    // trava na característica sorteada (escolhida antes da animação)
    s_last = s_target;
    lv_label_set_text(s_phrase, TRAITS[s_target]);
    lv_obj_set_style_text_color(s_phrase, lv_color_hex(s_accent), 0);

    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    s_drawing = false;
    lv_obj_set_style_opa(s_go_btn, LV_OPA_COVER, 0);

    const kit_api_table_t *a = api();
    if (a && a->audio) a->audio->sfx(KIT_SFX_REVEAL);   // duas notas subindo, no fim
}

void kit_primeiro_draw(void)
{
    do_draw();
}

// --- callbacks ------------------------------------------------------

static void back_cb(lv_event_t *e)
{
    (void)e;
    const kit_api_table_t *t = api();
    if (t && t->system) t->system->exit();
}

static void draw_cb(lv_event_t *e)
{
    (void)e;
    do_draw();
}

// --- construção da tela -------------------------------------------

static void build_titlebar(void)
{
    lv_obj_t *chip = lv_obj_create(s_screen);
    lv_obj_set_size(chip, P_CHIP, P_CHIP);
    lv_obj_set_style_bg_color(chip, lv_color_hex(KIT_COLOR_SURFACE), 0);
    lv_obj_set_style_border_width(chip, 0, 0);
    lv_obj_set_style_radius(chip, 18, 0);
    lv_obj_set_style_pad_all(chip, 0, 0);
    lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(chip, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(chip, 12);
    lv_obj_add_event_cb(chip, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(chip, LV_ALIGN_TOP_LEFT, P_PAD, 16);

    lv_obj_t *g = add_label(chip, KIT_ICON_BACK, KIT_COLOR_TEXT, &kit_display_44, 0);
    lv_obj_center(g);

    lv_obj_t *title = add_label(s_screen, "PRIMEIRO", KIT_COLOR_TEXT, &kit_mono_26, 3);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, P_PAD + P_CHIP + 12, 30);
}

// Palco: área tocável entre a titlebar e o botão, com a coluna
// "A PESSOA QUE" / frase / "COMEÇA O JOGO" centralizada.
static void build_stage(void)
{
    lv_obj_t *stage = lv_obj_create(s_screen);
    lv_obj_remove_style_all(stage);
    lv_obj_set_size(stage, KIT_DISPLAY_WIDTH, P_PAGE_H);
    lv_obj_set_pos(stage, 0, P_TITLEBAR);
    lv_obj_clear_flag(stage, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(stage, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(stage, draw_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *col = plain_box(stage);
    lv_obj_set_size(col, KIT_DISPLAY_WIDTH, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, 12, 0);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_CLICKABLE);   // toque cai no palco
    lv_obj_add_flag(col, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_center(col);

    s_lead_in = add_label(col, "A PESSOA QUE", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 3);
    lv_obj_add_flag(s_lead_in, LV_OBJ_FLAG_HIDDEN);

    // Frase protagonista: mono_26 caixa alta, quebra em várias linhas. O
    // pad vertical dá folga para acentos/cedilha da primeira e da última
    // linha não serem cortados (mesmo motivo do pad_bottom em
    // build_timeout_list no launcher).
    s_phrase = add_label(col, "TOQUE EM SORTEAR", KIT_COLOR_TEXT_MUTED, &kit_mono_26, 2);
    lv_label_set_long_mode(s_phrase, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_phrase, P_CONTENT);
    lv_obj_set_height(s_phrase, LV_SIZE_CONTENT);
    lv_obj_set_style_text_align(s_phrase, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(s_phrase, 6, 0);
    lv_obj_set_style_pad_bottom(s_phrase, 10, 0);

    s_lead_out = add_label(col, "COMEÇA O JOGO", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 3);
    lv_obj_add_flag(s_lead_out, LV_OBJ_FLAG_HIDDEN);
}

static void build_footer(void)
{
    // lv_obj puro (sem tema) pelo mesmo motivo da Dice/Bottle Tool: o
    // lv_button aplica transform no estado PRESSED e desalinha o rótulo.
    s_go_btn = lv_obj_create(s_screen);
    lv_obj_set_size(s_go_btn, P_CONTENT, P_GO_H);
    lv_obj_set_style_radius(s_go_btn, P_GO_H / 2, 0);
    lv_obj_set_style_border_width(s_go_btn, 0, 0);
    lv_obj_set_style_shadow_width(s_go_btn, 0, 0);
    lv_obj_set_style_pad_all(s_go_btn, 0, 0);
    lv_obj_set_style_bg_color(s_go_btn, lv_color_hex(s_accent), 0);
    lv_obj_set_style_bg_opa(s_go_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(s_go_btn, LV_OPA_80, LV_STATE_PRESSED);
    lv_obj_clear_flag(s_go_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_go_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_go_btn, 8);
    lv_obj_align(s_go_btn, LV_ALIGN_BOTTOM_MID, 0, -P_GO_MARGIN);
    lv_obj_add_event_cb(s_go_btn, draw_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *l = add_label(s_go_btn, "SORTEAR", on_accent(), &kit_mono_26, 3);
    lv_obj_center(l);
}

// --- ciclo de vida ------------------------------------------------

kit_err_t kit_primeiro_start(uint32_t accent)
{
    if (s_screen) kit_primeiro_destroy();

    ESP_LOGI(TAG, "Montando Quem Vai Primeiro...");
    s_accent  = accent ? accent : KIT_COLOR_YELLOW;
    s_drawing = false;
    s_drawn   = false;
    s_last    = -1;
    s_target  = -1;
    s_tick    = 0;

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(KIT_COLOR_BG), 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    build_titlebar();
    build_stage();
    build_footer();

    lv_screen_load(s_screen);
    return KIT_OK;
}

void kit_primeiro_destroy(void)
{
    ESP_LOGI(TAG, "Encerrando Quem Vai Primeiro.");
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    s_drawing = false;

    if (s_screen) {
        lv_obj_delete(s_screen);
        s_screen = NULL;
    }
    s_lead_in = s_phrase = s_lead_out = s_go_btn = NULL;
}
