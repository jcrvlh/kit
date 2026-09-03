#pragma once

#include "kit_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Placar — placar de mesa para qualquer jogo.
 *
 * De 2 a 4 jogadores lado a lado, cada um numa coluna grande com a pontuação em
 * fonte de display. Um TOQUE na coluna soma 1; SEGURAR subtrai 1. Passo fixo,
 * sem digitar.
 *
 * Meta é opcional (desligada por padrão): SEM META / 10 / 21 / 50 / 100. Quando
 * um jogador bate a meta, um overlay VENCEU anuncia (uma vez por jogador) e a
 * mesa decide continuar (toque fora) ou começar de novo.
 *
 * As INICIAIS de cada jogador são opcionais e seguem o padrão da Tool Fora: um
 * seletor JOGADOR N + três caixas que giram a letra (vazio → A → … → Z → vazio).
 * Sem iniciais, a coluna mostra só o número do jogador (#1..#4) na cor dele.
 *
 * A partida em andamento sobrevive a fechar/reabrir a Tool: pontuação, nº de
 * jogadores, meta e iniciais persistem no Storage. ZERAR (dois toques) limpa a
 * pontuação.
 *
 * Tool interna, compilada junto do KIT Core e despachada pelo kit_tool_manager
 * a partir do id "com.kit.placar". Card verde na Home (ferramenta, não mini-jogo).
 *
 * Tela: titlebar fixa + lv_tileview de 3 páginas (AJUSTE / PLACAR / COMO USA,
 * começa no PLACAR) + botão ZERAR fixo no rodapé da página PLACAR.
 *
 * @param accent  Cor de destaque da Tool (a cor do card na Home). 0 → verde.
 */
kit_err_t kit_placar_start(uint32_t accent);
void      kit_placar_destroy(void);

#ifdef __cplusplus
}
#endif
