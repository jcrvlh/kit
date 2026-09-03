#include "kit_api.h"
#include <string.h>

// Forward declarations das implementações nos módulos de subsistema
extern lv_obj_t *kit_display_get_screen_impl(void);
extern kit_err_t kit_display_refresh_impl(void);
extern kit_err_t kit_display_set_brightness_impl(uint8_t percentage);
extern uint8_t   kit_display_get_brightness_impl(void);

extern kit_err_t kit_input_register_callback_impl(kit_input_callback_t cb, void *user_data);

extern kit_err_t kit_storage_set_str_impl(const char *key, const char *value);
extern kit_err_t kit_storage_get_str_impl(const char *key, char *buffer, size_t max_len);
extern kit_err_t kit_storage_set_i32_impl(const char *key, int32_t value);
extern kit_err_t kit_storage_get_i32_impl(const char *key, int32_t *out_value);
extern FILE     *kit_storage_open_file_impl(const char *filename, const char *mode);

extern uint32_t  kit_random_u32_impl(void);
extern int32_t   kit_random_range_impl(int32_t min, int32_t max);
extern kit_err_t kit_random_bytes_impl(uint8_t *buffer, size_t length);
extern float     kit_random_float_impl(void);

extern uint64_t  kit_time_get_millis_impl(void);
extern kit_err_t kit_time_get_datetime_impl(kit_datetime_t *dt);
extern void      kit_time_delay_ms_impl(uint32_t ms);

extern kit_err_t kit_audio_beep_impl(uint16_t freq_hz, uint16_t duration_ms);
extern kit_err_t kit_audio_set_volume_impl(uint8_t percentage);
extern kit_err_t kit_audio_sfx_impl(kit_sfx_t sfx);
extern kit_err_t kit_audio_fuse_impl(int16_t tension);

extern kit_err_t kit_power_keep_awake_impl(bool enable);

extern kit_err_t kit_system_get_info_impl(kit_system_info_t *info);
extern void      kit_system_exit_impl(void);

extern kit_err_t kit_imu_register_shake_callback_impl(kit_shake_callback_t cb, void *user_data);
extern kit_err_t kit_imu_register_tilt_callback_impl(kit_tilt_callback_t cb, void *user_data);

// Definição das tabelas estáticas de APIs
static const kit_display_api_t s_display_api = {
    .get_screen = kit_display_get_screen_impl,
    .refresh = kit_display_refresh_impl,
    .set_brightness = kit_display_set_brightness_impl,
    .get_brightness = kit_display_get_brightness_impl,
};

static const kit_input_api_t s_input_api = {
    .register_callback = kit_input_register_callback_impl,
};

static const kit_storage_api_t s_storage_api = {
    .set_str = kit_storage_set_str_impl,
    .get_str = kit_storage_get_str_impl,
    .set_i32 = kit_storage_set_i32_impl,
    .get_i32 = kit_storage_get_i32_impl,
    .open_file = kit_storage_open_file_impl,
};

static const kit_random_api_t s_random_api = {
    .u32 = kit_random_u32_impl,
    .range = kit_random_range_impl,
    .bytes = kit_random_bytes_impl,
    .get_float = kit_random_float_impl,
};

static const kit_time_api_t s_time_api = {
    .get_millis = kit_time_get_millis_impl,
    .get_datetime = kit_time_get_datetime_impl,
    .delay_ms = kit_time_delay_ms_impl,
};

static const kit_audio_api_t s_audio_api = {
    .beep = kit_audio_beep_impl,
    .set_volume = kit_audio_set_volume_impl,
    .sfx = kit_audio_sfx_impl,
    .fuse = kit_audio_fuse_impl,
};

static const kit_power_api_t s_power_api = {
    .keep_awake = kit_power_keep_awake_impl,
};

static const kit_system_api_t s_system_api = {
    .get_info = kit_system_get_info_impl,
    .exit = kit_system_exit_impl,
};

static const kit_imu_api_t s_imu_api = {
    .register_shake_callback = kit_imu_register_shake_callback_impl,
    .register_tilt_callback = kit_imu_register_tilt_callback_impl,
};

static const kit_api_table_t s_master_api_table = {
    .display = &s_display_api,
    .input   = &s_input_api,
    .storage = &s_storage_api,
    .random  = &s_random_api,
    .time    = &s_time_api,
    .audio   = &s_audio_api,
    .power   = &s_power_api,
    .system  = &s_system_api,
    .imu     = &s_imu_api,
};

const kit_api_table_t *kit_api_get_table(void)
{
    return &s_master_api_table;
}
