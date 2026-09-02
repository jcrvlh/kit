#include "kit_network.h"

#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_http_server.h"

#include "lwip/sockets.h"

static const char *TAG = "KIT_PORTAL";

#define AP_IP        "192.168.4.1"
#define AP_CHANNEL   1
#define AP_MAX_CONN  4

static httpd_handle_t s_httpd;
static TaskHandle_t   s_dns_task;
static esp_netif_t   *s_ap_netif;
static volatile bool  s_active;
static volatile bool  s_dns_run;

// ---------------------------------------------------------------------------
// Página do portal (auto-contida, sem CDN)
// ---------------------------------------------------------------------------

static const char PAGE_HTML[] =
"<!doctype html><html lang=pt-BR><head><meta charset=utf-8>"
"<meta name=viewport content=\"width=device-width,initial-scale=1\">"
"<title>KIT &middot; Wi-Fi</title><style>"
"*{box-sizing:border-box}body{margin:0;font:16px/1.5 system-ui,sans-serif;"
"background:#111;color:#eee;padding:24px}"
".logo{display:flex;gap:10px;align-items:center;justify-content:center;margin:6px 0}"
".logo i{width:26px;height:26px;display:block}"
".logo .sq{background:#C6472F}.logo .ci{background:#2C3CC4;border-radius:50%}"
".logo .tr{width:0;height:0;background:0;border-left:15px solid transparent;"
"border-right:15px solid transparent;border-bottom:26px solid #E9B23C}"
".wm{text-align:center;font-weight:800;letter-spacing:4px;font-size:22px;margin-bottom:16px}"
"h1{font-size:19px;margin:0 0 4px;text-align:center}"
"p.sub{color:#999;margin:0 0 20px;text-align:center}ul{list-style:none;padding:0;margin:0}"
"li{background:#1c1c1c;border-radius:12px;padding:14px 16px;margin-bottom:8px;"
"display:flex;justify-content:space-between;align-items:center;cursor:pointer}"
"li:active{background:#262626}.rssi{color:#888;font-size:13px}"
"form{margin-top:16px;background:#1c1c1c;border-radius:12px;padding:16px}"
"input{width:100%;padding:12px;border-radius:8px;border:1px solid #333;"
"background:#111;color:#eee;font-size:16px;margin:8px 0}"
"button{width:100%;padding:13px;border:0;border-radius:8px;background:#3b82f6;"
"color:#fff;font-size:16px;font-weight:600;cursor:pointer}"
"button:disabled{opacity:.5}#msg{margin-top:14px;text-align:center;color:#999}"
".hidden{display:none}</style></head><body>"
"<div class=logo><i class=sq></i><i class=ci></i><i class=tr></i></div>"
"<div class=wm>KIT</div>"
"<h1>Ol&aacute;! Vamos configurar o Wi-Fi</h1>"
"<p class=sub>Escolha a sua rede de 2,4 GHz e digite a senha.</p>"
"<ul id=list><li>Procurando redes&hellip;</li></ul>"
"<form id=f class=hidden><div id=fn></div>"
"<input id=p type=password placeholder=\"Senha da rede\" autocomplete=off>"
"<button type=submit id=b>Conectar</button></form>"
"<div id=msg></div>"
"<script>"
"var L=document.getElementById('list'),F=document.getElementById('f'),"
"FN=document.getElementById('fn'),P=document.getElementById('p'),"
"B=document.getElementById('b'),M=document.getElementById('msg'),cur='';"
"function scan(){fetch('/scan').then(r=>r.json()).then(function(n){"
"if(!n.length){L.innerHTML='<li>Nenhuma rede encontrada</li>';return}"
"L.innerHTML='';n.forEach(function(w){var li=document.createElement('li');"
"li.innerHTML='<span></span><span class=rssi>'+(w.open?'aberta':'&#128274;')+"
"' '+w.rssi+' dBm</span>';li.firstChild.textContent=w.ssid;"
"li.onclick=function(){pick(w)};L.appendChild(li)})}).catch(function(){"
"setTimeout(scan,1500)})}"
"function pick(w){cur=w.ssid;FN.textContent=w.ssid;F.classList.remove('hidden');"
"P.classList.toggle('hidden',w.open);P.value='';P.focus();"
"window.scrollTo(0,document.body.scrollHeight)}"
"F.onsubmit=function(e){e.preventDefault();B.disabled=true;M.textContent='Conectando\\u2026';"
"var d='ssid='+encodeURIComponent(cur)+'&pass='+encodeURIComponent(P.value);"
"fetch('/connect',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},"
"body:d}).then(function(){poll(0)})};"
"function poll(i){fetch('/status').then(r=>r.json()).then(function(s){"
"if(s.state==='connected'){M.innerHTML='Conectado &#10003;<br>IP '+s.ip+'<br>O KIT j\\u00e1 voltou para a tela inicial. Pode fechar esta p\\u00e1gina.';"
"B.disabled=false;return}"
"if(i>20){M.textContent='N&atilde;o consegui conectar. Confira a senha e tente de novo.';"
"B.disabled=false;return}setTimeout(function(){poll(i+1)},1000)})}"
"scan();setInterval(function(){if(F.classList.contains('hidden'))scan()},5000);"
"</script></body></html>";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Decodifica percent-encoding in-place (application/x-www-form-urlencoded:
// '+' vira espaço).
static void url_decode(char *s)
{
    char *o = s;
    for (char *i = s; *i; i++) {
        if (*i == '+') {
            *o++ = ' ';
        } else if (*i == '%' && i[1] && i[2]) {
            int hi = i[1], lo = i[2];
            hi = (hi <= '9') ? hi - '0' : (hi | 0x20) - 'a' + 10;
            lo = (lo <= '9') ? lo - '0' : (lo | 0x20) - 'a' + 10;
            *o++ = (char)((hi << 4) | lo);
            i += 2;
        } else {
            *o++ = *i;
        }
    }
    *o = '\0';
}

// Extrai o valor de `key` de um corpo urlencoded para `out`.
static bool form_field(const char *body, const char *key, char *out, size_t len)
{
    size_t klen = strlen(key);
    const char *p = body;
    while (p && *p) {
        if (strncmp(p, key, klen) == 0 && p[klen] == '=') {
            const char *v = p + klen + 1;
            const char *e = strchr(v, '&');
            size_t n = e ? (size_t)(e - v) : strlen(v);
            if (n >= len) n = len - 1;
            memcpy(out, v, n);
            out[n] = '\0';
            url_decode(out);
            return true;
        }
        p = strchr(p, '&');
        if (p) p++;
    }
    return false;
}

// Acrescenta `s` a `dst` como conteúdo de string JSON, escapando o necessário.
static void json_escape_append(char *dst, size_t cap, const char *s)
{
    size_t n = strlen(dst);
    for (; *s && n + 6 < cap; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') { dst[n++] = '\\'; dst[n++] = c; }
        else if (c == '\n') { dst[n++] = '\\'; dst[n++] = 'n'; }
        else if (c == '\r') { dst[n++] = '\\'; dst[n++] = 'r'; }
        else if (c < 0x20)  { n += snprintf(dst + n, cap - n, "\\u%04x", c); }
        else dst[n++] = c;
    }
    dst[n] = '\0';
}

// ---------------------------------------------------------------------------
// Handlers HTTP
// ---------------------------------------------------------------------------

static esp_err_t h_root(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, PAGE_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t h_scan(httpd_req_t *req)
{
    static kit_net_ap_t aps[KIT_NET_SCAN_MAX];
    size_t n = 0;
    kit_network_scan(aps, KIT_NET_SCAN_MAX, &n);

    // ~90 bytes por entrada; SSID escapado pode dobrar. Folga larga.
    char *buf = malloc(64 + n * 160);
    if (!buf) return httpd_resp_send_500(req);

    strcpy(buf, "[");
    for (size_t i = 0; i < n; i++) {
        if (i) strcat(buf, ",");
        strcat(buf, "{\"ssid\":\"");
        json_escape_append(buf, 64 + n * 160, aps[i].ssid);
        char tail[48];
        snprintf(tail, sizeof(tail), "\",\"rssi\":%d,\"open\":%s}",
                 aps[i].rssi, aps[i].open ? "true" : "false");
        strcat(buf, tail);
    }
    strcat(buf, "]");

    httpd_resp_set_type(req, "application/json");
    esp_err_t r = httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
    free(buf);
    return r;
}

static esp_err_t h_status(httpd_req_t *req)
{
    const char *st;
    switch (kit_network_get_state()) {
    case KIT_NET_CONNECTED:  st = "connected";  break;
    case KIT_NET_CONNECTING: st = "connecting"; break;
    default:                 st = "idle";       break;
    }
    char ip[16] = {0}, ssid[KIT_NET_SSID_MAX] = {0};
    kit_network_get_ip(ip, sizeof(ip));
    kit_network_get_ssid(ssid, sizeof(ssid));

    char body[128];
    char essid[2 * KIT_NET_SSID_MAX] = {0};
    json_escape_append(essid, sizeof(essid), ssid);
    snprintf(body, sizeof(body), "{\"state\":\"%s\",\"ssid\":\"%s\",\"ip\":\"%s\"}",
             st, essid, ip);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t h_connect(httpd_req_t *req)
{
    char body[256];
    int total = 0;
    while (total < (int)sizeof(body) - 1) {
        int r = httpd_req_recv(req, body + total, sizeof(body) - 1 - total);
        if (r <= 0) break;
        total += r;
    }
    body[total > 0 ? total : 0] = '\0';

    char ssid[KIT_NET_SSID_MAX] = {0}, pass[KIT_NET_PASS_MAX] = {0};
    if (!form_field(body, "ssid", ssid, sizeof(ssid)) || ssid[0] == '\0') {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "{\"ok\":false}");
    }
    form_field(body, "pass", pass, sizeof(pass));

    ESP_LOGI(TAG, "portal: salvando rede \"%s\"", ssid);
    kit_network_save(ssid, pass[0] ? pass : NULL);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

// Captura de portal: qualquer outra URL redireciona para a raiz.
static esp_err_t h_redirect(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://" AP_IP "/");
    return httpd_resp_send(req, NULL, 0);
}

// ---------------------------------------------------------------------------
// Servidor DNS de captura (responde tudo com o IP do AP)
// ---------------------------------------------------------------------------

static void dns_task(void *arg)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "DNS: socket falhou");
        s_dns_task = NULL;
        vTaskDelete(NULL);
        return;
    }
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(53),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "DNS: bind :53 falhou");
        close(sock);
        s_dns_task = NULL;
        vTaskDelete(NULL);
        return;
    }
    struct timeval tv = { .tv_sec = 1 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    uint8_t pkt[512];
    while (s_dns_run) {
        struct sockaddr_in from;
        socklen_t flen = sizeof(from);
        int n = recvfrom(sock, pkt, sizeof(pkt), 0, (struct sockaddr *)&from, &flen);
        if (n < (int)sizeof(uint16_t) * 6) continue;

        // Resposta: flags = 0x8180, 1 pergunta ecoada, 1 resposta.
        pkt[2] = 0x81; pkt[3] = 0x80;
        pkt[6] = 0x00; pkt[7] = 0x01;   // ANCOUNT = 1
        pkt[8] = pkt[9] = pkt[10] = pkt[11] = 0;

        if (n + 16 > (int)sizeof(pkt)) continue;
        uint8_t *a = pkt + n;
        *a++ = 0xc0; *a++ = 0x0c;             // ponteiro para o nome da pergunta
        *a++ = 0x00; *a++ = 0x01;             // TYPE A
        *a++ = 0x00; *a++ = 0x01;             // CLASS IN
        *a++ = 0x00; *a++ = 0x00; *a++ = 0x00; *a++ = 0x3c;  // TTL 60
        *a++ = 0x00; *a++ = 0x04;             // RDLENGTH 4
        *a++ = 192; *a++ = 168; *a++ = 4; *a++ = 1;

        sendto(sock, pkt, n + 16, 0, (struct sockaddr *)&from, flen);
    }
    close(sock);
    s_dns_task = NULL;
    vTaskDelete(NULL);
}

// ---------------------------------------------------------------------------
// Ciclo de vida do portal
// ---------------------------------------------------------------------------

static void start_httpd(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.lru_purge_enable = true;
    cfg.max_uri_handlers = 8;
    cfg.uri_match_fn = httpd_uri_match_wildcard;

    if (httpd_start(&s_httpd, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start falhou");
        return;
    }
    const httpd_uri_t uris[] = {
        { .uri = "/scan",     .method = HTTP_GET,  .handler = h_scan },
        { .uri = "/status",   .method = HTTP_GET,  .handler = h_status },
        { .uri = "/connect",  .method = HTTP_POST, .handler = h_connect },
        { .uri = "/",         .method = HTTP_GET,  .handler = h_root },
        { .uri = "/*",        .method = HTTP_GET,  .handler = h_redirect },
    };
    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        httpd_register_uri_handler(s_httpd, &uris[i]);
    }
}

kit_err_t kit_network_portal_start(const char *ap_name)
{
    if (s_active) return KIT_OK;

    // Garante STA no ar (scan + associação usam a interface STA).
    kit_network_start();

    if (esp_wifi_set_mode(WIFI_MODE_APSTA) != ESP_OK) return KIT_FAIL;

    if (!s_ap_netif) s_ap_netif = esp_netif_create_default_wifi_ap();

    wifi_config_t ap = { 0 };
    const char *name = (ap_name && ap_name[0]) ? ap_name : "KIT-SETUP";
    strlcpy((char *)ap.ap.ssid, name, sizeof(ap.ap.ssid));
    ap.ap.ssid_len = strlen(name);
    ap.ap.channel = AP_CHANNEL;
    ap.ap.max_connection = AP_MAX_CONN;
    ap.ap.authmode = WIFI_AUTH_OPEN;
    if (esp_wifi_set_config(WIFI_IF_AP, &ap) != ESP_OK) return KIT_FAIL;

    s_dns_run = true;
    xTaskCreate(dns_task, "kit_dns", 3072, NULL, 4, &s_dns_task);
    start_httpd();

    s_active = true;
    ESP_LOGI(TAG, "portal no ar — AP \"%s\", http://%s/", name, AP_IP);
    return KIT_OK;
}

kit_err_t kit_network_portal_stop(void)
{
    if (!s_active) return KIT_OK;
    s_active = false;

    if (s_httpd) { httpd_stop(s_httpd); s_httpd = NULL; }
    s_dns_run = false;
    for (int i = 0; i < 20 && s_dns_task; i++) vTaskDelay(pdMS_TO_TICKS(100));

    // Volta para STA puro; o netif do AP fica alocado para reuso.
    esp_wifi_set_mode(WIFI_MODE_STA);

    ESP_LOGI(TAG, "portal encerrado");
    return KIT_OK;
}

bool kit_network_portal_is_active(void)
{
    return s_active;
}
