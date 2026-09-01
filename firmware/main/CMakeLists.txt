idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES 
        nvs_flash 
        app_update 
        kit_runtime 
        kit_display 
        kit_input 
        kit_storage 
        kit_tool_manager 
        kit_launcher 
        kit_random 
        kit_power 
        kit_time 
        kit_audio
        kit_diagnostics 
        kit_api
        kit_comms
)
