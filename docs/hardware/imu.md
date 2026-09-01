# Sensor IMU QMI8658 (Acelerômetro e Giroscópio)

Especificação do sensor inercial integrado ao KIT.

---

## 🧭 Especificações Técnicas

* **Sensor:** QMI8658 6-DOF (Graus de Liberdade)
* **Acelerômetro:** Escalas configuráveis de ±2g, ±4g, ±8g, ±16g.
* **Giroscópio:** Escalas de ±16 a ±2048 graus/segundo (DPS).
* **Interface:** I2C (Endereço `0x6A` ou `0x6B`).

---

## 🎮 Aplicações em Tools

- **Gesto de Chacoalhar (*Shake to Roll*):** ✅ implementado. O componente
  `kit_imu` configura o acelerômetro (±8 g, 125 Hz, giroscópio desligado) e
  expõe `kit_imu_poll_shake()`. O Runtime faz polling a ~60 ms **apenas enquanto
  há uma Tool ativa** e, ao detectar `|a| > 2,2 g` (debounce 0,7 s), dispara a
  _ação principal_ da Tool (o mesmo hook do botão PWR). Limiar e debounce são
  `#define` em `kit_imu.c` — calibrar pelo log `Chacoalhar detectado (|a| = …)`.
- **Detecção de Orientação:** rotação automática da tela — futuro.
- **Wake on Motion (WoM):** despertar ao pegar o aparelho — futuro.

### Registradores usados (QMI8658)

| Reg | Valor | Efeito |
|---|---|---|
| `CTRL1` (0x02) | `0x40` | endereço auto-incrementa (burst read de 0x35..0x3A) |
| `CTRL2` (0x03) | `0x26` | acelerômetro ±8 g @ 125 Hz |
| `CTRL3` (0x04) | `0x00` | giroscópio desligado |
| `CTRL7` (0x08) | `0x01` | habilita só o acelerômetro |
| `AX_L` (0x35) | — | início dos 6 bytes de aceleração (X/Y/Z, int16 LE) |
