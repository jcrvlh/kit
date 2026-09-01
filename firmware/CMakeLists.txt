# CMakeLists.txt para o KIT Core Firmware
cmake_minimum_required(VERSION 3.16)

# Inclui os componentes modulares do KIT
set(EXTRA_COMPONENT_DIRS "components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(kit_core)
