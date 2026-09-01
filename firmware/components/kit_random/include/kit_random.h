#pragma once

#include "kit_api.h"

#ifdef __cplusplus
extern "C" {
#endif

kit_err_t kit_random_init(void);

// Implementações para kit_api
uint32_t  kit_random_u32_impl(void);
int32_t   kit_random_range_impl(int32_t min, int32_t max);
kit_err_t kit_random_bytes_impl(uint8_t *buffer, size_t length);
float     kit_random_float_impl(void);

#ifdef __cplusplus
}
#endif
