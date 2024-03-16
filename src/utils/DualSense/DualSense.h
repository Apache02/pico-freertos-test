#ifndef PICO_FREERTOS_TEST_DUALSENSE_H
#define PICO_FREERTOS_TEST_DUALSENSE_H

enum DualSense_DPAD_DIRECTION {
    DPAD_UP = 0,
    DPAD_UP_RIGHT,
    DPAD_RIGHT,
    DPAD_BOTTOM_RIGHT,
    DPAD_BOTTOM,
    DPAD_BOTTOM_LEFT,
    DPAD_LEFT,
    DPAD_UP_LEFT,
    DPAD_NONE,
};

struct DualSense_State_t {

    struct {
        uint8_t x: 8;
        uint8_t y: 8;
    } left_stick;
    struct {
        uint8_t x: 8;
        uint8_t y: 8;
    } right_stick;
    struct {
        uint8_t dpad: 4;
        uint8_t square: 1;
        uint8_t cross: 1;
        uint8_t circle: 1;
        uint8_t triangle: 1;
        uint8_t l1: 1;
        uint8_t r1: 1;
        uint8_t l2: 1;
        uint8_t r2: 1;
        uint8_t create: 1;
        uint8_t options: 1;
        uint8_t l3: 1;
        uint8_t r3: 1;
    } buttons;
    uint8_t ps_button: 1;
    uint8_t unknown: 7;
    uint8_t l2_stick: 8;
    uint8_t r2_stick: 8;
} __packed;

#endif // PICO_FREERTOS_TEST_DUALSENSE_H
