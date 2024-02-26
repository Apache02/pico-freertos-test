cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)
include(scripts/functions.cmake)
include(freertos_rp2040_import.cmake)

project(pico-freertos-test C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_subdirectory(src/blink)
add_subdirectory(src/blinks-morze)
add_subdirectory(src/bootsel)
