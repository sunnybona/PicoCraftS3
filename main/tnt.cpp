#include "tnt.h"
#include "voxel/voxel_world.h"
#include "player_state.h"
#include "item_drops.h"
#include "voxel/entities.h"
#include "engine/render_globals.h"
#include "engine/render_math.h"
#include <math.h>

extern ItemDrops item_drops;
extern EntitySystem entities;

TntSystem::TntSystem() { clear(); }

void TntSystem::clear() {
    for (int i = 0; i < MAX_PRIMED_TNT; i++) tnts[i].active = false;
}

void TntSystem::ignite(float x, float y, float z) {
    for (int i = 0; i < MAX_PRIMED_TNT; i++) {
        if (!tnts[i].active) {
            PrimedTnt &t = tnts[i];
            t.active = true;
            t.x = x; t.y = y; t.z = z;
            t.fuse = TNT_FUSE;
            return;
        }
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
    triangle_32 tr;
    tr.shader_id = 1;
    tr.texture_id = 0;
    for (int f = 0; f < 6; f++) {
        if (number_triangles >= MAX_RENDER_TRIANGLES - 2) return;
        const int tri[2][3] = {{0,1,2},{0,2,3}};
        for (int ti = 0; ti < 2; ti++) {
            vertex_32 *v[3] = {&tr.vertex1,&tr.vertex2,&tr.vertex3};
            for (int k = 0; k < 3; k++) {
                const float *q = p[faces[f][tri[ti][k]]];
                v[k]->x = float_to_int(q[0]);
                v[k]->y = float_to_int(q[1]);
                v[k]->z = float_to_int(q[2]);
            }
            tr.vertex_parameter1.color = col;
            tr.vertex_parameter2.color = col;
            tr.vertex_parameter3.color = col;
            render_triangle(tr);
        }
    }
}

void TntSystem::update(VoxelWorld &world, float px, float py, float pz) {
    for (int i = 0; i < MAX_PRIMED_TNT; i++) {
        PrimedTnt &t = tnts[i];
        if (!t.active) continue;

        // 下落
        int below = world.get_block((int)t.x, (int)(t.y - 0.2f), (int)t.z);
        if (!VoxelWorld::is_solid((uint8_t)below)) t.y -= 0.08f;

        t.fuse--;
        if (t.fuse > 0) continue;

        // ===== 爆炸 =====
        int R = 3;
        for (int dx = -R; dx <= R; dx++) {
            for (int dy = -R; dy <= R; dy++) {
                for (int dz = -R; dz <= R; dz++) {
                    if (dx*dx + dy*dy + dz*dz > R*R) continue;
                    int bx = (int)t.x + dx, by = (int)t.y + dy, bz = (int)t.z + dz;
                    int b = world.get_block(bx, by, bz);
                    if (b == BLK_AIR || b == BLK_BEDROCK || b == BLK_WATER) continue;
                    world.set_block(bx, by, bz, BLK_AIR);
                    uint8_t d = block_drop_item((uint8_t)b);
                    if (d != 0)
                        item_drops.spawn((float)bx + 0.5f, (float)by + 0.5f,
                                        (float)bz + 0.5f, d);
                }
            }
        }

        // 玩家伤害
        float pdx = px - t.x, pdy = (py - 0.8f) - t.y, pdz = pz - t.z;
        float pd = sqrtf(pdx*pdx + pdy*pdy + pdz*pdz);
        if (pd < 5.0f) {
            int dmg = (int)(8.0f - pd);
            if (dmg < 1) dmg = 1;
            player.hp = (int8_t)(player.hp - dmg);
            if (player.hp <= 0) {
                player.hp = 0;
                player.respawn_pending = true;
            }
        }

        // 怪物范围伤害
        entities.damage_in_radius(t.x, t.y, t.z, 4.0f, 6);

        player.unlock(ACH_TNT);
        t.active = false;
    }
}

void TntSystem::render(float cam_x, float cam_z) const {
    for (int i = 0; i < MAX_PRIMED_TNT; i++) {
        const PrimedTnt &t = tnts[i];
        if (!t.active) continue;
        float dx = t.x - cam_x, dz = t.z - cam_z;
        if (dx*dx + dz*dz > 196.0f) continue;

        // 引信最后阶段快速闪烁
        bool flash = (t.fuse < 12) && ((t.fuse & 1) == 0);
        color_t col;
        if (flash) col = (color_t)(((14) << 12) | (14 << 8) | 14);
        else       col = (color_t)(((4) << 12) | (3 << 8) | 13);
        float s = 0.42f;
        emit_cube(t.x-s, t.y-s, t.z-s, t.x+s, t.y+s, t.z+s, col);
    }
}
