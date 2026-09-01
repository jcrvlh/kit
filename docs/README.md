# Índice da Documentação Técnica do KIT

Bem-vindo à documentação técnica do **KIT**. Esta seção descreve a arquitetura, hardware, APIs e guias de desenvolvimento da plataforma.

---

## 📚 Seções da Documentação

### 1. [Arquitetura do Sistema](architecture/overview.md)
* [Visão Geral e Camadas](architecture/overview.md)
* [Runtime e Ciclo de Vida](architecture/runtime.md)
* [Modelo de Execução e Isolamento de Tools](architecture/tools.md)
* [Estratégia de Armazenamento e Particionamento](architecture/storage.md)
* [Atualizações OTA e Rollback](architecture/ota.md)
* [Segurança e Integridade](architecture/security.md)
* [Gerenciamento de Rede e Conectividade](architecture/networking.md)

### 2. [Especificação de Hardware](hardware/board.md)
* [Placa de Referência (Waveshare ESP32-S3 Touch AMOLED 1.8 V2)](hardware/board.md)
* [Display AMOLED CO5300 & Renderização](hardware/display.md)
* [Touch Capacitivo CST820 & Gestos](hardware/touch.md)
* [Áudio, Codec ES8311 & Speaker](hardware/audio.md)
* [Sensor IMU QMI8658](hardware/imu.md)
* [Relógio de Tempo Real (RTC) PCF85063A](hardware/rtc.md)
* [Gerenciamento de Energia & Bateria (AXP2101)](hardware/power.md)
* [Slot microSD (Expansão Futura)](hardware/microsd.md)

### 3. [Interface e Linguagem Visual](design/design-language.md)
* [Linguagem "Brutalist Bauhaus": princípios, paleta, tipografia](design/design-language.md)
* [Formas, ícones e componentes](design/design-language.md)
* [Área de toque, acessibilidade e rolagem](design/design-language.md)

### 4. [Referência de APIs do KIT](api/overview.md)
* [Visão Geral e Modelo de Export Table](api/overview.md)
* [Display API](api/display.md)
* [Input API](api/input.md)
* [Storage API](api/storage.md)
* [Random API](api/random.md)
* [Time API](api/time.md)
* [Network API](api/network.md)
* [System API](api/system.md)

### 5. [Especificação de Tools](tools/specification.md)
* [Visão Geral de Tools](tools/specification.md)
* [Manifesto (`manifest.json`)](tools/manifest.md)
* [Formato de Pacote (`.kit`)](tools/package-format.md)
* [Catálogo Comunitário de Tools](tools/registry.md)
* [Ciclo de Vida da Tool](tools/lifecycle.md)
* [Modelo de Permissões](tools/permissions.md)
* [Decisor Tool (Moeda)](tools/decisor.md)
* [Timer Tool (Cronômetro / Regressivo)](tools/timer.md)
* [Quem Vai Primeiro](tools/primeiro.md)
* [Quebra-Gelo](tools/quebragelo.md)
* [Sortear Times](tools/times.md)
* [Globo de Bingo](tools/bingo.md)

### 6. [Guia de Desenvolvimento](development/setup.md)
* [Configuração do Ambiente ESP-IDF](development/setup.md)
* [Compilação e Flash](development/build.md)
* [Gravação do Firmware](development/flash.md)
* [Depuração e Logs](development/debugging.md)

### 7. [Guia do Usuário](user/installation.md)
* [Instalação Inicial](user/installation.md)
* [Configurações do Dispositivo](user/configuration.md)
* [Configuração de Wi-Fi](user/wifi.md)
* [Modo de Recuperação (Recovery Mode)](user/recovery.md)
