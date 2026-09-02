#include "kit_catalog.h"
#include "kit_network.h"
#include "kit_tool_manager.h"

#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "mbedtls/sha256.h"

static const char *TAG = "KIT_CATALOG";

#define INDEX_BUF_MAX   (48 * 1024)   // index.json (hoje ~2 KB) — folga larga
#define KIT_MAX_BYTES   (768 * 1024)  // teto de um .kit baixado
#define KIT_TMP_PATH    "/sdcard/tools/.download.kit"

// ---------------------------------------------------------------------------
// Estado
// ---------------------------------------------------------------------------

static SemaphoreHandle_t s_lock;
static TaskHandle_t      s_worker;

static kit_catalog_entry_t s_entries[KIT_CATALOG_MAX];
static uint32_t            s_n;

static volatile kit_catalog_state_t s_state = KIT_CAT_IDLE;
static volatile int  s_progress = -1;
static char          s_err[80];

static kit_catalog_cb_t s_cb;
static void            *s_cb_arg;

typedef enum { REQ_NONE = 0, REQ_REFRESH, REQ_INSTALL, REQ_UNINSTALL } req_t;
static volatile req_t s_req;
static char           s_req_id[40];

static void set_state(kit_catalog_state_t st)
{
    s_state = st;
    kit_catalog_cb_t cb = s_cb;
    if (cb) cb(st, s_cb_arg);
}

static void fail(kit_catalog_state_t st, const char *msg)
{
    strlcpy(s_err, msg, sizeof(s_err));
    ESP_LOGW(TAG, "%s", msg);
    set_state(st);
}

// ---------------------------------------------------------------------------
// HTTP
// ---------------------------------------------------------------------------

typedef struct {
    FILE  *f;                 // != NULL -> grava em arquivo
    char  *buf;               // != NULL -> acumula em memória
    size_t cap;
    size_t len;
    bool   overflow;
    mbedtls_sha256_context sha;
    bool   do_sha;
    size_t expected;          // para o progresso (0 = desconhecido)
} dl_ctx_t;

static esp_err_t on_http_event(esp_http_client_event_t *evt)
{
    if (evt->event_id != HTTP_EVENT_ON_DATA) return ESP_OK;
    dl_ctx_t *d = evt->user_data;

    if (d->f) {
        if (fwrite(evt->data, 1, evt->data_len, d->f) != (size_t)evt->data_len) {
            d->overflow = true;
        }
    } else if (d->buf) {
        if (d->len + (size_t)evt->data_len < d->cap) {
            memcpy(d->buf + d->len, evt->data, evt->data_len);
        } else {
            d->overflow = true;
        }
    }
    if (d->do_sha) mbedtls_sha256_update(&d->sha, evt->data, evt->data_len);
    d->len += evt->data_len;

    if (d->expected) {
        int p = (int)((uint64_t)d->len * 100 / d->expected);
        s_progress = p > 100 ? 100 : p;
    }
    return ESP_OK;
}

// Executa o GET. Devolve KIT_OK só com HTTP 200 e sem overflow.
static kit_err_t http_get(const char *url, dl_ctx_t *ctx)
{
    esp_http_client_config_t cfg = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 20000,
        .user_agent = "KIT/0.1 (+catalog)",
        .event_handler = on_http_event,
        .user_data = ctx,
        .buffer_size = 2048,
        .buffer_size_tx = 1024,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) return KIT_FAIL;

    esp_err_t err = esp_http_client_perform(c);
    int status = esp_http_client_get_status_code(c);
    esp_http_client_cleanup(c);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "http %s: %s", url, esp_err_to_name(err));
        return KIT_FAIL;
    }
    if (status != 200) {
        ESP_LOGW(TAG, "http %s: status %d", url, status);
        return KIT_FAIL;
    }
    if (ctx->overflow) {
        ESP_LOGW(TAG, "http %s: resposta grande demais", url);
        return KIT_ERR_NO_MEM;
    }
    return KIT_OK;
}

// ---------------------------------------------------------------------------
// index.json
// ---------------------------------------------------------------------------

static void str_field(cJSON *obj, const char *key, char *dst, size_t cap)
{
    cJSON *j = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsString(j) && j->valuestring) strlcpy(dst, j->valuestring, cap);
    else dst[0] = '\0';
}

// Cruza cada entrada do catálogo com o que está instalado (kit_tool_manager).
static void cross_reference(void)
{
    uint32_t inst_n = kit_tool_manager_get_count();
    for (uint32_t i = 0; i < s_n; i++) {
        kit_catalog_entry_t *e = &s_entries[i];
        e->install = KIT_CAT_NOT_INSTALLED;
        e->installed_vc = 0;
        for (uint32_t k = 0; k < inst_n; k++) {
            kit_tool_entry_t te;
            if (kit_tool_manager_get_entry(k, &te) != KIT_OK) continue;
            if (strcmp(te.id, e->id) != 0) continue;
            e->installed_vc = te.version_code;
            e->install = (e->version_code > te.version_code)
                       ? KIT_CAT_UPDATE : KIT_CAT_INSTALLED;
            break;
        }
    }
}

static kit_err_t parse_index(const char *json, size_t len)
{
    cJSON *root = cJSON_ParseWithLength(json, len);
    if (!root) return KIT_FAIL;

    cJSON *tools = cJSON_GetObjectItemCaseSensitive(root, "tools");
    if (!cJSON_IsArray(tools)) { cJSON_Delete(root); return KIT_FAIL; }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_n = 0;
    cJSON *t;
    cJSON_ArrayForEach(t, tools) {
        if (!cJSON_IsObject(t) || s_n >= KIT_CATALOG_MAX) continue;
        kit_catalog_entry_t *e = &s_entries[s_n];
        memset(e, 0, sizeof(*e));

        str_field(t, "id",          e->id,          sizeof(e->id));
        str_field(t, "name",        e->name,        sizeof(e->name));
        str_field(t, "version",     e->version,     sizeof(e->version));
        str_field(t, "author",      e->author,      sizeof(e->author));
        str_field(t, "description", e->description, sizeof(e->description));
        str_field(t, "tier",        e->tier,        sizeof(e->tier));
        if (e->id[0] == '\0' || e->name[0] == '\0') continue;

        cJSON *vc = cJSON_GetObjectItemCaseSensitive(t, "version_code");
        e->version_code = cJSON_IsNumber(vc) && vc->valueint > 0 ? (uint32_t)vc->valueint : 0;

        cJSON *pkg = cJSON_GetObjectItemCaseSensitive(t, "package");
        if (cJSON_IsObject(pkg)) {
            str_field(pkg, "url",    e->url,    sizeof(e->url));
            str_field(pkg, "sha256", e->sha256, sizeof(e->sha256));
            cJSON *sz = cJSON_GetObjectItemCaseSensitive(pkg, "size");
            e->size = cJSON_IsNumber(sz) && sz->valueint > 0 ? (uint32_t)sz->valueint : 0;
        }
        if (e->url[0] == '\0') continue;   // sem pacote, sem entrada

        s_n++;
    }
    xSemaphoreGive(s_lock);

    cJSON_Delete(root);
    cross_reference();
    ESP_LOGI(TAG, "catálogo: %lu Tool(s)", (unsigned long)s_n);
    return KIT_OK;
}

static void do_refresh(void)
{
    if (!kit_network_is_connected()) { fail(KIT_CAT_OFFLINE, "Sem Wi-Fi"); return; }

    set_state(KIT_CAT_FETCHING);
    s_progress = -1;

    char *buf = heap_caps_malloc(INDEX_BUF_MAX, MALLOC_CAP_SPIRAM);
    if (!buf) { fail(KIT_CAT_FETCH_ERR, "Sem memória"); return; }

    dl_ctx_t ctx = { .buf = buf, .cap = INDEX_BUF_MAX };
    kit_err_t r = http_get(KIT_CATALOG_URL, &ctx);
    if (r == KIT_OK) {
        buf[ctx.len] = '\0';
        r = parse_index(buf, ctx.len);
    }
    free(buf);

    if (r == KIT_OK) { s_err[0] = '\0'; set_state(KIT_CAT_READY); }
    else             { fail(KIT_CAT_FETCH_ERR, "Não consegui ler o catálogo"); }
}

// ---------------------------------------------------------------------------
// Instalação
// ---------------------------------------------------------------------------

static kit_err_t entry_by_id(const char *id, kit_catalog_entry_t *out)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    kit_err_t r = KIT_ERR_NOT_FOUND;
    for (uint32_t i = 0; i < s_n; i++) {
        if (strcmp(s_entries[i].id, id) == 0) { *out = s_entries[i]; r = KIT_OK; break; }
    }
    xSemaphoreGive(s_lock);
    return r;
}

static void do_install(const char *id)
{
    kit_catalog_entry_t e;
    if (entry_by_id(id, &e) != KIT_OK) { fail(KIT_CAT_WORK_ERR, "Tool não está no catálogo"); return; }

    if (!kit_network_is_connected()) { fail(KIT_CAT_OFFLINE, "Sem Wi-Fi"); return; }

    set_state(KIT_CAT_WORKING);
    s_progress = 0;
    ESP_LOGI(TAG, "baixando '%s' de %s", id, e.url);

    FILE *f = fopen(KIT_TMP_PATH, "wb");
    if (!f) { fail(KIT_CAT_WORK_ERR, "Cartão indisponível"); return; }

    dl_ctx_t ctx = { .f = f, .expected = e.size };
    bool want_sha = (strlen(e.sha256) == 64);
    if (want_sha) { mbedtls_sha256_init(&ctx.sha); mbedtls_sha256_starts(&ctx.sha, 0); ctx.do_sha = true; }

    kit_err_t r = http_get(e.url, &ctx);
    fclose(f);

    if (r != KIT_OK || ctx.len == 0 || ctx.len > KIT_MAX_BYTES) {
        if (want_sha) mbedtls_sha256_free(&ctx.sha);
        unlink(KIT_TMP_PATH);
        fail(KIT_CAT_WORK_ERR, "Falha no download");
        return;
    }

    if (want_sha) {
        uint8_t digest[32];
        mbedtls_sha256_finish(&ctx.sha, digest);
        mbedtls_sha256_free(&ctx.sha);
        char hex[65];
        for (int i = 0; i < 32; i++) snprintf(hex + i * 2, 3, "%02x", digest[i]);
        if (strcasecmp(hex, e.sha256) != 0) {
            ESP_LOGE(TAG, "SHA-256 não confere: %s != %s", hex, e.sha256);
            unlink(KIT_TMP_PATH);
            fail(KIT_CAT_WORK_ERR, "Pacote corrompido (SHA-256)");
            return;
        }
        ESP_LOGI(TAG, "SHA-256 do pacote OK");
    } else {
        ESP_LOGW(TAG, "catálogo sem SHA-256 para '%s' — instalando só com integridade do transporte", id);
    }

    s_progress = -1;   // extração: indeterminado
    if (kit_tool_manager_install(KIT_TMP_PATH, e.id) != KIT_OK) {
        fail(KIT_CAT_WORK_ERR, "Falha ao instalar o pacote");
        return;
    }

    cross_reference();
    s_err[0] = '\0';
    ESP_LOGI(TAG, "'%s' instalada (v%s)", e.name, e.version);
    set_state(KIT_CAT_WORK_OK);
}

static void do_uninstall(const char *id)
{
    set_state(KIT_CAT_WORKING);
    s_progress = -1;
    kit_err_t r = kit_tool_manager_uninstall(id);
    cross_reference();
    if (r == KIT_OK) { s_err[0] = '\0'; set_state(KIT_CAT_WORK_OK); }
    else             { fail(KIT_CAT_WORK_ERR, "Não consegui remover"); }
}

// ---------------------------------------------------------------------------
// Worker
// ---------------------------------------------------------------------------

static void worker_task(void *arg)
{
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        req_t req = s_req;
        s_req = REQ_NONE;
        switch (req) {
        case REQ_REFRESH:   do_refresh();              break;
        case REQ_INSTALL:   do_install(s_req_id);      break;
        case REQ_UNINSTALL: do_uninstall(s_req_id);    break;
        default: break;
        }
    }
}

static kit_err_t post(req_t req, const char *id)
{
    if (!s_worker) return KIT_FAIL;
    if (s_state == KIT_CAT_FETCHING || s_state == KIT_CAT_WORKING) return KIT_FAIL;
    if (id) strlcpy(s_req_id, id, sizeof(s_req_id));
    s_req = req;
    xTaskNotifyGive(s_worker);
    return KIT_OK;
}

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------

kit_err_t kit_catalog_init(void)
{
    if (s_lock) return KIT_OK;
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return KIT_ERR_NO_MEM;
    if (xTaskCreate(worker_task, "kit_catalog", 6144, NULL, 4, &s_worker) != pdPASS) {
        return KIT_ERR_NO_MEM;
    }
    return KIT_OK;
}

void kit_catalog_set_cb(kit_catalog_cb_t cb, void *user) { s_cb = cb; s_cb_arg = user; }
kit_catalog_state_t kit_catalog_get_state(void) { return s_state; }
int         kit_catalog_progress(void)   { return s_progress; }
const char *kit_catalog_last_error(void) { return s_err; }

kit_err_t kit_catalog_refresh(void)          { return post(REQ_REFRESH, NULL); }
kit_err_t kit_catalog_install(const char *id)   { return post(REQ_INSTALL, id); }
kit_err_t kit_catalog_uninstall(const char *id) { return post(REQ_UNINSTALL, id); }

uint32_t kit_catalog_get_count(void) { return s_n; }

kit_err_t kit_catalog_get_entry(uint32_t i, kit_catalog_entry_t *out)
{
    if (!out) return KIT_ERR_INVALID_ARG;
    kit_err_t r = KIT_ERR_NOT_FOUND;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (i < s_n) { *out = s_entries[i]; r = KIT_OK; }
    xSemaphoreGive(s_lock);
    return r;
}
