#pragma once

#include "kit_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Timer Tool (Cronômetro / Regressivo) — Fase 2 do KIT.
 *
 * Tool interna, compilada junto do KIT Core e despachada pelo
 * kit_tool_manager a partir do id "com.kit.timer".
 *
 * Duas páginas (tileview, arrasta na horizontal), no mesmo idioma da
 * Dice / Bottle / Coin:
 *   0 AJUSTE     — modo (CRONÔMETRO ↑ / REGRESSIVO ↓); no regressivo, tempos
 *                  fixos (3/5/10/15/30 min) + roda MM:SS (a mesma lv_roller
 *                  da Coin Tool) para digitar o tempo.
 *   1 RELÓGIO    — só o mostrador MM:SS e dois botões: PARAR e COMEÇAR
 *                  (alterna COMEÇAR → PAUSAR → CONTINUAR). Página inicial.
 *
 * Enquanto conta, a Tool segura o repouso/desligamento (kit_power keep-awake)
 * e apenas escurece o painel após ~15 s sem toque, sem apagar. Ao zerar a
 * contagem regressiva, roda uma animação de anéis + "TEMPO". Sem áudio.
 *
 * Ciclo de vida no padrão das outras Tools: kit_timer_start() monta e carrega
 * a tela; kit_timer_destroy() derruba timers e objetos LVGL.
 *
 * @param accent  Cor principal da Tool (a cor do card na grade da Home).
 *                Passe 0 para o padrão (verde, o card "Timer").
 */
kit_err_t kit_timer_start(uint32_t accent);
void      kit_timer_destroy(void);

/**
 * Ação principal da Tool — alterna COMEÇAR / PAUSAR / CONTINUAR, igual ao botão
 * primário do rodapé. Ligada ao botão físico PWR (e ao chacoalhar) pelo Runtime
 * enquanto a Timer Tool está ativa (ver kit_runtime_set_tool_primary_action).
 * Sem efeito com a tela não montada ou durante a animação de fim.
 */
void      kit_timer_toggle(void);

#ifdef __cplusplus
}
#endif
