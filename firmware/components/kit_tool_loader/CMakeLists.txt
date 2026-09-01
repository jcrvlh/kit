idf_component_register(
    SRCS "src/kit_tool_loader.c"
         "src/kit_tool_symbols.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    REQUIRES kit_api
    PRIV_REQUIRES espressif__elf_loader lvgl
)
