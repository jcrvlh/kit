# Visão Geral das APIs do KIT

As APIs do KIT formam o contrato de comunicação entre o **KIT Core Runtime** e as **Tools**.

---

## 🔌 Modelo de Export Table

Ao invés de vincular dinamicamente símbolos globais com nomes arbitrários, o KIT expõe uma estrutura consolidada de ponteiros de função (`kit_api_table_t`) passada à Tool durante sua inicialização.

```c
typedef struct {
    const kit_display_api_t *display;
    const kit_input_api_t   *input;
    const kit_storage_api_t *storage;
    const kit_random_api_t  *random;
    const kit_time_api_t    *time;
    const kit_audio_api_t   *audio;
    const kit_power_api_t   *power;
    const kit_system_api_t  *system;
} kit_api_table_t;

typedef struct {
    const char *tool_id;
    const char *data_path;
    const kit_api_table_t *api;
} kit_tool_ctx_t;
```

---

## 📜 Convenções de Chamada e Retorno

* Todas as funções retornam `kit_err_t` (onde `KIT_OK = 0` indica sucesso).
* Nomes de funções seguem o padrão `kit_<modulo>_<acao>()`.
* Strings são codificadas em UTF-8 com terminação nula.
