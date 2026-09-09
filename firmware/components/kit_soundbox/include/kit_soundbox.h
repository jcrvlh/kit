#pragma once

#include "kit_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Soundbox — mesa de sons de mão pro KIT.
 *
 * Uma grade de até 9 pads; cada toque toca um `.wav` do cartão microSD
 * (`s_api->audio->play_sample`). Um som novo corta o anterior — sem
 * polifonia, no ritmo de uma soundbox de zoeira.
 *
 * Os sons moram em `/sdcard/soundbox/<banco>/`. Cada subpasta é um "banco"
 * (um conjunto de pads); a página AJUSTE lista os bancos encontrados e o
 * usuário escolhe qual está na grade. Um `banco.json` opcional na pasta dá
 * o nome de exibição, a cor e o rótulo/ordem/cor de cada pad; sem ele, os
 * arquivos entram em ordem alfabética com o nome do arquivo como rótulo.
 *
 * Os bancos são feitos no conversor web (jcrvlh.github.io/kit/soundbox.html):
 * ele valida/converte o áudio pro formato que o KIT toca (WAV PCM 16-bit
 * mono 16 kHz) e entrega um `.zip` com a pasta pronta pra raiz do cartão.
 *
 * O banco escolhido persiste (índice em NVS `sb_bank`). Sem cartão ou sem a
 * pasta `soundbox/`, a Tool abre num estado vazio explicando o conversor.
 *
 * Tool interna, compilada junto do KIT Core e despachada pelo
 * kit_tool_manager a partir do id "com.kit.soundbox". Card verde na Home
 * (ferramenta, não mini-jogo).
 *
 * Tela: titlebar fixa + lv_tileview de 3 páginas (BANCOS / PADS / COMO USA,
 * começa em PADS).
 *
 * @param accent  Cor de destaque da Tool (a cor do card na Home). 0 → verde.
 */
kit_err_t kit_soundbox_start(uint32_t accent);
void      kit_soundbox_destroy(void);

/**
 * Ação principal (botão físico PWR): retoca o último pad acionado.
 */
void      kit_soundbox_replay(void);

#ifdef __cplusplus
}
#endif
