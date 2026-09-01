#include "kit_random.h"
#include "esp_random.h"
#include "esp_log.h"

static const char *TAG = "KIT_RANDOM";

kit_err_t kit_random_init(void)
{
    ESP_LOGI(TAG, "Motor de aleatoriedade alimentado com TRNG de hardware.");
    return KIT_OK;
}

uint32_t kit_random_u32_impl(void)
{
    return esp_random();
}

int32_t kit_random_range_impl(int32_t min, int32_t max)
{
    if (min >= max) return min;
    uint32_t range = (uint32_t)(max - min + 1);
    return min + (int32_t)(esp_random() % range);
}

kit_err_t kit_random_bytes_impl(uint8_t *buffer, size_t length)
{
    if (!buffer) return KIT_ERR_INVALID_ARG;
    esp_fill_random(buffer, length);
    return KIT_OK;
}

float kit_random_float_impl(void)
{
    return (float)esp_random() / (float)UINT32_MAX;
}
