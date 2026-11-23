// ST7789V display driver for ESP32-S3 over SPI2, with PSRAM DMA buffers.
// Pushes the 120x120 GBAR logical frame to the 320x240 physical panel:
// GBAR -> RGB565 conversion, 2x nearest-neighbour scale, centered.
#pragma once

#include <cstdint>

// Initialize the SPI bus, the ST7789V panel and the backlight.
void display_init();

// Push one 120x120 GBAR frame to the panel.
void display_present(const uint16_t *gbar_src);
