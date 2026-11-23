#include "voxel_ray.h"
#include "../engine/render_math.h"
#include "../engine/render_globals.h"
#include <math.h>

// ---------------- 体素 DDA ----------------

RayHit voxel_raycast(const VoxelWorld &world,
                     float ox, float oy, float oz,
                     float dx, float dy, float dz,
                     float max_dist) {
    RayHit result;
    result.hit = false;

    float len = sqrtf(dx * dx + dy * dy + dz * dz);
    if (len < 1e-6f) return result;
    dx /= len; dy /= len; dz /= len;

    int x = (int)floorf(ox);
    int y = (int)floorf(oy);
    int z = (int)floorf(oz);

    int stepX = (dx > 0.0f) ? 1 : ((dx < 0.0f) ? -1 : 0);
    int stepY = (dy > 0.0f) ? 1 : ((dy < 0.0f) ? -1 : 0);
    int stepZ = (dz > 0.0f) ? 1 : ((dz < 0.0f) ? -1 : 0);

    const float BIG = 1e30f;
    float tDeltaX = (dx != 0.0f) ? fabsf(1.0f / dx) : BIG;
    float tDeltaY = (dy != 0.0f) ? fabsf(1.0f / dy) : BIG;
    float tDeltaZ = (dz != 0.0f) ? fabsf(1.0f / dz) : BIG;

    float tMaxX = (stepX != 0) ? (((stepX > 0 ? (float)(x + 1) : (float)x) - ox) / dx) : BIG;
    float tMaxY = (stepY != 0) ? (((stepY > 0 ? (float)(y + 1) : (float)y) - oy) / dy) : BIG;
    float tMaxZ = (stepZ != 0) ? (((stepZ > 0 ? (float)(z + 1) : (float)z) - oz) / dz) : BIG;

    int px = x, py = y, pz = z;
    float t = 0.0f;

    for (int iter = 0; iter < 256; iter++) {
        // 当前格实心则命中
        if (VoxelWorld::is_solid((uint8_t)world.get_block(x, y, z))) {
            result.hit = true;
            result.bx = x; result.by = y; result.bz = z;
            result.px = px; result.py = py; result.pz = pz;
            result.distance = t;
            return result;
        }

        px = x; py = y; pz = z;

        // 沿 tMax 最小的轴前进到下一格
        if (tMaxX < tMaxY && tMaxX < tMaxZ) {
            x += stepX; t = tMaxX; tMaxX += tDeltaX;
        } else if (tMaxY < tMaxZ) {
            y += stepY; t = tMaxY; tMaxY += tDeltaY;
        } else {
            z += stepZ; t = tMaxZ; tMaxZ += tDeltaZ;
        }

        if (t > max_dist) break;
    }
    return result;
}

// ---------------- 世界坐标 -> 屏幕坐标 ----------------

bool world_to_screen(float wx, float wy, float wz, int &sx, int &sy) {
    int X = float_to_int(wx);
    int Y = float_to_int(wy);
    int Z = float_to_int(wz);

    int w = ((mat_vp[3][0] * X) + (mat_vp[3][1] * Y) + (mat_vp[3][2] * Z)
             + (mat_vp[3][3] * FIXED_POINT_FACTOR)) / FIXED_POINT_FACTOR;
    if (w <= 0) return false;   // 相机后方或退化点

    int clip_x = ((mat_vp[0][0] * X) + (mat_vp[0][1] * Y) + (mat_vp[0][2] * Z)
                  + (mat_vp[0][3] * FIXED_POINT_FACTOR)) / w;
    int clip_y = ((mat_vp[1][0] * X) + (mat_vp[1][1] * Y) + (mat_vp[1][2] * Z)
                  + (mat_vp[1][3] * FIXED_POINT_FACTOR)) / w;

    sx = (clip_x + FIXED_POINT_FACTOR) * (SCREEN_WIDTH - 1) / FIXED_POINT_FACTOR / 2;
    sy = SCREEN_HEIGHT - ((clip_y + FIXED_POINT_FACTOR) * (SCREEN_HEIGHT - 1))
                          / FIXED_POINT_FACTOR / 2;
    return true;
}
