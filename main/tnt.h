// TNT：点燃后倒计时（可下落、闪烁），到时爆炸：
// 破坏半径内方块（基岩除外）并转为掉落物，对玩家和僵尸造成范围伤害。
#ifndef TNT_H
#define TNT_H

#include <cstdint>

class VoxelWorld;

#define MAX_PRIMED_TNT 4
#define TNT_FUSE 32          // 引信帧数
#define TNT_BLAST_R 3.2f     // 爆炸半径

struct PrimedTnt {
    bool active;
    float x, y, z;
    uint8_t fuse;
};

class TntSystem {
public:
    PrimedTnt tnts[MAX_PRIMED_TNT];

    TntSystem();

    // 点燃 (x,y,z) 处的 TNT 方块（调用方先把该方块置空）
    void ignite(float x, float y, float z);

    // 每帧：引信推进、下落、到时爆炸
    void update(VoxelWorld &world, float px, float py, float pz);

    // 渲染已点燃 TNT（闪烁立方体）
    void render(float cam_x, float cam_z) const;

    void clear();
};

#endif
