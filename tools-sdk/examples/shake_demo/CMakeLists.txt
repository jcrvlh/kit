cmake_minimum_required(VERSION 3.16)
project(shake_demo C)

set(CMAKE_C_STANDARD 11)

add_executable(shake_demo
    src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../stubs/stub_main.c
)

target_link_libraries(shake_demo PRIVATE kit_stubs)
