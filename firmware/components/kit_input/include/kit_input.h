#pragma once

#include "kit_api.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CST820_I2C_ADDR         0x15
#define CST820_REG_DIS_AUTOSLEEP 0xFE
#define KIT_TOUCH_INT_PIN       GPIO_NUM_21

/**
 * Inicializa o controlador de toque CST820 e registra o dispositivo de entrada no LVGL v9.
 */
kit_err_t kit_input_init(void);

/**
 * Registra um callback para eventos brutos de entrada do touch.
 */
kit_err_t kit_input_register_callback_impl(kit_input_callback_t cb, void *user_data);

/**
 * Habilita/desabilita o processamento de toque no LVGL (usado quando a tela
 * é apagada pelo botão PWR).
 */
kit_err_t kit_input_set_enabled_impl(bool enabled);

/**
 * Lê o CST820 diretamente (sem o indev do LVGL) e informa se há um toque
 * válido na tela agora. Usado para acordar o aparelho do repouso de tela,
 * quando o processamento de toque do LVGL está desligado.
 */
bool kit_input_touch_present_impl(void);

#ifdef __cplusplus
}
#endif
