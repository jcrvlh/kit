# Changelog

Todas as alterações notáveis deste projeto serão documentadas neste arquivo.

O formato é baseado em [Keep a Changelog](https://keepachangelog.com/pt-BR/1.0.0/),
e este projeto adere ao [Semantic Versioning](https://semver.org/lang/pt-BR/).

---

## [Não lançado]

### Adicionado
- **KIT Core (v0.1.0):** runtime embarcado, HAL (display AMOLED CO5300, touch
  CST820, PMIC AXP2101, RTC PCF85063A, áudio ES8311, IMU QMI8658), Tool Manager
  com ciclo de vida e tabela de APIs, launcher LVGL v9, introdução de primeiro
  uso (repetível pelos Ajustes), recuperação de fábrica e particionamento OTA
  dual-slot.
- **Tools oficiais (built-in no Core):** Dados, Quem Vai Primeiro, Quebra-Gelo,
  Garrafa, Decisor (Moeda), Sortear Times, Bingo e Timer.
- **Tools SDK + `kit-cli`:** headers e stubs para compilação local, simulador de
  desktop (SDL/LVGL) e CLI para criar, validar, empacotar e enviar arquivos `.kit`.
- **Web Installer:** portal WebSerial para instalar Tools sem terminal.
- **Distribuição de Tools:** especificação do catálogo comunitário e das trilhas
  de confiança com assinatura Ed25519 (ver `docs/tools/registry.md`).
- **Cartão microSD:** o Runtime detecta e monta um cartão microSD (SDMMC 1-bit,
  FAT32/exFAT) em `/sdcard` na inicialização. É opcional — sem cartão o KIT
  segue normalmente. Base para o armazenamento secundário de pacotes `.kit` e
  assets multimídia pesados (`kit_storage_sd_*`).
- **Carregamento de Tools pelo cartão SD (Marco 1):** novo componente
  `kit_tool_loader` que faz `dlopen`/`dlsym` de um objeto compartilhado Xtensa
  (`/sdcard/tools/<id>/tool.so`), relocado em PSRAM pelo `espressif/elf_loader`,
  com os símbolos LVGL resolvidos contra a tabela do firmware
  (`kit_tool_symbols`). O `kit_tool_manager` cai nesse caminho para qualquer ID
  que não seja uma Tool built-in. Exemplo e toolchain em
  `tools-sdk/examples/hello_sd/` (+ `tools-sdk/include/kit_lvgl_min.h`).
  Habilitado via `CONFIG_ELF_DYNAMIC_LOAD_SHARED_OBJECT`; base do `dlopen` é
  `/sdcard`. Ainda sem catálogo dinâmico nem validação de `.kit`.
  **Validado em hardware real** (ESP32-S3): três bugs corrigidos no caminho —
  o `elf_loader` 1.3.3 não remapeia código PSRAM para o barramento de
  instrução no caminho de `.so` (Tool carregada na RAM interna em vez de
  PSRAM), `MALLOC_CAP_EXEC` fica sem RAM disponível com o memory protection
  da IDF ligado (`CONFIG_ESP_SYSTEM_MEMPROT_FEATURE` desligado), e uma colisão
  de borda entre `.text` e `.rodata` no cálculo do loader (constantes globais
  da Tool precisam de `__attribute__((aligned(4)))` — ver
  `tools-sdk/examples/hello_sd/README.md`).
- **Catálogo dinâmico de Tools do cartão SD (Marco 2):** o `kit_tool_manager`
  varre `/sdcard/tools/*/manifest.json` na inicialização (cJSON), valida
  `id`/`name`, `arch` (`xtensa-esp32s3`) e compatibilidade
  `min_runtime`/`max_runtime` contra a versão do Runtime, e monta um catálogo
  em RAM. `kit_tool_manager_start()` resolve o `entry_point` exato do
  manifest em vez de assumir `tool.so`. O Launcher soma essas Tools às
  built-in na grade da Home (ícone genérico "cartão", cor por rotação de
  paleta) — a entrada fixa de teste "Olá SD" saiu do código.
  **Validado em hardware:** o card do `com.kit.hello` apareceu sozinho na
  Home a partir do `manifest.json` do cartão e abriu normalmente.
- **Armazenamento nos Ajustes:** nova tela **Ajustes → Armazenamento** com o
  espaço livre/total do LittleFS interno e do cartão SD, nº de Tools no
  cartão, e botão **Formatar cartão** (com tela de confirmação). O format
  (`esp_vfs_fat_sdcard_format`) apaga o cartão e recria a estrutura
  `tools/` — cartão "pronto pra uso" (`kit_storage_get_info`,
  `kit_storage_sd_format`).
- **Extração de pacotes `.kit` (Marco 3):** novo componente `kit_pkg` — um
  leitor de ZIP enxuto (STORED + DEFLATE via `tinfl` da ROM, CRC-32 por
  entrada). O `kit_tool_manager` descompacta automaticamente `.kit` largados
  na raiz do cartão ou em `tools/` para `tools/<id>/`, e verifica o SHA-256
  do binário contra o campo `checksum` do manifest (mbedTLS) antes de a Tool
  entrar no catálogo. Pacotes sem checksum (sideload manual) seguem sem
  verificação. Assinatura Ed25519 (ADR-0012) fica para um passo seguinte.
  **Validado em hardware.**
- **Aparência da Tool do cartão + recarga sem reboot:** o `manifest.json`
  agora aceita `accent` (cor do card na Home, hex `#RRGGBB`) e `home_icon`
  (um dos ícones geométricos do KIT: `dice`, `coin`, `timer`, `card`…). O
  `kit-cli` valida os dois campos. **Ajustes → Armazenamento** ganhou
  **Procurar cartão** (monta um cartão inserido depois de ligar) e
  **Recarregar Tools** (relê o catálogo) — a Home se redesenha na hora, sem
  reiniciar o KIT (`kit_tool_manager_set_catalog_changed_cb`). Formatar o
  cartão também atualiza a Home imediatamente.
- **Ferramentas × mini-jogos:** as Tools agora têm um tipo — ferramenta
  (`kind: "tool"`, padrão) ou mini-jogo (`kind: "game"`). Na tela **TUDO** da
  Home as Tools ficam em seções — `FERRAMENTAS`, `MINI-JOGOS` e `SISTEMA`
  (o card de Ajustes). Entre as Tools oficiais, só o **Bingo** é mini-jogo.
  O `kit-cli` valida o campo `kind`.
- **Modo pen drive (USB Mass Storage):** novo componente `kit_usb_msc` +
  **Ajustes → Modo pen drive**. Ao ativar, o KIT solta o cartão do seu
  FATFS e o expõe ao computador como um pen drive (TinyUSB MSC sobre
  SDMMC) — dá pra copiar e organizar os `.kit` sem tirar o cartão. Como o
  ESP32-S3 tem um único PHY USB, o console USB-Serial/JTAG (e o Web
  Installer) ficam suspensos enquanto o modo está ativo; sair do modo
  reinicia o KIT, que remonta o cartão e reescaneia as Tools. O TinyUSB só
  é instalado sob demanda — o uso normal não muda
  (`CONFIG_TINYUSB_MSC_ENABLED`). A tela do modo ativo avisa em destaque
  para ejetar o cartão no computador antes de sair, e o botão **Sair**
  passa por uma confirmação ("já ejetou?") antes de reiniciar.
- **Suporte a exFAT no cartão microSD:** o componente `fatfs` da ESP-IDF foi
  sobreposto (`firmware/components/fatfs/`) só para ligar `FF_FS_EXFAT`. O KIT
  agora lê cartões exFAT direto, e **Formatar cartão** gera exFAT em cartões
  grandes. Motivo: cartões de 64 GB+ vêm em exFAT de fábrica e o macOS não
  monta bem o FAT32 gerado pelo `f_mkfs` (o `fsck` aceita, o mount recusa) —
  com exFAT o modo pen drive funciona em qualquer sistema sem reformatar.

### Alterado
- **Ajustes reorganizados:** os itens soltos viraram grupos — **Tela** (brilho +
  repouso), **Som** (liga/desliga + volume), **Bateria** (nível + estado +
  "Desligar sozinho") e **Armazenamento** (que absorveu o **Modo pen drive**).
  **Testes** foi para dentro de **Sobre o KIT**. A Home passou a se redesenhar
  via `lv_async_call` quando o catálogo muda (instalar/remover pelo Catálogo),
  sem empilhar objetos por cima de um overlay aberto.
- **Catálogo:** som próprio ao concluir um download (`KIT_SFX_CATALOG_DONE`,
  arpejo alegre), distinto do `CONFIRM` usado na remoção.
- **Energia — repouso da tela vira meia-hibernação:** ao apagar a tela (manual
  ou por inatividade), o Runtime desliga o acelerômetro do QMI8658, suspende o
  áudio (efeitos novos são descartados até acordar) e habilita o *light sleep*
  automático da CPU (esp_pm, DFS 240↔40 MHz), que só entra quando o aparelho
  está na bateria — plugado no USB o console/Web Installer continua intacto.
  Tudo religa no toque de tela ou no botão PWR.

### Corrigido
- **Tools do catálogo reiniciavam a placa ao abrir:** tocar em Tools como
  Quebra-Gelo ou Pavio dava Guru Meditation (`LoadStoreError`) no `dlopen`. O
  `elf_loader` copia o `.text` da Tool para RAM interna executável (IRAM, que só
  aceita acesso alinhado de 32 bits no ESP32-S3) com um `memcpy` do tamanho cru
  da seção; quando esse tamanho não é múltiplo de 4 (Quebra-Gelo `0xa86`, Pavio
  `0x1d6b`), a cauda vira um store sub-word na IRAM e a placa reinicia. Override
  do componente em `firmware/components/espressif__elf_loader/` (ver
  `README.KIT.md`) arredonda só o `memcpy` — o bloco já é alocado com folga e
  `.text` nunca é a última seção. Tools com `.text` já múltiplo de 4 (Tarot,
  Adedonha, Veto, Fora) nunca foram afetadas.
- **Áudio:** o codec/PA do ES8311 agora desliga sozinho após ~3 s sem som
  (e religa no próximo bipe), eliminando o chiado contínuo no alto-falante e
  a corrente de repouso do amplificador de 5 V.
- **Áudio — primeiro toque mudo:** depois do codec dormir, o primeiro efeito
  curto saía enquanto o `PA_EN` ainda subia e sumia (só do 2º toque em diante
  se ouvia). `audio_codec_wake()` passou a inserir ~80 ms de silêncio antes do
  primeiro tom.
- **Bateria:** o percentual ficava preso em 0% — o gauge interno do AXP2101
  (registrador `0xA4`) não funciona nesta placa da Waveshare. Agora é estimado
  pela tensão da bateria (curva Li-ion), com os canais de ADC do PMIC ligados
  no `kit_power_init`; a detecção de carga passou a ler os bits de sentido de
  corrente da `STATUS2`.
- **Display sob Wi-Fi:** um `esp_lcd_panel_draw_bitmap` que falhava ao ser
  enfileirado (disputa de barramento com o Wi-Fi) deixava o LVGL preso para
  sempre em `wait_for_flushing`; agora o flush é liberado na mão. Os
  framebuffers foram para a PSRAM e a sessão TLS do mbedTLS foi realocada para
  a PSRAM (`CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC`), liberando IRAM contígua para o
  catálogo (`mbedtls_ssl_setup` falhava com `-0x7F00`) e para relocar o `.so`
  das Tools.
- **Catálogo:** a barra de progresso ficava em 0% a download inteiro quando o
  `index.json` não trazia `size` — agora usa o `Content-Length` da resposta.
  Travessão, reticências e aspas curvas vindas do `index.json` viravam um
  retângulo vazado na tela; agora são convertidas para ASCII.
- **Remover Tool:** o `rm_rf` pulava entradas (o FatFs não garante um `readdir`
  consistente enquanto a pasta é modificada) e a Tool ressurgia no próximo
  scan; agora a pasta é reaberta a cada remoção e o `.kit` de origem também é
  apagado.

_Projeto ainda pré-lançamento; a primeira release marcará a v0.1.0._
