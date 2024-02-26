if (DEFINED ENV{FREERTOS_KERNEL_PATH} AND (NOT FREERTOS_KERNEL_PATH))
    set(FREERTOS_KERNEL_PATH $ENV{FREERTOS_KERNEL_PATH})
    message("Using FREERTOS_KERNEL_PATH from environment ('${FREERTOS_KERNEL_PATH}')")
endif ()

if (NOT FREERTOS_KERNEL_PATH)
    set(FREERTOS_KERNEL_PATH ${PICO_SDK_PATH}/../FreeRTOS-Kernel)
endif ()

if (NOT EXISTS ${FREERTOS_KERNEL_PATH})
    message(FATAL_ERROR "Directory '${FREERTOS_KERNEL_PATH}' not found")
endif ()

if (NOT EXISTS ${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040)
    message(FATAL_ERROR "Directory '${PICO_LIB_DRIVERS_PATH}/portable/ThirdParty/GCC/RP2040' not found")
endif ()

include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)
