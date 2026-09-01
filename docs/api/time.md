# Time API

A **Time API** fornece funções de relógio em tempo real, timestamps e temporizadores.

---

## 📑 Assinaturas de Funções

```c
typedef struct {
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
} kit_datetime_t;

/**
 * Obtém os milissegundos decorridos desde a inicialização do KIT.
 */
uint64_t kit_time_get_millis(void);

/**
 * Obtém a data e hora atual do RTC PCF85063A.
 */
kit_err_t kit_time_get_datetime(kit_datetime_t *dt);

/**
 * Pausa a execução da tarefa atual por uma quantidade de milissegundos.
 */
void kit_time_delay_ms(uint32_t ms);
```
