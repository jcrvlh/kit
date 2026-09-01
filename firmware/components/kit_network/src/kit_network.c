#include "kit_network.h"
#include "esp_log.h"

static const char *TAG = "KIT_NETWORK";
static bool s_connected = false;

kit_err_t kit_network_init(void)
{
    ESP_LOGI(TAG, "Inicializando subsistema Wi-Fi 802.11 b/g/n...");
    return KIT_OK;
}

bool kit_network_is_connected(void)
{
    return s_connected;
}
