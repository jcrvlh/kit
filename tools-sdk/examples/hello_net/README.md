# Olá Rede — exemplo da API de rede

Busca `https://jcrvlh.github.io/kit/firmware.json` por HTTPS e mostra o status e o
começo da resposta. Exige runtime **0.16.0** ou mais novo.

- **Toque na metade de cima:** GET num host declarado em `network_domains`.
- **Toque na metade de baixo:** GET num host fora da lista; o KIT recusa
  (`KIT_ERR_PERMISSION_DENIED`).

O que a Tool precisa no `manifest.json`:

```json
"permissions": ["display", "input", "time", "network"],
"network_domains": ["jcrvlh.github.io"]
```

## Compilar e instalar

```bash
. $IDF_PATH/export.sh
kit-cli build . --target xtensa      # gera tool.so
```

Copie `manifest.json` e `tool.so` para `tools/com.kit.hellonet/` no cartão
(Modo pen drive). Tools com rede ainda não entram pelo Catálogo do aparelho.

Referência da API: [`api_reference.md`](../../docs/api_reference.md#8-net-api-ctx-api-net).
