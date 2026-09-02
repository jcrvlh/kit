# Gerenciamento de Rede e Conectividade

O KIT suporta conectividade sem fio via Wi-Fi 802.11 b/g/n e Bluetooth 5.0 (BLE).

---

## 📶 Conectividade Wi-Fi

O subsistema de rede é projetado com o princípio de **independência operacional**:
- **Funcionamento Offline Nativo:** O dispositivo é 100% utilizável para jogos e sorteios sem nenhuma rede configurada.
- **Pilha sob demanda:** o componente `kit_network` só carrega mutex + a lista
  de redes salvas (NVS `kit_net`) no boot. `esp_netif`/`esp_wifi`/lwip (~45 KB de
  RAM interna) e o rádio só sobem quando o usuário liga o Wi-Fi — inicializar a
  pilha durante o bring-up dos periféricos trava o display AMOLED (contenção de
  RAM interna/DMA com o `esp_lcd` QSPI). A religação automática às redes
  memorizadas é adiada ~3 s, já com a UI renderizando.
- **Casos de Uso Conectados:**
  1. Download e atualização de Tools a partir de catálogo remoto.
  2. Atualizações de firmware via OTA (Over-The-Air).
  3. Sincronização de horário via NTP (para alimentar o RTC PCF85063A).

### Economia de bateria

O rádio Wi-Fi é o maior consumidor depois do display, então `kit_network` é
agressivo em desligá-lo:

- **Rádio off por padrão:** nasce desligado; só liga quando o usuário ativa o
  Wi-Fi (persistido em `kit_config` `wifi_en`).
- **Modem-sleep profundo:** associado, opera em `WIFI_PS_MAX_MODEM` com
  `listen_interval = 3` — o rádio dorme e só acorda a cada 3 beacons DTIM. O KIT
  não recebe tráfego não solicitado (HTTPS pontual do catálogo/OTA, NTP), então
  a latência extra de RX não custa nada.
- **Suspensão no repouso da tela:** `kit_network_suspend(true)` (chamado pelo
  runtime junto com o IMU/áudio quando o painel apaga) faz `esp_wifi_stop()` —
  rádio 100% off. Ao acordar, religa e reconecta em ~1 s. Downloads seguram
  keep-awake, então a tela nunca dorme no meio de um.
- **Potência de TX adaptativa:** a cada associação, `esp_wifi_set_max_tx_power()`
  é ajustada ao RSSI (10 dBm colado no AP … 20 dBm em sinal fraco). Revisada a
  cada 5 min enquanto conectado.
- **Scan com canal conhecido:** a reconexão automática varre só o canal do AP
  memorizado (`WIFI_FAST_SCAN`) em vez dos 13. Depois de ~8 ciclos sem enxergar
  nenhuma rede conhecida (usuário saiu de casa), o intervalo entre varreduras
  sobe de 60 s para 5 min.
- **NTP raro:** resync a cada 12 h — o RTC PCF85063A segura a hora entre uma e
  outra, sem acordar o rádio à toa.

### Provisionamento

Como o KIT não tem teclado, a senha da rede é digitada pelo celular:
`kit_network_portal_start()` sobe um **SoftAP aberto** (`KIT-XXXX`) em modo
APSTA, um servidor HTTP com uma página única (sem CDN) e um **servidor DNS de
captura** que responde toda consulta com o IP do AP (`192.168.4.1`), disparando
a tela de login automática do celular. A página lista as redes visíveis
(`/scan`), recebe SSID + senha (`/connect`) e acompanha o resultado (`/status`).

---

## 📡 Bluetooth 5.0 (BLE)

O rádio Bluetooth integrado permite casos de uso futuros como:
- Provisionamento facilitado de Wi-Fi via aplicativo web/mobile (*BLE Provisioning*).
- Modo de controle remoto sem fio para passar turnos ou rolar dados à distância.
