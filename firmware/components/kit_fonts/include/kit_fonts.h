#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Família "Brutalist Bauhaus" (ver kit_theme.h). Cada fonte carrega o texto
 * Latin + Latin-1 (acentuação PT) e um conjunto mínimo de ícones FontAwesome:
 *   0xF067 mais  ·  0xF0C8 quadrado  ·  0xF111 círculo  ·  0xF0D8 triângulo
 *   0xF0D9 seta-esquerda (voltar)  ·  0xF0DA seta-direita (chevron)  ·  0xF012 barras
 *
 *   kit_mono_26  Space Mono Bold   — barra de status "KIT", títulos de tela
 *   kit_mono_20  Space Mono Bold   — rótulos de botão, legendas em caixa alta
 *   kit_mono_16  Space Mono Regular— tabela de especificações, MIN/MAX
 *   kit_sans_22  Archivo Bold      — rótulos de linha de lista (caixa normal)
 *   kit_sans_28  Archivo Bold      — frases da Introdução (onboarding)
 *   kit_display_44 Archivo Black   — wordmark "KIT", número grande do brilho
 *   kit_display_72 Archivo Black   — só " - 0-9 A-Z Ã Ç Õ": rótulo do resultado
 *                                    da Decisor Tool (CARA/COROA/SIM/NÃO/custom)
 *   kit_display_120 Archivo Black  — só dígitos + "-" "+": número do resultado
 *                                    da Dice Tool (protagonista da página)
 */
extern const lv_font_t kit_mono_16;
extern const lv_font_t kit_mono_20;
extern const lv_font_t kit_mono_26;
extern const lv_font_t kit_sans_22;
extern const lv_font_t kit_sans_28;
extern const lv_font_t kit_display_44;
extern const lv_font_t kit_display_72;
extern const lv_font_t kit_display_120;

#ifdef __cplusplus
}
#endif
