# KIT Tools SDK

O **KIT Tools SDK** é o ambiente de desenvolvimento oficial para criar aplicações, experiências e ferramentas ("Tools") para a plataforma **KIT**.

Ele fornece headers C, bibliotecas de simulação (stubs), integração com CMake, e uma CLI robusta (`kit-cli`) para compilação nativa (desktop) e para o hardware real (Xtensa ESP32-S3).

## O que é uma Tool?

Uma Tool no KIT é um módulo de software autônomo. O firmware do KIT não precisa ser recompilado quando você instala uma Tool.
As Tools são bibliotecas dinâmicas executáveis (`.elf`) carregadas na memória PSRAM durante a execução. O KIT oferece uma **tabela de APIs segura** para a Tool acessar hardware (Buzzer, Tela AMOLED, IMU, TRNG).

## Arquitetura Visual (Brutalist Bauhaus)

Todas as Tools devem aderir estritamente à linguagem de design do KIT.
Consulte o [kit_theme.h](include/kit_theme.h) e [kit_fonts.h](include/kit_fonts.h) e utilize cores como `KIT_COLOR_YELLOW`, `KIT_COLOR_RED`, fundo absoluto `KIT_COLOR_BG` (#000000) e fontes como a `kit_mono_20` (Space Mono).

## 🚀 Começando Rápido

### 1. Instalação do kit-cli

Você precisa de Python 3.9+ instalado:

```bash
cd cli/
pip install -e .
```

Verifique se a CLI está funcionando:
```bash
kit-cli --version
```

### 2. Criando o primeiro projeto

Use o comando `new` para gerar o boilerplate:

```bash
kit-cli new "Minha Primeira Tool" --id com.meunome.primeira
cd minha_primeira_tool/
```

### 3. Compilando e Testando no Desktop (Stubs)

Você pode compilar e rodar a lógica (sem interface visual LVGL) usando o compilador nativo do seu computador (Mac, Linux, Windows).
Isso usa os **stubs de linkagem**, excelentes para testes rápidos e desenvolvimento orientado a testes (TDD) em C.

```bash
# Compila nativamente:
kit-cli build . --target native

# Executa o simulador:
./build/tool.elf
```

### 4. Compilando para o Hardware (ESP32-S3)

Para gerar o binário final, você precisa do [ESP-IDF v5.1+](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/index.html) instalado.

```bash
source $IDF_PATH/export.sh
kit-cli build . --target xtensa
```

### 5. Empacotamento e Flash

O KIT utiliza pacotes estritamente isolados (`.kit`).

```bash
kit-cli pack . -o pacote.kit
kit-cli flash pacote.kit
```

*(O suporte total a upload via interface web/USB está chegando na Fase 4).*

## 📚 Documentação Adicional

- [Guia do Manifesto (`manifest.json`)](docs/manifest_spec.md)
- [Referência Completa de API (`kit_api_table_t`)](docs/api_reference.md)
- Exemplos práticos:
  - `examples/hello_tool/`: O clássico rolar de um D20 via TRNG.
  - `examples/counter_tool/`: Salvamento de estado persistente com Storage API.
  - `examples/shake_demo/`: Como interceptar o giroscópio/acelerômetro.
