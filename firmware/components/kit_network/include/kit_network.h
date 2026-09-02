#pragma once

#include "kit_api.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Subsistema de conectividade Wi-Fi (STA) do KIT.
 *
 * Princípio: **offline-first**. O rádio nasce desligado; nada de rede é
 * necessário para jogar. `kit_network_start()` liga o STA e reconecta
 * automaticamente às redes memorizadas (NVS, namespace "kit_net").
 *
 * Todo o trabalho de associação/scan/reconexão acontece numa task própria
 * (`kit_net`). Os eventos do driver Wi-Fi atualizam o estado e disparam o
 * callback registrado por `kit_network_set_state_cb()` — a UI usa isso para
 * atualizar o ícone da barra de status e a tela de Ajustes.
 *
 * O portal de provisionamento (SoftAP + página web) é um passo seguinte e
 * ainda não está aqui; o estado KIT_NET_PROVISIONING já está reservado.
 */

#define KIT_NET_SSID_MAX   33   ///< 32 octetos de SSID + NUL
#define KIT_NET_PASS_MAX   64   ///< 63 de PSK WPA2 + NUL
#define KIT_NET_MAX_SAVED   5   ///< redes memorizadas em NVS (FIFO ao estourar)
#define KIT_NET_SCAN_MAX   24   ///< teto de resultados devolvidos por um scan

typedef enum {
    KIT_NET_OFF = 0,       ///< rádio desligado
    KIT_NET_DISCONNECTED,  ///< STA ligado, sem associação
    KIT_NET_CONNECTING,    ///< tentando associar / obter IP
    KIT_NET_CONNECTED,     ///< associado e com IP
    KIT_NET_PROVISIONING,  ///< portal SoftAP no ar (reservado, ainda não usado)
} kit_net_state_t;

typedef struct {
    char    ssid[KIT_NET_SSID_MAX];
    int8_t  rssi;      ///< dBm (mais perto de 0 é melhor)
    uint8_t channel;
    bool    open;      ///< true = rede aberta (sem senha)
    bool    saved;     ///< true = já está nas redes memorizadas
} kit_net_ap_t;

typedef void (*kit_net_state_cb_t)(kit_net_state_t state, void *user_data);

// -- Ciclo de vida ---------------------------------------------------------

/**
 * Inicializa a pilha (esp_netif, event loop, esp_wifi) sem ligar o rádio.
 * Carrega a lista de redes salvas do NVS. Chamar uma vez no boot.
 */
kit_err_t kit_network_init(void);

/**
 * Liga o STA e começa a reconexão automática às redes memorizadas.
 * Idempotente. Sem redes salvas, fica em KIT_NET_DISCONNECTED varrendo
 * de tempos em tempos.
 */
kit_err_t kit_network_start(void);

/**
 * Desliga o rádio (esp_wifi_stop). Volta para KIT_NET_OFF.
 */
kit_err_t kit_network_stop(void);

/**
 * Suspende / retoma o rádio sem esquecer que o Wi-Fi está "ligado".
 *
 * Usado pelo runtime quando a tela entra em repouso: com o painel apagado o
 * KIT não faz nada de rede (o catálogo/OTA só rodam em primeiro plano, com
 * keep-awake), então derrubamos o rádio por completo — economia bem maior que
 * o modem-sleep. Ao acordar, `kit_network_suspend(false)` religa e reconecta
 * às redes memorizadas em ~1 s.
 *
 * No-op se o Wi-Fi estiver desligado (KIT_NET_OFF) ou já no estado pedido.
 */
kit_err_t kit_network_suspend(bool suspend);

kit_net_state_t kit_network_get_state(void);
bool            kit_network_is_connected(void);

// -- Conexão ativa -------------------------------------------------------

kit_err_t kit_network_get_ip(char *buf, size_t len);    ///< "192.168.0.42"
kit_err_t kit_network_get_ssid(char *buf, size_t len);  ///< SSID associado
int8_t    kit_network_get_rssi(void);                   ///< 0 se desconectado

// -- Scan (bloqueante, ~2 s) -------------------------------------------------

/**
 * Varre as redes 2.4 GHz visíveis e preenche `out[]` ordenado por RSSI
 * (desc), sem SSID repetido. Requer o rádio ligado (`kit_network_start()`).
 * Bloqueia a task chamadora durante a varredura — a UI deve rodar isto fora
 * da thread do LVGL ou mostrar um indicador de progresso.
 */
kit_err_t kit_network_scan(kit_net_ap_t *out, size_t max, size_t *count);

// -- Redes memorizadas (NVS "kit_net") -----------------------------------

/**
 * Adiciona (ou atualiza a senha de) uma rede e tenta conectar nela agora.
 * Ao estourar KIT_NET_MAX_SAVED, descarta a mais antiga.
 */
kit_err_t kit_network_save(const char *ssid, const char *pass);

/** Remove uma rede memorizada. Se for a rede ativa, desconecta. */
kit_err_t kit_network_forget(const char *ssid);

size_t    kit_network_saved_count(void);
kit_err_t kit_network_saved_ssid(size_t idx, char *buf, size_t len);
bool      kit_network_has_saved(const char *ssid);

// -- Portal de provisionamento (SoftAP + página web) ----------------------
//
// Sobe um ponto de acesso aberto e um servidor web com captura de DNS: a
// pessoa conecta o celular na rede `ap_name`, cai numa página que lista as
// redes visíveis e manda SSID + senha de volta. O KIT grava a rede
// (`kit_network_save`) e tenta associar; a página acompanha o resultado.
//
// O rádio opera em APSTA enquanto o portal está no ar. O chamador deve
// segurar o keep-awake (kit_power) — o portal não mexe em energia. Sai por
// `kit_network_portal_stop()`, e é bom ter um timeout na camada de UI.

/** @param ap_name SSID do AP (ex.: "KIT-A83F"). NULL => "KIT-SETUP". */
kit_err_t kit_network_portal_start(const char *ap_name);
kit_err_t kit_network_portal_stop(void);
bool      kit_network_portal_is_active(void);

// -- Observador de estado -------------------------------------------------

/**
 * Registra (ou remove, com cb=NULL) o observador de mudança de estado.
 * O callback roda na task de eventos do Wi-Fi — a UI deve marshalar para a
 * thread do LVGL (ex.: `lv_async_call`) antes de mexer em objetos.
 */
void kit_network_set_state_cb(kit_net_state_cb_t cb, void *user_data);

#ifdef __cplusplus
}
#endif
