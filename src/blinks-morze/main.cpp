#include <FreeRTOS.h>
#include <task.h>
#include <map>
#include "pico/stdlib.h"
#include "pico/binary_info.h"

static const std::map<char, const char *> morzeAlphabet = {
        {'a', ".-"},
        {'b', "-..."},
        {'w', ".--"},
        {'g', "--."},
        {'d', "-.."},
        {'e', "."},
        {'v', "...-"},
        {'z', "--.."},
        {'i', ".."},
        {'j', ".---"},
        {'k', "-.-"},
        {'l', ".-.."},
        {'m', "--"},
        {'n', "-."},
        {'o', "---"},
        {'p', ".--."},
        {'r', ".-."},
        {'s', "..."},
        {'t', "-"},
        {'u', "..-"},
        {'f', "..-."},
        {'h', "...."},
        {'c', "-.-."},
        {'q', "--.-"},
        {'y', "-.--"},
        {'x', "-..-"},
        {'1', ".----"},
        {'2', "..---"},
        {'3', "...--"},
        {'4', "....-"},
        {'5', "....."},
        {'6', "-...."},
        {'7', "--..."},
        {'8', "---.."},
        {'9', "----."},
        {'0', "-----"},
        {'.', "......"},
        {',', ".-.-.-"},
        {':', "---..."},
        {';', "-.-.-."},
        {'(', "-.--.-"},
        {')', "-.--.-"},
        {'"', ".-..-."},
        {'-', "-....-"},
        {'/', "-..-."},
        {'?', "..--.."},
        {'!', "--..--"},
        {' ', "-...-"},
        {'@', ".--.-."},
};

struct BlinkMorzeParams {
    uint gpio;
    uint interval;
    const char *message;
};

void taskMorzeMessage(void *pvParams) {
    if (!pvParams) {
        return;
    }

    const BlinkMorzeParams *pParams = static_cast<const BlinkMorzeParams *>(pvParams);
    gpio_init(pParams->gpio);
    gpio_set_dir(pParams->gpio, GPIO_OUT);

    const uint interval = pParams->interval;

    while (true) {
        const char *ptr = pParams->message;
        char c;
        while ((c = *ptr++) != '\0') {
            const char *sequence = morzeAlphabet.find(c)->second;
            char s;
            while ((s = *sequence++) != '\0') {
                gpio_put(pParams->gpio, 1);
                if (s == '.') {
                    vTaskDelay(pdMS_TO_TICKS(interval));
                } else if (s == '-') {
                    vTaskDelay(pdMS_TO_TICKS(interval * 3));
                }
                gpio_put(pParams->gpio, 0);
                vTaskDelay(pdMS_TO_TICKS(interval));
            }
        }

        // end of word
        vTaskDelay(pdMS_TO_TICKS(interval * 2));
    }
}

int main() {
    bi_decl(bi_program_description("This is a hello-world binary."));
//    bi_decl(bi_1pin_with_name(PICO_DEFAULT_LED_PIN, "On-board LED"));

    stdio_init_all();

    static const struct BlinkMorzeParams task1_hello_params = {
            0,
            200,
            "hello world"
    };

    static const struct BlinkMorzeParams task2_sos_params = {
            1,
            100,
            "sos"
    };

    static const struct BlinkMorzeParams task3_params = {
            2,
            150,
            "eee"
    };

    static const struct BlinkMorzeParams task4_params = {
            3,
            160,
            "vvv"
    };

    xTaskCreate(taskMorzeMessage,
                "led_hello",
                128,
                (void *) &task1_hello_params,
                tskIDLE_PRIORITY,
                NULL
    );

    xTaskCreate(taskMorzeMessage,
                "led_sos",
                128,
                (void *) &task2_sos_params,
                tskIDLE_PRIORITY,
                NULL
    );

    xTaskCreate(taskMorzeMessage,
                "led_3",
                128,
                (void *) &task3_params,
                tskIDLE_PRIORITY,
                NULL
    );

    xTaskCreate(taskMorzeMessage,
                "led_4",
                128,
                (void *) &task4_params,
                tskIDLE_PRIORITY,
                NULL
    );

    vTaskStartScheduler();

    return 0;
}
