#pragma once

#include "kit_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Inicializa o subsistema de comunicações do KIT.
 * Cria a tarefa FreeRTOS em background que escuta a porta Serial (USB CDC)
 * em busca de comandos do protocolo de upload de Tools (KIT_TOOL_BEGIN).
 */
kit_err_t kit_comms_init(void);

#ifdef __cplusplus
}
#endif
