idf_component_register(
    SRCS "src/kit_imu.c"
    INCLUDE_DIRS "include"
    REQUIRES kit_api kit_power esp_timer
)
