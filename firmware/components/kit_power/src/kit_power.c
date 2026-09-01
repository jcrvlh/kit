#include "kit_power.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "KIT_POWER";
static char s_device_id[16] = "KIT-0000";
static bool s_keep_awake = false;
static bool s_i2c_initialized = false;
static i2c_master_bus_handle_t s_i2c_bus = NULL;

// A API i2c_master exige um handle por dispositivo (diferente da API legada,
// que aceitava o endereço direto em cada transação). Este pequeno cache
// registra um handle na primeira vez que um endereço é usado e reaproveita
// nas chamadas seguintes.
#define KIT_I2C_MAX_DEVICES 8
static struct {
    uint8_t addr;
    i2c_master_dev_handle_t handle;
} s_i2c_devices[KIT_I2C_MAX_DEVICES];
static uint8_t s_i2c_device_count = 0;

static i2c_master_dev_handle_t get_or_add_i2c_device(uint8_t dev_addr)
{
    for (uint8_t i = 0; i < s_i2c_device_count; i++) {
        if (s_i2c_devices[i].addr == dev_addr) {
            return s_i2c_devices[i].handle;
        }
    }
    if (!s_i2c_bus || s_i2c_device_count >= KIT_I2C_MAX_DEVICES) {
        return NULL;
    }
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = dev_addr,
        .scl_speed_hz = KIT_I2C_FREQ_HZ,
    };
    i2c_master_dev_handle_t handle = NULL;
    if (i2c_master_bus_add_device(s_i2c_bus, &dev_cfg, &handle) != ESP_OK) {
        return NULL;
    }
    s_i2c_devices[s_i2c_device_count].addr = dev_addr;
    s_i2c_devices[s_i2c_device_count].handle = handle;
    s_i2c_device_count++;
    return handle;
}

// Registradores principais do AXP2101
#define AXP2101_REG_COMMON_CONFIG   0x10   // bit 0 = soft power-off
#define AXP2101_REG_PMU_STATUS1     0x00
#define AXP2101_REG_PMU_STATUS2     0x01
#define AXP2101_REG_BAT_PERCENT     0xA4
#define AXP2101_REG_ALDO1_VOLT      0x92
#define AXP2101_REG_ALDO2_VOLT      0x93
#define AXP2101_REG_ALDO3_VOLT      0x94
#define AXP2101_REG_ALDO4_VOLT      0x95
#define AXP2101_REG_BLDO1_VOLT      0x96
#define AXP2101_REG_BLDO2_VOLT      0x97
#define AXP2101_REG_LDO_ONOFF       0x90

// IRQ do botão PWRON (mapeamento conforme XPowersLib / datasheet AXP2101 v1.0):
// enable em INTEN2 (0x41), status em INTSTS2 (0x49), "write-1-to-clear".
//   bit 3 = toque curto (ponsp_irq)   bit 2 = toque longo (ponlp_irq)
#define AXP2101_REG_IRQ_EN2         0x41
#define AXP2101_REG_IRQ_STS2        0x49
#define AXP2101_PKEY_SHORT_BIT      0x08
#define AXP2101_PKEY_LONG_BIT       0x04
#define AXP2101_PKEY_NEG_BIT        0x02
#define AXP2101_PKEY_POS_BIT        0x01

static bool s_pwr_pressed = false;

esp_err_t kit_i2c_write_reg(uint8_t dev_addr, uint8_t reg, uint8_t val)
{
    i2c_master_dev_handle_t dev = get_or_add_i2c_device(dev_addr);
    if (!dev) return ESP_FAIL;
    uint8_t write_buf[2] = {reg, val};
    return i2c_master_transmit(dev, write_buf, sizeof(write_buf), pdMS_TO_TICKS(100));
}

esp_err_t kit_i2c_read_reg(uint8_t dev_addr, uint8_t reg, uint8_t *out_val)
{
    i2c_master_dev_handle_t dev = get_or_add_i2c_device(dev_addr);
    if (!dev) return ESP_FAIL;
    return i2c_master_transmit_receive(dev, &reg, 1, out_val, 1, pdMS_TO_TICKS(100));
}

esp_err_t kit_i2c_read_bytes(uint8_t dev_addr, uint8_t reg, uint8_t *data, size_t len)
{
    i2c_master_dev_handle_t dev = get_or_add_i2c_device(dev_addr);
    if (!dev) return ESP_FAIL;
    return i2c_master_transmit_receive(dev, &reg, 1, data, len, pdMS_TO_TICKS(100));
}

i2c_master_bus_handle_t kit_power_get_i2c_bus_handle(void)
{
    return s_i2c_bus;
}

static esp_err_t init_i2c_master(void)
{
    if (s_i2c_initialized) return ESP_OK;

    i2c_master_bus_config_t bus_config = {
        .i2c_port = KIT_I2C_PORT,
        .sda_io_num = KIT_I2C_SDA_PIN,
        .scl_io_num = KIT_I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&bus_config, &s_i2c_bus);
    if (ret == ESP_OK) {
        s_i2c_initialized = true;
        ESP_LOGI(TAG, "Barramento I2C compartilhado inicializado (SDA=%d, SCL=%d @ 400kHz)",
                 KIT_I2C_SDA_PIN, KIT_I2C_SCL_PIN);

        // Varredura de dispositivos I2C conectados
        ESP_LOGI(TAG, "Escaneando barramento I2C...");
        for (uint8_t addr = 1; addr < 127; addr++) {
            if (i2c_master_probe(s_i2c_bus, addr, 10) == ESP_OK) {
                ESP_LOGI(TAG, " -> Dispositivo I2C encontrado no endereco: 0x%02X", addr);
            }
        }
    }
    return ret;
}


kit_err_t kit_power_init(void)
{
    ESP_LOGI(TAG, "Inicializando subsistema de energia e PMIC AXP2101...");

    // 1. Inicializa o barramento I2C
    esp_err_t err = init_i2c_master();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar barramento I2C: %s", esp_err_to_name(err));
        return KIT_FAIL;
    }

    // 2. Configura tensões do PMIC AXP2101 conforme especificação oficial Waveshare AMOLED 1.8"
    // ALDO1 (DVDD 1.8V) -> 0x0D
    kit_i2c_write_reg(AXP2101_I2C_ADDR, AXP2101_REG_ALDO1_VOLT, 0x0D);
    // ALDO2 (DVDD 2.8V) -> 0x17
    kit_i2c_write_reg(AXP2101_I2C_ADDR, AXP2101_REG_ALDO2_VOLT, 0x17);
    // ALDO3 (Touch VDD 3.3V) -> 0x1C
    kit_i2c_write_reg(AXP2101_I2C_ADDR, AXP2101_REG_ALDO3_VOLT, 0x1C);
    // ALDO4 (AVDD 3.3V) -> 0x1C
    kit_i2c_write_reg(AXP2101_I2C_ADDR, AXP2101_REG_ALDO4_VOLT, 0x1C);
    // BLDO1 (OLED VDD 3.3V) -> 0x1C
    kit_i2c_write_reg(AXP2101_I2C_ADDR, AXP2101_REG_BLDO1_VOLT, 0x1C);
    // BLDO2 (MIC/Audio VDD 3.3V) -> 0x1C
    kit_i2c_write_reg(AXP2101_I2C_ADDR, AXP2101_REG_BLDO2_VOLT, 0x1C);

    // Habilita as saídas LDO (ALDO1-4, BLDO1-2, DLDO1-2)
    uint8_t ldo_state = 0;
    if (kit_i2c_read_reg(AXP2101_I2C_ADDR, AXP2101_REG_LDO_ONOFF, &ldo_state) == ESP_OK) {
        ldo_state |= 0xFF; // Habilita todas as saídas de alimentação
        kit_i2c_write_reg(AXP2101_I2C_ADDR, AXP2101_REG_LDO_ONOFF, ldo_state);
        // Habilita também registrador 0x91 (BLDO on/off)
        kit_i2c_write_reg(AXP2101_I2C_ADDR, 0x91, 0xFF);
        ESP_LOGI(TAG, "Trilhas de alimentação do AMOLED e Touch (ALDO1-4, BLDO1-2) ativadas.");
    } else {
        ESP_LOGW(TAG, "AXP2101 não respondeu no endereço 0x34.");
    }


    // 3. Gera a Identidade Única do Dispositivo via MAC de fábrica
    uint8_t mac[6] = {0};
    if (esp_read_mac(mac, ESP_MAC_BASE) == ESP_OK) {
        snprintf(s_device_id, sizeof(s_device_id), "KIT-%02X%02X", mac[4], mac[5]);
    } else {
        snprintf(s_device_id, sizeof(s_device_id), "KIT-ABCD");
    }

    // Habilita o IRQ de toque (curto/longo/press/release) na tecla PWRON e limpa pendências.
    uint8_t irq_en = 0;
    if (kit_i2c_read_reg(AXP2101_I2C_ADDR, AXP2101_REG_IRQ_EN2, &irq_en) == ESP_OK) {
        kit_i2c_write_reg(AXP2101_I2C_ADDR, AXP2101_REG_IRQ_EN2,
                          irq_en | AXP2101_PKEY_SHORT_BIT | AXP2101_PKEY_LONG_BIT |
                          AXP2101_PKEY_NEG_BIT | AXP2101_PKEY_POS_BIT);
    }
    kit_i2c_write_reg(AXP2101_I2C_ADDR, AXP2101_REG_IRQ_STS2,
                      AXP2101_PKEY_SHORT_BIT | AXP2101_PKEY_LONG_BIT |
                      AXP2101_PKEY_NEG_BIT | AXP2101_PKEY_POS_BIT);

    ESP_LOGI(TAG, "Device Identity: %s", s_device_id);
    return KIT_OK;
}

kit_pek_event_t kit_power_poll_pek_event(void)
{
    uint8_t sts = 0;
    if (kit_i2c_read_reg(AXP2101_I2C_ADDR, AXP2101_REG_IRQ_STS2, &sts) != ESP_OK) {
        return KIT_PEK_NONE;
    }
    
    if (sts & AXP2101_PKEY_POS_BIT) s_pwr_pressed = true;
    if (sts & AXP2101_PKEY_NEG_BIT) s_pwr_pressed = false;

    uint8_t hit = sts & (AXP2101_PKEY_SHORT_BIT | AXP2101_PKEY_LONG_BIT | AXP2101_PKEY_NEG_BIT | AXP2101_PKEY_POS_BIT);
    if (!hit) return KIT_PEK_NONE;

    kit_i2c_write_reg(AXP2101_I2C_ADDR, AXP2101_REG_IRQ_STS2, hit); // W1C
    
    if (hit & AXP2101_PKEY_LONG_BIT) {
        // Quando dá o long press event, nós assumimos que ele ainda está segurando 
        // caso o release event não tenha chegado ao mesmo tempo.
        s_pwr_pressed = true;
        return KIT_PEK_LONG;
    }
    if (hit & AXP2101_PKEY_SHORT_BIT) return KIT_PEK_SHORT;
    
    return KIT_PEK_NONE;
}

bool kit_power_is_pwr_pressed(void)
{
    // Usa o estado rastreado pelos IRQs de borda
    return s_pwr_pressed;
}

uint8_t kit_power_get_battery_percentage(void)
{
    uint8_t percent = 100;
    if (kit_i2c_read_reg(AXP2101_I2C_ADDR, AXP2101_REG_BAT_PERCENT, &percent) == ESP_OK) {
        if (percent > 100) percent = 100;
        return percent;
    }
    return 100; // Valor padrão se a leitura I2C falhar
}

bool kit_power_is_charging(void)
{
    uint8_t status = 0;
    if (kit_i2c_read_reg(AXP2101_I2C_ADDR, AXP2101_REG_PMU_STATUS2, &status) == ESP_OK) {
        return (status & 0x20) != 0; // Bit de status de carregamento ativo
    }
    return false;
}

const char *kit_power_get_device_id(void)
{
    return s_device_id;
}

kit_err_t kit_power_keep_awake_impl(bool enable)
{
    s_keep_awake = enable;
    ESP_LOGI(TAG, "Modo Keep-Awake: %s", enable ? "ATIVADO" : "DESATIVADO");
    return KIT_OK;
}

bool kit_power_is_keep_awake(void)
{
    return s_keep_awake;
}

kit_err_t kit_power_shutdown(void)
{
    ESP_LOGW(TAG, "Desligando o dispositivo (soft power-off do AXP2101)...");
    uint8_t cfg = 0;
    kit_i2c_read_reg(AXP2101_I2C_ADDR, AXP2101_REG_COMMON_CONFIG, &cfg);
    esp_err_t err = kit_i2c_write_reg(AXP2101_I2C_ADDR, AXP2101_REG_COMMON_CONFIG,
                                      cfg | 0x01);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao acionar o desligamento no AXP2101: %s",
                 esp_err_to_name(err));
        return KIT_FAIL;
    }
    return KIT_OK;
}
