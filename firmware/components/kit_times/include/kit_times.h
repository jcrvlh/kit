#pragma once

#include "kit_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Sortear Times — realiza a ideia "sorteio de times" da Fase 2 do KIT
 * (Game Night). Divide a mesa em times equilibrados, sem digitação: o
 * usuário escolhe quantas pessoas e quantos times e o KIT lida as
 * posições numeradas (01..N) — cada um se conta pela roda da mesa.
 *
 * Tool interna, compilada junto do KIT Core e despachada pelo
 * kit_tool_manager a partir do id "com.kit.times".
 *
 * Duas páginas (tileview, arrasta na horizontal), no mesmo idioma da
 * Timer / Dice / Coin:
 *   0 AJUSTE   — PESSOAS (4 a 16, botões -/+) e TIMES (2/3/4).
 *   1 SORTEIO  — o palco. Botão SORTEAR (também toque no palco, PWR
 *                físico e chacoalhar). Página inicial.
 *
 * O resultado é sempre revelado **um a um**: o KIT passa de mão em mão e
 * cada toque (ou PWR) mostra, em tela cheia na cor do time, o time de uma
 * pessoa — o número "PESSOA X" grande é o que muda a cada avanço.
 *
 * A divisão é sempre equilibrada (tamanhos diferem no máximo em 1) e o
 * embaralhamento usa a Random API (TRNG de hardware). Config persistida
 * via Storage API (times_people / times_count). Sem histórico.
 *
 * Ciclo de vida no padrão das outras Tools: kit_times_start() monta e
 * carrega a tela; kit_times_destroy() derruba timers e objetos LVGL.
 *
 * @param accent  Cor principal da Tool (a cor do card na grade da Home).
 *                Passe 0 para o padrão (azul, o card "Times").
 */
kit_err_t kit_times_start(uint32_t accent);
void      kit_times_destroy(void);

/**
 * Ação principal da Tool — ligada ao botão físico PWR e ao chacoalhar
 * pelo Runtime enquanto a Tool está ativa
 * (ver kit_runtime_set_tool_primary_action).
 *
 * Durante a revelação, avança para a próxima pessoa (igual a tocar na
 * tela). Caso contrário, dispara um novo sorteio. Sem efeito se a tela
 * não estiver montada ou se um sorteio já estiver em curso.
 */
void      kit_times_draw(void);

#ifdef __cplusplus
}
#endif
