/**
 * @file kit_lvgl_min.h
 * @brief Compat — superava a "superfície mínima" do Marco 1.
 *
 * @deprecated Use `kit_lvgl.h` (incluído por `kit_tool_api.h`). No build para o
 * KIT ele traz o `lvgl.h` real; no build nativo, tipos opacos. A lista de
 * funções LVGL que o firmware resolve está em
 * `tools-sdk/docs/tool_lvgl_runtime.md`.
 *
 * Mantido só para não quebrar Tools antigas que incluíam este header e usavam
 * `KIT_LV_ALIGN_CENTER`.
 *
 * @copyright GNU General Public License v3.0 (GPL-3.0)
 */
#pragma once

#include "kit_tool_api.h"   /* -> kit_lvgl.h */

/** Compat: valor de `LV_ALIGN_CENTER`. Prefira `LV_ALIGN_CENTER` do lvgl.h. */
#ifndef KIT_LV_ALIGN_CENTER
#define KIT_LV_ALIGN_CENTER 9
#endif
