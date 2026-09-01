#include "kit_pkg.h"
#include "esp_log.h"
#include "esp_rom_crc.h"
#include "miniz.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

static const char *TAG = "KIT_PKG";

#define KIT_PKG_MAX_SIZE   (4 * 1024 * 1024)   // pacote .kit até 4 MB
#define KIT_PKG_MAX_ENTRY  (2 * 1024 * 1024)   // arquivo extraído até 2 MB

// -- Leitura little-endian de campos do ZIP --------------------------------
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

#define SIG_EOCD  0x06054b50u
#define SIG_CDIR  0x02014b50u
#define SIG_LFH   0x04034b50u

// Cria todos os diretórios do caminho até o último '/'. `path` é modificável.
static void mkdirs(char *path)
{
    for (char *s = strchr(path + 1, '/'); s; s = strchr(s + 1, '/')) {
        *s = '\0';
        if (mkdir(path, 0775) != 0 && errno != EEXIST) {
            ESP_LOGW(TAG, "mkdir('%s') falhou: errno=%d", path, errno);
        }
        *s = '/';
    }
}

// Rejeita nomes perigosos (path traversal, caminho absoluto).
static bool name_is_safe(const char *name)
{
    if (!name[0] || name[0] == '/') return false;
    if (strstr(name, "..")) return false;
    return true;
}

static kit_err_t write_file(const char *dest_dir, const char *name,
                            const uint8_t *data, size_t len)
{
    char path[192];
    int n = snprintf(path, sizeof(path), "%s/%s", dest_dir, name);
    if (n <= 0 || n >= (int)sizeof(path)) return KIT_ERR_INVALID_ARG;

    mkdirs(path);

    FILE *f = fopen(path, "wb");
    if (!f) {
        ESP_LOGE(TAG, "Não consegui criar '%s' (errno=%d)", path, errno);
        return KIT_ERR_STORAGE;
    }
    size_t wr = (len > 0) ? fwrite(data, 1, len, f) : 0;
    fclose(f);
    if (wr != len) {
        ESP_LOGE(TAG, "Escrita incompleta em '%s' (%u de %u)", path, (unsigned)wr, (unsigned)len);
        return KIT_ERR_STORAGE;
    }
    return KIT_OK;
}

// Infla um bloco raw-deflate para um buffer do tamanho exato esperado.
static kit_err_t inflate_entry(const uint8_t *src, size_t src_len,
                               uint8_t *out, size_t out_len)
{
    tinfl_decompressor *d = malloc(sizeof(tinfl_decompressor));
    if (!d) return KIT_ERR_NO_MEM;
    tinfl_init(d);

    size_t in_bytes = src_len;
    size_t out_bytes = out_len;
    tinfl_status st = tinfl_decompress(d, src, &in_bytes, out, out, &out_bytes,
                                       TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    free(d);

    if (st != TINFL_STATUS_DONE || out_bytes != out_len) {
        ESP_LOGE(TAG, "inflate falhou: st=%d, %u/%u bytes", (int)st,
                 (unsigned)out_bytes, (unsigned)out_len);
        return KIT_FAIL;
    }
    return KIT_OK;
}

kit_err_t kit_pkg_extract(const char *kit_path, const char *dest_dir)
{
    if (!kit_path || !dest_dir) return KIT_ERR_INVALID_ARG;

    FILE *f = fopen(kit_path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Não consegui abrir '%s'", kit_path);
        return KIT_ERR_NOT_FOUND;
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize <= 22 || fsize > KIT_PKG_MAX_SIZE) {
        ESP_LOGE(TAG, "'%s' com tamanho inválido para um .kit (%ld B)", kit_path, fsize);
        fclose(f);
        return KIT_ERR_INVALID_ARG;
    }

    uint8_t *buf = malloc((size_t)fsize);
    if (!buf) { fclose(f); return KIT_ERR_NO_MEM; }
    size_t rd = fread(buf, 1, (size_t)fsize, f);
    fclose(f);
    if (rd != (size_t)fsize) { free(buf); return KIT_ERR_STORAGE; }

    // Localiza o End Of Central Directory (varre de trás pra frente).
    long eocd = -1;
    for (long i = fsize - 22; i >= 0 && i > fsize - 22 - 65536; i--) {
        if (rd32(buf + i) == SIG_EOCD) { eocd = i; break; }
    }
    if (eocd < 0) {
        ESP_LOGE(TAG, "'%s' não parece um ZIP (sem EOCD)", kit_path);
        free(buf);
        return KIT_ERR_INVALID_ARG;
    }

    uint16_t n_entries = rd16(buf + eocd + 10);
    uint32_t cd_off    = rd32(buf + eocd + 16);
    if (cd_off >= (uint32_t)fsize) { free(buf); return KIT_ERR_INVALID_ARG; }

    mkdir(dest_dir, 0775);   // garante a raiz do destino

    kit_err_t result = KIT_OK;
    uint32_t p = cd_off;
    int extracted = 0;

    for (uint16_t e = 0; e < n_entries; e++) {
        if (p + 46 > (uint32_t)fsize || rd32(buf + p) != SIG_CDIR) {
            ESP_LOGE(TAG, "Diretório central corrompido na entrada %u", e);
            result = KIT_ERR_INVALID_ARG;
            break;
        }
        uint16_t method   = rd16(buf + p + 10);
        uint32_t crc      = rd32(buf + p + 16);
        uint32_t csize    = rd32(buf + p + 20);
        uint32_t usize    = rd32(buf + p + 24);
        uint16_t fn_len   = rd16(buf + p + 28);
        uint16_t ex_len   = rd16(buf + p + 30);
        uint16_t cm_len   = rd16(buf + p + 32);
        uint32_t lho      = rd32(buf + p + 42);

        char name[128];
        if (fn_len == 0 || fn_len >= sizeof(name)) {
            ESP_LOGW(TAG, "Entrada %u com nome de tamanho inválido (%u)", e, fn_len);
            p += 46 + fn_len + ex_len + cm_len;
            continue;
        }
        memcpy(name, buf + p + 46, fn_len);
        name[fn_len] = '\0';
        p += 46 + fn_len + ex_len + cm_len;

        // Entrada de diretório: só cria a pasta.
        if (name[fn_len - 1] == '/') {
            char dpath[192];
            snprintf(dpath, sizeof(dpath), "%s/%sx", dest_dir, name);   // 'x' vira sentinela p/ mkdirs
            mkdirs(dpath);
            continue;
        }

        if (!name_is_safe(name)) {
            ESP_LOGW(TAG, "Entrada '%s' ignorada (nome inseguro)", name);
            continue;
        }
        if (usize > KIT_PKG_MAX_ENTRY) {
            ESP_LOGW(TAG, "Entrada '%s' grande demais (%u B) — ignorada", name, (unsigned)usize);
            continue;
        }

        // Cabeçalho local: o offset dos dados depende dos campos DELE.
        if (lho + 30 > (uint32_t)fsize || rd32(buf + lho) != SIG_LFH) {
            ESP_LOGE(TAG, "Cabeçalho local inválido para '%s'", name);
            result = KIT_ERR_INVALID_ARG;
            break;
        }
        uint16_t l_fn = rd16(buf + lho + 26);
        uint16_t l_ex = rd16(buf + lho + 28);
        uint32_t data_off = lho + 30 + l_fn + l_ex;
        if (data_off + csize > (uint32_t)fsize) {
            ESP_LOGE(TAG, "Dados de '%s' fora do arquivo", name);
            result = KIT_ERR_INVALID_ARG;
            break;
        }

        const uint8_t *src = buf + data_off;
        uint8_t *out = NULL;
        const uint8_t *payload = NULL;
        size_t payload_len = 0;

        if (method == 0) {                 // STORED
            payload = src;
            payload_len = csize;
        } else if (method == 8) {          // DEFLATE
            out = malloc(usize ? usize : 1);
            if (!out) { result = KIT_ERR_NO_MEM; break; }
            if (inflate_entry(src, csize, out, usize) != KIT_OK) {
                free(out);
                result = KIT_FAIL;
                break;
            }
            payload = out;
            payload_len = usize;
        } else {
            ESP_LOGW(TAG, "Entrada '%s' com método de compressão %u não suportado", name, method);
            continue;
        }

        uint32_t got = esp_rom_crc32_le(0, payload, payload_len);
        if (got != crc) {
            ESP_LOGE(TAG, "CRC de '%s' não confere (0x%08lx != 0x%08lx)",
                     name, (unsigned long)got, (unsigned long)crc);
            free(out);
            result = KIT_FAIL;
            break;
        }

        kit_err_t wr = write_file(dest_dir, name, payload, payload_len);
        free(out);
        if (wr != KIT_OK) { result = wr; break; }
        extracted++;
    }

    free(buf);

    if (result == KIT_OK) {
        ESP_LOGI(TAG, "Pacote '%s' extraído: %d arquivo(s) em %s", kit_path, extracted, dest_dir);
    }
    return result;
}
