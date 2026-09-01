#!/usr/bin/env bash
#
# Compila a Tool "Olá SD" como objeto compartilhado Xtensa (tool.so) para o
# carregador dinâmico do KIT (Marco 1 do cartão microSD).
#
# Requisitos: toolchain Xtensa no PATH (`. $IDF_PATH/export.sh`).
# Uso:        ./build.sh   ->  gera ./tool.so
#
# As flags espelham a macro `project_so` do espressif/elf_loader: PIC, -shared,
# sem libc/CRT, visibilidade oculta (só tool_init/tool_destroy exportados),
# gc-sections e strip das seções que o loader não usa.

set -euo pipefail
cd "$(dirname "$0")"

CC=xtensa-esp32s3-elf-gcc
STRIP=xtensa-esp32s3-elf-strip

command -v "$CC" >/dev/null || { echo "erro: $CC não está no PATH. Rode '. \$IDF_PATH/export.sh'." >&2; exit 1; }

SDK_INC="../../include"
OUT=tool.so

CFLAGS=(
  -std=gnu17 -Os -mlongcalls
  -fPIC -shared
  -nostdlib -nostartfiles
  -ffunction-sections -fdata-sections
  -fvisibility=hidden
  -Wall -Wextra
  -I"$SDK_INC"
)
LDFLAGS=(
  -Wl,--gc-sections
  -Wl,--strip-all
  -Wl,--allow-shlib-undefined
)

echo ">> compilando $OUT"
"$CC" "${CFLAGS[@]}" src/main.c "${LDFLAGS[@]}" -o "$OUT"

echo ">> strip das seções não usadas pelo loader"
"$STRIP" --strip-unneeded \
  --remove-section=.comment \
  --remove-section=.got.loc \
  --remove-section=.dynamic \
  --remove-section=.xt.lit \
  --remove-section=.xt.prop \
  --remove-section=.xtensa.info \
  "$OUT" || true

echo ">> pronto: $(pwd)/$OUT"
xtensa-esp32s3-elf-nm -D "$OUT" | grep -E ' T (tool_init|tool_destroy)$' \
  || { echo "AVISO: tool_init/tool_destroy não estão no .dynsym!" >&2; exit 1; }

cat <<EOF

Instale no cartão microSD:
  mkdir -p /Volumes/SEU_SD/tools/com.kit.hello
  cp tool.so manifest.json /Volumes/SEU_SD/tools/com.kit.hello/

Depois ligue o KIT e abra "Olá SD" na Home.
EOF
