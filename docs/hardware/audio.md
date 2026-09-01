# Áudio, Codec ES8311 & Speaker

Especificação do subsistema de áudio no KIT.

---

## 🔊 Arquitetura de Áudio

* **Codec de Áudio:** ES8311 (I2S + I2C para controle de volume e ganho)
* **Amplificador de Potência:** Conectado ao speaker onboard com pino de habilitação **PA_EN (GPIO 46)**.
* **Microfone:** Entrada de microfone analógico/digital integrada via codec ES8311.

---

## 🎵 Aplicação no KIT Core

1. **Feedback Sonoro Imediato:**
   - Sons de clique ao tocar em botões do Launcher.
   - Efeitos sonoros para Tools (ex: som de rolagem de dados ou campainha de timer).
2. **Gerenciamento de Energia do PA:** O pino `PA_EN` é desligado automaticamente quando nenhum som estiver sendo reproduzido para evitar ruído residual e economizar bateria.

---

## ⚙️ Implementação (`kit_audio`)

* **Codec:** `esp_codec_dev` (ES8311 em modo DAC) sobre o barramento I2C
  compartilhado (`bus_handle` do `kit_power`) e um canal I2S mestre a 16 kHz
  mono, MCLK 256×.
* **Bipe assíncrono:** `kit_audio_beep_impl(freq, ms)` **não** toca o som na
  hora — apenas enfileira o pedido numa fila FreeRTOS. Uma task dedicada
  (`kit_audio`, prio 5) renderiza a senoide e escreve no I2S. Escrever no I2S é
  bloqueante (espera o codec drenar o buffer); chamado direto do callback do
  LVGL, travava a task `main` (rolagem da Dice Tool, navegação do Launcher). A
  fila tem 6 posições e descarta bipes em excesso. Cada tom tem um envelope de
  ~2 ms nas pontas para não estalar o alto-falante.
* **Volume padrão:** 80 % (`esp_codec_dev_set_out_vol`).
* **Diagnóstico:** se `kit_audio_init()` falhar (I2S ou codec), o Runtime segue
  sem áudio (não-fatal) e todo bipe vira um `ESP_LOGW`. Conferir o log de boot
  (`Codec ES8311 e I2S prontos para reprodução`) para saber se o subsistema
  subiu.
