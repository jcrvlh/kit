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

// Gesto de inclinar (Tool "Testa"). O eixo normal à tela fica ~0 g com o
// aparelho vertical na testa e vai a ±1 g quando a tela vira para o chão/teto.
// Sinais/eixo calibrados no HW pelo log "Inclinar: … (n = … g)".
#define TILT_NEUTRAL_G       0.40f       // abaixo disto o gesto rearma (~volta à vertical)
#define TILT_TRIGGER_G       0.70f       // acima disto dispara (~45° da vertical)
#define TILT_DEBOUNCE_US     700000LL    // 0,7 s entre disparos
#define TILT_CONFIRM         2           // amostras seguidas acima do gatilho (~120 ms)
#define TILT_STEADY_G        0.55f       // rejeita só solavanco de verdade (|a| longe de 1 g)
#define TILT_DOWN_IS_POSITIVE 1          // n > 0 ⇒ tela para o chão (DOWN)

static bool    s_ready = false;
static bool    s_enabled = true;   // acelerômetro ligado? (desligado no repouso)
static int64_t s_last_shake_us = 0;

static bool    s_tilt_armed = true;
static int64_t s_last_tilt_us = 0;
static int     s_tilt_hits = 0;    // amostras seguidas acima do gatilho

// Callback de shake registrado por Tools externas via kit_api.imu
static kit_shake_callback_t s_shake_cb = NULL;
static void                *s_shake_ud = NULL;

// Callback de inclinar registrado pela Tool ativa via kit_api.imu
static kit_tilt_callback_t s_tilt_cb = NULL;
static void               *s_tilt_ud = NULL;

// Lê os 6 bytes de aceleração e converte para g. false se o I2C falhou.
static bool read_accel_g(float *gx, float *gy, float *gz)
{
    uint8_t b[6];
    if (kit_i2c_read_bytes(QMI8658_ADDR, QMI8658_REG_AX_L, b, sizeof(b)) != ESP_OK)
        return false;

    int16_t ax = (int16_t)((b[1] << 8) | b[0]);
    int16_t ay = (int16_t)((b[3] << 8) | b[2]);
    int16_t az = (int16_t)((b[5] << 8) | b[4]);

    *gx = ax * ACCEL_SCALE_G;
    *gy = ay * ACCEL_SCALE_G;
    *gz = az * ACCEL_SCALE_G;
    return true;
}

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

void kit_imu_set_enabled(bool enable)
{
    if (!s_ready || enable == s_enabled) return;
    s_enabled = enable;
    // CTRL7: bit0 = acelerômetro. Com a tela em repouso o gesto de chacoalhar
    // não é usado, então derrubamos o acelerômetro (~ dezenas de µA) e o
    // religamos ao acordar. O QMI8658 retoma na configuração já gravada em
    // CTRL2 assim que CTRL7.bit0 volta a 1.
    kit_i2c_write_reg(QMI8658_ADDR, QMI8658_REG_CTRL7, enable ? 0x01 : 0x00);
    ESP_LOGI(TAG, "Acelerômetro %s", enable ? "ligado" : "em repouso");
}

bool kit_imu_poll_shake(void)
{
    if (!s_ready || !s_enabled) return false;

    float gx, gy, gz;
    if (!read_accel_g(&gx, &gy, &gz)) return false;

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

kit_tilt_t kit_imu_poll_tilt(void)
{
    if (!s_ready || !s_enabled || !s_tilt_cb) return KIT_TILT_NONE;

    float gx, gy, gz;
    if (!read_accel_g(&gx, &gy, &gz)) return KIT_TILT_NONE;

    // Eixo normal à tela (Z do QMI8658 nesta placa — calibrar no HW).
    float n = gz;
    float mag = sqrtf(gx * gx + gy * gy + gz * gz);

    if (!s_tilt_armed) {
        if (fabsf(n) < TILT_NEUTRAL_G) s_tilt_armed = true;
        return KIT_TILT_NONE;
    }

    // Só conta como inclinada deliberada se o aparelho não está sendo sacudido
    // (|a| perto de 1 g) e o eixo passou do gatilho por várias amostras seguidas.
    bool steady = fabsf(mag - 1.0f) < TILT_STEADY_G;
    if (!steady || fabsf(n) < TILT_TRIGGER_G) {
        s_tilt_hits = 0;
        return KIT_TILT_NONE;
    }
    if (++s_tilt_hits < TILT_CONFIRM) return KIT_TILT_NONE;
    s_tilt_hits = 0;

    int64_t now = esp_timer_get_time();
    if (now - s_last_tilt_us < TILT_DEBOUNCE_US) return KIT_TILT_NONE;
    s_last_tilt_us = now;
    s_tilt_armed = false;

    bool positive = (n > 0.0f);
#if TILT_DOWN_IS_POSITIVE
    kit_tilt_t d = positive ? KIT_TILT_DOWN : KIT_TILT_UP;
#else
    kit_tilt_t d = positive ? KIT_TILT_UP : KIT_TILT_DOWN;
#endif
    ESP_LOGI(TAG, "Inclinar: %s (n = %.2f g)", d == KIT_TILT_DOWN ? "DOWN" : "UP", n);
    return d;
}

kit_err_t kit_imu_register_tilt_callback_impl(kit_tilt_callback_t cb, void *user_data)
{
    s_tilt_cb = cb;
    s_tilt_ud = user_data;
    s_tilt_armed = true;
    s_last_tilt_us = 0;
    s_tilt_hits = 0;
    ESP_LOGI(TAG, "Callback de inclinar %s.", cb ? "registrado" : "removido");
    return KIT_OK;
}

void kit_imu_dispatch_tilt(kit_tilt_t dir)
{
    if (s_tilt_cb && dir != KIT_TILT_NONE) {
        s_tilt_cb(dir, s_tilt_ud);
    }
}

void kit_imu_clear_tilt_callback(void)
{
    s_tilt_cb = NULL;
    s_tilt_ud = NULL;
    s_tilt_armed = true;
    s_tilt_hits = 0;
}
