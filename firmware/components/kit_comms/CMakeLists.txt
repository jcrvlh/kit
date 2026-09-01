idf_component_register(
    SRCS "src/kit_comms.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_timer vfs esp_vfs_console kit_storage kit_tool_manager kit_api
)
