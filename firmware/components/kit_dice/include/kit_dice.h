#pragma once

#include "kit_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Dice Tool (Dados) — primeira Tool oficial do KIT (Fase 2).
 *
 * Tool interna, compilada junto do KIT Core e despachada pelo
 * kit_tool_manager a partir do id "com.kit.dice". Rola D4, D6, D8, D10,
 * D12, D20 e D100, com rolagem múltipla, modificador, soma automática,
 * animação de "tombo" das faces e feedback sonoro. A última configuração
 * (dado, quantidade, modificador) é persistida via Storage API.
 *
 * O ciclo de vida segue o padrão da Test Tool: kit_dice_start() monta e
 * carrega a tela; kit_dice_destroy() derruba timers e objetos LVGL.
 *
 * @param accent  Cor principal da Tool (a cor do card na grade da Home).
 *                Usada no botão ROLAR, no dado selecionado e no número em
 *                rolagem. Passe 0 para o padrão (vermelho, o card "Dados").
 */
kit_err_t kit_dice_start(uint32_t accent);
void      kit_dice_destroy(void);

/**
 * Ação principal da Tool — rola os dados. Ligada ao botão físico PWR pelo
 * Runtime enquanto a Dice Tool está ativa (ver kit_runtime_set_tool_primary_action).
 * Sem efeito se a tela não estiver montada ou já houver uma rolagem em curso.
 */
void      kit_dice_roll(void);

#ifdef __cplusplus
}
#endif
