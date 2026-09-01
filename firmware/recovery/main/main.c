#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "KIT_RECOVERY";

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  KIT RECOVERY MODE (Imagem de Fábrica) ");
    ESP_LOGI(TAG, "========================================");

    nvs_flash_init();

    ESP_LOGI(TAG, "Dispositivo em modo de recuperação.");
    ESP_LOGI(TAG, "Aguardando conexão USB ou rede Wi-Fi para restauração...");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
