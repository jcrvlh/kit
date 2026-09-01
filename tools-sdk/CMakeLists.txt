cmake_minimum_required(VERSION 3.16)
project(kit_tools_sdk C)

set(CMAKE_C_STANDARD 11)

# --- Stubs (biblioteca estática para compilação desktop) ---
add_subdirectory(stubs)

# --- Exemplos ---
option(KIT_SDK_BUILD_EXAMPLES "Compilar exemplos de Tools" ON)

if(KIT_SDK_BUILD_EXAMPLES)
    add_subdirectory(examples/hello_tool)
    add_subdirectory(examples/counter_tool)
    add_subdirectory(examples/shake_demo)
endif()
