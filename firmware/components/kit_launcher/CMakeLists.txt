idf_component_register(
    SRCS    "src/kit_launcher.c"
    "src/kit_icon_triangle.c"
    INCLUDE_DIRS "include"
    REQUIRES lvgl kit_tool_manager kit_power kit_display kit_audio kit_config kit_api kit_fonts
)
