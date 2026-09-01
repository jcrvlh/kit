/**
 * @file kit_stubs.h
 * @brief API pública dos stubs — funções auxiliares para testes desktop.
 *
 * Inclua este header no seu test harness (não na Tool em si).
 *
 * @copyright GNU General Public License v3.0 (GPL-3.0)
 */

#pragma once

#include "kit_tool_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Retorna a tabela de API stub (equivalente a kit_api_get_table() do firmware).
 */
const kit_api_table_t *kit_stub_get_api_table(void);

/**
 * Cria um kit_tool_ctx_t preenchido com stubs, pronto para passar a tool_init().
 * @param tool_id ID da Tool (ou NULL para "com.kit.stub").
 */
kit_tool_ctx_t kit_stub_create_context(const char *tool_id);

#ifdef __cplusplus
}
#endif
