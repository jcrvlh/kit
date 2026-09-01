#pragma once

#include "kit_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Quebra-Gelo — realiza a ideia "sorteio de listas rápidas" da Random Tool
 * (Fase 2, Game Night), na mesma linha da Quem Vai Primeiro: sorteia uma
 * pergunta quebra-gelo leve e criativa (de um baralho fixo de ~95) e mostra
 * o texto grande no centro da tela. Todo mundo na roda responde.
 *
 * Tool interna, compilada junto do KIT Core e despachada pelo
 * kit_tool_manager a partir do id "com.kit.quebragelo".
 *
 * Página única — sem tileview, sem ajuste, sem histórico, sem persistência
 * (o formato enxuto da Bottle / Quem Vai Primeiro):
 *   - titlebar fixa (chip de voltar + "QUEBRA-GELO");
 *   - a pergunta sorteada no centro, em kit_mono_26 CAIXA ALTA, quebrando em
 *     várias linhas — sem "wrap box" em volta;
 *   - "PERGUNTA" acima e "PASSE ADIANTE" abaixo, em mono apagado;
 *   - botão SORTEAR fixo no rodapé (cor da Tool).
 *
 * Sortear dispara pelo botão, por toque em qualquer lugar do palco e pelo
 * botão físico PWR / chacoalhar (kit_quebragelo_draw -> Runtime). Embaralha
 * num único lv_timer e trava na sorteada (nunca repete a anterior) + 1 bipe.
 *
 * @param accent  Cor principal da Tool (a cor do card na grade da Home).
 *                Passe 0 para o padrão (azul, o card "Quebra-Gelo").
 */
kit_err_t kit_quebragelo_start(uint32_t accent);
void      kit_quebragelo_destroy(void);

/**
 * Ação principal da Tool — ligada ao botão físico PWR e ao chacoalhar pelo
 * Runtime enquanto a Tool está ativa
 * (ver kit_runtime_set_tool_primary_action). Sorteia a próxima pergunta.
 * Sem efeito se a tela não estiver montada ou se um sorteio já estiver em
 * curso.
 */
void      kit_quebragelo_draw(void);

#ifdef __cplusplus
}
#endif
