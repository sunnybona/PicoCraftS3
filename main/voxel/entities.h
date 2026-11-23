// 实体系统：动物（牛/羊/鸡/鸭）、NPC 与怪物（僵尸）。
// 固定大小实体池、严格限数量。动物按玩家位置确定性生成并流式激活；
// 僵尸仅在夜晚围绕玩家生成、追击攻击，白天消失。不写入 flash。
// 实体用轴对齐的小立方体组合渲染，复用 Pico3D 三角形管线。
#ifndef ENTITIES_H
#define ENTITIES_H

#include <cstdint>

class VoxelWorld;

enum EntityType : uint8_t {
    ENT_NONE = 0,
    ENT_COW,
    ENT_SHEEP,
    ENT_CHICKEN,
    ENT_DUCK,
    ENT_NPC,
    ENT_ZOMBIE,
};

#define MAX_ENTITIES 16          // 同时驻留的实体上限
#define ENTITY_ACTIVE_RADIUS 24  // 距玩家（方块）内激活/更新
#define ENTITY_DRAW_RADIUS  14   // 距玩家（方块）内渲染
#define SPAWN_ENTITY_N 5         // 出生点强制保留的实体数量（占前 N 槽）
#define MAX_ZOMBIES 4            // 同时存在的僵尸上限
#define ZOMBIE_HP 5              // 僵尸生命值
#define ZOMBIE_ATTACK 2          // 僵尸攻击伤害
#define ZOMBIE_ATTACK_CD 22      // 僵尸攻击冷却帧

struct Entity {
    uint8_t  type;          // ENT_NONE = 空槽
    int16_t  home_cx;       // 生成所在 chunk 坐标
    int16_t  home_cz;
    uint8_t  seq;           // 同 chunk 内序号
    float    x, y, z;       // 脚部基准位置（米）
    float    yaw;
    uint8_t  rng;           // 内部确定性随机状态
    uint8_t  timer;         // AI 决策计时
    uint8_t  moving;        // 当前是否在游走
    int8_t   hp;            // 生命值（怪物用）
    uint8_t  atk_cd;        // 攻击冷却（怪物用）
};

// 攻击结果
struct AttackResult {
    bool hit;
    bool killed;
    uint8_t drop;           // 击杀掉落物品 id
    float kx, ky, kz;       // 击杀位置
};

class EntitySystem {
public:
    Entity entities[MAX_ENTITIES];

    EntitySystem();

    // 每帧：动物流式激活 + 怪物生成/消失 + 全部 AI 推进
    void update(const VoxelWorld &world, uint32_t seed,
                float cam_x, float cam_z, uint32_t tick);

    // 渲染距离内的实体（小立方体组合），写入三角形列表
    void render(float cam_x, float cam_z);

    // 玩家沿视线攻击：对近距离、朝向内的僵尸造成伤害
    AttackResult attack_entities(float px, float py, float pz,
                                 float yaw, uint8_t damage);

    // 范围伤害（TNT 爆炸）：对中心半径内的僵尸造成伤害，死亡掉落在内部处理
    void damage_in_radius(float cx, float cy, float cz,
                          float radius, uint8_t damage);

    // 当前僵尸数量
    int zombie_count() const;
};

#endif
