# Ciclo de Vida da Tool

Toda Tool no KIT deve implementar duas funções obrigatórias de ciclo de vida:

---

## 🔄 Funções de Interface

```c
#include "kit_api.h"

/**
 * Ponto de entrada chamado pelo Runtime ao iniciar a Tool.
 * @param ctx Contexto da Tool com ponteiros para as APIs do KIT e dados privados.
 * @return KIT_OK se inicializado com sucesso.
 */
kit_err_t tool_init(kit_tool_ctx_t *ctx);

/**
 * Chamado antes do encerramento da Tool.
 * A Tool deve destruir seus objetos LVGL, fechar arquivos e liberar memória alocada.
 */
void tool_destroy(void);
```

---

## 📊 Diagrama de Estados da Tool

```text
   [ Não Instalada ]
           │
           │ (Instalação do pacote .kit)
           ▼
     [ Instalada ] ◄───────────────────┐
           │                           │
           │ (Usuário clica no ícone)  │ (Gesto Home ou kit_system_exit)
           ▼                           │
   [ Carregando em PSRAM ]             │
           │                           │
           │ (tool_init() == KIT_OK)   │
           ▼                           │
      [ Executando ] ──────────────────┘
```
