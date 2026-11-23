#include "input.hpp"

#include "board_config.h"
#include "driver/gpio.h"

static bool s_level[BTN_COUNT];
static bool s_prev[BTN_COUNT];

void input_init() {
    for (int i = 0; i < BTN_COUNT; i++) {
        gpio_num_t g = BTN_GPIO[i];
        gpio_config_t io = {};
        io.pin_bit_mask = 1ULL << g;
        io.mode = GPIO_MODE_INPUT;
        io.pull_up_en = GPIO_PULLUP_ENABLE;
        io.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io.intr_type = GPIO_INTR_DISABLE;
        gpio_config(&io);
        s_level[i] = false;
        s_prev[i] = false;
    }
}

void input_poll() {
    for (int i = 0; i < BTN_COUNT; i++) {
        s_prev[i] = s_level[i];
        // Active LOW: physical level 0 means pressed.
        s_level[i] = (gpio_get_level(BTN_GPIO[i]) == 0);
    }
}

bool input_level(int idx) {
    return (idx >= 0 && idx < BTN_COUNT) ? s_level[idx] : false;
}

bool input_pressed(int idx) {
    return (idx >= 0 && idx < BTN_COUNT) ? (s_level[idx] && !s_prev[idx]) : false;
}

bool input_released(int idx) {
    return (idx >= 0 && idx < BTN_COUNT) ? (!s_level[idx] && s_prev[idx]) : false;
}
