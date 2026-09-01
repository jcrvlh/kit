#pragma once

#include "kit_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// Carregador dinâmico de Tools externas (objetos compartilhados .so relocados
// em PSRAM pelo espressif/elf_loader).
//
// Marco 1 do SD: prova o caminho crítico completo —
//   FAT (/sdcard) -> esp_elf relocate em PSRAM -> dlsym(tool_init/tool_destroy)
//   -> chamada com kit_tool_ctx_t -> a Tool desenha via símbolos LVGL
//   registrados e volta -> tool_destroy -> dlclose.

// Registra a tabela de símbolos do KIT (LVGL + libc extra) no elf_loader para
// que os objetos compartilhados das Tools resolvam suas referências externas.
// Idempotente; chamado uma vez pelo Runtime na inicialização.
kit_err_t kit_tool_loader_init(void);

// Carrega e inicializa uma Tool externa.
//   so_rel_path : caminho do .so relativo à base do dlopen (ex.:
//                 "tools/com.kit.hello/hello.so" -> /sdcard/tools/.../hello.so)
//   ctx         : contexto entregue a tool_init (tool_id, data_path, api).
// Em caso de sucesso a Tool fica ativa até kit_tool_loader_stop().
kit_err_t kit_tool_loader_start(const char *so_rel_path, kit_tool_ctx_t *ctx);

// Encerra a Tool externa ativa: chama tool_destroy() (se existir) e libera o
// objeto compartilhado da PSRAM. Seguro chamar sem Tool ativa.
void kit_tool_loader_stop(void);

// true enquanto houver uma Tool externa carregada.
bool kit_tool_loader_is_active(void);

#ifdef __cplusplus
}
#endif
