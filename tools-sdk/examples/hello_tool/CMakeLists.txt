cmake_minimum_required(VERSION 3.16)
project(hello_tool C)

set(CMAKE_C_STANDARD 11)

add_executable(hello_tool
    src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../stubs/stub_main.c
)

target_link_libraries(hello_tool PRIVATE kit_stubs)
