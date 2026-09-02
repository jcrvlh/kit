idf_component_register(
    SRCS "src/kit_catalog.c"
    INCLUDE_DIRS "include"
    REQUIRES kit_api kit_tool_manager kit_network
    PRIV_REQUIRES esp_http_client esp-tls mbedtls json esp_timer
)
