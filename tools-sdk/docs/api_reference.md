# KIT Tools SDK — Referência da API

Esta documentação descreve as interfaces disponibilizadas pelo KIT Runtime para as Tools (`tool.so`).

O ponto de entrada de toda Tool recebe um contexto (`kit_tool_ctx_t`) contendo uma tabela de APIs baseada na especificação do manifesto (permissões).

## `kit_tool_api.h` — Core

### Ciclo de Vida
```c
kit_err_t tool_init(kit_tool_ctx_t *ctx);
void      tool_destroy(void);
```
Obrigatórias em toda Tool. `tool_init` é chamada após o binário ser carregado na RAM. `tool_destroy` é chamada antes do descarregamento (a Tool deve limpar todos os recursos alocados).

---

## 1. Display API (`ctx->api->display`)
**Permissão necessária:** `"display"`

Gere a tela principal e controle o display AMOLED.

- `lv_obj_t* get_screen(void)`: Retorna o objeto base (parent) para criar os widgets LVGL da sua Tool.
- `kit_err_t refresh(void)`: Força uma atualização síncrona do painel.
- `kit_err_t set_brightness(uint8_t percentage)`: Ajusta o brilho (0-100).
- `uint8_t get_brightness(void)`: Retorna o brilho atual.

---

## 2. Input API (`ctx->api->input`)
**Permissão necessária:** `"input"`

Reaja a toques e gestos (como os swipes laterais).

- `kit_err_t register_callback(kit_input_callback_t cb, void *user_data)`: Registra a função que será chamada assincronamente quando ocorrerem toques.

Tipos de evento:
- `KIT_INPUT_TAP`: Toque curto.
- `KIT_INPUT_LONG_PRESS`: Toque longo (útil para "reset" ou opções).
- `KIT_INPUT_SWIPE_LEFT / RIGHT / UP / DOWN`: Gestos direcionais.

---

## 3. Random API (`ctx->api->random`)
**Permissão necessária:** `"random"`

Geração de números verdadeiramente aleatórios usando o TRNG de hardware do ESP32-S3 (alta entropia termal).

- `uint32_t u32(void)`: Inteiro de 32 bits não sinalizado.
- `int32_t range(int32_t min, int32_t max)`: Inteiro aleatório entre min e max (inclusivo).
- `kit_err_t bytes(uint8_t *buffer, size_t length)`: Preenche um buffer com ruído TRNG puro.
- `float get_float(void)`: Float no intervalo [0.0, 1.0].

---

## 4. Storage API (`ctx->api->storage`)
**Permissão necessária:** `"storage"`

Armazenamento não-volátil usando LittleFS (partição `/tools/<id>/`). Útil para configurações, high scores, arquivos em `assets/`, etc.

- `set_str(key, value)` / `get_str(key, out_buffer, max_len)`: Lê e escreve Strings (Key-Value).
- `set_i32(key, value)` / `get_i32(key, out_value)`: Lê e escreve Inteiros.
- `FILE* open_file(filename, mode)`: Acesso ao sistema de arquivos padrão POSIX (`fopen` wrapper) *dentro* do diretório da Tool.

---

## 5. Audio API (`ctx->api->audio`)
**Permissão necessária:** `"audio"`

Controle do Buzzer ativo embutido no KIT.

- `kit_err_t beep(uint16_t freq_hz, uint16_t duration_ms)`: Emite um som PWM (ex: 1500 Hz por 50 ms). Chamada não bloqueante.
- `kit_err_t set_volume(uint8_t percentage)`: Altera intensidade do sinal PWM.

---

## 6. IMU API (`ctx->api->imu`)
**Permissão necessária:** `"imu"`

Acesso ao módulo inercial 6-DOF (QMI8658). Atualmente expõe exclusivamente detecção avançada de "chacoalhar".

- `kit_err_t register_shake_callback(kit_shake_callback_t cb, void *user_data)`: Registra uma função que é invocada sempre que o KIT for fortemente chacoalhado. O Runtime lida com os cálculos vetoriais e debounce (0.7s) internamente.

---

## 7. System & Power API
**Permissões:** `"system"`, `"power"`

- `system->get_info(&info)`: Pega versão do OS, bateria atual e memória RAM livre.
- `system->exit()`: Encerra a Tool proativamente, devolvendo o controle para a Home.
- `power->keep_awake(true)`: Impede que o KIT entre em Deep Sleep por inatividade (cuidado com bateria).
