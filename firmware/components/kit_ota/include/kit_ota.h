#pragma once

#include "kit_api.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Atualização de firmware pela internet (OTA).
 *
 * Consulta um manifesto JSON estático por HTTPS (GitHub Pages, cert bundle CMN),
 * compara a versão anunciada com a que está rodando e, a pedido da UI, baixa o
 * `.bin` de uma GitHub Release, grava no slot OTA inativo (streaming +
 * SHA-256 incremental) e agenda o boot no slot novo. O bootloader já reverte
 * sozinho se o firmware novo não subir o Runtime (ver main.c + ADR-0007).
 *
 * Integridade por HTTPS + SHA-256 do manifesto. Assinatura Ed25519 / Secure
 * Boot ficam para depois (ADR-0012 / ADR-0013).
 *
 * Todo o trabalho de rede/gravação roda numa task própria (`kit_ota`). A UI
 * dispara `check`/`apply` e acompanha por `state` + callback (que roda na task
 * de rede — marshalar para o LVGL).
 */

#define KIT_OTA_MANIFEST_URL "https://jcrvlh.github.io/kit/firmware.json"

typedef enum {
    KIT_OTA_IDLE = 0,
    KIT_OTA_CHECKING,      // baixando/parseando o manifesto
    KIT_OTA_UP_TO_DATE,    // manifesto lido, nada mais novo
    KIT_OTA_AVAILABLE,     // há versão para instalar — ver kit_ota_get_release()
    KIT_OTA_DOWNLOADING,   // baixando + gravando no slot inativo
    KIT_OTA_APPLYING,      // validando a imagem / set_boot_partition
    KIT_OTA_DONE,          // slot novo armado; a UI oferece "Reiniciar agora"
    KIT_OTA_OFFLINE,       // sem Wi-Fi
    KIT_OTA_NO_POWER,      // sem cabo USB (obrigatório para aplicar)
    KIT_OTA_ERR,           // ver kit_ota_last_error()
} kit_ota_state_t;

typedef struct {
    char     version[16];      // "0.2.0"
    uint32_t version_code;      // 200 (monotônico, opcional no manifesto)
    char     notes[160];        // linha curta de novidades (ASCII)
    char     url[256];          // .bin da Release
    char     sha256[65];        // "" = ausente no manifesto
    uint32_t size;              // bytes (0 = desconhecido)
    bool     newer;             // versão > a que está rodando (semver)
} kit_ota_release_t;

typedef void (*kit_ota_cb_t)(kit_ota_state_t state, void *user);

kit_err_t   kit_ota_init(void);
void        kit_ota_set_cb(kit_ota_cb_t cb, void *user);
kit_ota_state_t kit_ota_get_state(void);

/** Baixa e parseia o manifesto. Requer Wi-Fi. Assíncrono. */
kit_err_t   kit_ota_check(void);

/** Baixa o `.bin` e grava no slot inativo. Requer Wi-Fi + cabo USB. Assíncrono. */
kit_err_t   kit_ota_apply(void);

/** Copia a última release conhecida. Devolve false se nunca houve um check OK. */
bool        kit_ota_get_release(kit_ota_release_t *out);

/** Progresso da operação atual: 0..100, ou -1 se indeterminado. */
int         kit_ota_progress(void);
const char *kit_ota_last_error(void);

/** Versão do firmware rodando ("0.2.0", ou "?" se o build não tem version.txt). */
void        kit_ota_current_version(char *buf, size_t len);

/** Reinicia o dispositivo (para a UI chamar após KIT_OTA_DONE). Não retorna. */
void        kit_ota_reboot(void);

#ifdef __cplusplus
}
#endif
