idf_component_register(
    SRCS "src/kit_audio.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer freertos kit_power kit_api espressif__esp_codec_dev
)
