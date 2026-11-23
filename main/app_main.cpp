// ESP32-S3 entry point and single-threaded game loop.
//
// The original PicoCraft runs on the RP2040/RP2350 with a second core doing
// the rasterization. On the S3 everything is driven from this one task:
// poll inputs -> update logic -> rasterize (inside draw/render_sync) -> push
// the frame to the ST7789V panel. The loop is paced to the original
// picosystem target of roughly 40 FPS, with an elastic deadline so a slow
// frame never accumulates extra catch-up delays.
#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "picosystem.hpp"
#include "input.hpp"
#include "st7789_display.hpp"

using namespace picosystem;

static const char *TAG = "picocraft";

// Target frame period: 40 FPS (25 ms), matching the original picosystem loop.
static constexpr uint32_t FRAME_PERIOD_MS = 25;

extern "C" void app_main() {
    // 1) NVS flash (used by the save store)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2) Peripherals
    input_init();
    display_init();

    // 3) Game initialization (world generation, save load, camera, physics)
    init();

    uint32_t tick = 0;
    while (true) {
        const int64_t start_us = esp_timer_get_time();

        // Reset per-frame drawing state, mirroring the picosystem loop.
        // (The shim uses fixed COPY semantics, so there is no alpha/blend.)
        pen();
        clip();
        target();
        camera();
        cursor();

        // Sample the gamepad, then run one logical + render step.
        input_poll();
        update(tick);
        draw(tick);

        // Push the finished 120x120 GBAR frame to the panel.
        display_present(SCREEN->data);

        ++tick;

        // Elastic pacing: sleep only for the remainder of the frame budget.
        const uint32_t elapsed_ms =
            (uint32_t)((esp_timer_get_time() - start_us) / 1000);
        if (elapsed_ms < FRAME_PERIOD_MS) {
            vTaskDelay(pdMS_TO_TICKS(FRAME_PERIOD_MS - elapsed_ms));
        } else {
            // Overran the budget: just yield so the idle task / watchdog run.
            vTaskDelay(1);
        }
    }
}
