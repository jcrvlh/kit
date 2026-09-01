cmake_minimum_required(VERSION 3.16)
project(kit_stubs C)

set(CMAKE_C_STANDARD 11)

add_library(kit_stubs STATIC kit_api_stubs.c)

target_include_directories(kit_stubs PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_compile_definitions(kit_stubs PUBLIC KIT_SDK_STUBS)
