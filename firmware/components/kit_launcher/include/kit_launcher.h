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

/**
 * Libera o slideshow da Home (deck + pontos de página) para dar folga de RAM
 * ao tool_init de uma Tool. O deck é reconstruído ao voltar (kit_launcher_go_home).
 * Chamada pelo kit_runtime no relançamento pelo botão BOOT.
 */
void      kit_launcher_release_home_deck(void);

#ifdef __cplusplus
}
#endif
