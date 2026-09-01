# Input API

A **Input API** gerencia eventos de toque, botões e gestos.

---

## 📑 Tipos e Funções

```c
typedef enum {
    KIT_INPUT_TOUCH_DOWN,
    KIT_INPUT_TOUCH_UP,
    KIT_INPUT_TAP,
    KIT_INPUT_LONG_PRESS,
    KIT_INPUT_SWIPE_UP,
    KIT_INPUT_SWIPE_DOWN,
    KIT_INPUT_SWIPE_LEFT,
    KIT_INPUT_SWIPE_RIGHT
} kit_input_event_type_t;

typedef struct {
    kit_input_event_type_t type;
    int16_t x;
    int16_t y;
    uint32_t duration_ms;
} kit_input_event_t;

typedef void (*kit_input_callback_t)(const kit_input_event_t *event, void *user_data);

/**
 * Registra um callback para eventos brutos de entrada do touch.
 */
kit_err_t kit_input_register_callback(kit_input_callback_t cb, void *user_data);
```
