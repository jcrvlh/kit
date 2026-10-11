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
#include "cJSON.h"

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
static kit_upload_removed_cb_t s_removed_cb;

// ---------------------------------------------------------------------------
// Página (auto-contida, sem CDN — mesma pegada do portal de Wi-Fi)
// ---------------------------------------------------------------------------

static const char PAGE_HTML[] =
"<!doctype html><html lang=pt-BR><head><meta charset=utf-8>\n"
"<meta name=viewport content=\"width=device-width,initial-scale=1\">\n"
"<title>KIT &middot; Tools</title><style>\n"
"*{box-sizing:border-box}\n"
"body{margin:0 auto;max-width:520px;font:16px/1.5 system-ui,sans-serif;background:#000;color:#EFEADD;padding:24px 16px}\n"
".logo{display:flex;gap:10px;align-items:center;justify-content:center;margin:6px 0}\n"
".logo i{width:26px;height:26px;display:block}\n"
".sq{background:#C6472F}.ci{background:#2C3CC4;border-radius:50%}\n"
".tr{background:#E9B23C;clip-path:polygon(50% 0,100% 100%,0 100%)}\n"
".wm{text-align:center;font-weight:800;letter-spacing:4px;font-size:22px;margin-bottom:16px}\n"
"h1{font-size:19px;margin:0 0 4px;text-align:center}\n"
"p.sub{color:#9a978f;margin:0 0 20px;text-align:center}\n"
".lb{display:block;color:#6E6C66;font:13px ui-monospace,Menlo,monospace;letter-spacing:3px;margin-top:22px}\n"
"#pin{width:100%;padding:12px;border-radius:12px;border:2px solid #2C2C2E;background:#171719;color:#EFEADD;\n"
"font:28px ui-monospace,Menlo,monospace;letter-spacing:12px;text-align:center;margin:6px 0}\n"
"#pin:focus{outline:0;border-color:#E9B23C}\n"
"#file{display:none}\n"
"#dz{display:block;margin-top:6px;padding:24px 16px;border:2px dashed #2C2C2E;border-radius:20px;text-align:center;cursor:pointer}\n"
"#dz.over{border-color:#E9B23C;background:#120f06}\n"
"#dz small{display:block;color:#6E6C66;font:12px ui-monospace,Menlo,monospace;letter-spacing:2px;margin-top:4px}\n"
".grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:12px;margin-top:12px}\n"
".card{position:relative;background:var(--c);color:var(--k);border-radius:20px;min-height:118px;padding:52px 14px 14px;\n"
"display:flex;flex-direction:column;justify-content:flex-end;overflow:hidden;transition:opacity .3s}\n"
".card .sh{position:absolute;top:16px;left:16px;width:24px;height:24px;background:var(--k)}\n"
".card .x{position:absolute;top:8px;right:8px;width:40px;height:40px;border-radius:50%;border:0;padding:0;\n"
"background:rgba(0,0,0,.22);color:var(--k);font-size:22px;line-height:40px;cursor:pointer}\n"
".card.sending .x,.card.ok .x,.card.ask .x{display:none}\n"
".card .nm{font-size:18px;font-weight:800;line-height:1.2;word-break:break-word}\n"
".card .mt{font:11px/1.45 ui-monospace,Menlo,monospace;letter-spacing:2px;opacity:.75;margin-top:4px;text-transform:uppercase;white-space:pre-line}\n"
".card .pg{position:absolute;left:0;right:0;bottom:0;height:6px}\n"
".card .pg i{display:block;height:100%;width:0;background:var(--k);opacity:.55}\n"
".card.err{--c:#171719!important;--k:#EFEADD!important;box-shadow:inset 0 0 0 2px #C6472F}\n"
".card.ok::after{content:'\\2713';position:absolute;top:10px;right:16px;font-size:24px;font-weight:800}\n"
".card .er{font-size:13px;line-height:1.3;margin-top:6px;font-weight:600;color:#E2664D}\n"
".card .ak{display:none;gap:6px;margin-top:10px}\n"
".card.ask .ak{display:flex}\n"
".card .ak button{flex:1;height:40px;border-radius:999px;border:2px solid var(--k);background:0;color:var(--k);\n"
"font:700 12px ui-monospace,Menlo,monospace;letter-spacing:1px;cursor:pointer;padding:0}\n"
".card .ak .yes{background:#000;border-color:#000;color:#EFEADD}\n"
".card.gone{opacity:0}\n"
"#btn{width:100%;padding:16px;border:0;border-radius:999px;background:#E9B23C;color:#000;\n"
"font:700 16px ui-monospace,Menlo,monospace;letter-spacing:3px;margin-top:20px;cursor:pointer}\n"
"#btn:disabled{opacity:.35;cursor:default}\n"
"#msg{margin-top:14px;text-align:center;color:#E2664D;font-weight:600;min-height:1.5em}\n"
"#kit{display:none}\n"
"#empty{color:#6E6C66;margin-top:8px}\n"
"</style></head><body>\n"
"<div class=logo><i class=sq></i><i class=ci></i><i class=tr></i></div>\n"
"<div class=wm>KIT</div>\n"
"<h1>Tools do KIT</h1>\n"
"<p class=sub>Digite o PIN que aparece na tela do KIT. Depois, arraste arquivos .kit pra instalar ou remova o que j&aacute; est&aacute; nele.</p>\n"
"<form id=f>\n"
"<span class=lb>PIN</span>\n"
"<input id=pin inputmode=numeric maxlength=4 autocomplete=off>\n"
"<div id=msg></div>\n"
"<span class=lb>INSTALAR</span>\n"
"<label id=dz for=file>Arraste os .kit aqui<small>OU TOQUE PRA ESCOLHER</small></label>\n"
"<input id=file type=file accept=.kit multiple>\n"
"<div class=grid id=grid></div>\n"
"<button id=btn disabled>INSTALAR</button>\n"
"</form>\n"
"<div id=kit><span class=lb id=kith>NO KIT</span><div class=grid id=inst></div><div id=empty></div></div>\n"
"<script>\n"
"const $=i=>document.getElementById(i);\n"
"const PAL=['#C6472F','#2C3CC4','#E9B23C','#45A05B'];\n"
"let items=[],busy=false,dc=0,lastPin='';\n"
"function msg(t){$('msg').textContent=t||''}\n"
"function pin(){return $('pin').value.trim()}\n"
"function hex(s){s=(s||'').replace('#','');return /^[0-9a-f]{6}$/i.test(s)?'#'+s:null}\n"
"function ink(c){const n=parseInt(c.slice(1),16),r=n>>16,g=n>>8&255,b=n&255;\n"
"return (0.299*r+0.587*g+0.114*b)>150?'#000':'#EFEADD'}\n"
"function h(s){let x=0;for(const ch of s)x=(x*31+ch.charCodeAt(0))|0;return Math.abs(x)}\n"
"function post(url,hd){return fetch(url,{method:'POST',headers:Object.assign({'X-Kit-Pin':pin()},hd||{})})\n"
".then(r=>r.json()).catch(()=>({ok:false,err:'O KIT n\303\243o respondeu'}))}\n"
"function card(o){const c=hex(o.accent)||PAL[h(o.key)%4],el=document.createElement('div');\n"
"el.className='card';el.style.setProperty('--c',c);el.style.setProperty('--k',ink(c));\n"
"el.innerHTML='<i class=\"sh '+o.shape+'\"></i><button type=button class=x title=Remover>&times;</button>'+\n"
"'<span class=nm></span><span class=mt></span><span class=er></span>'+\n"
"'<span class=ak><button type=button class=yes>REMOVER</button><button type=button class=no>N\303\203O</button></span>'+\n"
"'<span class=pg><i></i></span>';\n"
"el.querySelector('.nm').textContent=o.name;el.querySelector('.mt').textContent=o.meta;return el}\n"
"function meta(game,v){return (game?'mini-jogo':'ferramenta')+(v?'\\nv'+v:'')}\n"
"function isGame(k){return /^(game|minigame|minijogo)$/i.test(k||'')}\n"
"async function manifest(f){try{\n"
"const b=new Uint8Array(await f.arrayBuffer()),v=new DataView(b.buffer);\n"
"let e=b.length-22;while(e>=0&&v.getUint32(e,true)!=0x06054b50)e--;if(e<0)return null;\n"
"let n=v.getUint16(e+10,true),p=v.getUint32(e+16,true);\n"
"for(let i=0;i<n;i++){if(v.getUint32(p,true)!=0x02014b50)return null;\n"
"const m=v.getUint16(p+10,true),cs=v.getUint32(p+20,true),nl=v.getUint16(p+28,true),\n"
"xl=v.getUint16(p+30,true),cl=v.getUint16(p+32,true),lo=v.getUint32(p+42,true);\n"
"if(new TextDecoder().decode(b.subarray(p+46,p+46+nl))=='manifest.json'){\n"
"const d=lo+30+v.getUint16(lo+26,true)+v.getUint16(lo+28,true);let data=b.subarray(d,d+cs);\n"
"if(m==8)data=new Uint8Array(await new Response(new Blob([data]).stream()\n"
".pipeThrough(new DecompressionStream('deflate-raw'))).arrayBuffer());else if(m)return null;\n"
"return JSON.parse(new TextDecoder().decode(data))}\n"
"p+=46+nl+xl+cl}}catch(_){}return null}\n"
"\n"
"// -- Instalar -------------------------------------------------------------\n"
"function refresh(){const k=items.filter(t=>t.st=='new'||t.st=='err').length;\n"
"$('btn').disabled=busy||!k;$('btn').textContent=busy?'ENVIANDO...':k>1?'INSTALAR '+k:'INSTALAR'}\n"
"function render(t){const m=t.m||{},g=isGame(m.kind);\n"
"t.meta=t.m?meta(g,m.version):'sem manifest';\n"
"const el=card({accent:m.accent,key:m.id||t.f.name,shape:t.m?(g?'tr':'ci'):'sq',name:m.name||t.f.name,meta:t.meta});\n"
"el.querySelector('.x').onclick=()=>{items=items.filter(o=>o!=t);el.remove();refresh()};\n"
"t.el=el;return el}\n"
"async function pick(fs){msg('');for(const f of fs){\n"
"if(!/\\.kit$/i.test(f.name)){msg('S\303\263 arquivos .kit');continue}\n"
"const t={f,st:'new',m:await manifest(f)};\n"
"const dup=items.find(o=>o.st!='sending'&&(t.m&&o.m?o.m.id==t.m.id:o.f.name==f.name));\n"
"if(dup){items=items.filter(o=>o!=dup);dup.el.remove()}\n"
"items.push(t);$('grid').append(render(t))}\n"
"refresh()}\n"
"function setSt(t,st,text,er){t.st=st;t.el.className='card '+st;\n"
"t.el.querySelector('.mt').textContent=text;t.el.querySelector('.er').textContent=er||''}\n"
"function send(t){return new Promise(res=>{const x=new XMLHttpRequest(),bar=t.el.querySelector('.pg i');\n"
"x.open('POST','/upload');x.setRequestHeader('X-Kit-Pin',pin());\n"
"x.setRequestHeader('Content-Type','application/octet-stream');\n"
"x.upload.onprogress=e=>{if(e.lengthComputable)bar.style.width=(e.loaded*100/e.total)+'%'};\n"
"x.onload=()=>{bar.style.width='0';let j={ok:false,err:'Resposta estranha do KIT'};\n"
"try{j=JSON.parse(x.responseText)}catch(_){}res(j)};\n"
"x.onerror=()=>{bar.style.width='0';res({ok:false,err:'A conex\303\243o caiu'})};x.send(t.f)})}\n"
"async function go(){if(busy)return;\n"
"const todo=items.filter(t=>t.st=='new'||t.st=='err');if(!todo.length)return;\n"
"if(pin().length!=4){$('pin').focus();msg('Digite o PIN da tela do KIT');return}\n"
"busy=true;msg('');refresh();let any=false;\n"
"const c=await post('/check');\n"
"if(!c.ok)msg(c.err);\n"
"else for(const t of todo){setSt(t,'sending','enviando...');const r=await send(t);\n"
"if(r.ok){any=true;setSt(t,'ok','instalada\\nv'+r.version);fade(t.el,()=>{items=items.filter(o=>o!=t)})}\n"
"else{setSt(t,'err',t.meta,r.err);if(r.locked){msg(r.err);break}}}\n"
"busy=false;refresh();if(any)load()}\n"
"function fade(el,done){setTimeout(()=>{el.classList.add('gone');\n"
"setTimeout(()=>{el.remove();done&&done()},300)},2500)}\n"
"\n"
"// -- No KIT ---------------------------------------------------------------\n"
"async function load(){if(pin().length!=4)return;lastPin=pin();\n"
"const r=await post('/tools');\n"
"if(!r.ok){msg(r.err);$('kit').style.display='none';return}\n"
"msg('');const ts=r.tools.sort((a,b)=>a.name.localeCompare(b.name,'pt'));\n"
"$('kith').textContent='NO KIT \302\267 '+ts.length;$('inst').innerHTML='';\n"
"$('empty').textContent=ts.length?'':'Nenhuma Tool no cart\303\243o ainda.';\n"
"for(const t of ts){const el=card({accent:t.accent,key:t.id,shape:t.game?'tr':'ci',name:t.name,meta:meta(t.game,t.version)});\n"
"el.querySelector('.x').onclick=()=>{if(!busy){el.classList.remove('err');el.querySelector('.er').textContent='';el.classList.add('ask')}};\n"
"el.querySelector('.no').onclick=()=>el.classList.remove('ask');\n"
"el.querySelector('.yes').onclick=()=>remove(t,el);\n"
"$('inst').append(el)}\n"
"$('kit').style.display='block'}\n"
"async function remove(t,el){if(busy)return;busy=true;refresh();\n"
"el.querySelector('.yes').textContent='...';\n"
"const r=await post('/remove',{'X-Kit-Id':t.id});\n"
"busy=false;refresh();\n"
"if(r.ok){el.classList.add('gone');setTimeout(load,300)}\n"
"else{el.classList.remove('ask');el.classList.add('err');el.querySelector('.yes').textContent='REMOVER';\n"
"el.querySelector('.er').textContent=r.err;if(r.locked)msg(r.err)}}\n"
"\n"
"$('f').onsubmit=e=>{e.preventDefault();go()};\n"
"$('pin').oninput=()=>{if(pin().length==4&&pin()!=lastPin)load()};\n"
"$('file').onchange=e=>{pick([...e.target.files]);e.target.value=''};\n"
"document.addEventListener('dragenter',e=>{e.preventDefault();dc++;$('dz').classList.add('over')});\n"
"document.addEventListener('dragleave',()=>{if(--dc<=0){dc=0;$('dz').classList.remove('over')}});\n"
"document.addEventListener('dragover',e=>e.preventDefault());\n"
"document.addEventListener('drop',async e=>{e.preventDefault();dc=0;$('dz').classList.remove('over');\n"
"await pick([...e.dataTransfer.files]);if(pin().length==4)go()});\n"
"</script></body></html>\n";

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

// Tools do cartão (as built-in não saem). accent "" = a página escolhe a cor.
static esp_err_t h_tools(httpd_req_t *req)
{
    s_activity++;
    if (!pin_ok(req)) return pin_refused(req);

    cJSON *root = cJSON_CreateObject();
    cJSON *arr = cJSON_AddArrayToObject(root, "tools");
    uint32_t n = kit_tool_manager_get_count();
    for (uint32_t i = 0; i < n; i++) {
        kit_tool_entry_t e;
        if (kit_tool_manager_get_entry(i, &e) != KIT_OK) continue;
        char accent[10] = "";
        if (e.accent) snprintf(accent, sizeof(accent), "#%06lX", (unsigned long)(e.accent & 0xFFFFFF));
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "id", e.id);
        cJSON_AddStringToObject(o, "name", e.name);
        cJSON_AddStringToObject(o, "version", e.version);
        cJSON_AddStringToObject(o, "accent", accent);
        cJSON_AddBoolToObject(o, "game", e.is_game);
        cJSON_AddItemToArray(arr, o);
    }
    cJSON_AddBoolToObject(root, "ok", true);
    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!body) return httpd_resp_send_500(req);
    esp_err_t r = send_json(req, NULL, body);
    cJSON_free(body);
    return r;
}

static esp_err_t h_remove(httpd_req_t *req)
{
    s_activity++;
    if (!pin_ok(req)) return pin_refused(req);

    char id[40] = {0};
    if (httpd_req_get_hdr_value_str(req, "X-Kit-Id", id, sizeof(id)) != ESP_OK || !id[0]) {
        return send_err(req, "400 Bad Request", "Qual Tool?");
    }
    char name[32] = "";
    uint32_t n = kit_tool_manager_get_count();
    for (uint32_t i = 0; i < n; i++) {
        kit_tool_entry_t e;
        if (kit_tool_manager_get_entry(i, &e) == KIT_OK && strcmp(e.id, id) == 0) {
            strlcpy(name, e.name, sizeof(name));
            break;
        }
    }
    if (!name[0]) return send_err(req, "404 Not Found", "Essa Tool n\xC3\xA3o est\xC3\xA1 no KIT");

    kit_err_t r = kit_tool_manager_uninstall(id);
    if (r != KIT_OK) {
        fail(r == KIT_FAIL ? "N\xC3\xA3o consegui remover (Tool aberta?)"
                           : "N\xC3\xA3o consegui remover");
        return send_err(req, "500 Internal Server Error", s_msg);
    }
    if (s_removed_cb) s_removed_cb(id);
    strlcpy(s_msg, name, sizeof(s_msg));
    s_state = KIT_UP_REMOVED;
    ESP_LOGI(TAG, "removida: %s (%s)", name, id);
    return send_json(req, NULL, "{\"ok\":true}");
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
    cfg.max_uri_handlers = 6;
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
        { .uri = "/tools",  .method = HTTP_POST, .handler = h_tools },
        { .uri = "/remove", .method = HTTP_POST, .handler = h_remove },
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
void kit_upload_set_removed_cb(kit_upload_removed_cb_t cb) { s_removed_cb = cb; }
