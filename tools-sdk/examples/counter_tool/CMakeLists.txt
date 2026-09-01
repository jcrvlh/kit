cmake_minimum_required(VERSION 3.16)
project(counter_tool C)

set(CMAKE_C_STANDARD 11)

add_executable(counter_tool
    src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../stubs/stub_main.c
)

target_link_libraries(counter_tool PRIVATE kit_stubs)
