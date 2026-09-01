#pragma once

#include "kit_api.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KIT_I2C_PORT       I2C_NUM_0
#define KIT_I2C_SDA_PIN    GPIO_NUM_15
#define KIT_I2C_SCL_PIN    GPIO_NUM_14
#define KIT_I2C_FREQ_HZ    400000

#define AXP2101_I2C_ADDR   0x34

/**
 * Inicializa o barramento I2C mestre compartilhado e o PMIC AXP2101.
 */
kit_err_t kit_power_init(void);

/**
 * Funções auxiliares para leitura/escrita no barramento I2C compartilhado.
 */
esp_err_t kit_i2c_write_reg(uint8_t dev_addr, uint8_t reg, uint8_t val);
esp_err_t kit_i2c_read_reg(uint8_t dev_addr, uint8_t reg, uint8_t *out_val);
esp_err_t kit_i2c_read_bytes(uint8_t dev_addr, uint8_t reg, uint8_t *data, size_t len);

/**
 * Handle do barramento I2C mestre compartilhado (driver/i2c_master.h), para
 * componentes que precisam registrar seu próprio dispositivo no barramento
 * (ex: kit_audio via esp_codec_dev).
 */
i2c_master_bus_handle_t kit_power_get_i2c_bus_handle(void);

/**
 * Obtém a porcentagem da bateria (0 a 100%).
 */
uint8_t kit_power_get_battery_percentage(void);

/**
 * Retorna true se a bateria estiver em processo de carga via USB.
 */
bool kit_power_is_charging(void);

/**
 * Retorna true se houver VBUS (cabo USB conectado), independente de a bateria
 * estar carregando. Usado para não entrar em light sleep enquanto plugado
 * (console serial / Web Installer).
 */
bool kit_power_is_usb_connected(void);

/**
 * Evento do botão de energia (PWRON do AXP2101).
 */
typedef enum {
    KIT_PEK_NONE = 0,
    KIT_PEK_SHORT,   // toque curto: alterna a tela ligada/desligada
    KIT_PEK_LONG,    // toque longo (o AXP também desliga o sistema por hardware)
} kit_pek_event_t;

/**
 * Consome (e limpa) o último evento de toque no botão PWR. Deve ser chamada
 * periodicamente — o AXP2101 mantém o evento latcheado até ser lido.
 */
kit_pek_event_t kit_power_poll_pek_event(void);

/**
 * Retorna true se o botão PWR estiver sendo pressionado fisicamente no momento.
 */
bool kit_power_is_pwr_pressed(void);

/**
 * Obtém o identificador único do dispositivo (ex: "KIT-A83F").
 */
const char *kit_power_get_device_id(void);

/**
 * Informa ao subsistema de energia que a tela entrou (true) ou saiu (false) do
 * repouso. Em repouso e fora do USB, habilita o light sleep automático do
 * esp_pm (CPU dorme entre os polls, ~1–2 mA); ao acordar, volta ao
 * escalonamento de frequência normal. No-op se CONFIG_PM_ENABLE estiver
 * desligado. Deve ser reavaliada quando o estado de carga muda.
 */
kit_err_t kit_power_set_screen_sleeping(bool sleeping);

/**
 * Implementação para kit_api: Impede suspensão automática.
 */
kit_err_t kit_power_keep_awake_impl(bool enable);

/**
 * Retorna true enquanto o modo Keep-Awake estiver ativo. O Runtime usa isso
 * para não apagar a tela por repouso nem desligar o aparelho por inatividade
 * (ex.: a Timer Tool mantém isso ligado enquanto conta).
 */
bool kit_power_is_keep_awake(void);

/**
 * Desliga o dispositivo por software (soft power-off do AXP2101 — bit 0 do
 * registrador 0x10, "PMU common config"). Não retorna se der certo: o PMIC
 * corta a alimentação e só volta com o botão PWR. Usado pelo desligamento
 * automático por inatividade (ver kit_config).
 */
kit_err_t kit_power_shutdown(void);

#ifdef __cplusplus
}
#endif
