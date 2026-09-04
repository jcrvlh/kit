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
#include <stdio.h>    /* snprintf */
#include <string.h>   /* memmove */

static const char *TAG = "KIT_TOOL_SYM";

// Referências fortes para impedir que o linker do firmware descarte estes
// símbolos (a Tool os resolve por nome, não há chamada direta no firmware).
static const struct esp_elfsym s_kit_tool_symbols[] = {

    /* --- Ciclo de vida de objeto / árvore ------------------------------- */
    ESP_ELFSYM_EXPORT(lv_obj_create),
    ESP_ELFSYM_EXPORT(lv_obj_delete),
    ESP_ELFSYM_EXPORT(lv_obj_clean),          /* apaga só os filhos (Adedonha: relista a cartela) */
    ESP_ELFSYM_EXPORT(lv_obj_get_child),
    ESP_ELFSYM_EXPORT(lv_screen_load),

    /* --- Flags / estilo base ------------------------------------------- */
    ESP_ELFSYM_EXPORT(lv_obj_add_flag),
    ESP_ELFSYM_EXPORT(lv_obj_remove_flag),   /* lv_obj_clear_flag() é alias v8 */
    ESP_ELFSYM_EXPORT(lv_obj_has_flag),      /* Mímica: releitura de HIDDEN no palco */
    ESP_ELFSYM_EXPORT(lv_obj_remove_style_all),
    ESP_ELFSYM_EXPORT(lv_obj_invalidate),

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
    ESP_ELFSYM_EXPORT(lv_obj_set_style_border_color),  /* botão contornado, anel pulsante */
    ESP_ELFSYM_EXPORT(lv_obj_set_style_border_opa),
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

    ESP_ELFSYM_EXPORT(lv_obj_set_style_opa),
    ESP_ELFSYM_EXPORT(lv_obj_set_style_translate_x),
    ESP_ELFSYM_EXPORT(lv_obj_set_style_translate_y),
    ESP_ELFSYM_EXPORT(lv_obj_set_style_text_line_space),
    ESP_ELFSYM_EXPORT(lv_obj_set_style_min_width),
    ESP_ELFSYM_EXPORT(lv_obj_update_layout),
    ESP_ELFSYM_EXPORT(lv_pct),   /* lv_pct() virou função no LVGL 9 (era macro) */

    /* --- Label ------------------------------------------------------- */
    ESP_ELFSYM_EXPORT(lv_label_create),
    ESP_ELFSYM_EXPORT(lv_label_set_text),
    ESP_ELFSYM_EXPORT(lv_label_set_text_fmt),
    ESP_ELFSYM_EXPORT(lv_label_set_long_mode),

    /* --- Imagens / Bitmaps ------------------------------------------- */
    ESP_ELFSYM_EXPORT(lv_image_create),
    ESP_ELFSYM_EXPORT(lv_image_set_src),
    ESP_ELFSYM_EXPORT(lv_obj_set_style_image_recolor),
    ESP_ELFSYM_EXPORT(lv_obj_set_style_image_recolor_opa),

    /* --- QR code (Adedonha: link pro gerador de folhas; padrão do Bingo) - */
    ESP_ELFSYM_EXPORT(lv_qrcode_create),
    ESP_ELFSYM_EXPORT(lv_qrcode_set_size),
    ESP_ELFSYM_EXPORT(lv_qrcode_set_dark_color),
    ESP_ELFSYM_EXPORT(lv_qrcode_set_light_color),
    ESP_ELFSYM_EXPORT(lv_qrcode_set_quiet_zone),
    ESP_ELFSYM_EXPORT(lv_qrcode_update),

    /* --- Tileview (páginas que deslizam na horizontal, padrão da Dice) - */
    ESP_ELFSYM_EXPORT(lv_tileview_create),
    ESP_ELFSYM_EXPORT(lv_tileview_add_tile),
    ESP_ELFSYM_EXPORT(lv_tileview_set_tile_by_index),
    ESP_ELFSYM_EXPORT(lv_tileview_get_tile_active),

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

    /* --- libc que o elf_loader não exporta e Tools de UI usam -------
       (printf/puts/memcpy/memset/strlen/strcmp/malloc... já vêm do
        elf_loader; `rand` de propósito NÃO — use ctx->api->random.) */
    ESP_ELFSYM_EXPORT(snprintf),
    ESP_ELFSYM_EXPORT(memmove),   /* GCC emite p/ shift de array com sobreposição */
    /* O GCC sintetiza strcpy/strncpy/strlcpy a partir de snprintf(d,n,"%s",lit),
       inicialização de array e afins — o elf_loader traz só strlen/strcmp/strchr.
       Sem estes, uma Tool que os aciona falha no dlopen ("Can't find symbol"). */
    ESP_ELFSYM_EXPORT(strcpy),
    ESP_ELFSYM_EXPORT(strncpy),
    ESP_ELFSYM_EXPORT(strlcpy),
    ESP_ELFSYM_EXPORT(strcat),

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
