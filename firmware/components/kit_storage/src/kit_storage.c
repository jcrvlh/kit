#include "kit_storage.h"
#include "esp_log.h"
#include "esp_littlefs.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/gpio.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include <sys/stat.h>
#include <errno.h>

static const char *TAG = "KIT_STORAGE";
static const char *TOOL_NVS_NS = "kit_tools_data";

// Pinos do slot microSD (SDMMC, modo 1-bit) — ver docs/hardware/board.md.
#define KIT_SD_PIN_CLK 2
#define KIT_SD_PIN_CMD 1
#define KIT_SD_PIN_D0  3

static sdmmc_card_t *s_sd_card = NULL;

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

kit_err_t kit_storage_get_info(uint32_t *total_bytes, uint32_t *free_bytes)
{
    size_t total = 0, used = 0;
    if (esp_littlefs_info("tools", &total, &used) != ESP_OK) {
        return KIT_ERR_STORAGE;
    }
    if (total_bytes) *total_bytes = (uint32_t)total;
    if (free_bytes)  *free_bytes  = (uint32_t)(total - used);
    return KIT_OK;
}

kit_err_t kit_storage_sd_mount(void)
{
    if (s_sd_card) {
        return KIT_OK;
    }

    ESP_LOGI(TAG, "Procurando cartão microSD (SDMMC 1-bit) em %s...", KIT_SD_MOUNT_POINT);

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.flags = SDMMC_HOST_FLAG_1BIT;
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;

    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 1;
    slot.clk = KIT_SD_PIN_CLK;
    slot.cmd = KIT_SD_PIN_CMD;
    slot.d0  = KIT_SD_PIN_D0;
    slot.d1 = GPIO_NUM_NC;
    slot.d2 = GPIO_NUM_NC;
    slot.d3 = GPIO_NUM_NC;
    slot.cd = SDMMC_SLOT_NO_CD;
    slot.wp = SDMMC_SLOT_NO_WP;
    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    esp_err_t ret = esp_vfs_fat_sdmmc_mount(KIT_SD_MOUNT_POINT, &host, &slot,
                                            &mount_cfg, &s_sd_card);
    if (ret != ESP_OK) {
        s_sd_card = NULL;
        if (ret == ESP_ERR_TIMEOUT || ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGI(TAG, "Nenhum cartão microSD detectado.");
            return KIT_ERR_NOT_FOUND;
        }
        ESP_LOGW(TAG, "Falha ao montar o cartão microSD: %s", esp_err_to_name(ret));
        return KIT_ERR_STORAGE;
    }

    uint64_t total = 0, freeb = 0;
    kit_storage_sd_info(&total, &freeb);
    ESP_LOGI(TAG, "microSD montado em %s: %llu MB livres de %llu MB",
             KIT_SD_MOUNT_POINT, freeb / (1024 * 1024), total / (1024 * 1024));
    return KIT_OK;
}

void kit_storage_sd_unmount(void)
{
    if (!s_sd_card) {
        return;
    }
    esp_vfs_fat_sdcard_unmount(KIT_SD_MOUNT_POINT, s_sd_card);
    s_sd_card = NULL;
    ESP_LOGI(TAG, "Cartão microSD desmontado.");
}

bool kit_storage_sd_is_mounted(void)
{
    return s_sd_card != NULL;
}

kit_err_t kit_storage_sd_info(uint64_t *total_bytes, uint64_t *free_bytes)
{
    if (!s_sd_card) {
        return KIT_ERR_NOT_FOUND;
    }
    if (esp_vfs_fat_info(KIT_SD_MOUNT_POINT, total_bytes, free_bytes) != ESP_OK) {
        return KIT_ERR_STORAGE;
    }
    return KIT_OK;
}

kit_err_t kit_storage_sd_format(void)
{
    if (!s_sd_card) {
        return KIT_ERR_NOT_FOUND;
    }

    ESP_LOGW(TAG, "Formatando o cartão microSD (%s) — TODOS os dados serão apagados.",
             KIT_SD_MOUNT_POINT);
    esp_err_t ret = esp_vfs_fat_sdcard_format(KIT_SD_MOUNT_POINT, s_sd_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao formatar o cartão microSD: %s", esp_err_to_name(ret));
        return KIT_ERR_STORAGE;
    }

    // Deixa o cartão "pronto pra uso": a estrutura de pastas que o KIT espera.
    if (mkdir(KIT_SD_MOUNT_POINT "/tools", 0775) != 0 && errno != EEXIST) {
        ESP_LOGW(TAG, "Cartão formatado, mas não consegui criar %s/tools (errno=%d)",
                 KIT_SD_MOUNT_POINT, errno);
        return KIT_OK;   // formatou; só a pasta que faltou — não é fatal
    }

    ESP_LOGI(TAG, "Cartão microSD formatado e pronto (%s/tools/ criado).", KIT_SD_MOUNT_POINT);
    return KIT_OK;
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
