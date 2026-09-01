# Display AMOLED CO5300 & Renderização Gráfica

Detalhes de integração do display AMOLED de 1.8 polegadas no KIT.

---

## 🖥️ Especificações do Painel

* **Tecnologia:** AMOLED (Active-Matrix Organic Light-Emitting Diode)
* **Resolução:** 368 × 448 pixels
* **Profundidade de Cor:** RGB565 (16-bit, 65k cores)
* **Driver do Painel:** CO5300
* **Interface:** QSPI (Quad SPI @ 40-80 MHz)
* **Vantagens AMOLED:**
  - Contraste infinito (pretos absolutos com pixels desligados).
  - Alto ângulo de visão.
  - Consumo de energia extremamente reduzido com temas escuros (*Dark Mode*).

---

## 🎨 Integração com LVGL v9

* **Framebuffers:** Dois buffers de renderização parcial (10% a 20% da altura da tela) alocados na memória PSRAM para garantir transferências DMA fluidas a 60 FPS sem esgotar a SRAM interna.
* **Flushing:** O driver de flush do LVGL converte os dados do buffer e dispara transmissões assíncronas via driver `esp_lcd` nativo do ESP-IDF.

---

## 💡 Brilho (DCS 0x51)

O brilho do AMOLED é controlado pelo comando MIPI **"Write Display Brightness"
(0x51)** do CO5300 — não há PWM de backlight. Pré-requisitos, enviados uma vez
no `kit_display_init()`:

* **0x53 = 0x2C** (`Write CTRL Display`): liga `BCTRL` (bloco de controle de
  brilho), `DD` (dimming suave) e `BL`. Sem `BCTRL` o 0x51 é aceito e ignorado.
* **0x55 = 0x00** (`Write CABC`): desliga o *content-adaptive backlight*, que
  senão sobrepõe o valor pedido.

**Enquadramento QSPI (pegadinha):** no CO5300 em QSPI todo comando vai como um
endereço de 32 bits — `(0x02 << 24) | (cmd << 8)` — e é assim que o painel o
reconhece. O driver `esp_lcd_co5300` faz isso internamente, mas
`kit_display_set_brightness_impl()` fala direto com o `panel_io` (para não
depender de uma API de brilho que o driver não expõe), então precisa montar o
comando na mão (`co5300_write_cmd()` / macro `CO5300_QSPI_CMD`). Enviar o byte
"cru" do 0x51 **não chega ao painel** — era esse o motivo de o slider de brilho
não ter efeito nenhum.

O valor salvo em NVS (`kit_config`, chave `brightness`) é reaplicado pelo
`kit_runtime` logo após o init do painel (que sobe no brilho máximo).
