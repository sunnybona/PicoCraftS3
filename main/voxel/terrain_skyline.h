// 远景天际线：在不加载远处 chunk、不增加三角形的前提下，用确定性地形高度函数
// 在天空区域绘制远山轮廓与水面，扩大可视范围。仅填充帧缓冲中的天空像素，
// 因此近处体素/水会自然遮挡远景。
#ifndef TERRAIN_SKYLINE_H
#define TERRAIN_SKYLINE_H

#include <stdint.h>

// 在当前 SCREEN（2D HUD 阶段）上绘制远景，seed 需与世界种子一致
void render_terrain_skyline(uint32_t seed);

#endif
