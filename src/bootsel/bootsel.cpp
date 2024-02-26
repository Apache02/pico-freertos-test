#include "bootsel.h"
#include <FreeRTOS.h>
#include <task.h>
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/structs/ioqspi.h"
#include "pico/time.h"
#include "pico/bootrom.h"
#include <stdio.h>


#define ENTER_BOOT_DELAY_US 3000000


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
    for (volatile int i = 0; i < 1000; ++i);

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

void taskBootselProbe(void *pvParams) {
    bool pressed = false;
    uint64_t bootAfter = 0;

    vTaskDelay(pdMS_TO_TICKS(2000));

    while (true) {
        if (get_bootsel_button()) {
            auto now = time_us_64();
            if (!pressed) {
                bootAfter = now + ENTER_BOOT_DELAY_US;
                pressed = true;
            } else if (now > bootAfter) {
                reset_usb_boot(0, 0);
            }
        } else {
            pressed = false;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void initBootselTask() {
    xTaskCreate(taskBootselProbe,
                "bootsel",
                1024,
                NULL,
                tskIDLE_PRIORITY,
                NULL
    );
}