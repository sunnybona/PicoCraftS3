#include "item_drops.h"
#include "voxel/voxel_world.h"
#include "voxel/voxel_mesh.h"
#include "player_state.h"
#include "engine/render_globals.h"
#include "engine/render_math.h"
#include <math.h>

ItemDrops::ItemDrops() { clear(); }

void ItemDrops::clear() {
    for (int i = 0; i < MAX_DROPS; i++) {
        drops[i].active = false;
        drops[i].item = 0;
        drops[i].age = 0;
    }
}

void ItemDrops::spawn(float x, float y, float z, uint8_t item) {
    if (item == 0) return;
    for (int i = 0; i < MAX_DROPS; i++) {
        if (!drops[i].active) {
            DropItem &d = drops[i];
            d.active = true;
            d.x = x; d.y = y; d.z = z;
            d.item = item;
            d.age = (uint8_t)(i * 13);
            return;
        }
    }
}

void ItemDrops::update(const VoxelWorld &world, float px, float py, float pz) {
    float body_y = py - 0.8f;   // 玩家身体中心
    for (int i = 0; i < MAX_DROPS; i++) {
        DropItem &d = drops[i];
        if (!d.active) continue;
        d.age++;

        // 重力下落：下方为非实心则下降
        int below = world.get_block((int)d.x, (int)(d.y - 0.18f), (int)d.z);
        if (!VoxelWorld::is_solid((uint8_t)below)) {
            d.y -= 0.06f;
        }

        // 自动拾取
        float dx = d.x - px, dy = d.y - body_y, dz = d.z - pz;
        if (dx*dx + dy*dy + dz*dz < DROP_PICKUP_RADIUS * DROP_PICKUP_RADIUS) {
            player.add(d.item);
            d.active = false;
        }
    }
}

// ---------------- 渲染 ----------------
static color_t vrgb(uint8_t r, uint8_t g, uint8_t b) {
    return (color_t)(((g & 15) << 12) | ((b & 15) << 8) | (r & 15));
}

static void item_color(uint8_t id, uint8_t &r, uint8_t &g, uint8_t &b) {
    if (id < BLOCK_TYPE_COUNT) {
        block_rgb(id, r, g, b);
        return;
    }
    switch (id) {
        case ITEM_STICK:      r=12; g=9;  b=5;  break;
        case ITEM_COAL:       r=3;  g=3;  b=4;  break;
        case ITEM_IRON_INGOT: r=14; g=13; b=12; break;
        case ITEM_DIAMOND:    r=9;  g=15; b=15; break;
        case ITEM_WOOD_PICK:  r=12; g=9;  b=5;  break;
        case ITEM_STONE_PICK: r=10; g=10; b=11; break;
        case ITEM_IRON_PICK:  r=14; g=13; b=12; break;
        case ITEM_DIAMOND_PICK: r=9; g=15; b=15; break;
        default: r=8; g=8; b=8; break;
    }
}

static void emit_cube(float x0, float y0, float z0,
                      float x1, float y1, float z1, color_t col) {
    const float p[8][3] = {
        {x0,y0,z0},{x1,y0,z0},{x1,y0,z1},{x0,y0,z1},
        {x0,y1,z0},{x1,y1,z0},{x1,y1,z1},{x0,y1,z1},
    };
    static const uint8_t faces[6][4] = {
        {4,7,6,5},{0,1,2,3},{3,2,6,7},{0,4,5,1},{2,1,5,6},{3,7,4,0},
    };
    triangle_32 t;
    t.shader_id = 1;
    t.texture_id = 0;
    for (int f = 0; f < 6; f++) {
        if (number_triangles >= MAX_RENDER_TRIANGLES - 2) return;
        const int tri[2][3] = {{0,1,2},{0,2,3}};
        for (int ti = 0; ti < 2; ti++) {
            vertex_32 *v[3] = {&t.vertex1,&t.vertex2,&t.vertex3};
            for (int k = 0; k < 3; k++) {
                const float *q = p[faces[f][tri[ti][k]]];
                v[k]->x = float_to_int(q[0]);
                v[k]->y = float_to_int(q[1]);
                v[k]->z = float_to_int(q[2]);
            }
            t.vertex_parameter1.color = col;
            t.vertex_parameter2.color = col;
            t.vertex_parameter3.color = col;
            render_triangle(t);
        }
    }
}

void ItemDrops::render(float cam_x, float cam_z) const {
    for (int i = 0; i < MAX_DROPS; i++) {
        const DropItem &d = drops[i];
        if (!d.active) continue;
        float dx = d.x - cam_x, dz = d.z - cam_z;
        if (dx*dx + dz*dz > 169.0f) continue;     // 13 格内渲染

        uint8_t r, g, b;
        item_color(d.item, r, g, b);
        float bob = sinf((float)d.age * 0.12f) * 0.06f;
        float s = 0.18f;
        emit_cube(d.x-s, d.y-s+bob, d.z-s,
                  d.x+s, d.y+s+bob, d.z+s, vrgb(r, g, b));
    }
}
