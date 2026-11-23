#include "st7789_display.hpp"

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_attr.h"

#include "board_config.h"

// ---------------------------------------------------------------------------
// Panel initialization commands (ST7789V), identical to the verified project
// ---------------------------------------------------------------------------
typedef struct {
    uint8_t cmd;
    uint8_t data[16];
    uint8_t databytes; // bit 7 = delay after set; 0xFF = end
} lcd_init_cmd_t;

DRAM_ATTR static const lcd_init_cmd_t st_init_cmds[] = {
    {0x36, {0xA0}, 1}, 
 //   {0x21, {0x00}, 0},                                 // MADCTL: landscape
    {0x3A, {0x55}, 1},                                  // 16 bits/pixel
    {0xB2, {0x0c, 0x0c, 0x00, 0x33, 0x33}, 5},
    {0xB7, {0x45}, 1},
    {0xBB, {0x2B}, 1},
    {0xC0, {0x2C}, 1},
    {0xC2, {0x01, 0xff}, 2},
    {0xC3, {0x11}, 1},
    {0xC4, {0x20}, 1},
    {0xC6, {0x0f}, 1},
    {0xD0, {0xA4, 0xA1}, 2},
    {0xE0, {0xD0, 0x00, 0x05, 0x0E, 0x15, 0x0D, 0x37, 0x43, 0x47, 0x09, 0x15, 0x12, 0x16, 0x19}, 14},
    {0xE1, {0xD0, 0x00, 0x05, 0x0D, 0x0C, 0x06, 0x2D, 0x44, 0x40, 0x0E, 0x1C, 0x18, 0x16, 0x19}, 14},
    {0x11, {0}, 0x80},                                  // sleep out
    {0x29, {0}, 0x80},                                  // display on
    {0, {0}, 0xff}
};

static spi_device_handle_t s_spi;

// Persistent DMA line buffers (two ping-pong batches), allocated once in
// display_init and reused every frame. Each holds PARALLEL_LINES physical rows.
static uint16_t *s_lines[2];
static const uint32_t LINE_BYTES =
    LCD_PHY_WIDTH * PARALLEL_LINES * sizeof(uint16_t);

// Converted 120x120 RGB565 logical frame (byte-swapped, ready for the panel).
// The GBAR->RGB565 conversion runs once per frame into this buffer; the
// 2x scaling in fill_batch then only copies from it. Lives in PSRAM.
static uint16_t *s_logical;
static const uint32_t LOGICAL_BYTES =
    SCREEN_W * SCREEN_H * sizeof(uint16_t);

static void lcd_cmd(const uint8_t cmd) {
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.tx_buffer = &cmd;
    t.user = (void *)0;
    esp_err_t ret = spi_device_polling_transmit(s_spi, &t);
    assert(ret == ESP_OK);
}

static void lcd_data(const uint8_t *data, int len) {
    if (len == 0) return;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = len * 8;
    t.tx_buffer = data;
    t.user = (void *)1;
    esp_err_t ret = spi_device_polling_transmit(s_spi, &t);
    assert(ret == ESP_OK);
}

// IRQ-context callback: set D/C according to the transaction user field.
// Runs in interrupt context, so it must live in IRAM.
static IRAM_ATTR void lcd_pre_transfer_cb(spi_transaction_t *t) {
    int dc = (int)t->user;
    gpio_set_level(PIN_NUM_DC, dc);
}

static void lcd_panel_init() {
    // Non-SPI GPIOs
    gpio_config_t io = {};
    io.pin_bit_mask = ((1ULL << PIN_NUM_DC) | (1ULL << PIN_NUM_RST) | (1ULL << PIN_NUM_BCKL));
    io.mode = GPIO_MODE_OUTPUT;
    io.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io);

    // Hardware reset
    gpio_set_level(PIN_NUM_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_NUM_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    int cmd = 0;
    while (st_init_cmds[cmd].databytes != 0xff) {
        lcd_cmd(st_init_cmds[cmd].cmd);
        lcd_data(st_init_cmds[cmd].data, st_init_cmds[cmd].databytes & 0x1F);
        if (st_init_cmds[cmd].databytes & 0x80) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        cmd++;
    }

    gpio_set_level(PIN_NUM_BCKL, LCD_BK_LIGHT_ON);
}

// ---------------------------------------------------------------------------
// Queue one batch of physical rows (6 transactions), same as the verified code
// ---------------------------------------------------------------------------
static void send_lines(int ypos, uint16_t *linedata) {
    static spi_transaction_t trans[6];
    for (int x = 0; x < 6; x++) {
        memset(&trans[x], 0, sizeof(spi_transaction_t));
        if ((x & 1) == 0) {
            trans[x].length = 8;
            trans[x].user = (void *)0;
        } else {
            trans[x].length = 8 * 4;
            trans[x].user = (void *)1;
        }
        trans[x].flags = SPI_TRANS_USE_TXDATA;
    }
    trans[0].tx_data[0] = 0x2A;                             // column address
    trans[1].tx_data[0] = 0;
    trans[1].tx_data[1] = 0;
    trans[1].tx_data[2] = (LCD_PHY_WIDTH - 1) >> 8;
    trans[1].tx_data[3] = (LCD_PHY_WIDTH - 1) & 0xff;
    trans[2].tx_data[0] = 0x2B;                             // page address
    trans[3].tx_data[0] = ypos >> 8;
    trans[3].tx_data[1] = ypos & 0xff;
    trans[3].tx_data[2] = (ypos + PARALLEL_LINES - 1) >> 8;
    trans[3].tx_data[3] = (ypos + PARALLEL_LINES - 1) & 0xff;
    trans[4].tx_data[0] = 0x2C;                             // memory write
    trans[5].tx_buffer = linedata;
    trans[5].length = LCD_PHY_WIDTH * 2 * 8 * PARALLEL_LINES;
    trans[5].flags = SPI_TRANS_DMA_USE_PSRAM;               // PSRAM DMA

    for (int x = 0; x < 6; x++) {
        esp_err_t ret = spi_device_queue_trans(s_spi, &trans[x], portMAX_DELAY);
        assert(ret == ESP_OK);
    }
}

static void send_finish() {
    spi_transaction_t *rtrans;
    for (int x = 0; x < 6; x++) {
        esp_err_t ret = spi_device_get_trans_result(s_spi, &rtrans, portMAX_DELAY);
        assert(ret == ESP_OK);
    }
}

// ---------------------------------------------------------------------------
// GBAR -> RGB565
// ---------------------------------------------------------------------------
static inline IRAM_ATTR uint16_t gbar_to_rgb565(uint16_t c) {
    uint8_t r = c & 0xF;
    uint8_t b = (c >> 8) & 0xF;
    uint8_t g = (c >> 12) & 0xF;
    // Expand 4-bit channels, copying high bits into the low bits so the full
    // range (0..15) maps to the full range (0..31 / 0..63).
    uint8_t r5 = (uint8_t)((r << 1) | (r >> 3));
    uint8_t g6 = (uint8_t)((g << 2) | (g >> 2));
    uint8_t b5 = (uint8_t)((b << 1) | (b >> 3));
    uint16_t rgb565 = (uint16_t)((r5 << 11) | (g6 << 5) | b5);

    // ST7789 expects the high byte first (D15 on the first SPI clock), but the
    // little-endian DMA sends the low byte first. Swap the two bytes so the
    // panel reassembles the intended value. The 0x0000 black borders are
    // unaffected by a byte swap.
    return (uint16_t)((rgb565 >> 8) | (rgb565 << 8));
}

// Fill one DMA batch (16 physical rows) from the pre-converted 120x120 RGB565
// logical frame. This is pure memory work: memset the side borders and duplicate
// each logical pixel 2x horizontally; two physical rows share one logical row.
static IRAM_ATTR void fill_batch(uint16_t *out, int ypos, const uint16_t *logical) {
    for (int dy = 0; dy < PARALLEL_LINES; dy++) {
        const int py = ypos + dy;
        const int ly = py >> 1;                 // /LCD_SCALE; BORDER_Y == 0
        uint16_t *row = out + dy * LCD_PHY_WIDTH;

        memset(row, 0, LCD_BORDER_X * sizeof(uint16_t));                 // left border

        uint16_t *mid = row + LCD_BORDER_X;
        const uint16_t *srcrow = logical + ly * SCREEN_W;
        for (int lx = 0; lx < SCREEN_W; lx++) {
            const uint16_t v = srcrow[lx];
            mid[lx * 2] = v;                   // 2x horizontal nearest scale
            mid[lx * 2 + 1] = v;
        }

        memset(row + LCD_BORDER_X + SCREEN_W * LCD_SCALE, 0,
               LCD_BORDER_X * sizeof(uint16_t));                        // right border
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void display_init() {
    spi_bus_config_t buscfg = {};
    buscfg.miso_io_num = PIN_NUM_MISO;
    buscfg.mosi_io_num = PIN_NUM_MOSI;
    buscfg.sclk_io_num = PIN_NUM_CLK;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = PARALLEL_LINES * LCD_PHY_WIDTH * 2 + 8;

    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = LCD_SPI_CLOCK_HZ;
    devcfg.mode = 0;
    devcfg.spics_io_num = PIN_NUM_CS;
    devcfg.queue_size = 7;
    devcfg.pre_cb = lcd_pre_transfer_cb;

    ESP_ERROR_CHECK(spi_bus_add_device(LCD_HOST, &devcfg, &s_spi));

    lcd_panel_init();

    // Allocate the two persistent DMA line buffers from PSRAM.
    const uint32_t mem_cap = MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA;
    for (int i = 0; i < 2; i++) {
        s_lines[i] = (uint16_t *)spi_bus_dma_memory_alloc(
            LCD_HOST, LINE_BYTES, mem_cap);
        assert(s_lines[i] != NULL);
    }

    // Converted logical frame: plain PSRAM is enough (CPU access only, no DMA).
    s_logical = (uint16_t *)heap_caps_malloc(LOGICAL_BYTES, MALLOC_CAP_SPIRAM);
    assert(s_logical != NULL);
}

void display_present(const uint16_t *gbar_src) {
    // Convert the 120x120 GBAR frame once (14400 conversions) rather than
    // re-running the conversion for all 76800 physical pixels.
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
        s_logical[i] = gbar_to_rgb565(gbar_src[i]);
    }

    int sending = -1;
    int calc = 0;
    for (int y = 0; y < LCD_PHY_HEIGHT; y += PARALLEL_LINES) {
        fill_batch(s_lines[calc], y, s_logical);
        if (sending != -1) send_finish();
        sending = calc;
        calc = (calc == 1) ? 0 : 1;
        send_lines(y, s_lines[sending]);
    }
    send_finish();   // wait for the final batch
}
