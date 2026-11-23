// 体素 DDA 射线（Amanatides & Woo 快速体素遍历）：
// 从相机眼睛沿视线方向逐格步进，找到第一个实心方块，用于挖/放目标选择。
#ifndef VOXEL_RAY_H
#define VOXEL_RAY_H

#include "voxel_world.h"

struct RayHit {
    bool  hit;
    int   bx, by, bz;     // 命中的实心方块坐标
    int   px, py, pz;     // 进入该方块前所在的空方块（放置位置）
    float distance;       // 命中距离（米）
};

// 起点 (ox,oy,oz)、方向 (dx,dy,dz)（无需单位化，内部按单位方向处理），max_dist 为最大距离
RayHit voxel_raycast(const VoxelWorld &world,
                     float ox, float oy, float oz,
                     float dx, float dy, float dz,
                     float max_dist);

// 世界坐标点经 VP 矩阵变换到屏幕坐标；返回 false 表示在相机后面/无效
bool world_to_screen(float wx, float wy, float wz,
                     int &sx, int &sy);

#endif
