set(NAME picow-blink)

project(${NAME} C CXX ASM)

add_executable(${NAME}
        main.cpp
        )

pico_enable_stdio_usb(${NAME} 1)
pico_enable_stdio_uart(${NAME} 0)

pico_add_extra_outputs(${NAME})

include_directories(${CMAKE_CURRENT_LIST_DIR}/../../config)
target_link_libraries(${NAME}
        pico_stdlib
        pico_cyw43_arch_none
        FreeRTOS-Kernel
        FreeRTOS-Kernel-Heap4
        )

add_deploy_target(${NAME} "${CMAKE_CURRENT_BINARY_DIR}/${NAME}.uf2")
