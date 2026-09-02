idf_component_register(
    SRCS "src/kit_time.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer esp_netif lwip kit_api kit_power
)
