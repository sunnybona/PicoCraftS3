// 出生点强制平整平台：保证开局视野开阔，动物与房子都落在平台内、无地形遮挡。
// 纯头文件（常量 + inline 判定），供 voxel_world / structures / entities 共用，
// 避免多处重复实现导致口径不一致。
#ifndef SPAWN_PLATFORM_H
#define SPAWN_PLATFORM_H

#include <cstdint>

#define SPAWN_PLATFORM_HIGH 5      // 平台统一地表高度（与出生点自然高度一致，走过去是平的）

// 平台矩形（含端点）：玩家在 (0,0) 看向 -Z，平台铺在前方
#define SPAWN_PLATFORM_X0 -8
#define SPAWN_PLATFORM_X1  8
#define SPAWN_PLATFORM_Z0 -12
#define SPAWN_PLATFORM_Z1 -3

static inline bool in_spawn_platform(int wx, int wz) {
    return wx >= SPAWN_PLATFORM_X0 && wx <= SPAWN_PLATFORM_X1 &&
           wz >= SPAWN_PLATFORM_Z0 && wz <= SPAWN_PLATFORM_Z1;
}

// 平台内的强制地表高度；不在平台返回 -1，调用方回退到自然噪声高度
static inline int spawn_platform_surface(int wx, int wz) {
    return in_spawn_platform(wx, wz) ? SPAWN_PLATFORM_HIGH : -1;
}

#endif
