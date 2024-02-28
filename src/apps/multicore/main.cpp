#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pico/time.h"
#include "FreeRTOS.h"
#include "task.h"
#include <math.h>
#include "task-blink-sos.h"

void taskPrintCore(void *pvParams) {
    char *task_name = pcTaskGetName(NULL);

    const uint FULL_TICK_LOAD = 0xFF;

    while (true) {
        printf("%.03f: %s @ core %d, begin\n",
               time_us_32() / 1000000.0f,
               task_name,
               get_core_num()
       );

        uint load = rand() & (1024 - 1);
        uint iterations = (configCPU_CLOCK_HZ / configTICK_RATE_HZ / 16) * load;

        for (volatile int i = 0; i < iterations; ++i) {
            // nop (16 - 4) times
            pico_default_asm_volatile("nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;");
        }

        printf("%.03f: %s @ core %d, complete %d iterations (%d ticks)\n",
               time_us_32() / 1000000.0f,
               task_name,
               get_core_num(),
               iterations,
               load
        );

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}


int main() {
    bi_decl(bi_program_description("This is a multicore test binary."));
    bi_decl(bi_1pin_with_name(PICO_DEFAULT_LED_PIN, "On-board LED"));

    stdio_init_all();

    uint ledPin = PICO_DEFAULT_LED_PIN;
    xTaskCreate(taskBlinkSos, "blink_sos", configMINIMAL_STACK_SIZE, (void *) &ledPin, tskIDLE_PRIORITY, NULL);

    // Define the task handles
    TaskHandle_t handleA;
    TaskHandle_t handleB;

    xTaskCreate(taskPrintCore, "A", configMINIMAL_STACK_SIZE * 4, NULL, tskIDLE_PRIORITY, &handleA);
    xTaskCreate(taskPrintCore, "B", configMINIMAL_STACK_SIZE * 4, NULL, tskIDLE_PRIORITY, &handleB);
    xTaskCreate(taskPrintCore, "C", configMINIMAL_STACK_SIZE * 4, NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(taskPrintCore, "D", configMINIMAL_STACK_SIZE * 4, NULL, tskIDLE_PRIORITY, NULL);

#if configNUMBER_OF_CORES > 1
    vTaskCoreAffinitySet(handleA, 1 << 0);
    vTaskCoreAffinitySet(handleB, 1 << 1);
#endif

    vTaskStartScheduler();

    return 0;
}
