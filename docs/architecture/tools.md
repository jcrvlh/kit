# Modelo de Execução e Isolamento de Tools

As **Tools** no KIT são aplicações modulares compiladas que funcionam de forma desacoplada do binário principal do firmware.

---

## 📦 Como uma Tool é Carregada

1. **Descoberta:** O `kit_tool_manager` varre o diretório `/tools` no sistema de arquivos LittleFS procurando por subpastas com `manifest.json` válido.
2. **Validação:** Checa versão de API compatível (`min_runtime <= runtime_version <= max_runtime`), permissões requeridas e integridade do arquivo `tool.elf`.
3. **Alocação e Relocação:** Ao selecionar a Tool no Launcher, o Runtime utiliza o `espressif/elf_loader` para alocar espaço na memória PSRAM externa e efetuar a relocação dos símbolos da Tool contra a tabela de exportação (`kit_api`).
4. **Execução:** O ponto de entrada da Tool (`tool_init`) é invocado, recebendo uma estrutura de contexto `kit_tool_ctx_t` com ponteiros seguros para as APIs permitidas.

```text
+-------------------------------------------------------------+
|                      Memória PSRAM (8MB)                    |
|                                                             |
|  ┌───────────────────────┐       ┌───────────────────────┐  |
|  │  LVGL Framebuffers    │       │  ELF Loader Arena     │  |
|  │  (368 x 448 x 16bpp)  │       │  (Tool ativa em RAM)  │  |
|  └───────────────────────┘       └───────────────────────┘  |
+-------------------------------------------------------------+
```

---

## 🛡️ Isolamento e Resiliência

Para evitar que erros em Tools corrompam o sistema:

* **Watchdog de Tool:** Tarefas em execução são monitoradas pelo Task Watchdog Timer (TWDT). Loops infinitos são interrompidos.
* **Isolamento de Armazenamento:** Cada Tool tem acesso apenas ao seu subdiretório privado em `/tools/<tool_id>/data/`.
* **Descarregamento Limpo:** Ao sair ou trocar de Tool, a função `tool_destroy` é chamada para liberar widgets LVGL e alocações de memória. Se houver falha, o Runtime força a desalocação do espaço ocupado pela Tool na PSRAM.
