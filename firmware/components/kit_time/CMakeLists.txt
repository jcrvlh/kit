idf_component_register(
    SRCS "src/kit_time.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer kit_api
)
