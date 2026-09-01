#pragma once

#include "kit_api.h"

#ifdef __cplusplus
extern "C" {
#endif

kit_err_t kit_time_init(void);

// Implementações para kit_api
uint64_t  kit_time_get_millis_impl(void);
kit_err_t kit_time_get_datetime_impl(kit_datetime_t *dt);
void      kit_time_delay_ms_impl(uint32_t ms);

#ifdef __cplusplus
}
#endif
