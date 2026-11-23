#include "terrain_skyline.h"

#include "picosystem.hpp"
#include "../engine/render_globals.h"
#include "voxel_world.h"
#include "voxel_ray.h"
#include <math.h>

using namespace picosystem;

// 远景采样距离（格）。Pico3D 透视矩阵无硬性 far 裁剪，42 仍可正常投影。
#define SKY_DISTANCE 42.0f
// 角度采样数（覆盖相机前方 180°）
#define SKY_STEPS 120

// ---------------- 确定性地形（与 voxel_world.cpp 同一算法，独立可调用） ----------------
static uint32_t hash2(int x, int z, uint32_t seed) {
    uint32_t h = seed;
    h = h * 374761393u + (uint32_t)(x * 668265263);
    h = h * 374761393u + (uint32_t)(z * 668265263);
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static float value_noise(float x, float z, uint32_t seed) {
    int x0 = (int)floorf(x);
    int z0 = (int)floorf(z);
    float fx = x - x0;
    float fz = z - z0;
    float u = fx * fx * (3.0f - 2.0f * fx);
    float v = fz * fz * (3.0f - 2.0f * fz);

    float a = (hash2(x0,     z0,     seed) & 0xffff) / 65535.0f;
    float b = (hash2(x0 + 1, z0,     seed) & 0xffff) / 65535.0f;
    float c = (hash2(x0,     z0 + 1, seed) & 0xffff) / 65535.0f;
    float d = (hash2(x0 + 1, z0 + 1, seed) & 0xffff) / 65535.0f;

    float ab = a * (1.0f - u) + b * u;
    float cd = c * (1.0f - u) + d * u;
    return ab * (1.0f - v) + cd * v;
}

static float fbm(float x, float z, uint32_t seed) {
    float total = 0.0f, amp = 1.0f, freq = 1.0f, maxv = 0.0f;
    for (int o = 0; o < 3; o++) {
        total += value_noise(x * freq, z * freq, seed + (uint32_t)o * 1013u) * amp;
        maxv += amp;
        amp *= 0.5f;
        freq *= 2.0f;
    }
    return total / maxv;
}

// 与 voxel_world column_height 完全一致
static int terrain_height(int wx, int wz, uint32_t seed) {
    float n = fbm((float)wx * 0.12f, (float)wz * 0.12f, seed);
    int h = 2 + (int)(n * 8.0f);
    if (h > CHUNK_H - 6) h = CHUNK_H - 6;
    return h;
}

// 组装 GBAR 颜色
static inline color_t c16(uint8_t r, uint8_t g, uint8_t b) {
    return (color_t)(((g & 15) << 12) | ((b & 15) << 8) | (r & 15));
}

// 从 top_y 向下填充一列，仅覆盖天空像素；遇到近处内容立即停止（实现遮挡）
static void fill_sky_column(int x, int top_y, color_t c) {
    if (x < 0 || x >= SCREEN_WIDTH) return;
    if (top_y < 0) top_y = 0;
    for (int y = top_y; y < SCREEN_HEIGHT; y++) {
        color_t *p = &SCREEN->data[y * SCREEN_WIDTH + x];
        if (*p == sky) *p = c;
        else break;
    }
}

void render_terrain_skyline(uint32_t seed) {
    const float cam_x = camera_position[0];
    const float cam_z = camera_position[2];

    const color_t col_snow = c16(12, 12, 13);
    const color_t col_rock = c16(8, 8, 9);
    const color_t col_grass = c16(6, 9, 6);
    const color_t col_water = c16(4, 7, 12);

    int prev_sx = -1;
    // 屏幕左 -> 右：φ 从 yaw+90° 到 yaw-90°
    for (int i = 0; i <= SKY_STEPS; i++) {
        float t = (float)i / (float)SKY_STEPS;          // 0 -> 1
        float phi = yaw + (0.5f - t) * 3.14159265f;     // yaw+90° -> yaw-90°

        float wx = cam_x + SKY_DISTANCE * (-sinf(phi));
        float wz = cam_z + SKY_DISTANCE * (-cosf(phi));
        int h = terrain_height((int)floorf(wx), (int)floorf(wz), seed);

        int sx, sy;
        int x_from = (prev_sx < 0) ? 0 : prev_sx;

        if (h < SEA_LEVEL) {
            // 地形在水下：画水面（投影 SEA_LEVEL 处）
            int sy_w;
            if (world_to_screen(wx, (float)SEA_LEVEL, wz, sx, sy_w)) {
                for (int x = x_from; x <= sx; x++) fill_sky_column(x, sy_w, col_water);
            }
        } else {
            // 陆地：按高度选色，投影地形顶面
            color_t c = (h >= 11) ? col_snow : (h >= 8) ? col_rock : col_grass;
            if (world_to_screen(wx, (float)(h + 1), wz, sx, sy)) {
                for (int x = x_from; x <= sx; x++) fill_sky_column(x, sy, c);
            }
        }
        prev_sx = sx;
    }
}
