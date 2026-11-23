// 体素面提取：把玩家视距内方块的可见面转成 Pico3D 的 triangle_32，
// 经 render_triangle() 进入变换/裁剪/背面剔除并写入三角形列表，由 Core1 光栅化。
// 方块面使用 shader_id=100 采样程序化纹理（见 block_textures）。
#ifndef VOXEL_MESH_H
#define VOXEL_MESH_H

#include "voxel_world.h"
#include "../engine/render_globals.h"

// 查询某方块的基础 RGB（0..15），供 HUD 使用
void block_rgb(uint8_t type, uint8_t &r, uint8_t &g, uint8_t &b);

class VoxelMesh {
public:
    // 以相机水平位置 (cam_x,cam_z) 为中心、radius 为半径（方块），遍历并提交可见面
    void build(const VoxelWorld &world, float cam_x, float cam_z, int radius);
};

#endif
