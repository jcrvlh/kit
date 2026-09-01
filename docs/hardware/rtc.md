# Relógio de Tempo Real (RTC) PCF85063A

Especificação do módulo de relógio de hardware no KIT.

---

## ⏰ Especificações

* **Chip:** PCF85063A (NXP)
* **Interface:** I2C (Endereço `0x51`)
* **Consumo de Corrente:** < 0.22 µA em modo de bateria de backup.
* **Funções:** Registro contendo ano, mês, dia, dia da semana, horas, minutos e segundos.

---

## 💡 Papel no Sistema

- Mantém o relógio do sistema preciso mesmo com o dispositivo totalmente desligado ou sem bateria principal (quando equipado com bateria de botão/backup).
- Base temporal para temporizadores, timers de turno e histórico de sorteios.
