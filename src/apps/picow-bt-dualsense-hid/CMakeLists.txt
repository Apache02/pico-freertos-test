set(NAME picow-bt-dualsense-hid)

set(DUALSENSE_ADDR "${WIFI_SSID}" CACHE INTERNAL "DualSense Wireless Controller MAC")

if (DEFINED ENV{DUALSENSE_ADDR} AND (NOT DUALSENSE_ADDR))
    set(DUALSENSE_ADDR $ENV{DUALSENSE_ADDR})
    message("Using DUALSENSE_ADDR from environment ('${DUALSENSE_ADDR}')")
endif()

project(${NAME} C CXX ASM)

add_executable(${NAME}
        main.cpp
        )

target_compile_definitions(${NAME} PRIVATE
        DUALSENSE_ADDR=\"${DUALSENSE_ADDR}\"
        CYW43_HOST_NAME=\"pico\"
        CYW43_TASK_STACK_SIZE=1024
        CYW43_LWIP=0
        NO_SYS=0
        )

pico_enable_stdio_usb(${NAME} 1)
pico_enable_stdio_uart(${NAME} 0)

pico_add_extra_outputs(${NAME})

include_directories(${NAME} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/config
        ${CMAKE_CURRENT_LIST_DIR}/../../config
        ${CMAKE_CURRENT_LIST_DIR}/../../utils/DualSense
        )

target_link_libraries(${NAME}
        pico_stdlib
        pico_multicore
        pico_btstack_ble
        pico_btstack_classic
        pico_btstack_cyw43
        pico_cyw43_arch_sys_freertos
        FreeRTOS-Kernel
        FreeRTOS-Kernel-Heap4
        )

add_deploy_target(${NAME} "${CMAKE_CURRENT_BINARY_DIR}/${NAME}.uf2")
