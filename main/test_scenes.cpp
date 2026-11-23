#include "test_scenes.h"
#include "game_menu.h"
#include "voxel/voxel_world.h"
#include "voxel/player_physics.h"
#include "engine/render_globals.h"
#include "player_state.h"
#include "items.h"
#include "item_drops.h"
#include "tnt.h"
#include <math.h>
#include <cstdlib>

// Globals defined in main.cpp
extern VoxelWorld world;
extern PlayerPhysics physics;
extern ItemDrops item_drops;
extern TntSystem tnt_sys;

static const float EYE = 1.62f;
static const uint32_t SCENE_SEED = 1337u;

static inline void sb(int x, int y, int z, uint8_t b) {
    world.set_block(x, y, z, b);
}

// Load the 3x3 chunks around a centre (each update builds one chunk).
static void prepare_area(int cx, int cz) {
    for (int i = 0; i < 80; i++) world.update((float)cx, (float)cz);
}

static void place_player(int wx, int wz, int gy) {
    camera_position[0] = (float)wx;
    camera_position[2] = (float)wz;
    camera_position[1] = (float)gy + EYE;
    yaw = 0.0f;
    pitch = 0.0f;
    update_camera();
    physics.reset();
    physics.on_ground = true;
}

// A small cottage: (2*half+1) footprint, log corner posts, given wall material,
// glass band under the roof, plank roof, a door on the south (+z) side.
static void build_cottage(int cx, int cz, int gy, int half, uint8_t wall) {
    for (int x = -half; x <= half; x++) {
        for (int z = -half; z <= half; z++) {
            bool edge = (x == -half || x == half || z == -half || z == half);
            bool corner = (abs(x) == half && abs(z) == half);
            if (!edge) continue;
            for (int y = gy + 1; y <= gy + 3; y++) {
                if (z == half && x == 0 && (y == gy + 1 || y == gy + 2)) continue; // door
                if (y == gy + 3 && !corner) { sb(cx + x, y, cz + z, BLK_GLASS); continue; }
                sb(cx + x, y, cz + z, corner ? (uint8_t)BLK_LOG : wall);
            }
            sb(cx + x, gy + 4, cz + z, BLK_PLANK);   // roof
        }
    }
}

// ---------------- 1. Plains starter camp ----------------
static void scene_fresh() {
    prepare_area(0, 0);
    int gy = world.surface_y(0, 0);
    if (gy < 0) gy = 5;

    // Four corner torches around the camp
    static const int t[4][2] = {{2,2},{-2,2},{2,-2},{-2,-2}};
    for (int i = 0; i < 4; i++) sb(t[i][0], gy + 1, t[i][1], BLK_TORCH);

    player.add(ITEM_WOOD_PICK, 1);
    player.add(ITEM_TORCH, 5);
    player.add(ITEM_DIRT, 8);
    place_player(0, 0, gy);
}

// ---------------- 2. Forest workshop cottage ----------------
static void scene_craft() {
    const int cx = -36, cz = 20;
    prepare_area(cx, cz);
    int gy = world.surface_y(cx, cz);
    if (gy < 0) gy = 5;

    build_cottage(cx, cz, gy, 2, BLK_PLANK);
    // Workbench and interior lights
    sb(cx, gy + 1, cz, BLK_CRAFT_TABLE);
    sb(cx - 1, gy + 1, cz - 1, BLK_TORCH);
    sb(cx + 1, gy + 1, cz + 1, BLK_TORCH);

    player.add(ITEM_LOG, 20);
    player.add(ITEM_PLANK, 20);
    player.add(ITEM_STICK, 20);
    player.add(ITEM_COAL, 20);
    player.add(ITEM_SAND, 20);
    player.add(ITEM_COBBLE, 20);
    player.add(ITEM_IRON_INGOT, 12);
    player.add(ITEM_DIAMOND, 6);
    player.add(ITEM_CRAFT_TABLE, 1);
    place_player(cx, cz + 1, gy);
}

// ---------------- 3. Hand-built cave mine ----------------
static void scene_mine() {
    const int cx = 34, cz = -28;
    prepare_area(cx, cz);
    int gy = world.surface_y(cx, cz);
    if (gy < 4) gy = 6;

    // Hollow out an underground room (5x5, three levels)
    for (int x = -2; x <= 2; x++) {
        for (int z = -2; z <= 2; z++) {
            for (int y = gy - 3; y <= gy - 1; y++) sb(cx + x, y, cz + z, BLK_AIR);
        }
    }
    // A vertical entry shaft on the south side up to the surface
    for (int y = gy; y <= gy + 1; y++) sb(cx, y, cz + 2, BLK_AIR);
    sb(cx, gy + 1, cz + 2, BLK_TORCH);

    // Ore veins on the walls by height
    for (int x = -2; x <= 2; x++) {
        sb(cx + x, gy - 3, cz - 2, BLK_COAL_ORE);
        sb(cx + x, gy - 2, cz - 2, BLK_IRON_ORE);
        sb(cx + x, gy - 1, cz - 2, BLK_DIAMOND_ORE);
    }
    // Wall torches
    sb(cx - 2, gy - 1, cz, BLK_TORCH);
    sb(cx + 2, gy - 1, cz, BLK_TORCH);

    player.add(ITEM_WOOD_PICK, 1);
    player.add(ITEM_STONE_PICK, 1);
    player.add(ITEM_IRON_PICK, 1);
    player.add(ITEM_DIAMOND_PICK, 1);
    player.add(ITEM_TORCH, 20);
    // Player stands inside the room (feet at gy-2, floor gy-3)
    camera_position[0] = (float)cx;
    camera_position[2] = (float)cz;
    camera_position[1] = (float)(gy - 2) + EYE;
    yaw = 0.0f; pitch = 0.0f;
    update_camera();
    physics.reset();
    physics.on_ground = true;
}

// ---------------- 4. Night wilderness fort ----------------
static void scene_combat() {
    const int cx = 48, cz = 40;
    prepare_area(cx, cz);
    int gy = world.surface_y(cx, cz);
    if (gy < 0) gy = 5;

    // One-block low wall around a 9x9 fort with a south gap
    for (int x = -4; x <= 4; x++) {
        for (int z = -4; z <= 4; z++) {
            bool edge = (x == -4 || x == 4 || z == -4 || z == 4);
            if (!edge) continue;
            if (z == 4 && (x == 0 || x == 1)) continue;   // entrance gap
            sb(cx + x, gy + 1, cz + z, BLK_COBBLE);
        }
    }
    // Corner torches
    static const int t[4][2] = {{-3,-3},{3,-3},{-3,3},{3,3}};
    for (int i = 0; i < 4; i++) sb(cx + t[i][0], gy + 1, cz + t[i][1], BLK_TORCH);

    player.day_time = 1600;          // night
    player.add(ITEM_STONE_PICK, 1);
    player.add(ITEM_IRON_PICK, 1);
    player.add(ITEM_TORCH, 10);
    place_player(cx, cz, gy);
}

// ---------------- 5. Blasting quarry ----------------
static void scene_tnt() {
    const int cx = -44, cz = -40;
    prepare_area(cx, cz);
    int gy = world.surface_y(cx, cz);
    if (gy < 0) gy = 5;

    // TNT row ahead (-z)
    for (int x = -2; x <= 2; x++) sb(cx + x, gy + 1, cz - 4, BLK_TNT);
    // A brick cover wall between the player and the charge
    for (int x = -2; x <= 2; x++) {
        for (int y = gy + 1; y <= gy + 3; y++) sb(cx + x, y, cz - 1, BLK_BRICK);
    }
    sb(cx, gy + 1, cz + 2, BLK_TORCH);

    player.add(ITEM_TNT, 16);
    player.add(ITEM_DIRT, 16);
    player.add(ITEM_COBBLE, 16);
    player.add(ITEM_BRICK, 16);
    player.add(ITEM_TORCH, 10);
    place_player(cx, cz + 2, gy);
}

// ---------------- 6. Great hall showing every block ----------------
static void scene_all() {
    const int cx = 20, cz = 44;
    prepare_area(cx, cz);
    int gy = world.surface_y(cx, cz);
    if (gy < 0) gy = 5;

    build_cottage(cx, cz, gy, 3, BLK_BRICK);   // 7x7 brick hall
    // Display pedestals along the inner walls: one of each placeable block
    int n = 0;
    for (uint8_t b = 1; b < BLOCK_TYPE_COUNT; b++) {
        if (b == BLK_WATER || b == BLK_TORCH) continue;
        int slot = n++;
        int px = cx - 2 + (slot % 5);
        int pz = cz - 2 + (slot / 5);
        sb(px, gy + 1, pz, b);
    }
    sb(cx, gy + 1, cz + 1, BLK_TORCH);
    sb(cx, gy + 1, cz - 1, BLK_TORCH);

    for (uint8_t b = 1; b < BLOCK_TYPE_COUNT; b++) {
        if (b == BLK_WATER) continue;
        player.add(b, 8);
    }
    player.add(ITEM_STICK, 8);
    player.add(ITEM_COAL, 8);
    player.add(ITEM_IRON_INGOT, 8);
    player.add(ITEM_DIAMOND, 8);
    player.add(ITEM_WOOD_PICK, 1);
    player.add(ITEM_STONE_PICK, 1);
    player.add(ITEM_IRON_PICK, 1);
    player.add(ITEM_DIAMOND_PICK, 1);
    place_player(cx, cz + 1, gy);
}

// ---------------- 7. Lookout tower ----------------
static void scene_tower() {
    const int cx = -20, cz = -48;
    prepare_area(cx, cz);
    int gy = world.surface_y(cx, cz);
    if (gy < 0) gy = 5;

    // 5x5 cobble outer walls, seven levels
    for (int x = -2; x <= 2; x++) {
        for (int z = -2; z <= 2; z++) {
            bool edge = (x == -2 || x == 2 || z == -2 || z == 2);
            if (!edge) continue;
            for (int y = gy + 1; y <= gy + 7; y++) {
                if (z == 2 && x == 0 && (y == gy + 1 || y == gy + 2)) continue; // door
                sb(cx + x, y, cz + z, BLK_COBBLE);
            }
        }
    }
    // Alternating internal plank ledges to jump up
    for (int y = gy + 1; y <= gy + 6; y++) {
        int px = ((y - gy) % 2) ? -1 : 1;
        sb(cx + px, y, cz, BLK_PLANK);
    }
    // Battlements (alternating) and a top torch
    for (int x = -2; x <= 2; x++) {
        for (int z = -2; z <= 2; z++) {
            bool edge = (abs(x) == 2 || abs(z) == 2);
            if (edge && ((x + z) & 1) == 0) sb(cx + x, gy + 8, cz + z, BLK_COBBLE);
        }
    }
    sb(cx, gy + 8, cz, BLK_TORCH);

    player.add(ITEM_TORCH, 10);
    place_player(cx, cz + 1, gy);
}

// ---------------- 8. Lakeside cottage ----------------
static bool find_lakeside(int cx, int cz, int &ox, int &oz, int &ogy) {
    static const int nb[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
    for (int dz = -7; dz <= 7; dz++) {
        for (int dx = -7; dx <= 7; dx++) {
            int x = cx + dx, z = cz + dz;
            int g = world.surface_y(x, z);
            if (g < 0) continue;
            if (world.get_block(x, g, z) != BLK_SAND) continue;
            bool water = false;
            for (int k = 0; k < 4; k++) {
                int nx = x + nb[k][0], nz = z + nb[k][1];
                if (world.get_block(nx, g, nz) == BLK_WATER ||
                    world.get_block(nx, g + 1, nz) == BLK_WATER) { water = true; break; }
            }
            if (water) { ox = x; oz = z; ogy = g; return true; }
        }
    }
    return false;
}

static void scene_lake() {
    const int cx = 40, cz = -44;
    prepare_area(cx, cz);
    int hx = cx, hz = cz, gy = world.surface_y(cx, cz);
    if (gy < 0) gy = 4;
    find_lakeside(cx, cz, hx, hz, gy);

    build_cottage(hx, hz, gy, 2, BLK_PLANK);
    sb(hx, gy + 1, hz, BLK_CRAFT_TABLE);
    sb(hx - 1, gy + 1, hz + 1, BLK_TORCH);
    sb(hx + 1, gy + 1, hz - 1, BLK_TORCH);

    player.add(ITEM_SAND, 16);
    player.add(ITEM_COBBLE, 16);
    player.add(ITEM_TORCH, 10);
    player.add(ITEM_WOOD_PICK, 1);
    place_player(hx, hz + 1, gy);
}

// ---------------- dispatch ----------------
void enter_test_scene(int t) {
    sky = (color_t)((7 << 12) | (11 << 8) | 13);
    world.generate(SCENE_SEED);
    player.reset_new();

    switch (t) {
        case TEST_FRESH:  scene_fresh();  break;
        case TEST_CRAFT:  scene_craft();  break;
        case TEST_MINE:   scene_mine();   break;
        case TEST_COMBAT: scene_combat(); break;
        case TEST_TNT:    scene_tnt();    break;
        case TEST_ALL:    scene_all();    break;
        case TEST_TOWER:  scene_tower();  break;
        case TEST_LAKE:   scene_lake();   break;
        default:          scene_fresh();  break;
    }

    item_drops.clear();
    tnt_sys.clear();
    game_menu.mode = MODE_PLAY;
}
