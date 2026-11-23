// GPIO gamepad driver: all pull-up pins, active LOW, with edge detection.
#pragma once

#include <cstdint>

// Configure all gamepad GPIO pins.
void input_init();

// Sample all buttons; call once at the start of each logical frame.
void input_poll();

// True while the button is held down.
bool input_level(int idx);

// True on the frame the button goes down (press edge).
bool input_pressed(int idx);

// True on the frame the button goes up (release edge).
bool input_released(int idx);
