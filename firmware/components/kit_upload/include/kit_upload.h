#pragma once

#include "kit_api.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Instalar Tool pelo Wi-Fi (Ajustes > Instalar pelo Wi-Fi).
 *
 * Com a tela aberta, o KIT sobe um servidor HTTP na rede em que já está
 * conectado. Do navegador (http://<ip do KIT>), a pessoa escolhe um ou mais
 * `.kit` e digita o PIN de 4 dígitos que aparece no display; o pacote é
 * gravado no cartão em pedaços e instalado pelo kit_tool_manager.
 *
 * Segurança: sem assinatura dos pacotes (ADR-0012), quem está na mesma rede
 * poderia empurrar código. Por isso o servidor só existe com a tela aberta,
 * o PIN é novo a cada abertura e 5 PINs errados travam o envio até reabrir.
 * Quem vê o PIN está com o KIT na mão — mesma confiança do cartão SD, então
 * Tools com "network" são aceitas (o Catálogo online as recusa).
 *
 * O servidor processa um pedido por vez (task única do esp_http_server). A UI
 * acompanha por polling dos getters abaixo.
 */

#define KIT_UPLOAD_MAX_BYTES (1024 * 1024)

typedef enum {
    KIT_UP_OFF = 0,
    KIT_UP_WAITING,      // no ar, esperando arquivo
    KIT_UP_RECEIVING,    // recebendo (ver kit_upload_progress)
    KIT_UP_INSTALLING,   // extraindo + recarregando o catálogo
    KIT_UP_DONE,         // última instalação deu certo (mensagem = nome + versão)
    KIT_UP_ERROR,        // última tentativa falhou (mensagem = motivo)
    KIT_UP_LOCKED,       // PIN errado demais: envio travado até reabrir
} kit_upload_state_t;

/** Sobe o servidor e sorteia um PIN novo. Requer o Wi-Fi ligado. */
kit_err_t kit_upload_start(void);
void      kit_upload_stop(void);
bool      kit_upload_is_active(void);

kit_upload_state_t kit_upload_get_state(void);
int         kit_upload_progress(void);    ///< 0..100 em RECEIVING
const char *kit_upload_pin(void);         ///< "0427"
const char *kit_upload_message(void);     ///< detalhe do último DONE/ERROR
/** Sobe a cada pedido recebido — a UI usa pra adiar o tempo limite. */
uint32_t    kit_upload_activity(void);

#ifdef __cplusplus
}
#endif
