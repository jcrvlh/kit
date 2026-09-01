// Tabela de símbolos exportada para os objetos compartilhados das Tools.
//
// Uma Tool externa (.so) é compilada com -fvisibility=hidden e sem libc, então
// toda referência externa (LVGL, fontes do KIT) precisa ser resolvida em tempo
// de relocação pelo elf_loader. O elf_loader já traz tabelas de libc e do
// ESP-IDF; aqui adicionamos a superfície do LVGL v9 e das fontes que uma Tool
// de UI usa. A API do KIT (kit_api_table_t) NÃO entra aqui: ela chega à Tool
// por ponteiro, dentro do kit_tool_ctx_t.
//
// Regra: só exportar o que uma Tool real precisa. Esta lista é a "superfície
// garantida" do SDK — ver tools-sdk/docs/tool_lvgl_runtime.md. Toda entrada
// nova aqui deve nascer de uma Tool que a exige e ser refletida na doc.

#include "kit_tool_symbols.h"
#include "esp_elf.h"
#include "esp_log.h"

#include "lvgl.h"
#include "kit_fonts.h"

#include <errno.h>

static const char *TAG = "KIT_TOOL_SYM";

// Referências fortes para impedir que o linker do firmware descarte estes
// símbolos (a Tool os resolve por nome, não há chamada direta no firmware).
static const struct esp_elfsym s_kit_tool_symbols[] = {

    /* --- Ciclo de vida de objeto / árvore ------------------------------- */
    ESP_ELFSYM_EXPORT(lv_obj_create),
    ESP_ELFSYM_EXPORT(lv_obj_delete),
    ESP_ELFSYM_EXPORT(lv_obj_get_child),
    ESP_ELFSYM_EXPORT(lv_screen_load),

    /* --- Flags / estilo base ------------------------------------------- */
    ESP_ELFSYM_EXPORT(lv_obj_add_flag),
    ESP_ELFSYM_EXPORT(lv_obj_remove_flag),   /* lv_obj_clear_flag() é alias v8 */
    ESP_ELFSYM_EXPORT(lv_obj_remove_style_all),

    /* --- Posição / tamanho / alinhamento ------------------------------- */
    ESP_ELFSYM_EXPORT(lv_obj_set_pos),
    ESP_ELFSYM_EXPORT(lv_obj_set_size),
    ESP_ELFSYM_EXPORT(lv_obj_set_width),
    ESP_ELFSYM_EXPORT(lv_obj_set_height),
    ESP_ELFSYM_EXPORT(lv_obj_align),
    ESP_ELFSYM_EXPORT(lv_obj_center),
    ESP_ELFSYM_EXPORT(lv_obj_set_ext_click_area),

    /* --- Layout flex -------------------------------------------------- */
    ESP_ELFSYM_EXPORT(lv_obj_set_flex_flow),
    ESP_ELFSYM_EXPORT(lv_obj_set_flex_align),
    ESP_ELFSYM_EXPORT(lv_obj_set_flex_grow),

    /* --- Scroll ------------------------------------------------------- */
    ESP_ELFSYM_EXPORT(lv_obj_set_scroll_dir),
    ESP_ELFSYM_EXPORT(lv_obj_set_scrollbar_mode),

    /* --- Estilos locais (parte/estado via selector) ------------------- */
    ESP_ELFSYM_EXPORT(lv_obj_set_style_bg_color),
    ESP_ELFSYM_EXPORT(lv_obj_set_style_bg_opa),
    ESP_ELFSYM_EXPORT(lv_obj_set_style_border_width),
    ESP_ELFSYM_EXPORT(lv_obj_set_style_shadow_width),
    ESP_ELFSYM_EXPORT(lv_obj_set_style_radius),
    /* lv_obj_set_style_pad_all/hor/ver/gap são `static inline` no LVGL 9.5:
       expandem para as 4 direções abaixo, que são os símbolos reais. */
    ESP_ELFSYM_EXPORT(lv_obj_set_style_pad_top),
    ESP_ELFSYM_EXPORT(lv_obj_set_style_pad_bottom),
    ESP_ELFSYM_EXPORT(lv_obj_set_style_pad_left),
    ESP_ELFSYM_EXPORT(lv_obj_set_style_pad_right),
    ESP_ELFSYM_EXPORT(lv_obj_set_style_pad_row),
    ESP_ELFSYM_EXPORT(lv_obj_set_style_pad_column),
    ESP_ELFSYM_EXPORT(lv_obj_set_style_text_color),
    ESP_ELFSYM_EXPORT(lv_obj_set_style_text_font),
    ESP_ELFSYM_EXPORT(lv_obj_set_style_text_align),
    ESP_ELFSYM_EXPORT(lv_obj_set_style_text_letter_space),

    /* --- Label ------------------------------------------------------- */
    ESP_ELFSYM_EXPORT(lv_label_create),
    ESP_ELFSYM_EXPORT(lv_label_set_text),
    ESP_ELFSYM_EXPORT(lv_label_set_text_fmt),
    ESP_ELFSYM_EXPORT(lv_label_set_long_mode),

    /* --- Eventos ---------------------------------------------------- */
    ESP_ELFSYM_EXPORT(lv_obj_add_event_cb),
    ESP_ELFSYM_EXPORT(lv_event_get_user_data),

    /* --- Timers ---------------------------------------------------- */
    ESP_ELFSYM_EXPORT(lv_timer_create),
    ESP_ELFSYM_EXPORT(lv_timer_delete),
    ESP_ELFSYM_EXPORT(lv_timer_set_period),
    ESP_ELFSYM_EXPORT(lv_timer_set_repeat_count),

    /* --- Cor ------------------------------------------------------ */
    ESP_ELFSYM_EXPORT(lv_color_hex),

    /* --- Fontes do KIT (dados) ---------------------------------- */
    ESP_ELFSYM_EXPORT(kit_mono_16),
    ESP_ELFSYM_EXPORT(kit_mono_20),
    ESP_ELFSYM_EXPORT(kit_mono_26),
    ESP_ELFSYM_EXPORT(kit_sans_22),
    ESP_ELFSYM_EXPORT(kit_sans_28),
    ESP_ELFSYM_EXPORT(kit_display_44),
    ESP_ELFSYM_EXPORT(kit_display_72),
    ESP_ELFSYM_EXPORT(kit_display_120),

    ESP_ELFSYM_END,
};

kit_err_t kit_tool_symbols_register(void)
{
    int ret = esp_elf_register_symbol(s_kit_tool_symbols);
    if (ret != 0 && ret != -EEXIST) {
        ESP_LOGE(TAG, "esp_elf_register_symbol falhou: %d", ret);
        return KIT_FAIL;
    }
    ESP_LOGI(TAG, "Tabela de símbolos das Tools registrada (%d entradas).",
             (int)(sizeof(s_kit_tool_symbols) / sizeof(s_kit_tool_symbols[0]) - 1));
    return KIT_OK;
}
