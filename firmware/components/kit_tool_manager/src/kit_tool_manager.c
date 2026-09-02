#include "kit_tool_manager.h"
#include "kit_api.h"
#include "kit_runtime.h"
#include "kit_audio.h"
#include "kit_dice.h"
#include "kit_bottle.h"
#include "kit_coin.h"
#include "kit_timer.h"
#include "kit_primeiro.h"
#include "kit_times.h"
#include "kit_bingo.h"
#include "kit_quebragelo.h"
#include "kit_fonts.h"
#include "kit_theme.h"
#include "kit_tool_loader.h"
#include "kit_pkg.h"
#include "esp_log.h"
#include "lvgl.h"
#include "cJSON.h"
#include "mbedtls/sha256.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *TAG = "KIT_TOOL_MGR";

// -- Catálogo dinâmico de Tools do cartão microSD (Marco 2) -----------------
// Varredura de /sdcard/tools/<pasta>/manifest.json na inicialização. Cada
// entrada válida e compatível vira um item no catálogo; o Launcher usa
// kit_tool_manager_get_count/get_entry para desenhar os cards dinâmicos.
#define KIT_TOOL_CATALOG_MAX  8
#define KIT_TOOL_SD_TOOLS_DIR "/sdcard/tools"

typedef struct {
    char id[40];
    char name[32];
    char version[16];
    uint32_t version_code; // manifest "version_code", 0 = ausente
    char entry_rel[96];   // caminho pro dlopen, relativo à base ("/sdcard")
    uint32_t accent;      // cor do card na Home (0xRRGGBB), 0 = não declarada
    char icon[16];        // nome do ícone da Home, "" = genérico
    bool is_game;         // manifest "kind":"game" -> mini-jogo
} kit_tool_catalog_entry_t;

static kit_tool_catalog_entry_t s_catalog[KIT_TOOL_CATALOG_MAX];
static uint32_t s_catalog_n = 0;
static kit_tool_catalog_changed_cb_t s_catalog_changed_cb = NULL;

static char s_current_tool[40] = {0};
static char s_last_tool[40] = {0};
static lv_obj_t *s_test_tool_screen = NULL;
static lv_obj_t *s_touch_val_lbl = NULL;
static lv_obj_t *s_random_val_lbl = NULL;
static uint32_t s_tap_count = 0;

// A Test Tool segue a linguagem "Brutalist Bauhaus" do Launcher
// (ver docs/design/design-language.md): fundo AMOLED preto, tipografia
// monoespaçada em caixa alta, uma linha de diagnóstico por subsistema
// (chave à esquerda, valor à direita, fio embaixo — igual à tabela de
// specs da tela "Sobre") e uma pílula vermelha "SAIR".
#define TT_PAD      16
#define TT_CONTENT  (368 - 2 * TT_PAD)

static lv_obj_t *tt_label(lv_obj_t *parent, const char *txt, uint32_t color,
                          const lv_font_t *font, int letter_space)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(l, font, 0);
    if (letter_space) lv_obj_set_style_text_letter_space(l, letter_space, 0);
    return l;
}

// Devolve o label do valor (à direita) para os testes que atualizam ao vivo.
static lv_obj_t *tt_row(lv_obj_t *parent, const char *key, const char *val,
                        uint32_t val_color)
{
    lv_obj_t *r = lv_obj_create(parent);
    lv_obj_remove_style_all(r);
    lv_obj_set_size(r, TT_CONTENT, 52);
    lv_obj_set_style_border_width(r, 1, 0);
    lv_obj_set_style_border_side(r, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(r, lv_color_hex(KIT_COLOR_LINE), 0);
    lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *k = tt_label(r, key, KIT_COLOR_TEXT_MUTED, &kit_mono_16, 1);
    lv_obj_align(k, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t *v = tt_label(r, val, val_color, &kit_mono_16, 0);
    lv_obj_align(v, LV_ALIGN_RIGHT_MID, 0, 0);
    return v;
}

// Linha "SOM" do diagnóstico: toca ao tocar. Re-sonda o ES8311 e emite um
// tom longo e contínuo (era o botão "Testar som" dos Ajustes).
static void test_tool_sound_cb(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "Test Tool: teste de som.");
    kit_audio_selftest_impl();
}

static void tt_sound_row(lv_obj_t *parent)
{
    lv_obj_t *r = lv_obj_create(parent);
    lv_obj_remove_style_all(r);
    lv_obj_set_size(r, TT_CONTENT, 52);
    lv_obj_set_style_border_width(r, 1, 0);
    lv_obj_set_style_border_side(r, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(r, lv_color_hex(KIT_COLOR_LINE), 0);
    lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(r, 6);
    lv_obj_add_event_cb(r, test_tool_sound_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *k = tt_label(r, "SOM", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 1);
    lv_obj_align(k, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t *v = tt_label(r, "TOCAR TOM", KIT_COLOR_GREEN, &kit_mono_16, 0);
    lv_obj_align(v, LV_ALIGN_RIGHT_MID, 0, 0);
}

static void test_tool_exit_cb(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "Test Tool: saindo.");
    kit_system_exit_impl();
}

static void test_tool_touch_cb(lv_event_t *e)
{
    (void)e;
    s_tap_count++;
    lv_point_t p = {0};
    lv_indev_get_point(lv_indev_active(), &p);

    if (s_touch_val_lbl) {
        char buf[40];
        snprintf(buf, sizeof(buf), "X%d Y%d  %lu",
                 (int)p.x, (int)p.y, (unsigned long)s_tap_count);
        lv_label_set_text(s_touch_val_lbl, buf);
        lv_obj_set_style_text_color(s_touch_val_lbl, lv_color_hex(KIT_COLOR_GREEN), 0);
        lv_obj_align(s_touch_val_lbl, LV_ALIGN_RIGHT_MID, 0, 0);
    }
    if (s_random_val_lbl) {
        char buf[20];
        uint32_t r = kit_api_get_table()->random->u32();
        snprintf(buf, sizeof(buf), "0x%08lX", (unsigned long)r);
        lv_label_set_text(s_random_val_lbl, buf);
        lv_obj_align(s_random_val_lbl, LV_ALIGN_RIGHT_MID, 0, 0);
    }
}

static kit_err_t run_internal_test_tool(void)
{
    ESP_LOGI(TAG, "Montando UI da Test Tool (Brutalist Bauhaus)...");

    s_tap_count = 0;
    s_test_tool_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_test_tool_screen, lv_color_hex(KIT_COLOR_BG), 0);
    lv_obj_clear_flag(s_test_tool_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_test_tool_screen, test_tool_touch_cb, LV_EVENT_CLICKED, NULL);

    // Cabeçalho — a saída é o botão SAIR ou o botão físico BOOT.
    lv_obj_t *ttl = tt_label(s_test_tool_screen, "TESTES", KIT_COLOR_TEXT, &kit_mono_26, 3);
    lv_obj_align(ttl, LV_ALIGN_TOP_LEFT, TT_PAD, 30);
    lv_obj_t *sub = tt_label(s_test_tool_screen, "DIAGNOSTICO DO SISTEMA",
                             KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);
    lv_obj_align(sub, LV_ALIGN_TOP_LEFT, TT_PAD, 66);

    // Corpo rolável entre o cabeçalho e a pílula fixa do rodapé.
    lv_obj_t *body = lv_obj_create(s_test_tool_screen);
    lv_obj_remove_style_all(body);
    lv_obj_set_size(body, 368, 448 - 104 - (KIT_TOUCH_TARGET_COMFORTABLE + 28));
    lv_obj_set_pos(body, 0, 104);
    lv_obj_set_style_pad_hor(body, TT_PAD, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_AUTO);

    const kit_api_table_t *api = kit_api_get_table();

    // Storage API — escreve e relê.
    api->storage->set_str("test_key", "KIT_OK");
    char sv[32] = {0};
    api->storage->get_str("test_key", sv, sizeof(sv));
    bool storage_ok = (strcmp(sv, "KIT_OK") == 0);

    kit_system_info_t info = {0};
    if (api->system) api->system->get_info(&info);
    char batt[12];
    snprintf(batt, sizeof(batt), "%d%%", info.battery_percentage);

    char rnd[20];
    snprintf(rnd, sizeof(rnd), "0x%08lX", (unsigned long)api->random->u32());

    tt_row(body, "RUNTIME", "OK v0.1.0", KIT_COLOR_GREEN);
    tt_row(body, "DISPLAY", "CO5300 OK", KIT_COLOR_GREEN);
    s_touch_val_lbl  = tt_row(body, "TOUCH", "TOQUE NA TELA", KIT_COLOR_TEXT_MUTED);
    tt_row(body, "STORAGE", storage_ok ? "OK" : "FALHA",
           storage_ok ? KIT_COLOR_GREEN : KIT_COLOR_RED);
    s_random_val_lbl = tt_row(body, "RANDOM", rnd, KIT_COLOR_TEXT);
    tt_row(body, "BATERIA", batt, KIT_COLOR_TEXT);
    tt_sound_row(body);

    // Pílula "SAIR" — ação destrutiva (vermelho), alvo confortável.
    lv_obj_t *exit_btn = lv_obj_create(s_test_tool_screen);
    lv_obj_set_size(exit_btn, TT_CONTENT, KIT_TOUCH_TARGET_COMFORTABLE);
    lv_obj_set_style_bg_color(exit_btn, lv_color_hex(KIT_COLOR_RED), 0);
    lv_obj_set_style_border_width(exit_btn, 0, 0);
    lv_obj_set_style_radius(exit_btn, KIT_TOUCH_TARGET_COMFORTABLE / 2, 0);
    lv_obj_set_style_pad_all(exit_btn, 0, 0);
    lv_obj_clear_flag(exit_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(exit_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(exit_btn, 8);
    lv_obj_align(exit_btn, LV_ALIGN_BOTTOM_MID, 0, -14);
    lv_obj_add_event_cb(exit_btn, test_tool_exit_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_center(tt_label(exit_btn, "SAIR", KIT_COLOR_ON_COLOR, &kit_mono_20, 3));

    lv_screen_load(s_test_tool_screen);
    return KIT_OK;
}

// Compara versões "MAJOR.MINOR.PATCH". Devolve <0, 0 ou >0 como strcmp.
static bool parse_semver(const char *s, int *maj, int *min, int *pat)
{
    return s && sscanf(s, "%d.%d.%d", maj, min, pat) == 3;
}

static int semver_cmp(int amaj, int amin, int apat, int bmaj, int bmin, int bpat)
{
    if (amaj != bmaj) return amaj - bmaj;
    if (amin != bmin) return amin - bmin;
    return apat - bpat;
}

// Confere o SHA-256 de um arquivo contra um hex esperado (64 chars).
static bool verify_sha256(const char *path, const char *want_hex)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 2 * 1024 * 1024) { fclose(f); return false; }

    uint8_t *b = malloc((size_t)sz);
    if (!b) { fclose(f); return false; }
    size_t rd = fread(b, 1, (size_t)sz, f);
    fclose(f);
    if (rd != (size_t)sz) { free(b); return false; }

    uint8_t digest[32];
    int rc = mbedtls_sha256(b, (size_t)sz, digest, 0);
    free(b);
    if (rc != 0) return false;

    char hex[65];
    for (int i = 0; i < 32; i++) snprintf(hex + i * 2, 3, "%02x", digest[i]);
    return strcasecmp(hex, want_hex) == 0;
}

// Procura *.kit em `src_dir` e extrai cada um para /sdcard/tools/<stem>/ (só
// se ainda não houver manifest.json lá). O .kit fica onde está.
static void extract_kits_from(const char *src_dir)
{
    DIR *dir = opendir(src_dir);
    if (!dir) return;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        const char *n = ent->d_name;
        size_t len = strlen(n);
        if (n[0] == '.' || len < 5 || len > 96 ||
            strcasecmp(n + len - 4, ".kit") != 0) continue;

        char stem[96];
        snprintf(stem, sizeof(stem), "%.*s", (int)(len - 4), n);

        char dest[128];
        snprintf(dest, sizeof(dest), "%s/%s", KIT_TOOL_SD_TOOLS_DIR, stem);

        char check[160];
        snprintf(check, sizeof(check), "%s/manifest.json", dest);
        struct stat st;
        if (stat(check, &st) == 0) {
            ESP_LOGI(TAG, "Pacote '%s' já extraído em %s — pulando", n, dest);
            continue;
        }

        char kitpath[160];
        snprintf(kitpath, sizeof(kitpath), "%s/%s.kit", src_dir, stem);
        ESP_LOGI(TAG, "Extraindo pacote '%s'...", kitpath);
        if (kit_pkg_extract(kitpath, dest) != KIT_OK) {
            ESP_LOGW(TAG, "Falha ao extrair '%s' — pacote ignorado", n);
        }
    }
    closedir(dir);
}

// Descompacta pacotes .kit largados na raiz do cartão OU em /sdcard/tools/.
static void extract_pending_kits(void)
{
    extract_kits_from("/sdcard");
    extract_kits_from(KIT_TOOL_SD_TOOLS_DIR);
}

// Lista o conteúdo de um diretório do cartão no log (diagnóstico de cartão).
static void log_dir(const char *path)
{
    DIR *d = opendir(path);
    if (!d) {
        ESP_LOGI(TAG, "  %s/  (inacessível)", path);
        return;
    }
    struct dirent *e;
    int n = 0;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        ESP_LOGI(TAG, "  %s/%s%s", path, e->d_name, (e->d_type == DT_DIR) ? "/" : "");
        n++;
    }
    if (n == 0) ESP_LOGI(TAG, "  %s/  (vazio)", path);
    closedir(d);
}

// Converte "#RRGGBB" / "RRGGBB" num 0xRRGGBB. Devolve 0 se não parsear (0 =
// "sem cor" para o Launcher, que então usa a paleta rotativa).
static uint32_t parse_hex_color(const char *s)
{
    if (!s) return 0;
    if (*s == '#') s++;
    if (strlen(s) != 6) return 0;
    uint32_t v = 0;
    for (int i = 0; i < 6; i++) {
        char c = s[i];
        int d;
        if      (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else return 0;
        v = (v << 4) | (uint32_t)d;
    }
    return v ? v : 0;
}

// Lê manifest.json de /sdcard/tools/<dirname>/ e, se válido e compatível,
// adiciona ao catálogo. Qualquer problema só pula a pasta (log de aviso) —
// um manifest ruim de uma Tool não pode impedir as outras de aparecerem.
static void load_manifest(const char *dirname)
{
    if (s_catalog_n >= KIT_TOOL_CATALOG_MAX) {
        ESP_LOGW(TAG, "Catálogo de Tools do SD cheio (max %d), ignorando '%s'",
                 KIT_TOOL_CATALOG_MAX, dirname);
        return;
    }

    char path[128];
    snprintf(path, sizeof(path), "%s/%s/manifest.json", KIT_TOOL_SD_TOOLS_DIR, dirname);

    FILE *f = fopen(path, "r");
    if (!f) return;   // pasta sem manifest.json: não é uma Tool, ignora

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0 || size > 8192) {
        ESP_LOGW(TAG, "manifest.json de '%s' com tamanho suspeito (%ld B)", dirname, size);
        fclose(f);
        return;
    }

    char *buf = malloc((size_t)size + 1);
    if (!buf) { fclose(f); return; }
    size_t read_n = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[read_n] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        ESP_LOGW(TAG, "manifest.json inválido (JSON) em '%s'", dirname);
        return;
    }

    cJSON *id_j    = cJSON_GetObjectItemCaseSensitive(root, "id");
    cJSON *name_j  = cJSON_GetObjectItemCaseSensitive(root, "name");
    cJSON *ver_j   = cJSON_GetObjectItemCaseSensitive(root, "version");
    cJSON *vc_j    = cJSON_GetObjectItemCaseSensitive(root, "version_code");
    cJSON *entry_j = cJSON_GetObjectItemCaseSensitive(root, "entry_point");
    cJSON *arch_j  = cJSON_GetObjectItemCaseSensitive(root, "arch");
    cJSON *min_j   = cJSON_GetObjectItemCaseSensitive(root, "min_runtime");
    cJSON *max_j   = cJSON_GetObjectItemCaseSensitive(root, "max_runtime");
    cJSON *csum_j  = cJSON_GetObjectItemCaseSensitive(root, "checksum");
    cJSON *acc_j   = cJSON_GetObjectItemCaseSensitive(root, "accent");
    cJSON *hicon_j = cJSON_GetObjectItemCaseSensitive(root, "home_icon");
    cJSON *kind_j  = cJSON_GetObjectItemCaseSensitive(root, "kind");

    if (!cJSON_IsString(id_j) || !id_j->valuestring[0] ||
        !cJSON_IsString(name_j) || !name_j->valuestring[0]) {
        ESP_LOGW(TAG, "manifest.json de '%s' sem 'id'/'name' válidos", dirname);
        cJSON_Delete(root);
        return;
    }

    if (cJSON_IsString(arch_j) && strcmp(arch_j->valuestring, "xtensa-esp32s3") != 0) {
        ESP_LOGW(TAG, "Tool '%s': arch '%s' não suportada (só xtensa-esp32s3) — ignorada",
                 id_j->valuestring, arch_j->valuestring);
        cJSON_Delete(root);
        return;
    }

    int rmaj, rmin, rpat, a, b, c;
    parse_semver(KIT_VERSION_STRING, &rmaj, &rmin, &rpat);
    if (cJSON_IsString(min_j) && parse_semver(min_j->valuestring, &a, &b, &c) &&
        semver_cmp(rmaj, rmin, rpat, a, b, c) < 0) {
        ESP_LOGW(TAG, "Tool '%s': exige runtime >= %s (atual %s) — ignorada",
                 id_j->valuestring, min_j->valuestring, KIT_VERSION_STRING);
        cJSON_Delete(root);
        return;
    }
    if (cJSON_IsString(max_j) && parse_semver(max_j->valuestring, &a, &b, &c) &&
        semver_cmp(rmaj, rmin, rpat, a, b, c) > 0) {
        ESP_LOGW(TAG, "Tool '%s': exige runtime <= %s (atual %s) — ignorada",
                 id_j->valuestring, max_j->valuestring, KIT_VERSION_STRING);
        cJSON_Delete(root);
        return;
    }

    const char *entry_name =
        (cJSON_IsString(entry_j) && entry_j->valuestring[0]) ? entry_j->valuestring : "tool.so";

    // Integridade: se o manifest declara checksum ("sha256:<hex>"), o binário
    // precisa bater. Sem checksum (sideload manual), segue sem verificar.
    if (cJSON_IsString(csum_j) && strncmp(csum_j->valuestring, "sha256:", 7) == 0) {
        char bin_path[160];
        snprintf(bin_path, sizeof(bin_path), "%s/%s/%s", KIT_TOOL_SD_TOOLS_DIR, dirname, entry_name);
        if (!verify_sha256(bin_path, csum_j->valuestring + 7)) {
            ESP_LOGE(TAG, "Tool '%s': SHA-256 de '%s' não confere — IGNORADA",
                     id_j->valuestring, entry_name);
            cJSON_Delete(root);
            return;
        }
        ESP_LOGI(TAG, "Tool '%s': integridade (SHA-256) OK", id_j->valuestring);
    }

    kit_tool_catalog_entry_t *e = &s_catalog[s_catalog_n];
    snprintf(e->id, sizeof(e->id), "%s", id_j->valuestring);
    snprintf(e->name, sizeof(e->name), "%s", name_j->valuestring);
    snprintf(e->version, sizeof(e->version), "%s", cJSON_IsString(ver_j) ? ver_j->valuestring : "");
    e->version_code = cJSON_IsNumber(vc_j) && vc_j->valueint > 0 ? (uint32_t)vc_j->valueint : 0;
    snprintf(e->entry_rel, sizeof(e->entry_rel), "tools/%s/%s", dirname, entry_name);
    e->accent = cJSON_IsString(acc_j) ? parse_hex_color(acc_j->valuestring) : 0;
    if (cJSON_IsString(hicon_j) && hicon_j->valuestring[0])
        snprintf(e->icon, sizeof(e->icon), "%s", hicon_j->valuestring);
    else
        e->icon[0] = '\0';
    e->is_game = cJSON_IsString(kind_j) &&
                 (strcasecmp(kind_j->valuestring, "game") == 0 ||
                  strcasecmp(kind_j->valuestring, "minigame") == 0 ||
                  strcasecmp(kind_j->valuestring, "minijogo") == 0);
    s_catalog_n++;

    ESP_LOGI(TAG, "Tool no cartão: '%s' (%s) v%s -> %s",
             e->name, e->id, e->version, e->entry_rel);

    cJSON_Delete(root);
}

// Varre /sdcard/tools/*/manifest.json. Ausência do cartão ou da pasta não é
// erro — o catálogo fica vazio e o KIT segue só com as Tools built-in.
static void scan_sd_catalog(void)
{
    s_catalog_n = 0;

    // Garante a estrutura que o KIT espera — cartão "pronto pra uso" mesmo se
    // o usuário só copiou um .kit solto na raiz.
    mkdir(KIT_TOOL_SD_TOOLS_DIR, 0775);

    ESP_LOGI(TAG, "Conteúdo do cartão:");
    log_dir("/sdcard");
    log_dir(KIT_TOOL_SD_TOOLS_DIR);

    extract_pending_kits();   // descompacta .kit ainda não extraídos

    DIR *dir = opendir(KIT_TOOL_SD_TOOLS_DIR);
    if (!dir) {
        ESP_LOGI(TAG, "Sem %s (sem cartão ou sem pasta tools/) — catálogo do SD vazio.",
                 KIT_TOOL_SD_TOOLS_DIR);
        return;
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') continue;   // "." / ".." / ocultos
#ifdef DT_DIR
        if (ent->d_type != DT_DIR && ent->d_type != DT_UNKNOWN) continue;
#endif
        load_manifest(ent->d_name);
    }
    closedir(dir);

    ESP_LOGI(TAG, "Catálogo do cartão SD: %lu Tool(s) encontrada(s).",
             (unsigned long)s_catalog_n);
}

kit_err_t kit_tool_manager_init(void)
{
    ESP_LOGI(TAG, "Inicializando Tool Manager e varrendo %s...", KIT_TOOL_SD_TOOLS_DIR);
    scan_sd_catalog();
    return KIT_OK;
}

void kit_tool_manager_reload_catalog(void)
{
    ESP_LOGI(TAG, "Recarregando catálogo de Tools...");
    scan_sd_catalog();
    if (s_catalog_changed_cb) s_catalog_changed_cb();
}

void kit_tool_manager_set_catalog_changed_cb(kit_tool_catalog_changed_cb_t cb)
{
    s_catalog_changed_cb = cb;
}

uint32_t kit_tool_manager_get_count(void)
{
    return s_catalog_n;
}

kit_err_t kit_tool_manager_get_entry(uint32_t index, kit_tool_entry_t *entry)
{
    if (index >= s_catalog_n || !entry) return KIT_ERR_NOT_FOUND;
    const kit_tool_catalog_entry_t *c = &s_catalog[index];
    snprintf(entry->id, sizeof(entry->id), "%s", c->id);
    snprintf(entry->name, sizeof(entry->name), "%s", c->name);
    snprintf(entry->version, sizeof(entry->version), "%s", c->version);
    entry->version_code = c->version_code;
    entry->description[0] = '\0';
    entry->size_bytes = 0;
    entry->accent = c->accent;
    snprintf(entry->icon, sizeof(entry->icon), "%s", c->icon);
    entry->is_game = c->is_game;
    return KIT_OK;
}

kit_err_t kit_tool_manager_start(const char *tool_id)
{
    ESP_LOGI(TAG, "Iniciando Tool '%s'...", tool_id);

    kit_err_t err = KIT_ERR_NOT_FOUND;
    void (*primary_action)(void) = NULL;
    if (strcmp(tool_id, "com.kit.test") == 0) {
        err = run_internal_test_tool();
    } else if (strcmp(tool_id, "com.kit.dice") == 0) {
        err = kit_dice_start(KIT_COLOR_RED);   // cor do card "Dados" na Home
        primary_action = kit_dice_roll;        // PWR físico rola os dados
    } else if (strcmp(tool_id, "com.kit.bottle") == 0) {
        err = kit_bottle_start(KIT_COLOR_BLUE); // cor do card "Garrafa" na Home
        primary_action = kit_bottle_spin;      // PWR físico gira a seta
    } else if (strcmp(tool_id, "com.kit.coin") == 0) {
        err = kit_coin_start(KIT_COLOR_YELLOW); // cor do card "Moeda" na Home
        primary_action = kit_coin_flip;        // PWR físico sorteia cara/coroa
    } else if (strcmp(tool_id, "com.kit.timer") == 0) {
        err = kit_timer_start(KIT_COLOR_GREEN); // cor do card "Timer" na Home
        primary_action = kit_timer_toggle;     // PWR físico começa/pausa
    } else if (strcmp(tool_id, "com.kit.primeiro") == 0) {
        err = kit_primeiro_start(KIT_COLOR_YELLOW); // cor do card "Primeiro" na Home
        primary_action = kit_primeiro_draw;        // PWR físico sorteia
    } else if (strcmp(tool_id, "com.kit.times") == 0) {
        err = kit_times_start(KIT_COLOR_BLUE); // cor do card "Times" na Home
        primary_action = kit_times_draw;      // PWR físico sorteia / avança a revelação
    } else if (strcmp(tool_id, "com.kit.bingo") == 0) {
        err = kit_bingo_start(KIT_COLOR_GREEN); // cor do card "Bingo" na Home
        primary_action = kit_bingo_draw;       // PWR físico sorteia o próximo número
    } else if (strcmp(tool_id, "com.kit.quebragelo") == 0) {
        err = kit_quebragelo_start(KIT_COLOR_RED); // cor do card "Quebra-Gelo" na Home
        primary_action = kit_quebragelo_draw;       // PWR físico sorteia a próxima pergunta
    } else {
        // Não é uma Tool built-in: procura no catálogo do cartão SD (Marco 2)
        // pelo caminho exato do entry_point; sem manifest, cai no layout
        // padrão tools/<id>/tool.so (sideload manual, Marco 1).
        const kit_tool_catalog_entry_t *found = NULL;
        for (uint32_t i = 0; i < s_catalog_n; i++) {
            if (strcmp(s_catalog[i].id, tool_id) == 0) {
                found = &s_catalog[i];
                break;
            }
        }

        static char id_buf[32];
        static char data_buf[80];
        snprintf(id_buf, sizeof(id_buf), "%s", tool_id);
        snprintf(data_buf, sizeof(data_buf), "/sdcard/tools/%s", tool_id);
        static kit_tool_ctx_t ext_ctx;
        ext_ctx.tool_id   = id_buf;
        ext_ctx.data_path = data_buf;
        ext_ctx.api       = kit_api_get_table();

        char so_path[96];
        if (found) {
            snprintf(so_path, sizeof(so_path), "%s", found->entry_rel);
        } else {
            snprintf(so_path, sizeof(so_path), "tools/%s/tool.so", tool_id);
        }
        err = kit_tool_loader_start(so_path, &ext_ctx);
    }

    if (err == KIT_OK) {
        snprintf(s_current_tool, sizeof(s_current_tool), "%s", tool_id);
        snprintf(s_last_tool, sizeof(s_last_tool), "%s", tool_id);
        kit_runtime_set_in_tool(true);
        kit_runtime_set_tool_primary_action(primary_action);
    }
    return err;
}

void kit_tool_manager_start_last(void)
{
    if (s_last_tool[0] != '\0') {
        kit_tool_manager_start(s_last_tool);
    }
}

void kit_tool_manager_stop_current(void)
{
    ESP_LOGI(TAG, "Finalizando Tool ativa ('%s').", s_current_tool[0] ? s_current_tool : "-");
    kit_runtime_set_tool_primary_action(NULL);

    if (strcmp(s_current_tool, "com.kit.dice") == 0) {
        kit_dice_destroy();
    } else if (strcmp(s_current_tool, "com.kit.bottle") == 0) {
        kit_bottle_destroy();
    } else if (strcmp(s_current_tool, "com.kit.coin") == 0) {
        kit_coin_destroy();
    } else if (strcmp(s_current_tool, "com.kit.timer") == 0) {
        kit_timer_destroy();
    } else if (strcmp(s_current_tool, "com.kit.primeiro") == 0) {
        kit_primeiro_destroy();
    } else if (strcmp(s_current_tool, "com.kit.times") == 0) {
        kit_times_destroy();
    } else if (strcmp(s_current_tool, "com.kit.bingo") == 0) {
        kit_bingo_destroy();
    } else if (strcmp(s_current_tool, "com.kit.quebragelo") == 0) {
        kit_quebragelo_destroy();
    } else if (kit_tool_loader_is_active()) {
        kit_tool_loader_stop();
    } else if (s_test_tool_screen) {
        lv_obj_delete(s_test_tool_screen);
        s_test_tool_screen = NULL;
    }

    s_current_tool[0] = '\0';
}

const char *kit_tool_manager_current(void)
{
    return s_current_tool;
}

// Apaga recursivamente um diretório do cartão (arquivos + subpastas).
//
// O FatFs do ESP-IDF não garante um readdir() consistente enquanto a própria
// pasta é modificada: um f_unlink() no meio da varredura faz o próximo
// f_readdir() pular entradas, então um rm_rf() ingênuo deixava metade dos
// arquivos pra trás e o rmdir() final falhava (ENOTEMPTY). A Tool "some" da
// lista mas ressurge no próximo scan. Aqui a gente reabre a pasta a cada
// remoção: pega só a primeira entrada, fecha, apaga, repete até esvaziar.
static void rm_rf(const char *path)
{
    for (int guard = 0; guard < 4096; guard++) {
        DIR *d = opendir(path);
        if (!d) break;

        char name[128] = {0};
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (ent->d_name[0] == '.' &&
                (ent->d_name[1] == '\0' ||
                 (ent->d_name[1] == '.' && ent->d_name[2] == '\0'))) continue;
            strlcpy(name, ent->d_name, sizeof(name));
            break;
        }
        closedir(d);

        if (name[0] == '\0') break;   // pasta vazia

        char child[400];
        snprintf(child, sizeof(child), "%.256s/%.128s", path, name);
        struct stat st;
        if (stat(child, &st) == 0 && S_ISDIR(st.st_mode)) rm_rf(child);
        else if (unlink(child) != 0) {
            ESP_LOGW(TAG, "unlink('%s') falhou: errno=%d", child, errno);
            break;   // não trava num arquivo que não sai
        }
    }
    rmdir(path);
}

kit_err_t kit_tool_manager_install(const char *kit_path, const char *tool_id)
{
    if (!kit_path || !tool_id || !tool_id[0]) return KIT_ERR_INVALID_ARG;

    char dest[160];
    snprintf(dest, sizeof(dest), "%s/%.96s", KIT_TOOL_SD_TOOLS_DIR, tool_id);

    struct stat st;
    if (stat(dest, &st) == 0) {
        ESP_LOGI(TAG, "Atualizando '%s' — removendo versão anterior", tool_id);
        rm_rf(dest);
    }

    ESP_LOGI(TAG, "Instalando '%s' de '%s' -> %s", tool_id, kit_path, dest);
    kit_err_t r = kit_pkg_extract(kit_path, dest);
    unlink(kit_path);   // o .kit já cumpriu o papel; libera o cartão

    if (r != KIT_OK) {
        ESP_LOGE(TAG, "Falha ao extrair '%s'", tool_id);
        rm_rf(dest);    // não deixa uma instalação pela metade
        return r;
    }

    kit_tool_manager_reload_catalog();
    return KIT_OK;
}

kit_err_t kit_tool_manager_uninstall(const char *tool_id)
{
    if (!tool_id || !tool_id[0]) return KIT_ERR_INVALID_ARG;
    if (strcmp(tool_id, s_current_tool) == 0) {
        ESP_LOGW(TAG, "Não dá pra remover '%s': está rodando", tool_id);
        return KIT_FAIL;
    }

    char dir[160];
    snprintf(dir, sizeof(dir), "%s/%.96s", KIT_TOOL_SD_TOOLS_DIR, tool_id);
    struct stat st;
    if (stat(dir, &st) != 0) return KIT_ERR_NOT_FOUND;

    ESP_LOGI(TAG, "Removendo Tool '%s' (%s)", tool_id, dir);
    rm_rf(dir);

    // O pacote .kit que originou a Tool (sideload manual) pode ter ficado no
    // cartão. Se sobrar, o extract_pending_kits() do próximo scan reinstala a
    // Tool na hora — parece que "não removeu". Apaga o .kit junto.
    char kit_file[176];
    snprintf(kit_file, sizeof(kit_file), "%s/%.96s.kit", KIT_TOOL_SD_TOOLS_DIR, tool_id);
    unlink(kit_file);
    snprintf(kit_file, sizeof(kit_file), "/sdcard/%.96s.kit", tool_id);
    unlink(kit_file);

    if (stat(dir, &st) == 0) {
        ESP_LOGE(TAG, "Tool '%s' não saiu do cartão (%s ainda existe)", tool_id, dir);
        kit_tool_manager_reload_catalog();
        return KIT_FAIL;
    }

    if (strcmp(tool_id, s_last_tool) == 0) s_last_tool[0] = '\0';
    kit_tool_manager_reload_catalog();
    return KIT_OK;
}
