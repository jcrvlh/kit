#pragma once

#include "kit_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KIT_VERSION_MAJOR 0
#define KIT_VERSION_MINOR 2
#define KIT_VERSION_PATCH 0
#define KIT_VERSION_STRING "0.2.0"   // 0.2.0: kit_imu_api_t ganha register_tilt_callback

/**
 * Inicializa todo o ambiente operacional do KIT Runtime e periféricos.
 */
kit_err_t kit_runtime_init(void);

/**
 * Executa o loop principal de despacho de eventos do FreeRTOS/LVGL.
 */
void kit_runtime_run(void);

/**
 * Retorna se o dispositivo está executando o Launcher ou uma Tool.
 */
bool kit_runtime_is_in_tool(void);

/**
 * Marca que uma Tool está (ou não) em execução. Chamado pelo Tool Manager ao
 * iniciar uma Tool; a saída (kit_system_exit_impl / botão BOOT) volta o estado
 * para false. Controla se o botão BOOT sai da Tool ou apenas fecha sub-telas.
 */
void kit_runtime_set_in_tool(bool in_tool);

/**
 * Registra a "ação principal" da Tool ativa — enquanto uma Tool estiver rodando,
 * o toque curto no botão físico PWR dispara essa ação em vez de apagar a tela
 * (na Home o PWR continua ligando/desligando o painel). Passe NULL para Tools
 * sem ação principal. O Tool Manager limpa isso ao encerrar a Tool.
 * Ex.: a Dice Tool registra kit_dice_roll (PWR rola os dados).
 */
void kit_runtime_set_tool_primary_action(void (*action)(void));

/**
 * Liga/desliga o gesto de chacoalhar como atalho da ação principal na Tool
 * ativa. Padrão: ligado. Uma Tool em que gesticular é natural (ex: Veto, quem
 * descreve mexe as mãos) desliga para não disparar a ação sem querer — o PWR
 * continua valendo. O Tool Manager religa ao encerrar a Tool.
 */
void kit_runtime_set_tool_shake_enabled(bool enabled);

/**
 * Encerra a Tool ativa e retorna ao Launcher.
 */
void kit_system_exit_impl(void);


#ifdef __cplusplus
}
#endif
