# Random API

A **Random API** fornece geração de números aleatórios uniforme e segura, combinando o TRNG (Hardware True Random Number Generator) do ESP32-S3 com um PRNG de alta velocidade.

---

## 📑 Assinaturas de Funções

```c
/**
 * Retorna um inteiro não-sinalizado de 32 bits aleatório uniforme.
 */
uint32_t kit_random_u32(void);

/**
 * Retorna um inteiro entre min e max (inclusivo).
 * Ex: kit_random_range(1, 6) para um dado de 6 faces.
 */
int32_t kit_random_range(int32_t min, int32_t max);

/**
 * Preenche um buffer de memória com bytes aleatórios de alta entropia.
 */
kit_err_t kit_random_bytes(uint8_t *buffer, size_t length);

/**
 * Retorna um número de ponto flutuante entre 0.0f e 1.0f.
 */
float kit_random_float(void);
```
