#include "picosystem.hpp"
#include "math.h"

using namespace picosystem;

#include "engine/render_globals.h"
#include "engine/render_math.h"
#include "voxel/voxel_world.h"
#include "voxel/voxel_mesh.h"
#include "voxel/voxel_ray.h"
#include "voxel/player_physics.h"
#include "voxel/save_store.h"
#include "voxel/block_textures.h"
#include "voxel/terrain_skyline.h"
#include "voxel/entities.h"
#include "items.h"
#include "player_state.h"
#include "item_drops.h"
#include "crafting.h"
#include "day_night.h"
#include "tnt.h"
#include "game_menu.h"
#include "test_scenes.h"

// ---------------- 世界与玩家参数 ----------------
static const uint32_t WORLD_SEED = 1337u;
static const int RENDER_RADIUS = 11;
static const float EYE_HEIGHT = 1.62f;
static const float TURN_STEP = 0.07f;
static const float LOOK_STEP = 0.05f;
static const float MOVE_SPEED = 0.16f;
static const float PITCH_LIMIT = 1.3f;
static const float REACH = 8.0f;

VoxelWorld world;
VoxelMesh mesh;
PlayerPhysics physics;
EntitySystem entities;
ItemDrops item_drops;
TntSystem tnt_sys;

// 当前注视目标
RayHit current_target;

// 挖掘进度（按住 A 累积）
static int dig_progress = 0;
static int dig_bx = -999, dig_by = 0, dig_bz = 0;
static int pick_swing = 0;
#define PICK_SWING_TICKS 12

// 放置计数（成就）
static int blocks_placed = 0;

// 合成菜单
static bool menu_open = false;
static int menu_sel = 0;

// flash 保存防抖
static bool world_dirty = false;
static int save_countdown = 0;
#define SAVE_DELAY_TICKS 90

// 当前关联的真实存档槽；-1 = 测试场景（无关联槽，不自动保存）
static int current_slot = -1;

// ESP32-S3 单线程：同步光栅化
int32_t render_sync() {
    uint32_t t = time_us();
    render_rasterize(number_triangles, SCREEN->data);
    t = time_us() - t;
    return (int32_t)(t + 1);
}

// 把相机状态同步到 player 存档字段（save_slot 读取）
static void sync_cam_to_player() {
    player.cam_x = camera_position[0];
    player.cam_y = camera_position[1];
    player.cam_z = camera_position[2];
    player.cam_yaw = yaw;
    player.cam_pitch = pitch;
}

// ---------------- 输入与移动 ----------------
static void handle_input(uint32_t tick) {
    if (button(LEFT))  yaw += TURN_STEP;
    if (button(RIGHT)) yaw -= TURN_STEP;

    if (pressed(X)) physics.request_jump();

    // B 修饰键状态机
    static bool b_held = false;
    static uint32_t b_start = 0;
    static bool b_long_fired = false;
    static bool b_look_used = false;
    #define B_LONG_TICKS 18

    bool b_down = button(B);
    if (!b_held && b_down) {
        b_held = true; b_start = tick;
        b_long_fired = false; b_look_used = false;
    } else if (b_held && b_down) {
        if (button(UP))   { pitch += LOOK_STEP; b_look_used = true; }
        if (button(DOWN)) { pitch -= LOOK_STEP; b_look_used = true; }
        if (!b_long_fired && !b_look_used && (tick - b_start) >= B_LONG_TICKS) {
            b_long_fired = true;
            player.hand_index = (player.hand_index + 1) % HAND_BAR_N;
        }
    } else if (b_held && !b_down) {
        b_held = false;
        if (!b_long_fired && !b_look_used) menu_open = true;   // 短按 B 打开合成菜单
    }
    if (pitch >  PITCH_LIMIT) pitch =  PITCH_LIMIT;
    if (pitch < -PITCH_LIMIT) pitch = -PITCH_LIMIT;

    float move = 0.0f;
    if (!b_held) {
        if (button(UP))   move += MOVE_SPEED;
        if (button(DOWN)) move -= MOVE_SPEED;
    }
    float wish_x = -sinf(yaw) * move;
    float wish_z = -cosf(yaw) * move;
    physics.step(world, wish_x, wish_z);

    update_camera();
}

// ---------------- 合成菜单逻辑 ----------------
static void handle_menu() {
    if (pressed(UP))   menu_sel = (menu_sel - 1 + recipe_count()) % recipe_count();
    if (pressed(DOWN)) menu_sel = (menu_sel + 1) % recipe_count();
    if (pressed(B))    menu_open = false;
    if (pressed(A)) {
        if (can_craft(menu_sel)) {
            do_craft(menu_sel);
            player.show_toast("MADE ITEM");
        } else {
            player.show_toast("NEED MORE");
        }
    }
}

// ---------------- 重生 ----------------
static void do_respawn() {
    player.hp = PLAYER_MAX_HP;
    player.day_time = 200;
    player.respawn_pending = false;
    player.hurt_cooldown = 0;
    for (int i = 0; i < CHUNK_POOL_COUNT; i++) world.update(0.0f, 0.0f);
    int g = world.surface_y(0, 0);
    camera_position[0] = 0.0f;
    camera_position[2] = 0.0f;
    camera_position[1] = (g >= 0 ? (float)g + EYE_HEIGHT : EYE_HEIGHT);
    yaw = 0.0f; pitch = 0.0f;
    update_camera();
    physics.reset();
    physics.on_ground = true;
    item_drops.clear();
    tnt_sys.clear();
    player.show_toast("RESPAWNED");
}

// ---------------- 进入世界的公共辅助 ----------------
static void load_chunks_around(int wx, int wz) {
    for (int i = 0; i < CHUNK_POOL_COUNT * 2; i++)
        world.update((float)wx, (float)wz);
}

static void reset_transient() {
    dig_progress = 0;
    dig_bx = -999;
    pick_swing = 0;
    blocks_placed = 0;
    menu_open = false;
    menu_sel = 0;
    world_dirty = false;
    save_countdown = 0;
    item_drops.clear();
    tnt_sys.clear();
}

// 从真实存档槽进入
static void start_slot(int slot) {
    sky = (color_t)((7 << 12) | (11 << 8) | 13);

    world.generate(WORLD_SEED);
    bool restored = false;
    SaveStore::load_slot(slot, world, &restored);

    if (!restored) {
        player.add(ITEM_WOOD_PICK, 1);
        player.add(ITEM_TORCH, 5);
        player.add(ITEM_DIRT, 8);
        player.hand_index = 0;
    }

    if (restored && player.have_cam_pos) {
        load_chunks_around((int)player.cam_x, (int)player.cam_z);
        camera_position[0] = player.cam_x;
        camera_position[1] = player.cam_y;
        camera_position[2] = player.cam_z;
        yaw = player.cam_yaw;
        pitch = player.cam_pitch;
    } else {
        load_chunks_around(0, 0);
        int g = world.surface_y(0, 0);
        camera_position[0] = 0.0f;
        camera_position[2] = 0.0f;
        camera_position[1] = (g >= 0 ? (float)g + EYE_HEIGHT : EYE_HEIGHT);
        yaw = 0.0f; pitch = 0.0f;
    }

    update_camera();
    physics.reset();
    physics.on_ground = true;
    reset_transient();

    current_slot = slot;
    game_menu.mode = MODE_PLAY;
}

// 程序化测试场景见 test_scenes.cpp（enter_test_scene）

// ---------------- 生命周期 ----------------
void init() {
    sky = (color_t)((7 << 12) | (11 << 8) | 13);
    init_block_textures();

    camera_position[0] = 0.0f;
    camera_position[1] = EYE_HEIGHT;
    camera_position[2] = 0.0f;
    yaw = 0.0f; pitch = 0.0f;
    update_camera();

    game_menu.mode = MODE_TITLE;
    game_menu.title_sel = 0;
}

// ---------------- 交互：攻击 / 挖掘 / 放置 / 点燃 ----------------
static void handle_interaction() {
    float cp = cosf(pitch);
    float dx = -sinf(yaw) * cp;
    float dy = sinf(pitch);
    float dz = -cosf(yaw) * cp;

    current_target = voxel_raycast(world,
        camera_position[0], camera_position[1], camera_position[2],
        dx, dy, dz, REACH);

    uint8_t held = player.held_item();
    bool press_a = pressed(A);

    // 1) 攻击怪物（边沿）
    if (press_a) {
        uint8_t dmg = item_is_tool(held)
                      ? (uint8_t)(item_tool_level(held) + 1) : 2u;
        AttackResult ar = entities.attack_entities(
            camera_position[0], camera_position[1], camera_position[2], yaw, dmg);
        if (ar.hit) {
            if (ar.killed) {
                item_drops.spawn(ar.kx, ar.ky, ar.kz, ar.drop);
                player.unlock(ACH_KILL);
                player.show_toast("ZOMBIE DOWN");
            }
            return;   // 本帧攻击命中，不挖掘
        }
    }

    // 2) 按住 A 挖掘 / 点燃 TNT
    if (button(A) && current_target.hit) {
        int tb = world.get_block(current_target.bx, current_target.by, current_target.bz);

        if (tb == BLK_TNT) {
            if (press_a) {
                world.set_block(current_target.bx, current_target.by, current_target.bz, BLK_AIR);
                tnt_sys.ignite((float)current_target.bx + 0.5f,
                               (float)current_target.by + 0.5f,
                               (float)current_target.bz + 0.5f);
                world_dirty = true;
            }
            return;
        }
        if (tb == BLK_BEDROCK) { dig_progress = 0; return; }

        uint8_t need_tool = block_required_tool((uint8_t)tb);
        uint8_t level = item_tool_level(held);
        if (level < need_tool) {
            dig_progress = 0;
            if (press_a) player.show_toast("NEED BETTER PICK");
            return;
        }

        if (dig_bx != current_target.bx || dig_by != current_target.by ||
            dig_bz != current_target.bz) {
            dig_progress = 0;
            dig_bx = current_target.bx; dig_by = current_target.by; dig_bz = current_target.bz;
        }
        dig_progress += item_pick_power(held);
        int need = (int)block_hardness((uint8_t)tb) * 4;
        if (dig_progress >= need) {
            world.set_block(current_target.bx, current_target.by, current_target.bz, BLK_AIR);
            uint8_t drop = block_drop_item((uint8_t)tb);
            if (drop != 0)
                item_drops.spawn((float)current_target.bx + 0.5f,
                                 (float)current_target.by + 0.5f,
                                 (float)current_target.bz + 0.5f, drop);
            pick_swing = PICK_SWING_TICKS;
            world_dirty = true;
            dig_progress = 0;
        }
    } else if (!button(A)) {
        dig_progress = 0;
    }

    // 3) 放置（Y 边沿，消耗库存）
    if (pressed(Y)) {
        if (!item_is_placeable(held)) return;
        int px = current_target.px, py = current_target.py, pz = current_target.pz;
        if (current_target.hit && world.get_block(px, py, pz) == BLK_AIR) {
            int eye_x = (int)floorf(camera_position[0]);
            int eye_y = (int)floorf(camera_position[1]);
            int eye_z = (int)floorf(camera_position[2]);
            bool on_player = (px == eye_x && pz == eye_z &&
                              (py == eye_y || py == eye_y - 1));
            if (!on_player && player.count(held) > 0) {
                world.set_block(px, py, pz, held);
                player.consume(held, 1);
                world_dirty = true;
                blocks_placed++;
                if (blocks_placed >= 10) player.unlock(ACH_BUILD);
            }
        }
    }
}

// 暂停菜单触发的保存
static void do_manual_save() {
    sync_cam_to_player();
    int slot = current_slot;
    if (slot < 0) { slot = 0; current_slot = 0; }
    if (SaveStore::save_slot(slot, world))
        player.show_toast("SAVED");
    else
        player.show_toast("SAVE FAIL");
}

// X+Y 同时按下的边沿（进入暂停）
static bool pause_combo_edge() {
    static bool was = false;
    bool now = button(X) && button(Y);
    bool edge = now && !was;
    was = now;
    return edge;
}

void update(uint32_t tick) {
    // ===== 标题模式 =====
    if (game_menu.mode == MODE_TITLE) {
        TitleEntryType tt;
        int id = -1;
        if (game_menu.title_update(tt, id)) {
            if (tt == TENTRY_SLOT) start_slot(id);
            else { enter_test_scene(id); current_slot = -1; }
        }
        return;
    }

    // ===== 暂停模式 =====
    if (game_menu.mode == MODE_PAUSE) {
        PauseAction a = game_menu.pause_update();
        if (a == PAUSE_RESUME) {
            game_menu.mode = MODE_PLAY;
        } else if (a == PAUSE_SAVE) {
            do_manual_save();
        } else if (a == PAUSE_QUIT) {
            game_menu.mode = MODE_TITLE;
        }
        return;
    }

    // ===== 游戏模式 =====
    if (pause_combo_edge()) {
        game_menu.mode = MODE_PAUSE;
        game_menu.pause_sel = 0;
        return;
    }

    if (player.hurt_cooldown > 0) player.hurt_cooldown--;
    if (player.toast_time > 0) player.toast_time--;
    if (pick_swing > 0) pick_swing--;

    if (menu_open) {
        handle_menu();
        return;
    }
    if (player.respawn_pending) do_respawn();

    handle_input(tick);

    world.update(camera_position[0], camera_position[2]);
    entities.update(world, WORLD_SEED,
                    camera_position[0], camera_position[2], tick);
    item_drops.update(world,
                      camera_position[0], camera_position[1], camera_position[2]);
    tnt_sys.update(world,
                   camera_position[0], camera_position[1], camera_position[2]);

    update_day_night(world,
                     camera_position[0], camera_position[1], camera_position[2]);

    render_view_projection();
    handle_interaction();

    number_triangles = 0;
    mesh.build(world, camera_position[0], camera_position[2], RENDER_RADIUS);
    entities.render(camera_position[0], camera_position[2]);
    item_drops.render(camera_position[0], camera_position[2]);
    tnt_sys.render(camera_position[0], camera_position[2]);

    // 自动保存仅在真实存档槽；测试场景需手动保存
    sync_cam_to_player();
    if (current_slot >= 0 && world_dirty) {
        if (save_countdown <= 0) save_countdown = SAVE_DELAY_TICKS;
        save_countdown--;
        if (save_countdown == 0) {
            SaveStore::save_slot(current_slot, world);
            world_dirty = false;
        }
    }
}

// ---------------- 高亮 ----------------
static void draw_highlight() {
    if (!current_target.hit) return;
    const int c[8][3] = {
        {0,0,0},{1,0,0},{1,1,0},{0,1,0},
        {0,0,1},{1,0,1},{1,1,1},{0,1,1}
    };
    const int edges[12][2] = {
        {0,1},{1,5},{5,4},{4,0},
        {2,3},{3,7},{7,6},{6,2},
        {0,3},{1,2},{5,6},{4,7}
    };
    int sx[8], sy[8]; bool ok[8];
    for (int i = 0; i < 8; i++)
        ok[i] = world_to_screen((float)(current_target.bx + c[i][0]),
                                (float)(current_target.by + c[i][1]),
                                (float)(current_target.bz + c[i][2]),
                                sx[i], sy[i]);
    pen(15, 15, 15);
    for (int e = 0; e < 12; e++)
        if (ok[edges[e][0]] && ok[edges[e][1]])
            line(sx[edges[e][0]], sy[edges[e][0]],
                 sx[edges[e][1]], sy[edges[e][1]]);
}

// ---------------- 镐头（纤细版：头部逆时针转 45° 与杆垂直，1 格宽线条，单元 2x2） ----------------
static const char PICK_BMP[11][12] = {
    "...........",
    "...........",
    "........p..",
    ".......p...",
    "......p....",
    ".....p.....",
    "....p.h....",
    "...p...h...",
    "..p.....h..",
    ".........h.",
    "..........h",
};
static const int PICK_SCALE = 2;

static void draw_pickaxe() {
    const float base_x = (float)(SCREEN_WIDTH - 14);
    const float base_y = (float)(SCREEN_HEIGHT - 13);
    const float pivotx = 5.0f, pivoty = 5.0f;

    float phase = 0.0f;
    if (pick_swing > 0) {
        float t = 1.0f - (float)pick_swing / (float)PICK_SWING_TICKS;
        phase = sinf(t * PI);
    }
    float ang = -1.05f * phase;
    float ca = cosf(ang), sa = sinf(ang);
    float oy = 3.0f * phase;

    for (int yy = 0; yy < 11; yy++) {
        for (int xx = 0; xx < 11; xx++) {
            char ch = PICK_BMP[yy][xx];
            if (ch == 'p')      pen(5, 3, 2);
            else if (ch == 'h') pen(9, 6, 3);
            else continue;
            float lx = ((float)xx - pivotx) * PICK_SCALE;
            float ly = ((float)yy - pivoty) * PICK_SCALE;
            int sx = (int)(base_x + lx*ca - ly*sa + 0.5f);
            int sy = (int)(base_y + lx*sa + ly*ca + oy + 0.5f);
            if (sx >= 0 && sx < SCREEN_WIDTH && sy >= 0 && sy < SCREEN_HEIGHT)
                frect(sx, sy, PICK_SCALE, PICK_SCALE);
        }
    }
}

// ---------------- HUD ----------------
static void draw_health() {
    int hearts = PLAYER_MAX_HP / 2;
    for (int i = 0; i < hearts; i++) {
        int x = 2 + i * 6, y = 2;
        bool full = player.hp >= (i + 1) * 2;
        bool half = !full && player.hp == i * 2 + 1;
        if (full)      pen(14, 2, 2);
        else if (half) pen(10, 2, 2);
        else          pen(6, 5, 5);
        frect(x, y, 4, 3);
    }
}

static void draw_hand_hud() {
    uint8_t held = player.held_item();
    uint8_t r, g, b;
    if (held < BLOCK_TYPE_COUNT) block_rgb(held, r, g, b);
    else { r = 12; g = 12; b = 12; }
    int x0 = 2, y0 = SCREEN_HEIGHT - 12, s = 9;
    pen(r, g, b);
    frect(x0, y0, s, s);
    pen(15, 15, 15);
    rect(x0, y0, s, s);
    text(item_name(held), x0 + s + 2, y0);
    if (!item_is_tool(held))
        text(str((int32_t)player.count(held)), x0 + s + 2, y0 + 7);
}

static void draw_coords_time() {
    int x = (int)camera_position[0], z = (int)camera_position[2];
    pen(15, 15, 15);
    text("X" + str((int32_t)x) + " Z" + str((int32_t)z),
         SCREEN_WIDTH - 58, 2);
    // 昼夜图标
    if (is_night_time()) { pen(12, 12, 15); frect(SCREEN_WIDTH - 8, 10, 4, 4); }
    else                 { pen(15, 13, 4); frect(SCREEN_WIDTH - 8, 10, 4, 4); }
}

static void draw_menu() {
    pen(2, 2, 4);
    frect(4, 6, SCREEN_WIDTH - 8, SCREEN_HEIGHT - 12);
    pen(15, 15, 15);
    rect(4, 6, SCREEN_WIDTH - 8, SCREEN_HEIGHT - 12);
    text("CRAFT", 10, 8);
    for (int i = 0; i < recipe_count(); i++) {
        int y = 18 + i * 9;
        if (y > SCREEN_HEIGHT - 18) break;
        bool ok = can_craft(i);
        if (i == menu_sel) pen(15, 15, 8);
        else if (ok)      pen(12, 15, 12);
        else             pen(8, 8, 9);
        std::string mark = (i == menu_sel) ? "> " : "  ";
        text(mark + item_name(recipe_at(i).out)
             + " x" + str((int32_t)recipe_at(i).out_n), 8, y);
    }
    pen(15, 15, 15);
    text("A:MAKE B:EXIT", 8, SCREEN_HEIGHT - 10);
}

void draw(uint32_t tick) {
    (void)tick;

    // 标题模式：纯 2D 菜单
    if (game_menu.mode == MODE_TITLE) {
        bool used[SAVE_SLOT_COUNT];
        for (int i = 0; i < SAVE_SLOT_COUNT; i++)
            used[i] = SaveStore::slot_exists(i);
        game_menu.title_draw(used);
        skip_frame = 0;
        return;
    }

    uint32_t core1_time = render_sync();
    if (core1_time == 0) { skip_frame = 1; return; }
    skip_frame = 0;

    // 暂停模式：在游戏画面上叠加菜单
    if (game_menu.mode == MODE_PAUSE) {
        game_menu.pause_draw();
        return;
    }

    if (menu_open) { draw_menu(); return; }

    pen(15, 15, 15);
    pixel(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
    pixel(SCREEN_WIDTH / 2 - 1, SCREEN_HEIGHT / 2);
    pixel(SCREEN_WIDTH / 2 + 1, SCREEN_HEIGHT / 2);

    draw_highlight();
    render_terrain_skyline(WORLD_SEED);

    // 挖掘进度条
    if (dig_progress > 0) {
        int need = (int)block_hardness((uint8_t)world.get_block(
                     current_target.bx, current_target.by, current_target.bz)) * 4;
        int w = (need > 0) ? dig_progress * 16 / need : 0;
        if (w > 16) w = 16;
        pen(15, 15, 15);
        rect(SCREEN_WIDTH/2 - 9, SCREEN_HEIGHT/2 + 8, 18, 3);
        pen(12, 15, 12);
        frect(SCREEN_WIDTH/2 - 8, SCREEN_HEIGHT/2 + 9, w, 1);
    }

    draw_health();
    draw_coords_time();
    draw_hand_hud();
    draw_pickaxe();

    // 受伤红屏
    if (player.hurt_cooldown > 8) {
        pen(12, 1, 1);
        for (int i = 0; i < 12; i++)
            pixel(i, 0);
    }

    // toast
    if (player.toast_time > 0) {
        pen(0, 0, 0);
        frect(20, 30, SCREEN_WIDTH - 40, 9);
        pen(15, 15, 10);
        text(player.toast, 24, 31);
    }
}
