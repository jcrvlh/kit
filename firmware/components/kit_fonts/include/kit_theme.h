#pragma once

/**
 * Paleta do KIT — "Brutalist Bauhaus": preto AMOLED absoluto, as três
 * primitivas (quadrado vermelho, círculo azul, triângulo amarelo) como
 * identidade, e uma cor por papel semântico. A tipografia monoespaçada em
 * caixa alta com tracking largo (ver kit_fonts.h) carrega a hierarquia;
 * a cor é usada com parcimônia, uma superfície colorida por vez.
 */

/* -- Neutros -- */
#define KIT_COLOR_BG          0x000000  // Fundo AMOLED (preto absoluto)
#define KIT_COLOR_SURFACE     0x171719  // Linhas de lista, botões secundários
#define KIT_COLOR_SURFACE_ALT 0x222226  // Superfície elevada (badge dentro da linha)
#define KIT_COLOR_LINE        0x2C2C2E  // Fios, bordas, contorno tracejado
#define KIT_COLOR_TEXT        0xEFEADD  // Texto principal (off-white quente, "paper")
#define KIT_COLOR_TEXT_MUTED  0x6E6C66  // Rótulos apagados, legendas

/* -- Primárias Bauhaus -- */
#define KIT_COLOR_RED         0xC6472F  // Quadrado · erro / ação destrutiva
#define KIT_COLOR_BLUE        0x2C3CC4  // Círculo · informação
#define KIT_COLOR_YELLOW      0xE9B23C  // Triângulo · ação primária
#define KIT_COLOR_GREEN       0x45A05B  // Conexão · sucesso

/* -- Texto sobre superfícies cheias -- */
#define KIT_COLOR_ON_YELLOW   0x000000  // Sobre amarelo (preto, contraste alto)
#define KIT_COLOR_ON_COLOR    0xEFEADD  // Sobre vermelho / azul / verde

/**
 * Guideline de área de toque do KIT (tela pequena, dedo real — não mouse):
 * todo alvo interativo (botão, linha de lista, slider) deve ter pelo menos
 * KIT_TOUCH_TARGET_MIN de lado. KIT_TOUCH_TARGET_COMFORTABLE é o padrão a
 * usar em botões/linhas principais — só cai para o mínimo em controles
 * secundários (ex: botão fechar de um modal).
 */
#define KIT_TOUCH_TARGET_MIN         56
#define KIT_TOUCH_TARGET_COMFORTABLE 80

/* Ícones FontAwesome embutidos nas fontes kit_mono_* / kit_display_30
 * (UTF-8, para lv_label_set_text). */
#define KIT_ICON_PLUS      "\xEF\x81\xA7"  // 0xF067
#define KIT_ICON_SQUARE    "\xEF\x83\x88"  // 0xF0C8
#define KIT_ICON_CIRCLE    "\xEF\x84\x91"  // 0xF111
#define KIT_ICON_TRIANGLE  "\xEF\x83\x98"  // 0xF0D8 (caret-up)
#define KIT_ICON_BACK      "\xEF\x83\x99"  // 0xF0D9 (caret-left)
#define KIT_ICON_CHEVRON   "\xEF\x83\x9A"  // 0xF0DA (caret-right)
#define KIT_ICON_BARS      "\xEF\x80\x92"  // 0xF012 (signal)
#define KIT_ICON_CHECK     "\xEF\x80\x8C"  // 0xF00C (check)
#define KIT_ICON_PLAY      "\xEF\x81\x8B"  // 0xF04B (play — triângulo)
#define KIT_ICON_BOLT      "\xEF\x83\xA7"  // 0xF0E7 (bolt — carga)
