#pragma once

#include "kit_api.h"

#ifdef __cplusplus
extern "C" {
#endif

kit_err_t kit_network_init(void);
bool      kit_network_is_connected(void);

#ifdef __cplusplus
}
#endif
