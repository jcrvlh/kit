#include "kit_storage.h"
#include "esp_log.h"
#include "esp_littlefs.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "KIT_STORAGE";
static const char *TOOL_NVS_NS = "kit_tools_data";

kit_err_t kit_storage_init(void)
{
    ESP_LOGI(TAG, "Montando partição LittleFS em /tools...");

    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/tools",
        .partition_label = "tools",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao registrar LittleFS: %s", esp_err_to_name(ret));
        return KIT_ERR_STORAGE;
    }

    size_t total = 0, used = 0;
    esp_littlefs_info("tools", &total, &used);
    ESP_LOGI(TAG, "LittleFS montado: Total: %d KB, Usado: %d KB", (int)(total / 1024), (int)(used / 1024));

    return KIT_OK;
}

uint32_t kit_storage_get_free_bytes(void)
{
    size_t total = 0, used = 0;
    if (esp_littlefs_info("tools", &total, &used) == ESP_OK) {
        return (uint32_t)(total - used);
    }
    return 0;
}

kit_err_t kit_storage_set_str_impl(const char *key, const char *value)
{
    nvs_handle_t h;
    if (nvs_open(TOOL_NVS_NS, NVS_READWRITE, &h) != ESP_OK) return KIT_ERR_STORAGE;
    nvs_set_str(h, key, value);
    nvs_commit(h);
    nvs_close(h);
    return KIT_OK;
}

kit_err_t kit_storage_get_str_impl(const char *key, char *buffer, size_t max_len)
{
    nvs_handle_t h;
    if (nvs_open(TOOL_NVS_NS, NVS_READONLY, &h) != ESP_OK) return KIT_ERR_NOT_FOUND;
    esp_err_t err = nvs_get_str(h, key, buffer, &max_len);
    nvs_close(h);
    return (err == ESP_OK) ? KIT_OK : KIT_ERR_NOT_FOUND;
}

kit_err_t kit_storage_set_i32_impl(const char *key, int32_t value)
{
    nvs_handle_t h;
    if (nvs_open(TOOL_NVS_NS, NVS_READWRITE, &h) != ESP_OK) return KIT_ERR_STORAGE;
    nvs_set_i32(h, key, value);
    nvs_commit(h);
    nvs_close(h);
    return KIT_OK;
}

kit_err_t kit_storage_get_i32_impl(const char *key, int32_t *out_value)
{
    nvs_handle_t h;
    if (nvs_open(TOOL_NVS_NS, NVS_READONLY, &h) != ESP_OK) return KIT_ERR_NOT_FOUND;
    esp_err_t err = nvs_get_i32(h, key, out_value);
    nvs_close(h);
    return (err == ESP_OK) ? KIT_OK : KIT_ERR_NOT_FOUND;
}

FILE *kit_storage_open_file_impl(const char *filename, const char *mode)
{
    char fullpath[128];
    snprintf(fullpath, sizeof(fullpath), "/tools/%s", filename);
    return fopen(fullpath, mode);
}
