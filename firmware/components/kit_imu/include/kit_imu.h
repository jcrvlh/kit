#pragma once

#include "kit_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * IMU QMI8658 (6-DOF, I2C 0x6B) — só o acelerômetro é usado, para o gesto de
 * "chacoalhar" (Shake to Roll). Ver docs/hardware/imu.md.
 *
 * kit_imu_init() configura o sensor (±8 g, 125 Hz, giroscópio desligado).
 * Falha é não-fatal: kit_imu_poll_shake() apenas devolve false.
 */
kit_err_t kit_imu_init(void);

/**
 * Lê o acelerômetro e devolve true quando o módulo da aceleração passa do
 * limiar de "chacoalhar" (com debounce interno de ~0,7 s). Deve ser chamado
 * periodicamente pelo Runtime (~a cada 50–60 ms) enquanto há uma Tool ativa.
 */
bool kit_imu_poll_shake(void);

/**
 * Liga/desliga o acelerômetro (CTRL7.bit0 do QMI8658). O Runtime desliga
 * quando a tela entra em repouso — sem gesto de chacoalhar, o sensor só
 * gastaria bateria — e religa ao acordar. Chamada barata e idempotente.
 */
void kit_imu_set_enabled(bool enable);

/**
 * Despacha o callback de shake registrado por uma Tool externa.
 * Chamado pelo Runtime quando kit_imu_poll_shake() retorna true.
 */
void kit_imu_dispatch_shake(void);

/**
 * Remove o callback de shake (usado pelo Runtime ao encerrar uma Tool).
 */
void kit_imu_clear_shake_callback(void);

/**
 * Implementação do register_shake_callback da API table (kit_api.h).
 * Não chamar diretamente — é exportado via kit_api_table_t.imu.
 */
kit_err_t kit_imu_register_shake_callback_impl(kit_shake_callback_t cb, void *user_data);

/**
 * Gesto de inclinar (Tool "Testa"). Lê o acelerômetro e devolve KIT_TILT_DOWN /
 * KIT_TILT_UP quando o eixo normal à tela cruza o limiar, uma única vez por
 * inclinada (só rearma quando o aparelho volta a ~vertical). KIT_TILT_NONE no
 * resto. O Runtime só chama isto enquanto a Tool ativa pediu o gesto.
 */
kit_tilt_t kit_imu_poll_tilt(void);

/** Despacha o callback de inclinar registrado pela Tool ativa. */
void kit_imu_dispatch_tilt(kit_tilt_t dir);

/** Remove o callback de inclinar (usado pelo Runtime ao encerrar uma Tool). */
void kit_imu_clear_tilt_callback(void);

/**
 * Implementação do register_tilt_callback da API table (kit_api.h).
 * Não chamar diretamente — é exportado via kit_api_table_t.imu.
 */
kit_err_t kit_imu_register_tilt_callback_impl(kit_tilt_callback_t cb, void *user_data);

#ifdef __cplusplus
}
#endif
