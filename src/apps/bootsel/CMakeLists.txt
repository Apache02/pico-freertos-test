set(NAME pico-freertos-bootsel)

project(${NAME} C CXX ASM)

add_executable(${NAME}
        main.cpp
        )

pico_enable_stdio_usb(${NAME} 1)
pico_enable_stdio_uart(${NAME} 0)

pico_add_extra_outputs(${NAME})

target_include_directories(${NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(${NAME}
        pico_stdlib
        FreeRTOS-Kernel
        FreeRTOS-Kernel-Heap4
        task-bootsel-pool
        )

add_flash_target(${NAME} "${CMAKE_CURRENT_BINARY_DIR}/${NAME}.uf2")
