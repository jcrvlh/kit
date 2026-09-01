# Gerenciamento de Energia e Bateria (PMIC AXP2101)

Especificação do controle de alimentação do KIT.

---

## 🔋 Especificações do PMIC

* **Chip:** X-Powers AXP2101
* **Interface:** I2C (Endereço `0x34`)
* **Conector de Bateria:** MX1.25 de 2 pinos
* **Suporte de Carga:** Carga linear de baterias Li-ion / LiPo de 3.7V (corrente de carga configurável de 100 mA a 1000 mA).
* **Medição de Bateria (E-Gauge):** ADC integrado de 14-bit que reporta percentual de carga, tensão instantânea e status de carregamento via USB.

---

## ⚡ Trilhas de Tensão Controladas

O AXP2101 fornece múltiplos reguladores DCDC e LDOs responsáveis por alimentar:
1. Linhas de alimentação do painel AMOLED CO5300.
2. VDD de sensores I2C (Touch, IMU, RTC).
3. Linhas de alimentação do Codec de Áudio ES8311.

Ao suspender o sistema, o Runtime desliga essas saídas de tensão para garantir autonomia máxima.
