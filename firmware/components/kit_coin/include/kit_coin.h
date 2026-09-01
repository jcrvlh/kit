#pragma once

#include "kit_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Coin Tool (Moeda) — cara ou coroa com um toque.
 *
 * Tool interna, compilada junto do KIT Core e despachada pelo
 * kit_tool_manager a partir do id "com.kit.coin". Exibe um resultado
 * "CARA" ou "COROA" em destaque com animação de "flip" (troca rápida
 * de texto) e histórico das últimas jogadas. Uma página de AJUSTE
 * (tileview, arrasta na horizontal) não existe nesta tool — a interface
 * é deliberadamente mínima: página de RESULTADO e HISTÓRICO.
 *
 * O ciclo de vida segue o padrão da Dice Tool: kit_coin_start() monta e
 * carrega a tela; kit_coin_destroy() derruba timers e objetos LVGL.
 *
 * @param accent  Cor principal da Tool (a cor do card na grade da Home).
 *                Usada no botão SORTEAR e no resultado em "flip".
 *                Passe 0 para o padrão (amarelo, o card "Moeda").
 */
kit_err_t kit_coin_start(uint32_t accent);
void      kit_coin_destroy(void);

/**
 * Ação principal da Tool — sorteia cara ou coroa. Ligada ao botão físico
 * PWR pelo Runtime enquanto a Coin Tool está ativa
 * (ver kit_runtime_set_tool_primary_action).
 * Sem efeito se a tela não estiver montada ou já houver um flip em curso.
 */
void      kit_coin_flip(void);

#ifdef __cplusplus
}
#endif
