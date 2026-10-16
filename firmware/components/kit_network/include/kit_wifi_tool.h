#pragma once

#include "kit_api.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Scan de Wi-Fi para as Tools do catálogo (kit_api_table_t.wifi, runtime >=
 * 0.18.0), liberado pela permissão "wifi_scan" no manifest.
 *
 * Um scan por vez, no máximo um a cada KIT_WIFI_SCAN_MIN_GAP_MS. O scan roda
 * numa task própria (kit_network_scan_aps bloqueia ~2 s); o resultado é
 * entregue por kit_wifi_tool_poll(), chamado do loop do Runtime — o mesmo
 * contexto do LVGL, então a Tool pode mexer na UI dentro do callback.
 */

/** Ao abrir uma Tool externa: zera qualquer scan anterior. */
void kit_wifi_tool_begin(void);

/** Ao sair da Tool: descarta o scan pendente (o callback não é chamado). */
void kit_wifi_tool_end(void);

/** Entrega o resultado pronto (se houver) ao callback da Tool. Loop do Runtime. */
void kit_wifi_tool_poll(void);

/* Implementações da kit_wifi_api_t (kit_api.c). */
kit_err_t kit_wifi_tool_scan(kit_wifi_scan_cb_t cb, void *user_data);
void      kit_wifi_tool_cancel(void);

#ifdef __cplusplus
}
#endif
