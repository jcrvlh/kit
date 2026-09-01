# System API

A **System API** fornece informações sobre o dispositivo, status de bateria e controle do ciclo de vida da Tool.

---

## 📑 Assinaturas de Funções

```c
typedef struct {
    uint8_t  battery_percentage; // 0 - 100%
    bool     is_charging;
    uint32_t free_psram_bytes;
    uint32_t free_flash_bytes;
    char     device_id[16];      // Ex: "KIT-A83F"
    char     runtime_version[16];// Ex: "0.1.0"
} kit_system_info_t;

/**
 * Obtém informações consolidadas de status do sistema e hardware.
 */
kit_err_t kit_system_get_info(kit_system_info_t *info);

/**
 * Solicita o encerramento da Tool e retorno imediato ao Launcher.
 */
void kit_system_exit(void);

/**
 * Impede que o dispositivo entre em suspensão automática (útil para timers em andamento).
 */
kit_err_t kit_power_keep_awake(bool enable);
```
