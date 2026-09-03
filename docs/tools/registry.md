# Catálogo Comunitário de Tools

O **catálogo** é a lista pública de Tools instaláveis no KIT. Ele é servido como um
arquivo estático `index.json` (via GitHub Pages) e consumido tanto pelo
web-installer quanto, futuramente, pelo próprio dispositivo por Wi-Fi.

Não há servidor de aplicação: o catálogo é um repositório Git (`kit-tools`) com
CI. Isso mantém a distribuição gratuita, auditável (todo pacote nasce de um Pull
Request) e reprodutível (os `.kit` são construídos pela CI, não enviados prontos).

---

## 🧩 Tools built-in vs. Tools do catálogo

| | **Built-in no Core** | **Tools do catálogo** |
| :--- | :--- | :--- |
| Onde vive | Componentes do firmware (`firmware/components/kit_*`), repositório `kit` | Repositório `kit-tools`, distribuídas como `.kit` |
| Instalação | Já vêm no Core; despachadas pelo `kit_tool_manager` por `id` | Instaladas em `/sdcard/tools/<id>/` (catálogo, Wi-Fi ou web-installer) |
| Curadoria | Conjunto oficial mínimo, mantido pela equipe | Aberto a contribuições da comunidade |
| Exemplos | `kit_dice`, `kit_bingo`, `kit_coin`, `kit_timer`, `kit_times`, `kit_primeiro`, `kit_placar`, `kit_bottle` | `io.github.jcrvlh.quebragelo`, `io.github.jcrvlh.pavio`, `io.github.jcrvlh.adedonha`, `io.github.jcrvlh.veto`, `io.github.jcrvlh.mimica`, `io.github.jcrvlh.testa`, `io.github.jcrvlh.tarot`, `io.github.jcrvlh.fora` |

O Core traz um conjunto curado e enxuto. Tudo além disso é distribuído pelo
catálogo — inclusive Tools oficiais que não justifiquem ocupar espaço na Flash de
todos os dispositivos.

---

## 🌐 Estágios de distribuição

1. **Catálogo Git (atual):** submissão por PR no `kit-tools`; a CI compila, empacota
   com `kit-cli pack`, assina o pacote e publica o
   `.kit` em GitHub Releases; regenera o `index.json` no GitHub Pages. O
   web-installer ganha uma aba **Catálogo** que lê o `index.json`, baixa o `.kit`
   e o instala pelo mesmo caminho WebSerial já existente
   (`KIT_TOOL_BEGIN` / `KIT_FILE_WRITE` / `KIT_TOOL_COMMIT`).
2. **Catálogo no dispositivo (Fase 3):** com Wi-Fi, o firmware lê o **mesmo**
   `index.json` por HTTPS e instala direto; atualização detectada por
   `version_code`.
3. **Backend dedicado (se necessário):** só se surgir demanda por busca, avaliações,
   contas ou telemetria. Serviria a mesma estrutura de `index.json`.

---

## 🆔 Namespaces de `id`

O `id` é um identificador em domínio reverso, único no catálogo.

| Prefixo | Uso |
| :--- | :--- |
| `com.kit.*`, `org.kit.*` | **Reservados** para Tools oficiais do projeto. PRs de terceiros com esses prefixos são recusados. |
| `<seu-domínio-reverso>.*` | Autores com domínio próprio (ex.: `br.com.fulano.jogo`). |
| `io.github.<usuário>.*` | Autores sem domínio, usando o handle do GitHub. |

Regras aplicadas na revisão do PR:
- unicidade de `id` em todo o catálogo;
- `version_code` estritamente incremental entre versões do mesmo `id`;
- o `id` não pode ser transferido para outro autor sem aprovação de um mantenedor.

---

## 🔐 Assinatura e trilhas de confiança

O `checksum.sha256` do pacote garante **integridade**, não **autenticidade**. Para
o catálogo aberto, cada `.kit` publicado é **assinado com Ed25519** sobre os bytes
canônicos do `manifest.json` — que já contém o hash SHA-256 de `tool.so` e de
cada asset, então uma assinatura cobre o pacote inteiro.

```
signature.bin  ──assina──▶  manifest.json (bytes canônicos)
                                   ├─ checksum de tool.so
                                   └─ checksum de cada assets/*
```

* A assinatura de 64 bytes vai em `signature.bin` na raiz do pacote; o campo
  `key_id` no manifesto identifica a chave.
* O firmware embute um **vetor** de chaves públicas de release (permite rotação
  via OTA). A CI do `kit-tools` assina tudo que é mesclado com a chave de release
  do projeto — a revisão no PR é o portão de confiança.
* O `manifest.json` precisa de serialização canônica estável (chaves ordenadas,
  sem espaços supérfluos, UTF-8 sem BOM). A `kit-cli` é a referência.

| Trilha | Origem | Verificação no dispositivo | Aviso ao usuário |
| :--- | :--- | :--- | :--- |
| **Oficial** | Tool mantida pela equipe do KIT, revisada e mesclada em `kit-tools`. | Assinatura confere com chave pública embutida no firmware. | Nenhum. Selo "Catálogo KIT". |
| **Comunidade** | Tool de terceiros, revisada por um mantenedor e mesclada em `kit-tools`. | Igual à Oficial (mesma chave de release). | "Tool da comunidade — revisada pela equipe KIT." A distinção é de autoria, não de nível criptográfico. |
| **Sideload** | `.kit` local fora do catálogo (web-installer com arquivo próprio, microSD, `kit-cli flash`). | Apenas SHA-256. | Confirmação explícita; exige **Modo Desenvolvedor** ligado em Configurações. |

Verificação em três pontos: a CI assina na publicação; o `kit_tool_manager` valida
assinatura + hashes no `KIT_TOOL_COMMIT`; o hash de `tool.so` é reconferido a cada
carregamento, antes da relocação em PSRAM. O `index.json` carrega uma lista
`revoked` que o dispositivo sincroniza quando online.

Chaves por desenvolvedor (cada autor assina o próprio pacote, dispositivo valida
contra a chave registrada no catálogo) ficam como evolução futura.

---

## 📤 Fluxo de submissão (repositório `kit-tools`)

```
kit-tools/
├── tools/
│   └── <id>/
│       ├── manifest.json
│       ├── CMakeLists.txt
│       ├── src/
│       ├── icon.bin
│       ├── assets/            # opcional
│       └── README.md
├── catalog/
│   └── index.json             # gerado pela CI — não editar à mão
└── .github/workflows/
```

1. **Fork + PR** adicionando `tools/<id>/`.
2. **CI de validação** (roda no PR):
   - `kit-cli validate` — manifesto, arch (`xtensa-esp32s3`), permissões declaradas;
   - build com `kit-cli build . --target xtensa` sem alertas críticos;
   - `kit-cli pack` e checagem de limites de tamanho (`size_installed`);
   - verificação de namespace do `id` e de `version_code`;
   - destaque das permissões sensíveis (`network`) para revisão manual.
3. **Revisão humana** por um mantenedor (código, conteúdo, aderência à
   [linguagem visual](../design/design-language.md)).
4. **Merge → CI de publicação:**
   - compila o `.kit` final;
   - assina com a chave de release do projeto (assinatura Ed25519);
   - publica em `Releases` (`<id>-v<version>`);
   - regenera `catalog/index.json` e faz deploy no GitHub Pages.

Atualizar uma Tool = novo PR do mesmo autor, com `version` e `version_code`
incrementados.

---

## 📄 Estrutura do `index.json`

```json
{
  "catalog_version": 1,
  "generated_at": "2026-08-31T00:00:00Z",
  "keys": [
    {
      "key_id": "kit-release-2026",
      "algo": "ed25519",
      "public_key": "<base64 de 32 bytes>",
      "owner": "Projeto KIT",
      "official": true
    }
  ],
  "revoked": [],
  "tools": [
    {
      "id": "io.github.fulano.labirinto",
      "name": "Labirinto",
      "version": "1.2.0",
      "version_code": 5,
      "min_runtime": "0.1.0",
      "max_runtime": "1.0.0",
      "author": "Fulano de Tal",
      "description": "Gerador de labirintos para desafios rápidos.",
      "tier": "community",
      "permissions": ["display", "input", "random"],
      "size_installed": 128000,
      "key_id": "kit-release-2026",
      "package": {
        "url": "https://github.com/<org>/kit-tools/releases/download/io.github.fulano.labirinto-v1.2.0/labirinto.kit",
        "sha256": "e3b0c442...",
        "size": 41234
      },
      "icon_url": "https://<org>.github.io/kit-tools/icons/io.github.fulano.labirinto.png",
      "source_url": "https://github.com/<org>/kit-tools/tree/main/tools/io.github.fulano.labirinto",
      "updated_at": "2026-08-31T00:00:00Z"
    }
  ]
}
```

### Campos
* `keys` — chaves públicas confiáveis; o dispositivo já embute as oficiais, mas o
  catálogo as repete para o web-installer.
* `revoked` — lista de `id@version_code` ou `key_id` que o dispositivo deve recusar.
* `tools[].tier` — `official` ou `community`; ambas são assinadas e revisadas (ver _Assinatura e trilhas de confiança_ acima).
* `tools[].package` — URL, hash e tamanho do `.kit`; o hash é reconferido após o
  download.
* Os campos de compatibilidade (`min_runtime` / `max_runtime` / `permissions`)
  espelham o `manifest.json` para o cliente filtrar antes de baixar.

---

## 🔗 Relacionados

* [Formato de Pacote (`.kit`)](package-format.md)
* [Manifesto (`manifest.json`)](manifest.md)
* [Modelo de Permissões](permissions.md)
* [Segurança e Integridade](../architecture/security.md)
