#include "kit_diagnostics.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"

static const char *TAG = "KIT_DIAG";

void kit_diagnostics_print_summary(void)
{
    ESP_LOGI(TAG, "=== DIAGNÓSTICO DO SISTEMA ===");
    ESP_LOGI(TAG, "Heap Livre (DRAM): %lu bytes", (unsigned long)esp_get_free_heap_size());
    ESP_LOGI(TAG, "PSRAM Livre: %lu bytes", (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    ESP_LOGI(TAG, "Menor Heap Livre Registrado: %lu bytes", (unsigned long)esp_get_minimum_free_heap_size());
    ESP_LOGI(TAG, "=============================");
}
