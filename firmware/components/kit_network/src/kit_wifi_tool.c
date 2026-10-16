// Scan de Wi-Fi para as Tools do catálogo — ver kit_wifi_tool.h.
//
// Máquina de estados igual à do kit_net_tool: IDLE -> RUNNING (worker em
// kit_network_scan_aps) -> DONE (resultado guardado) -> IDLE (kit_wifi_tool_poll
// entregou ao callback). s_gen muda a cada Tool nova e a cada cancel: resultado
// de geração velha é descartado em vez de cair no callback de uma Tool que saiu.

#include "kit_wifi_tool.h"
#include "kit_network.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "KIT_WIFI_TOOL";

typedef enum { WT_IDLE = 0, WT_RUNNING, WT_DONE } wt_state_t;

static SemaphoreHandle_t s_lock = NULL;
static TaskHandle_t      s_worker = NULL;

static wt_state_t s_state = WT_IDLE;
static uint32_t   s_gen = 0;
static uint32_t   s_req_gen = 0;
static int64_t    s_last_req_us = 0;

static kit_wifi_scan_cb_t s_cb = NULL;
static void              *s_ud = NULL;

// Resultado (estado DONE). O buffer fica na PSRAM e é reaproveitado: só o
// worker escreve nele (em RUNNING) e só o poll lê (em DONE).
static kit_wifi_ap_t *s_aps = NULL;
static size_t         s_count = 0;
static kit_err_t      s_res = KIT_OK;

static void worker_task(void *arg)
{
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        xSemaphoreTake(s_lock, portMAX_DELAY);
        uint32_t gen = s_req_gen;
        xSemaphoreGive(s_lock);

        size_t n = 0;
        kit_err_t res = kit_network_scan_aps(s_aps, KIT_WIFI_SCAN_MAX, &n);

        xSemaphoreTake(s_lock, portMAX_DELAY);
        if (gen == s_gen) {
            s_res = res;
            s_count = res == KIT_OK ? n : 0;
            s_state = WT_DONE;
        } else {
            s_state = WT_IDLE;   // a Tool saiu ou cancelou no meio do scan
        }
        xSemaphoreGive(s_lock);
        ESP_LOGI(TAG, "scan concluído: res=%d, %u AP(s)", res, (unsigned)n);
    }
}

static bool ensure_worker(void)
{
    if (s_worker) return true;
    if (!s_lock) s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return false;
    if (!s_aps) {
        s_aps = heap_caps_calloc(KIT_WIFI_SCAN_MAX, sizeof(kit_wifi_ap_t), MALLOC_CAP_SPIRAM);
        if (!s_aps) return false;
    }
    // Pilha na RAM interna: chama o driver Wi-Fi (e o esp_wifi_init, se a
    // pilha ainda não subiu).
    if (xTaskCreate(worker_task, "kit_wifi_tool", 4096, NULL, 4, &s_worker) != pdPASS) {
        s_worker = NULL;
        return false;
    }
    return true;
}

// Esquece callback e resultado guardado. Chamar com s_lock tomado.
static void reset_locked(void)
{
    s_gen++;
    s_cb = NULL;
    s_ud = NULL;
    if (s_state == WT_DONE) s_state = WT_IDLE;
    // WT_RUNNING: o worker vê a geração trocada e descarta sozinho.
}

void kit_wifi_tool_begin(void)
{
    if (!s_lock) s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    reset_locked();
    xSemaphoreGive(s_lock);
}

void kit_wifi_tool_end(void)
{
    kit_wifi_tool_cancel();
}

void kit_wifi_tool_cancel(void)
{
    if (!s_lock) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    reset_locked();
    xSemaphoreGive(s_lock);
}

void kit_wifi_tool_poll(void)
{
    if (!s_lock || s_state != WT_DONE) return;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_state != WT_DONE) { xSemaphoreGive(s_lock); return; }
    kit_wifi_scan_cb_t cb = s_cb;
    void *ud = s_ud;
    kit_err_t res = s_res;
    size_t count = s_count;
    s_cb = NULL;
    s_ud = NULL;
    xSemaphoreGive(s_lock);

    // Ainda DONE durante a chamada: um scan() pedido de dentro do callback
    // sai como "ocupado" em vez de deixar o worker sobrescrever s_aps.
    if (cb) cb(res, res == KIT_OK ? s_aps : NULL, count, ud);

    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_state == WT_DONE) s_state = WT_IDLE;
    xSemaphoreGive(s_lock);
}

kit_err_t kit_wifi_tool_scan(kit_wifi_scan_cb_t cb, void *user_data)
{
    if (!cb) return KIT_ERR_INVALID_ARG;
    if (!ensure_worker()) return KIT_ERR_NO_MEM;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    kit_err_t r = KIT_OK;
    int64_t now = esp_timer_get_time();
    if (s_state != WT_IDLE ||
        (s_last_req_us && now - s_last_req_us < (int64_t)KIT_WIFI_SCAN_MIN_GAP_MS * 1000)) {
        r = KIT_FAIL;   // ocupado: um scan por vez, no máximo um a cada 5 s
    } else {
        s_cb = cb;
        s_ud = user_data;
        s_req_gen = s_gen;
        s_last_req_us = now;
        s_state = WT_RUNNING;
    }
    xSemaphoreGive(s_lock);

    if (r == KIT_OK) xTaskNotifyGive(s_worker);
    return r;
}
