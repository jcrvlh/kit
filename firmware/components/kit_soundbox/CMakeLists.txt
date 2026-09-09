idf_component_register(
    SRCS "src/kit_soundbox.c"
    INCLUDE_DIRS "include"
    REQUIRES lvgl kit_api kit_fonts kit_display kit_config json
)
