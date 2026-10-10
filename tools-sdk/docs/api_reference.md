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

Alto-falante embutido (codec ES8311). Todas as chamadas são não bloqueantes
(enfileiram e voltam) e respeitam a flag **"Som"** dos Ajustes.

- `kit_err_t beep(uint16_t freq_hz, uint16_t duration_ms)`: Tom senoidal (ex: 1500 Hz por 50 ms).
- `kit_err_t set_volume(uint8_t percentage)`: Volume do alto-falante (0–100).
- `kit_err_t sfx(kit_sfx_t sfx)`: Toca um efeito sonoro pronto do KIT (ver `kit_sfx_t`). Para o toque de botão, use `KIT_SFX_TAP` (runtime ≥ 0.15.0): um "tic" médio-agudo e curto. Um `beep()` curto e grave (abaixo de ~600 Hz) distorce no alto-falante do KIT.
- `kit_err_t fuse(int16_t tension)`: "Pavio queimando" — tique metronômico gerado na task de áudio; `tension` 0–255 acelera, `< 0` apaga.
- `kit_err_t play_sample(const char *path)`: Toca um `.wav` do cartão microSD. **Requer `min_runtime` `0.7.0`.** `path` absoluto sob `/sdcard/` — em geral `"<ctx->data_path>/assets/<nome>.wav"` (asset embutido no `.kit`) ou `"/sdcard/soundbox/<banco>/<nome>.wav"`. Formato: **WAV PCM 16-bit mono, 16 kHz** (8 kHz também; estéreo é rebaixado). Um som novo corta o anterior (retrigger, sem polifonia).
- `kit_err_t stop_sample(void)`: Corta o sample em reprodução.

---

## 6. IMU API (`ctx->api->imu`)
**Permissão necessária:** `"imu"`

Acesso ao módulo inercial 6-DOF (QMI8658). Tudo em inteiros: o loader das Tools não resolve float.

- `kit_err_t register_shake_callback(kit_shake_callback_t cb, void *user_data)`: Registra uma função que é invocada sempre que o KIT for fortemente chacoalhado. O Runtime lida com os cálculos vetoriais e debounce (0.7s) internamente.
- `kit_err_t register_tilt_callback(kit_tilt_callback_t cb, void *user_data)` (runtime ≥ 0.2.0): gesto discreto de virar a tela pro chão (`KIT_TILT_DOWN`) ou pro teto (`KIT_TILT_UP`), uma vez por inclinada.
- `gyro_start()` · `gyro_rezero()` · `gyro_poll(&yaw, &pitch, &roll, &rate)` · `gyro_stop()` (runtime ≥ 0.4.0): giroscópio sob demanda, ângulo **relativo** ao último zero, em centigraus.
- `bool accel_poll(int32_t *x_mg, int32_t *y_mg, int32_t *z_mg)` (runtime ≥ 0.15.0): aceleração em mili-g.
- `bool accel_tilt(int32_t *x_cdeg, int32_t *y_cdeg)` (runtime ≥ 0.15.0): inclinação **absoluta** pela gravidade, em centigraus (-9000..9000), sem calibrar e sem drift.

**Eixos do acelerômetro** — os da tela, como o LVGL desenha, já com a rotação do Modo canhoto:

| Eixo | Sentido | `accel_tilt` positivo |
| :--- | :--- | :--- |
| `x` | pra direita | borda direita mais alta |
| `y` | pra baixo | borda de baixo mais alta |
| `z` | saindo da tela, pro rosto | — |

Parado, o vetor aponta pra cima: deitado de tela pra cima dá `(0, 0, +1000)`; em pé na mão, tela pro rosto, `(0, -1000, 0)`. Em movimento soma-se a aceleração da mão. O acelerômetro fica ligado sempre que a tela está acesa (sem start/stop); com a tela em repouso as duas funções devolvem `false`.

```c
/* Bolinha que rola pro lado mais baixo da tela. */
int32_t tx, ty;
if (api->imu->accel_tilt(&tx, &ty)) {
    s_vx -= tx / 100;   /* centigraus -> graus */
    s_vy -= ty / 100;
}
```

---

## 7. System & Power API
**Permissões:** `"system"`, `"power"`

- `system->get_info(&info)`: Pega versão do OS, bateria atual e memória RAM livre.
- `system->exit()`: Encerra a Tool proativamente, devolvendo o controle para a Home.
- `power->keep_awake(true)`: Impede que o KIT entre em Deep Sleep por inatividade (cuidado com bateria).
