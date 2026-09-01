#pragma once

#include "kit_api.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Configurações do sistema persistidas em NVS (namespace "kit_sys").
//
// O acesso é feito por um cache em RAM carregado no boot (kit_config_init):
// os getters devolvem o valor em memória (baratos, chamados no loop do Runtime)
// e os setters gravam RAM + NVS (chamados pela UI de Ajustes). Todo o acesso
// acontece na task `main` (loop LVGL + Runtime), então não há trava.

/**
 * Lê todas as chaves de NVS para o cache em RAM. Chamar uma vez no boot,
 * depois de nvs_flash_init(). Se uma chave não existir, assume o padrão.
 */
kit_err_t kit_config_init(void);

// -- Brilho da tela (0..100). Padrão 80. Reaplicado no boot pelo Runtime. --
uint8_t kit_config_get_brightness(void);
void    kit_config_set_brightness(uint8_t percent);

// -- Repouso da tela: segundos de inatividade até apagar o painel.
//    0 = nunca. Padrão 120 (2 min). --
uint32_t kit_config_get_screen_sleep_s(void);
void     kit_config_set_screen_sleep_s(uint32_t seconds);

// -- Desligamento automático: segundos de inatividade até desligar o
//    dispositivo (só quando fora da tomada). 0 = nunca. Padrão 0. --
uint32_t kit_config_get_auto_poweroff_s(void);
void     kit_config_set_auto_poweroff_s(uint32_t seconds);

// -- Som: liga/desliga todos os bipes e efeitos sonoros. Padrão ligado.
//    Lido pela kit_audio a cada bipe/SFX. --
bool kit_config_get_sound_enabled(void);
void kit_config_set_sound_enabled(bool enabled);

// -- Acesso genérico a NVS (namespace "kit_sys"), sem passar pelo cache. --
kit_err_t kit_config_get_u8(const char *key, uint8_t *out_val, uint8_t default_val);
kit_err_t kit_config_set_u8(const char *key, uint8_t val);
kit_err_t kit_config_get_u32(const char *key, uint32_t *out_val, uint32_t default_val);
kit_err_t kit_config_set_u32(const char *key, uint32_t val);

#ifdef __cplusplus
}
#endif
