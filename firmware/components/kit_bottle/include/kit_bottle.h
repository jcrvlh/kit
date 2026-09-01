#pragma once

#include "kit_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Bottle Tool (Garrafa) — a brincadeira da garrafa no meio da roda.
 *
 * Tool interna, compilada junto do KIT Core e despachada pelo
 * kit_tool_manager a partir do id "com.kit.bottle". De 1 a 3 setas no centro
 * da tela giram rápido e vão desacelerando até parar apontando direções
 * quaisquer — as pessoas ficam fisicamente em volta do aparelho. Uma
 * página de AJUSTE (tileview, arrasta na horizontal) escolhe quantas setas
 * giram; a quantidade é persistida via Storage API ("bottle_count").
 *
 * O ciclo de vida segue o padrão da Dice Tool: kit_bottle_start() monta e
 * carrega a tela; kit_bottle_destroy() derruba o timer e os objetos LVGL.
 *
 * @param accent  Cor principal da Tool (a cor do card na grade da Home).
 *                Usada na seta e no botão GIRAR. Passe 0 para o padrão (azul).
 */
kit_err_t kit_bottle_start(uint32_t accent);
void      kit_bottle_destroy(void);

/**
 * Ação principal da Tool — gira as setas. Ligada ao botão físico PWR pelo
 * Runtime enquanto a Bottle Tool está ativa (ver
 * kit_runtime_set_tool_primary_action). Sem efeito se a tela não estiver
 * montada ou já houver um giro em curso.
 */
void      kit_bottle_spin(void);

#ifdef __cplusplus
}
#endif
