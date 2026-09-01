/**
 * @file main.c
 * @brief "Olá SD" — Tool de prova do Marco 1 do carregamento via cartão microSD.
 *
 * Compilada como objeto compartilhado Xtensa (hello.so), copiada para
 * /sdcard/tools/com.kit.hello/tool.so e carregada em runtime pelo
 * kit_tool_loader: FAT -> relocação em PSRAM -> dlsym -> tool_init(ctx).
 *
 * A Tool desenha um rótulo (símbolos LVGL resolvidos pelo firmware) e emite
 * um bipe (ponteiro de função na kit_api_table_t, recebido no contexto).
 */

#include "kit_tool_api.h"
#include "kit_lvgl_min.h"

/* Torna o símbolo visível no .dynsym para o dlsym do loader encontrar. */
#define KIT_TOOL_EXPORT __attribute__((visibility("default"), used))

static lv_obj_t *s_screen;

/*
 * IMPORTANTE (bug do elf_loader 1.3.3, ver tools-sdk/examples/hello_sd/README.md):
 * o loader calcula o fim de ".text" arredondado a 4 bytes, mas não confere se
 * isso invade a seção seguinte. Se ".rodata" começar sem folga logo depois de
 * ".text" (comum com strings, que só pedem alinhamento de 1 byte), o primeiro
 * ponteiro para ela é resolvido para dentro de ".text" por engano -> crash.
 * `aligned(4)` força o linker a deixar uma folga real antes de ".rodata".
 * Toda Tool com constantes globais (strings, tabelas) deve alinhar a PRIMEIRA
 * assim.
 */
static const char KIT_TOOL_MSG[] __attribute__((aligned(4))) =
    "OLA DO CARTAO SD\n\ntool.so relocada em RAM";

KIT_TOOL_EXPORT kit_err_t tool_init(kit_tool_ctx_t *ctx)
{
    if (!ctx || !ctx->api) {
        return KIT_ERR_INVALID_ARG;
    }

    s_screen = lv_obj_create(0);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x000000), 0);
    lv_screen_load(s_screen);

    lv_obj_t *lbl = lv_label_create(s_screen);
    lv_label_set_text(lbl, KIT_TOOL_MSG);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xF5F0E6), 0);
    lv_obj_align(lbl, KIT_LV_ALIGN_CENTER, 0, 0);

    if (ctx->api->audio) {
        ctx->api->audio->beep(1600, 80);
    }
    return KIT_OK;
}

KIT_TOOL_EXPORT void tool_destroy(void)
{
    if (s_screen) {
        lv_obj_delete(s_screen);
        s_screen = 0;
    }
}
