#include "kit_time.h"
#include "kit_power.h"          // kit_i2c_read_bytes / _write_reg (barramento I2C compartilhado)

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <time.h>
#include <sys/time.h>
#include <string.h>

static const char *TAG = "KIT_TIME";

// -- PCF85063A (RTC de hardware, I2C 0x51) --------------------------------
#define PCF_ADDR        0x51
#define PCF_REG_CTRL1   0x00
#define PCF_REG_SECONDS 0x04   // bit7 = OS (oscilador parou => hora inválida)
#define PCF_CTRL1_STOP  0x20

// Fuso do Brasil (UTC-3, sem horário de verão desde 2019).
#define KIT_TZ "<-03>3"

static bool s_synced;          // hora validada nesta sessão (RTC ou NTP)
static bool s_sntp_started;

static inline uint8_t bcd2dec(uint8_t b) { return (b >> 4) * 10 + (b & 0x0F); }
static inline uint8_t dec2bcd(uint8_t d) { return ((d / 10) << 4) | (d % 10); }

// Lê o RTC. Devolve KIT_FAIL se o oscilador parou (bit OS) ou o I2C falhou.
static kit_err_t rtc_read(struct tm *out)
{
    uint8_t r[7];
    if (kit_i2c_read_bytes(PCF_ADDR, PCF_REG_SECONDS, r, sizeof(r)) != ESP_OK) {
        return KIT_FAIL;
    }
    if (r[0] & 0x80) return KIT_FAIL;   // OS: hora não confiável

    memset(out, 0, sizeof(*out));
    out->tm_sec  = bcd2dec(r[0] & 0x7F);
    out->tm_min  = bcd2dec(r[1] & 0x7F);
    out->tm_hour = bcd2dec(r[2] & 0x3F);
    out->tm_mday = bcd2dec(r[3] & 0x3F);
    // r[4] = dia da semana (ignorado, mktime recalcula)
    out->tm_mon  = bcd2dec(r[5] & 0x1F) - 1;
    out->tm_year = bcd2dec(r[6]) + 100;   // anos desde 1900; PCF conta de 2000
    return KIT_OK;
}

// Grava a hora local no RTC (STOP -> escreve -> RUN, limpando o bit OS).
static void rtc_write(const struct tm *t)
{
    kit_i2c_write_reg(PCF_ADDR, PCF_REG_CTRL1, PCF_CTRL1_STOP);
    kit_i2c_write_reg(PCF_ADDR, PCF_REG_SECONDS + 0, dec2bcd(t->tm_sec) & 0x7F);
    kit_i2c_write_reg(PCF_ADDR, PCF_REG_SECONDS + 1, dec2bcd(t->tm_min));
    kit_i2c_write_reg(PCF_ADDR, PCF_REG_SECONDS + 2, dec2bcd(t->tm_hour));
    kit_i2c_write_reg(PCF_ADDR, PCF_REG_SECONDS + 3, dec2bcd(t->tm_mday));
    kit_i2c_write_reg(PCF_ADDR, PCF_REG_SECONDS + 4, t->tm_wday & 0x07);
    kit_i2c_write_reg(PCF_ADDR, PCF_REG_SECONDS + 5, dec2bcd(t->tm_mon + 1));
    kit_i2c_write_reg(PCF_ADDR, PCF_REG_SECONDS + 6, dec2bcd((t->tm_year - 100) & 0xFF));
    kit_i2c_write_reg(PCF_ADDR, PCF_REG_CTRL1, 0x00);
}

// -- SNTP ----------------------------------------------------------------

static void on_sntp_sync(struct timeval *tv)
{
    (void)tv;
    s_synced = true;

    time_t now = time(NULL);
    struct tm local;
    localtime_r(&now, &local);
    rtc_write(&local);

    char buf[32];
    strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", &local);
    ESP_LOGI(TAG, "hora sincronizada por NTP: %s (gravada no RTC)", buf);
}

void kit_time_notify_online(void)
{
    if (!s_sntp_started) {
        esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
        cfg.start = true;
        cfg.sync_cb = on_sntp_sync;
        if (esp_netif_sntp_init(&cfg) != ESP_OK) {
            ESP_LOGW(TAG, "falha ao iniciar SNTP");
            return;
        }
        // O PCF85063A segura a hora com folga entre sincronizações; não há
        // motivo para acordar o rádio de hora em hora (default do lwIP) só
        // para o NTP. 12 h chega de sobra para corrigir o drift do RTC.
        esp_sntp_set_sync_interval(12 * 60 * 60 * 1000);
        s_sntp_started = true;
        ESP_LOGI(TAG, "SNTP iniciado (pool.ntp.org, resync 12 h)");
    } else {
        esp_netif_sntp_start();   // reconectou: força uma nova consulta
    }
}

bool kit_time_is_synced(void)
{
    return s_synced;
}

// -- Ciclo de vida -----------------------------------------------------

kit_err_t kit_time_init(void)
{
    setenv("TZ", KIT_TZ, 1);
    tzset();

    ESP_LOGI(TAG, "Lendo RTC PCF85063A (I2C 0x%02X)...", PCF_ADDR);
    struct tm t;
    if (rtc_read(&t) == KIT_OK && (t.tm_year + 1900) >= 2024) {
        time_t epoch = mktime(&t);   // 't' está em hora local (TZ já setado)
        struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
        settimeofday(&tv, NULL);
        s_synced = true;

        char buf[32];
        strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", &t);
        ESP_LOGI(TAG, "hora do RTC: %s", buf);
    } else {
        ESP_LOGW(TAG, "RTC sem hora válida — aguardando NTP (Wi-Fi)");
    }
    return KIT_OK;
}

uint64_t kit_time_get_millis_impl(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000);
}

kit_err_t kit_time_get_datetime_impl(kit_datetime_t *dt)
{
    if (!dt) return KIT_ERR_INVALID_ARG;

    time_t now = time(NULL);
    struct tm local;
    localtime_r(&now, &local);

    dt->year   = local.tm_year + 1900;
    dt->month  = local.tm_mon + 1;
    dt->day    = local.tm_mday;
    dt->hour   = local.tm_hour;
    dt->minute = local.tm_min;
    dt->second = local.tm_sec;
    return s_synced ? KIT_OK : KIT_FAIL;
}

void kit_time_delay_ms_impl(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}
