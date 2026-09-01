# Touch Capacitivo CST820 & Gestos

Especificação da camada de entrada por toque no KIT.

---

## 👆 Especificações do Controlador

* **Controlador:** CST820
* **Interface:** I2C (compartilhada no SDA GPIO 15, SCL GPIO 14)
* **Pino de Interrupção:** GPIO 21 (ativo em nível baixo ao detectar toque)
* **Modo de Operação:** Coordenadas X/Y absolutas mapeadas para a resolução de 368 × 448 pixels.

---

## 🖐️ Eventos e Gestos Suportados

O `kit_input` converte os sinais brutos do CST820 em eventos de alto nível:
- **`KIT_INPUT_TOUCH_DOWN`**: Dedo tocou a tela.
- **`KIT_INPUT_TOUCH_UP`**: Dedo liberou a tela.
- **`KIT_INPUT_TAP`**: Toque rápido em uma coordenada específica.
- **`KIT_INPUT_LONG_PRESS`**: Toque mantido por mais de 500 ms.
- **`KIT_INPUT_SWIPE`**: Deslize com vetor de direção (Up, Down, Left, Right).
- **Gesto de Retorno do Sistema:** Swipe para baixo a partir do topo aciona o retorno ao Launcher pelo Runtime.
