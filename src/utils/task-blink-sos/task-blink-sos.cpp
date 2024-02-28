#include "task-blink-sos.h"
#include <FreeRTOS.h>
#include <task.h>
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/structs/ioqspi.h"
#include "pico/bootrom.h"
#include <stdio.h>


void taskBlinkSos(void *pvParams) {
    uint pin = *(static_cast<uint *>(pvParams));

    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);

    static const char *message = "...---...";
    static const uint interval = 150;

    while (true) {
        const char *ptr = message;
        char c;
        while ((c = *ptr++) != '\0') {
            gpio_put(pin, 1);
            vTaskDelay(pdMS_TO_TICKS(c == '-' ? (interval * 3) : interval));
            gpio_put(pin, 0);
            vTaskDelay(pdMS_TO_TICKS(interval));
        }

        // end of word
        vTaskDelay(pdMS_TO_TICKS(interval * 2));
    }
}
