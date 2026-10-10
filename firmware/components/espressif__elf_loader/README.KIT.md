# espressif/elf_loader — override do KIT

Cópia do componente `espressif/elf_loader` **v1.3.3** do registro da ESP-IDF com
**duas** mudanças em relação ao original.

## Mudança 1: `.text` alinhado na IRAM

`src/esp_elf.c`, em `esp_elf_load_section()` (caminho
`CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR`, usado no ESP32-S3):

```c
- memcpy(elf->ptext, pbuf + elf->sec[ELF_SEC_TEXT].offset,
-        elf->sec[ELF_SEC_TEXT].size);
+ memcpy(elf->ptext, pbuf + elf->sec[ELF_SEC_TEXT].offset,
+        ELF_ALIGN(elf->sec[ELF_SEC_TEXT].size, 4));
```

## Por quê

`elf->ptext` é RAM interna executável (IRAM). No ESP32-S3, IRAM **só aceita
acesso alinhado de 32 bits** — um `s8i`/`s16i` ali dispara `LoadStoreError`
(EXCCAUSE 3).

Quando a seção `.text` de uma Tool tem tamanho que **não é múltiplo de 4**
(ex.: `io.github.jcrvlh.quebragelo` = `0xa86`, `io.github.jcrvlh.pavio` =
`0x1d6b`), o `memcpy` copia o grosso como palavras e fecha com uma cauda de
1–3 bytes num store sub-word na IRAM → Guru Meditation → a placa **reinicia no
instante em que a Tool abre** (`dlopen`).

O bloco de `.text` já é alocado com `esp_elf_malloc(ELF_ALIGN(size, 4), true)`,
e `.text` nunca é a última seção do arquivo `.so`, então copiar
`ELF_ALIGN(size, 4)` lê no máximo 3 bytes a mais do `pbuf` (conteúdo da próxima
seção, nunca usado) e mantém todos os stores na IRAM alinhados.

Não altera `elf->sec[ELF_SEC_TEXT].size`: o `esp_elf_map_sym()` usa esse valor
para decidir se um endereço cai em `.text` ou em `.rodata` (que são adjacentes
no espaço de vaddr da Tool). Arredondá-lo faria a primeira string de `.rodata`
ser classificada como `.text` e reintroduziria o crash noutro ponto.

## Mudança 2: sem rede nem threads pras Tools

`src/esp_elf_symbol.c`: saem da tabela de símbolos os `lwip_*` (`socket`,
`connect`, `send`, `recv`, `bind`, `listen`, `accept`, `setsockopt`, `sendto`,
`recvfrom`, `htons`, `htonl`), `ipaddr_addr`, `ip4addr_ntoa` e os `pthread_*`.

O original exporta isso por padrão. Com o Wi-Fi conectado, qualquer Tool do
catálogo conseguiria abrir um socket TCP/UDP e mandar dados pra fora (inclusive
o que lê do cartão com `fopen`/`opendir`) sem declarar nada no manifest. Uma
thread criada pela Tool sobreviveria ao `tool_destroy()` rodando código de um
`.so` já descarregado. Nenhuma Tool usava esses símbolos (conferido no código
de todas as branches do `kit-tools` e com `nm -D` nos `tool.so`). Uma Tool que
tente usar falha no `dlopen` com símbolo indefinido.

Rede para Tools, se vier, entra pela `kit_api_table_t` com permissão e lista de
domínios no manifest, não por socket cru.

## Manutenção

Ao subir a versão do `elf_loader`: recopie o componente do registro por cima
(`idf.py add-dependency` / cache em `managed_components/`) e reaplique as duas
mudanças: em `src/esp_elf.c` (comentário `KIT:` perto do `memcpy` do `.text`) e
em `src/esp_elf_symbol.c` (comentários `KIT:` onde ficavam os `pthread_*` e os
`lwip_*`).

Um componente com o mesmo nome em `components/` substitui o de
`managed_components/` — o `main/idf_component.yml` continua listando
`espressif/elf_loader` só para travar a versão e puxar a dependência
transitiva `espressif/cmake_utilities`.
