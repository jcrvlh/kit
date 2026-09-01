# Slot microSD

Interface para cartões de memória microSD — armazenamento secundário do KIT.

---

## 💾 Conexão de Hardware

* **Barramento:** SDMMC no modo de 1-bit (Clock no GPIO 2, CMD no GPIO 1, Data 0 no GPIO 3).
* **Compatibilidade:** micro SD / SDHC / SDXC em **FAT32 ou exFAT** (o
  suporte a exFAT vem do override em `firmware/components/fatfs/` — cartões
  de 64 GB+ já vêm em exFAT e é o formato que todo computador monta sem
  drama). Cartões grandes formatados pelo próprio KIT saem em exFAT.
* **Sem detecção de cartão (CD) nem write-protect (WP):** a presença do cartão é
  verificada por tentativa de montagem na inicialização.

---

## ⚙️ Software

* **Montagem:** o `kit_runtime` chama `kit_storage_sd_mount()` na inicialização
  (passo 2b, logo após o LittleFS). O cartão é montado via
  `esp_vfs_fat_sdmmc_mount()` em **`/sdcard`**.
* **Opcional:** a ausência de cartão **não** é um erro — o log registra
  "Nenhum cartão microSD detectado." e o KIT continua normalmente. Nada no
  KIT Core depende do cartão para inicializar ou operar.
* **FATFS:** nomes de arquivo longos na heap (`CONFIG_FATFS_LFN_HEAP`, até 255
  caracteres) e codificação UTF-8, para pacotes `.kit` e assets com nomes
  acentuados.
* **API interna** (`kit_storage.h`):
  * `kit_storage_sd_mount()` — detecta e monta; devolve `KIT_ERR_NOT_FOUND` se
    não houver cartão.
  * `kit_storage_sd_unmount()` — desmonta e libera o barramento.
  * `kit_storage_sd_is_mounted()` — `true` se há cartão em `/sdcard`.
  * `kit_storage_sd_info(&total, &free)` — capacidade e espaço livre em bytes.

---

## 📦 Carregamento de Tools pelo cartão (em andamento)

O KIT carrega Tools externas como objetos compartilhados Xtensa (`tool.so`)
diretamente do cartão, sem regravar o firmware (ver ADR-0001).

* **Marco 1 (feito, validado em hardware):** `kit_tool_loader` faz `dlopen` de
  um `tool.so` do cartão, reloca na RAM interna (`espressif/elf_loader` —
  PSRAM não é usada por um bug do loader nesse caminho, ver
  `docs/architecture/tools.md`), resolve os símbolos LVGL contra a tabela do
  firmware (`kit_tool_symbols.c`) e chama `tool_init(ctx)` / `tool_destroy()`.
  Exemplo: `tools-sdk/examples/hello_sd/` (com as pegadinhas documentadas).
* **Marco 2 (feito, validado em hardware):** `kit_tool_manager` varre
  `/sdcard/tools/*/manifest.json` na inicialização (cJSON), valida `id`/`name`,
  `arch` e compatibilidade `min_runtime`/`max_runtime`, e monta um catálogo em
  RAM. O Launcher soma essas Tools às built-in na Home (ícone genérico, cor
  por rotação de paleta) — não há mais entrada fixa de teste.
* **Marco 3 (feito, validado em hardware):** `kit_pkg` descompacta pacotes
  `.kit` (ZIP: STORED + DEFLATE via `tinfl` da ROM, CRC-32 por entrada)
  largados na raiz do cartão ou em `tools/`, para `tools/<id>/`. O
  `kit_tool_manager` roda isso antes da varredura e confere o SHA-256 do
  binário contra o `checksum` do manifest (mbedTLS). O KIT cria
  `/sdcard/tools/` sozinho se não existir.
* **Aparência + recarga (feito):** o manifest aceita `accent` (cor do card)
  e `home_icon` (ícone geométrico do KIT). **Ajustes → Armazenamento** monta
  um cartão inserido depois do boot (**Procurar cartão**) e relê o catálogo
  (**Recarregar Tools**) — a Home se redesenha na hora, sem reiniciar.
* **Pendente:** assinatura Ed25519 dos pacotes (ADR-0012), ícone bitmap
  próprio por Tool (`icon.bin`), cópia opcional para o LittleFS,
  detecção automática de troca de cartão (sem pino CD).

## 🗂️ Como pôr uma Tool no cartão

* **Modo pen drive:** **Ajustes → Modo pen drive → Ativar** liga o cartão no
  computador como um pen drive USB (sem tirar o cartão). Copie/organize os
  `.kit`, ejete no computador e toque em **Sair** — o KIT reinicia e lê as
  Tools novas. Enquanto o modo está ativo o cabo USB não serve de console
  (o ESP32-S3 tem um PHY USB só).
* **Pacote `.kit`:** copie o arquivo para a raiz do cartão (ou para `tools/`).
  O KIT descompacta e valida no próximo boot.
* **Sideload manual (sem `.kit`):** crie `tools/<id>/` com pelo menos
  `manifest.json` e o binário do `entry_point` (`tool.so`). Sem o campo
  `checksum` no manifest, a verificação de integridade é pulada.
* **Ajustes → Armazenamento → Formatar cartão** deixa o cartão limpo e já
  com a pasta `tools/`.

## 🎯 Escopo no KIT

* **Fase Atual (V1 Core):** o slot é montado quando há cartão, mas **não** é
  obrigatório para a inicialização nem para a operação do KIT Core.
* **Fase Futura:** partição de armazenamento secundária para dezenas de pacotes
  `.kit` e assets multimídia pesados.
