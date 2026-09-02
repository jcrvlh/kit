#include "kit_tool_loader.h"
#include "kit_tool_symbols.h"

#include "esp_dlfcn.h"
#include "esp_log.h"
#include "lvgl.h"
#include <string.h>

static const char *TAG = "KIT_TOOL_LOADER";

typedef kit_err_t (*tool_init_fn)(kit_tool_ctx_t *ctx);
typedef void      (*tool_destroy_fn)(void);

static void            *s_handle;   // handle do dlopen (objeto compartilhado)
static tool_destroy_fn   s_destroy; // ponteiro para tool_destroy (pode ser NULL)
static lv_obj_t         *s_tool_screen; // tela da Tool (marcada pelo Runtime na saída)

kit_err_t kit_tool_loader_init(void)
{
    return kit_tool_symbols_register();
}

kit_err_t kit_tool_loader_start(const char *so_rel_path, kit_tool_ctx_t *ctx)
{
    if (!so_rel_path || !ctx) {
        return KIT_ERR_INVALID_ARG;
    }
    if (s_handle) {
        ESP_LOGE(TAG, "Já existe uma Tool externa carregada.");
        return KIT_FAIL;
    }

    ESP_LOGI(TAG, "dlopen('%s')...", so_rel_path);
    void *handle = dlopen(so_rel_path, RTLD_NOW);
    if (!handle) {
        const char *e = dlerror();
        ESP_LOGE(TAG, "Falha ao carregar '%s': %s", so_rel_path, e ? e : "(sem detalhe)");
        return KIT_ERR_NOT_FOUND;
    }

    tool_init_fn    init    = (tool_init_fn)dlsym(handle, "tool_init");
    tool_destroy_fn destroy = (tool_destroy_fn)dlsym(handle, "tool_destroy");
    if (!init) {
        ESP_LOGE(TAG, "'%s' não exporta tool_init.", so_rel_path);
        dlclose(handle);
        return KIT_FAIL;
    }

    ESP_LOGI(TAG, "tool_init(id='%s')...", ctx->tool_id ? ctx->tool_id : "?");
    kit_err_t err = init(ctx);
    if (err != KIT_OK) {
        ESP_LOGE(TAG, "tool_init retornou %d — abortando carga.", err);
        if (destroy) {
            destroy();
        }
        dlclose(handle);
        return err;
    }

    s_handle  = handle;
    s_destroy = destroy;
    ESP_LOGI(TAG, "Tool externa ativa.");
    return KIT_OK;
}

void kit_tool_loader_mark_tool_screen(void *screen)
{
    s_tool_screen = (lv_obj_t *)screen;
}

void kit_tool_loader_stop(void)
{
    if (!s_handle) {
        return;
    }
    ESP_LOGI(TAG, "Encerrando Tool externa...");
    if (s_destroy) {
        s_destroy();
    }

    // Rede de segurança: a Tool deveria ter deletado a própria tela no
    // tool_destroy. Se ela sobreviveu, precisa sumir AGORA — enquanto o .so
    // ainda está mapeado e os LV_EVENT_DELETE dela resolvem. Depois do dlclose
    // essa tela vira lixo (callbacks em código desmapeado) e o LVGL trava no
    // próximo render.
    if (s_tool_screen && lv_obj_is_valid(s_tool_screen) &&
        s_tool_screen != lv_screen_active()) {
        ESP_LOGW(TAG, "Tool não liberou a própria tela — removendo antes do dlclose.");
        lv_obj_delete(s_tool_screen);
    }
    s_tool_screen = NULL;

    dlclose(s_handle);
    s_handle  = NULL;
    s_destroy = NULL;
}

bool kit_tool_loader_is_active(void)
{
    return s_handle != NULL;
}
