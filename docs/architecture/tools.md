# Modelo de Execução e Isolamento de Tools

As **Tools** no KIT são aplicações modulares compiladas que funcionam de forma desacoplada do binário principal do firmware.

## Ferramenta × Mini-jogo

Toda Tool tem um **tipo**, declarado no manifest em `kind`:

* **`"tool"` (padrão) — ferramenta.** Utilitário de mesa: resolve algo rápido
  (dados, moeda, timer, sortear times, quebra-gelo…).
* **`"game"` — mini-jogo.** Tem rodada, estado e progressão — dá pra "jogar".

O tipo é só de **apresentação**: a tela **TUDO** da Home é dividida em seções
— `FERRAMENTAS`, depois `MINI-JOGOS`, depois `SISTEMA` (o card de Ajustes).
Não muda nada no carregamento, nas permissões ou no ciclo de vida. Entre as
Tools oficiais, só o **Bingo** é `game`; as outras sete são `tool`.

> **Estado da implementação — Marcos 1 e 2 do SD: validados em hardware real.**
> `kit_tool_manager` varre `/sdcard/tools/*/manifest.json` na inicialização
> (`id`, `name`, `version`, `entry_point`, checagem de `arch` e de
> `min_runtime`/`max_runtime`), monta um catálogo em RAM e o Launcher soma
> essas Tools às built-in na grade da Home (ícone genérico "cartão", cor por
> rotação de paleta — o manifest ainda não carrega ícone/cor próprios).
> `kit_tool_manager_start()` resolve o `entry_point` exato do catálogo e
> `kit_tool_loader` faz `dlopen`/relocação (RAM interna — ver nota abaixo) e
> chama `tool_init(ctx)` / `tool_destroy()`. Ainda **não** existem: extração
> de pacotes `.kit` (a Tool precisa já estar descompactada em
> `/sdcard/tools/<id>/`), verificação de SHA-256/assinatura, ícone próprio por
> Tool, nem catálogo em LittleFS. Os passos 1–2 abaixo descrevem o alvo (ADR),
> não 1:1 o que já roda.
>
> **Por que RAM interna e não PSRAM:** o `elf_loader` 1.3.3 não remapeia
> código carregado em PSRAM para o barramento de instrução do ESP32-S3 no
> caminho de `.so` (`dlopen`) — só o caminho de ELF "normal" faz esse remap.
> Carregar em PSRAM hoje causa `InstructionFetchError`. Efeito prático: o
> orçamento de memória por Tool é bem menor que os "50–200 KB" da ADR-0001
> até isso ser resolvido (upstream ou workaround). Rodar Tools externas
> também exigiu desligar `CONFIG_ESP_SYSTEM_MEMPROT_FEATURE` (W^X do
> ESP-IDF) — sem RAM executável sobrando com ele ligado. Detalhes completos
> em `tools-sdk/examples/hello_sd/README.md`.

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
