#include "task-bootsel-pool.h"
#include <FreeRTOS.h>
#include <task.h>
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/structs/ioqspi.h"
#include "pico/bootrom.h"
#include <stdio.h>


#define STARTUP_DELAY_MS 2000
#define HOLD_BOOT_DELAY_MS 3000


bool __no_inline_not_in_flash_func(get_bootsel_button)() {
    const uint CS_PIN_INDEX = 1;

    // Must disable interrupts, as interrupt handlers may be in flash, and we
    // are about to temporarily disable flash access!
    portENTER_CRITICAL();

    // Set chip select to Hi-Z
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    // Note we can't call into any sleep functions in flash right now
    for (volatile int i = 0; i < 100; ++i);

    // The HI GPIO registers in SIO can observe and control the 6 QSPI pins.
    // Note the button pulls the pin *low* when pressed.
    bool button_state = !(sio_hw->gpio_hi_in & (1u << CS_PIN_INDEX));

    // Need to restore the state of chip select, else we are going to have a
    // bad time when we return to code in flash!
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    portEXIT_CRITICAL();

    return button_state;
}

void taskBootselPool(void *pvParams) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(STARTUP_DELAY_MS));

    while (true) {
        // got fresh xLastWakeTime value here
        if (get_bootsel_button()) {
            TickType_t xBootAfterTime = xLastWakeTime + pdMS_TO_TICKS(HOLD_BOOT_DELAY_MS);
            while (true) {
                vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(20));
                if (!get_bootsel_button()) {
                    break;
                }
                if (xLastWakeTime > xBootAfterTime) {
                    // goto bootloader
                    reset_usb_boot(0, 0);
                }
            }
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));
    }
}

void initTaskBootselPool() {
    TaskHandle_t handle;

    xTaskCreate(taskBootselPool,
                "bootsel-pool",
                1024,
                NULL,
                tskIDLE_PRIORITY,
                &handle
    );

#if (configNUM_CORES > 1 && configUSE_CORE_AFFINITY == 1)
    vTaskCoreAffinitySet(handle, 1 << 0);
#endif
}
