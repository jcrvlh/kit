/**
 * @file kit_lvgl_min.h
 * @brief Superfície mínima do LVGL v9 para Tools compiladas como objeto
 *        compartilhado (.so) do KIT — Marco 1 do carregamento via cartão SD.
 *
 * Uma Tool externa NÃO liga contra a biblioteca LVGL. As funções abaixo são
 * resolvidas em tempo de relocação pelo elf_loader do firmware, contra a
 * tabela `s_kit_tool_symbols` (ver firmware/components/kit_tool_loader).
 *
 * Só declare aqui o que o firmware realmente exporta. Manter os tipos
 * byte-a-byte iguais aos do LVGL v9.2 é obrigatório (ABI).
 *
 * @copyright GNU General Public License v3.0 (GPL-3.0)
 */

#pragma once

#include <stdint.h>

/* `lv_obj_t` já vem de kit_tool_api.h (typedef void). Inclua-o antes deste. */
#include "kit_tool_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Espelho exato de `lv_color_t` do LVGL v9 (RGB888, 3 bytes). */
typedef struct {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
} lv_color_t;

/** Subconjunto de `lv_align_t` usado pelas Tools. */
enum {
    KIT_LV_ALIGN_CENTER = 9,   /* LV_ALIGN_CENTER */
};

/* --- Funções exportadas pelo firmware (kit_tool_symbols.c) --------------- */

lv_obj_t   *lv_obj_create(lv_obj_t *parent);
void        lv_obj_delete(lv_obj_t *obj);
void        lv_obj_align(lv_obj_t *obj, int align, int32_t x_ofs, int32_t y_ofs);
void        lv_obj_set_style_bg_color(lv_obj_t *obj, lv_color_t value, uint32_t selector);
void        lv_obj_set_style_text_color(lv_obj_t *obj, lv_color_t value, uint32_t selector);
void        lv_screen_load(lv_obj_t *scr);
lv_obj_t   *lv_label_create(lv_obj_t *parent);
void        lv_label_set_text(lv_obj_t *obj, const char *text);
lv_color_t  lv_color_hex(uint32_t c);

#ifdef __cplusplus
}
#endif
