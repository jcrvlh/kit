#pragma once

#include "kit_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// Registra a tabela de símbolos (LVGL) usada pelos objetos compartilhados das
// Tools no elf_loader. Idempotente.
kit_err_t kit_tool_symbols_register(void);

#ifdef __cplusplus
}
#endif
