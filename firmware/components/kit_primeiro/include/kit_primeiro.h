#pragma once

#include "kit_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Quem Vai Primeiro — realiza a ideia "Quem começa?" da entrada
 * Random Tool (Sorteio).
 *
 * Tool interna, compilada junto do KIT Core e despachada pelo
 * kit_tool_manager a partir do id "com.kit.primeiro". Sorteia uma
 * característica de uma lista fixa ("é mais alta", "comeu feijão por
 * último", …) e mostra o texto grande no centro da tela em caixa alta;
 * quem se encaixa começa o jogo. Página única, sem ajuste, sem histórico
 * e sem persistência — o mesmo formato da Bottle Tool.
 *
 * O ciclo de vida segue o padrão da Dice/Bottle/Coin Tool:
 * kit_primeiro_start() monta e carrega a tela; kit_primeiro_destroy()
 * derruba o timer e os objetos LVGL.
 *
 * @param accent  Cor principal da Tool (a cor do card na grade da Home).
 *                Usada no botão SORTEAR e na frase sorteada. Passe 0 para
 *                o padrão (amarelo).
 */
kit_err_t kit_primeiro_start(uint32_t accent);
void      kit_primeiro_destroy(void);

/**
 * Ação principal da Tool — sorteia uma nova característica. Ligada ao
 * botão físico PWR e ao gesto de chacoalhar pelo Runtime enquanto a Tool
 * está ativa (ver kit_runtime_set_tool_primary_action). Sem efeito se a
 * tela não estiver montada ou se um sorteio já estiver em curso.
 */
void      kit_primeiro_draw(void);

#ifdef __cplusplus
}
#endif
