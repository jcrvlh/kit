#include "kit_quebragelo.h"
#include "kit_api.h"
#include "kit_display.h"
#include "kit_fonts.h"
#include "kit_theme.h"
#include "esp_log.h"
#include "lvgl.h"
#include <stdio.h>
#include <stdlib.h>

// Quebra-Gelo — linguagem "Brutalist Bauhaus" (ver docs/design/design-language.md).
// Mesma estrutura da Quem Vai Primeiro (kit_primeiro): página única, sem
// tileview / ajuste / histórico / persistência.
//   - titlebar fixa (chip de voltar + "QUEBRA-GELO");
//   - a pergunta sorteada no centro, em kit_mono_26 CAIXA ALTA, quebrando em
//     várias linhas — sem "wrap box";
//   - "PERGUNTA" acima e "PASSE ADIANTE" abaixo, em mono apagado;
//   - botão SORTEAR fixo no rodapé (cor da Tool).
// Sortear dispara pelo botão, por toque no palco e pelo PWR físico / chacoalhar
// (kit_quebragelo_draw -> Runtime). A saída é feita pela API (system->exit).
//
// Animação = o padrão validado da Dice/Coin/Primeiro: um único lv_timer curto
// que só faz lv_label_set_text por tick, embaralhando entre as perguntas e
// travando na sorteada (escolhida ANTES da animação) no último tick. Nada de
// transform_scale/transform_rotation — o layer transformado animado estoura o
// render no CO5300/PSRAM e reinicia a board.

static const char *TAG = "KIT_QUEBRAGELO";

#define Q_PAD        16
#define Q_CONTENT    (KIT_DISPLAY_WIDTH - 2 * Q_PAD)            // 336
#define Q_TITLEBAR   88
#define Q_FOOT       104
#define Q_CHIP       56
#define Q_GO_H       76
#define Q_GO_MARGIN  18
#define Q_PAGE_H     (KIT_DISPLAY_HEIGHT - Q_TITLEBAR - Q_FOOT) // 256

#define Q_DRAW_TICKS   13
#define Q_DRAW_TICK_MS 55

// Baralho fixo de perguntas quebra-gelo — leves e criativas, todas em CAIXA
// ALTA (regra da tipografia mono). Curtas o bastante para caber em poucas
// linhas de kit_mono_26 num palco de 336 px.
static const char *const QUESTIONS[] = {
    "SE VOCÊ FOSSE UM ELETRODOMÉSTICO, QUAL SERIA?",
    "QUAL SUPERPODER INÚTIL VOCÊ GOSTARIA DE TER?",
    "QUAL MÚSICA GRUDA NA SUA CABEÇA COM MAIS FACILIDADE?",
    "QUAL COMIDA VOCÊ COMERIA TODO DIA SEM ENJOAR?",
    "SE OS ANIMAIS FALASSEM, QUAL SERIA O MAIS CHATO?",
    "QUAL NOME VOCÊ DARIA PARA UM BARCO?",
    "QUAL TALENTO ESTRANHO VOCÊ TEM?",
    "QUAL FILME VOCÊ JÁ ASSISTIU MAIS VEZES?",
    "SE VOCÊ PUDESSE JANTAR COM UM FAMOSO, QUEM SERIA?",
    "QUAL O MELHOR CHEIRO DO MUNDO?",
    "QUAL APLICATIVO VOCÊ MAIS USA SEM PERCEBER?",
    "QUAL FOI A ÚLTIMA COISA QUE TE FEZ RIR ALTO?",
    "QUAL LUGAR VOCÊ SONHA EM VISITAR?",
    "SE VOCÊ ABRISSE UM RESTAURANTE, QUAL SERIA O PRATO?",
    "QUAL DESENHO ANIMADO MARCOU SUA INFÂNCIA?",
    "QUAL A PIOR MODA QUE VOCÊ JÁ SEGUIU?",
    "QUAL OBJETO INÚTIL VOCÊ NUNCA CONSEGUE JOGAR FORA?",
    "SE VOCÊ VIRASSE UMA ESTAÇÃO DO ANO, QUAL SERIA?",
    "QUAL PALAVRA VOCÊ ACHA ENGRAÇADA DE FALAR?",
    "QUAL FOI SUA MAIOR GAFE RECENTE?",
    "SE PUDESSE APAGAR UMA MÚSICA DA HISTÓRIA, QUAL SERIA?",
    "QUAL HABILIDADE VOCÊ QUERIA APRENDER DO NADA?",
    "QUAL O SEU PEDIDO SECRETO NO CARDÁPIO?",
    "QUAL PERSONAGEM DE FICÇÃO VOCÊ LEVARIA PRA MORAR COM VOCÊ?",
    "QUAL O MELHOR PRESENTE QUE VOCÊ JÁ GANHOU?",
    "SE SEU DIA TIVESSE TRILHA SONORA, O QUE TOCARIA AGORA?",
    "QUAL COMIDA ESTRANGEIRA VOCÊ QUER PROVAR?",
    "QUAL FOI A MELHOR SESTA DA SUA VIDA?",
    "QUAL APELIDO VOCÊ TEVE NA ESCOLA?",
    "SE VOCÊ FOSSE UM SORVETE, QUAL SABOR SERIA?",
    "QUAL TAREFA DOMÉSTICA VOCÊ ODEIA DE VERDADE?",
    "QUAL A COISA MAIS CARA QUE VOCÊ JÁ QUEBROU?",
    "QUAL SÉRIE VOCÊ ABANDONOU E NÃO SE ARREPENDE?",
    "QUAL O MELHOR CONSELHO INÚTIL QUE VOCÊ JÁ RECEBEU?",
    "SE VOCÊ FALASSE UMA LÍNGUA NOVA AGORA, QUAL SERIA?",
    "QUAL COMIDA DE FESTA VOCÊ ATACA PRIMEIRO?",
    "QUAL FOI O SEU PRIMEIRO SHOW OU EVENTO?",
    "QUAL LOJA VOCÊ ENTRA SÓ PRA OLHAR E SAI COMPRANDO?",
    "SE GANHASSE UM ANIMAL EXÓTICO, QUAL VOCÊ TOPARIA?",
    "QUAL O SOM QUE MAIS TE IRRITA?",
    "QUAL A MELHOR VIAGEM MAL PLANEJADA QUE VOCÊ FEZ?",
    "QUAL COISA DE ADULTO VOCÊ AINDA NÃO SABE FAZER?",
    "QUAL EMOJI TE REPRESENTA HOJE?",
    "SE VOCÊ TIVESSE UM PROGRAMA DE TV, QUAL SERIA O TEMA?",
    "QUAL MÚSICA VOCÊ CANTA ALTO SÓ QUANDO ESTÁ SOZINHA?",
    "QUAL O MELHOR LANCHE DA MADRUGADA DA SUA VIDA?",
    "QUAL LUGAR DA SUA CIDADE VOCÊ MOSTRARIA A UM TURISTA?",
    "QUAL BRINQUEDO VOCÊ QUERIA E NUNCA GANHOU?",
    "SE VOCÊ VIRASSE UMA PLANTA, QUAL SERIA?",
    "QUAL A DECISÃO MAIS ALEATÓRIA QUE DEU CERTO PRA VOCÊ?",
    "QUAL PERSONAGEM MERECIA UM FINAL MELHOR?",
    "QUAL CHEIRO TE LEVA DIRETO PRA INFÂNCIA?",
    "QUAL FOI O SEU MAIOR PERRENGUE DE TECNOLOGIA?",
    "SE PUDESSE TELETRANSPORTAR PRA UM LUGAR AGORA, ONDE?",
    "QUAL COMIDA VOCÊ ACHAVA NOJENTA E HOJE AMA?",
    "QUAL FOI O MELHOR NÃO QUE VOCÊ JÁ RECEBEU?",
    "QUAL MANIA VOCÊ PEGOU DE ALGUÉM DA FAMÍLIA?",
    "QUAL SERIA O SEU NOME ARTÍSTICO?",
    "QUAL A COISA MAIS FOFA QUE UM BICHO JÁ FEZ COM VOCÊ?",
    "SE VOCÊ ESCREVESSE UM LIVRO, QUAL SERIA O TÍTULO?",
    "QUAL PROGRAMA DE DOMINGO É SAGRADO PRA VOCÊ?",
    "QUAL FOI O SEU PLANO MIRABOLANTE DE INFÂNCIA?",
    "QUAL PALAVRA VOCÊ SEMPRE ESCREVE ERRADO?",
    "QUAL A MELHOR FANTASIA QUE VOCÊ JÁ USOU?",
    "SE VOCÊ FOSSE UMA BEBIDA, QUAL SERIA?",
    "QUAL COISA PEQUENA MELHORA QUALQUER DIA RUIM?",
    "QUAL FOI A ÚLTIMA FOTO QUE VOCÊ TIROU?",
    "QUE SOM VOCÊ FARIA SE FOSSE UM DESENHO ANIMADO?",
    "QUAL FOI O SEU HOBBY MAIS CURTO DA VIDA?",
    "SE VOCÊ ORGANIZASSE UMA FESTA, QUAL SERIA O TEMA?",
    "QUAL CANTINHO DA CASA É O SEU FAVORITO?",
    "QUAL O MELHOR SURTO DE COMPRAS QUE VOCÊ JÁ TEVE?",
    "DE QUAL PERSONAGEM VOCÊ FARIA COSPLAY SEM PENSAR?",
    "QUAL COISA DA NATUREZA AINDA TE DEIXA DE QUEIXO CAÍDO?",
    "QUAL A MELHOR DESCULPA QUE VOCÊ JÁ DEU?",
    "SE VOCÊ TIVESSE UMA BANDA, QUAL SERIA O NOME?",
    "QUAL COMIDA VOCÊ DEFENDE COM UNHAS E DENTES?",
    "QUAL ELOGIO VOCÊ NUNCA ESQUECEU?",
    "QUAL APOSTA BOBA VOCÊ JÁ FEZ E GANHOU?",
    "QUAL FOI O MELHOR DIA DE CHUVA QUE VOCÊ JÁ TEVE?",
    "SE VOCÊ FOSSE UM MÓVEL DA CASA, QUAL SERIA?",
    "O QUE VOCÊ COLECIONOU QUANDO ERA CRIANÇA?",
    "QUAL É O SEU RITUAL ANTES DE DORMIR?",
    "QUAL VIAGEM DOS SONHOS VOCÊ AINDA VAI FAZER?",
    "QUAL PALAVRA ESTRANGEIRA VOCÊ ACHA LINDA?",
    "QUAL FOI O MELHOR CONSELHO QUE VOCÊ DEU A ALGUÉM?",
    "SE TIVESSE UM JARDIM DE QUALQUER COISA, DO QUE SERIA?",
    "QUAL A COMIDA MAIS ESQUISITA QUE VOCÊ JÁ GOSTOU?",
    "QUAL MÚSICA TE FAZ VIAJAR NO TEMPO NA HORA?",
    "QUAL TALENTO DE OUTRA PESSOA VOCÊ INVEJA DO BEM?",
    "QUAL FOI A MELHOR BAGUNÇA QUE VALEU A PENA?",
    "QUAL SERIA O SEU PODER NUM VIDEOGAME DA VIDA REAL?",
    "QUAL COISA VOCÊ FAZ DIFERENTE DE TODO MUNDO?",
    "QUAL FOI O MELHOR OI QUE VOCÊ JÁ RECEBEU?",
    "QUAL É A SUA TEORIA MALUCA FAVORITA?",
    "QUAL CENA DE FILME VOCÊ QUERIA TER VIVIDO?",
    "QUAL O SEU MAIOR ORGULHO BOBO?",
    "SE O SEU HUMOR DE HOJE FOSSE UM CLIMA, QUAL SERIA?",
    "QUAL FOI A GENTILEZA ALEATÓRIA MAIS LEGAL QUE TE FIZERAM?",
};
#define QUESTIONS_N ((int)(sizeof(QUESTIONS) / sizeof(QUESTIONS[0])))

// --- estado --------------------------------------------------------------
static uint32_t s_accent  = KIT_COLOR_BLUE;
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

// Texto sobre a cor da Tool: preto no amarelo, paper no resto.
static uint32_t on_accent(void)
{
    return (s_accent == KIT_COLOR_YELLOW) ? KIT_COLOR_ON_YELLOW : KIT_COLOR_ON_COLOR;
}

static int rnd_index(void)
{
    const kit_api_table_t *t = api();
    int i;
    do {
        if (t && t->random) i = (int)t->random->range(0, QUESTIONS_N - 1);
        else                i = rand() % QUESTIONS_N;
    } while (i == s_last && QUESTIONS_N > 1);
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
    s_timer = lv_timer_create(draw_tick_cb, Q_DRAW_TICK_MS, NULL);
}

static void draw_tick_cb(lv_timer_t *t)
{
    (void)t;
    s_tick++;

    if (s_tick < Q_DRAW_TICKS) {
        int i;
        if (api() && api()->random) i = (int)api()->random->range(0, QUESTIONS_N - 1);
        else                        i = rand() % QUESTIONS_N;
        lv_label_set_text(s_phrase, QUESTIONS[i]);
        return;
    }

    // trava na pergunta sorteada (escolhida antes da animação)
    s_last = s_target;
    lv_label_set_text(s_phrase, QUESTIONS[s_target]);
    lv_obj_set_style_text_color(s_phrase, lv_color_hex(s_accent), 0);

    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    s_drawing = false;
    lv_obj_set_style_opa(s_go_btn, LV_OPA_COVER, 0);

    const kit_api_table_t *a = api();
    if (a && a->audio) a->audio->sfx(KIT_SFX_REVEAL);   // duas notas subindo, no fim
}

void kit_quebragelo_draw(void)
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
    lv_obj_set_size(chip, Q_CHIP, Q_CHIP);
    lv_obj_set_style_bg_color(chip, lv_color_hex(KIT_COLOR_SURFACE), 0);
    lv_obj_set_style_border_width(chip, 0, 0);
    lv_obj_set_style_radius(chip, 18, 0);
    lv_obj_set_style_pad_all(chip, 0, 0);
    lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(chip, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(chip, 12);
    lv_obj_add_event_cb(chip, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(chip, LV_ALIGN_TOP_LEFT, Q_PAD, 16);

    lv_obj_t *g = add_label(chip, KIT_ICON_BACK, KIT_COLOR_TEXT, &kit_display_44, 0);
    lv_obj_center(g);

    lv_obj_t *title = add_label(s_screen, "QUEBRA-GELO", KIT_COLOR_TEXT, &kit_mono_26, 2);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, Q_PAD + Q_CHIP + 12, 30);
}

// Palco: área tocável entre a titlebar e o botão, com a coluna
// "PERGUNTA" / pergunta / "PASSE ADIANTE" centralizada.
static void build_stage(void)
{
    lv_obj_t *stage = lv_obj_create(s_screen);
    lv_obj_remove_style_all(stage);
    lv_obj_set_size(stage, KIT_DISPLAY_WIDTH, Q_PAGE_H);
    lv_obj_set_pos(stage, 0, Q_TITLEBAR);
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

    s_lead_in = add_label(col, "PERGUNTA", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 3);
    lv_obj_add_flag(s_lead_in, LV_OBJ_FLAG_HIDDEN);

    // Pergunta protagonista: mono_26 caixa alta, quebra em várias linhas. O
    // pad vertical dá folga para acentos/cedilha da primeira e da última linha
    // não serem cortados (mesmo motivo do pad_bottom em build_timeout_list).
    s_phrase = add_label(col, "TOQUE EM SORTEAR", KIT_COLOR_TEXT_MUTED, &kit_mono_26, 2);
    lv_label_set_long_mode(s_phrase, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_phrase, Q_CONTENT);
    lv_obj_set_height(s_phrase, LV_SIZE_CONTENT);
    lv_obj_set_style_text_align(s_phrase, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(s_phrase, 6, 0);
    lv_obj_set_style_pad_bottom(s_phrase, 10, 0);

    s_lead_out = add_label(col, "PASSE ADIANTE", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 3);
    lv_obj_add_flag(s_lead_out, LV_OBJ_FLAG_HIDDEN);
}

static void build_footer(void)
{
    // lv_obj puro (sem tema) pelo mesmo motivo da Dice/Bottle/Primeiro Tool: o
    // lv_button aplica transform no estado PRESSED e desalinha o rótulo.
    s_go_btn = lv_obj_create(s_screen);
    lv_obj_set_size(s_go_btn, Q_CONTENT, Q_GO_H);
    lv_obj_set_style_radius(s_go_btn, Q_GO_H / 2, 0);
    lv_obj_set_style_border_width(s_go_btn, 0, 0);
    lv_obj_set_style_shadow_width(s_go_btn, 0, 0);
    lv_obj_set_style_pad_all(s_go_btn, 0, 0);
    lv_obj_set_style_bg_color(s_go_btn, lv_color_hex(s_accent), 0);
    lv_obj_set_style_bg_opa(s_go_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(s_go_btn, LV_OPA_80, LV_STATE_PRESSED);
    lv_obj_clear_flag(s_go_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_go_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_go_btn, 8);
    lv_obj_align(s_go_btn, LV_ALIGN_BOTTOM_MID, 0, -Q_GO_MARGIN);
    lv_obj_add_event_cb(s_go_btn, draw_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *l = add_label(s_go_btn, "SORTEAR", on_accent(), &kit_mono_26, 3);
    lv_obj_center(l);
}

// --- ciclo de vida ------------------------------------------------

kit_err_t kit_quebragelo_start(uint32_t accent)
{
    if (s_screen) kit_quebragelo_destroy();

    ESP_LOGI(TAG, "Montando Quebra-Gelo...");
    s_accent  = accent ? accent : KIT_COLOR_BLUE;
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

void kit_quebragelo_destroy(void)
{
    ESP_LOGI(TAG, "Encerrando Quebra-Gelo.");
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    s_drawing = false;

    if (s_screen) {
        lv_obj_delete(s_screen);
        s_screen = NULL;
    }
    s_lead_in = s_phrase = s_lead_out = s_go_btn = NULL;
}
