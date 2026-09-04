# Estratégia de OTA (Over-The-Air) e Rollback

O KIT implementa um mecanismo de atualização de firmware robusto e à prova de falhas com base nas capacidades nativas do ESP-IDF 5.x.

---

## 🔄 Arquitetura Dual-Slot com Factory Recovery

```text
               +----------------------------------+
               |         BOOTLOADER (ROM)         |
               +----------------------------------+
                                 │
                                 ▼
               +----------------------------------+
               |        2nd STAGE BOOTLOADER      |
               +----------------------------------+
                                 │
               ┌─────────────────┴─────────────────┐
               ▼                                   ▼
      [ otadata Válido? ]                 [ otadata Inválido / Corrompido ]
               │                                   │
       ┌───────┴───────┐                           │
       ▼               ▼                           ▼
  [ Slot ota_0 ]  [ Slot ota_1 ]          [ Factory Recovery App ]
       │               │                           │
       └───────┬───────┘                           │
               ▼                                   │
      [ Health Check OK? ]                         │
         │          │                              │
        SIM        NÃO                             │
         │          │                              │
         │          ▼                              │
         │    [ Rollback para Slot Anterior ]      │
         │          │                              │
         │      Falhou?                            │
         │       SIM ──► [ Boot Factory Recovery ] ◄
         ▼
[ KIT Runtime Pronto ]
```

---

## 🛡️ Health Check e Cancelamento de Rollback

Ao instalar uma nova versão de firmware no slot inativo:
1. O dispositivo reinicia no novo slot com estado `ESP_OTA_IMG_PENDING_VERIFY`.
2. O sistema executa testes de integridade:
   - Inicialização bem-sucedida de display, touch e LittleFS.
   - Ausência de travamentos (*panics*) nos primeiros 5 segundos.
3. Se todos os testes passarem, a função `esp_ota_mark_app_valid_cancel_rollback()` é executada, fixando o novo firmware como padrão.
4. Se ocorrer falha ou timeout do Watchdog, o bootloader reverte automaticamente para a versão estável anterior no próximo boot.

---

## 🌐 Cliente OTA (`kit_ota`)

Componente `firmware/components/kit_ota` — mesmo desenho *offline-first* do
`kit_catalog` (task própria ociosa, HTTPS pontual com o cert bundle CMN,
`state` + callback, progresso 0–100). Decisão em
[ADR-0013](../decisions/ADR-0013-ota-firmware-update.md).

### Manifesto — `https://jcrvlh.github.io/kit/firmware.json`

| Campo | Uso |
| :--- | :--- |
| `version` / `version_code` | `"0.2.0"` / `200`. O device compara `MAJOR.MINOR.PATCH` de `esp_app_get_description()->version`. |
| `notes` | Linha curta (ASCII, ≤160) mostrada na tela. |
| `url` | `kit_core.bin` anexado à GitHub Release `fw-v<versão>`. |
| `sha256` / `size` | Conferidos no download (streaming). |

Gerado na CI (`pages.yml`) a partir de `firmware/version.txt` + do `.bin`
compilado, só quando a Release já existe. O `.bin` sai por `firmware-release.yml`
(tag `fw-v*`).

### Máquina de estados

```text
IDLE ─check──▶ CHECKING ─┬─▶ UP_TO_DATE
                         └─▶ AVAILABLE ─apply──▶ DOWNLOADING ──▶ APPLYING ──▶ DONE ──▶ (reiniciar)
                                                     │              │
                                             OFFLINE / NO_POWER / ERR  (slot atual intacto)
```

- **Check automático:** 1×/boot, ~30 s após obter IP, no máx. 1×/dia
  (`ota_last_chk` em NVS; `ota_auto=0` desliga). Achou versão nova → toast +
  ponto no card *Ajustes* (uma vez por `version_code`).
- **Apply:** sempre manual, em *Ajustes → Atualizar firmware*. Escreve direto no
  slot inativo (`esp_ota_write`), sem passar pelo cartão.

### Salvaguardas

| Risco | Proteção |
| :--- | :--- |
| Queda de energia no meio da gravação | Exige **cabo USB conectado** para aplicar. |
| Imagem que não é do KIT no slot | Confere `esp_app_desc_t.project_name == "kit_core"` após `esp_ota_end`; senão apaga o começo do slot e aborta. |
| Tela dormir / Wi-Fi cair durante a operação | `kit_power_keep_awake_impl(true)` enquanto baixa/grava. |
| Sair da tela no meio | "Voltar" travado em `DOWNLOADING`/`APPLYING`. |
| Binário corrompido ou trocado | SHA-256 contra o manifesto (HTTPS) + validação nativa da imagem no `esp_ota_end`. |

*Downgrade não é bloqueado* — reinstalar uma versão anterior é recuperação
válida. Assinatura Ed25519 / Secure Boot: evolução futura (ADR-0013 · ADR-0012).
