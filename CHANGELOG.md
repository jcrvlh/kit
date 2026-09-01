# Changelog

Todas as alterações notáveis deste projeto serão documentadas neste arquivo.

O formato é baseado em [Keep a Changelog](https://keepachangelog.com/pt-BR/1.0.0/),
e este projeto adere ao [Semantic Versioning](https://semver.org/lang/pt-BR/).

---

## [Não lançado]

### Adicionado
- **KIT Core (v0.1.0):** runtime embarcado, HAL (display AMOLED CO5300, touch
  CST820, PMIC AXP2101, RTC PCF85063A, áudio ES8311, IMU QMI8658), Tool Manager
  com ciclo de vida e tabela de APIs, launcher LVGL v9, recuperação de fábrica e
  particionamento OTA dual-slot.
- **Tools oficiais (built-in no Core):** Dados, Quem Vai Primeiro, Quebra-Gelo,
  Garrafa, Decisor (Moeda), Sortear Times, Bingo e Timer.
- **Tools SDK + `kit-cli`:** headers e stubs para compilação local, simulador de
  desktop (SDL/LVGL) e CLI para criar, validar, empacotar e enviar arquivos `.kit`.
- **Web Installer:** portal WebSerial para instalar Tools sem terminal.
- **Distribuição de Tools:** especificação do catálogo comunitário e das trilhas
  de confiança com assinatura Ed25519 (ver `docs/tools/registry.md`).

_Projeto ainda pré-lançamento; a primeira release marcará a v0.1.0._
