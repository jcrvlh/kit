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
} kit_tool_entry_t;

kit_err_t kit_tool_manager_init(void);
uint32_t  kit_tool_manager_get_count(void);
kit_err_t kit_tool_manager_get_entry(uint32_t index, kit_tool_entry_t *entry);
kit_err_t kit_tool_manager_start(const char *tool_id);
void      kit_tool_manager_start_last(void);
void      kit_tool_manager_stop_current(void);
kit_err_t kit_tool_manager_install(const char *pkg_path);
kit_err_t kit_tool_manager_uninstall(const char *tool_id);
void      kit_tool_manager_reload_catalog(void);

#ifdef __cplusplus
}
#endif
