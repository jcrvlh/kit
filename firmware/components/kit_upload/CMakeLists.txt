idf_component_register(
    SRCS "src/kit_upload.c"
    INCLUDE_DIRS "include"
    REQUIRES kit_api
    PRIV_REQUIRES kit_tool_manager kit_network esp_http_server esp_hw_support json
)

# kit_upload -> kit_tool_manager -> kit_runtime -> kit_launcher -> kit_upload:
# ciclo de libs estáticas; repete o grupo no link pra resolver os símbolos.
set_property(TARGET ${COMPONENT_LIB} APPEND PROPERTY LINK_INTERFACE_MULTIPLICITY 4)
