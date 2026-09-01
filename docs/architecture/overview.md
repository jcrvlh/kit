# Arquitetura do Sistema — Visão Geral

O **KIT** é estruturado em camadas com responsabilidades estritas e desacopladas, assegurando que o sistema permaneça funcional, seguro e extensível.

---

## 🏛️ Diagrama de Camadas

```text
+-------------------------------------------------------------------+
|                            APLICAÇÕES                             |
|       [ Dice Tool ]       [ Bingo Tool ]      [ Custom Tool ]     |
+-------------------------------------------------------------------+
                                  │
                                  ▼ (Chama APIs via kit_api.h)
+-------------------------------------------------------------------+
|                        KIT API (EXPORT TABLE)                     |
|  Display | Input | Storage | Random | Time | Audio | Power | Sys  |
+-------------------------------------------------------------------+
                                  │
                                  ▼
+-------------------------------------------------------------------+
|                         KIT CORE RUNTIME                          |
|  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐ |
|  │   Tool Manager   │  │   Launcher UI    │  │  Power & Battery │ |
|  └──────────────────┘  └──────────────────┘  └──────────────────┘ |
|  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐ |
|  │  Event Router    │  │  Storage Manager │  │  OTA & Recovery  │ |
|  └──────────────────┘  └──────────────────┘  └──────────────────┘ |
+-------------------------------------------------------------------+
                                  │
                                  ▼
+-------------------------------------------------------------------+
|                   HARDWARE ABSTRACTION LAYER (HAL)                |
|   Display (CO5300) │ Touch (CST820) │ PMIC (AXP2101) │ RTC (PCF)  |
|   Audio (ES8311)   │ IMU (QMI8658)  │ LittleFS Flash │ FreeRTOS   |
+-------------------------------------------------------------------+
                                  │
                                  ▼
+-------------------------------------------------------------------+
|                       ESP32-S3 HARDWARE (V2)                      |
|   Xtensa Dual-Core │ 16MB Flash │ 8MB PSRAM │ AMOLED │ WiFi / BLE |
+-------------------------------------------------------------------+
```

---

## ⚙️ Princípios Arquiteturais

1. **Separação Kernel / Userland:** O Runtime atua como o núcleo do sistema, controlando hardware e memória. As Tools rodam como código modular desacoplado.
2. **Execução Segura em PSRAM:** Binários de Tools compilados no formato ELF são carregados do sistema de arquivos para a memória PSRAM e executados sob demanda.
3. **Nenhum Acesso Direto ao Hardware:** Tools não incluem headers do ESP-IDF nem acessam diretamente periféricos SPI, I2C ou GPIOs. Toda interação ocorre pelas funções padronizadas da API do KIT.
4. **Resiliência a Falhas:** Falhas ou erros em uma Tool são capturados e tratados pelo Runtime, permitindo retornar ao Launcher sem reiniciar o dispositivo.
5. **Autonomia:** O sistema funciona perfeitamente sem conexão de rede ou cartões microSD externos.
