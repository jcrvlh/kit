#include "kit_upload.h"
#include "kit_network.h"
#include "kit_tool_manager.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "esp_log.h"
#include "esp_random.h"
#include "esp_http_server.h"

static const char *TAG = "KIT_UPLOAD";

#define UPLOAD_TMP_PATH "/sdcard/tools/.upload.kit"
#define CHUNK           4096
#define MAX_BAD_PINS    5

static httpd_handle_t s_httpd;
static volatile kit_upload_state_t s_state = KIT_UP_OFF;
static volatile int      s_progress;
static volatile uint32_t s_activity;
static char s_pin[5];
static char s_msg[80];
static int  s_bad_pins;

// ---------------------------------------------------------------------------
// Página (auto-contida, sem CDN — mesma pegada do portal de Wi-Fi)
// ---------------------------------------------------------------------------

static const char PAGE_HTML[] =
"<!doctype html><html lang=pt-BR><head><meta charset=utf-8>"
"<meta name=viewport content=\"width=device-width,initial-scale=1\">"
"<title>KIT &middot; Instalar</title><style>"
"*{box-sizing:border-box}body{margin:0 auto;max-width:480px;"
"font:16px/1.5 system-ui,sans-serif;background:#111;color:#eee;padding:24px}"
".logo{display:flex;gap:10px;align-items:center;justify-content:center;margin:6px 0}"
".logo i{width:26px;height:26px;display:block}"
".logo .sq{background:#C6472F}.logo .ci{background:#2C3CC4;border-radius:50%}"
".logo .tr{width:0;height:0;background:0;border-left:15px solid transparent;"
"border-right:15px solid transparent;border-bottom:26px solid #E9B23C}"
".wm{text-align:center;font-weight:800;letter-spacing:4px;font-size:22px;margin-bottom:16px}"
"h1{font-size:19px;margin:0 0 4px;text-align:center}"
"p.sub{color:#999;margin:0 0 20px;text-align:center}"
"label{display:block;color:#999;font-size:13px;letter-spacing:2px;margin-top:14px}"
"input{width:100%;padding:12px;border-radius:8px;border:1px solid #333;"
"background:#1c1c1c;color:#eee;font-size:16px;margin:6px 0}"
"#pin{font-size:28px;letter-spacing:12px;text-align:center}"
"#file{display:none}"
"#dz{padding:28px 16px;border:2px dashed #444;border-radius:12px;text-align:center;"
"color:#ccc;font-size:16px;letter-spacing:0;cursor:pointer}"
"#dz.over{border-color:#E9B23C;background:#1c1a12;color:#eee}"
"#dz small{display:block;color:#888;margin-top:4px}"
"#sel{color:#E9B23C;font-size:14px;margin-top:8px;text-align:center;word-break:break-all}"
"button{width:100%;padding:14px;border:0;border-radius:999px;background:#E9B23C;"
"color:#111;font-size:16px;font-weight:700;letter-spacing:2px;margin-top:18px;cursor:pointer}"
"button:disabled{opacity:.5}"
".bar{height:6px;background:#1c1c1c;border-radius:3px;margin-top:18px;overflow:hidden}"
".bar i{display:block;height:100%;width:0;background:#E9B23C}"
"ul{list-style:none;padding:0;margin:16px 0 0}"
"li{background:#1c1c1c;border-radius:12px;padding:12px 16px;margin-bottom:8px}"
"li.ok{border-left:4px solid #3C9A5F}li.err{border-left:4px solid #C6472F}"
"</style></head><body>"
"<div class=logo><i class=sq></i><i class=ci></i><i class=tr></i></div>"
"<div class=wm>KIT</div>"
"<h1>Instalar Tool</h1>"
"<p class=sub>Digite o PIN que aparece na tela do KIT e arraste os arquivos .kit pra c&aacute;.</p>"
"<form id=f><label>PIN</label>"
"<input id=pin inputmode=numeric maxlength=4 autocomplete=off>"
"<label>ARQUIVOS</label>"
"<label id=dz for=file>Arraste os .kit aqui<small>ou toque pra escolher</small></label>"
"<input id=file type=file accept=.kit multiple><div id=sel></div>"
"<button id=btn>INSTALAR</button></form>"
"<div class=bar><i id=bar></i></div><ul id=log></ul>"
"<script>"
"const $=i=>document.getElementById(i);let picked=[],busy=false,dc=0;"
"function log(t,c){const l=document.createElement('li');l.className=c||'';"
"l.textContent=t;$('log').prepend(l)}"
"function pick(fs){picked=[...fs].filter(f=>/\\.kit$/i.test(f.name));"
"$('sel').textContent=picked.length?picked.map(f=>f.name).join(', ')"
":(fs.length?'S\\u00f3 arquivos .kit':'')}"
"function send(f,pin){return new Promise(res=>{const x=new XMLHttpRequest();"
"x.open('POST','/upload');x.setRequestHeader('X-Kit-Pin',pin);"
"x.setRequestHeader('Content-Type','application/octet-stream');"
"x.upload.onprogress=e=>{if(e.lengthComputable)$('bar').style.width=(e.loaded*100/e.total)+'%'};"
"x.onload=()=>{let j={ok:false,err:'Resposta estranha do KIT'};"
"try{j=JSON.parse(x.responseText)}catch(_){}res(j)};"
"x.onerror=()=>res({ok:false,err:'A conex\\u00e3o caiu'});x.send(f)})}"
"async function go(){if(busy)return;const pin=$('pin').value.trim();"
"if(pin.length!=4){$('pin').focus();log('Digite o PIN da tela do KIT','err');return}"
"if(!picked.length){log('Escolha ou arraste um .kit','err');return}"
"busy=true;$('btn').disabled=true;"
"const c=await fetch('/check',{method:'POST',headers:{'X-Kit-Pin':pin}})"
".then(r=>r.json()).catch(()=>({ok:false,err:'O KIT n\\u00e3o respondeu'}));"
"if(c.ok){for(const f of picked){$('bar').style.width='0';const r=await send(f,pin);"
"if(r.ok)log('Instalada: '+r.name+' '+r.version,'ok');"
"else{log(f.name+': '+r.err,'err');if(r.locked)break}}"
"picked=[];$('file').value='';$('sel').textContent=''}"
"else log(c.err,'err');"
"$('bar').style.width='0';$('btn').disabled=false;busy=false}"
"$('f').onsubmit=e=>{e.preventDefault();go()};"
"$('file').onchange=e=>pick(e.target.files);"
"document.addEventListener('dragenter',e=>{e.preventDefault();dc++;$('dz').classList.add('over')});"
"document.addEventListener('dragleave',()=>{if(--dc<=0){dc=0;$('dz').classList.remove('over')}});"
"document.addEventListener('dragover',e=>e.preventDefault());"
"document.addEventListener('drop',e=>{e.preventDefault();dc=0;$('dz').classList.remove('over');"
"pick(e.dataTransfer.files);if(picked.length&&$('pin').value.trim().length==4)go()});"
"</script></body></html>";

// ---------------------------------------------------------------------------
// Handlers
// ---------------------------------------------------------------------------

static esp_err_t send_json(httpd_req_t *req, const char *status, const char *body)
{
    if (status) httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, body);
}

static esp_err_t send_err(httpd_req_t *req, const char *status, const char *msg)
{
    char body[160];
    snprintf(body, sizeof(body), "{\"ok\":false,\"err\":\"%s\"%s}",
             msg, s_state == KIT_UP_LOCKED ? ",\"locked\":true" : "");
    return send_json(req, status, body);
}

static void fail(const char *msg)
{
    strlcpy(s_msg, msg, sizeof(s_msg));
    s_state = KIT_UP_ERROR;
    ESP_LOGW(TAG, "%s", msg);
}

// Confere o PIN do header. Errar MAX_BAD_PINS vezes trava até reabrir a tela:
// 4 dígitos só seguram força bruta com tentativa limitada.
static bool pin_ok(httpd_req_t *req)
{
    char got[8] = {0};
    if (s_state != KIT_UP_LOCKED &&
        httpd_req_get_hdr_value_str(req, "X-Kit-Pin", got, sizeof(got)) == ESP_OK &&
        strcmp(got, s_pin) == 0) {
        return true;
    }
    if (s_state != KIT_UP_LOCKED && ++s_bad_pins >= MAX_BAD_PINS) {
        strlcpy(s_msg, "PIN errado demais", sizeof(s_msg));
        s_state = KIT_UP_LOCKED;
        ESP_LOGW(TAG, "PIN errado %d vezes — envio travado", s_bad_pins);
    }
    return false;
}

static esp_err_t pin_refused(httpd_req_t *req)
{
    return send_err(req, "403 Forbidden",
                    s_state == KIT_UP_LOCKED
                        ? "Envio travado. Feche e abra a tela de novo no KIT."
                        : "PIN errado");
}

static esp_err_t h_root(httpd_req_t *req)
{
    s_activity++;
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, PAGE_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t h_check(httpd_req_t *req)
{
    s_activity++;
    if (!pin_ok(req)) return pin_refused(req);
    return send_json(req, NULL, "{\"ok\":true}");
}

// Nome da Tool vai dentro de uma string JSON: aspas/barra viram espaço.
static void json_safe(char *s)
{
    for (; *s; s++) if (*s == '"' || *s == '\\' || (unsigned char)*s < 0x20) *s = ' ';
}

static esp_err_t h_upload(httpd_req_t *req)
{
    s_activity++;
    if (!pin_ok(req)) return pin_refused(req);

    size_t total = req->content_len;
    if (total == 0 || total > KIT_UPLOAD_MAX_BYTES) {
        fail("Arquivo vazio ou grande demais");
        return send_err(req, "413 Payload Too Large", s_msg);
    }

    FILE *f = fopen(UPLOAD_TMP_PATH, "wb");
    char *buf = malloc(CHUNK);
    if (!f || !buf) {
        if (f) fclose(f);
        free(buf);
        fail("Cart\xC3\xA3o indispon\xC3\xADvel");
        return send_err(req, "500 Internal Server Error", s_msg);
    }

    ESP_LOGI(TAG, "recebendo %u bytes", (unsigned)total);
    s_progress = 0;
    s_state = KIT_UP_RECEIVING;

    size_t got = 0;
    int timeouts = 0;
    bool ok = true;
    while (got < total) {
        size_t want = total - got < CHUNK ? total - got : CHUNK;
        int n = httpd_req_recv(req, buf, want);
        if (n == HTTPD_SOCK_ERR_TIMEOUT && ++timeouts < 3) continue;
        if (n <= 0) { ok = false; break; }
        timeouts = 0;
        if (got == 0 && (n < 4 || memcmp(buf, "PK\x03\x04", 4) != 0)) {
            fclose(f); free(buf); unlink(UPLOAD_TMP_PATH);
            fail("N\xC3\xA3o \xC3\xA9 um pacote .kit");
            return send_err(req, "400 Bad Request", s_msg);
        }
        if (fwrite(buf, 1, (size_t)n, f) != (size_t)n) { ok = false; break; }
        got += (size_t)n;
        s_progress = (int)((uint64_t)got * 100 / total);
        s_activity++;
    }
    fclose(f);
    free(buf);

    if (!ok) {
        unlink(UPLOAD_TMP_PATH);
        fail("O envio n\xC3\xA3o chegou inteiro");
        return send_err(req, "500 Internal Server Error", s_msg);
    }

    s_state = KIT_UP_INSTALLING;
    kit_tool_entry_t e = {0};
    kit_err_t r = kit_tool_manager_install_pkg(UPLOAD_TMP_PATH, &e);
    switch (r) {
    case KIT_OK: break;
    case KIT_ERR_INVALID_ARG:   fail("Manifest sem id v\xC3\xA1lido");                 break;
    case KIT_ERR_NO_MEM:        fail("Limite de Tools no cart\xC3\xA3o");              break;
    case KIT_FAIL:              fail("Esta Tool est\xC3\xA1 aberta no KIT");            break;
    case KIT_ERR_NOT_SUPPORTED: fail("Incompat\xC3\xADvel com este firmware");          break;
    default:                    fail("Pacote corrompido");                              break;
    }
    if (r != KIT_OK) return send_err(req, "422 Unprocessable Entity", s_msg);

    json_safe(e.name);
    json_safe(e.version);
    snprintf(s_msg, sizeof(s_msg), "%s %s", e.name, e.version);
    s_state = KIT_UP_DONE;
    ESP_LOGI(TAG, "instalada: %s (%s) v%s", e.name, e.id, e.version);

    char body[160];
    snprintf(body, sizeof(body), "{\"ok\":true,\"name\":\"%s\",\"version\":\"%s\"}",
             e.name, e.version);
    return send_json(req, NULL, body);
}

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------

kit_err_t kit_upload_start(void)
{
    if (s_httpd) return KIT_OK;
    if (kit_network_get_state() == KIT_NET_OFF) return KIT_ERR_NOT_SUPPORTED;
    if (kit_network_portal_is_active()) return KIT_FAIL;   // porta 80 ocupada

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = 6144;        // extração + recarga do catálogo rodam aqui
    cfg.max_uri_handlers = 4;
    cfg.lru_purge_enable = true;
    cfg.recv_wait_timeout = 10;

    if (httpd_start(&s_httpd, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start falhou");
        s_httpd = NULL;
        return KIT_FAIL;
    }
    const httpd_uri_t uris[] = {
        { .uri = "/",       .method = HTTP_GET,  .handler = h_root },
        { .uri = "/check",  .method = HTTP_POST, .handler = h_check },
        { .uri = "/upload", .method = HTTP_POST, .handler = h_upload },
    };
    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        httpd_register_uri_handler(s_httpd, &uris[i]);
    }

    snprintf(s_pin, sizeof(s_pin), "%04lu", (unsigned long)(esp_random() % 10000));
    s_bad_pins = 0;
    s_msg[0] = '\0';
    s_progress = 0;
    s_state = KIT_UP_WAITING;
    ESP_LOGI(TAG, "servidor no ar");
    return KIT_OK;
}

void kit_upload_stop(void)
{
    if (!s_httpd) return;
    httpd_stop(s_httpd);   // espera o handler em curso terminar
    s_httpd = NULL;
    s_state = KIT_UP_OFF;
    unlink(UPLOAD_TMP_PATH);
    ESP_LOGI(TAG, "servidor parado");
}

bool               kit_upload_is_active(void)  { return s_httpd != NULL; }
kit_upload_state_t kit_upload_get_state(void)  { return s_state; }
int                kit_upload_progress(void)   { return s_progress; }
const char        *kit_upload_pin(void)        { return s_pin; }
const char        *kit_upload_message(void)    { return s_msg; }
uint32_t           kit_upload_activity(void)   { return s_activity; }
