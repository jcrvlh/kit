# Estratégia de OTA (Over-The-Air) e Rollback

O KIT implementa um mecanismo de atualização de firmware robusto e à prova de falhas com base nas capacidades nativas do ESP-IDF 5.x.

---

## 🔄 Arquitetura Dual-Slot com Factory Recovery

```text
               +----------------------------------+
               |         BOOTLOADER (ROM)         |
               +----------------------------------+
                                 │
                                 ▼
               +----------------------------------+
               |        2nd STAGE BOOTLOADER      |
               +----------------------------------+
                                 │
               ┌─────────────────┴─────────────────┐
               ▼                                   ▼
      [ otadata Válido? ]                 [ otadata Inválido / Corrompido ]
               │                                   │
       ┌───────┴───────┐                           │
       ▼               ▼                           ▼
  [ Slot ota_0 ]  [ Slot ota_1 ]          [ Factory Recovery App ]
       │               │                           │
       └───────┬───────┘                           │
               ▼                                   │
      [ Health Check OK? ]                         │
         │          │                              │
        SIM        NÃO                             │
         │          │                              │
         │          ▼                              │
         │    [ Rollback para Slot Anterior ]      │
         │          │                              │
         │      Falhou?                            │
         │       SIM ──► [ Boot Factory Recovery ] ◄
         ▼
[ KIT Runtime Pronto ]
```

---

## 🛡️ Health Check e Cancelamento de Rollback

Ao instalar uma nova versão de firmware no slot inativo:
1. O dispositivo reinicia no novo slot com estado `ESP_OTA_IMG_PENDING_VERIFY`.
2. O sistema executa testes de integridade:
   - Inicialização bem-sucedida de display, touch e LittleFS.
   - Ausência de travamentos (*panics*) nos primeiros 5 segundos.
3. Se todos os testes passarem, a função `esp_ota_mark_app_valid_cancel_rollback()` é executada, fixando o novo firmware como padrão.
4. Se ocorrer falha ou timeout do Watchdog, o bootloader reverte automaticamente para a versão estável anterior no próximo boot.
