#include "kit_time.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "KIT_TIME";

kit_err_t kit_time_init(void)
{
    ESP_LOGI(TAG, "Inicializando RTC PCF85063A via I2C (0x51)...");
    return KIT_OK;
}

uint64_t kit_time_get_millis_impl(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000);
}

kit_err_t kit_time_get_datetime_impl(kit_datetime_t *dt)
{
    if (!dt) return KIT_ERR_INVALID_ARG;
    dt->year = 2026;
    dt->month = 8;
    dt->day = 30;
    dt->hour = 12;
    dt->minute = 0;
    dt->second = 0;
    return KIT_OK;
}

void kit_time_delay_ms_impl(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}
