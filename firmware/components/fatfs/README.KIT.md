# fatfs (override do KIT)

Cópia do componente `fatfs` da ESP-IDF (v5.3) com **uma** mudança:

* `src/ffconf.h`: `FF_FS_EXFAT` de `0` → `1` (e um bloco curto de fallback
  para `CONFIG_FATFS_USE_LABEL`, que o `ff.c` do exFAT usa como valor em C).

## Por quê

Cartões microSD de 64 GB ou mais vêm formatados em **exFAT** de fábrica, e o
macOS não monta de forma confiável o FAT32 que o `f_mkfs` da FATFS gera (o
`fsck_msdos` aceita, mas o mount recusa). Com exFAT ligado:

* o KIT lê cartões exFAT direto (`esp_vfs_fat_sdmmc_mount` autodetecta);
* **Ajustes → Armazenamento → Formatar cartão** passa a gerar exFAT em
  cartões grandes (`FM_ANY` inclui `FM_EXFAT`), que todo sistema monta.

A ESP-IDF não expõe `FF_FS_EXFAT` via Kconfig, então a única forma limpa é
sobrepor o componente inteiro (um componente com o mesmo nome em
`components/` substitui o da IDF).

## Manutenção

Ao subir de versão da ESP-IDF: recopie `$IDF_PATH/components/fatfs/` por cima
e reaplique a mudança em `src/ffconf.h` (procure `FF_FS_EXFAT` e o comentário
"KIT:"). `FF_LBA64` fica em `0` de propósito — só é preciso acima de 2 TB.
