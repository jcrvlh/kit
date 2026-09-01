idf_component_register(
    SRCS "src/kit_power.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_i2c esp_hw_support esp_pm freertos kit_api
)
