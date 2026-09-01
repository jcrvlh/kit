# Display API

A **Display API** provê acesso às funções de desenho e integração com o LVGL v9 para as Tools.

---

## 📑 Assinaturas de Funções

```c
/**
 * Obtém o objeto container raiz (lv_obj_t*) alocado para a tela da Tool.
 */
lv_obj_t *kit_display_get_screen(void);

/**
 * Força a invalidação e redesenho da tela.
 */
kit_err_t kit_display_refresh(void);

/**
 * Ajusta o nível de brilho do painel AMOLED (0 a 100%).
 */
kit_err_t kit_display_set_brightness(uint8_t percentage);

/**
 * Obtém o brilho atual configurado.
 */
uint8_t kit_display_get_brightness(void);
```
