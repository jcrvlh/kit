#include "kit_ota.h"
#include "kit_network.h"
#include "kit_power.h"

#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_ota_ops.h"
#include "esp_app_desc.h"
#include "esp_partition.h"
#include "cJSON.h"
#include "mbedtls/sha256.h"

static const char *TAG = "KIT_OTA";

#define MANIFEST_BUF_MAX  (16 * 1024)   // firmware.json (hoje <1 KB) — folga larga
#define FW_MIN_BYTES      (256 * 1024)  // piso de sanidade de um app do KIT
#define FW_MAX_BYTES      (0x300000)    // tamanho do slot ota (partitions.csv)
#define FW_PROJECT_NAME   "kit_core"    // project() em firmware/CMakeLists.txt

// ---------------------------------------------------------------------------
// Estado
// ---------------------------------------------------------------------------

static SemaphoreHandle_t s_lock;
static TaskHandle_t      s_worker;

static kit_ota_release_t s_release;
static bool              s_have_release;

static volatile kit_ota_state_t s_state = KIT_OTA_IDLE;
static volatile int  s_progress = -1;
static char          s_err[80];

static kit_ota_cb_t s_cb;
static void        *s_cb_arg;

typedef enum { REQ_NONE = 0, REQ_CHECK, REQ_APPLY } req_t;
static volatile req_t s_req;

static void set_state(kit_ota_state_t st)
{
    s_state = st;
    kit_ota_cb_t cb = s_cb;
    if (cb) cb(st, s_cb_arg);
}

static void fail(kit_ota_state_t st, const char *msg)
{
    strlcpy(s_err, msg, sizeof(s_err));
    ESP_LOGW(TAG, "%s", msg);
    set_state(st);
}

// ---------------------------------------------------------------------------
// Versão (semver simples: MAJOR.MINOR.PATCH)
// ---------------------------------------------------------------------------

typedef struct { int major, minor, patch; bool ok; } semver_t;

static semver_t semver_parse(const char *s)
{
    semver_t v = {0};
    if (!s || !*s) return v;
    char *end = NULL;
    long a = strtol(s, &end, 10);
    if (end == s) return v;                 // não começa com dígito -> desconhecida
    v.major = (int)a;
    if (*end == '.') { s = end + 1; v.minor = (int)strtol(s, &end, 10); }
    if (*end == '.') { s = end + 1; v.patch = (int)strtol(s, &end, 10); }
    v.ok = true;
    return v;
}

// > 0 se `a` for mais nova que `b`; 0 se igual; < 0 se mais velha.
static int semver_cmp(semver_t a, semver_t b)
{
    if (a.major != b.major) return a.major - b.major;
    if (a.minor != b.minor) return a.minor - b.minor;
    return a.patch - b.patch;
}

static const char *running_version(void)
{
    const esp_app_desc_t *d = esp_app_get_description();
    return (d && d->version[0]) ? d->version : "?";
}

// ---------------------------------------------------------------------------
// HTTP  (mesmo desenho do kit_catalog: progresso por Content-Length, cert bundle)
// ---------------------------------------------------------------------------

typedef struct {
    char  *buf;               // != NULL -> acumula em memória (manifesto)
    size_t cap;
    size_t len;
    bool   overflow;

    esp_ota_handle_t ota;     // != 0 -> grava no slot inativo (firmware)
    bool   ota_active;
    esp_err_t ota_err;

    mbedtls_sha256_context sha;
    bool   do_sha;

    size_t expected;          // Content-Length (0 = desconhecido)
    size_t written;
} dl_ctx_t;

static esp_err_t on_http_event(esp_http_client_event_t *evt)
{
    dl_ctx_t *d = evt->user_data;

    if (evt->event_id == HTTP_EVENT_ON_HEADER) {
        if (d && d->expected == 0 && evt->header_key &&
            strcasecmp(evt->header_key, "Content-Length") == 0 && evt->header_value) {
            long cl = strtol(evt->header_value, NULL, 10);
            if (cl > 0) d->expected = (size_t)cl;
        }
        return ESP_OK;
    }

    if (evt->event_id != HTTP_EVENT_ON_DATA) return ESP_OK;

    if (d->buf) {
        if (d->len + (size_t)evt->data_len < d->cap) {
            memcpy(d->buf + d->len, evt->data, evt->data_len);
        } else {
            d->overflow = true;
        }
    } else if (d->ota_active && d->ota_err == ESP_OK) {
        d->ota_err = esp_ota_write(d->ota, evt->data, evt->data_len);
        if (d->ota_err == ESP_OK) d->written += evt->data_len;
    }
    if (d->do_sha) mbedtls_sha256_update(&d->sha, evt->data, evt->data_len);
    d->len += evt->data_len;

    if (d->expected) {
        int p = (int)((uint64_t)d->len * 100 / d->expected);
        s_progress = p > 100 ? 100 : p;
    }
    return ESP_OK;
}

// GET. Devolve KIT_OK só com HTTP 200 e sem overflow / erro de gravação.
static kit_err_t http_get(const char *url, dl_ctx_t *ctx)
{
    esp_http_client_config_t cfg = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 20000,
        .user_agent = "KIT/0.2 (+ota)",
        .event_handler = on_http_event,
        .user_data = ctx,
        .buffer_size = 2048,
        .buffer_size_tx = 1024,
        .keep_alive_enable = true,
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
    if (ctx->overflow)          return KIT_ERR_NO_MEM;
    if (ctx->ota_err != ESP_OK) return KIT_FAIL;
    return KIT_OK;
}

// ---------------------------------------------------------------------------
// Manifesto
// ---------------------------------------------------------------------------

static void str_field(cJSON *obj, const char *key, char *dst, size_t cap)
{
    cJSON *j = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsString(j) && j->valuestring) strlcpy(dst, j->valuestring, cap);
    else dst[0] = '\0';
}

// As fontes do KIT só têm Latin-1; `notes` vem de uma mensagem de tag e pode
// trazer travessão/aspas curvas (viram retângulo). Troca byte alto ou de
// controle por '.' in-place.
static void ascii_only(char *s)
{
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c < 0x20 || c >= 0x7f) *s = (c == '\n' || c == '\t') ? ' ' : '.';
    }
}

static kit_err_t parse_manifest(const char *json, size_t len)
{
    cJSON *root = cJSON_ParseWithLength(json, len);
    if (!root) return KIT_FAIL;

    kit_ota_release_t r;
    memset(&r, 0, sizeof(r));
    str_field(root, "version", r.version, sizeof(r.version));
    str_field(root, "notes",   r.notes,   sizeof(r.notes));
    str_field(root, "url",     r.url,     sizeof(r.url));
    str_field(root, "sha256",  r.sha256,  sizeof(r.sha256));
    ascii_only(r.notes);

    cJSON *vc = cJSON_GetObjectItemCaseSensitive(root, "version_code");
    r.version_code = cJSON_IsNumber(vc) && vc->valueint > 0 ? (uint32_t)vc->valueint : 0;
    cJSON *sz = cJSON_GetObjectItemCaseSensitive(root, "size");
    r.size = cJSON_IsNumber(sz) && sz->valueint > 0 ? (uint32_t)sz->valueint : 0;

    cJSON_Delete(root);

    if (r.version[0] == '\0' || r.url[0] == '\0') return KIT_FAIL;

    semver_t mine  = semver_parse(running_version());
    semver_t theirs = semver_parse(r.version);
    // Sem versão local legível (build de dev) => qualquer release conta como nova.
    r.newer = !mine.ok || (theirs.ok && semver_cmp(theirs, mine) > 0);

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_release = r;
    s_have_release = true;
    xSemaphoreGive(s_lock);

    ESP_LOGI(TAG, "manifesto: v%s (rodando %s) -> %s",
             r.version, running_version(), r.newer ? "atualizar" : "em dia");
    return KIT_OK;
}

static void do_check(void)
{
    if (!kit_network_is_connected()) { fail(KIT_OTA_OFFLINE, "Sem Wi-Fi"); return; }

    set_state(KIT_OTA_CHECKING);
    s_progress = -1;

    char *buf = heap_caps_malloc(MANIFEST_BUF_MAX, MALLOC_CAP_SPIRAM);
    if (!buf) { fail(KIT_OTA_ERR, "Sem memoria"); return; }

    dl_ctx_t ctx = { .buf = buf, .cap = MANIFEST_BUF_MAX };
    kit_err_t r = http_get(KIT_OTA_MANIFEST_URL, &ctx);
    if (r == KIT_OK) {
        buf[ctx.len] = '\0';
        r = parse_manifest(buf, ctx.len);
    }
    free(buf);

    if (r != KIT_OK) { fail(KIT_OTA_ERR, "Nao consegui ler o manifesto"); return; }

    s_err[0] = '\0';
    set_state(s_release.newer ? KIT_OTA_AVAILABLE : KIT_OTA_UP_TO_DATE);
}

// ---------------------------------------------------------------------------
// Aplicação
// ---------------------------------------------------------------------------

static void do_apply(void)
{
    kit_ota_release_t r;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    r = s_release;
    bool have = s_have_release;
    xSemaphoreGive(s_lock);

    if (!have || r.url[0] == '\0') { fail(KIT_OTA_ERR, "Rode 'Procurar' primeiro"); return; }
    if (!kit_power_is_usb_connected()) { fail(KIT_OTA_NO_POWER, "Conecte o cabo USB"); return; }
    if (!kit_network_is_connected())   { fail(KIT_OTA_OFFLINE, "Sem Wi-Fi"); return; }

    // Segura o repouso: a tela não pode apagar (nem o rádio cair) no meio da gravação.
    kit_power_keep_awake_impl(true);
    set_state(KIT_OTA_DOWNLOADING);
    s_progress = r.size ? 0 : -1;

    const esp_partition_t *dst = esp_ota_get_next_update_partition(NULL);
    if (!dst) { kit_power_keep_awake_impl(false); fail(KIT_OTA_ERR, "Sem slot OTA livre"); return; }
    ESP_LOGI(TAG, "baixando %s -> slot '%s'", r.url, dst->label);

    esp_ota_handle_t h = 0;
    esp_err_t e = esp_ota_begin(dst, OTA_SIZE_UNKNOWN, &h);
    if (e != ESP_OK) {
        kit_power_keep_awake_impl(false);
        fail(KIT_OTA_ERR, "esp_ota_begin falhou");
        return;
    }

    dl_ctx_t ctx = { .ota = h, .ota_active = true, .expected = r.size };
    bool want_sha = (strlen(r.sha256) == 64);
    if (want_sha) { mbedtls_sha256_init(&ctx.sha); mbedtls_sha256_starts(&ctx.sha, 0); ctx.do_sha = true; }

    kit_err_t hr = http_get(r.url, &ctx);

    if (hr != KIT_OK || ctx.written < FW_MIN_BYTES || ctx.written > FW_MAX_BYTES) {
        if (want_sha) mbedtls_sha256_free(&ctx.sha);
        esp_ota_abort(h);
        kit_power_keep_awake_impl(false);
        fail(KIT_OTA_ERR, "Falha no download");
        return;
    }

    if (want_sha) {
        uint8_t digest[32];
        mbedtls_sha256_finish(&ctx.sha, digest);
        mbedtls_sha256_free(&ctx.sha);
        char hex[65];
        for (int i = 0; i < 32; i++) snprintf(hex + i * 2, 3, "%02x", digest[i]);
        if (strcasecmp(hex, r.sha256) != 0) {
            ESP_LOGE(TAG, "SHA-256 nao confere: %s != %s", hex, r.sha256);
            esp_ota_abort(h);
            kit_power_keep_awake_impl(false);
            fail(KIT_OTA_ERR, "SHA-256 nao confere");
            return;
        }
        ESP_LOGI(TAG, "SHA-256 da imagem OK");
    } else {
        ESP_LOGW(TAG, "manifesto sem SHA-256 — so a integridade do TLS + imagem");
    }

    set_state(KIT_OTA_APPLYING);
    s_progress = -1;

    e = esp_ota_end(h);   // valida header, checksum e (se houver) secure boot
    if (e != ESP_OK) {
        kit_power_keep_awake_impl(false);
        fail(KIT_OTA_ERR, e == ESP_ERR_OTA_VALIDATE_FAILED ? "Imagem invalida" : "esp_ota_end falhou");
        return;
    }

    // Confere que a imagem é mesmo do KIT antes de armar o boot.
    esp_app_desc_t desc = {0};
    if (esp_ota_get_partition_description(dst, &desc) != ESP_OK ||
        strcmp(desc.project_name, FW_PROJECT_NAME) != 0) {
        ESP_LOGE(TAG, "imagem nao e do KIT (project_name='%s')",
                 desc.project_name[0] ? desc.project_name : "?");
        esp_partition_erase_range(dst, 0, 64 * 1024);   // invalida o slot
        kit_power_keep_awake_impl(false);
        fail(KIT_OTA_ERR, "Imagem nao e do KIT");
        return;
    }

    e = esp_ota_set_boot_partition(dst);
    if (e != ESP_OK) {
        kit_power_keep_awake_impl(false);
        fail(KIT_OTA_ERR, "set_boot_partition falhou");
        return;
    }

    kit_power_keep_awake_impl(false);
    s_err[0] = '\0';
    ESP_LOGI(TAG, "firmware v%s pronto no slot '%s' — reiniciar para aplicar",
             desc.version, dst->label);
    set_state(KIT_OTA_DONE);
}

// ---------------------------------------------------------------------------
// Worker
// ---------------------------------------------------------------------------

static void worker_task(void *arg)
{
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        req_t req = s_req;
        s_req = REQ_NONE;
        switch (req) {
        case REQ_CHECK: do_check(); break;
        case REQ_APPLY: do_apply(); break;
        default: break;
        }
    }
}

static kit_err_t post(req_t req)
{
    if (!s_worker) return KIT_FAIL;
    if (s_state == KIT_OTA_CHECKING || s_state == KIT_OTA_DOWNLOADING ||
        s_state == KIT_OTA_APPLYING) return KIT_FAIL;
    s_req = req;
    xTaskNotifyGive(s_worker);
    return KIT_OK;
}

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------

kit_err_t kit_ota_init(void)
{
    if (s_lock) return KIT_OK;
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return KIT_ERR_NO_MEM;
    if (xTaskCreate(worker_task, "kit_ota", 8192, NULL, 4, &s_worker) != pdPASS) {
        return KIT_ERR_NO_MEM;
    }
    return KIT_OK;
}

void kit_ota_set_cb(kit_ota_cb_t cb, void *user) { s_cb = cb; s_cb_arg = user; }
kit_ota_state_t kit_ota_get_state(void) { return s_state; }
int         kit_ota_progress(void)   { return s_progress; }
const char *kit_ota_last_error(void) { return s_err; }

kit_err_t kit_ota_check(void) { return post(REQ_CHECK); }
kit_err_t kit_ota_apply(void) { return post(REQ_APPLY); }

bool kit_ota_get_release(kit_ota_release_t *out)
{
    if (!out) return false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool have = s_have_release;
    if (have) *out = s_release;
    xSemaphoreGive(s_lock);
    return have;
}

void kit_ota_current_version(char *buf, size_t len)
{
    if (buf && len) strlcpy(buf, running_version(), len);
}

void kit_ota_reboot(void)
{
    ESP_LOGI(TAG, "reiniciando para aplicar o firmware novo");
    vTaskDelay(pdMS_TO_TICKS(120));
    esp_restart();
}
