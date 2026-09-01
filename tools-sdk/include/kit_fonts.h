#pragma once

/**
 * @file kit_fonts.h
 * @brief Declarações das fontes disponíveis no KIT Runtime.
 *
 * O KIT embarca fontes otimizadas para a tela AMOLED 368×448:
 *
 * | Nome              | Família       | Tamanho | Uso Principal                          |
 * |-------------------|---------------|---------|----------------------------------------|
 * | `kit_mono_16`     | Space Mono    | 16px    | Tabelas de specs, valores MIN/MAX      |
 * | `kit_mono_20`     | Space Mono Bold| 20px   | Rótulos de botão, legendas em caixa alta|
 * | `kit_mono_26`     | Space Mono Bold| 26px   | Barra de status "KIT", títulos de tela |
 * | `kit_sans_22`     | Archivo Bold  | 22px    | Rótulos de linha de lista (caixa normal)|
 * | `kit_display_44`  | Archivo Black | 44px    | Wordmark "KIT", número grande do brilho|
 * | `kit_display_72`  | Archivo Black | 72px    | Resultado do Decisor (CARA/COROA/custom)|
 * | `kit_display_120` | Archivo Black | 120px   | Resultado dos Dados (número protagonista)|
 *
 * Todas as fontes incluem Latin + Latin-1 (acentuação PT-BR: Ã, Ç, Ç, É, Ê, Ó, Õ, Ú)
 * e um conjunto mínimo de ícones FontAwesome (ver @ref kit_theme.h).
 *
 * @section usage_fonts Exemplo de Uso
 * @code
 * #include "kit_fonts.h"
 * #include "kit_theme.h"
 *
 * // Título da tela
 * lv_obj_t *title = lv_label_create(screen);
 * lv_label_set_text(title, "MINHA TOOL");
 * lv_obj_set_style_text_font(title, &kit_mono_26, 0);
 * lv_obj_set_style_text_color(title, lv_color_hex(KIT_COLOR_TEXT), 0);
 * lv_obj_set_style_text_letter_space(title, 3, 0);  // tracking largo
 *
 * // Número grande
 * lv_obj_t *number = lv_label_create(screen);
 * lv_label_set_text(number, "42");
 * lv_obj_set_style_text_font(number, &kit_display_120, 0);
 * @endcode
 *
 * @note No SDK desktop (compilação com stubs), estas fontes são mapeadas
 * para `LV_FONT_DEFAULT` do LVGL. Na compilação real para o KIT, o
 * Runtime fornece os dados binários das fontes.
 *
 * @copyright GNU General Public License v3.0 (GPL-3.0)
 */

#ifdef KIT_SDK_STUBS
/* Compilação desktop (stubs): fontes LVGL padrão serão usadas.
 * Os ponteiros são declarados mas resolvidos nos stubs. */
#include <stdint.h>
typedef struct { uint8_t _placeholder; } lv_font_t;
#else
#include "lvgl.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Space Mono Regular 16px — tabelas, specs, valores secundários. */
extern const lv_font_t kit_mono_16;

/** Space Mono Bold 20px — rótulos de botão em CAIXA ALTA, legendas. */
extern const lv_font_t kit_mono_20;

/** Space Mono Bold 26px — títulos de tela, barra de status "KIT-XXXX". */
extern const lv_font_t kit_mono_26;

/** Archivo Bold 22px — rótulos de lista (caixa normal, não mono). */
extern const lv_font_t kit_sans_22;

/** Archivo Black 44px — wordmark "KIT", números médios (brilho). */
extern const lv_font_t kit_display_44;

/**
 * Archivo Black 72px — resultado do Decisor Tool.
 * @note Glyphs limitados: " - 0-9 A-Z Ã Ç Õ (caixa alta apenas).
 */
extern const lv_font_t kit_display_72;

/**
 * Archivo Black 120px — resultado dos Dados (número protagonista).
 * @note Glyphs limitados: dígitos 0-9, "-", "+".
 */
extern const lv_font_t kit_display_120;

#ifdef __cplusplus
}
#endif
