# KIT — Plataforma Modular de Tools para ESP32-S3

[![Licença GPL-3.0](https://img.shields.io/badge/licen%C3%A7a-GPL--3.0-blue.svg)](LICENSE)
[![Hardware](https://img.shields.io/badge/hardware-Waveshare%20ESP32--S3%20Touch--AMOLED--1.8%20(V2)-orange.svg)](docs/hardware/board.md)
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.3%2B-green.svg)](https://docs.espressif.com/)

**KIT** é uma plataforma modular de hardware e software para noites de jogos (*game nights*), jogos de mesa, sorteios e tomada de decisões em grupo. Funciona como um **canivete suíço digital**, permitindo a instalação dinâmica de ferramentas independentes chamadas **Tools**.

---

## 🎯 Visão do Projeto

Diferente de um firmware monolítico com jogos pré-gravados, o KIT é um **sistema operacional/runtime embarcado** que fornece:
- Gerenciamento de ciclo de vida de Tools (instalação, atualização, remoção e isolamento).
- Camada de abstração de hardware (Display AMOLED, Touch capacitivo, Bateria, PMIC, RTC, Áudio, IMU).
- Launcher gráfico fluido construído sobre LVGL v9.
- Carregamento dinâmico de binários executáveis em runtime via PSRAM.
- Mecanismo seguro de recuperação e atualizações OTA com rollback automático.

```text
+-------------------------------------------------------+
|                      BOOTLOADER                       |
+-------------------------------------------------------+
                           |
+-------------------------------------------------------+
|                      KIT RUNTIME                      |
|  [Display API] [Input API] [Storage API] [Random API] |
|  [Audio API]   [Time API]  [Power API]   [Tool Mgr]   |
+-------------------------------------------------------+
                           |
+-------------------------------------------------------+
|                      KIT LAUNCHER                     |
|            (Interface Gráfica Dinâmica)               |
+-------------------------------------------------------+
                           |
+-------------------------------------------------------+
|                   INSTALLED TOOLS                     |
|      [Dice]    [Spinner]    [Bingo]    [Custom]       |
+-------------------------------------------------------+
```

---

## 🛠️ Hardware Alvo

O hardware de referência é a placa **Waveshare ESP32-S3 Touch-AMOLED-1.8 (Revisão V2)**:

* **Microcontrolador:** ESP32-S3 (Xtensa Dual-Core LX7 @ 240MHz)
* **Memória:** 16 MB Flash (QSPI) + 8 MB PSRAM (Octal)
* **Display:** 1.8" AMOLED colorido, 368 × 448 pixels, driver **CO5300** (QSPI)
* **Touchscreen:** Capacitivo com suporte a gestos, controlador **CST820** (I2C)
* **Gerenciamento de Energia:** PMIC **AXP2101** (com suporte e conector para bateria LiPo/Li-ion)
* **Sensores & Periféricos:**
  * IMU 6 eixos (Acelerômetro + Giroscópio): **QMI8658** (I2C)
  * RTC de alta precisão: **PCF85063A** (I2C)
  * Codec de Áudio & Amplificador: **ES8311** (I2S) + Speaker Onboard
  * Conectividade: Wi-Fi 802.11 b/g/n + Bluetooth 5.0 (BLE)

---

## 🧰 KIT Tool SDK & `kit-cli`

O projeto inclui um SDK completo para desenvolvedores criarem Tools:

```bash
# 1. Instalar o CLI de desenvolvimento
cd tools-sdk/cli
pip install -e .

# 2. Criar um novo projeto de Tool
kit-cli new "Meu Jogo" --id "com.exemplo.jogo"

# 3. Validar e empacotar
kit-cli validate ./meu_jogo
kit-cli pack ./meu_jogo -o jogo.kit
```

Consulte o [Guia do Desenvolvedor de Tools](tools-sdk/docs/sdk_guide.md).

---

## 📂 Estrutura da Documentação

A documentação detalhada do KIT está organizada em [`/docs`](docs/README.md):

* **[Arquitetura](docs/architecture/overview.md):** Visão geral, Runtime, isolamento de Tools, armazenamento e segurança.
* **[Hardware](docs/hardware/board.md):** Pinouts, especificações de componentes, esquemático e energia.
* **[APIs](docs/api/overview.md):** Referência de APIs expostas pelo Runtime para desenvolvimento de Tools.
* **[Tools](docs/tools/specification.md):** Especificação de manifesto, empacotamento `.kit`, ciclo de vida, permissões e [catálogo comunitário](docs/tools/registry.md).
* **[Desenvolvimento](docs/development/setup.md):** Instruções de compilação, flash e depuração com ESP-IDF.
* **[Guia do Usuário](docs/user/installation.md):** Primeiro uso, configuração de Wi-Fi e modo de recuperação.

---

## 📜 Licença

Este projeto é software livre sob os termos da licença [GNU General Public License v3.0 (GPL-3.0)](LICENSE).
