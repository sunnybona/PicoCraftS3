// Board configuration: single source of truth for all hardware pins.
// Matches the verified desktop lcd project (ST7789V) and the
// esp32s3-devkit-c gamepad wiring.
#pragma once

#include "driver/gpio.h"
#include "driver/spi_common.h"

// ---------------------------------------------------------------------------
// LCD (ST7789V over SPI2)
// ---------------------------------------------------------------------------
#define LCD_HOST            SPI2_HOST

#define PIN_NUM_MISO        17
#define PIN_NUM_MOSI        12
#define PIN_NUM_CLK         48
#define PIN_NUM_CS          14
// These three are passed directly to the GPIO API (gpio_num_t), so cast them;
// the enum still implicitly converts back to int where needed (e.g. shifts).
#define PIN_NUM_DC          ((gpio_num_t)47)
#define PIN_NUM_RST         ((gpio_num_t)3)
#define PIN_NUM_BCKL        ((gpio_num_t)39)

#define LCD_BK_LIGHT_ON     1

// Physical panel geometry
#define LCD_PHY_WIDTH       320
#define LCD_PHY_HEIGHT      240

// Logical PicoCraft frame geometry. 160x120 (4:3) matches the physical panel
// aspect ratio, so a uniform 2x scale fills the whole 320x240 screen with no
// side borders and no distortion.
#define SCREEN_W            160
#define SCREEN_H            120

// Logical frame is scaled 2x: 160*2=320 wide, 120*2=240 tall, full panel.
#define LCD_SCALE           2
#define LCD_BORDER_X        ((LCD_PHY_WIDTH - SCREEN_W * LCD_SCALE) / 2)
#define LCD_BORDER_Y        ((LCD_PHY_HEIGHT - SCREEN_H * LCD_SCALE) / 2)

// Number of physical rows pushed per DMA batch (must divide LCD_PHY_HEIGHT).
#define PARALLEL_LINES      16

// SPI clock. The ST7789V is rated far lower on paper, but the user asked to
// probe the actual panel limit. 80 MHz is the SPI2 source ceiling; if the
// picture tears/garbles, step back to 40 MHz or 20 MHz.
#define LCD_SPI_CLOCK_HZ    (80 * 1000 * 1000)

// ---------------------------------------------------------------------------
// Gamepad: all GPIO pins, pull-up, active LOW.
// picosystem logical button -> physical GPIO.
// ---------------------------------------------------------------------------
// Logical button indices (redefine the picosystem enum locally; the RP GPIO
// numbers are meaningless on the S3).
enum BtnId : int {
    BTN_UP = 0,
    BTN_DOWN,
    BTN_LEFT,
    BTN_RIGHT,
    BTN_A,
    BTN_B,
    BTN_X,      // jump
    BTN_Y,      // place block
    BTN_COUNT
};

// Logical button -> physical GPIO mapping.
static const gpio_num_t BTN_GPIO[BTN_COUNT] = {
    [BTN_UP]    = GPIO_NUM_7,
    [BTN_DOWN]  = GPIO_NUM_46,
    [BTN_LEFT]  = GPIO_NUM_45,
    [BTN_RIGHT] = GPIO_NUM_6,
    [BTN_A]     = GPIO_NUM_15,
    [BTN_B]     = GPIO_NUM_5,
    [BTN_X]     = GPIO_NUM_17,   // START -> jump
    [BTN_Y]     = GPIO_NUM_8,    // OPTION -> place block
};
