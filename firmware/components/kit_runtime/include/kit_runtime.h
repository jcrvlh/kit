#pragma once

#include "kit_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KIT_VERSION_MAJOR 0
#define KIT_VERSION_MINOR 1
#define KIT_VERSION_PATCH 0
#define KIT_VERSION_STRING "0.1.0"

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
 * Encerra a Tool ativa e retorna ao Launcher.
 */
void kit_system_exit_impl(void);


#ifdef __cplusplus
}
#endif
