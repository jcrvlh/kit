#pragma once

#include "kit_api.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ES8311_I2C_ADDR     0x18

// Pinos I2S e Amplificador de Áudio
#define KIT_I2S_MCK_PIN     GPIO_NUM_16
#define KIT_I2S_BCK_PIN     GPIO_NUM_9
#define KIT_I2S_DI_PIN      GPIO_NUM_10
#define KIT_I2S_DO_PIN      GPIO_NUM_8
#define KIT_I2S_WS_PIN      GPIO_NUM_45
#define KIT_AUDIO_PA_PIN    GPIO_NUM_46

/**
 * Inicializa o barramento I2S, o Codec ES8311 e o controle do amplificador onboard.
 */
kit_err_t kit_audio_init(void);

/**
 * Implementações expostas para kit_api
 */
kit_err_t kit_audio_beep_impl(uint16_t freq_hz, uint16_t duration_ms);
kit_err_t kit_audio_set_volume_impl(uint8_t percentage);

/**
 * Toca um efeito sonoro pronto (sequência curta). Respeita a flag "Som".
 */
kit_err_t kit_audio_sfx_impl(kit_sfx_t sfx);

/**
 * Suspende (true) ou reativa (false) o áudio. Suspenso, bipes e efeitos novos
 * são descartados silenciosamente; o que já estava na fila termina e o codec/PA
 * desliga por ociosidade. O Runtime usa isso quando a tela entra em repouso.
 */
void kit_audio_suspend(bool suspend);

/**
 * Diagnóstico: re-sonda o ES8311 no I2C, loga o estado do pipeline e toca
 * um tom longo e contínuo (1,5 s). Usado pela linha "SOM" da tela Testes.
 */
kit_err_t kit_audio_selftest_impl(void);

#ifdef __cplusplus
}
#endif
