# Olá SD — prova do carregamento de Tools pelo cartão microSD (Marco 1)

Esta Tool existe para validar o **caminho crítico** do carregamento dinâmico:

```
/sdcard/tools/com.kit.hello/tool.so
        │
        ├─ FAT (esp_vfs_fat)                    ← kit_storage monta /sdcard no boot
        ├─ esp_elf_open + esp_elf_relocate      ← objeto compartilhado relocado em PSRAM
        ├─ dlsym("tool_init") / dlsym("tool_destroy")
        ├─ tool_init(ctx)  ── desenha um label (símbolos LVGL do firmware)
        │                  └─ ctx->api->audio->beep(...)  (ponteiro na kit_api_table_t)
        └─ ao sair: tool_destroy() + dlclose()  ← libera a PSRAM
```

## Compilar

```bash
. $IDF_PATH/export.sh        # toolchain Xtensa no PATH
./build.sh                   # gera ./tool.so
```

O `build.sh` usa as mesmas flags da macro `project_so` do `espressif/elf_loader`:
PIC, `-shared`, sem libc/CRT, `-fvisibility=hidden` (só `tool_init`/`tool_destroy`
ficam no `.dynsym`), `--gc-sections` e `strip`.

## Instalar no cartão

Cartão formatado em **FAT32** ou **exFAT**:

```
<raiz do cartão>/
└── tools/
    └── com.kit.hello/
        ├── tool.so
        └── manifest.json
```

## Rodar

Ligue o KIT. Na Home aparece o card **"Olá SD"** (entrada temporária do Marco 1).
Toque nele:

- **Sucesso:** tela preta com "OLA DO CARTAO SD" + bipe. O botão físico BOOT volta
  para a Home (dispara `tool_destroy` + `dlclose`).
- **Sem cartão / sem `tool.so`:** toast "NAO ABRIU" e um bipe grave.

Acompanhe pelo monitor serial (`idf.py monitor`): as tags `KIT_TOOL_LOADER`,
`DLMOD` e `ELF` mostram cada etapa da relocação.

## ✅ Validado em hardware (2026-09-01)

`tool_init` → desenha o label + bipe → `tool_destroy` ao sair (BOOT) → reabre
sem problema. Três bugs reais precisaram ser corrigidos para chegar aqui — ver
`firmware/sdkconfig.defaults` e o comentário no topo de `src/main.c`:

1. **PSRAM sem remap de barramento:** o caminho de `.so` do `elf_loader` 1.3.3
   não remapeia código carregado em PSRAM para o barramento de instrução do
   S3 (o caminho de ELF "normal" faz isso, o de `dlopen` não). Resultado:
   `InstructionFetchError` ao chamar `tool_init`. Corrigido carregando a Tool
   na RAM interna (`CONFIG_ELF_LOADER_LOAD_PSRAM=n`) — unificada (I e D) no S3,
   dispensa remap. Tools continuam pequenas (dezenas de KB) neste marco.
2. **`MALLOC_CAP_EXEC` sem RAM disponível:** com o memory protection (PMP W^X)
   da IDF ligado, a DIRAM comum perde a capacidade EXEC e só sobra RAM
   executável se já houver um pool de IRAM livre — não há, e o `esp_elf_malloc`
   do texto falha com ENOMEM. Corrigido com
   `# CONFIG_ESP_SYSTEM_MEMPROT_FEATURE is not set`.
3. **Colisão texto/rodata:** o loader calcula o fim de `.text` arredondado a 4
   bytes mas não confere se isso invade a seção seguinte. Como `.rodata` (uma
   `const char[]`) só pede alinhamento de 1 byte, o linker às vezes a coloca
   sem folga logo após `.text`, e o primeiro ponteiro para ela é resolvido
   para *dentro* de `.text` por engano — lendo a string ali dá
   `LoadStoreError`. Corrigido com `__attribute__((aligned(4)))` na constante
   (ver o comentário em `src/main.c`) — **toda Tool com strings/tabelas
   globais precisa disso na primeira constante da seção**.

## Limitações conhecidas do Marco 1

- O nome do módulo no loader é o *basename* sem extensão (`tool`), então só uma
  Tool externa por vez (o `kit_tool_loader` já tem um único slot).
- Sem verificação de `manifest.json`, SHA-256, assinatura ou compatibilidade de
  versão — isso entra no Marco 3 (extração/validação de `.kit`).
- A superfície LVGL exportada é mínima (ver `tools-sdk/include/kit_lvgl_min.h` e
  `firmware/components/kit_tool_loader/src/kit_tool_symbols.c`).
- Carrega direto de `/sdcard`; ainda não copia para o LittleFS.
- Tool roda na RAM interna, não na PSRAM — o orçamento de memória por Tool é
  bem menor que os "50–200 KB" da ADR-0001 até o remap de PSRAM ser resolvido
  (upstream no `elf_loader` ou um workaround nosso).
- Memory protection (PMP W^X) do ESP-IDF está desligado globalmente para
  liberar RAM executável — é uma redução real de postura de segurança do
  firmware inteiro, não só das Tools; revisitar junto da assinatura de
  pacotes (ADR-0012).
