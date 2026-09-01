#pragma once

#include "kit_api.h"

#ifdef __cplusplus
extern "C" {
#endif

kit_err_t kit_storage_init(void);
uint32_t  kit_storage_get_free_bytes(void);

// Implementações para kit_api
kit_err_t kit_storage_set_str_impl(const char *key, const char *value);
kit_err_t kit_storage_get_str_impl(const char *key, char *buffer, size_t max_len);
kit_err_t kit_storage_set_i32_impl(const char *key, int32_t value);
kit_err_t kit_storage_get_i32_impl(const char *key, int32_t *out_value);
FILE     *kit_storage_open_file_impl(const char *filename, const char *mode);

#ifdef __cplusplus
}
#endif
