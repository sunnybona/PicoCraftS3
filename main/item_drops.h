// 掉落物：方块被挖掉后产生的小立方体，受重力下落贴地，玩家靠近自动拾取进库存。
// 固定大小对象池，不写入 flash（掉落物是瞬态的）。
#ifndef ITEM_DROPS_H
#define ITEM_DROPS_H

#include <cstdint>

class VoxelWorld;

#define MAX_DROPS 20
#define DROP_PICKUP_RADIUS 1.35f

struct DropItem {
    float x, y, z;       // 中心位置
    uint8_t item;        // 物品 id
    uint8_t age;         // 存在帧数（用于弹跳/旋转动画）
    bool active;
};

class ItemDrops {
public:
    DropItem drops[MAX_DROPS];

    ItemDrops();

    // 在 (x,y,z) 生成一个物品掉落（找空槽，池满则丢弃）
    void spawn(float x, float y, float z, uint8_t item);

    // 每帧：下落贴地 + 动画 + 靠近玩家自动拾取（直接写入全局库存）
    void update(const VoxelWorld &world, float px, float py, float pz);

    // 渲染距离内的掉落物（小旋转立方体）
    void render(float cam_x, float cam_z) const;

    // 清空全部掉落
    void clear();
};

#endif
