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
#define QMI8658_REG_GX_L     0x3B   // GX_L, GX_H, GY_L, GY_H, GZ_L, GZ_H
#define QMI8658_WHOAMI_VAL   0x05

// CTRL2 = 0b010 (±8 g) << 4 | 0x06 (125 Hz)
#define QMI8658_CTRL2_ACC    0x26
#define ACCEL_SCALE_G        (8.0f / 32768.0f)

// CTRL3 = 0b110 (±1024 dps) << 4 | 0x05 (250 Hz) — só ligado sob demanda
// (Tool "Vira Certo"), CTRL7 bit1. Fica desligado o resto do tempo: mais
// corrente que o acelerômetro sozinho.
#define QMI8658_CTRL3_GYRO   0x65
#define GYRO_SCALE_DPS       (1024.0f / 32768.0f)
#define GYRO_CALIB_SAMPLES   15
#define GYRO_CALIB_GAP_MS    4

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

// Giroscópio (Tool "Vira Certo"): ligado só sob demanda, integra ângulo em
// graus desde o kit_imu_gyro_start() mais recente. Sem magnetômetro não há
// "norte" — é sempre ângulo relativo ao instante em que a Tool zerou.
static bool  s_gyro_on = false;
static float s_gyro_bias_x = 0.0f, s_gyro_bias_y = 0.0f, s_gyro_bias_z = 0.0f;
static float s_gyro_yaw = 0.0f, s_gyro_pitch = 0.0f, s_gyro_roll = 0.0f;
static float s_gyro_rate_dps = 0.0f;
static int64_t s_gyro_last_us = 0;

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

// Lê os 6 bytes de giroscópio e converte para graus/s. false se o I2C falhou.
static bool read_gyro_dps(float *gx, float *gy, float *gz)
{
    uint8_t b[6];
    if (kit_i2c_read_bytes(QMI8658_ADDR, QMI8658_REG_GX_L, b, sizeof(b)) != ESP_OK)
        return false;

    int16_t x = (int16_t)((b[1] << 8) | b[0]);
    int16_t y = (int16_t)((b[3] << 8) | b[2]);
    int16_t z = (int16_t)((b[5] << 8) | b[4]);

    *gx = x * GYRO_SCALE_DPS;
    *gy = y * GYRO_SCALE_DPS;
    *gz = z * GYRO_SCALE_DPS;
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

// Liga fisicamente o giroscópio (escreve CTRL3/CTRL7 no QMI8658) sem
// calibrar nada. Separado de kit_imu_gyro_zero() porque religar o sensor
// puxa uma corrente extra do PMIC na hora — chamado repetidas vezes (uma
// por tentativa) dava uma piscada visível no AMOLED. Idempotente: chamar de
// novo com o giroscópio já ligado não faz nada.
static kit_err_t gyro_power_on(void)
{
    if (!s_ready) return KIT_FAIL;
    if (s_gyro_on) return KIT_OK;

    uint8_t ctrl7 = 0;
    kit_i2c_read_reg(QMI8658_ADDR, QMI8658_REG_CTRL7, &ctrl7);
    kit_i2c_write_reg(QMI8658_ADDR, QMI8658_REG_CTRL3, QMI8658_CTRL3_GYRO);
    kit_i2c_write_reg(QMI8658_ADDR, QMI8658_REG_CTRL7, (uint8_t)(ctrl7 | 0x02));
    vTaskDelay(pdMS_TO_TICKS(20));   // assenta depois de ligar
    s_gyro_on = true;
    return KIT_OK;
}

// Tira a média de algumas amostras como bias (zero-rate offset) e zera os
// ângulos acumulados. Não mexe em registrador nenhum — só leitura I2C, bem
// mais barato que ligar o sensor. Chame com o aparelho parado.
static kit_err_t gyro_calibrate(void)
{
    if (!s_gyro_on) return KIT_FAIL;

    float sx = 0.0f, sy = 0.0f, sz = 0.0f;
    int n = 0;
    for (int i = 0; i < GYRO_CALIB_SAMPLES; i++) {
        float gx, gy, gz;
        if (read_gyro_dps(&gx, &gy, &gz)) { sx += gx; sy += gy; sz += gz; n++; }
        vTaskDelay(pdMS_TO_TICKS(GYRO_CALIB_GAP_MS));
    }
    s_gyro_bias_x = n ? sx / n : 0.0f;
    s_gyro_bias_y = n ? sy / n : 0.0f;
    s_gyro_bias_z = n ? sz / n : 0.0f;

    s_gyro_yaw = s_gyro_pitch = s_gyro_roll = 0.0f;
    s_gyro_rate_dps = 0.0f;
    s_gyro_last_us = esp_timer_get_time();
    ESP_LOGI(TAG, "Giroscópio zerado (bias x=%.2f y=%.2f z=%.2f \xC2\xB0/s)",
             s_gyro_bias_x, s_gyro_bias_y, s_gyro_bias_z);
    return KIT_OK;
}

kit_err_t kit_imu_gyro_start(void)
{
    kit_err_t r = gyro_power_on();
    if (r != KIT_OK) return r;
    return gyro_calibrate();
}

kit_err_t kit_imu_gyro_rezero(void)
{
    kit_err_t r = gyro_power_on();   // idempotente — já deve estar ligado
    if (r != KIT_OK) return r;
    return gyro_calibrate();
}

bool kit_imu_gyro_poll(float *yaw_deg, float *pitch_deg, float *roll_deg, float *rate_dps)
{
    if (!s_gyro_on) return false;

    float gx, gy, gz;
    if (!read_gyro_dps(&gx, &gy, &gz)) return false;

    gx -= s_gyro_bias_x;
    gy -= s_gyro_bias_y;
    gz -= s_gyro_bias_z;

    int64_t now = esp_timer_get_time();
    float dt = (float)(now - s_gyro_last_us) / 1000000.0f;
    s_gyro_last_us = now;
    if (dt > 0.2f) dt = 0.2f;   // 1ª amostra / engasgo do I2C: não acumula um salto

    // Eixos calibrados no HW (mesma ressalva do gesto de inclinar): Z é o
    // normal à tela (girar deitado na mesa), X/Y são as inclinadas na mão.
    s_gyro_roll  += gx * dt;
    s_gyro_pitch += gy * dt;
    s_gyro_yaw   += gz * dt;
    s_gyro_rate_dps = sqrtf(gx * gx + gy * gy + gz * gz);

    if (yaw_deg)   *yaw_deg   = s_gyro_yaw;
    if (pitch_deg) *pitch_deg = s_gyro_pitch;
    if (roll_deg)  *roll_deg  = s_gyro_roll;
    if (rate_dps)  *rate_dps  = s_gyro_rate_dps;
    return true;
}

void kit_imu_gyro_stop(void)
{
    if (!s_gyro_on) return;
    s_gyro_on = false;
    uint8_t ctrl7 = 0;
    kit_i2c_read_reg(QMI8658_ADDR, QMI8658_REG_CTRL7, &ctrl7);
    kit_i2c_write_reg(QMI8658_ADDR, QMI8658_REG_CTRL7, (uint8_t)(ctrl7 & ~0x02));
}

// Shim para a API de Tools (kit_api_table_t.imu->gyro_poll): mesma leitura,
// mas em centigraus inteiros — o .so do catálogo não resolve float no
// elf_loader. Só a conversão fica aqui (contexto do firmware, float ok).
bool kit_imu_gyro_poll_centi(int32_t *yaw_cdeg, int32_t *pitch_cdeg,
                             int32_t *roll_cdeg, int32_t *rate_cdps)
{
    float yaw, pitch, roll, rate;
    if (!kit_imu_gyro_poll(&yaw, &pitch, &roll, &rate)) return false;
    if (yaw_cdeg)   *yaw_cdeg   = (int32_t)lroundf(yaw   * 100.0f);
    if (pitch_cdeg) *pitch_cdeg = (int32_t)lroundf(pitch * 100.0f);
    if (roll_cdeg)  *roll_cdeg  = (int32_t)lroundf(roll  * 100.0f);
    if (rate_cdps)  *rate_cdps  = (int32_t)lroundf(rate  * 100.0f);
    return true;
}
