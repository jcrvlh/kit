idf_component_register(
    SRCS "src/kit_audio.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer esp_hw_support freertos kit_power kit_config kit_api espressif__esp_codec_dev
)
