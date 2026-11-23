#include "items.h"

// 挖掘工作量基数（按方块 id）
static const uint8_t HARDNESS[BLOCK_TYPE_COUNT] = {
    /*AIR*/0, /*GRASS*/1, /*DIRT*/1, /*STONE*/3, /*LOG*/2,
    /*LEAVES*/1, /*SAND*/1, /*PLANK*/2, /*WATER*/0, /*COBBLE*/3,
    /*BRICK*/3, /*GLASS*/1, /*BEDROCK*/255, /*SNOW*/1, /*GOLD*/3,
    /*COAL_ORE*/3, /*IRON_ORE*/4, /*DIAMOND_ORE*/5,
    /*CRAFT_TABLE*/2, /*TORCH*/1, /*TNT*/2,
};

// 挖掘所需最低工具等级（按方块 id）
static const uint8_t REQUIRED_TOOL[BLOCK_TYPE_COUNT] = {
    0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,
    /*COAL_ORE*/1, /*IRON_ORE*/2, /*DIAMOND_ORE*/3,
    0,0,0,
};

// 方块掉落物（按方块 id）；0 = 无掉落
static const uint8_t DROP[BLOCK_TYPE_COUNT] = {
    /*AIR*/0, /*GRASS*/ITEM_DIRT, /*DIRT*/ITEM_DIRT, /*STONE*/ITEM_COBBLE,
    /*LOG*/ITEM_LOG, /*LEAVES*/ITEM_LEAVES, /*SAND*/ITEM_SAND, /*PLANK*/ITEM_PLANK,
    /*WATER*/0, /*COBBLE*/ITEM_COBBLE, /*BRICK*/ITEM_BRICK, /*GLASS*/ITEM_GLASS,
    /*BEDROCK*/0, /*SNOW*/ITEM_SNOW, /*GOLD*/ITEM_GOLD,
    /*COAL_ORE*/ITEM_COAL, /*IRON_ORE*/ITEM_IRON_INGOT, /*DIAMOND_ORE*/ITEM_DIAMOND,
    /*CRAFT_TABLE*/ITEM_CRAFT_TABLE, /*TORCH*/ITEM_TORCH, /*TNT*/ITEM_TNT,
};

static const char *NAMES[ITEM_ID_COUNT] = {
    "AIR","GRASS","DIRT","STONE","LOG","LEAF","SAND","PLANK","WATER",
    "COBBLE","BRICK","GLASS","ROCK","SNOW","GOLD","COAL.O","IRON.O","DIA.O",
    "CRAFT","TORCH","TNT","STICK","COAL","IRON","DIA",
    "W.PICK","S.PICK","I.PICK","D.PICK",
};

uint8_t item_tool_level(uint8_t id) {
    switch (id) {
        case ITEM_WOOD_PICK:  return TOOL_WOOD;
        case ITEM_STONE_PICK: return TOOL_STONE;
        case ITEM_IRON_PICK:  return TOOL_IRON;
        case ITEM_DIAMOND_PICK: return TOOL_DIA;
        default: return TOOL_HAND;
    }
}

uint8_t item_pick_power(uint8_t id) {
    switch (id) {
        case ITEM_WOOD_PICK:    return 2;
        case ITEM_STONE_PICK:   return 3;
        case ITEM_IRON_PICK:    return 4;
        case ITEM_DIAMOND_PICK: return 6;
        default: return 1;   // 徒手
    }
}

bool item_is_placeable(uint8_t id) {
    return id < BLOCK_TYPE_COUNT && id != ITEM_AIR && id != ITEM_WATER;
}

bool item_is_tool(uint8_t id) {
    return id >= ITEM_WOOD_PICK && id <= ITEM_DIAMOND_PICK;
}

uint8_t block_drop_item(uint8_t block) {
    if (block >= BLOCK_TYPE_COUNT) return 0;
    return DROP[block];
}

uint8_t block_required_tool(uint8_t block) {
    if (block >= BLOCK_TYPE_COUNT) return 0;
    return REQUIRED_TOOL[block];
}

uint8_t block_hardness(uint8_t block) {
    if (block >= BLOCK_TYPE_COUNT) return 0;
    return HARDNESS[block];
}

bool block_is_emissive(uint8_t block) {
    return block == BLK_TORCH;
}

const char *item_name(uint8_t id) {
    if (id >= ITEM_ID_COUNT) return "?";
    return NAMES[id];
}
