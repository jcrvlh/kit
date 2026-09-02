#include "kit_runtime.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "lvgl.h"
#include "kit_config.h"
#include "kit_display.h"
#include "kit_input.h"
#include "kit_storage.h"
#include "kit_power.h"
#include "kit_random.h"
#include "kit_time.h"
#include "kit_audio.h"
#include "kit_imu.h"
#include "kit_tool_manager.h"
#include "kit_tool_loader.h"
#include "kit_network.h"
#include "kit_catalog.h"
#include "kit_launcher.h"

#include "kit_theme.h"
#include "kit_fonts.h"

static const char *TAG = "KIT_RUNTIME";
static bool s_is_in_tool = false;
static void (*s_tool_primary_action)(void) = NULL;

// Botões físicos do sistema.
//   PWR  — tecla PWRON do PMIC AXP2101 (toque curto: liga/desliga a tela).
//   BOOT — GPIO0, ativo-baixo (toque: volta para a Home / sai da Tool).
#define KIT_BOOT_BTN   GPIO_NUM_0
static bool s_screen_on = true;
static int  s_boot_prev = 1;

static lv_obj_t *s_shutdown_overlay = NULL;
static lv_obj_t *s_shutdown_label = NULL;
static lv_timer_t *s_shutdown_timer = NULL;
static int s_shutdown_count = 3;

static void kit_runtime_show_shutdown_screen(void);

// Marca "houve atividade" no LVGL — usado pelos gestos físicos (PWR, chacoalhar)
// que não passam pelo indev de toque, para que o repouso/desligamento por
// inatividade não dispare enquanto a pessoa está mexendo no aparelho.
static void note_activity(void)
{
    lv_display_trigger_activity(NULL);
}

static void screen_set_on(bool on)
{
    s_screen_on = on;
    kit_display_set_on_impl(on);
    // Com a tela apagada o touch do LVGL também fica desligado (evita toques
    // fantasma no CST820 do barramento compartilhado). O aparelho acorda pelo
    // botão PWR ou por um toque deliberado na tela (ver poll_wake_touch), que é
    // consumido só para acordar — não chega à UI.
    kit_input_set_enabled_impl(on);

    // Repouso = "meia-hibernação": sem tela, desligamos o que não faz falta
    // para poupar bateria e religamos tudo ao acordar.
    //   - acelerômetro (gesto de chacoalhar não é usado com a tela apagada);
    //   - áudio (nenhum efeito novo é enfileirado);
    //   - Wi-Fi: rádio desligado (o catálogo/OTA seguram keep-awake, então
    //     nunca dormimos no meio de um download);
    //   - CPU: light sleep automático entre os polls (só na bateria).
    kit_imu_set_enabled(on);
    kit_audio_suspend(!on);
    kit_network_suspend(!on);
    kit_power_set_screen_sleeping(!on);

    if (on) note_activity();
}

// Enquanto true, poll_wake_touch continua rodando mesmo com a tela já ligada,
// esperando o dedo soltar para devolver o toque ao LVGL.
static bool s_wake_pending = false;

// Com a tela em repouso, detecta um toque na tela para acordá-la. Lê o CST820
// direto (o indev do LVGL está desligado) e exige duas leituras seguidas com
// toque para filtrar os blips fantasma do barramento I2C compartilhado. O toque
// que acorda fica preso aqui até soltar, pra não disparar um item da Home.
static void poll_wake_touch(void)
{
    static int touch_streak = 0;

    bool present = kit_input_touch_present_impl();

    if (s_wake_pending) {
        note_activity(); // o dedo continua na tela; não deixa re-dormir
        if (!present) {
            s_wake_pending = false;
            kit_input_set_enabled_impl(s_screen_on);
        }
        return;
    }

    if (present) {
        if (++touch_streak >= 2) {
            touch_streak = 0;
            ESP_LOGI(TAG, "Toque na tela: acordando do repouso");
            screen_set_on(true);
            kit_input_set_enabled_impl(false); // segura até soltar o dedo
            s_wake_pending = true;
        }
    } else {
        touch_streak = 0;
    }
}

// Repouso da tela e desligamento automático por inatividade (configurados em
// Ajustes, persistidos por kit_config). Chamado a cada ~1 s pelo loop.
static void poll_inactivity(void)
{
    uint32_t idle_ms  = lv_display_get_inactive_time(NULL);
    uint32_t sleep_s  = kit_config_get_screen_sleep_s();
    uint32_t off_s    = kit_config_get_auto_poweroff_s();

    // Keep-Awake (kit_power): uma Tool pode segurar o repouso e o desligamento
    // automático enquanto precisa da tela viva (ex.: a Timer Tool contando —
    // ela mesma escurece o painel para poupar bateria, sem apagar).
    if (kit_power_is_keep_awake()) return;

    // Já em repouso: reavalia o light sleep (ex.: USB plugado/desplugado
    // enquanto a tela está apagada). Chamada barata e idempotente.
    if (!s_screen_on) kit_power_set_screen_sleeping(true);

    if (s_screen_on && sleep_s && idle_ms >= sleep_s * 1000) {
        ESP_LOGI(TAG, "Repouso: apagando a tela após %lu s sem atividade",
                 (unsigned long)sleep_s);
        screen_set_on(false);
    }

    if (off_s && idle_ms >= off_s * 1000 && !kit_power_is_charging()) {
        ESP_LOGW(TAG, "Desligamento automático após %lu s sem atividade",
                 (unsigned long)off_s);
        kit_power_shutdown();
    }
}

static void kit_runtime_cancel_shutdown(void)
{
    if (s_shutdown_overlay) {
        ESP_LOGI(TAG, "Desligamento cancelado (PWR solto).");
        lv_obj_delete(s_shutdown_overlay);
        s_shutdown_overlay = NULL;
        if (s_shutdown_timer) {
            lv_timer_delete(s_shutdown_timer);
            s_shutdown_timer = NULL;
        }
    }
}

static void shutdown_timer_cb(lv_timer_t *timer)
{
    if (!kit_power_is_pwr_pressed()) {
        kit_runtime_cancel_shutdown();
        return;
    }

    s_shutdown_count--;
    if (s_shutdown_count <= 0) {
        lv_timer_delete(timer);
        s_shutdown_timer = NULL;
        kit_power_shutdown();
    } else {
        if (s_shutdown_label) {
            lv_label_set_text_fmt(s_shutdown_label, "%d", s_shutdown_count);
        }
    }
}

static void kit_runtime_show_shutdown_screen(void)
{
    if (s_shutdown_overlay) return;
    s_shutdown_count = 3;
    s_shutdown_overlay = lv_obj_create(lv_layer_sys());
    lv_obj_remove_style_all(s_shutdown_overlay);
    lv_obj_set_size(s_shutdown_overlay, 368, 448);
    lv_obj_set_style_bg_color(s_shutdown_overlay, lv_color_hex(KIT_COLOR_RED), 0);
    lv_obj_set_style_bg_opa(s_shutdown_overlay, LV_OPA_COVER, 0);
    
    s_shutdown_label = lv_label_create(s_shutdown_overlay);
    lv_obj_set_style_text_color(s_shutdown_label, lv_color_hex(KIT_COLOR_BG), 0);
    lv_obj_set_style_text_font(s_shutdown_label, &kit_display_120, 0);
    lv_label_set_text_fmt(s_shutdown_label, "%d", s_shutdown_count);
    lv_obj_align(s_shutdown_label, LV_ALIGN_CENTER, 0, -40);

    lv_obj_t *text = lv_label_create(s_shutdown_overlay);
    lv_obj_set_style_text_color(text, lv_color_hex(KIT_COLOR_BG), 0);
    lv_obj_set_style_text_font(text, &kit_mono_26, 0);
    lv_label_set_text(text, "DESLIGANDO...");
    lv_obj_align(text, LV_ALIGN_CENTER, 0, 60);

    s_shutdown_timer = lv_timer_create(shutdown_timer_cb, 1000, NULL);
}

static void poll_system_buttons(void)
{
    kit_pek_event_t pek = kit_power_poll_pek_event();
    
    if (s_shutdown_overlay && !kit_power_is_pwr_pressed()) {
        kit_runtime_cancel_shutdown();
    }

    if (pek == KIT_PEK_LONG) {
        kit_runtime_show_shutdown_screen();
        return;
    }
    if (pek == KIT_PEK_SHORT) {
        note_activity();
        // Dentro de uma Tool, o PWR curto dispara a ação principal dela
        // (ex.: rolar os dados). Na Home, liga/desliga o painel.
        if (s_is_in_tool && s_tool_primary_action && s_screen_on) {
            s_tool_primary_action();
        } else if (s_screen_on) {
            // Aperta-e-solta o PWR: cadeado fechando. O som é enfileirado antes
            // de screen_set_on() suspender o áudio, então ainda toca.
            kit_audio_sfx_impl(KIT_SFX_LOCK);
            screen_set_on(false);
        } else {
            // Cadeado abrindo: acorda primeiro (levanta a suspensão do áudio),
            // depois toca.
            screen_set_on(true);
            kit_audio_sfx_impl(KIT_SFX_UNLOCK);
        }
        return;
    }

    if (!s_screen_on) return;   // tela apagada: PWR e toque acordam (poll_wake_touch)

    int lvl = gpio_get_level(KIT_BOOT_BTN);
    if (s_boot_prev == 0 && lvl == 1) {   // soltou o BOOT
        if (s_is_in_tool) {
            kit_system_exit_impl();
        } else {
            // Libera o slideshow da Home antes de relançar a última Tool (mais
            // RAM para o tool_init). Sem última Tool ou falha ao abrir:
            // kit_launcher_go_home reconstrói a Home.
            kit_launcher_release_home_deck();
            if (kit_tool_manager_start_last() != KIT_OK) {
                kit_launcher_go_home();
            }
        }
    }
    s_boot_prev = lvl;
}

kit_err_t kit_runtime_init(void)
{
    ESP_LOGI(TAG, "Iniciando subsistemas do KIT Runtime...");

    // 0. Carrega as configurações persistidas (brilho, repouso, desligamento).
    kit_config_init();

    // 1. Inicializa PMIC e Alimentação
    kit_err_t err = kit_power_init();
    if (err != KIT_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar subsistema de energia");
    }

    // 2. Inicializa Armazenamento LittleFS
    err = kit_storage_init();
    if (err != KIT_OK) {
        ESP_LOGE(TAG, "Falha ao montar LittleFS (/tools)");
    }

    // 2b. Cartão microSD (opcional): armazenamento secundário para pacotes .kit
    //     e assets multimídia pesados. A ausência de cartão não é um erro.
    if (kit_storage_sd_mount() == KIT_OK) {
        ESP_LOGI(TAG, "Cartão microSD disponível em %s", KIT_SD_MOUNT_POINT);
    }

    // 3. Inicializa Gerador de Números Aleatórios
    err = kit_random_init();
    if (err != KIT_OK) {
        ESP_LOGW(TAG, "Falha ao semear TRNG");
    }

    // 4. Inicializa RTC
    err = kit_time_init();
    if (err != KIT_OK) {
        ESP_LOGW(TAG, "Falha ao inicializar RTC");
    }

    // 5. Inicializa Áudio
    err = kit_audio_init();
    if (err != KIT_OK) {
        ESP_LOGW(TAG, "Falha ao inicializar áudio");
    }

    // 5b. Inicializa IMU QMI8658 (gesto de chacoalhar). Não-fatal.
    if (kit_imu_init() != KIT_OK) {
        ESP_LOGW(TAG, "IMU indisponível — chacoalhar desativado");
    }

    // 6. Inicializa Display AMOLED e LVGL v9
    err = kit_display_init();
    if (err != KIT_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar Display AMOLED");
        return err;
    }
    // Reaplica o brilho salvo (o init do painel liga no máximo).
    kit_display_set_brightness_impl(kit_config_get_brightness());

    // 7. Inicializa Touch CST820
    err = kit_input_init();
    if (err != KIT_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar Touch CST820");
    }

    // 8. Inicializa Tool Manager
    err = kit_tool_manager_init();
    if (err != KIT_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar Tool Manager");
    }

    // 8b. Carregador dinâmico de Tools (.so do cartão microSD). Registra a
    //     tabela de símbolos LVGL usada pelos objetos compartilhados.
    if (kit_tool_loader_init() != KIT_OK) {
        ESP_LOGW(TAG, "Carregador de Tools externas indisponível");
    }

    // 8c. Conectividade Wi-Fi (offline-first): só carrega mutex + lista de redes
    //     do NVS. A pilha (esp_wifi/lwip, ~45 KB) e o rádio só sobem quando o
    //     usuário liga o Wi-Fi. A religação automática às redes salvas é adiada
    //     para depois do primeiro frame (ver kit_runtime_run) — subir a pilha
    //     durante o bring-up disputa RAM interna com o DMA do display.
    if (kit_network_init() != KIT_OK) {
        ESP_LOGW(TAG, "Subsistema Wi-Fi indisponível");
    }

    // 8d. Catálogo de Tools no dispositivo (só cria a task de rede, ociosa até
    //     a UI pedir um refresh).
    if (kit_catalog_init() != KIT_OK) {
        ESP_LOGW(TAG, "Catálogo de Tools indisponível");
    }

    // 9. Inicializa Launcher UI
    err = kit_launcher_init();
    if (err != KIT_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar Launcher UI");
        return err;
    }

    // 10. Botão BOOT (GPIO0) para navegação
    gpio_config_t boot_cfg = {
        .pin_bit_mask = (1ULL << KIT_BOOT_BTN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&boot_cfg);

    ESP_LOGI(TAG, "Todos os subsistemas inicializados com sucesso.");
    return KIT_OK;
}

void kit_runtime_run(void)
{
    int64_t last_btn_us = 0;
    int64_t last_imu_us = 0;
    int64_t last_inact_us = 0;
    int64_t last_wake_us = 0;
    bool    wifi_autostart_done = false;
    while (1) {
        // Incrementa o tempo do LVGL e processa tarefas gráficas
        uint32_t delay_ms = kit_display_process();

        int64_t now = esp_timer_get_time();

        // Religação automática do Wi-Fi às redes salvas — adiada ~3 s para o
        // display já estar renderizando quando a pilha de rede sobe.
        if (!wifi_autostart_done && now >= 3000000) {
            wifi_autostart_done = true;
            uint8_t wifi_en = 1;
            kit_config_get_u8("wifi_en", &wifi_en, 1);
            if (wifi_en && kit_network_saved_count() > 0) {
                ESP_LOGI(TAG, "Wi-Fi: religando às redes memorizadas");
                kit_network_start();
            }
        }

        // Confere os botões físicos ~a cada 200 ms (o AXP2101 mantém o evento
        // latcheado; poll raro evita disputa no barramento I2C compartilhado).
        if (now - last_btn_us >= 200000) {
            last_btn_us = now;
            poll_system_buttons();
        }

        // Com a tela em repouso, confere ~a cada 80 ms se há um toque para
        // acordar; segue rodando logo após acordar até o dedo soltar.
        if ((!s_screen_on || s_wake_pending) && now - last_wake_us >= 80000) {
            last_wake_us = now;
            poll_wake_touch();
        }

        // Repouso da tela / desligamento automático ~a cada 1 s.
        if (now - last_inact_us >= 1000000) {
            last_inact_us = now;
            poll_inactivity();
        }

        // Chacoalhar (IMU): dentro de qualquer Tool, ~a cada 60 ms.
        //  - Tool built-in: dispara a "ação principal" (mesma do botão PWR).
        //  - Tool externa (.so): despacha o callback que a Tool registrou
        //    via kit_api.imu->register_shake_callback (ela não tem
        //    primary_action). kit_imu_dispatch_shake() é no-op sem callback.
        if (s_screen_on && s_is_in_tool && now - last_imu_us >= 60000) {
            last_imu_us = now;
            if (kit_imu_poll_shake()) {
                note_activity();
                if (s_tool_primary_action) s_tool_primary_action();
                kit_imu_dispatch_shake();
            }
        }

        uint32_t sleep_ms = delay_ms > 0 ? delay_ms : 5;
        // Com a tela apagada o LVGL não tem nada a fazer e devolve um intervalo
        // longo; limita para manter o polling de toque para acordar responsivo.
        if ((!s_screen_on || s_wake_pending) && sleep_ms > 40) {
            sleep_ms = 40;
        }
        vTaskDelay(pdMS_TO_TICKS(sleep_ms));
    }
}

bool kit_runtime_is_in_tool(void)
{
    return s_is_in_tool;
}

void kit_runtime_set_in_tool(bool in_tool)
{
    s_is_in_tool = in_tool;
    if (!in_tool) {
        s_tool_primary_action = NULL;
        kit_imu_clear_shake_callback();   // não deixa callback órfão de Tool externa
    }
}

void kit_runtime_set_tool_primary_action(void (*action)(void))
{
    s_tool_primary_action = action;
}

// Implementações do módulo System da API
kit_err_t kit_system_get_info_impl(kit_system_info_t *info)
{
    if (!info) return KIT_ERR_INVALID_ARG;
    info->battery_percentage = kit_power_get_battery_percentage();
    info->is_charging = kit_power_is_charging();
    info->free_psram_bytes = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    info->free_flash_bytes = kit_storage_get_free_bytes();
    snprintf(info->device_id, sizeof(info->device_id), "%s", kit_power_get_device_id());
    snprintf(info->runtime_version, sizeof(info->runtime_version), "%s", KIT_VERSION_STRING);
    return KIT_OK;
}

void kit_system_exit_impl(void)
{
    ESP_LOGI(TAG, "Tool solicitou saída para o Launcher.");

    // A tela do Launcher precisa voltar a ser a tela ATIVA do LVGL *antes* de
    // destruir a Tool: kit_tool_manager_stop_current() deleta a tela da Tool, e
    // deletar a tela ativa deixa o lv_display com um ponteiro solto que o
    // lv_screen_load() seguinte ainda toca (use-after-free) — de vez em quando
    // isso corrompia a Home e ela travava com só o cabeçalho vivo.
    //
    // Para Tools .so: marca a tela da Tool ANTES da troca, para o tool_loader
    // liberá-la antes do dlclose (nem toda Tool deleta a própria tela no
    // tool_destroy; depois do dlclose ela vira lixo e trava o LVGL).
    if (kit_tool_loader_is_active()) {
        kit_tool_loader_mark_tool_screen(lv_screen_active());
    }

    kit_launcher_go_home();
    kit_tool_manager_stop_current();
    kit_runtime_set_in_tool(false);   // limpa primary_action + callback de shake órfão
}
