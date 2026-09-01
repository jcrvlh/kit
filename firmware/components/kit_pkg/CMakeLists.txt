idf_component_register(
    SRCS "src/kit_pkg.c"
    INCLUDE_DIRS "include"
    REQUIRES kit_api
    PRIV_REQUIRES esp_rom
)
