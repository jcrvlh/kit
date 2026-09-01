idf_component_register(
    SRCS "src/kit_network.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi esp_event esp_netif kit_api
)
