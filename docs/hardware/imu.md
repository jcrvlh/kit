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
- **Gesto de Inclinar (*Heads Up!* / Tool "Testa"):** ✅ implementado (Runtime
  ≥ 0.2.0). `kit_imu_poll_tilt()` observa o eixo normal à tela (Z do QMI8658):
  com o aparelho vertical na testa fica ~0 g; virar a tela para o chão dispara
  `KIT_TILT_DOWN` (acertou), para o teto `KIT_TILT_UP` (passou). Dispara **uma
  vez por inclinada** — rearma em `|n| < 0,40 g`, gatilho em `|n| > 0,70 g` por
  `TILT_CONFIRM` amostras seguidas, com `|a| ≈ 1 g` (rejeita solavanco),
  debounce 0,7 s. Limiares e o sinal do eixo (`TILT_DOWN_IS_POSITIVE`) são
  `#define` em `kit_imu.c` — calibrar pelo log `Inclinar: … (n = … g)`. O
  Runtime faz o polling (no mesmo bloco do chacoalhar, ~60 ms) apenas quando
  uma Tool registrou o callback via `kit_api.imu->register_tilt_callback`;
  `kit_imu_poll_tilt()` devolve `KIT_TILT_NONE` de graça enquanto não há
  consumidor.
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

> **Repouso:** com a tela apagada o Runtime chama `kit_imu_set_enabled(false)`,
> que grava `CTRL7 = 0x00` e para o acelerômetro. Ao acordar, `CTRL7` volta a
> `0x01` e o sensor retoma na configuração já gravada em `CTRL2`. Ver
> `docs/hardware/power.md`.
