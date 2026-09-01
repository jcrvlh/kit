#!/usr/bin/env bash
#
# Compila e empacota a Tool "Olá SD" via kit-cli.
#
# Requisitos:
#   - toolchain Xtensa no PATH:  . $IDF_PATH/export.sh
#   - kit-cli:                   pip install ../../cli
#   - headers do LVGL: $KIT_LVGL_DIR, ou um build do firmware já feito
#     (popula firmware/managed_components/lvgl__lvgl)
#
# Uso:  ./build.sh   ->  gera ./tool.so e ./com.kit.hello.kit

set -euo pipefail
cd "$(dirname "$0")"

kit-cli build . --target xtensa       # -> tool.so
kit-cli pack  . -o com.kit.hello.kit

xtensa-esp32s3-elf-nm -D tool.so | grep -E ' T (tool_init|tool_destroy)$' \
  || { echo "AVISO: tool_init/tool_destroy não estão no .dynsym!" >&2; exit 1; }

cat <<'EOF'

Instale no cartão microSD:
  mkdir -p /Volumes/SEU_SD/tools/com.kit.hello
  cp tool.so manifest.json /Volumes/SEU_SD/tools/com.kit.hello/

Depois ligue o KIT e abra "Olá SD" na Home.
EOF
