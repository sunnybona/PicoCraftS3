// 昼夜循环：根据 day_time 计算天空色与全局亮度，并在玩家靠近火炬时提升亮度。
// 渲染器（shader 1/2/100/101）读取 g_light_shade 对非自发光像素做统一变暗；
// HUD 与自发光火炬不受影响。
#ifndef DAY_NIGHT_H
#define DAY_NIGHT_H

#include <cstdint>

class VoxelWorld;

// 当前全局亮度乘数（0..15，15=全亮，夜晚约 5）
extern uint8_t g_light_shade;

// 是否处于夜晚（用于怪物生成判断）
bool is_night_time();

// 每帧推进：更新 day_time、天空色、全局亮度（含火炬加成）
void update_day_night(const VoxelWorld &world,
                      float px, float py, float pz);

// 对一个 GBAR 颜色按当前亮度缩放（自发光方块传 emissive=true 跳过）
uint16_t apply_light(uint16_t gbar, bool emissive);

#endif
