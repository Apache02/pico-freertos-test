#include <FreeRTOS.h>
#include <task.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "bootsel.h"


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

int main() {
    bi_decl(bi_program_description("This is a hello-world binary."));
    bi_decl(bi_1pin_with_name(PICO_DEFAULT_LED_PIN, "On-board LED"));

    stdio_init_all();

    uint ledPin = PICO_DEFAULT_LED_PIN;
    xTaskCreate(taskBlinkSos,
                "led_sos",
                128,
                (void *) &ledPin,
                tskIDLE_PRIORITY,
                NULL
    );

    initBootselTask();

    vTaskStartScheduler();

    return 0;
}
