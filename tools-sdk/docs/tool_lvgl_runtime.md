# Runtime LVGL das Tools

Como a interface de uma Tool externa chega ao LVGL do KIT, e o que é garantido.

## Modelo

A Tool é um objeto compartilhado (`tool.so`, Xtensa) carregado com `dlopen` pelo
`kit_tool_loader`. Ela é compilada **sem libc e sem LVGL** (`-nostdlib -shared
-fvisibility=hidden`) — só `tool_init` e `tool_destroy` ficam visíveis. Toda
chamada a `lv_*` vira um símbolo indefinido, resolvido **no carregamento**
contra a tabela do firmware (`firmware/components/kit_tool_loader/src/kit_tool_symbols.c`).

### Por que não há risco de ABI

A Tool inclui o **`lvgl.h` real** (via `kit_lvgl.h`, na versão do firmware,
9.5.x), então assinaturas e valores de enum são os verdadeiros — nada é
transcrito à mão. E a Tool **nunca instancia nem desreferencia um struct do
LVGL**: só passa handles opacos (`lv_obj_t*`, `lv_timer_t*`), escalares e
`lv_color_t` (3 bytes, estável no LVGL 9). Logo o `lv_conf.h` do ambiente de
build não afeta o binário — daí o `-DLV_CONF_SKIP=1` no build da Tool.

`kit_lvgl.h` (incluído por `kit_tool_api.h`):

| Build | O que entra |
| :--- | :--- |
| nativo (`KIT_SDK_STUBS`) | tipos opacos; a UI fica atrás de `#ifndef KIT_SDK_STUBS` |
| KIT (Xtensa) | `#include "lvgl.h"` de verdade |

## Superfície garantida

Só o que está na tabela de símbolos resolve no dispositivo. Chamar algo fora
dela **compila e linka**, mas falha no `dlopen` (símbolo indefinido no log do
`KIT_TOOL_LOADER`). Para adicionar: uma entrada em `kit_tool_symbols.c` + rebuild
do firmware + registrar aqui.

**Objetos / árvore:** `lv_obj_create` · `lv_obj_delete` · `lv_obj_get_child` ·
`lv_screen_load`
**Flags / estilo base:** `lv_obj_add_flag` · `lv_obj_remove_flag`
(`lv_obj_clear_flag` é alias v8) · `lv_obj_remove_style_all`
**Posição / tamanho:** `lv_obj_set_pos` · `lv_obj_set_size` · `lv_obj_set_width` ·
`lv_obj_set_height` · `lv_obj_align` · `lv_obj_center` · `lv_obj_set_ext_click_area`
**Flex:** `lv_obj_set_flex_flow` · `lv_obj_set_flex_align` · `lv_obj_set_flex_grow`
**Scroll:** `lv_obj_set_scroll_dir` · `lv_obj_set_scrollbar_mode`
**Estilos locais:** `lv_obj_set_style_bg_color` · `…_bg_opa` · `…_border_width` ·
`…_shadow_width` · `…_radius` · `…_pad_top/bottom/left/right/row/column`
(`…_pad_all/hor/ver/gap` são `static inline` no 9.5 → expandem para essas) ·
`…_text_color` · `…_text_font` · `…_text_align` · `…_text_letter_space`
**Label:** `lv_label_create` · `lv_label_set_text` · `lv_label_set_text_fmt` ·
`lv_label_set_long_mode`
**Eventos:** `lv_obj_add_event_cb` · `lv_event_get_user_data`
**Timers:** `lv_timer_create` · `lv_timer_delete` · `lv_timer_set_period` ·
`lv_timer_set_repeat_count`
**Cor:** `lv_color_hex`
**Fontes (dados):** `kit_mono_16/20/26` · `kit_sans_22/28` · `kit_display_44/72/120`

Para **entrada de toque** e o resto do hardware, use a `kit_api_table_t`
(`ctx->api->input->register_callback`, etc.) — não o LVGL direto.

## Build

```bash
kit-cli build . --target native     # lógica no desktop (stubs)
kit-cli build . --target xtensa     # tool.so para o KIT (precisa do toolchain do IDF)
```

O `--target xtensa` espelha `tools-sdk/examples/hello_sd/build.sh`: compila
`src/*.c`, PIC + `-shared` + `-nostdlib`, faz strip das seções que o loader não
usa. Precisa dos headers do LVGL — localizados por:

1. `$KIT_LVGL_DIR` (checkout de `lvgl/lvgl` 9.5.x);
2. `firmware/managed_components/lvgl__lvgl` (após um build do firmware).

`manifest.json`: `entry_point` = `"tool.so"`.

## Estado de verificação

| Item | Verificado |
| :--- | :--- |
| Tabela de símbolos compila no firmware | ⏳ precisa de um build do firmware |
| `kit-cli build --target xtensa` gera `tool.so` | ⏳ precisa do toolchain Xtensa |
| `dlopen` da Tool no dispositivo | ❌ precisa de hardware |
| `hello_sd` (superfície mínima) carrega no HW | ✅ (Marco 1) |

Checklist ao validar em HW uma Tool de UI completa (ex.: Tarot):

1. `kit-cli build . --target xtensa` sem erro; `xtensa-esp32s3-elf-nm -D tool.so`
   mostra só `tool_init`/`tool_destroy` como `T`.
2. Instalar em `/sdcard/tools/<id>/` (`tool.so` + `manifest.json`).
3. Abrir no KIT. Se o log `KIT_TOOL_LOADER` acusar símbolo indefinido, adicionar
   à tabela e repetir.
4. Conferir layout, toque, timers e troca de telas.
