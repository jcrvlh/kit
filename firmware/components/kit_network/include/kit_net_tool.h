#pragma once

#include "kit_api.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Rede para as Tools do catálogo (kit_api_table_t.net, runtime >= 0.16.0).
 *
 * Só HTTPS GET, um pedido por vez, para os domínios que a Tool declarou em
 * "network_domains" no manifest. Sem redirect automático (o destino poderia
 * sair da lista), sem corpo nem header customizado, resposta até
 * KIT_NET_TOOL_BODY_MAX. O pedido roda numa task própria; o resultado é
 * entregue por kit_net_tool_poll(), chamado do loop do Runtime — o mesmo
 * contexto do LVGL, então a Tool pode mexer na UI dentro do callback.
 */

#define KIT_NET_TOOL_MAX_DOMAINS  4
#define KIT_NET_TOOL_DOMAIN_LEN   64
#define KIT_NET_TOOL_URL_MAX      256
#define KIT_NET_TOOL_BODY_MAX     (16 * 1024)

/**
 * Chamado pelo Tool Manager ao abrir uma Tool externa com a permissão
 * "network": define os domínios liberados. Zera qualquer pedido anterior.
 */
void kit_net_tool_begin(const char domains[][KIT_NET_TOOL_DOMAIN_LEN], int n);

/** Chamado ao sair da Tool: descarta pedido pendente e esquece os domínios. */
void kit_net_tool_end(void);

/** Entrega o resultado pronto (se houver) ao callback da Tool. Loop do Runtime. */
void kit_net_tool_poll(void);

/* Implementações da kit_net_api_t (kit_api.c). */
bool      kit_net_tool_is_online(void);
kit_err_t kit_net_tool_http_get(const char *url, kit_net_callback_t cb, void *user_data);
void      kit_net_tool_cancel(void);

#ifdef __cplusplus
}
#endif
