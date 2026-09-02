/**
 * @file kit_lvgl.h
 * @brief Ponte entre a Tool e o LVGL — um único ponto de entrada para os tipos
 *        gráficos, resolvido conforme o alvo de compilação.
 *
 * - **Build nativo (`KIT_SDK_STUBS`)**: a lógica LVGL da Tool fica atrás de
 *   `#ifndef KIT_SDK_STUBS`, então aqui só definimos tipos suficientes para o
 *   código *parsear*. Nada é chamado.
 *
 * - **Build para o KIT (Xtensa, objeto compartilhado)**: inclui o `lvgl.h`
 *   **real** (a mesma versão do firmware, 9.5.x). A Tool compila contra as
 *   assinaturas e enums verdadeiros — zero risco de ABI — e as chamadas são
 *   resolvidas em tempo de carga contra a tabela de símbolos do firmware
 *   (`kit_tool_symbols.c`). A Tool nunca instancia nem desreferencia structs
 *   do LVGL (só usa handles opacos), então a config do LVGL (`lv_conf.h`) do
 *   ambiente de build não afeta a Tool.
 *
 * A **superfície garantida** (funções LVGL que o firmware exporta) está em
 * `tools-sdk/docs/tool_lvgl_runtime.md`. Chamar algo fora dela compila e linka,
 * mas falha no `dlopen` do dispositivo.
 *
 * @copyright GNU General Public License v3.0 (GPL-3.0)
 */
#pragma once

#ifdef KIT_SDK_STUBS

#include <stdint.h>

/** Handle opaco de objeto LVGL (na Tool, nunca é desreferenciado). */
typedef void lv_obj_t;

/** Espelho de `lv_color_t` do LVGL v9 (RGB888, 3 bytes). */
typedef struct {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
} lv_color_t;

/** Fonte LVGL — opaca no build nativo. */
typedef struct { uint8_t _placeholder; } lv_font_t;

#else  /* build real: LVGL de verdade */

#include "lvgl.h"

#endif /* KIT_SDK_STUBS */
