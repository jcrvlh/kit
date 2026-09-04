idf_component_register(
    SRCS "src/kit_runtime.c"
    INCLUDE_DIRS "include"
    REQUIRES
        esp_timer
        freertos
        driver
        lvgl
        kit_config
        kit_display
        kit_input 
        kit_storage 
        kit_tool_manager
        kit_tool_loader
        kit_network
        kit_catalog
        kit_ota
        kit_launcher
        kit_random
        kit_power
        kit_imu
        kit_time
        kit_audio
        kit_api
)
