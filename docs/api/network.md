# Network API (Stub / Planejada)

A **Network API** provê abstração para conexões de rede e requisições HTTP para Tools com permissão `network`.

---

## 📑 Assinaturas Previstas

```c
/**
 * Verifica se o KIT está conectado a uma rede Wi-Fi com acesso à internet.
 */
bool kit_network_is_connected(void);

/**
 * Realiza uma requisição HTTP GET simples para um endpoint seguro (HTTPS).
 */
kit_err_t kit_network_http_get(const char *url, char *response_buffer, size_t max_len);
```
