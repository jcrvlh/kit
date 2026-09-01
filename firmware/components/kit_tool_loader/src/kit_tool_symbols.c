// Tabela de símbolos exportada para os objetos compartilhados das Tools.
//
// Uma Tool externa (.so) é compilada com -fvisibility=hidden e sem libc, então
// toda referência externa (LVGL, alguns utilitários) precisa ser resolvida em
// tempo de relocação pelo elf_loader. O elf_loader já traz tabelas de libc e do
// ESP-IDF; aqui adicionamos a superfície mínima do LVGL v9 que uma Tool de UI
// usa. A API do KIT (kit_api_table_t) NÃO entra aqui: ela chega à Tool por
// ponteiro, dentro do kit_tool_ctx_t.
//
// Marco 1: conjunto enxuto — criar uma tela, um label, alinhar, colorir,
// destruir. Cresce conforme as Tools reais precisarem.

#include "kit_tool_symbols.h"
#include "esp_elf.h"
#include "esp_log.h"

#include "lvgl.h"

#include <errno.h>

static const char *TAG = "KIT_TOOL_SYM";

// Referências fortes para impedir que o linker do firmware descarte estes
// símbolos (a Tool os resolve por nome, não há chamada direta no firmware).
static const struct esp_elfsym s_kit_tool_symbols[] = {
    ESP_ELFSYM_EXPORT(lv_obj_create),
    ESP_ELFSYM_EXPORT(lv_obj_delete),
    ESP_ELFSYM_EXPORT(lv_obj_align),
    ESP_ELFSYM_EXPORT(lv_obj_set_style_bg_color),
    ESP_ELFSYM_EXPORT(lv_obj_set_style_text_color),
    ESP_ELFSYM_EXPORT(lv_screen_load),
    ESP_ELFSYM_EXPORT(lv_label_create),
    ESP_ELFSYM_EXPORT(lv_label_set_text),
    ESP_ELFSYM_EXPORT(lv_color_hex),
    ESP_ELFSYM_END,
};

kit_err_t kit_tool_symbols_register(void)
{
    int ret = esp_elf_register_symbol(s_kit_tool_symbols);
    if (ret != 0 && ret != -EEXIST) {
        ESP_LOGE(TAG, "esp_elf_register_symbol falhou: %d", ret);
        return KIT_FAIL;
    }
    ESP_LOGI(TAG, "Tabela de símbolos das Tools registrada (%d símbolos LVGL).",
             (int)(sizeof(s_kit_tool_symbols) / sizeof(s_kit_tool_symbols[0]) - 1));
    return KIT_OK;
}
