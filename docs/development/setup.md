# Configuração do Ambiente de Desenvolvimento

Guia passo a passo para configurar as ferramentas necessárias para compilar o KIT Core e desenvolver Tools.

---

## 💻 Requisitos do Sistema

* **Sistema Operacional:** Linux (Ubuntu 22.04+), macOS (Apple Silicon ou Intel) ou Windows 11 (via WSL2 recomendado).
* **Git:** Para clonar o repositório e gerenciar versões.
* **Python:** Versão 3.10 ou superior com `venv` instalado.

---

## 🛠️ Instalando o ESP-IDF v5.3+

O KIT utiliza o framework oficial **ESP-IDF v5.3** da Espressif.

```bash
# 1. Crie a pasta do toolchain
mkdir -p ~/esp
cd ~/esp

# 2. Clone o repositório do ESP-IDF
git clone -b v5.3 --recursive https://github.com/espressif/esp-idf.git

# 3. Execute o script de instalação para o alvo ESP32-S3
cd ~/esp/esp-idf
./install.sh esp32s3

# 4. Configure as variáveis de ambiente no terminal atual
. ./export.sh
```

> 💡 **Dica:** Adicione o comando `. $HOME/esp/esp-idf/export.sh` ao seu `~/.bashrc` ou `~/.zshrc` para carregar o ambiente automaticamente.
