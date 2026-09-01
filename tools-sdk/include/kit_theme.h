#pragma once

/**
 * @file kit_theme.h
 * @brief Paleta de cores, constantes de layout e ícones do KIT.
 *
 * Parte do SDK de desenvolvimento de Tools — espelho exato do header
 * interno do firmware. Toda Tool deve usar estas constantes para
 * garantir consistência visual com o sistema.
 *
 * @section design_language Linguagem Visual: "Brutalist Bauhaus"
 *
 * O KIT adota a estética "Brutalist Bauhaus": fundo AMOLED preto
 * absoluto, tipografia monoespaçada em caixa alta com tracking largo,
 * e as três primitivas Bauhaus (quadrado vermelho, círculo azul,
 * triângulo amarelo) como identidade cromática.
 *
 * A cor é usada com parcimônia — **uma superfície colorida por vez**.
 * Cada Tool recebe uma cor "accent" (a cor do card na Home), que deve
 * ser usada no botão de ação principal e em destaques pontuais.
 *
 * @section usage Exemplo de Uso
 * @code
 * // Fundo preto AMOLED
 * lv_obj_set_style_bg_color(tela, lv_color_hex(KIT_COLOR_BG), 0);
 *
 * // Texto principal (off-white quente)
 * lv_obj_set_style_text_color(label, lv_color_hex(KIT_COLOR_TEXT), 0);
 *
 * // Botão de ação (vermelho para destrutiva, amarelo para primária)
 * lv_obj_set_style_bg_color(btn, lv_color_hex(KIT_COLOR_YELLOW), 0);
 * lv_obj_set_style_text_color(lbl, lv_color_hex(KIT_COLOR_ON_YELLOW), 0);
 * @endcode
 *
 * @copyright GNU General Public License v3.0 (GPL-3.0)
 */

/* -----------------------------------------------------------------------
 * Neutros — superfícies e texto
 * ----------------------------------------------------------------------- */

/** Fundo AMOLED — preto absoluto (#000000). Cada pixel apagado = economia de energia. */
#define KIT_COLOR_BG          0x000000

/** Superfície elevada nível 1 — linhas de lista, botões secundários. */
#define KIT_COLOR_SURFACE     0x171719

/** Superfície elevada nível 2 — badges, chips dentro de linhas. */
#define KIT_COLOR_SURFACE_ALT 0x222226

/** Fios, bordas, contornos — separadores visuais. */
#define KIT_COLOR_LINE        0x2C2C2E

/** Texto principal — off-white quente ("paper"), alta legibilidade no AMOLED. */
#define KIT_COLOR_TEXT        0xEFEADD

/** Texto secundário — rótulos apagados, legendas, placeholders. */
#define KIT_COLOR_TEXT_MUTED  0x6E6C66

/* -----------------------------------------------------------------------
 * Primárias Bauhaus — as três primitivas + verde
 * ----------------------------------------------------------------------- */

/** Quadrado vermelho — erro, ação destrutiva (ex: SAIR, LIMPAR). */
#define KIT_COLOR_RED         0xC6472F

/** Círculo azul — informação, navegação (ex: card Garrafa, Quebra-Gelo). */
#define KIT_COLOR_BLUE        0x2C3CC4

/** Triângulo amarelo — ação primária, destaque (ex: card Moeda, SORTEAR). */
#define KIT_COLOR_YELLOW      0xE9B23C

/** Verde — conexão, sucesso, confirmação (ex: card Timer, Bingo). */
#define KIT_COLOR_GREEN       0x45A05B

/* -----------------------------------------------------------------------
 * Texto sobre superfícies cheias (contraste)
 * ----------------------------------------------------------------------- */

/** Texto sobre fundo amarelo — preto, contraste WCAG AAA. */
#define KIT_COLOR_ON_YELLOW   0x000000

/** Texto sobre vermelho, azul ou verde — off-white quente. */
#define KIT_COLOR_ON_COLOR    0xEFEADD

/* -----------------------------------------------------------------------
 * Guidelines de Área de Toque
 * ----------------------------------------------------------------------- */

/**
 * Tamanho mínimo absoluto de um alvo de toque (pixels).
 * Só usar em controles secundários (ex: botão X de um modal).
 * Equivalente a ~10 mm na tela AMOLED do KIT.
 */
#define KIT_TOUCH_TARGET_MIN         56

/**
 * Tamanho confortável recomendado para alvos de toque (pixels).
 * Padrão para botões principais, linhas de lista e sliders.
 * Equivalente a ~14 mm na tela AMOLED do KIT.
 */
#define KIT_TOUCH_TARGET_COMFORTABLE 80

/* -----------------------------------------------------------------------
 * Ícones FontAwesome (embutidos nas fontes kit_mono_* e kit_display_30)
 *
 * Usar com lv_label_set_text(label, KIT_ICON_CHECK);
 * Cada ícone é uma sequência UTF-8 de 3 bytes.
 * ----------------------------------------------------------------------- */

/** ➕ Mais / Adicionar (U+F067). */
#define KIT_ICON_PLUS      "\xEF\x81\xA7"

/** ⬛ Quadrado (U+F0C8) — primitiva Bauhaus vermelha. */
#define KIT_ICON_SQUARE    "\xEF\x83\x88"

/** ⚫ Círculo (U+F111) — primitiva Bauhaus azul. */
#define KIT_ICON_CIRCLE    "\xEF\x84\x91"

/** 🔺 Triângulo / Caret-up (U+F0D8) — primitiva Bauhaus amarela. */
#define KIT_ICON_TRIANGLE  "\xEF\x83\x98"

/** ◀ Seta esquerda / Voltar (U+F0D9). */
#define KIT_ICON_BACK      "\xEF\x83\x99"

/** ▶ Seta direita / Chevron / Avançar (U+F0DA). */
#define KIT_ICON_CHEVRON   "\xEF\x83\x9A"

/** 📶 Barras de sinal (U+F012). */
#define KIT_ICON_BARS      "\xEF\x80\x92"

/** ✔ Check / Confirmar (U+F00C). */
#define KIT_ICON_CHECK     "\xEF\x80\x8C"

/** ▶ Play / Triângulo (U+F04B). */
#define KIT_ICON_PLAY      "\xEF\x81\x8B"

/** ⚡ Raio / Carga (U+F0E7). */
#define KIT_ICON_BOLT      "\xEF\x83\xA7"
