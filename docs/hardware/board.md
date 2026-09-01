# Placa de Referência: Waveshare ESP32-S3 Touch AMOLED 1.8 (V2)

Especificação completa da placa de desenvolvimento adotada como padrão para o KIT.

---

## 📋 Resumo Técnico

* **MCU:** ESP32-S3R8 (Dual-core Xtensa 32-bit LX7 @ até 240 MHz)
* **Flash:** 16 MB SPI Flash (Quad SPI)
* **PSRAM:** 8 MB PSRAM (Octal SPI)
* **Display:** 1.8" AMOLED, 368 × 448 pixels, Driver CO5300 (QSPI)
* **Touch:** CST820 Capacitivo (I2C)
* **PMIC / Carregador:** AXP2101 (I2C) com conector MX1.25 para bateria LiPo
* **IMU:** QMI8658 6-eixos (Acelerômetro + Giroscópio, I2C)
* **RTC:** PCF85063A com suporte a bateria de backup (I2C)
* **Áudio:** Codec ES8311 (I2S) + Alto-falante onboard + Amplificador com controle de Enable
* **Microfone:** PDM/I2S onboard
* **Armazenamento:** Slot microSD (SDMMC)

---

## 📌 Mapeamento Geral de GPIOs (V2)

| Periférico | Função | GPIO ESP32-S3 | Barramento / Protocolo |
| :--- | :--- | :--- | :--- |
| **Display** | LCD_SDIO0 | GPIO 4 | QSPI Data 0 |
| | LCD_SDIO1 | GPIO 5 | QSPI Data 1 |
| | LCD_SDIO2 | GPIO 6 | QSPI Data 2 |
| | LCD_SDIO3 | GPIO 7 | QSPI Data 3 |
| | LCD_SCLK | GPIO 11 | QSPI Clock |
| | LCD_CS | GPIO 12 | QSPI Chip Select |
| | LCD_RESET | GPIO 39 | Reset do Painel |
| **Touch** | TOUCH_SDA | GPIO 15 | I2C Data (compartilhado) |
| | TOUCH_SCL | GPIO 14 | I2C Clock (compartilhado) |
| | TOUCH_INT | GPIO 21 | Interrupção de Toque |
| **Áudio** | I2S_MCK | GPIO 16 | I2S Master Clock |
| | I2S_BCK | GPIO 9 | I2S Bit Clock |
| | I2S_DI | GPIO 10 | I2S Data In (Mic) |
| | I2S_DO | GPIO 8 | I2S Data Out (Codec) |
| | I2S_WS | GPIO 45 | I2S Word Select |
| | PA_EN | GPIO 46 | Habilitação do Amplificador |
| **SD Card** | SD_CLK | GPIO 2 | SDMMC Clock |
| | SD_CMD | GPIO 1 | SDMMC Command |
| | SD_DATA | GPIO 3 | SDMMC Data 0 |

---

## 🗺️ Mapa de Endereços I2C

| Componente | Função | Endereço I2C Padrão |
| :--- | :--- | :--- |
| **AXP2101** | Gerenciador de Energia / Bateria | `0x34` |
| **CST820** | Controlador de Touchscreen | `0x15` |
| **PCF85063A** | Relógio de Tempo Real (RTC) | `0x51` |
| **ES8311** | Codec de Áudio | `0x18` |
| **QMI8658** | IMU 6-Eixos (Acelerômetro/Giroscópio) | `0x6A` ou `0x6B` |
