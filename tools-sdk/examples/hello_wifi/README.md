# Olá Wi-Fi — exemplo da API de scan

Varre as redes Wi-Fi em volta e lista as mais fortes: sinal (dBm), canal,
segurança e nome. Rede oculta aparece como `(oculta)`. Exige runtime **0.18.0**
ou mais novo.

- **Ao abrir:** já faz o primeiro scan.
- **Toque na tela:** varre de novo. Antes de 5 s do scan anterior, mostra `AGUARDE`.

O KIT só olha as redes: não conecta em nenhuma. Funciona com o Wi-Fi do KIT
desligado (o rádio liga só durante o scan).

O que a Tool precisa no `manifest.json`:

```json
"permissions": ["display", "input", "time", "wifi_scan"]
```

## Compilar e instalar

```bash
. $IDF_PATH/export.sh
kit-cli build . --target xtensa      # gera tool.so
```

Copie `manifest.json` e `tool.so` para `tools/com.kit.hellowifi/` no cartão
(Modo pen drive).

Referência da API: [`api_reference.md`](../../docs/api_reference.md#9-wi-fi-scan-api-ctx-api-wifi).
