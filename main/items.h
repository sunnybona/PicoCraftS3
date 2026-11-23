// 统一物品系统：方块（与 BlockType 对齐）+ 纯材料 + 工具，共用同一套物品 id。
// 库存、掉落、合成、HUD、挖掘判定都通过这里的 id 与属性表访问。
#ifndef ITEMS_H
#define ITEMS_H

#include <cstdint>
#include "voxel/voxel_world.h"

// ---------------- 物品 id（0..20 与 BlockType 对齐） ----------------
enum ItemId : uint8_t {
    // 方块（镜像 BlockType，0..20）
    ITEM_AIR = 0,
    ITEM_GRASS = 1,
    ITEM_DIRT = 2,
    ITEM_STONE = 3,
    ITEM_LOG = 4,
    ITEM_LEAVES = 5,
    ITEM_SAND = 6,
    ITEM_PLANK = 7,
    ITEM_WATER = 8,
    ITEM_COBBLE = 9,
    ITEM_BRICK = 10,
    ITEM_GLASS = 11,
    ITEM_BEDROCK = 12,
    ITEM_SNOW = 13,
    ITEM_GOLD = 14,
    ITEM_COAL_ORE = 15,
    ITEM_IRON_ORE = 16,
    ITEM_DIAMOND_ORE = 17,
    ITEM_CRAFT_TABLE = 18,
    ITEM_TORCH = 19,
    ITEM_TNT = 20,
    // 纯材料（不可放置）
    ITEM_STICK = 21,
    ITEM_COAL = 22,
    ITEM_IRON_INGOT = 23,
    ITEM_DIAMOND = 24,
    // 工具（镐）
    ITEM_WOOD_PICK = 25,
    ITEM_STONE_PICK = 26,
    ITEM_IRON_PICK = 27,
    ITEM_DIAMOND_PICK = 28,
    ITEM_ID_COUNT = 29,
};

// ---------------- 工具等级（0=徒手，1=木，2=石，3=铁，4=钻石） ----------------
#define TOOL_HAND  0
#define TOOL_WOOD  1
#define TOOL_STONE 2
#define TOOL_IRON  3
#define TOOL_DIA   4

// 手持物品 id 对应的工具等级；非工具返回 TOOL_HAND
uint8_t item_tool_level(uint8_t id);

// 手持物品每帧的挖掘力
uint8_t item_pick_power(uint8_t id);

// 物品是否为可放置的方块（id < BLOCK_TYPE_COUNT 且不是空气/水）
bool item_is_placeable(uint8_t id);

// 物品是否为工具（不占普通堆叠，耐久本版不做损耗）
bool item_is_tool(uint8_t id);

// 方块挖掉后掉落的物品 id；空气/水/基岩返回 ITEM_AIR（无掉落）
uint8_t block_drop_item(uint8_t block);

// 挖该方块所需的最低工具等级（0=徒手即可）
uint8_t block_required_tool(uint8_t block);

// 方块挖掘工作量基数（达到 工作量*4 的累计挖掘力即碎）
uint8_t block_hardness(uint8_t block);

// 方块是否自发光（火炬：夜晚不被压暗）
bool block_is_emissive(uint8_t block);

// 物品短名（ASCII，菜单/HUD 用）
const char *item_name(uint8_t id);

#endif
