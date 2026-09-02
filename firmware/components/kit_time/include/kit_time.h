#pragma once

#include "kit_api.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Relógio do KIT. Fonte da verdade = hora do sistema (`settimeofday`), semeada
 * no boot pelo RTC de hardware PCF85063A (I2C 0x51) quando ele está rodando, e
 * corrigida por NTP quando há Wi-Fi. Fuso fixo em UTC-3 (Brasil, sem horário
 * de verão desde 2019).
 */
kit_err_t kit_time_init(void);

/**
 * Chamada pelo `kit_network` quando o STA obtém IP: (re)dispara a
 * sincronização SNTP. Idempotente. Ao sincronizar, a hora nova é gravada de
 * volta no PCF85063A para sobreviver ao desligamento.
 */
void kit_time_notify_online(void);

/**
 * true depois que a hora foi validada ao menos uma vez nesta sessão — pelo
 * RTC de hardware no boot ou por uma sincronização NTP.
 */
bool kit_time_is_synced(void);

// Implementações para kit_api
uint64_t  kit_time_get_millis_impl(void);
kit_err_t kit_time_get_datetime_impl(kit_datetime_t *dt);
void      kit_time_delay_ms_impl(uint32_t ms);

#ifdef __cplusplus
}
#endif
