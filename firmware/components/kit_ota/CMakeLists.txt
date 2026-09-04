idf_component_register(
    SRCS "src/kit_ota.c"
    INCLUDE_DIRS "include"
    REQUIRES kit_api kit_network kit_power kit_config
    PRIV_REQUIRES esp_http_client esp-tls mbedtls json esp_timer app_update esp_app_format esp_system
)
