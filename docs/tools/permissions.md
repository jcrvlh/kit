# Modelo de Permissões de Tools

O KIT adota um modelo de permissões estrito baseado no princípio do menor privilégio.

---

## 🛡️ Lista de Permissões Disponíveis

| Permissão | API Liberada | Descrição |
| :--- | :--- | :--- |
| `display` | `kit_display_*` | Permite desenhar na tela e criar widgets LVGL. |
| `input` | `kit_input_*` | Permite receber eventos de toque e gestos. |
| `storage` | `kit_storage_*` | Permite persistir dados no diretório privado da Tool. |
| `random` | `kit_random_*` | Permite gerar números e bytes aleatórios de alta entropia. |
| `time` | `kit_time_*` | Permite consultar relógio RTC e usar delays. |
| `audio` | `kit_audio_*` | Permite emitir bipes e reproduzir arquivos de som. |
| `imu` | `kit_imu_*` | Permite ler acelerômetro e giroscópio. |
| `network` | `kit_network_*` | Permite efetuar requisições de rede. |

---

## 🔒 Aplicação em Tempo de Execução

Se uma Tool tentar acessar uma API não declarada no manifesto:
- O ponteiro correspondente na `kit_api_table_t` será `NULL`.
- Tentativas de acesso direto resultam em retorno de erro imediato `KIT_ERR_PERMISSION_DENIED` ou interrupção segura pelo Runtime.
