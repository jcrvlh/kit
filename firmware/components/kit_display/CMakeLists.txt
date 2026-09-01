idf_component_register(
    SRCS "src/kit_display.c"
    INCLUDE_DIRS "include"
    REQUIRES lvgl esp_lcd esp_driver_spi driver esp_timer kit_api espressif__esp_lcd_co5300
)

