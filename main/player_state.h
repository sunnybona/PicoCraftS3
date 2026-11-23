// 玩家状态：库存、生命值、昼夜计时、成就、手持快捷栏索引、提示文字（toast）。
// 全局单例 player；掉落物/合成/战斗/HUD/存档都通过这里访问。
#ifndef PLAYER_STATE_H
#define PLAYER_STATE_H

#include <cstdint>
#include "items.h"

#define PLAYER_MAX_HP 10          // 生命值上限（10 点 = 5 颗心）
#define DAY_LENGTH 2400           // 一个昼夜周期（帧，约 60 秒）
#define TOAST_TICKS 70            // 提示文字持续帧数

// 成就位
#define ACH_COAL   (1u << 0)
#define ACH_IRON   (1u << 1)
#define ACH_DIA    (1u << 2)
#define ACH_CRAFT  (1u << 3)
#define ACH_KILL   (1u << 4)
#define ACH_TNT    (1u << 5)
#define ACH_BUILD  (1u << 6)

class PlayerState {
public:
    uint16_t inv[ITEM_ID_COUNT];  // 各物品数量
    int8_t   hp;                  // 当前生命
    uint16_t day_time;            // 昼夜计时 0..DAY_LENGTH-1
    uint32_t achievements;        // 已解锁成就位
    int      hand_index;          // 手持快捷栏索引
    uint8_t  hurt_cooldown;       // 受伤后无敌帧
    char     toast[18];           // 提示文字
    uint8_t  toast_time;          // 提示剩余帧
    bool     respawn_pending;     // 待重生标志

    // 存档恢复的相机位置/朝向（load 写入，main 初始化相机时应用）
    float    cam_x, cam_y, cam_z;
    float    cam_yaw, cam_pitch;
    bool     have_cam_pos;

    PlayerState();

    // 全新开局初始化（生命满、时间早晨）
    void reset_new();

    // 当前手持物品 id
    uint8_t held_item() const;

    // 增加物品（自动触发对应成就）
    void add(uint8_t item, uint16_t n = 1);

    // 查询数量
    uint16_t count(uint8_t item) const;

    // 消耗：数量足够才扣并返回 true
    bool consume(uint8_t item, uint16_t n = 1);

    // 提示
    void show_toast(const char *msg);

    // 成就
    void unlock(uint32_t bit);
    bool has(uint32_t bit) const;
};

extern PlayerState player;

// 手持快捷栏候选（可放置方块 + 镐），B 长按循环
extern const uint8_t HAND_BAR[];
extern const int HAND_BAR_N;

#endif
