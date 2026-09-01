idf_component_register(
    SRCS "src/kit_tool_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES joltwallet__littlefs espressif__elf_loader json mbedtls lvgl kit_api kit_runtime kit_audio kit_fonts kit_tool_loader kit_pkg kit_dice kit_bottle kit_coin kit_timer kit_primeiro kit_times kit_bingo kit_quebragelo
)
