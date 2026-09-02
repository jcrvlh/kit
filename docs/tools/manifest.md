# Especificação do Manifesto de Tool (`manifest.json`)

O arquivo `manifest.json` descreve os metadados, requisitos e permissões de uma Tool.

---

## 📄 Estrutura JSON (Versão 1)

```json
{
  "manifest_version": 1,
  "id": "com.kit.dice",
  "name": "Dados",
  "version": "1.0.0",
  "version_code": 1,
  "min_runtime": "0.1.0",
  "max_runtime": "1.0.0",
  "author": "KIT Community",
  "description": "Rolador de dados poliédricos para jogos de RPG e mesa.",
  "icon": "icon.bin",
  "entry_point": "tool.so",
  "arch": "xtensa-esp32s3",
  "size_installed": 128000,
  "checksum": {
    "tool.so": "sha256:e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "assets/dice_faces.bin": "sha256:1b4f0e9851971998e732078544c96b36c3d01cedf7caa332359d6f1d83567014"
  },
  "key_id": "kit-release-2026",
  "permissions": [
    "display",
    "input",
    "random",
    "audio"
  ],
  "api_level": 1,
  "assets": [
    "assets/dice_faces.bin"
  ]
}
```

---

## 🔑 Descrição dos Campos

* `manifest_version`: Versão do formato de manifesto (inicia em 1).
* `id`: Identificador exclusivo em formato de domínio reverso (ex: `com.autor.toolname`).
* `version`: Versão legível em SemVer.
* `version_code`: Número inteiro estritamente incremental usado para verificar atualizações.
* `min_runtime` / `max_runtime`: Intervalo de compatibilidade com a versão do KIT Runtime.
* `arch`: Deve ser obrigatoriamente `xtensa-esp32s3`.
* `checksum`: Objeto com o hash SHA-256 de `tool.so` e de cada asset. É o que a
  assinatura do pacote (`signature.bin`) cobre transitivamente.
* `key_id`: Identificador da chave usada para assinar o pacote. Presente apenas em
  pacotes do [catálogo](registry.md); ausente em pacotes Sideload.
* `permissions`: Array de permissões de APIs requeridas.

> O `manifest.json` deve ter serialização canônica estável (chaves ordenadas, sem
> espaços supérfluos, UTF-8 sem BOM) — a `kit-cli` é a referência de
> canonicalização, pois a assinatura é calculada sobre esses bytes exatos.
