# Arquitetura do Runtime do KIT

O **KIT Runtime** é o ambiente operacional embarcado executado sobre o FreeRTOS no ESP32-S3. Ele é responsável pela inicialização de hardware, controle de energia, renderização de interface base e orquestração de Tools.

---

## 🔄 Fluxo de Inicialização (Boot Sequence)

```text
[ Power-on / Reset ]
         │
         ▼
[ ESP-IDF 2nd Stage Bootloader ]
         │
         ├── Verifica integridade da partição OTA ativa (otadata)
         │
         ▼
[ Inicialização do KIT Core (app_main) ]
         │
         ├── 1. Inicializa NVS (configurações do sistema)
         ├── 2. Inicializa PMIC AXP2101 (tensões do display, bateria)
         ├── 3. Inicializa Barramento I2C (Touch CST820, RTC, IMU)
         ├── 4. Inicializa QSPI Display CO5300 + LVGL v9
         ├── 5. Monta Partição de Armazenamento LittleFS (/tools)
         ├── 6. Inicializa TRNG & Motor de Aleatoriedade
         ├── 7. Inicializa Driver de Áudio ES8311
         ├── 8. Valida OTA Health Check (cancela rollback pendente)
         ├── 9. Carrega Lista de Tools do Tool Manager
         │
         ▼
[ Exibição do KIT Launcher ]
         │
         ├── Se Tools instaladas > 0: Exibe Grid / Carrossel de Tools
         └── Se Tools instaladas == 0: Exibe Tela "Nenhuma Tool Instalada"
```

---

## ⏱️ Ciclo de Vida do Sistema

O Runtime gerencia três estados principais de operação:

1. **Estado Ativo (Active):** Display ligado, touch ativo, taxa de quadros a 60 FPS quando houver animações.
2. **Estado Ocioso (Idle / Dimmed):** Após timeout configurável sem toques (ex: 30s), o brilho do AMOLED é reduzido para economia de bateria e proteção contra *burn-in*.
3. **Estado de Suspensão (Light / Deep Sleep):** Display desligado, periféricos em baixo consumo. O dispositivo desperta via toque no CST820 (interrupção no GPIO 21) ou botão de energia do PMIC.

---

## 🎛️ Gesto Global de Retorno (Home Gesture)

Para garantir que o usuário nunca fique preso em uma Tool defeituosa ou sem botão de saída:
- O Runtime intercepta toques na camada mais externa do LVGL.
- Um gesto padrão do sistema (**Swipe para baixo a partir do topo** ou **Toque longo de 2 segundos com dois dedos**) força o encerramento da Tool ativa e o retorno imediato ao Launcher.
