#include "kit_tool_manager.h"
#include "kit_api.h"
#include "kit_runtime.h"
#include "kit_dice.h"
#include "kit_bottle.h"
#include "kit_coin.h"
#include "kit_timer.h"
#include "kit_primeiro.h"
#include "kit_times.h"
#include "kit_bingo.h"
#include "kit_quebragelo.h"
#include "kit_fonts.h"
#include "kit_theme.h"
#include "esp_log.h"
#include "lvgl.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "KIT_TOOL_MGR";
static uint32_t s_installed_count = 0;
static char s_current_tool[32] = {0};
static char s_last_tool[32] = {0};
static lv_obj_t *s_test_tool_screen = NULL;
static lv_obj_t *s_touch_val_lbl = NULL;
static lv_obj_t *s_random_val_lbl = NULL;
static uint32_t s_tap_count = 0;

// A Test Tool segue a linguagem "Brutalist Bauhaus" do Launcher
// (ver docs/design/design-language.md): fundo AMOLED preto, tipografia
// monoespaçada em caixa alta, uma linha de diagnóstico por subsistema
// (chave à esquerda, valor à direita, fio embaixo — igual à tabela de
// specs da tela "Sobre") e uma pílula vermelha "SAIR".
#define TT_PAD      16
#define TT_CONTENT  (368 - 2 * TT_PAD)

static lv_obj_t *tt_label(lv_obj_t *parent, const char *txt, uint32_t color,
                          const lv_font_t *font, int letter_space)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(l, font, 0);
    if (letter_space) lv_obj_set_style_text_letter_space(l, letter_space, 0);
    return l;
}

// Devolve o label do valor (à direita) para os testes que atualizam ao vivo.
static lv_obj_t *tt_row(lv_obj_t *parent, const char *key, const char *val,
                        uint32_t val_color)
{
    lv_obj_t *r = lv_obj_create(parent);
    lv_obj_remove_style_all(r);
    lv_obj_set_size(r, TT_CONTENT, 52);
    lv_obj_set_style_border_width(r, 1, 0);
    lv_obj_set_style_border_side(r, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(r, lv_color_hex(KIT_COLOR_LINE), 0);
    lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *k = tt_label(r, key, KIT_COLOR_TEXT_MUTED, &kit_mono_16, 1);
    lv_obj_align(k, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t *v = tt_label(r, val, val_color, &kit_mono_16, 0);
    lv_obj_align(v, LV_ALIGN_RIGHT_MID, 0, 0);
    return v;
}

static void test_tool_exit_cb(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "Test Tool: saindo.");
    kit_system_exit_impl();
}

static void test_tool_touch_cb(lv_event_t *e)
{
    (void)e;
    s_tap_count++;
    lv_point_t p = {0};
    lv_indev_get_point(lv_indev_active(), &p);

    if (s_touch_val_lbl) {
        char buf[40];
        snprintf(buf, sizeof(buf), "X%d Y%d  %lu",
                 (int)p.x, (int)p.y, (unsigned long)s_tap_count);
        lv_label_set_text(s_touch_val_lbl, buf);
        lv_obj_set_style_text_color(s_touch_val_lbl, lv_color_hex(KIT_COLOR_GREEN), 0);
        lv_obj_align(s_touch_val_lbl, LV_ALIGN_RIGHT_MID, 0, 0);
    }
    if (s_random_val_lbl) {
        char buf[20];
        uint32_t r = kit_api_get_table()->random->u32();
        snprintf(buf, sizeof(buf), "0x%08lX", (unsigned long)r);
        lv_label_set_text(s_random_val_lbl, buf);
        lv_obj_align(s_random_val_lbl, LV_ALIGN_RIGHT_MID, 0, 0);
    }
}

static kit_err_t run_internal_test_tool(void)
{
    ESP_LOGI(TAG, "Montando UI da Test Tool (Brutalist Bauhaus)...");

    s_tap_count = 0;
    s_test_tool_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_test_tool_screen, lv_color_hex(KIT_COLOR_BG), 0);
    lv_obj_clear_flag(s_test_tool_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_test_tool_screen, test_tool_touch_cb, LV_EVENT_CLICKED, NULL);

    // Cabeçalho — a saída é o botão SAIR ou o botão físico BOOT.
    lv_obj_t *ttl = tt_label(s_test_tool_screen, "TEST TOOL", KIT_COLOR_TEXT, &kit_mono_26, 3);
    lv_obj_align(ttl, LV_ALIGN_TOP_LEFT, TT_PAD, 30);
    lv_obj_t *sub = tt_label(s_test_tool_screen, "DIAGNOSTICO DO SISTEMA",
                             KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);
    lv_obj_align(sub, LV_ALIGN_TOP_LEFT, TT_PAD, 66);

    // Corpo rolável entre o cabeçalho e a pílula fixa do rodapé.
    lv_obj_t *body = lv_obj_create(s_test_tool_screen);
    lv_obj_remove_style_all(body);
    lv_obj_set_size(body, 368, 448 - 104 - (KIT_TOUCH_TARGET_COMFORTABLE + 28));
    lv_obj_set_pos(body, 0, 104);
    lv_obj_set_style_pad_hor(body, TT_PAD, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_AUTO);

    const kit_api_table_t *api = kit_api_get_table();

    // Storage API — escreve e relê.
    api->storage->set_str("test_key", "KIT_OK");
    char sv[32] = {0};
    api->storage->get_str("test_key", sv, sizeof(sv));
    bool storage_ok = (strcmp(sv, "KIT_OK") == 0);

    kit_system_info_t info = {0};
    if (api->system) api->system->get_info(&info);
    char batt[12];
    snprintf(batt, sizeof(batt), "%d%%", info.battery_percentage);

    char rnd[20];
    snprintf(rnd, sizeof(rnd), "0x%08lX", (unsigned long)api->random->u32());

    tt_row(body, "RUNTIME", "OK v0.1.0", KIT_COLOR_GREEN);
    tt_row(body, "DISPLAY", "CO5300 OK", KIT_COLOR_GREEN);
    s_touch_val_lbl  = tt_row(body, "TOUCH", "TOQUE NA TELA", KIT_COLOR_TEXT_MUTED);
    tt_row(body, "STORAGE", storage_ok ? "OK" : "FALHA",
           storage_ok ? KIT_COLOR_GREEN : KIT_COLOR_RED);
    s_random_val_lbl = tt_row(body, "RANDOM", rnd, KIT_COLOR_TEXT);
    tt_row(body, "BATERIA", batt, KIT_COLOR_TEXT);

    // Pílula "SAIR" — ação destrutiva (vermelho), alvo confortável.
    lv_obj_t *exit_btn = lv_obj_create(s_test_tool_screen);
    lv_obj_set_size(exit_btn, TT_CONTENT, KIT_TOUCH_TARGET_COMFORTABLE);
    lv_obj_set_style_bg_color(exit_btn, lv_color_hex(KIT_COLOR_RED), 0);
    lv_obj_set_style_border_width(exit_btn, 0, 0);
    lv_obj_set_style_radius(exit_btn, KIT_TOUCH_TARGET_COMFORTABLE / 2, 0);
    lv_obj_set_style_pad_all(exit_btn, 0, 0);
    lv_obj_clear_flag(exit_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(exit_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(exit_btn, 8);
    lv_obj_align(exit_btn, LV_ALIGN_BOTTOM_MID, 0, -14);
    lv_obj_add_event_cb(exit_btn, test_tool_exit_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_center(tt_label(exit_btn, "SAIR", KIT_COLOR_ON_COLOR, &kit_mono_20, 3));

    lv_screen_load(s_test_tool_screen);
    return KIT_OK;
}

kit_err_t kit_tool_manager_init(void)
{
    ESP_LOGI(TAG, "Inicializando Tool Manager e varrendo /tools...");
    s_installed_count = 0;
    return KIT_OK;
}

void kit_tool_manager_reload_catalog(void)
{
    ESP_LOGI(TAG, "Recarregando catálogo de Tools...");
    kit_tool_manager_init();
    // No futuro, isso também notificará o Launcher (LVGL) para atualizar o carrossel.
}

uint32_t kit_tool_manager_get_count(void)
{
    return s_installed_count;
}

kit_err_t kit_tool_manager_get_entry(uint32_t index, kit_tool_entry_t *entry)
{
    if (index >= s_installed_count || !entry) return KIT_ERR_NOT_FOUND;
    return KIT_OK;
}

kit_err_t kit_tool_manager_start(const char *tool_id)
{
    ESP_LOGI(TAG, "Iniciando Tool '%s'...", tool_id);

    kit_err_t err = KIT_ERR_NOT_FOUND;
    void (*primary_action)(void) = NULL;
    if (strcmp(tool_id, "com.kit.test") == 0) {
        err = run_internal_test_tool();
    } else if (strcmp(tool_id, "com.kit.dice") == 0) {
        err = kit_dice_start(KIT_COLOR_RED);   // cor do card "Dados" na Home
        primary_action = kit_dice_roll;        // PWR físico rola os dados
    } else if (strcmp(tool_id, "com.kit.bottle") == 0) {
        err = kit_bottle_start(KIT_COLOR_BLUE); // cor do card "Garrafa" na Home
        primary_action = kit_bottle_spin;      // PWR físico gira a seta
    } else if (strcmp(tool_id, "com.kit.coin") == 0) {
        err = kit_coin_start(KIT_COLOR_YELLOW); // cor do card "Moeda" na Home
        primary_action = kit_coin_flip;        // PWR físico sorteia cara/coroa
    } else if (strcmp(tool_id, "com.kit.timer") == 0) {
        err = kit_timer_start(KIT_COLOR_GREEN); // cor do card "Timer" na Home
        primary_action = kit_timer_toggle;     // PWR físico começa/pausa
    } else if (strcmp(tool_id, "com.kit.primeiro") == 0) {
        err = kit_primeiro_start(KIT_COLOR_YELLOW); // cor do card "Primeiro" na Home
        primary_action = kit_primeiro_draw;        // PWR físico sorteia
    } else if (strcmp(tool_id, "com.kit.times") == 0) {
        err = kit_times_start(KIT_COLOR_BLUE); // cor do card "Times" na Home
        primary_action = kit_times_draw;      // PWR físico sorteia / avança a revelação
    } else if (strcmp(tool_id, "com.kit.bingo") == 0) {
        err = kit_bingo_start(KIT_COLOR_GREEN); // cor do card "Bingo" na Home
        primary_action = kit_bingo_draw;       // PWR físico sorteia o próximo número
    } else if (strcmp(tool_id, "com.kit.quebragelo") == 0) {
        err = kit_quebragelo_start(KIT_COLOR_RED); // cor do card "Quebra-Gelo" na Home
        primary_action = kit_quebragelo_draw;       // PWR físico sorteia a próxima pergunta
    }

    if (err == KIT_OK) {
        snprintf(s_current_tool, sizeof(s_current_tool), "%s", tool_id);
        snprintf(s_last_tool, sizeof(s_last_tool), "%s", tool_id);
        kit_runtime_set_in_tool(true);
        kit_runtime_set_tool_primary_action(primary_action);
    }
    return err;
}

void kit_tool_manager_start_last(void)
{
    if (s_last_tool[0] != '\0') {
        kit_tool_manager_start(s_last_tool);
    }
}

void kit_tool_manager_stop_current(void)
{
    ESP_LOGI(TAG, "Finalizando Tool ativa ('%s').", s_current_tool[0] ? s_current_tool : "-");
    kit_runtime_set_tool_primary_action(NULL);

    if (strcmp(s_current_tool, "com.kit.dice") == 0) {
        kit_dice_destroy();
    } else if (strcmp(s_current_tool, "com.kit.bottle") == 0) {
        kit_bottle_destroy();
    } else if (strcmp(s_current_tool, "com.kit.coin") == 0) {
        kit_coin_destroy();
    } else if (strcmp(s_current_tool, "com.kit.timer") == 0) {
        kit_timer_destroy();
    } else if (strcmp(s_current_tool, "com.kit.primeiro") == 0) {
        kit_primeiro_destroy();
    } else if (strcmp(s_current_tool, "com.kit.times") == 0) {
        kit_times_destroy();
    } else if (strcmp(s_current_tool, "com.kit.bingo") == 0) {
        kit_bingo_destroy();
    } else if (strcmp(s_current_tool, "com.kit.quebragelo") == 0) {
        kit_quebragelo_destroy();
    } else if (s_test_tool_screen) {
        lv_obj_delete(s_test_tool_screen);
        s_test_tool_screen = NULL;
    }

    s_current_tool[0] = '\0';
}

kit_err_t kit_tool_manager_install(const char *pkg_path)
{
    ESP_LOGI(TAG, "Instalando pacote .kit de '%s'...", pkg_path);
    return KIT_OK;
}

kit_err_t kit_tool_manager_uninstall(const char *tool_id)
{
    ESP_LOGI(TAG, "Desinstalando Tool '%s'...", tool_id);
    return KIT_OK;
}
