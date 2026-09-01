cmake_minimum_required(VERSION 3.16)
project(fora_tool C)

set(CMAKE_C_STANDARD 11)

# Stubs library (compilação desktop)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../stubs stubs_build)

add_executable(tool.elf
    src/main.c
    src/fora_game.c
    src/fora_words.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../stubs/stub_main.c
)

target_include_directories(tool.elf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(tool.elf PRIVATE kit_stubs)
