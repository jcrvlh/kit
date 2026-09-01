#pragma once

#include "kit_api.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Resolução nativa do AMOLED Waveshare 1.8"
#define KIT_DISPLAY_WIDTH   368
#define KIT_DISPLAY_HEIGHT  448

// Pinos QSPI do Display AMOLED CO5300 (V2)
#define KIT_LCD_SDIO0       GPIO_NUM_4
#define KIT_LCD_SDIO1       GPIO_NUM_5
#define KIT_LCD_SDIO2       GPIO_NUM_6
#define KIT_LCD_SDIO3       GPIO_NUM_7
#define KIT_LCD_SCLK        GPIO_NUM_11
#define KIT_LCD_CS          GPIO_NUM_12
#define KIT_LCD_RST         GPIO_NUM_39

/**
 * Inicializa a controladora QSPI, o painel AMOLED CO5300 e o LVGL v9.
 */
kit_err_t kit_display_init(void);

/**
 * Executa o tratador de timers e tarefas gráficas do LVGL v9.
 * @return Próximo intervalo em ms para a chamada seguinte.
 */
uint32_t kit_display_process(void);

/**
 * Implementações expostas para a export table kit_api
 */
lv_obj_t *kit_display_get_screen_impl(void);
kit_err_t kit_display_refresh_impl(void);
kit_err_t kit_display_set_brightness_impl(uint8_t percentage);
uint8_t   kit_display_get_brightness_impl(void);

/**
 * Liga/desliga o painel AMOLED (sem derrubar o LVGL). Usado pelo botão PWR
 * para apagar/acender a tela.
 */
kit_err_t kit_display_set_on_impl(bool on);
bool      kit_display_is_on_impl(void);

#ifdef __cplusplus
}
#endif
