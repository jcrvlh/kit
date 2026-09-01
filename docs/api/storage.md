# Storage API

A **Storage API** fornece persistência de dados isolada para cada Tool.

---

## 📑 Assinaturas de Funções

```c
/**
 * Salva uma string com valor associado a uma chave (formato chave-valor persistente).
 */
kit_err_t kit_storage_set_str(const char *key, const char *value);

/**
 * Recupera uma string gravada.
 */
kit_err_t kit_storage_get_str(const char *key, char *buffer, size_t max_len);

/**
 * Salva um valor inteiro de 32 bits.
 */
kit_err_t kit_storage_set_i32(const char *key, int32_t value);

/**
 * Recupera um valor inteiro de 32 bits.
 */
kit_err_t kit_storage_get_i32(const char *key, int32_t *out_value);

/**
 * Abre um arquivo no diretório privado da Tool (/tools/<tool_id>/data/).
 */
FILE *kit_storage_open_file(const char *filename, const char *mode);
```
