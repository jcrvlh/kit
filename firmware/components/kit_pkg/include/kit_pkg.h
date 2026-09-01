#pragma once

#include "kit_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// Extrai um pacote .kit (contêiner ZIP: manifest.json + tool.so + icon.bin +
// assets/) para um diretório de destino, criando as subpastas necessárias.
//
// Suporta entradas STORED (sem compressão) e DEFLATE (via tinfl da ROM). O
// CRC-32 de cada entrada é conferido contra o diretório central do ZIP.
//
//   kit_path : caminho completo do .kit (ex.: "/sdcard/tools/com.kit.x.kit")
//   dest_dir : diretório onde extrair (ex.: "/sdcard/tools/com.kit.x")
//
// KIT_OK em caso de sucesso. Não é fatal para o sistema — o chamador só
// registra e segue.
kit_err_t kit_pkg_extract(const char *kit_path, const char *dest_dir);

#ifdef __cplusplus
}
#endif
