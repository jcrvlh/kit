// Rede para as Tools do catálogo — ver kit_net_tool.h.
//
// Máquina de estados: IDLE -> RUNNING (worker no esp_http_client) -> DONE
// (resultado guardado) -> IDLE (kit_net_tool_poll entregou ao callback).
// s_gen muda a cada Tool nova e a cada cancel: um resultado de geração velha
// é descartado em vez de cair no callback de uma Tool que já saiu.

#include "kit_net_tool.h"
#include "kit_network.h"

#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include <string.h>
#include <strings.h>

static const char *TAG = "KIT_NET_TOOL";

#define NT_TIMEOUT_MS   10000
#define NT_MIN_GAP_US   1000000LL   // no máximo 1 pedido por segundo

typedef enum { NT_IDLE = 0, NT_RUNNING, NT_DONE } nt_state_t;

static char s_domains[KIT_NET_TOOL_MAX_DOMAINS][KIT_NET_TOOL_DOMAIN_LEN];
static int  s_n_domains = 0;

static SemaphoreHandle_t s_lock = NULL;
static TaskHandle_t      s_worker = NULL;

static nt_state_t s_state = NT_IDLE;
static uint32_t   s_gen = 0;
static uint32_t   s_req_gen = 0;
static int64_t    s_last_req_us = 0;
static char       s_url[KIT_NET_TOOL_URL_MAX];

static kit_net_callback_t s_cb = NULL;
static void              *s_ud = NULL;

// Resultado pronto (estado DONE)
static kit_err_t s_res = KIT_OK;
static int       s_status = 0;
static char     *s_body = NULL;
static size_t    s_len = 0;

typedef struct {
    char  *buf;
    size_t len;
    bool   overflow;
} nt_dl_t;

static esp_err_t on_http_event(esp_http_client_event_t *evt)
{
    nt_dl_t *d = evt->user_data;
    if (evt->event_id != HTTP_EVENT_ON_DATA || !d) return ESP_OK;
    if (d->len + (size_t)evt->data_len > KIT_NET_TOOL_BODY_MAX) {
        d->overflow = true;
        return ESP_OK;
    }
    memcpy(d->buf + d->len, evt->data, evt->data_len);
    d->len += (size_t)evt->data_len;
    return ESP_OK;
}

static void worker_task(void *arg)
{
    (void)arg;
    static char url[KIT_NET_TOOL_URL_MAX];
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        xSemaphoreTake(s_lock, portMAX_DELAY);
        uint32_t gen = s_req_gen;
        strlcpy(url, s_url, sizeof(url));
        xSemaphoreGive(s_lock);

        kit_err_t res = KIT_OK;
        int status = 0;
        nt_dl_t d = { 0 };
        d.buf = heap_caps_malloc(KIT_NET_TOOL_BODY_MAX + 1, MALLOC_CAP_SPIRAM);
        if (!d.buf) {
            res = KIT_ERR_NO_MEM;
        } else {
            esp_http_client_config_t cfg = {
                .url = url,
                .crt_bundle_attach = esp_crt_bundle_attach,
                .timeout_ms = NT_TIMEOUT_MS,
                .user_agent = "KIT (+tool)",
                .disable_auto_redirect = true,   // redirect poderia sair dos domínios liberados
                .event_handler = on_http_event,
                .user_data = &d,
                .buffer_size = 2048,
                .buffer_size_tx = 1024,
            };
            esp_http_client_handle_t c = esp_http_client_init(&cfg);
            if (!c) {
                res = KIT_FAIL;
            } else {
                esp_err_t err = esp_http_client_perform(c);
                status = esp_http_client_get_status_code(c);
                esp_http_client_cleanup(c);
                if (err == ESP_ERR_HTTP_EAGAIN || err == ESP_ERR_TIMEOUT) res = KIT_ERR_TIMEOUT;
                else if (err != ESP_OK) res = KIT_FAIL;
                else if (d.overflow)    res = KIT_ERR_NO_MEM;
                if (err != ESP_OK)
                    ESP_LOGW(TAG, "GET %s: %s", url, esp_err_to_name(err));
            }
        }
        if (res != KIT_OK && d.buf) { free(d.buf); d.buf = NULL; d.len = 0; }
        if (d.buf) d.buf[d.len] = '\0';

        xSemaphoreTake(s_lock, portMAX_DELAY);
        if (gen == s_gen) {
            s_res = res;
            s_status = status;
            s_body = d.buf;
            s_len = d.len;
            s_state = NT_DONE;
        } else {
            free(d.buf);           // a Tool saiu ou cancelou no meio do caminho
            s_state = NT_IDLE;
        }
        xSemaphoreGive(s_lock);
        ESP_LOGI(TAG, "GET concluído: res=%d status=%d %u B", res, status, (unsigned)d.len);
    }
}

static bool ensure_worker(void)
{
    if (s_worker) return true;
    if (!s_lock) s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return false;
    // Pilha na RAM interna (o esp_http_client/TLS não roda com pilha na PSRAM).
    if (xTaskCreate(worker_task, "kit_net_tool", 6144, NULL, 4, &s_worker) != pdPASS) {
        s_worker = NULL;
        return false;
    }
    return true;
}

// Drop do resultado guardado e do callback. Chamar com s_lock tomado.
static void reset_locked(void)
{
    s_gen++;
    s_cb = NULL;
    s_ud = NULL;
    if (s_state == NT_DONE) {
        free(s_body);
        s_body = NULL;
        s_len = 0;
        s_state = NT_IDLE;
    }
    // NT_RUNNING: o worker vê a geração trocada e descarta sozinho.
}

void kit_net_tool_begin(const char domains[][KIT_NET_TOOL_DOMAIN_LEN], int n)
{
    if (!s_lock) s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    reset_locked();
    if (n > KIT_NET_TOOL_MAX_DOMAINS) n = KIT_NET_TOOL_MAX_DOMAINS;
    for (int i = 0; i < n; i++) strlcpy(s_domains[i], domains[i], KIT_NET_TOOL_DOMAIN_LEN);
    s_n_domains = n < 0 ? 0 : n;
    xSemaphoreGive(s_lock);
}

void kit_net_tool_end(void)
{
    if (!s_lock) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    reset_locked();
    s_n_domains = 0;
    xSemaphoreGive(s_lock);
}

void kit_net_tool_cancel(void)
{
    if (!s_lock) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    reset_locked();
    xSemaphoreGive(s_lock);
}

void kit_net_tool_poll(void)
{
    if (!s_lock || s_state != NT_DONE) return;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_state != NT_DONE) { xSemaphoreGive(s_lock); return; }
    kit_net_callback_t cb = s_cb;
    void *ud = s_ud;
    kit_err_t res = s_res;
    int status = s_status;
    char *body = s_body;
    size_t len = s_len;
    s_body = NULL;
    s_len = 0;
    s_cb = NULL;
    s_ud = NULL;
    s_state = NT_IDLE;
    xSemaphoreGive(s_lock);

    if (cb) cb(res, status, body, len, ud);   // body só vale durante a chamada
    free(body);
}

bool kit_net_tool_is_online(void)
{
    return kit_network_is_connected();
}

// "https://host/..." -> host liberado? Recusa porta, usuário@ e qualquer
// esquema que não seja https.
static bool host_allowed(const char *url)
{
    static const char pfx[] = "https://";
    if (strncasecmp(url, pfx, sizeof(pfx) - 1) != 0) return false;
    const char *h = url + sizeof(pfx) - 1;
    size_t n = strcspn(h, "/?#");
    if (n == 0 || n >= KIT_NET_TOOL_DOMAIN_LEN) return false;
    if (memchr(h, ':', n) || memchr(h, '@', n)) return false;
    for (int i = 0; i < s_n_domains; i++) {
        if (strlen(s_domains[i]) == n && strncasecmp(h, s_domains[i], n) == 0) return true;
    }
    return false;
}

kit_err_t kit_net_tool_http_get(const char *url, kit_net_callback_t cb, void *user_data)
{
    if (!url || !cb) return KIT_ERR_INVALID_ARG;
    if (strlen(url) >= KIT_NET_TOOL_URL_MAX) return KIT_ERR_INVALID_ARG;
    if (!ensure_worker()) return KIT_ERR_NO_MEM;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    kit_err_t r = KIT_OK;
    int64_t now = esp_timer_get_time();
    if (!host_allowed(url)) {
        ESP_LOGW(TAG, "GET recusado (domínio fora do manifest): %s", url);
        r = KIT_ERR_PERMISSION_DENIED;
    } else if (!kit_network_is_connected()) {
        r = KIT_ERR_NOT_SUPPORTED;
    } else if (s_state != NT_IDLE || now - s_last_req_us < NT_MIN_GAP_US) {
        r = KIT_FAIL;   // ocupado: um pedido por vez, no máximo 1/s
    } else {
        strlcpy(s_url, url, sizeof(s_url));
        s_cb = cb;
        s_ud = user_data;
        s_req_gen = s_gen;
        s_last_req_us = now;
        s_state = NT_RUNNING;
    }
    xSemaphoreGive(s_lock);

    if (r == KIT_OK) {
        ESP_LOGI(TAG, "GET %s", url);
        xTaskNotifyGive(s_worker);
    }
    return r;
}
