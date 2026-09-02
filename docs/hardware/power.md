# Gerenciamento de Energia e Bateria (PMIC AXP2101)

Especificação do controle de alimentação do KIT.

---

## 🔋 Especificações do PMIC

* **Chip:** X-Powers AXP2101
* **Interface:** I2C (Endereço `0x34`)
* **Conector de Bateria:** MX1.25 de 2 pinos
* **Suporte de Carga:** Carga linear de baterias Li-ion / LiPo de 3.7V (corrente de carga configurável de 100 mA a 1000 mA).
* **Medição de Bateria:** ADC integrado de 14-bit (tensão da bateria, VBUS e VSYS) + detecção de bateria e status de carregamento via USB. Os canais de ADC nascem desligados após o reset — `kit_power_init` habilita `0x30` (ADC) e `0x68` (detecção).
* **Percentual de carga:** o gauge interno do AXP2101 (registrador `0xA4`) fica travado em 0 nesta placa — problema conhecido do chip, cujo estimador de SoC depende de uma curva de caracterização da célula que a Waveshare não carrega. `kit_power_get_battery_percentage()` estima o percentual a partir da tensão da bateria (curva Li-ion em `kit_power_soc_from_mv`), usando `0xA4` apenas se ele devolver um valor plausível.

---

## ⚡ Trilhas de Tensão Controladas

O AXP2101 fornece múltiplos reguladores DCDC e LDOs responsáveis por alimentar:
1. Linhas de alimentação do painel AMOLED CO5300.
2. VDD de sensores I2C (Touch, IMU, RTC).
3. Linhas de alimentação do Codec de Áudio ES8311.

Ao suspender o sistema, o Runtime desliga essas saídas de tensão para garantir autonomia máxima.

---

## 😴 Repouso da tela (meia-hibernação)

Quando a tela apaga — pelo toque curto no PWR ou pelo tempo de inatividade dos
Ajustes — o Runtime (`kit_runtime.c::screen_set_on(false)`) coloca o aparelho no
menor consumo possível sem desligar de vez:

| Subsistema | No repouso | Acorda em |
|---|---|---|
| Painel AMOLED | `disp_off` (DCS 0x28) | toque na tela / PWR |
| Touch CST820 | indev do LVGL desligado; leitura crua a ~80 ms só para detectar o toque que acorda | — |
| Acelerômetro QMI8658 | `CTRL7 = 0x00` (sensor parado) — `kit_imu_set_enabled(false)` | `screen_set_on(true)` religa em `CTRL7 = 0x01` |
| Áudio ES8311 | `kit_audio_suspend(true)`: efeitos novos são descartados; o codec/PA desliga por ociosidade (~3 s) | `kit_audio_suspend(false)` |
| CPU | `esp_pm`: DFS 240↔40 MHz sempre; **light sleep automático** entre os polls **somente na bateria** (`kit_power_set_screen_sleeping`) | qualquer atividade / tick do FreeRTOS |

O *light sleep* fica **desabilitado enquanto houver VBUS** (cabo USB), para o
console serial e o Web Installer nunca serem interrompidos por uma dormida.
A política é reavaliada a cada ~1 s (`poll_inactivity`), então plugar/desplugar
o USB com a tela apagada ajusta o modo sozinho.

Requer `CONFIG_PM_ENABLE=y` e `CONFIG_FREERTOS_USE_TICKLESS_IDLE=y`
(ver `sdkconfig.defaults`). Sem essas opções, `esp_pm_configure()` devolve
`ESP_ERR_NOT_SUPPORTED` e o sistema segue a 240 MHz fixo — o resto (IMU, áudio)
continua funcionando igual.
