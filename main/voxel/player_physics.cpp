#include "player_physics.h"
#include <math.h>
#include "../engine/render_globals.h"

PlayerPhysics::PlayerPhysics()
    : vx(0), vy(0), vz(0), on_ground(false), in_water(false),
      just_landed(false), jump_requested(false) {}

void PlayerPhysics::reset() {
    vx = vy = vz = 0.0f;
    on_ground = false;
    in_water = false;
    just_landed = false;
    jump_requested = false;
}

void PlayerPhysics::request_jump() {
    jump_requested = true;
}

bool PlayerPhysics::collides(const VoxelWorld &world, float x, float feet_y, float z) {
    const float eps = 0.001f;
    float minx = x - PLAYER_HALF_WIDTH, maxx = x + PLAYER_HALF_WIDTH;
    float miny = feet_y,             maxy = feet_y + PLAYER_HEIGHT;
    float minz = z - PLAYER_HALF_WIDTH, maxz = z + PLAYER_HALF_WIDTH;

    int bx0 = (int)floorf(minx + eps), bx1 = (int)floorf(maxx - eps);
    int by0 = (int)floorf(miny + eps), by1 = (int)floorf(maxy - eps);
    int bz0 = (int)floorf(minz + eps), bz1 = (int)floorf(maxz - eps);

    for (int bx = bx0; bx <= bx1; bx++) {
        for (int by = by0; by <= by1; by++) {
            for (int bz = bz0; bz <= bz1; bz++) {
                if (VoxelWorld::is_solid((uint8_t)world.get_block(bx, by, bz))) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool PlayerPhysics::body_in_water(const VoxelWorld &world, float x, float feet_y, float z) {
    int gx = (int)floorf(x), gz = (int)floorf(z);
    int eye_y = (int)floorf(feet_y + PLAYER_EYE);
    int feet_ycell = (int)floorf(feet_y + 0.1f);
    return VoxelWorld::is_water((uint8_t)world.get_block(gx, eye_y, gz)) ||
           VoxelWorld::is_water((uint8_t)world.get_block(gx, feet_ycell, gz));
}

void PlayerPhysics::step(const VoxelWorld &world, float wish_x, float wish_z) {
    // 相机存的是眼睛位置，反推脚位置
    float x = camera_position[0];
    float z = camera_position[2];
    float feet_y = camera_position[1] - PLAYER_EYE;

    in_water = body_in_water(world, x, feet_y, z);
    just_landed = false;

    // 显式跳跃请求：地面起跳，水中向上游
    if (jump_requested) {
        if (in_water) vy = WATER_MAX_RISE;
        else if (on_ground) vy = JUMP_SPEED;
        jump_requested = false;
    }

    // 水平速度（无惯性，直接取期望）；水中移动减半
    float water_scale = in_water ? 0.5f : 1.0f;
    vx = wish_x * water_scale;
    vz = wish_z * water_scale;

    // 垂直：重力 / 水中浮力
    if (in_water) {
        vy += WATER_BUOYANCY - WATER_GRAVITY;
        if (vy > WATER_MAX_RISE) vy = WATER_MAX_RISE;
        if (vy < -WATER_MAX_FALL) vy = -WATER_MAX_FALL;
    } else {
        vy -= GRAVITY;
        if (vy < -MAX_FALL) vy = -MAX_FALL;
    }

    // ---- 分轴移动 + 碰撞 ----

    // X
    float nx = x + vx;
    if (collides(world, nx, feet_y, z)) vx = 0.0f;
    else x = nx;

    // Z
    float nz = z + vz;
    if (collides(world, x, feet_y, nz)) vz = 0.0f;
    else z = nz;

    // Y
    float nfeet = feet_y + vy;
    if (vy <= 0.0f) {
        if (collides(world, x, nfeet, z)) {
            just_landed = true;
            on_ground = true;
            vy = 0.0f;
            feet_y = floorf(nfeet) + 1.0f;
            while (collides(world, x, feet_y, z)) feet_y += 1.0f;
        } else {
            on_ground = false;
            feet_y = nfeet;
        }
    } else {
        if (collides(world, x, nfeet, z)) {
            vy = 0.0f;   // 顶头
        } else {
            on_ground = false;
            feet_y = nfeet;
        }
    }

    // 写回相机（眼睛位置）
    camera_position[0] = x;
    camera_position[2] = z;
    camera_position[1] = feet_y + PLAYER_EYE;
}
