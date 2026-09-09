#include "kit_soundbox.h"
#include "kit_api.h"
#include "kit_display.h"
#include "kit_fonts.h"
#include "kit_theme.h"
#include "kit_config.h"
#include "esp_log.h"
#include "lvgl.h"
#include "cJSON.h"

static const kit_api_table_t *api(void) { return kit_api_get_table(); }
static void rebuild_grid(void);

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>

// Soundbox — linguagem "Brutalist Bauhaus" (docs/design/design-system.html).
// Mesa de sons de mão. Três páginas:
//   0 BANCOS — volume (stepper) + lista dos conjuntos achados em
//              /sdcard/soundbox/<banco>/; toque escolhe e volta pros PADS.
//   1 PADS   — o palco: grade 3x3, um toque toca o .wav do pad (play_sample).
//              Um som novo corta o anterior. É a página inicial.
//   2 ADICIONAR SONS — passo a passo + QR do conversor + créditos dos exemplos.
//
// O banco escolhido persiste (índice em NVS "sb_bank"). Sons e metadados são
// SÓ lidos — a Tool nunca escreve no cartão.

static const char *TAG = "KIT_SOUNDBOX";

#define SB_CONVERTER_URL  "https://jcrvlh.github.io/kit/soundbox.html"

// ---------------------------------------------------------------------------
// Layout (métricas da Placar)
// ---------------------------------------------------------------------------
#define X_PAD         16
#define X_CONTENT     (KIT_DISPLAY_WIDTH - 2 * X_PAD)          // 336
#define X_CHIP        56
#define X_TITLEBAR    88
#define X_PAGE_H      (KIT_DISPLAY_HEIGHT - X_TITLEBAR)        // 360
#define PAGES         3

#define SB_ROOT        "/sdcard/soundbox"
#define SB_MAX_BANKS   12
#define SB_MAX_PADS    9
#define SB_DIR_LEN     40
#define SB_NAME_LEN    28
#define SB_FILE_LEN    64
#define SB_LABEL_LEN   18
#define SB_JSON_MAX    4096

#define K_BANK        "sb_bank"

#define FLASH_MS      140     // brilho do pad ao tocar

// ---------------------------------------------------------------------------
// Modelo
// ---------------------------------------------------------------------------
typedef struct {
    char     file[SB_FILE_LEN];       // "buzina.wav"
    char     label[SB_LABEL_LEN];     // "BUZINA"
    uint32_t color;                   // já resolvida (herda a do banco)
} sb_pad_t;

typedef struct {
    char     dir[SB_DIR_LEN];         // nome da subpasta
    char     name[SB_NAME_LEN];       // nome de exibição
    uint32_t color;                   // cor do banco
    int      npads;
    sb_pad_t pads[SB_MAX_PADS];
} sb_bank_t;

static uint32_t   s_accent   = KIT_COLOR_GREEN;
static sb_bank_t  s_banks[SB_MAX_BANKS];
static int        s_nbanks   = 0;
static int        s_cur      = -1;     // banco na grade, -1 = nenhum
static int        s_last_pad = -1;     // último pad tocado (PWR retoca)

// ---------------------------------------------------------------------------
// Objetos LVGL (todos zerados em destroy)
// ---------------------------------------------------------------------------
static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_tv = NULL;
static lv_obj_t *s_tiles[PAGES];
static lv_obj_t *s_dots[PAGES];
static lv_obj_t *s_bank_lbl = NULL;         // nome do banco no topo da grade
static lv_obj_t *s_grid = NULL;             // container dos pads
static lv_obj_t *s_pad_btn[SB_MAX_PADS];
static lv_obj_t *s_vol_lbl = NULL;          // valor do volume na página AJUDA
static lv_timer_t *s_flash_timer = NULL;
static int         s_flash_pad = -1;

// ---------------------------------------------------------------------------
// Helpers de UI
// ---------------------------------------------------------------------------
static lv_obj_t *add_label(lv_obj_t *parent, const char *txt, uint32_t color,
                           const lv_font_t *font, int letter_space)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(l, font, 0);
    if (letter_space) lv_obj_set_style_text_letter_space(l, letter_space, 0);
    return l;
}

static lv_obj_t *plain_box(lv_obj_t *parent)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

// Container rolável na vertical — plain_box tira o flag SCROLLABLE, então uma
// página que precisa rolar (BANCOS, AJUDA) usa este.
static lv_obj_t *scroll_box(lv_obj_t *parent)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_add_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(o, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(o, LV_SCROLLBAR_MODE_AUTO);
    return o;
}

static uint32_t on_color(uint32_t bg)
{
    return (bg == KIT_COLOR_YELLOW) ? KIT_COLOR_ON_YELLOW : KIT_COLOR_ON_COLOR;
}

// Cópia truncada e terminada — sem o -Wformat-truncation do snprintf("%s").
static void copy_str(char *dst, const char *src, size_t n)
{
    if (n == 0) return;
    size_t i = 0;
    for (; src && src[i] && i < n - 1; i++) dst[i] = src[i];
    dst[i] = '\0';
}

// Rótulo de pad em CAIXA ALTA (Bauhaus, fonte mono). Só ASCII a-z.
static void upcase(char *s)
{
    for (; *s; s++)
        if (*s >= 'a' && *s <= 'z') *s = (char)(*s - 32);
}

// ---------------------------------------------------------------------------
// Scan do cartão
// ---------------------------------------------------------------------------
static uint32_t color_from_name(const char *s, uint32_t fallback)
{
    if (!s) return fallback;
    if (strcasecmp(s, "vermelho") == 0 || strcasecmp(s, "red") == 0)    return KIT_COLOR_RED;
    if (strcasecmp(s, "azul") == 0     || strcasecmp(s, "blue") == 0)   return KIT_COLOR_BLUE;
    if (strcasecmp(s, "amarelo") == 0  || strcasecmp(s, "yellow") == 0) return KIT_COLOR_YELLOW;
    if (strcasecmp(s, "verde") == 0    || strcasecmp(s, "green") == 0)  return KIT_COLOR_GREEN;
    return fallback;
}

static bool ends_wav(const char *n)
{
    size_t l = strlen(n);
    return l > 4 && strcasecmp(n + l - 4, ".wav") == 0;
}

// "buzina.wav" -> "BUZINA" (cortado em SB_LABEL_LEN-1)
static void label_from_file(const char *file, char *out)
{
    size_t l = strlen(file);
    if (l > 4 && strcasecmp(file + l - 4, ".wav") == 0) l -= 4;
    size_t j = 0;
    for (size_t i = 0; i < l && j < SB_LABEL_LEN - 1; i++) {
        char c = file[i];
        if (c == '_' || c == '-') c = ' ';
        out[j++] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
    }
    out[j] = '\0';
}

static bool pad_has_file(const sb_bank_t *b, const char *file)
{
    for (int i = 0; i < b->npads; i++)
        if (strcasecmp(b->pads[i].file, file) == 0) return true;
    return false;
}

static bool file_exists(const char *dir, const char *file)
{
    char d[SB_DIR_LEN], f[SB_FILE_LEN];
    char p[SB_DIR_LEN + SB_FILE_LEN + 24];
    copy_str(d, dir, sizeof d);
    copy_str(f, file, sizeof f);
    snprintf(p, sizeof(p), SB_ROOT "/%s/%s", d, f);
    struct stat st;
    return stat(p, &st) == 0 && S_ISREG(st.st_mode);
}

// Lê o banco.json (se houver) e preenche name/color/pads na ordem dele.
static void load_banco_json(sb_bank_t *b)
{
    char path[256];
    snprintf(path, sizeof(path), SB_ROOT "/%s/banco.json", b->dir);
    FILE *f = fopen(path, "rb");
    if (!f) return;

    static char buf[SB_JSON_MAX];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    if (n == 0) return;
    buf[n] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        ESP_LOGW(TAG, "banco.json inválido em '%s'", b->dir);
        return;
    }

    const cJSON *nome = cJSON_GetObjectItemCaseSensitive(root, "nome");
    if (cJSON_IsString(nome) && nome->valuestring[0])
        copy_str(b->name, nome->valuestring, sizeof(b->name));

    const cJSON *cor = cJSON_GetObjectItemCaseSensitive(root, "cor");
    if (cJSON_IsString(cor))
        b->color = color_from_name(cor->valuestring, b->color);

    const cJSON *pads = cJSON_GetObjectItemCaseSensitive(root, "pads");
    if (cJSON_IsArray(pads)) {
        const cJSON *p;
        cJSON_ArrayForEach(p, pads) {
            if (b->npads >= SB_MAX_PADS) break;
            const cJSON *arq = cJSON_GetObjectItemCaseSensitive(p, "arquivo");
            if (!cJSON_IsString(arq) || !arq->valuestring[0]) continue;
            if (!file_exists(b->dir, arq->valuestring)) continue;
            if (pad_has_file(b, arq->valuestring)) continue;

            sb_pad_t *pad = &b->pads[b->npads++];
            copy_str(pad->file, arq->valuestring, sizeof(pad->file));
            const cJSON *rot = cJSON_GetObjectItemCaseSensitive(p, "rotulo");
            if (cJSON_IsString(rot) && rot->valuestring[0]) {
                copy_str(pad->label, rot->valuestring, sizeof(pad->label));
                upcase(pad->label);
            } else {
                label_from_file(pad->file, pad->label);
            }
            const cJSON *pc = cJSON_GetObjectItemCaseSensitive(p, "cor");
            pad->color = cJSON_IsString(pc)
                ? color_from_name(pc->valuestring, b->color) : b->color;
        }
    }

    cJSON_Delete(root);
}

// Preenche a grade de um banco: banco.json primeiro (ordem dele), depois todo
// *.wav restante da pasta em ordem alfabética, até 9.
static void load_bank(sb_bank_t *b)
{
    b->npads = 0;
    b->color = s_accent;
    copy_str(b->name, b->dir, sizeof(b->name));

    load_banco_json(b);

    char dpath[SB_DIR_LEN + 24];
    snprintf(dpath, sizeof(dpath), SB_ROOT "/%s", b->dir);
    DIR *d = opendir(dpath);
    if (!d) return;

    // coleta os nomes restantes e ordena (bubble — no máx. algumas dezenas)
    static char names[SB_MAX_PADS * 3][SB_FILE_LEN];
    int nn = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL && nn < (int)(sizeof(names) / sizeof(names[0]))) {
        char nm[SB_FILE_LEN];
        copy_str(nm, e->d_name, sizeof(nm));
        if (!ends_wav(nm)) continue;
        if (nm[0] == '.') continue;                 // ._resource forks do macOS
        if (pad_has_file(b, nm)) continue;
        bool dup = false;                           // FatFS pode repetir entrada
        for (int k = 0; k < nn; k++)
            if (strcasecmp(names[k], nm) == 0) { dup = true; break; }
        if (dup) continue;
        copy_str(names[nn++], nm, SB_FILE_LEN);
    }
    closedir(d);

    for (int i = 0; i < nn - 1; i++)
        for (int j = 0; j < nn - 1 - i; j++)
            if (strcasecmp(names[j], names[j + 1]) > 0) {
                char tmp[SB_FILE_LEN];
                strcpy(tmp, names[j]);
                strcpy(names[j], names[j + 1]);
                strcpy(names[j + 1], tmp);
            }

    for (int i = 0; i < nn && b->npads < SB_MAX_PADS; i++) {
        sb_pad_t *pad = &b->pads[b->npads++];
        copy_str(pad->file, names[i], sizeof(pad->file));
        label_from_file(pad->file, pad->label);
        pad->color = b->color;
    }
}

static void scan_banks(void)
{
    s_nbanks = 0;
    DIR *d = opendir(SB_ROOT);
    if (!d) {
        ESP_LOGI(TAG, "sem " SB_ROOT " (sem cartão ou sem a pasta)");
        return;
    }
    struct dirent *e;
    while ((e = readdir(d)) != NULL && s_nbanks < SB_MAX_BANKS) {
        if (e->d_name[0] == '.') continue;
#ifdef DT_DIR
        if (e->d_type != DT_DIR && e->d_type != DT_UNKNOWN) continue;
#endif
        char name[SB_DIR_LEN];
        copy_str(name, e->d_name, sizeof(name));
        char p[SB_DIR_LEN + 24];
        snprintf(p, sizeof(p), SB_ROOT "/%s", name);
        struct stat st;
        if (stat(p, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        sb_bank_t *b = &s_banks[s_nbanks];
        memset(b, 0, sizeof(*b));
        copy_str(b->dir, name, sizeof(b->dir));
        load_bank(b);
        if (b->npads > 0) s_nbanks++;   // banco vazio não entra na lista
    }
    closedir(d);
    ESP_LOGI(TAG, "%d banco(s) em " SB_ROOT, s_nbanks);
}

// ---------------------------------------------------------------------------
// Tocar
// ---------------------------------------------------------------------------
static void flash_restore(void)
{
    if (s_flash_pad >= 0 && s_flash_pad < SB_MAX_PADS && s_pad_btn[s_flash_pad] &&
        s_cur >= 0 && s_flash_pad < s_banks[s_cur].npads) {
        lv_obj_set_style_bg_color(s_pad_btn[s_flash_pad],
            lv_color_hex(s_banks[s_cur].pads[s_flash_pad].color), 0);
    }
    s_flash_pad = -1;
}

static void flash_end_cb(lv_timer_t *t)
{
    (void)t;
    flash_restore();
    s_flash_timer = NULL;   // repeat_count esgotou — o LVGL deleta sozinho
}

static void play_pad(int i)
{
    if (s_cur < 0 || i < 0 || i >= s_banks[s_cur].npads) return;
    const sb_bank_t *b = &s_banks[s_cur];

    char path[256];
    snprintf(path, sizeof(path), SB_ROOT "/%s/%s", b->dir, b->pads[i].file);
    const kit_api_table_t *t = api();
    if (t && t->audio && t->audio->play_sample) t->audio->play_sample(path);
    s_last_pad = i;

    // flash branco curto pra confirmar o toque (o som pode ser baixo)
    if (i < SB_MAX_PADS && s_pad_btn[i]) {
        if (s_flash_timer) { lv_timer_delete(s_flash_timer); s_flash_timer = NULL; }
        flash_restore();
        s_flash_pad = i;
        lv_obj_set_style_bg_color(s_pad_btn[i], lv_color_hex(KIT_COLOR_TEXT), 0);
        s_flash_timer = lv_timer_create(flash_end_cb, FLASH_MS, NULL);
        lv_timer_set_repeat_count(s_flash_timer, 1);
    }
}

void kit_soundbox_replay(void)
{
    if (s_last_pad >= 0) play_pad(s_last_pad);
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------
static void back_cb(lv_event_t *e)
{
    (void)e;
    const kit_api_table_t *t = api();
    if (t && t->system) t->system->exit();
}

static void pad_cb(lv_event_t *e)
{
    play_pad((int)(intptr_t)lv_event_get_user_data(e));
}

static void bank_pick_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= s_nbanks) return;
    s_cur = idx;
    s_last_pad = -1;
    kit_config_set_u8(K_BANK, (uint8_t)idx);
    rebuild_grid();
    lv_tileview_set_tile_by_index(s_tv, 1, 0, LV_ANIM_ON);
}

static void tv_changed_cb(lv_event_t *e)
{
    (void)e;
    int act = 0;
    lv_obj_t *t = lv_tileview_get_tile_active(s_tv);
    for (int i = 0; i < PAGES; i++) if (s_tiles[i] == t) act = i;
    for (int i = 0; i < PAGES; i++)
        lv_obj_set_style_bg_color(s_dots[i],
            lv_color_hex(i == act ? s_accent : KIT_COLOR_LINE), 0);
}

// ---------------------------------------------------------------------------
// Grade de pads (página 1)
// ---------------------------------------------------------------------------
static void rebuild_grid(void)
{
    if (!s_grid) return;
    lv_obj_clean(s_grid);
    for (int i = 0; i < SB_MAX_PADS; i++) s_pad_btn[i] = NULL;

    if (s_cur < 0) {
        lv_label_set_text(s_bank_lbl, "NENHUM BANCO");
        lv_obj_set_style_text_color(s_bank_lbl, lv_color_hex(KIT_COLOR_TEXT_MUTED), 0);
        lv_obj_t *msg = add_label(s_grid,
            "Crie um banco no conversor e copie a pasta\n"
            "pra raiz do cartao:\n\n"
            "jcrvlh.github.io/kit/soundbox.html",
            KIT_COLOR_TEXT_MUTED, &kit_sans_22, 0);
        lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(msg, X_CONTENT);
        lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(msg);
        return;
    }

    const sb_bank_t *b = &s_banks[s_cur];
    lv_label_set_text(s_bank_lbl, b->name);
    lv_obj_set_style_text_color(s_bank_lbl, lv_color_hex(b->color), 0);

    const int gap = 8;
    const int cols = 3;
    const int cell_w = (X_CONTENT - gap * (cols - 1)) / cols;   // 106
    const int rows = 3;
    int gh = lv_obj_get_height(s_grid);
    if (gh < 120) gh = X_PAGE_H - 72;      // layout ainda não resolvido
    const int cell_h = (gh - gap * (rows - 1)) / rows;

    for (int i = 0; i < b->npads && i < SB_MAX_PADS; i++) {
        lv_obj_t *pad = lv_obj_create(s_grid);
        lv_obj_remove_style_all(pad);
        lv_obj_remove_flag(pad, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(pad, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(pad, cell_w, cell_h);
        lv_obj_set_pos(pad, (i % cols) * (cell_w + gap), (i / cols) * (cell_h + gap));
        lv_obj_set_style_bg_color(pad, lv_color_hex(b->pads[i].color), 0);
        lv_obj_set_style_bg_opa(pad, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(pad, 16, 0);
        lv_obj_set_style_pad_all(pad, 6, 0);
        lv_obj_set_ext_click_area(pad, 4);
        // SHORT_CLICKED: dispara na soltura só se NÃO houve arraste — deslizar
        // pra trocar de página (ou rolar) não toca o som sem querer.
        lv_obj_add_event_cb(pad, pad_cb, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)i);

        lv_obj_t *l = add_label(pad, b->pads[i].label, on_color(b->pads[i].color),
                                &kit_mono_20, 1);
        lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(l, cell_w - 12);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(l);

        s_pad_btn[i] = pad;
    }
}

static void build_page_pads(lv_obj_t *tile)
{
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_t *p = plain_box(tile);
    lv_obj_set_size(p, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_left(p, X_PAD, 0);
    lv_obj_set_style_pad_right(p, X_PAD, 0);
    lv_obj_set_style_pad_top(p, 12, 0);
    lv_obj_set_style_pad_bottom(p, 16, 0);
    lv_obj_set_flex_flow(p, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(p, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(p, 10, 0);

    s_bank_lbl = add_label(p, "", s_accent, &kit_mono_20, 2);
    lv_label_set_long_mode(s_bank_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_bank_lbl, X_CONTENT);
    lv_obj_set_style_text_align(s_bank_lbl, LV_TEXT_ALIGN_CENTER, 0);

    s_grid = plain_box(p);
    lv_obj_set_width(s_grid, X_CONTENT);
    lv_obj_set_flex_grow(s_grid, 1);
}

// ---------------------------------------------------------------------------
// Volume (na página AJUDA) — mexe no volume global do KIT, aplicado ao vivo
// e persistido em NVS (mesmo par de funções da tela Ajustes > Som).
// ---------------------------------------------------------------------------
static void apply_volume(int v)
{
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    const kit_api_table_t *t = api();
    if (t && t->audio && t->audio->set_volume) t->audio->set_volume((uint8_t)v);
    kit_config_set_volume((uint8_t)v);
    if (s_vol_lbl) lv_label_set_text_fmt(s_vol_lbl, "%d%%", v);
}

static void vol_step_cb(lv_event_t *e)
{
    int delta = (int)(intptr_t)lv_event_get_user_data(e);
    apply_volume((int)kit_config_get_volume() + delta);
    const kit_api_table_t *t = api();
    if (t && t->audio && t->audio->beep) t->audio->beep(1320, 45);  // prévia
}

static void make_vol_btn(lv_obj_t *parent, const char *sym, int delta)
{
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, 66, 66);
    lv_obj_set_style_bg_color(b, lv_color_hex(KIT_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(b, 16, 0);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(b, 12);
    lv_obj_add_event_cb(b, vol_step_cb, LV_EVENT_CLICKED, (void *)(intptr_t)delta);
    lv_obj_center(add_label(b, sym, KIT_COLOR_TEXT, &kit_display_44, 0));
}

// ---------------------------------------------------------------------------
// "Adicionar sons" — título + passo a passo leigo + QR. Conteúdo da página 2.
// ---------------------------------------------------------------------------
static const char ADD_STEPS[] =
    "1. Aponte a camera no codigo abaixo pra abrir o conversor.\n\n"
    "2. Solte seus audios la - MP3, WAV, o que tiver. De um nome e "
    "uma cor pra cada pad.\n\n"
    "3. O conversor deixa cada audio no formato que o KIT toca e "
    "todos no mesmo volume, e avisa se algum nao serve.\n\n"
    "4. Baixe a pasta pronta - ou grave direto no cartao com o KIT "
    "em Ajustes > Modo pen drive.\n\n"
    "5. A pasta vai pra /soundbox na raiz do cartao. Cada pasta "
    "dentro de /soundbox e um banco aqui na Soundbox.";

static void build_add_sounds(lv_obj_t *parent)
{
    lv_obj_t *ttl = add_label(parent, "ADICIONAR SONS", KIT_COLOR_TEXT, &kit_mono_26, 3);
    lv_obj_set_width(ttl, X_CONTENT);

    lv_obj_t *steps = add_label(parent, ADD_STEPS, KIT_COLOR_TEXT, &kit_sans_22, 0);
    lv_label_set_long_mode(steps, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(steps, X_CONTENT);

    // QR centralizado — num wrapper de largura cheia, já que a página alinha
    // o resto à esquerda.
    lv_obj_t *qwrap = plain_box(parent);
    lv_obj_set_size(qwrap, X_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(qwrap, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(qwrap, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(qwrap, 6, 0);

    lv_obj_t *qr = lv_qrcode_create(qwrap);
    lv_qrcode_set_size(qr, 150);
    lv_qrcode_set_dark_color(qr, lv_color_hex(KIT_COLOR_BG));
    lv_qrcode_set_light_color(qr, lv_color_white());
    lv_qrcode_set_quiet_zone(qr, true);
    lv_qrcode_update(qr, SB_CONVERTER_URL, sizeof(SB_CONVERTER_URL) - 1);
    lv_obj_set_style_border_width(qr, 8, 0);
    lv_obj_set_style_border_color(qr, lv_color_white(), 0);
    lv_obj_set_style_radius(qr, 3, 0);
}

// ---------------------------------------------------------------------------
// Bloco de volume — mexe no volume global do KIT, aplicado ao vivo e persistido
// (mesmo par de funções da tela Ajustes > Som). Fica no topo da página BANCOS.
// ---------------------------------------------------------------------------
static void make_volume_block(lv_obj_t *p)
{
    lv_obj_t *vttl = add_label(p, "VOLUME", s_accent, &kit_mono_16, 2);
    lv_obj_set_width(vttl, X_CONTENT);

    lv_obj_t *vr = plain_box(p);
    lv_obj_set_size(vr, X_CONTENT, 84);
    lv_obj_set_style_pad_bottom(vr, 10, 0);
    lv_obj_set_flex_flow(vr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(vr, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(vr, 22, 0);
    make_vol_btn(vr, "-", -10);
    s_vol_lbl = add_label(vr, "", KIT_COLOR_TEXT, &kit_display_44, 0);
    lv_obj_set_width(s_vol_lbl, 150);
    lv_label_set_long_mode(s_vol_lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(s_vol_lbl, LV_TEXT_ALIGN_CENTER, 0);
    make_vol_btn(vr, "+", 10);
    lv_label_set_text_fmt(s_vol_lbl, "%d%%", (int)kit_config_get_volume());
}

// ---------------------------------------------------------------------------
// Página 0 (esquerda) — volume + seletor de bancos
// ---------------------------------------------------------------------------
static void build_page_banks(lv_obj_t *tile)
{
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_t *p = scroll_box(tile);
    lv_obj_set_size(p, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_left(p, X_PAD, 0);
    lv_obj_set_style_pad_right(p, X_PAD, 0);
    lv_obj_set_style_pad_top(p, 18, 0);
    lv_obj_set_style_pad_bottom(p, 44, 0);
    lv_obj_set_flex_flow(p, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(p, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(p, 14, 0);

    make_volume_block(p);

    lv_obj_t *hdr = add_label(p, "BANCOS", s_accent, &kit_mono_16, 2);
    lv_obj_set_width(hdr, X_CONTENT);
    lv_obj_set_style_pad_top(hdr, 12, 0);

    if (s_nbanks == 0) {
        lv_obj_t *m = add_label(p,
            "Nenhum banco no cartao ainda.\n\n"
            "Deslize pro lado (ADICIONAR SONS) pra ver como pôr sons no KIT.",
            KIT_COLOR_TEXT, &kit_sans_22, 0);
        lv_label_set_long_mode(m, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(m, X_CONTENT);
        return;
    }

    for (int i = 0; i < s_nbanks; i++) {
        lv_obj_t *chip = lv_obj_create(p);
        lv_obj_remove_style_all(chip);
        lv_obj_remove_flag(chip, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(chip, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(chip, X_CONTENT, 80);
        lv_obj_set_style_bg_color(chip, lv_color_hex(KIT_COLOR_SURFACE), 0);
        lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(chip, 16, 0);
        lv_obj_set_style_pad_left(chip, 16, 0);
        lv_obj_set_style_pad_right(chip, 16, 0);
        lv_obj_set_style_border_side(chip, LV_BORDER_SIDE_LEFT, 0);
        lv_obj_set_style_border_width(chip, 5, 0);
        lv_obj_set_style_border_color(chip, lv_color_hex(s_banks[i].color), 0);
        lv_obj_set_ext_click_area(chip, 6);
        lv_obj_add_event_cb(chip, bank_pick_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        lv_obj_set_flex_flow(chip, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(chip, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

        lv_obj_t *nm = add_label(chip, s_banks[i].name, KIT_COLOR_TEXT, &kit_sans_22, 0);
        lv_label_set_long_mode(nm, LV_LABEL_LONG_DOT);
        lv_obj_set_width(nm, X_CONTENT - 40);

        char sub[32];
        snprintf(sub, sizeof(sub), "%d SOM%s", s_banks[i].npads,
                 s_banks[i].npads == 1 ? "" : "S");
        add_label(chip, sub, KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);
    }
}

// ---------------------------------------------------------------------------
// Página 2 (direita) — como adicionar sons + créditos dos exemplos
// ---------------------------------------------------------------------------

// Sons do banco de exemplo — todos do Pixabay (pixabay.com). Fontes do KIT
// cobrem Latin-1: sem em-dash nem outros glifos fora do range.
static const char CREDITS[] =
    "Todos do Pixabay (pixabay.com):\n\n"
    "Coins, Magic - Game Studio\n"
    "Goblin, WOW - freesound_community\n"
    "Crickets - Alex\n"
    "Boxing Bell, Horn - Universfield\n"
    "Fah! - JohnnyBacon156";

static void build_page_add(lv_obj_t *tile)
{
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_t *p = scroll_box(tile);
    lv_obj_set_size(p, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(p, X_PAD, 0);
    lv_obj_set_style_pad_top(p, 18, 0);
    lv_obj_set_style_pad_bottom(p, 44, 0);
    lv_obj_set_style_pad_row(p, 18, 0);
    lv_obj_set_flex_flow(p, LV_FLEX_FLOW_COLUMN);
    // Tudo à esquerda; só o QR se centraliza sozinho (largura cheia).
    lv_obj_set_flex_align(p, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // --- Como adicionar sons (título + passos + QR) ---
    build_add_sounds(p);

    // --- Créditos dos sons de exemplo ---
    lv_obj_t *sep = add_label(p, "SONS DE EXEMPLO", s_accent, &kit_mono_16, 2);
    lv_obj_set_width(sep, X_CONTENT);
    lv_obj_set_style_pad_top(sep, 28, 0);

    lv_obj_t *cr = add_label(p, CREDITS, KIT_COLOR_TEXT_MUTED, &kit_sans_22, 0);
    lv_label_set_long_mode(cr, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(cr, X_CONTENT);
}

// ---------------------------------------------------------------------------
// Titlebar + tileview
// ---------------------------------------------------------------------------
static void build_titlebar(void)
{
    lv_obj_t *chip = lv_obj_create(s_screen);
    lv_obj_remove_style_all(chip);
    lv_obj_set_size(chip, X_CHIP, X_CHIP);
    lv_obj_set_style_bg_color(chip, lv_color_hex(KIT_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(chip, 18, 0);
    lv_obj_remove_flag(chip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(chip, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(chip, 12);
    lv_obj_add_event_cb(chip, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(chip, LV_ALIGN_TOP_LEFT, X_PAD, 16);
    lv_obj_center(add_label(chip, KIT_ICON_BACK, KIT_COLOR_TEXT, &kit_display_44, 0));

    lv_obj_t *title = add_label(s_screen, "SOUNDBOX", KIT_COLOR_TEXT, &kit_mono_26, 3);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, X_PAD + X_CHIP + 12, 30);

    lv_obj_t *dots = plain_box(s_screen);
    lv_obj_set_size(dots, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(dots, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(dots, 6, 0);
    lv_obj_align(dots, LV_ALIGN_TOP_RIGHT, -X_PAD, 40);
    for (int i = 0; i < PAGES; i++) {
        lv_obj_t *d = lv_obj_create(dots);
        lv_obj_remove_style_all(d);
        lv_obj_set_size(d, 8, 8);
        lv_obj_set_style_radius(d, 4, 0);
        lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
        s_dots[i] = d;
    }
}

static void build_tileview(void)
{
    s_tv = lv_tileview_create(s_screen);
    lv_obj_set_size(s_tv, KIT_DISPLAY_WIDTH, X_PAGE_H);
    lv_obj_set_pos(s_tv, 0, X_TITLEBAR);
    lv_obj_set_style_bg_opa(s_tv, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_tv, 0, 0);
    lv_obj_set_scrollbar_mode(s_tv, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(s_tv, tv_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Ordem: VOLUME+BANCOS (esq.) ◄─► PADS (centro, abre aqui) ◄─► ADICIONAR SONS (dir.)
    s_tiles[0] = lv_tileview_add_tile(s_tv, 0, 0, LV_DIR_HOR);
    s_tiles[1] = lv_tileview_add_tile(s_tv, 1, 0, LV_DIR_HOR);
    s_tiles[2] = lv_tileview_add_tile(s_tv, 2, 0, LV_DIR_HOR);
    build_page_banks(s_tiles[0]);
    build_page_pads(s_tiles[1]);
    build_page_add(s_tiles[2]);
}

// ---------------------------------------------------------------------------
// Ciclo de vida
// ---------------------------------------------------------------------------
kit_err_t kit_soundbox_start(uint32_t accent)
{
    if (accent) s_accent = accent;

    scan_banks();

    uint8_t saved = 0;
    kit_config_get_u8(K_BANK, &saved, 0);
    s_cur = (s_nbanks > 0) ? (saved < s_nbanks ? saved : 0) : -1;
    s_last_pad = -1;

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(KIT_COLOR_BG), 0);
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    build_titlebar();
    build_tileview();

    lv_tileview_set_tile_by_index(s_tv, 1, 0, LV_ANIM_OFF);   // abre nos PADS
    tv_changed_cb(NULL);

    // A grade precisa da altura já calculada do tile — rebuild depois do layout.
    lv_obj_update_layout(s_screen);
    rebuild_grid();

    lv_screen_load(s_screen);
    return KIT_OK;
}

void kit_soundbox_destroy(void)
{
    if (s_flash_timer) { lv_timer_delete(s_flash_timer); s_flash_timer = NULL; }
    s_flash_pad = -1;
    const kit_api_table_t *t = api();
    if (t && t->audio && t->audio->stop_sample) t->audio->stop_sample();

    if (s_screen) { lv_obj_delete(s_screen); s_screen = NULL; }
    s_tv = NULL;
    for (int i = 0; i < PAGES; i++) { s_tiles[i] = NULL; s_dots[i] = NULL; }
    for (int i = 0; i < SB_MAX_PADS; i++) s_pad_btn[i] = NULL;
    s_bank_lbl = s_grid = s_vol_lbl = NULL;
    // s_banks / s_nbanks / s_cur ficam — re-scan no próximo start
}
