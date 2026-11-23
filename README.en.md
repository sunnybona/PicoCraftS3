# PicoCraft (ESP32-S3 Port)

English | [简体中文](README.md)

A first-person voxel sandbox game (Minecraft-style). This repository ports PicoCraft, originally running on the
**Raspberry Pi RP2040 / RP2350 with the Pimoroni PicoSystem**, to the **Espressif ESP32-S3 (ESP-IDF v6.1.1)**.
The game drives an ST7789V LCD over SPI, uses the on-chip PSRAM for frame and DMA buffers, and persists block
edits in NVS.

> **Stage note: no audio in stage 1.** Display, game logic, buttons and saving all run on real hardware. Audio is
> deferred until the I2S hardware is validated; it is currently stubbed (`voxel/audio_stub.h`) and all sound calls are no-ops.

---

## Contents

- [Features](#features)
- [Hardware and Pinout](#hardware-and-pinout)
- [Project Layout](#project-layout)
- [Controls](#controls)
- [Build and Flash](#build-and-flash)
- [Display and Color](#display-and-color)
- [Save System](#save-system)
- [Resource Usage](#resource-usage)
- [Porting Notes](#porting-notes)
- [Status and Roadmap](#status-and-roadmap)
- [Credits and License](#credits-and-license)

---

## Features

### Voxel World

- **Chunk-based streaming world**: 16×16×16 chunks, horizontally infinite (bounded by integer range), fixed height of 16.
- A 3×3 grid of 9 chunks around the player stays resident; distant chunks unload and at most one chunk is generated per frame to avoid hitches.
- **Procedural terrain**: deterministic noise creates rolling terrain; blocks below sea level (y=4) fill with water.
- **15 block types**: air, grass, dirt, stone, log, leaves, sand, plank, water, cobble, brick, glass, bedrock, snow, gold.
- Sparse dig/place edits (up to 256) persist in **NVS** and restore after reboot.
- A flat spawn platform is forced, with a house, animals and an NPC generated deterministically on it.

### First-Person and Rendering

- Custom software 3D pipeline: triangle transform, near-plane clipping, back-face culling, rasterization and fixed directional lighting.
- Voxel visible-face extraction (exposed faces only), built within a view radius of 11 blocks; entities share the same triangle pipeline.
- **DDA voxel ray** selects the target block with a 3D wireframe highlight; dig/place reflects in the mesh on the same frame.
- Full AABB physics: gravity, collision, manual jumping and landing detection.
- Procedural 16×16 block textures generated at runtime (no Flash cost); grass tops carry patch-distributed flowers.
- A first-person pickaxe with a swing animation; HUD includes a crosshair and a hotbar swatch with index.
- **Distant skyline**: mountain silhouettes and water drawn in the sky region without extra chunks or triangles.

### Entities

- Animals **cow / sheep / chicken / duck** plus one **NPC**; a fixed-size pool streams entities in/out around the player with simple wander AI.

---

## Hardware and Pinout

Target: an ESP32-S3 (e.g. esp32s3-devkit-c) with an ST7789V 320×240 SPI LCD and GPIO-wired buttons.

### LCD (SPI2_HOST, mode 0)

| Function | GPIO | Notes |
| --- | --- | --- |
| MISO | 17 | configured though the app is write-only |
| MOSI | 12 | data line |
| SCLK | 48 | serial clock |
| CS | 14 | chip select |
| DC | 47 | data/command select |
| RST | 3 | hardware reset |
| Backlight BCKL | 39 | HIGH = on |

### Buttons (internal pull-up, active LOW)

| Logical button | Role | GPIO |
| --- | --- | --- |
| UP | move forward | 7 |
| RIGHT | turn right | 6 |
| DOWN | move backward | 46 |
| LEFT | turn left | 45 |
| A | dig block | 15 |
| B | modifier (hotbar / pitch) | 5 |
| X | jump | 17 |
| Y | place block | 8 |

> All pins live in `main/board_config.h`; rewiring only requires editing that one file.

---

## Project Layout

```
PicoCraftS3/
├── CMakeLists.txt          # top-level ESP-IDF project (MINIMAL_BUILD, project picocraft_s3)
├── sdkconfig.defaults      # defaults: PSRAM/Flash/CPU/main task stack
├── sdkconfig               # active config (generated, updated on changes)
│
└── main/
    ├── CMakeLists.txt      # component registration: sources, includes, deps, macros
    ├── board_config.h      # pins, screen geometry, button map (single source of truth)
    ├── app_main.cpp        # ESP32-S3 entry and single-threaded loop (elastic pacing)
    ├── main.cpp            # game body: world/input/interaction/physics/HUD (RP hooks conditionally compiled)
    ├── chunk_data.h        # texture list, etc.
    │
    ├── picosystem.hpp      # PicoSystem compatibility API
    ├── picosystem_compat.cpp  # platform primitives/text/state/framebuffer
    ├── input.hpp / input.cpp  # GPIO button driver (debounce, edge detection)
    ├── st7789_display.hpp / st7789_display.cpp  # ST7789 driver, GBAR->RGB565, scaling, DMA
    │
    ├── engine/             # software 3D engine
    │   ├── render_camera.cpp      # camera and projection matrices (aspect compensation)
    │   ├── render_triangle.cpp    # transform/clip/cull/enqueue
    │   ├── render_clipping.cpp    # near-plane clipping
    │   ├── render_culling.cpp     # frustum culling
    │   ├── render_rasterize.cpp   # rasterization (includes shader_wireframe.cpp)
    │   ├── render_lighting.cpp    # lighting (empty here; colors are baked)
    │   ├── render_globals.h / render_math.h / chunk_globals.h
    │   └── shader_wireframe.cpp   # line/pixel shaders (included, not compiled separately)
    │
    └── voxel/              # game logic
        ├── voxel_world.*          # chunk streaming world, blocks, sparse edits
        ├── voxel_mesh.*           # visible-face extraction
        ├── voxel_ray.*            # DDA ray
        ├── player_physics.*       # AABB physics
        ├── save_store.*           # NVS persistence
        ├── block_textures.*       # procedural textures
        ├── structures.*           # house structure
        ├── entities.*             # animals/NPC
        ├── terrain_skyline.*      # distant skyline
        ├── spawn_platform.h        # spawn platform
        └── audio_stub.h           # audio stub (stage 1)
```

---

## Controls

| Action | Button |
| --- | --- |
| Turn left / right | LEFT / RIGHT (hold to turn continuously) |
| Move forward / backward | UP / DOWN |
| Jump | X (on press edge) |
| Dig target block | A (bedrock is unbreakable) |
| Place held block | Y (into the empty block ahead of the target) |
| Fine pitch | Hold **B** + UP / DOWN |
| Cycle hotbar block | Long-press **B** alone (no direction held) |
| Reset pitch | Quick tap on **B** |

Hotbar order (11 items): dirt, stone, cobble, log, plank, sand, brick, glass, snow, gold, leaves.

---

## Build and Flash

### Requirements

- **ESP-IDF v6.1.1** (target chip esp32s3) with its bundled xtensa-esp32s3 toolchain, CMake, Ninja and Python.
- Every source and config is in the repo; **no extra components or manual downloads are needed**.

### 1. Open an ESP-IDF terminal

Launch **"ESP-IDF 6.1 PowerShell"** or **"ESP-IDF 6.1 CMD"** from the Start Menu (bundled with the ESP-IDF installer);
it sets `IDF_PATH` and the toolchain automatically.

### 2. Build

In the ESP-IDF terminal:

```cmd
idf.py -C E:\PicoCraftS3 build
```

### 3. Flash and monitor

Connect the board and note the port (COM22 for the CH340 here):

```cmd
idf.py -C E:\PicoCraftS3 -p COM22 flash monitor
```

This builds, flashes and opens the serial monitor in one step; exit with `Ctrl + ]`. To flash without rebuilding, just run `idf.py -p COM22 flash`.

### Configuration

- `sdkconfig.defaults` sets: 16MB Flash (DIO/80MHz), octal PSRAM (80MHz), CPU 240MHz, main task stack 8192, FreeRTOS 1000Hz.
- The first build generates `build/` and `sdkconfig`; run `idf.py menuconfig` to change settings.
- For a clean rebuild: `idf.py fullclean` then `build`.

---

## Display and Color

- **Physical 320×240, logical 160×120**; a 2x nearest-neighbour uniform scale fills the whole screen with no borders and no distortion.
- The panel runs **RGB565** at 16 bits/pixel; the game internally uses the PicoSystem **GBAR** 4-bit-per-channel layout
  (`aaaarrrrggggbbbb`), and the driver extracts channels, expands 4→5/6 bits and converts formats in `gbar_to_rgb565()`.
- **Byte swap**: the ST7789 expects the high byte first while little-endian DMA sends the low byte first; the driver swaps
  each pixel's bytes. Without it, green disappears and brown turns pale blue.
- **Aspect compensation**: the projection matrix scales X by `160/120` so world cubes stay square on the non-square frame.
- The SPI pixel clock is currently **80MHz**; if long wiring causes artefacts, lower `LCD_SPI_CLOCK_HZ` in `board_config.h` to 40MHz or 20MHz.
- The two DMA line buffers (16 rows each) and the conversion buffer live in PSRAM, allocated once and reused per frame rather than alloc/free at runtime.

---

## Save System

- Dig/place edits are stored sparsely in `VoxelWorld::edits` (up to 256); the world itself is generated deterministically from the seed.
- About 90 frames after editing stops, `SaveStore::save()` writes the whole save image (16-byte header + records, 4352 bytes) as
  **one NVS blob** (namespace `picocraft`, key `save`).
- At boot, `SaveStore::load()` reads it back and replays edits after chunk generation. The record layout matches the RP original (magic `PCBL`, version 1).

---

## Resource Usage

Latest build (ESP-IDF v6.1.1, CPU 240MHz, SPI 80MHz):

| Item | Used | Free |
| --- | --- | --- |
| App firmware (Flash app partition) | ~229 KB (0x38280) | 1MB partition, 78% free |
| On-chip DIRAM | ~242 KB (71%) | ~99 KB |
| Hot-function IRAM | 16 KB (100%, system reserved) | — |

> Hot functions (rasterization, triangle transform, clipping, face extraction, color conversion and the SPI interrupt
> callback) are placed in on-chip SRAM with `IRAM_ATTR`, avoiding XIP-from-Flash waits.

---

## Porting Notes

See [PORTING.md](PORTING.md) for the full porting process, change list and troubleshooting record.

---

## Status and Roadmap

- Done: project skeleton, game-source adaptation, PicoSystem compatibility layer, ST7789 driver, GPIO buttons, NVS saves,
  single-threaded loop, color byte swap, full-screen display and IRAM hot functions; it builds, flashes and runs on hardware.
- Next (stage 2): after I2S hardware validation, port audio and the real-time synthesizer using the new `driver/i2s_std.h`, replacing the current stub.
- To keep confirming on hardware: long-run stability and all button/hotbar interactions.

---

## Credits and License

- The game and PicoSystem compatibility layer are based on [Pimoroni PicoSystem](https://github.com/pimoroni/picosystem)
  (**MIT License**, Copyright (c) 2021 Pimoroni Ltd).
- The software 3D pipeline draws on the Pico3D approach.

The project's own code is recommended to use the **MIT License**; PicoSystem-related parts remain under their MIT License.
