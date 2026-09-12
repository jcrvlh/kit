# espressif/esp_lcd_co5300 — override do KIT

Cópia do componente `espressif/esp_lcd_co5300` **v1.0.2** do registro da ESP-IDF
com **uma** mudança em relação ao original.

## A mudança

`esp_lcd_co5300_spi.c`, no fim de `panel_co5300_draw_bitmap()`:

```c
- tx_color(co5300, io, LCD_CMD_RAMWR, color_data, len);
-
- return ESP_OK;
+ ESP_RETURN_ON_ERROR(tx_color(co5300, io, LCD_CMD_RAMWR, color_data, len),
+                     TAG, "send color failed");
+
+ return ESP_OK;
```

## Por quê

O original **descarta** o retorno de `tx_color()` e devolve `ESP_OK`
incondicionalmente. Quando o envio falha, quem chamou
`esp_lcd_panel_draw_bitmap()` recebe "deu certo" — mas o callback
`on_color_trans_done` **nunca** vai disparar, porque nada foi enfileirado.

No KIT isso trava a interface inteira: o `lvgl_flush_cb` do `kit_display`
depende desse callback para chamar `lv_display_flush_ready()`, e o LVGL espera
em `wait_for_flushing()` (`lv_refr.c`), que é um `while(disp->flushing);` **sem
yield nenhum**. O loop principal para de rodar, a task `IDLE0` nunca é
escalonada e o `task_wdt` dispara.

O caso real que expôs isso: abrir **Ajustes → Wi-Fi → Configurar rede**. Com o
AP do portal no ar, `spi_device_queue_trans()` passou a falhar com
`ESP_ERR_NO_MEM` (ver o comentário em `kit_display_init()` sobre o bounce
buffer de DMA), e o erro sumia aqui dentro — a tela congelava ou "misturava" o
quadro velho com o novo, sem nenhuma pista no log.

Com o retorno propagado, o `kit_display` vê o erro, solta o LVGL na hora e
registra a causa:

```
E co5300_spi: panel_co5300_draw_bitmap(291): send color failed
W KIT_DISPLAY: draw_bitmap falhou (ESP_ERR_NO_MEM) — liberando o flush
```

Foi assim que a causa raiz apareceu. Não remover ao atualizar o managed
component — reaplicar a mudança.
