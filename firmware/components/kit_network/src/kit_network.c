#include "kit_network.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "nvs.h"

#include "kit_time.h"   // kit_time_notify_online() ao obter IP (dispara SNTP)

static const char *TAG = "KIT_NETWORK";

// ---------------------------------------------------------------------------
// Redes memorizadas (NVS)
// ---------------------------------------------------------------------------
//
// Guardadas como um blob único ("nets") no namespace "kit_net": simples de
// carregar/gravar de uma vez e sem chaves órfãs. Ordem = ordem de inserção;
// a mais antiga (índice 0) é descartada quando a lista enche.

#define NVS_NS   "kit_net"
#define NVS_KEY  "nets"

// Economia de bateria: em MAX_MODEM o rádio só acorda a cada N beacons DTIM.
// O KIT faz HTTPS pontual (catálogo, OTA) e NTP — nada sensível a latência de
// RX — então pular beacons vale a corrente poupada.
#define KIT_NET_LISTEN_INTERVAL 3

// Depois de tantos ciclos sem enxergar nenhuma rede memorizada, o worker para
// de varrer a fundo a cada minuto e passa a tentar só a cada poucos minutos
// (usuário saiu de casa: varrer 13 canais toda hora não reconecta ninguém).
#define KIT_NET_SCAN_GIVEUP_RETRIES  8
#define KIT_NET_SCAN_GIVEUP_SECS     300

typedef struct {
    char ssid[KIT_NET_SSID_MAX];
    char pass[KIT_NET_PASS_MAX];
} kit_net_cred_t;

static struct {
    uint8_t        count;
    kit_net_cred_t nets[KIT_NET_MAX_SAVED];
} s_saved;

// ---------------------------------------------------------------------------
// Estado do subsistema
// ---------------------------------------------------------------------------

static SemaphoreHandle_t s_lock;             // protege s_saved e o radio config
static SemaphoreHandle_t s_scan_lock;        // serializa scans (interno vs. UI)
static TaskHandle_t      s_worker;           // task "kit_net": conecta/reconecta
static esp_netif_t      *s_netif;

static volatile kit_net_state_t s_state = KIT_NET_OFF;
static volatile bool  s_want_connected;      // start() pedido, stop() não veio
static volatile bool  s_suspended;           // rádio desligado p/ repouso da tela
static volatile bool  s_connecting;          // esp_wifi_connect() em andamento
static int64_t        s_connect_since_us;    // quando s_connecting virou true
static volatile bool  s_got_ip;
static char           s_ip[16];
static char           s_ssid[KIT_NET_SSID_MAX];
static volatile int   s_retries;             // falhas seguidas de associação

static kit_net_state_cb_t s_state_cb;
static void              *s_state_cb_arg;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void set_state(kit_net_state_t st)
{
    if (s_state == st) return;
    s_state = st;
    ESP_LOGI(TAG, "estado -> %d", (int)st);
    kit_net_state_cb_t cb = s_state_cb;
    if (cb) cb(st, s_state_cb_arg);
}

static void nvs_load(void)
{
    memset(&s_saved, 0, sizeof(s_saved));
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return;
    size_t len = sizeof(s_saved);
    if (nvs_get_blob(h, NVS_KEY, &s_saved, &len) != ESP_OK || len != sizeof(s_saved)) {
        memset(&s_saved, 0, sizeof(s_saved));
    }
    if (s_saved.count > KIT_NET_MAX_SAVED) s_saved.count = 0;
    nvs_close(h);
    ESP_LOGI(TAG, "%u rede(s) memorizada(s)", s_saved.count);
}

static void nvs_store(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
        ESP_LOGE(TAG, "NVS: falha ao abrir para escrita");
        return;
    }
    nvs_set_blob(h, NVS_KEY, &s_saved, sizeof(s_saved));
    nvs_commit(h);
    nvs_close(h);
}

// Índice da rede salva com este SSID, ou -1. Chamar com s_lock tomado.
static int saved_index(const char *ssid)
{
    for (int i = 0; i < s_saved.count; i++) {
        if (strncmp(s_saved.nets[i].ssid, ssid, KIT_NET_SSID_MAX) == 0) return i;
    }
    return -1;
}

static void kick_worker(void)
{
    if (s_worker) xTaskNotifyGive(s_worker);
}

// Casa a potência de TX com o RSSI da associação: colado no roteador não faz
// sentido gastar os 20 dBm cheios, e cada dB cortado é corrente poupada no PA.
// Reavaliado a cada associação (o worker também repete de tempos em tempos).
// Fica quieto enquanto o portal (APSTA) está no ar — o SoftAP precisa de
// alcance para o celular.
static void adjust_tx_power(void)
{
    if (kit_network_portal_is_active()) return;

    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) != ESP_OK) return;

    int8_t power;                          // unidades de 0,25 dBm (esp_wifi)
    if      (ap.rssi >= -58) power = 40;   // 10 dBm
    else if (ap.rssi >= -68) power = 60;   // 15 dBm
    else if (ap.rssi >= -75) power = 72;   // 18 dBm
    else                     power = 80;   // 20 dBm (teto prático)

    int8_t cur = 0;
    if (esp_wifi_get_max_tx_power(&cur) == ESP_OK && cur == power) return;
    if (esp_wifi_set_max_tx_power(power) == ESP_OK) {
        ESP_LOGI(TAG, "TX power -> %d dBm (RSSI %d)", power / 4, ap.rssi);
    }
}

// ---------------------------------------------------------------------------
// Eventos do driver Wi-Fi
// ---------------------------------------------------------------------------

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT) {
        switch (id) {
        case WIFI_EVENT_STA_START:
            kick_worker();
            break;
        case WIFI_EVENT_STA_DISCONNECTED: {
            s_got_ip = false;
            s_ip[0] = '\0';
            s_connecting = false;
            if (s_suspended) {
                // Parada proposital (repouso da tela): não reconecta, não mexe
                // no estado — a barra de status volta a valer ao acordar.
                break;
            }
            if (s_want_connected) {
                s_retries++;
                set_state(KIT_NET_CONNECTING);
                kick_worker();
            } else {
                set_state(KIT_NET_OFF);
            }
            break;
        }
        default:
            break;
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)data;
        snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&ev->ip_info.ip));
        s_got_ip = true;
        s_retries = 0;
        s_connecting = false;

        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
            strlcpy(s_ssid, (const char *)ap.ssid, sizeof(s_ssid));
        }
        ESP_LOGI(TAG, "conectado a \"%s\" — IP %s", s_ssid, s_ip);
        adjust_tx_power();
        set_state(KIT_NET_CONNECTED);
        kit_time_notify_online();   // (re)sincroniza o relógio por NTP
    }
}

// ---------------------------------------------------------------------------
// Scan
// ---------------------------------------------------------------------------

// Varre e devolve os APs ordenados por RSSI desc, sem SSID repetido.
// s_scan_lock deve estar tomado.
static kit_err_t do_scan(kit_net_ap_t *out, size_t max, size_t *count)
{
    *count = 0;

    wifi_scan_config_t cfg = { .show_hidden = false };
    esp_err_t err = esp_wifi_scan_start(&cfg, true /* block */);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "scan falhou: %s", esp_err_to_name(err));
        return KIT_FAIL;
    }

    uint16_t n = 0;
    esp_wifi_scan_get_ap_num(&n);
    if (n == 0) return KIT_OK;
    if (n > KIT_NET_SCAN_MAX) n = KIT_NET_SCAN_MAX;

    wifi_ap_record_t *recs = calloc(n, sizeof(wifi_ap_record_t));
    if (!recs) {
        esp_wifi_clear_ap_list();
        return KIT_ERR_NO_MEM;
    }
    esp_wifi_scan_get_ap_records(&n, recs);   // já vem por RSSI desc

    if (xSemaphoreTake(s_lock, portMAX_DELAY) == pdTRUE) {
        for (int i = 0; i < n && *count < max; i++) {
            const char *ssid = (const char *)recs[i].ssid;
            if (ssid[0] == '\0') continue;

            bool dup = false;
            for (size_t j = 0; j < *count; j++) {
                if (strcmp(out[j].ssid, ssid) == 0) { dup = true; break; }
            }
            if (dup) continue;

            kit_net_ap_t *a = &out[(*count)++];
            strlcpy(a->ssid, ssid, sizeof(a->ssid));
            a->rssi    = recs[i].rssi;
            a->channel = recs[i].primary;
            a->open    = (recs[i].authmode == WIFI_AUTH_OPEN);
            a->saved   = (saved_index(ssid) >= 0);
        }
        xSemaphoreGive(s_lock);
    }

    free(recs);
    return KIT_OK;
}

kit_err_t kit_network_scan(kit_net_ap_t *out, size_t max, size_t *count)
{
    if (!out || !count || max == 0) return KIT_ERR_INVALID_ARG;
    if (!s_want_connected)          return KIT_ERR_NOT_SUPPORTED;

    xSemaphoreTake(s_scan_lock, portMAX_DELAY);
    kit_err_t r = do_scan(out, max, count);
    xSemaphoreGive(s_scan_lock);
    return r;
}

// ---------------------------------------------------------------------------
// Reconexão automática (task "kit_net")
// ---------------------------------------------------------------------------

// Varre, escolhe a rede salva visível com melhor RSSI e associa nela.
// Retorna true se disparou uma tentativa de associação.
static bool connect_best_saved(void)
{
    kit_net_ap_t aps[KIT_NET_SCAN_MAX];
    size_t n = 0;

    xSemaphoreTake(s_scan_lock, portMAX_DELAY);
    kit_err_t scan_r = do_scan(aps, KIT_NET_SCAN_MAX, &n);
    xSemaphoreGive(s_scan_lock);

    char    best_ssid[KIT_NET_SSID_MAX] = {0};
    char    best_pass[KIT_NET_PASS_MAX] = {0};
    int8_t  best_rssi = -128;
    uint8_t best_channel = 0;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (size_t i = 0; i < n; i++) {
        int idx = saved_index(aps[i].ssid);
        if (idx < 0) continue;
        if (aps[i].rssi > best_rssi) {
            best_rssi = aps[i].rssi;
            best_channel = aps[i].channel;
            strlcpy(best_ssid, s_saved.nets[idx].ssid, sizeof(best_ssid));
            strlcpy(best_pass, s_saved.nets[idx].pass, sizeof(best_pass));
        }
    }
    // Scan falhou (ex.: driver ainda "connecting")? Não dá para saber o RSSI —
    // tenta a rede salva mais recente e deixa o esp_wifi varrer sozinho.
    if (best_ssid[0] == '\0' && scan_r != KIT_OK && s_saved.count > 0) {
        strlcpy(best_ssid, s_saved.nets[s_saved.count - 1].ssid, sizeof(best_ssid));
        strlcpy(best_pass, s_saved.nets[s_saved.count - 1].pass, sizeof(best_pass));
    }
    xSemaphoreGive(s_lock);

    if (best_ssid[0] == '\0') {
        ESP_LOGI(TAG, "nenhuma rede memorizada à vista");
        return false;
    }

    wifi_config_t wc = { 0 };
    strlcpy((char *)wc.sta.ssid,     best_ssid, sizeof(wc.sta.ssid));
    strlcpy((char *)wc.sta.password, best_pass, sizeof(wc.sta.password));
    wc.sta.listen_interval = KIT_NET_LISTEN_INTERVAL;   // p/ WIFI_PS_MAX_MODEM
    if (best_channel) {
        // Já sabemos em que canal o AP está — varre só ele em vez dos 13.
        wc.sta.channel     = best_channel;
        wc.sta.scan_method = WIFI_FAST_SCAN;
    } else {
        wc.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
        wc.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    }

    ESP_LOGI(TAG, "associando a \"%s\" (RSSI %d, canal %u)",
             best_ssid, best_rssi, best_channel);
    esp_wifi_set_config(WIFI_IF_STA, &wc);

    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK && err != ESP_ERR_WIFI_CONN) {
        ESP_LOGW(TAG, "esp_wifi_connect: %s", esp_err_to_name(err));
        return false;
    }
    s_connecting = true;   // aguarda GOT_IP ou STA_DISCONNECTED
    s_connect_since_us = esp_timer_get_time();
    return true;
}

static void worker_task(void *arg)
{
    for (;;) {
        // Suspenso (tela em repouso): rádio desligado, dorme até acordarem a
        // gente por notificação em kit_network_suspend(false).
        if (s_suspended) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            continue;
        }

        // Enquanto uma associação está em curso, espera até 20 s por um evento
        // (GOT_IP / DISCONNECTED). Sem associação em curso: 1ª tentativa quase
        // imediata, depois backoff de 3 s subindo até 60 s; se nada memorizado
        // aparecer por vários ciclos, cai para KIT_NET_SCAN_GIVEUP_SECS. Já
        // conectado: reavalia a potência de TX a cada 5 min. Ocioso: notificação.
        TickType_t wait = portMAX_DELAY;
        if (s_state == KIT_NET_CONNECTED) {
            wait = pdMS_TO_TICKS(300000);
        } else if (s_want_connected) {
            if (s_connecting) {
                wait = pdMS_TO_TICKS(20000);
            } else if (s_retries == 0) {
                wait = pdMS_TO_TICKS(300);
            } else if (s_retries >= KIT_NET_SCAN_GIVEUP_RETRIES) {
                wait = pdMS_TO_TICKS(KIT_NET_SCAN_GIVEUP_SECS * 1000);
            } else {
                int secs = 3 + s_retries * 5;
                if (secs > 60) secs = 60;
                wait = pdMS_TO_TICKS(secs * 1000);
            }
        }

        ulTaskNotifyTake(pdTRUE, wait);

        if (s_suspended)                 continue;
        if (!s_want_connected)           continue;
        if (s_state == KIT_NET_CONNECTED) {
            adjust_tx_power();           // a pessoa pode ter se movido
            continue;
        }

        if (s_connecting) {
            // Normalmente o driver Wi-Fi fecha a tentativa sozinho com um
            // STA_DISCONNECTED. Só forçamos se a associação passar de 25 s
            // sem qualquer evento (bem raro) — não a cada wake espúrio.
            if (esp_timer_get_time() - s_connect_since_us > 25000000) {
                ESP_LOGW(TAG, "associação sem resposta (25 s), reiniciando");
                s_connecting = false;
                esp_wifi_disconnect();
            }
            continue;
        }

        set_state(KIT_NET_CONNECTING);
        if (!connect_best_saved()) {
            // Nada conhecido à vista: cai para DISCONNECTED e tenta de novo
            // no próximo ciclo de backoff.
            set_state(KIT_NET_DISCONNECTED);
            if (s_retries < 100) s_retries++;
        }
    }
}

// ---------------------------------------------------------------------------
// Ciclo de vida
// ---------------------------------------------------------------------------

static bool s_stack_up;   // esp_netif/event/esp_wifi já inicializados?

// Traz a pilha de rede para o ar (esp_netif + event loop + esp_wifi). Custa
// ~45 KB de RAM interna, então só roda quando o usuário liga o Wi-Fi de fato
// — no boot, kit_network_init() faz só o barato (mutex + NVS). Sobe uma vez.
static kit_err_t ensure_stack(void)
{
    if (s_stack_up) return KIT_OK;
    if (!s_lock)    return KIT_FAIL;

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_netif_init: %s", esp_err_to_name(err));
        return KIT_FAIL;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "event loop: %s", esp_err_to_name(err));
        return KIT_FAIL;
    }

    if (!s_netif) s_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&wcfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init: %s", esp_err_to_name(err));
        return KIT_FAIL;
    }
    // Gerimos a lista de redes por conta própria (NVS "kit_net").
    esp_wifi_set_storage(WIFI_STORAGE_RAM);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                        on_wifi_event, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                        on_wifi_event, NULL, NULL);

    if (!s_worker &&
        xTaskCreate(worker_task, "kit_net", 4096, NULL, 4, &s_worker) != pdPASS) {
        return KIT_ERR_NO_MEM;
    }

    s_stack_up = true;
    ESP_LOGI(TAG, "pilha Wi-Fi no ar");
    return KIT_OK;
}

kit_err_t kit_network_init(void)
{
    if (s_lock) return KIT_OK;   // já inicializado

    s_lock      = xSemaphoreCreateMutex();
    s_scan_lock = xSemaphoreCreateMutex();
    if (!s_lock || !s_scan_lock) return KIT_ERR_NO_MEM;

    nvs_load();

    ESP_LOGI(TAG, "subsistema Wi-Fi pronto (pilha sob demanda, r\xC3\xA1""dio desligado)");
    return KIT_OK;
}

kit_err_t kit_network_start(void)
{
    if (!s_lock) return KIT_FAIL;
    if (s_want_connected) return KIT_OK;

    if (ensure_stack() != KIT_OK) return KIT_FAIL;

    s_want_connected = true;
    s_suspended = false;
    s_retries = 0;

    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) return KIT_FAIL;

    err = esp_wifi_start();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STOPPED) {
        ESP_LOGE(TAG, "esp_wifi_start: %s", esp_err_to_name(err));
        s_want_connected = false;
        return KIT_FAIL;
    }
    // Economia de bateria: o rádio dorme e só acorda a cada
    // KIT_NET_LISTEN_INTERVAL beacons DTIM (o listen_interval vai no
    // wifi_config de cada associação). O KIT não recebe tráfego não
    // solicitado, então a latência extra de RX não custa nada aqui.
    esp_wifi_set_ps(WIFI_PS_MAX_MODEM);

    set_state(KIT_NET_CONNECTING);
    kick_worker();
    return KIT_OK;
}

kit_err_t kit_network_stop(void)
{
    if (!s_lock) return KIT_FAIL;
    s_want_connected = false;
    s_suspended = false;
    s_connecting = false;
    s_got_ip = false;
    s_ip[0] = '\0';
    s_ssid[0] = '\0';
    if (s_stack_up) {
        esp_wifi_disconnect();
        esp_wifi_stop();
    }
    set_state(KIT_NET_OFF);
    return KIT_OK;
}

kit_err_t kit_network_suspend(bool suspend)
{
    if (!s_lock) return KIT_FAIL;
    if (suspend == s_suspended) return KIT_OK;
    s_suspended = suspend;

    // Wi-Fi desligado de fato, ou pilha nunca inicializada: só guarda a flag
    // (kit_network_start() a limpa depois).
    if (!s_want_connected || !s_stack_up) return KIT_OK;

    if (suspend) {
        ESP_LOGI(TAG, "suspenso (repouso da tela) — rádio desligado");
        s_connecting = false;
        s_got_ip = false;
        s_ip[0] = '\0';
        esp_wifi_disconnect();
        esp_wifi_stop();
        // s_state fica como está: ao acordar reconectamos em ~1 s e a barra de
        // status nem chega a piscar "desligado".
    } else {
        ESP_LOGI(TAG, "retomando — religando o rádio");
        s_retries = 0;
        esp_wifi_start();          // STA_START -> kick_worker -> reconecta
        set_state(KIT_NET_CONNECTING);
        kick_worker();
    }
    return KIT_OK;
}

kit_net_state_t kit_network_get_state(void) { return s_state; }

bool kit_network_is_connected(void)
{
    return s_state == KIT_NET_CONNECTED && s_got_ip;
}

kit_err_t kit_network_get_ip(char *buf, size_t len)
{
    if (!buf || len == 0) return KIT_ERR_INVALID_ARG;
    if (!s_got_ip) { buf[0] = '\0'; return KIT_ERR_NOT_FOUND; }
    strlcpy(buf, s_ip, len);
    return KIT_OK;
}

kit_err_t kit_network_get_ssid(char *buf, size_t len)
{
    if (!buf || len == 0) return KIT_ERR_INVALID_ARG;
    if (s_state != KIT_NET_CONNECTED) { buf[0] = '\0'; return KIT_ERR_NOT_FOUND; }
    strlcpy(buf, s_ssid, len);
    return KIT_OK;
}

int8_t kit_network_get_rssi(void)
{
    wifi_ap_record_t ap;
    if (s_state == KIT_NET_CONNECTED && esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        return ap.rssi;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Redes memorizadas
// ---------------------------------------------------------------------------

kit_err_t kit_network_save(const char *ssid, const char *pass)
{
    if (!ssid || ssid[0] == '\0' || strlen(ssid) >= KIT_NET_SSID_MAX) {
        return KIT_ERR_INVALID_ARG;
    }
    if (pass && strlen(pass) >= KIT_NET_PASS_MAX) return KIT_ERR_INVALID_ARG;

    xSemaphoreTake(s_lock, portMAX_DELAY);

    int idx = saved_index(ssid);
    if (idx < 0) {
        if (s_saved.count == KIT_NET_MAX_SAVED) {
            // descarta a mais antiga (índice 0)
            memmove(&s_saved.nets[0], &s_saved.nets[1],
                    (KIT_NET_MAX_SAVED - 1) * sizeof(kit_net_cred_t));
            s_saved.count--;
        }
        idx = s_saved.count++;
    }
    memset(&s_saved.nets[idx], 0, sizeof(kit_net_cred_t));
    strlcpy(s_saved.nets[idx].ssid, ssid, KIT_NET_SSID_MAX);
    if (pass) strlcpy(s_saved.nets[idx].pass, pass, KIT_NET_PASS_MAX);
    nvs_store();

    xSemaphoreGive(s_lock);

    ESP_LOGI(TAG, "rede \"%s\" memorizada", ssid);

    // Tenta conectar já, se o rádio estiver ligado.
    if (s_want_connected) {
        s_retries = 0;
        if (s_state != KIT_NET_CONNECTED) {
            kick_worker();
        }
    }
    return KIT_OK;
}

kit_err_t kit_network_forget(const char *ssid)
{
    if (!ssid) return KIT_ERR_INVALID_ARG;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    int idx = saved_index(ssid);
    if (idx < 0) {
        xSemaphoreGive(s_lock);
        return KIT_ERR_NOT_FOUND;
    }
    memmove(&s_saved.nets[idx], &s_saved.nets[idx + 1],
            (s_saved.count - idx - 1) * sizeof(kit_net_cred_t));
    s_saved.count--;
    memset(&s_saved.nets[s_saved.count], 0, sizeof(kit_net_cred_t));
    nvs_store();
    xSemaphoreGive(s_lock);

    ESP_LOGI(TAG, "rede \"%s\" esquecida", ssid);

    // Se era a rede ativa, desconecta e deixa o worker procurar outra.
    if (s_want_connected && strncmp(s_ssid, ssid, KIT_NET_SSID_MAX) == 0) {
        s_retries = 0;
        esp_wifi_disconnect();
    }
    return KIT_OK;
}

size_t kit_network_saved_count(void)
{
    return s_saved.count;
}

kit_err_t kit_network_saved_ssid(size_t idx, char *buf, size_t len)
{
    if (!buf || len == 0) return KIT_ERR_INVALID_ARG;
    kit_err_t r = KIT_ERR_NOT_FOUND;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (idx < s_saved.count) {
        strlcpy(buf, s_saved.nets[idx].ssid, len);
        r = KIT_OK;
    } else {
        buf[0] = '\0';
    }
    xSemaphoreGive(s_lock);
    return r;
}

bool kit_network_has_saved(const char *ssid)
{
    if (!ssid) return false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool found = saved_index(ssid) >= 0;
    xSemaphoreGive(s_lock);
    return found;
}

void kit_network_set_state_cb(kit_net_state_cb_t cb, void *user_data)
{
    s_state_cb     = cb;
    s_state_cb_arg = user_data;
}
