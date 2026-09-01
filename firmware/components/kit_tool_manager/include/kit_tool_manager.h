#pragma once

#include "kit_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char id[32];
    char name[32];
    char version[16];
    char description[64];
    uint32_t size_bytes;
    uint32_t accent;      // cor do card na Home (0xRRGGBB); 0 = sem cor no manifest
    char icon[16];        // nome do ícone da Home; "" = ícone genérico de cartão
    bool is_game;         // true = mini-jogo (manifest "kind":"game"); false = ferramenta
} kit_tool_entry_t;

// Chamado quando o catálogo do cartão muda (recarga, formatação, novo cartão).
// O Launcher usa isso para redesenhar a Home sem exigir reboot.
typedef void (*kit_tool_catalog_changed_cb_t)(void);

kit_err_t kit_tool_manager_init(void);
uint32_t  kit_tool_manager_get_count(void);
kit_err_t kit_tool_manager_get_entry(uint32_t index, kit_tool_entry_t *entry);
kit_err_t kit_tool_manager_start(const char *tool_id);
void      kit_tool_manager_start_last(void);
void      kit_tool_manager_stop_current(void);
kit_err_t kit_tool_manager_install(const char *pkg_path);
kit_err_t kit_tool_manager_uninstall(const char *tool_id);
void      kit_tool_manager_reload_catalog(void);
void      kit_tool_manager_set_catalog_changed_cb(kit_tool_catalog_changed_cb_t cb);

#ifdef __cplusplus
}
#endif
