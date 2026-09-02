#pragma once

#include "kit_api.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Catálogo de Tools no dispositivo. Lê o `index.json` público por HTTPS
 * (GitHub Pages), lista as Tools e baixa/instala pacotes `.kit` pelo Wi-Fi,
 * reaproveitando a pipeline do `kit_pkg` / `kit_tool_manager` (unzip +
 * SHA-256 + recarga de catálogo). Integridade por SHA-256; assinatura
 * Ed25519 (ADR-0012) fica para depois.
 *
 * Todo o trabalho de rede/instalação roda numa task própria (`kit_catalog`).
 * A UI dispara `refresh`/`install`/`uninstall` e acompanha por `state` +
 * callback (que roda na task de rede — marshalar para o LVGL).
 */

#define KIT_CATALOG_URL "https://jcrvlh.github.io/kit-tools/index.json"
#define KIT_CATALOG_MAX 24

typedef enum {
    KIT_CAT_NOT_INSTALLED = 0,
    KIT_CAT_INSTALLED,       // instalada, version_code do catálogo <= instalado
    KIT_CAT_UPDATE,          // instalada, o catálogo tem version_code maior
} kit_cat_install_t;

typedef struct {
    char     id[40];
    char     name[32];
    char     version[16];
    uint32_t version_code;
    char     author[32];
    char     description[160];
    char     tier[12];          // "official" / "community"
    uint32_t size;              // package.size em bytes (0 = desconhecido)
    char     sha256[65];        // "" = ausente no catálogo
    char     url[256];          // package.url do .kit
    kit_cat_install_t install;
    uint32_t installed_vc;      // version_code instalado (0 se não instalada)
} kit_catalog_entry_t;

typedef enum {
    KIT_CAT_IDLE = 0,
    KIT_CAT_FETCHING,
    KIT_CAT_READY,
    KIT_CAT_FETCH_ERR,
    KIT_CAT_WORKING,        // baixando/instalando/removendo
    KIT_CAT_WORK_OK,
    KIT_CAT_WORK_ERR,
    KIT_CAT_OFFLINE,        // sem Wi-Fi
} kit_catalog_state_t;

typedef void (*kit_catalog_cb_t)(kit_catalog_state_t state, void *user);

kit_err_t kit_catalog_init(void);
void      kit_catalog_set_cb(kit_catalog_cb_t cb, void *user);
kit_catalog_state_t kit_catalog_get_state(void);

/** Baixa e parseia o index.json. Requer Wi-Fi. Assíncrono. */
kit_err_t kit_catalog_refresh(void);

uint32_t  kit_catalog_get_count(void);
kit_err_t kit_catalog_get_entry(uint32_t i, kit_catalog_entry_t *out);

/** Baixa o .kit e instala/atualiza a Tool `id`. Assíncrono. */
kit_err_t kit_catalog_install(const char *id);

/** Remove a Tool `id` (delega ao kit_tool_manager). Assíncrono. */
kit_err_t kit_catalog_uninstall(const char *id);

/** Progresso da operação atual: 0..100, ou -1 se indeterminado. */
int         kit_catalog_progress(void);
const char *kit_catalog_last_error(void);

#ifdef __cplusplus
}
#endif
