// 人工结构（房子等）：基于世界坐标的确定性结构函数。
// 房子会跨 chunk，因此不依赖某个 chunk 的生成状态——每个 chunk 独立生成时，
// 对每个世界坐标调用 structure_block() 即可得到一致的结果。
#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <cstdint>

// 返回世界坐标 (wx, y, wz) 处的人工结构方块；该位置没有结构返回 -1。
// 与 voxel_world 使用同一套确定性哈希，保证多 chunk 拼接处对齐。
int structure_block(int wx, int y, int wz, uint32_t seed);

#endif
