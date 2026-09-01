#pragma once

#include "kit_api.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// "Modo pen drive" — expõe o cartão microSD do KIT ao computador como um
// dispositivo USB Mass Storage. O ESP32-S3 tem um único PHY USB, então
// enquanto isso o console USB-Serial/JTAG (e o Web Installer) ficam fora do ar.
//
// Fluxo: kit_usb_msc_enter() solta o cartão do FATFS do KIT e o entrega ao
// TinyUSB. Não há volta limpa ao normal — o jeito seguro de sair é reiniciar
// (kit_usb_msc_exit_reboot), que remonta o cartão e reescaneia as Tools.

// Requer um cartão já montado (kit_storage_sd_is_mounted). Devolve
// KIT_ERR_NOT_FOUND se não houver cartão, KIT_ERR_STORAGE em falha de USB/SD
// (nesse caso tenta remontar o cartão no KIT antes de retornar).
kit_err_t kit_usb_msc_enter(void);

// Sai do modo reiniciando o dispositivo. Não retorna.
void kit_usb_msc_exit_reboot(void);

// true depois de um kit_usb_msc_enter() bem-sucedido.
bool kit_usb_msc_is_active(void);

// true enquanto o computador estiver com o cartão montado (ainda não ejetado).
bool kit_usb_msc_host_connected(void);

// Capacidade do cartão exposto, em MB (0 se a leitura do cartão cru falhou).
uint32_t kit_usb_msc_card_mb(void);

// true quando o host USB terminou a enumeração (tud_mounted). Se isso nunca
// vira true, o problema é de USB/PHY, não do cartão.
bool kit_usb_msc_usb_ready(void);

#ifdef __cplusplus
}
#endif
