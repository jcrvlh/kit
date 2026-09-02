idf_component_register(
    SRCS "src/kit_network.c"
         "src/kit_net_portal.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi esp_event esp_netif esp_timer nvs_flash kit_api kit_time
    PRIV_REQUIRES esp_http_server lwip
)
