#include "kit_config.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"

static const char *TAG = "KIT_CONFIG";
static const char *NVS_NAMESPACE = "kit_sys";

// Chaves NVS
#define KEY_BRIGHTNESS   "brightness"
#define KEY_SCR_SLEEP    "scr_sleep_s"
#define KEY_PWR_OFF      "pwr_off_s"
#define KEY_SOUND        "sound_en"

// Padrões
#define DEF_BRIGHTNESS   80
#define DEF_SCR_SLEEP    120     // 2 min
#define DEF_PWR_OFF      0       // nunca
#define DEF_SOUND        1       // ligado

static struct {
    uint8_t  brightness;
    uint32_t screen_sleep_s;
    uint32_t auto_poweroff_s;
    uint8_t  sound_enabled;
} s_cache = {
    .brightness      = DEF_BRIGHTNESS,
    .screen_sleep_s  = DEF_SCR_SLEEP,
    .auto_poweroff_s = DEF_PWR_OFF,
    .sound_enabled   = DEF_SOUND,
};

kit_err_t kit_config_get_u8(const char *key, uint8_t *out_val, uint8_t default_val)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) {
        *out_val = default_val;
        return KIT_OK;
    }
    esp_err_t err = nvs_get_u8(h, key, out_val);
    nvs_close(h);
    if (err != ESP_OK) {
        *out_val = default_val;
    }
    return KIT_OK;
}

kit_err_t kit_config_set_u8(const char *key, uint8_t val)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) {
        return KIT_ERR_STORAGE;
    }
    nvs_set_u8(h, key, val);
    nvs_commit(h);
    nvs_close(h);
    return KIT_OK;
}

kit_err_t kit_config_get_u32(const char *key, uint32_t *out_val, uint32_t default_val)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) {
        *out_val = default_val;
        return KIT_OK;
    }
    esp_err_t err = nvs_get_u32(h, key, out_val);
    nvs_close(h);
    if (err != ESP_OK) {
        *out_val = default_val;
    }
    return KIT_OK;
}

kit_err_t kit_config_set_u32(const char *key, uint32_t val)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) {
        return KIT_ERR_STORAGE;
    }
    nvs_set_u32(h, key, val);
    nvs_commit(h);
    nvs_close(h);
    return KIT_OK;
}

kit_err_t kit_config_init(void)
{
    uint8_t b;
    kit_config_get_u8(KEY_BRIGHTNESS, &b, DEF_BRIGHTNESS);
    if (b < 10) b = 10;
    if (b > 100) b = 100;
    s_cache.brightness = b;

    kit_config_get_u32(KEY_SCR_SLEEP, &s_cache.screen_sleep_s, DEF_SCR_SLEEP);
    kit_config_get_u32(KEY_PWR_OFF,   &s_cache.auto_poweroff_s, DEF_PWR_OFF);
    kit_config_get_u8(KEY_SOUND, &s_cache.sound_enabled, DEF_SOUND);

    ESP_LOGI(TAG, "Config: brilho=%d%%, repouso=%lus, desliga=%lus, som=%s",
             s_cache.brightness,
             (unsigned long)s_cache.screen_sleep_s,
             (unsigned long)s_cache.auto_poweroff_s,
             s_cache.sound_enabled ? "on" : "off");
    return KIT_OK;
}

uint8_t kit_config_get_brightness(void) { return s_cache.brightness; }

void kit_config_set_brightness(uint8_t percent)
{
    if (percent < 10) percent = 10;
    if (percent > 100) percent = 100;
    if (percent == s_cache.brightness) return;
    s_cache.brightness = percent;
    kit_config_set_u8(KEY_BRIGHTNESS, percent);
}

uint32_t kit_config_get_screen_sleep_s(void) { return s_cache.screen_sleep_s; }

void kit_config_set_screen_sleep_s(uint32_t seconds)
{
    if (seconds == s_cache.screen_sleep_s) return;
    s_cache.screen_sleep_s = seconds;
    kit_config_set_u32(KEY_SCR_SLEEP, seconds);
}

uint32_t kit_config_get_auto_poweroff_s(void) { return s_cache.auto_poweroff_s; }

void kit_config_set_auto_poweroff_s(uint32_t seconds)
{
    if (seconds == s_cache.auto_poweroff_s) return;
    s_cache.auto_poweroff_s = seconds;
    kit_config_set_u32(KEY_PWR_OFF, seconds);
}

bool kit_config_get_sound_enabled(void) { return s_cache.sound_enabled != 0; }

void kit_config_set_sound_enabled(bool enabled)
{
    uint8_t v = enabled ? 1 : 0;
    if (v == s_cache.sound_enabled) return;
    s_cache.sound_enabled = v;
    kit_config_set_u8(KEY_SOUND, v);
}
