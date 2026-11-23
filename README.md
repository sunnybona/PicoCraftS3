# PicoCraft（ESP32-S3 移植版）

[English](README.en.md) | 简体中文

一个第一人称体素沙盒游戏（Minecraft 风格），本仓库是把原本运行在 **Raspberry Pi RP2040 / RP2350 + Pimoroni PicoSystem**
上的 PicoCraft，移植到 **乐鑫 ESP32-S3（ESP-IDF v6.1.1）** 的版本。游戏通过 SPI 驱动 ST7789V 液晶屏，
借助片上 PSRAM 承载帧缓冲与 DMA 缓冲，NVS 保存挖/放编辑。

> **阶段说明：第一阶段不含音频。** 显示、游戏逻辑、按键、存档均已在真机跑通；音频部分等 I2S 硬件测试完成后再移植，
> 当前以空桩（`voxel/audio_stub.h`）占位，音效调用均为空操作。

---

## 目录

- [功能特性](#功能特性)
- [硬件与引脚分配](#硬件与引脚分配)
- [目录结构](#目录结构)
- [按键操作](#按键操作)
- [构建与烧录](#构建与烧录)
- [显示与配色](#显示与配色)
- [存档机制](#存档机制)
- [资源占用](#资源占用)
- [移植说明](#移植说明)
- [当前状态与后续计划](#当前状态与后续计划)
- [致谢与开源协议](#致谢与开源协议)

---

## 功能特性

### 体素世界

- **基于 chunk 的流式世界**：chunk 尺寸 16×16×16，水平方向无限（受整型坐标限制），固定高度 16。
- 玩家周围 3×3 共 9 个 chunk 常驻内存，离开即卸载，每帧最多生成一个 chunk，避免卡顿。
- **程序化地形**：确定性噪声生成起伏地形，海平面（y=4）以下自动填充水方块。
- **15 种方块**：空气、草、泥土、石头、原木、树叶、沙子、木板、水、圆石、砖、玻璃、基岩、雪、金。
- 稀疏挖/放编辑（最多 256 条），持久化到 **NVS**，重启后自动恢复。
- 出生点强制平整平台，平台内确定性生成房子、动物与 NPC。

### 第一人称与渲染

- 自研软件 3D 管线：三角形变换、近平面裁剪、背面剔除、光栅化、固定方向光照。
- 体素可见面提取（只生成暴露面），按视距半径（11 格）构建，实体与世界共用同一三角形管线。
- **视线 DDA 体素射线**选择目标方块，叠加 3D 线框高亮；挖/放当帧反映到网格。
- 完整 AABB 物理：重力、碰撞、手动跳跃、落地检测。
- 程序化 16×16 方块纹理，运行时生成、不占 Flash；草顶面带成片分布的花朵。
- 第一人称手持镐头，挖掘时播放挥动动画；HUD 含准星、hotbar 色样与序号。
- **远景天际线**：不增加 chunk 与三角形，直接在天空区域绘制远山轮廓与水面。

### 实体

- 动物 **牛 / 羊 / 鸡 / 鸭** 与一个 **NPC**；固定大小实体池，按玩家位置流式激活/失活，带简单游走 AI。

---

## 硬件与引脚分配

目标板为 ESP32-S3（如 esp32s3-devkit-c）+ ST7789V 320×240 SPI 液晶屏 + GPIO 直连按键。

### LCD（SPI2_HOST，mode 0）

| 功能 | GPIO | 备注 |
| --- | --- | --- |
| MISO | 17 | 本应用只写，仍配置 |
| MOSI | 12 | 数据线 |
| SCLK | 48 | 串行时钟 |
| CS | 14 | 片选 |
| DC | 47 | 数据/命令选择 |
| RST | 3 | 硬件复位 |
| 背光 BCKL | 39 | 高电平点亮 |

### 按键（内部上拉，低电平有效）

| 逻辑按键 | 功能 | GPIO |
| --- | --- | --- |
| UP | 前进 | 7 |
| RIGHT | 右转 | 6 |
| DOWN | 后退 | 46 |
| LEFT | 左转 | 45 |
| A | 挖方块 | 15 |
| B | 修饰键（hotbar / 俯仰） | 5 |
| X | 跳跃 | 17 |
| Y | 放方块 | 8 |

> 引脚集中在 `main/board_config.h`，改线只需修改这一个文件。

---

## 目录结构

```
PicoCraftS3/
├── CMakeLists.txt          # 顶层 ESP-IDF 工程（MINIMAL_BUILD，project picocraft_s3）
├── sdkconfig.defaults      # 默认配置：PSRAM/Flash/CPU/主任务栈
├── sdkconfig               # 当前实际配置（构建生成并随调优更新）
│
└── main/
    ├── CMakeLists.txt      # 组件注册：源文件、include、依赖、编译宏
    ├── board_config.h      # 引脚、屏幕几何、按键映射（单一事实来源）
    ├── app_main.cpp        # ESP32-S3 入口与单线程主循环（弹性节拍）
    ├── main.cpp            # 游戏主体：世界/输入/交互/物理/HUD（RP 耦合点已条件编译）
    ├── chunk_data.h        # 纹理清单等
    │
    ├── picosystem.hpp      # PicoSystem 兼容层 API
    ├── picosystem_compat.cpp  # 图元/文本/状态/帧缓冲的平台实现
    ├── input.hpp / input.cpp  # GPIO 按键驱动（去抖、边沿检测）
    ├── st7789_display.hpp / st7789_display.cpp  # ST7789 驱动、GBAR→RGB565、缩放、DMA
    │
    ├── engine/             # 软件 3D 引擎
    │   ├── render_camera.cpp      # 相机与投影矩阵（含宽高比补偿）
    │   ├── render_triangle.cpp    # 变换/裁剪/剔除/入列
    │   ├── render_clipping.cpp    # 近平面裁剪
    │   ├── render_culling.cpp     # 视锥剔除
    │   ├── render_rasterize.cpp   # 光栅化（include shader_wireframe.cpp）
    │   ├── render_lighting.cpp    # 光照（本工程空实现，颜色已烘焙）
    │   ├── render_globals.h / render_math.h / chunk_globals.h
    │   └── shader_wireframe.cpp   # 线/像素着色（被 include，不单独编译）
    │
    └── voxel/              # 游戏逻辑
        ├── voxel_world.*          # chunk 流式世界、方块、稀疏编辑
        ├── voxel_mesh.*           # 可见面提取
        ├── voxel_ray.*            # DDA 射线
        ├── player_physics.*       # AABB 物理
        ├── save_store.*           # NVS 持久化
        ├── block_textures.*       # 程序化纹理
        ├── structures.*           # 房子结构
        ├── entities.*             # 动物/NPC
        ├── terrain_skyline.*      # 远景天际线
        ├── spawn_platform.h        # 出生平台
        └── audio_stub.h           # 音频空桩（第一阶段）
```

---

## 按键操作

| 操作 | 按键 |
| --- | --- |
| 左右转向 | LEFT / RIGHT（按住连续转） |
| 前进 / 后退 | UP / DOWN |
| 跳跃 | X（按下沿触发） |
| 挖掉目标方块 | A（基岩不可破坏） |
| 放置手持方块 | Y（放在目标前方空方块） |
| 精细俯仰 | 按住 **B** + UP / DOWN |
| 切换 hotbar 手持方块 | 单纯长按 **B**（不按方向） |
| 俯仰回正 | 快速点一下 **B** |

hotbar 顺序（共 11 项）：泥土、石头、圆石、原木、木板、沙子、砖、玻璃、雪、金、树叶。

---

## 构建与烧录

### 依赖

- **ESP-IDF v6.1.1**（目标芯片 esp32s3），以及其自带的 xtensa-esp32s3 工具链、CMake、Ninja、Python。
- 本工程所有源码与配置都在仓库内，**无需额外组件或手动下载依赖**。

### 1. 打开 ESP-IDF 终端

从开始菜单运行 **"ESP-IDF 6.1 PowerShell"** 或 **"ESP-IDF 6.1 CMD"**（安装 ESP-IDF 时自带），
终端会自动设置好 `IDF_PATH` 与工具链环境。

### 2. 编译

在 ESP-IDF 终端中执行：

```cmd
idf.py -C E:\PicoCraftS3 build
```

### 3. 烧录并监视

把开发板接到电脑，确认串口号（本机为 COM22，CH340），然后：

```cmd
idf.py -C E:\PicoCraftS3 -p COM22 flash monitor
```

一步完成编译、烧录、打开串口监视；退出监视器按 `Ctrl + ]`。只烧录不重新编译可省略 `build`，直接 `idf.py -p COM22 flash`。

### 配置说明

- `sdkconfig.defaults` 已写好：16MB Flash（DIO/80MHz）、八线 PSRAM（80MHz）、CPU 240MHz、主任务栈 8192、FreeRTOS 1000Hz。
- 首次构建自动生成 `build/` 与 `sdkconfig`；需要调整配置可运行 `idf.py menuconfig`。
- 想彻底干净重建：`idf.py fullclean` 后再 `build`。

---

## 显示与配色

- **物理分辨率 320×240，逻辑分辨率 160×120**，2x 最近邻等比放大，正好铺满全屏、无黑边、无变形。
- 面板工作在 **RGB565** 16bit/像素；游戏内部使用 PicoSystem 的 **GBAR** 4 位通道格式（`aaaarrrrggggbbbb`），
  驱动在 `gbar_to_rgb565()` 中完成通道提取、4→5/6 位扩展与格式转换。
- **字节交换**：ST7789 要求高字节先到，而小端 DMA 先发低字节，驱动对每个像素做高低字节交换，否则会出现"无绿色、褐色变浅蓝"的错位。
- **宽高比补偿**：投影矩阵 X 方向按 `160/120` 缩放，保证世界方块在非方形屏幕上仍是正方形。
- SPI 像素时钟当前设为 **80MHz**；若走线较长出现花屏，可在 `board_config.h` 把 `LCD_SPI_CLOCK_HZ` 降到 40MHz 或 20MHz。
- 两个 DMA 行缓冲（各 16 行）与转换缓冲均位于 PSRAM，持久分配、每帧复用，不在运行时反复申请/释放。

---

## 存档机制

- 挖/放编辑以稀疏记录形式保存在 `VoxelWorld::edits`（最多 256 条），世界本体由种子确定性生成。
- 停止编辑约 90 帧后，`SaveStore::save()` 把整幅存档映像（头部 16 字节 + 记录，共 4352 字节）作为
  **一个 NVS blob** 写入（命名空间 `picocraft`，键 `save`）。
- 启动时 `SaveStore::load()` 读回并在 chunk 生成后重放，恢复所有改动。记录布局与 RP 原版一致（magic `PCBL`，version 1）。

---

## 资源占用

最近一次构建（ESP-IDF v6.1.1，CPU 240MHz，SPI 80MHz）：

| 项目 | 占用 | 余量 |
| --- | --- | --- |
| 应用固件（Flash app 分区） | 约 229 KB（0x38280） | 分区 1MB，剩余 78% |
| 片上 DIRAM | 约 242 KB（71%） | 约 99 KB |
| 热函数 IRAM | 16 KB（100%，系统保留） | — |

> 热函数（光栅化、三角形变换、裁剪、面提取、配色、SPI 中断回调）已用 `IRAM_ATTR` 放入片上 SRAM 执行，
> 避免从 Flash XIP 读取带来的等待。

---

## 移植说明

完整的移植过程、改动清单与排错记录见 [PORTING.md](PORTING.md)。

---

## 当前状态与后续计划

- 已完成：工程骨架、游戏源码适配、PicoSystem 兼容层、ST7789 驱动、GPIO 按键、NVS 存档、单线程主循环、
  颜色字节交换、全屏显示、热函数 IRAM 化；编译烧录成功并在真机运行。
- 待办（第二阶段）：I2S 硬件测试通过后，用新版 `driver/i2s_std.h` 移植音频与实时合成器，替换当前空桩。
- 待真机持续确认：长时运行稳定性、全部按键与 hotbar 交互。

---

## 致谢与开源协议

- 游戏与 PicoSystem 兼容层基于 [Pimoroni PicoSystem](https://github.com/pimoroni/picosystem)（**MIT License**,
  Copyright (c) 2021 Pimoroni Ltd）修改。
- 软件 3D 管线借鉴 Pico3D 工程思路。

本工程自身代码建议沿用 **MIT License**；PicoSystem 相关部分仍遵循其 MIT License。
