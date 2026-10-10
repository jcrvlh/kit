idf_component_register(
    SRCS "src/kit_network.c"
         "src/kit_net_portal.c"
         "src/kit_net_tool.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi esp_event esp_netif esp_timer nvs_flash kit_api kit_time
    PRIV_REQUIRES esp_http_server esp_http_client esp-tls lwip
)
