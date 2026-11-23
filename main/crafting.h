// 合成系统：精简的"配方列表 + 一键制作"，无需 3x3 网格（适配小屏与少按键）。
// 每个配方最多 4 种材料；材料足够即可制作，自动扣材料、加产出。
#ifndef CRAFTING_H
#define CRAFTING_H

#include <cstdint>

#define MAX_ING 4

struct Recipe {
    uint8_t out;                       // 输出物品 id
    uint8_t out_n;                     // 输出数量
    struct Ing { uint8_t item; uint8_t n; } ing[MAX_ING];
};

// 配方总数
int recipe_count();

// 取配方
const Recipe &recipe_at(int i);

// 当前库存是否满足该配方
bool can_craft(int i);

// 执行制作：材料足够才扣材料并加产出，返回是否成功
bool do_craft(int i);

#endif
