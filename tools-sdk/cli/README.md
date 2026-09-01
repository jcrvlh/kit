# kit-cli — CLI Oficial de Gerenciamento de Tools do KIT

A ferramenta `kit-cli` auxilia desenvolvedores na criação, validação de manifesto e empacotamento de **Tools** no formato `.kit`.

---

## 🚀 Instalação Local

Você pode instalar o `kit-cli` em modo de desenvolvimento usando `pip`:

```bash
cd tools-sdk/cli
pip install -e .
```

---

## 🛠️ Comandos Disponíveis

### 1. Criar uma nova Tool
```bash
kit-cli new "Dados RPG" --id "com.exemplo.dados"
```
Cria a estrutura de diretórios (`src/main.c`, `manifest.json`, `assets/`, `CMakeLists.txt`).

### 2. Validar o Manifesto de uma Tool
```bash
kit-cli validate ./dados_rpg
# Ou validar diretamente um pacote compilado:
kit-cli validate meu_app.kit
```

### 3. Empacotar a Tool no Formato `.kit`
```bash
kit-cli pack ./dados_rpg -o dados.kit
```
Calcula a soma SHA-256 do executável `tool.elf`, atualiza o `manifest.json` e gera o arquivo ZIP comprimido `.kit`.

> A assinatura Ed25519 dos pacotes (`kit-cli sign`, `signature.bin`) e as trilhas
> de confiança do catálogo serão adicionadas — ver
> [Catálogo Comunitário de Tools](../../docs/tools/registry.md).

### 4. Inspecionar Metadados de um Pacote
```bash
kit-cli info dados.kit
```
Exibe o ID, versão, permissões requeridas, arquitetura e integridade do pacote.
