#include "crafting.h"
#include "player_state.h"
#include "items.h"

static const Recipe RECIPES[] = {
    // 1 原木 -> 4 木板
    { ITEM_PLANK, 4, { {ITEM_LOG,1}, {0,0}, {0,0}, {0,0} } },
    // 2 木板 -> 4 木棍
    { ITEM_STICK, 4, { {ITEM_PLANK,2}, {0,0}, {0,0}, {0,0} } },
    // 4 木板 + 2 木棍 -> 工作台
    { ITEM_CRAFT_TABLE, 1, { {ITEM_PLANK,4}, {ITEM_STICK,2}, {0,0}, {0,0} } },
    // 3 木板 + 2 木棍 -> 木镐
    { ITEM_WOOD_PICK, 1, { {ITEM_PLANK,3}, {ITEM_STICK,2}, {0,0}, {0,0} } },
    // 3 圆石 + 2 木棍 -> 石镐
    { ITEM_STONE_PICK, 1, { {ITEM_COBBLE,3}, {ITEM_STICK,2}, {0,0}, {0,0} } },
    // 3 铁锭 + 2 木棍 -> 铁镐
    { ITEM_IRON_PICK, 1, { {ITEM_IRON_INGOT,3}, {ITEM_STICK,2}, {0,0}, {0,0} } },
    // 3 钻石 + 2 木棍 -> 钻石镐
    { ITEM_DIAMOND_PICK, 1, { {ITEM_DIAMOND,3}, {ITEM_STICK,2}, {0,0}, {0,0} } },
    // 1 煤炭 + 1 木棍 -> 4 火炬
    { ITEM_TORCH, 4, { {ITEM_COAL,1}, {ITEM_STICK,1}, {0,0}, {0,0} } },
    // 5 沙子 + 2 煤炭 -> TNT
    { ITEM_TNT, 1, { {ITEM_SAND,5}, {ITEM_COAL,2}, {0,0}, {0,0} } },
};

int recipe_count() { return (int)(sizeof(RECIPES) / sizeof(RECIPES[0])); }

const Recipe &recipe_at(int i) { return RECIPES[i]; }

bool can_craft(int i) {
    if (i < 0 || i >= recipe_count()) return false;
    const Recipe &r = RECIPES[i];
    for (int k = 0; k < MAX_ING; k++) {
        if (r.ing[k].item == 0) break;
        if (player.count(r.ing[k].item) < r.ing[k].n) return false;
    }
    return true;
}

bool do_craft(int i) {
    if (!can_craft(i)) return false;
    const Recipe &r = RECIPES[i];
    for (int k = 0; k < MAX_ING; k++) {
        if (r.ing[k].item == 0) break;
        player.consume(r.ing[k].item, r.ing[k].n);
    }
    player.add(r.out, r.out_n);
    if (item_is_tool(r.out)) player.unlock(ACH_CRAFT);
    return true;
}
