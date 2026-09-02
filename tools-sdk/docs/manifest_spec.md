# Especificação do Manifesto (`manifest.json`) — v1

Toda Tool para o KIT deve possuir um arquivo `manifest.json` na raiz do seu projeto. Este arquivo descreve os metadados da Tool, versão, compatibilidade e as permissões de hardware que ela solicita ao Runtime do KIT.

## Estrutura Básica

```json
{
  "manifest_version": 1,
  "id": "com.seu_nome.sua_tool",
  "name": "Nome da Tool",
  "version": "1.0.0",
  "version_code": 1,
  "min_runtime": "0.1.0",
  "max_runtime": "1.0.0",
  "author": "Seu Nome",
  "description": "Uma breve descrição do que a Tool faz.",
  "icon": "icon.bin",
  "entry_point": "tool.so",
  "arch": "xtensa-esp32s3",
  "permissions": ["display", "input", "random"],
  "api_level": 1
}
```

## Campos Obrigatórios

| Campo | Tipo | Descrição | Exemplo |
|---|---|---|---|
| `manifest_version` | **int** | A versão da especificação deste manifesto. O KIT CLI suporta apenas `1`. | `1` |
| `id` | **string** | Identificador único global da sua Tool. Use a notação reversa de domínio. Apenas letras minúsculas, números e underlines (`_`). Deve possuir ao menos um ponto (`.`). | `"com.autor.tool"` |
| `name` | **string** | Nome legível da Tool exibido na interface do KIT. | `"Dados de RPG"` |
| `version` | **string** | Versão semântica (Semantic Versioning). | `"1.2.0"` |
| `version_code` | **int** | Número inteiro sequencial usado pelo KIT para determinar atualizações. Maior = mais recente. | `3` |
| `min_runtime` | **string** | Versão mínima do firmware KIT necessária para rodar a Tool. | `"0.1.0"` |
| `max_runtime` | **string** | Versão máxima suportada do firmware (previne quebras em updates maiores). | `"1.0.0"` |
| `author` | **string** | Nome do autor ou organização. | `"KIT Team"` |
| `description` | **string** | Descrição curta da funcionalidade (máx. 120 caracteres recomendado). | `"Rola múltiplos dados."` |
| `icon` | **string** | Caminho para o ícone no pacote. Atualmente reservado para o futuro (use `"icon.bin"`). | `"icon.bin"` |
| `entry_point` | **string** | Nome do objeto compartilhado carregado pelo dispositivo. Deve ser `"tool.so"` (gerado por `kit-cli build --target xtensa`). | `"tool.so"` |
| `arch` | **string** | Arquitetura alvo. Deve ser `"xtensa-esp32s3"`. | `"xtensa-esp32s3"` |
| `permissions` | **Array[string]** | Lista de APIs de hardware que a Tool deseja acessar. | `["display", "audio"]` |

## Campos Opcionais

| Campo | Tipo | Descrição | Exemplo |
|---|---|---|---|
| `api_level` | **int** | Nível da API do KIT contra o qual a Tool foi compilada. | `1` |
| `kind` | **string** | `"tool"` (padrão) ou `"game"`. Na tela "TUDO" da Home, ferramentas aparecem em cima e mini-jogos numa seção separada embaixo. | `"game"` |
| `accent` | **string** | Cor do card da Tool na Home, em hex `#RRGGBB`. Sem isso, o KIT escolhe uma cor da paleta. | `"#4C6EF5"` |
| `home_icon` | **string** | Ícone geométrico do card na Home. Um de: `card` (padrão), `dice`, `spin`, `coin`, `triangle`, `bingo`, `order`, `timer`, `first`, `teams`, `ask`. | `"dice"` |
| `checksum` | **string** | `sha256:<hex>` do `entry_point`. O `kit-cli` preenche ao empacotar; o KIT recusa a Tool se não bater. | `"sha256:cebf..."` |

## Permissões (Permissions)

O Runtime do KIT fornece os ponteiros de API no contexto da Tool **apenas se a permissão foi declarada**. Se você tentar acessar `ctx->api->audio` sem ter `"audio"` nas `permissions`, o ponteiro será `NULL` e sua Tool sofrerá uma exceção de *null pointer dereference*.

Valores suportados em `permissions`:

- `"display"`: Acesso à tela AMOLED, brilho e widget raiz (LVGL).
- `"input"`: Acesso a eventos de toque, gestos (swipe) e botões capacitivos.
- `"storage"`: Acesso ao sistema de arquivos `/tools/<id>/` (persiste dados e lê arquivos).
- `"random"`: Acesso ao Gerador de Números Aleatórios (TRNG) verdadeiro por hardware.
- `"time"`: Acesso ao relógio RTC e contador de milissegundos de alta precisão.
- `"audio"`: Acesso ao buzzer piezoelétrico para bipes e frequências.
- `"power"`: Acesso ao controle de bateria, sleep e wake locks.
- `"system"`: Acesso a informações de versão e solicitação de saída da Tool.
- `"imu"`: Acesso ao acelerômetro e callback de detecção de gestos como chacoalhar (*shake*).
- `"network"`: (Futuro) Acesso ao rádio Wi-Fi / BLE.

## Pacote Final (`.kit`)

O empacotamento é feito através da ferramenta oficial de linha de comando:

```bash
kit-cli pack /caminho/para/projeto -o minha_tool.kit
```

O arquivo gerado (`.kit`) é um arquivo `.zip` padrão que contém o `manifest.json`
(com os hashes SHA-256 dos artefatos), o objeto `tool.so`, a pasta `assets/` e —
em pacotes do catálogo — a `signature.bin` (assinatura Ed25519). Ele tem um limite
máximo rígido de **7 MB**. Ver [Formato de Pacote](../../docs/tools/package-format.md)
e [Catálogo Comunitário de Tools](../../docs/tools/registry.md).
