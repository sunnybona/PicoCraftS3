# PicoCraft 移植说明（RP2040/RP2350 → ESP32-S3）

本文记录把 PicoCraft 从 Pimoroni PicoSystem（RP2040/RP2350 裸机）移植到 ESP32-S3（ESP-IDF v6.1.1）的完整过程、
关键改动与排错记录，供后续维护与二次移植参考。

---

## 1. 移植目标与基准

- **目标平台**：ESP32-S3，ESP-IDF **v6.1.1**，C++（gnu++26，工程内按 C++17 特性使用），GCC 15.2。
- **显示驱动基准**：以桌面 `C:\Users\bobo\Desktop\lcd` 中已真机验证、PSRAM 已开启的 ST7789V 工程（spi_master 路线）为基准，
  其 SPI 配置、面板初始化命令、`send_lines` 事务结构直接沿用。
- **游戏源码**：来自 `E:\PicoCraft`，保留 engine/、voxel/、chunk_data.h 等核心，不改变游戏玩法与数据结构。
- **阶段边界**：第一阶段不做音频，以空桩占位；音频等 I2S 硬件测试后再做。

---

## 2. 平台架构差异

| 维度 | 原 PicoSystem（RP） | ESP32-S3 移植 |
| --- | --- | --- |
| 任务模型 | 双核：Core0 逻辑 + Core1 光栅化 | 单任务单线程：逻辑与光栅化同步执行 |
| 帧缓冲 | Core1 独立 framebuffer | 直接使用 SCREEN，无需独立帧缓冲 |
| 系统 API | pico-sdk / picosystem 库 | 自建 picosystem 兼容层 + ESP-IDF 驱动 |
| 内存 | 无 PSRAM，全部片上 SRAM | 八线 PSRAM 承载 DMA/转换缓冲，片上 SRAM 放热代码与状态 |
| 显示 | 120×120 方形，pixel double | 160×120（4:3），2x 放大铺满 320×240 |
| 存档 | 片上 Flash 保留区 | NVS blob，记录布局保持一致 |
| 构建 | CMake + pico-sdk | ESP-IDF 组件（idf_component_register） |

---

## 3. 主要改动点

### 3.1 工程骨架

- 顶层 `CMakeLists.txt`：`MINIMAL_BUILD ON` 精简组件，`project(picocraft_s3)`。
- `sdkconfig.defaults`：16MB Flash（DIO/80MHz）、八线 PSRAM（ESPPSRAM64/80MHz/USE_MALLOC）、CPU 240MHz、
  主任务栈 8192、FreeRTOS 1000Hz、尺寸优化。
- `main/CMakeLists.txt`：把全部游戏源码编入单个组件，include 目录扁平化（. / engine / voxel），
  依赖 `esp_driver_spi / esp_driver_gpio / nvs_flash / esp_timer`，定义 `NO_GLOBAL_OFFSET`、`RASTERIZER_IN_FLASH`，C++17。

### 3.2 PicoSystem 兼容层（picosystem.hpp / picosystem_compat.cpp）

- 提供游戏用到的 API 子集：帧缓冲 buffer_t、pen/clip/target/camera/cursor、图元、文本、rgb、按键状态等。
- 帧缓冲保持原生 GBAR 布局，RGB565 转换下沉到显示驱动。
- 逻辑按键枚举与板级 BtnId 对齐（RP GPIO 编号无意义，重新定义）。

### 3.3 主循环（app_main.cpp）

- NVS 初始化 → input_init → display_init → init()。
- 单线程循环：重置 pen/clip/target/camera/cursor → input_poll → update → draw → display_present。
- 弹性节拍：目标 40FPS（25ms），慢帧仅 `vTaskDelay(1)`，不累积追赶延迟。

### 3.4 main.cpp 适配（RP 耦合点全部条件编译）

- 独立 framebuffer/FRAMEBUFFER 用 `#ifndef ESP_PLATFORM` 包裹。
- 超频块保留：S3 不定义 `PICOCRAFT_SYSCLK_KHZ`，自动走空实现，RP 头不被 include。
- `core1_entry()` 在 S3 不编译；`render_sync()` 改为同步实现（调用 render_rasterize，返回非零表示新帧就绪）。
- `init()` 去掉 multicore 启动、bus_ctrl 优先级、FIFO 推送，均条件编译。
- `flush_save()` 在 S3 直接保存，RP 分支保留 Core1 暂停/重启逻辑。

### 3.5 ST7789 驱动（st7789_display.*）

- 面板初始化命令、SPI 配置、`send_lines` 六事务结构与验证工程一致。
- 实现 GBAR→RGB565（通道提取、4→5/6 位扩展、字节交换）、2x 缩放、PSRAM DMA。
- DMA 行缓冲改为 static 持久分配、每帧复用（见 4.3）。

### 3.6 输入与存档

- `input.cpp`：全 GPIO 上拉、低有效，软件去抖与按下/释放边沿检测。
- `save_store.*`：整幅存档映像作为单个 NVS blob 读写，记录布局与 RP 原版一致（magic `PCBL`）。

---

## 4. 关键技术问题与解决

### 4.1 颜色错位：字节交换（核心）

- **现象**：真机"没有绿色、褐色变成浅蓝色"。
- **机理**：ST7789 要求每个 16bit 像素高字节先到（第一个时钟是 D15）；小端内存中低字节在低地址，
  DMA 先发低字节，导致每个像素高低字节互换。
  - 纯绿 RGB565=0x07E0 交换为 0xE007，G 通道归零 → 无绿色。
  - 褐色 R 高 B 低，交换后 R/B 互换、B 最高 → 呈浅蓝。
- **解决**：在 `gbar_to_rgb565()` 返回前对结果做 `(v>>8)|(v<<8)`；黑边 0x0000 交换后仍为 0，不受影响。

### 4.2 放块死机：主任务栈溢出

- **现象**：运行十几秒、放块时死机。
- **机理**：`SaveStore::save/load` 在栈上开 4352 字节缓冲，而主任务栈仅 3584 字节；放块后约 90 帧触发 NVS 落盘，
  低帧率下正好约十几秒，栈被踩坏。
- **解决**：存档缓冲改为 `static`（单线程安全）；主任务栈 `CONFIG_ESP_MAIN_TASK_STACK_SIZE` 提升到 8192。

### 4.3 DMA 缓冲重构（API 变更）

- ESP-IDF v6.1 已移除 `spi_bus_dma_memory_free`（仅保留 alloc）。
- 两个 DMA 行缓冲改为 static 持久成员 `s_lines[2]`，display_init 用 `spi_bus_dma_memory_alloc` 分配一次，
  display_present 复用，删除每帧 alloc/free。

### 4.4 全屏显示与宽高比

- 逻辑帧由 120×120 改为 160×120（4:3），2x 等比放大正好铺满 320×240，去除黑边。
- 投影矩阵 X 方向按 `160/120` 做宽高比补偿，保证世界方块在非方形屏幕上仍是正方形，垂直视野不变、水平视野变宽。

### 4.5 热函数 IRAM 化

- 用 `IRAM_ATTR` 把高频函数放入片上 SRAM 执行：render_rasterize（含 shader）、render_triangle、裁剪、compute_code、
  voxel_mesh 面提取、纹理 ID 查询及哈希 helper、gbar_to_rgb565、fill_batch、SPI 中断回调。
- 注意 `shader_wireframe.cpp` 被 render_rasterize.cpp 在顶部 include，`esp_attr.h` 必须在其之前 include，
  否则报 `'IRAM_ATTR' does not name a type`。

---

## 5. 编译期排错记录（GCC 15 + -Werror）

按出现顺序：

1. `esp_timer.h` 找不到 → CMake 依赖加 esp_timer。
2. 裸 int 传给 gpio_num_t 报错 → 引脚宏用 `((gpio_num_t)n)` 强制转换。
3. `spi_bus_dma_memory_free` 不存在 → 见 4.3 重构为持久缓冲。
4. cos/sin 返回 double 触发 narrowing → 改用 cosf/sinf。
5. 兼容层 rgb 表达式括号少一个闭合 → 补全外层转换括号。
6. 多个 maybe-uninitialized → 相关变量声明处默认初始化（=0 / nullptr），逻辑不变。
7. misleading-indentation → 一行多个 if 拆成多行。
8. `abs` 未声明 → 加 `#include <cstdlib>`（实际用 std::abs）。
9. Python 环境路径不一致（配置态与当前 profile 不同）→ `idf.py fullclean` 后重新构建解决。

---

## 6. 已验证的死路（避免重复）

- v6.1 无 `spi_bus_dma_memory_free`，不要再调用；`idf.py build` 不支持 `-j`，并行度用环境变量
  `CMAKE_BUILD_PARALLEL_LEVEL`。
- 八线 PSRAM 正确符号为 `CONFIG_SPIRAM_MODE_OCT`；v6.1 无 legacy I2S，音频须用 `driver/i2s_std.h`。
- ST7789 数据手册标称 SPI 时钟远低于 80MHz，80MHz 属于探极限，若花屏应回退 40MHz（速度与稳定性的甜点）。

---

## 7. 后续工作（第二阶段：音频）

- 用新版 `driver/i2s_std.h` 实现 I2S 输出（外部 DAC 如 MAX98357A），替换 `voxel/audio_stub.h` 空桩。
- 移植实时波形合成器（正弦 + ADSR + 噪声 + 音高滑变）与挖/放/跳/落地四种音效，保持非阻塞触发。
- 接入后在 main.cpp 恢复 game_audio_* 的真实实现，重新编译并真机验证。
