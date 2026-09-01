idf_component_register(
    SRCS "src/kit_usb_msc.c"
    INCLUDE_DIRS "include"
    REQUIRES kit_api
    PRIV_REQUIRES kit_storage esp_tinyusb esp_driver_sdmmc sdmmc esp_system
)
