#pragma once

#include "kit_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Globo de Bingo — realiza a entrada "Bingo Tool" da Fase 2 do KIT
 * (Game Night): um globo de bingo digital que sorteia números sem
 * repetir e guarda o painel de chamadas para conferir a cartela.
 *
 * Tool interna, compilada junto do KIT Core e despachada pelo
 * kit_tool_manager a partir do id "com.kit.bingo".
 *
 * Três páginas (tileview, arrasta na horizontal), no mesmo idioma da
 * Times / Dice / Timer:
 *   0 AJUSTE     — FAIXA (1–75, com a letra B/I/N/G/O, ou 1–90) e
 *                  REINICIAR SORTEIO (dois toques para confirmar).
 *   1 GLOBO      — o palco. Número grande + letra da coluna + contador.
 *                  Botão SORTEAR (também toque no palco, PWR físico e
 *                  chacoalhar). Página inicial.
 *   2 CHAMADAS   — o painel inteiro da faixa: os números já sorteados
 *                  acesos, o último com um anel. Rola na vertical.
 *
 * O sorteio é sem reposição, alimentado pela Random API (TRNG de
 * hardware). Diferente da Sortear Times, a **rodada persiste**: faixa e
 * números já sorteados vão para o Storage (bingo_range / bingo_drawn) e
 * voltam ao reabrir a Tool — um jogo de bingo dura. Trocar a faixa ou
 * tocar REINICIAR zera o painel.
 *
 * Ciclo de vida no padrão das outras Tools: kit_bingo_start() monta e
 * carrega a tela; kit_bingo_destroy() derruba timers e objetos LVGL.
 *
 * @param accent  Cor principal da Tool (a cor do card na grade da Home).
 *                Passe 0 para o padrão (verde, o card "Bingo").
 */
kit_err_t kit_bingo_start(uint32_t accent);
void      kit_bingo_destroy(void);

/**
 * Ação principal da Tool — ligada ao botão físico PWR e ao chacoalhar
 * pelo Runtime enquanto a Tool está ativa
 * (ver kit_runtime_set_tool_primary_action).
 *
 * Sorteia o próximo número. Sem efeito se a tela não estiver montada, se
 * um sorteio já estiver em curso ou se a faixa já tiver acabado.
 */
void      kit_bingo_draw(void);

#ifdef __cplusplus
}
#endif
