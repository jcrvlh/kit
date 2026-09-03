#pragma once

#include "kit_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char id[40];
    char name[32];
    char version[16];
    uint32_t version_code; // manifest "version_code"; 0 = ausente
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
kit_err_t kit_tool_manager_start_last(void);   // KIT_ERR_NOT_FOUND se não houver última Tool
void      kit_tool_manager_stop_current(void);
const char *kit_tool_manager_current(void);   // id da Tool rodando, "" se nenhuma

// Instala (ou atualiza) uma Tool a partir de um pacote `.kit` já baixado:
// remove `/sdcard/tools/<tool_id>` se existir, extrai o pacote para lá, apaga
// o `.kit` e recarrega o catálogo. Não faz verificação de assinatura (o
// chamador já conferiu o SHA-256 do download).
kit_err_t kit_tool_manager_install(const char *kit_path, const char *tool_id);

// Remove `/sdcard/tools/<tool_id>` e recarrega o catálogo. Recusa se a Tool
// estiver rodando.
kit_err_t kit_tool_manager_uninstall(const char *tool_id);
void      kit_tool_manager_reload_catalog(void);
void      kit_tool_manager_set_catalog_changed_cb(kit_tool_catalog_changed_cb_t cb);

#ifdef __cplusplus
}
#endif
