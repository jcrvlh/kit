#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_ota_ops.h"
#include "kit_runtime.h"
#include "kit_comms.h"

static const char *TAG = "KIT_MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Iniciando KIT Core — Plataforma Modular");
    ESP_LOGI(TAG, "========================================");

    // 1. Inicializa NVS (Non-Volatile Storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Inicializa o KIT Runtime e todos os subsistemas de hardware
    kit_err_t err = kit_runtime_init();
    if (err != KIT_OK) {
        ESP_LOGE(TAG, "Falha crítica ao inicializar o KIT Runtime: %d", err);
        return;
    }

    // 2.5 Inicializa a camada de conectividade serial (Web Installer OTA)
    kit_comms_init();

    // 3. Validação do OTA Health Check
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGI(TAG, "Novo firmware validado com sucesso. Cancelando rollback.");
            esp_ota_mark_app_valid_cancel_rollback();
        }
    }

    // 4. Executa o loop principal do Runtime
    ESP_LOGI(TAG, "KIT Core operacional. Entrando no loop de eventos.");
    kit_runtime_run();
}
