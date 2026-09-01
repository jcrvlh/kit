#pragma once

#include "kit_api.h"

#ifdef __cplusplus
extern "C" {
#endif

kit_err_t kit_launcher_init(void);
void      kit_launcher_show(void);

/**
 * Fecha qualquer sub-tela aberta (Ajustes, Brilho, Sobre, feedback, splash) e
 * volta para a Home. Chamada pelo botão BOOT via kit_runtime.
 */
void      kit_launcher_go_home(void);

#ifdef __cplusplus
}
#endif
