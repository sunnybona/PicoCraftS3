// picosystem compatibility shim for ESP32-S3 (ESP-IDF).
// Provides the subset of the Pimoroni picosystem API used by PicoCraft.
// The logical framebuffer stays in the native 4-bit-per-channel GBAR layout
// (aaaarrrrggggbbbb); conversion to RGB565 happens in the display driver.
#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

// User lifecycle, implemented in main.cpp
void init();
void update(uint32_t tick);
void draw(uint32_t tick);

namespace picosystem {

  typedef uint16_t color_t;

  struct buffer_t {
    int32_t w, h;
    color_t *data;
    bool alloc;

    color_t *p(int32_t x, int32_t y) {
      return data + (x + y * w);
    }
    const color_t *p(int32_t x, int32_t y) const {
      return data + (x + y * w);
    }

    ~buffer_t();
  };

  // ---- drawing state ----
  extern color_t   _pen;
  extern buffer_t *SCREEN;
  extern buffer_t *_dt;
  extern int32_t   _cx, _cy, _cw, _ch;
  extern int32_t   _tx, _ty;
  extern int32_t   _camx, _camy;
  extern const uint8_t *_font;

  // Logical button indices. Redefined locally (the RP GPIO numbers are
  // meaningless on the S3); values match BtnId in board_config.h.
  enum button {
    UP    = 0,
    DOWN  = 1,
    LEFT  = 2,
    RIGHT = 3,
    A     = 4,
    B     = 5,
    X     = 6,
    Y     = 7
  };

  // ---- state ----
  void pen();
  void pen(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 15);
  void pen(color_t p);
  void target();
  void target(buffer_t *dt);
  void clip();
  void clip(int32_t x, int32_t y, uint32_t w, uint32_t h);
  void camera();
  void camera(int32_t x, int32_t y);
  void cursor();
  void cursor(int32_t x, int32_t y);

  // ---- primitives ----
  void clear();
  void pixel(int32_t x, int32_t y);
  void hline(int32_t x, int32_t y, int32_t l);
  void vline(int32_t x, int32_t y, int32_t l);
  void line(int32_t x1, int32_t y1, int32_t x2, int32_t y2);
  void rect(int32_t x, int32_t y, int32_t w, int32_t h);
  void frect(int32_t x, int32_t y, int32_t w, int32_t h);

  // ---- text ----
  void text(const std::string_view &t, int32_t x, int32_t y, int32_t wrap = -1);
  void text(const std::string_view &t, int32_t wrap = -1);

  // ---- utility ----
  std::string str(int32_t v);
  std::string str(uint32_t v);
  std::string str(std::size_t v);
  std::string str(float v, uint8_t precision = 2);
  color_t rgb(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 15);
  buffer_t *buffer(uint32_t w, uint32_t h, void *data = nullptr);
  uint32_t time();
  uint32_t time_us();
  void sleep(uint32_t d);

  // ---- input ----
  bool button(uint32_t b);
  bool pressed(uint32_t b);
  bool released(uint32_t b);

}
