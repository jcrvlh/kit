# espressif/elf_loader — override do KIT

Cópia do componente `espressif/elf_loader` **v1.3.3** do registro da ESP-IDF com
**uma** mudança em relação ao original.

## A mudança

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

## Manutenção

Ao subir a versão do `elf_loader`: recopie o componente do registro por cima
(`idf.py add-dependency` / cache em `managed_components/`) e reaplique a mudança
em `src/esp_elf.c` (procure o comentário `KIT:` perto do `memcpy` do `.text`).

Um componente com o mesmo nome em `components/` substitui o de
`managed_components/` — o `main/idf_component.yml` continua listando
`espressif/elf_loader` só para travar a versão e puxar a dependência
transitiva `espressif/cmake_utilities`.
