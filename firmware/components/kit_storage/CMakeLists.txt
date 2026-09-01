idf_component_register(
    SRCS "src/kit_storage.c"
    INCLUDE_DIRS "include"
    REQUIRES joltwallet__littlefs esp_partition nvs_flash kit_api
             fatfs esp_driver_sdmmc sdmmc
)
