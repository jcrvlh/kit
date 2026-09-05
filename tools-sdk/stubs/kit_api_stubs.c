/**
 * @file kit_api_stubs.c
 * @brief Stubs de linkagem para compilação offline de Tools do KIT.
 *
 * Implementações que simulam o comportamento do KIT Runtime em desktop,
 * permitindo compilar, linkar e testar a lógica básica de uma Tool sem
 * o hardware real ou o ESP-IDF.
 *
 * - Display: retorna NULL (sem LVGL).
 * - Storage: armazena em memória (HashMap simples, perde dados ao sair).
 * - Random: usa rand() do C stdlib (seeded com time()).
 * - Audio: no-op, imprime no stdout.
 * - Time: usa clock() POSIX.
 * - System: exit(0).
 * - IMU: armazena callback mas nunca dispara.
 *
 * @copyright GNU General Public License v3.0 (GPL-3.0)
 */

#include "kit_tool_api.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

/* -----------------------------------------------------------------------
 * Storage Stub — HashMap em memória (lista linear simples)
 * ----------------------------------------------------------------------- */

#define STUB_STORAGE_MAX_ENTRIES 64
#define STUB_STORAGE_KEY_LEN    16
#define STUB_STORAGE_VAL_LEN    512

typedef struct {
    char    key[STUB_STORAGE_KEY_LEN];
    char    str_val[STUB_STORAGE_VAL_LEN];
    int32_t i32_val;
    bool    has_str;
    bool    has_i32;
    bool    used;
} stub_storage_entry_t;

static stub_storage_entry_t s_store[STUB_STORAGE_MAX_ENTRIES] = {0};

static stub_storage_entry_t *storage_find(const char *key)
{
    for (int i = 0; i < STUB_STORAGE_MAX_ENTRIES; i++) {
        if (s_store[i].used && strcmp(s_store[i].key, key) == 0)
            return &s_store[i];
    }
    return NULL;
}

static stub_storage_entry_t *storage_alloc(const char *key)
{
    stub_storage_entry_t *e = storage_find(key);
    if (e) return e;
    for (int i = 0; i < STUB_STORAGE_MAX_ENTRIES; i++) {
        if (!s_store[i].used) {
            s_store[i].used = true;
            strncpy(s_store[i].key, key, STUB_STORAGE_KEY_LEN - 1);
            s_store[i].key[STUB_STORAGE_KEY_LEN - 1] = '\0';
            return &s_store[i];
        }
    }
    return NULL;
}

/* -----------------------------------------------------------------------
 * Display API Stubs
 * ----------------------------------------------------------------------- */

static lv_obj_t *stub_display_get_screen(void)
{
    return NULL;  /* Sem LVGL no modo stub */
}

static kit_err_t stub_display_refresh(void)
{
    return KIT_OK;
}

static uint8_t s_brightness = 80;

static kit_err_t stub_display_set_brightness(uint8_t percentage)
{
    s_brightness = percentage;
    return KIT_OK;
}

static uint8_t stub_display_get_brightness(void)
{
    return s_brightness;
}

/* -----------------------------------------------------------------------
 * Input API Stubs
 * ----------------------------------------------------------------------- */

static kit_input_callback_t s_input_cb = NULL;
static void *s_input_ud = NULL;

static kit_err_t stub_input_register_callback(kit_input_callback_t cb, void *user_data)
{
    s_input_cb = cb;
    s_input_ud = user_data;
    return KIT_OK;
}

/* -----------------------------------------------------------------------
 * Storage API Stubs
 * ----------------------------------------------------------------------- */

static kit_err_t stub_storage_set_str(const char *key, const char *value)
{
    stub_storage_entry_t *e = storage_alloc(key);
    if (!e) return KIT_ERR_NO_MEM;
    strncpy(e->str_val, value, STUB_STORAGE_VAL_LEN - 1);
    e->str_val[STUB_STORAGE_VAL_LEN - 1] = '\0';
    e->has_str = true;
    return KIT_OK;
}

static kit_err_t stub_storage_get_str(const char *key, char *buffer, size_t max_len)
{
    stub_storage_entry_t *e = storage_find(key);
    if (!e || !e->has_str) {
        if (buffer && max_len > 0) buffer[0] = '\0';
        return KIT_ERR_NOT_FOUND;
    }
    strncpy(buffer, e->str_val, max_len - 1);
    buffer[max_len - 1] = '\0';
    return KIT_OK;
}

static kit_err_t stub_storage_set_i32(const char *key, int32_t value)
{
    stub_storage_entry_t *e = storage_alloc(key);
    if (!e) return KIT_ERR_NO_MEM;
    e->i32_val = value;
    e->has_i32 = true;
    return KIT_OK;
}

static kit_err_t stub_storage_get_i32(const char *key, int32_t *out_value)
{
    stub_storage_entry_t *e = storage_find(key);
    if (!e || !e->has_i32) return KIT_ERR_NOT_FOUND;
    if (out_value) *out_value = e->i32_val;
    return KIT_OK;
}

static FILE *stub_storage_open_file(const char *filename, const char *mode)
{
    /* Abre no diretório de trabalho atual */
    return fopen(filename, mode);
}

/* -----------------------------------------------------------------------
 * Random API Stubs
 * ----------------------------------------------------------------------- */

static bool s_rand_seeded = false;

static void ensure_seeded(void)
{
    if (!s_rand_seeded) {
        srand((unsigned int)time(NULL));
        s_rand_seeded = true;
    }
}

static uint32_t stub_random_u32(void)
{
    ensure_seeded();
    return ((uint32_t)rand() << 16) ^ (uint32_t)rand();
}

static int32_t stub_random_range(int32_t min, int32_t max)
{
    ensure_seeded();
    if (min >= max) return min;
    return min + (int32_t)(rand() % (max - min + 1));
}

static kit_err_t stub_random_bytes(uint8_t *buffer, size_t length)
{
    ensure_seeded();
    for (size_t i = 0; i < length; i++)
        buffer[i] = (uint8_t)(rand() & 0xFF);
    return KIT_OK;
}

static float stub_random_float(void)
{
    ensure_seeded();
    return (float)rand() / (float)RAND_MAX;
}

/* -----------------------------------------------------------------------
 * Time API Stubs
 * ----------------------------------------------------------------------- */

static uint64_t stub_time_get_millis(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static kit_err_t stub_time_get_datetime(kit_datetime_t *dt)
{
    if (!dt) return KIT_ERR_INVALID_ARG;
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    dt->year   = (uint16_t)(tm->tm_year + 1900);
    dt->month  = (uint8_t)(tm->tm_mon + 1);
    dt->day    = (uint8_t)tm->tm_mday;
    dt->hour   = (uint8_t)tm->tm_hour;
    dt->minute = (uint8_t)tm->tm_min;
    dt->second = (uint8_t)tm->tm_sec;
    return KIT_OK;
}

static void stub_time_delay_ms(uint32_t ms)
{
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

/* -----------------------------------------------------------------------
 * Audio API Stubs
 * ----------------------------------------------------------------------- */

static kit_err_t stub_audio_beep(uint16_t freq_hz, uint16_t duration_ms)
{
    printf("[STUB AUDIO] beep(%u Hz, %u ms)\n", freq_hz, duration_ms);
    return KIT_OK;
}

static kit_err_t stub_audio_set_volume(uint8_t percentage)
{
    printf("[STUB AUDIO] set_volume(%u%%)\n", percentage);
    return KIT_OK;
}

static kit_err_t stub_audio_sfx(kit_sfx_t sfx)
{
    printf("[STUB AUDIO] sfx(%d)\n", (int)sfx);
    return KIT_OK;
}

static kit_err_t stub_audio_fuse(int16_t tension)
{
    printf("[STUB AUDIO] fuse(%d)\n", (int)tension);
    return KIT_OK;
}

/* -----------------------------------------------------------------------
 * Power API Stubs
 * ----------------------------------------------------------------------- */

static bool s_keep_awake = false;

static kit_err_t stub_power_keep_awake(bool enable)
{
    s_keep_awake = enable;
    return KIT_OK;
}

/* -----------------------------------------------------------------------
 * System API Stubs
 * ----------------------------------------------------------------------- */

static kit_err_t stub_system_get_info(kit_system_info_t *info)
{
    if (!info) return KIT_ERR_INVALID_ARG;
    memset(info, 0, sizeof(*info));
    info->battery_percentage = 100;
    info->is_charging = false;
    info->free_psram_bytes = 4 * 1024 * 1024;
    info->free_flash_bytes = 6 * 1024 * 1024;
    strncpy(info->device_id, "KIT-STUB", sizeof(info->device_id));
    strncpy(info->runtime_version, "0.1.0", sizeof(info->runtime_version));
    return KIT_OK;
}

static void stub_system_exit(void)
{
    printf("[STUB SYSTEM] exit() chamado — encerrando processo.\n");
    exit(0);
}

/* -----------------------------------------------------------------------
 * IMU API Stubs
 * ----------------------------------------------------------------------- */

static kit_shake_callback_t s_shake_cb = NULL;
static void *s_shake_ud = NULL;

static kit_err_t stub_imu_register_shake_callback(kit_shake_callback_t cb, void *user_data)
{
    s_shake_cb = cb;
    s_shake_ud = user_data;
    printf("[STUB IMU] shake callback %s.\n", cb ? "registrado" : "removido");
    return KIT_OK;
}

static kit_tilt_callback_t s_tilt_cb = NULL;
static void *s_tilt_ud = NULL;

static kit_err_t stub_imu_register_tilt_callback(kit_tilt_callback_t cb, void *user_data)
{
    s_tilt_cb = cb;
    s_tilt_ud = user_data;
    printf("[STUB IMU] tilt callback %s.\n", cb ? "registrado" : "removido");
    return KIT_OK;
}

/* Giroscópio: no desktop não há sensor — o ângulo fica sempre em zero e o
 * rate baixo (como um aparelho parado na mesa). Suficiente pra exercitar a
 * lógica da Tool no build nativo. */
static bool s_stub_gyro_on = false;

static kit_err_t stub_imu_gyro_start(void)
{
    s_stub_gyro_on = true;
    printf("[STUB IMU] giroscópio ligado (stub — sempre 0).\n");
    return KIT_OK;
}

static kit_err_t stub_imu_gyro_rezero(void)
{
    s_stub_gyro_on = true;
    printf("[STUB IMU] giroscópio re-zerado (stub).\n");
    return KIT_OK;
}

static bool stub_imu_gyro_poll(int32_t *yaw_cdeg, int32_t *pitch_cdeg,
                               int32_t *roll_cdeg, int32_t *rate_cdps)
{
    if (!s_stub_gyro_on) return false;
    if (yaw_cdeg)   *yaw_cdeg   = 0;
    if (pitch_cdeg) *pitch_cdeg = 0;
    if (roll_cdeg)  *roll_cdeg  = 0;
    if (rate_cdps)  *rate_cdps  = 0;
    return true;
}

static void stub_imu_gyro_stop(void)
{
    s_stub_gyro_on = false;
    printf("[STUB IMU] giroscópio desligado (stub).\n");
}

/* -----------------------------------------------------------------------
 * Montagem das Tabelas de API (Stubs)
 * ----------------------------------------------------------------------- */

static const kit_display_api_t s_stub_display = {
    .get_screen     = stub_display_get_screen,
    .refresh        = stub_display_refresh,
    .set_brightness = stub_display_set_brightness,
    .get_brightness = stub_display_get_brightness,
};

static const kit_input_api_t s_stub_input = {
    .register_callback = stub_input_register_callback,
};

static const kit_storage_api_t s_stub_storage = {
    .set_str   = stub_storage_set_str,
    .get_str   = stub_storage_get_str,
    .set_i32   = stub_storage_set_i32,
    .get_i32   = stub_storage_get_i32,
    .open_file = stub_storage_open_file,
};

static const kit_random_api_t s_stub_random = {
    .u32       = stub_random_u32,
    .range     = stub_random_range,
    .bytes     = stub_random_bytes,
    .get_float = stub_random_float,
};

static const kit_time_api_t s_stub_time = {
    .get_millis   = stub_time_get_millis,
    .get_datetime = stub_time_get_datetime,
    .delay_ms     = stub_time_delay_ms,
};

static const kit_audio_api_t s_stub_audio = {
    .beep       = stub_audio_beep,
    .set_volume = stub_audio_set_volume,
    .sfx        = stub_audio_sfx,
    .fuse       = stub_audio_fuse,
};

static const kit_power_api_t s_stub_power = {
    .keep_awake = stub_power_keep_awake,
};

static const kit_system_api_t s_stub_system = {
    .get_info = stub_system_get_info,
    .exit     = stub_system_exit,
};

static const kit_imu_api_t s_stub_imu = {
    .register_shake_callback = stub_imu_register_shake_callback,
    .register_tilt_callback = stub_imu_register_tilt_callback,
    .gyro_start  = stub_imu_gyro_start,
    .gyro_rezero = stub_imu_gyro_rezero,
    .gyro_poll   = stub_imu_gyro_poll,
    .gyro_stop   = stub_imu_gyro_stop,
};

static const kit_api_table_t s_stub_api_table = {
    .display = &s_stub_display,
    .input   = &s_stub_input,
    .storage = &s_stub_storage,
    .random  = &s_stub_random,
    .time    = &s_stub_time,
    .audio   = &s_stub_audio,
    .power   = &s_stub_power,
    .system  = &s_stub_system,
    .imu     = &s_stub_imu,
};

/* -----------------------------------------------------------------------
 * API pública dos Stubs
 * ----------------------------------------------------------------------- */

/**
 * Retorna a tabela de API stub para uso em testes desktop.
 * Equivalente a kit_api_get_table() do firmware.
 */
const kit_api_table_t *kit_stub_get_api_table(void)
{
    return &s_stub_api_table;
}

/**
 * Cria e retorna um kit_tool_ctx_t preenchido com stubs.
 * Uso típico:
 * @code
 * int main(void) {
 *     kit_tool_ctx_t ctx = kit_stub_create_context("com.test.tool");
 *     tool_init(&ctx);
 *     // ... interação ...
 *     tool_destroy();
 * }
 * @endcode
 */
kit_tool_ctx_t kit_stub_create_context(const char *tool_id)
{
    kit_tool_ctx_t ctx = {
        .tool_id   = tool_id ? tool_id : "com.kit.stub",
        .data_path = "./data",
        .api       = &s_stub_api_table,
    };
    return ctx;
}
