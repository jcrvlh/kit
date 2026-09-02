idf_component_register(
    SRCS "src/kit_tool_loader.c"
         "src/kit_tool_symbols.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    REQUIRES kit_api
    PRIV_REQUIRES espressif__elf_loader lvgl kit_fonts
)

# Interpõe esp_elf_malloc (ver kit_tool_loader.c): manda as alocações
# não-executáveis do elf_loader para a PSRAM, deixando só o segmento de código
# da Tool na RAM interna. Reduz o pico de RAM interna contígua na carga.
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_elf_malloc")
