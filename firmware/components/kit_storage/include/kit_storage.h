#pragma once

#include "kit_api.h"

#ifdef __cplusplus
extern "C" {
#endif

kit_err_t kit_storage_init(void);
uint32_t  kit_storage_get_free_bytes(void);

// Capacidade total e espaço livre da partição LittleFS (/tools), em bytes.
kit_err_t kit_storage_get_info(uint32_t *total_bytes, uint32_t *free_bytes);

// --- Cartão microSD (armazenamento secundário, opcional) --------------------
// Barramento SDMMC em modo 1-bit: CLK=GPIO2, CMD=GPIO1, D0=GPIO3.
// Ponto de montagem: /sdcard. Cartões FAT32 ou exFAT.
#define KIT_SD_MOUNT_POINT "/sdcard"

// Tenta detectar e montar um cartão microSD. A ausência de cartão não é um
// erro do sistema: devolve KIT_ERR_NOT_FOUND e o KIT segue normalmente.
kit_err_t kit_storage_sd_mount(void);

// Desmonta o cartão e libera o barramento SDMMC.
void      kit_storage_sd_unmount(void);

// true se há um cartão microSD montado em /sdcard.
bool      kit_storage_sd_is_mounted(void);

// Capacidade total e espaço livre do cartão, em bytes. KIT_ERR_NOT_FOUND se
// não houver cartão montado.
kit_err_t kit_storage_sd_info(uint64_t *total_bytes, uint64_t *free_bytes);

// Formata o cartão microSD (FAT) e recria a estrutura de pastas que o KIT
// espera (tools/). APAGA TODOS OS DADOS do cartão. Requer cartão montado;
// bloqueia por alguns segundos. KIT_ERR_NOT_FOUND se não houver cartão.
kit_err_t kit_storage_sd_format(void);

// Implementações para kit_api
kit_err_t kit_storage_set_str_impl(const char *key, const char *value);
kit_err_t kit_storage_get_str_impl(const char *key, char *buffer, size_t max_len);
kit_err_t kit_storage_set_i32_impl(const char *key, int32_t value);
kit_err_t kit_storage_get_i32_impl(const char *key, int32_t *out_value);
FILE     *kit_storage_open_file_impl(const char *filename, const char *mode);

#ifdef __cplusplus
}
#endif
