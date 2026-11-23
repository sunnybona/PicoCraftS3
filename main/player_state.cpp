#include "player_state.h"
#include <string.h>

const uint8_t HAND_BAR[] = {
    ITEM_GRASS, ITEM_DIRT, ITEM_STONE, ITEM_LOG, ITEM_LEAVES, ITEM_SAND,
    ITEM_PLANK, ITEM_COBBLE, ITEM_BRICK, ITEM_GLASS, ITEM_SNOW, ITEM_GOLD,
    ITEM_COAL_ORE, ITEM_IRON_ORE, ITEM_DIAMOND_ORE,
    ITEM_CRAFT_TABLE, ITEM_TORCH, ITEM_TNT,
    ITEM_WOOD_PICK, ITEM_STONE_PICK, ITEM_IRON_PICK, ITEM_DIAMOND_PICK
};
const int HAND_BAR_N = (int)(sizeof(HAND_BAR) / sizeof(HAND_BAR[0]));

PlayerState player;

PlayerState::PlayerState() {
    reset_new();
}

void PlayerState::reset_new() {
    memset(inv, 0, sizeof(inv));
    hp = PLAYER_MAX_HP;
    day_time = 200;             // 从早晨开始
    achievements = 0;
    hand_index = 0;
    hurt_cooldown = 0;
    toast_time = 0;
    toast[0] = 0;
    respawn_pending = false;
    cam_x = cam_y = cam_z = 0.0f;
    cam_yaw = cam_pitch = 0.0f;
    have_cam_pos = false;
}

uint8_t PlayerState::held_item() const {
    if (hand_index < 0 || hand_index >= HAND_BAR_N) return ITEM_AIR;
    return HAND_BAR[hand_index];
}

void PlayerState::add(uint8_t item, uint16_t n) {
    if (item >= ITEM_ID_COUNT || n == 0) return;
    uint32_t v = (uint32_t)inv[item] + n;
    inv[item] = (v > 65535u) ? 65535u : (uint16_t)v;

    if (item == ITEM_COAL) unlock(ACH_COAL);
    else if (item == ITEM_IRON_INGOT) unlock(ACH_IRON);
    else if (item == ITEM_DIAMOND) unlock(ACH_DIA);
}

uint16_t PlayerState::count(uint8_t item) const {
    if (item >= ITEM_ID_COUNT) return 0;
    return inv[item];
}

bool PlayerState::consume(uint8_t item, uint16_t n) {
    if (item >= ITEM_ID_COUNT) return false;
    if (inv[item] < n) return false;
    inv[item] = (uint16_t)(inv[item] - n);
    return true;
}

void PlayerState::show_toast(const char *msg) {
    if (!msg) return;
    strncpy(toast, msg, sizeof(toast) - 1);
    toast[sizeof(toast) - 1] = 0;
    toast_time = TOAST_TICKS;
}

void PlayerState::unlock(uint32_t bit) {
    if ((achievements & bit) == 0) {
        achievements |= bit;
    }
}

bool PlayerState::has(uint32_t bit) const {
    return (achievements & bit) != 0;
}
