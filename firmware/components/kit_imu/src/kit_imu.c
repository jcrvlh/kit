#include "kit_imu.h"
#include "kit_power.h"          // kit_i2c_read_reg / _read_bytes / _write_reg
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

static const char *TAG = "KIT_IMU";

// QMI8658 — mapa de registradores (subconjunto)
#define QMI8658_ADDR         0x6B
#define QMI8658_REG_WHOAMI   0x00
#define QMI8658_REG_CTRL1    0x02
#define QMI8658_REG_CTRL2    0x03   // config do acelerômetro (FS + ODR)
#define QMI8658_REG_CTRL3    0x04   // config do giroscópio
#define QMI8658_REG_CTRL7    0x08   // enable dos sensores
#define QMI8658_REG_RESET    0x60
#define QMI8658_REG_AX_L     0x35   // AX_L, AX_H, AY_L, AY_H, AZ_L, AZ_H
#define QMI8658_WHOAMI_VAL   0x05

// CTRL2 = 0b010 (±8 g) << 4 | 0x06 (125 Hz)
#define QMI8658_CTRL2_ACC    0x26
#define ACCEL_SCALE_G        (8.0f / 32768.0f)

// Ajuste do gesto (ver "Em aberto para decidir" do protótipo):
#define SHAKE_G              2.2f        // módulo de |a| que conta como chacoalhar
#define SHAKE_DEBOUNCE_US    700000LL    // 0,7 s entre disparos

static bool    s_ready = false;
static int64_t s_last_shake_us = 0;

// Callback de shake registrado por Tools externas via kit_api.imu
static kit_shake_callback_t s_shake_cb = NULL;
static void                *s_shake_ud = NULL;

kit_err_t kit_imu_init(void)
{
    uint8_t who = 0;
    if (kit_i2c_read_reg(QMI8658_ADDR, QMI8658_REG_WHOAMI, &who) != ESP_OK) {
        ESP_LOGW(TAG, "QMI8658 não respondeu no I2C 0x%02X — chacoalhar desativado", QMI8658_ADDR);
        return KIT_FAIL;
    }
    if (who != QMI8658_WHOAMI_VAL)
        ESP_LOGW(TAG, "WHO_AM_I = 0x%02X (esperado 0x%02X), seguindo assim mesmo", who, QMI8658_WHOAMI_VAL);

    kit_i2c_write_reg(QMI8658_ADDR, QMI8658_REG_RESET, 0xB0);
    vTaskDelay(pdMS_TO_TICKS(20));

    kit_i2c_write_reg(QMI8658_ADDR, QMI8658_REG_CTRL1, 0x40);              // endereço auto-incrementa (burst read)
    kit_i2c_write_reg(QMI8658_ADDR, QMI8658_REG_CTRL2, QMI8658_CTRL2_ACC); // accel ±8 g @ 125 Hz
    kit_i2c_write_reg(QMI8658_ADDR, QMI8658_REG_CTRL3, 0x00);             // giroscópio desligado
    kit_i2c_write_reg(QMI8658_ADDR, QMI8658_REG_CTRL7, 0x01);             // habilita só o acelerômetro
    vTaskDelay(pdMS_TO_TICKS(10));

    s_ready = true;
    ESP_LOGI(TAG, "QMI8658 pronto (accel ±8g @125Hz) — chacoalhar dispara em |a| > %.1f g", SHAKE_G);
    return KIT_OK;
}

bool kit_imu_poll_shake(void)
{
    if (!s_ready) return false;

    uint8_t b[6];
    if (kit_i2c_read_bytes(QMI8658_ADDR, QMI8658_REG_AX_L, b, sizeof(b)) != ESP_OK)
        return false;

    int16_t ax = (int16_t)((b[1] << 8) | b[0]);
    int16_t ay = (int16_t)((b[3] << 8) | b[2]);
    int16_t az = (int16_t)((b[5] << 8) | b[4]);

    float gx = ax * ACCEL_SCALE_G;
    float gy = ay * ACCEL_SCALE_G;
    float gz = az * ACCEL_SCALE_G;
    float mag = sqrtf(gx * gx + gy * gy + gz * gz);   // ~1,0 em repouso

    if (mag < SHAKE_G) return false;

    int64_t now = esp_timer_get_time();
    if (now - s_last_shake_us < SHAKE_DEBOUNCE_US) return false;
    s_last_shake_us = now;

    ESP_LOGI(TAG, "Chacoalhar detectado (|a| = %.2f g)", mag);
    return true;
}

kit_err_t kit_imu_register_shake_callback_impl(kit_shake_callback_t cb, void *user_data)
{
    s_shake_cb = cb;
    s_shake_ud = user_data;
    ESP_LOGI(TAG, "Callback de shake %s.", cb ? "registrado" : "removido");
    return KIT_OK;
}

void kit_imu_dispatch_shake(void)
{
    if (s_shake_cb) {
        s_shake_cb(s_shake_ud);
    }
}

void kit_imu_clear_shake_callback(void)
{
    s_shake_cb = NULL;
    s_shake_ud = NULL;
}
