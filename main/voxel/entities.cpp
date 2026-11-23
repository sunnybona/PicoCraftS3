#include "entities.h"
#include "voxel_world.h"
#include "player_state.h"
#include "day_night.h"
#include "spawn_platform.h"
#include "../engine/render_globals.h"
#include "../engine/render_math.h"
#include <math.h>
#include <string.h>

static int floor_div(int a, int b) {
    int q = a / b;
    int r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) q--;
    return q;
}

// 确定性整数哈希（实体锚点用）
static uint32_t ent_hash(int x, int z, uint32_t seed) {
    uint32_t h = seed ^ 0x454e544du;   // "ENTM"
    h = h * 374761393u + (uint32_t)(x * 668265263);
    h = h * 374761393u + (uint32_t)(z * 668265263);
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

// 出生点强制布局：全部放在出生平台内、玩家前方 5..9 格的扇形
static const struct { int8_t dx, dz; uint8_t type; } spawn_layout[SPAWN_ENTITY_N] = {
    {  0, -5, ENT_NPC},
    { 3, -6, ENT_COW},
    { 5, -7, ENT_SHEEP},
    { -1, -7, ENT_CHICKEN},
    { 2, -8, ENT_DUCK},
};

// ---------------- 生命周期 ----------------
EntitySystem::EntitySystem() {
    for (int i = 0; i < MAX_ENTITIES; i++) {
        entities[i].type = ENT_NONE;
        entities[i].home_cx = entities[i].home_cz = 0;
        entities[i].hp = 0;
        entities[i].atk_cd = 0;
    }
    // 预占出生实体槽
    for (int i = 0; i < SPAWN_ENTITY_N; i++) {
        Entity &e = entities[i];
        e.type = spawn_layout[i].type;
        e.home_cx = 100;
        e.home_cz = 100;
        e.seq = (uint8_t)i;
        e.x = (float)spawn_layout[i].dx;
        e.z = (float)spawn_layout[i].dz;
        e.yaw = 0.0f;
        e.rng = (uint8_t)(0x30 + i * 17);
        e.timer = (uint8_t)(10 + i * 5);
        e.moving = 0;
        e.y = 1.0f;
        e.hp = 0;
        e.atk_cd = 0;
    }
}

static uint8_t pick_type(uint32_t h) {
    switch ((h >> 10) % 10u) {
        case 0: case 1: case 9: return ENT_COW;
        case 2: case 3:         return ENT_SHEEP;
        case 4: case 5:         return ENT_CHICKEN;
        case 6: case 7:         return ENT_DUCK;
        default:                return ENT_NPC;
    }
}

int EntitySystem::zombie_count() const {
    int n = 0;
    for (int i = 0; i < MAX_ENTITIES; i++)
        if (entities[i].type == ENT_ZOMBIE) n++;
    return n;
}

// 夜晚在玩家周围生成僵尸（每帧最多 1，直到上限）
static void spawn_zombies(Entity entities[], const VoxelWorld &world,
                          uint32_t seed, float cam_x, float cam_z) {
    int pcx = floor_div((int)cam_x, CHUNK_S);
    int pcz = floor_div((int)cam_z, CHUNK_S);

    for (int ddz = -1; ddz <= 1; ddz++) {
        for (int ddx = -1; ddx <= 1; ddx++) {
            int cx = pcx + ddx, cz = pcz + ddz;
            uint32_t h = ent_hash(cx, cz, seed ^ 0x5a4d42u);
            if (h % 3u != 0u) continue;          // 约 1/3 chunk 候选

            int lx = 2 + (int)((h >> 6)  % 12u);
            int lz = 2 + (int)((h >> 18) % 12u);
            int wx = cx * CHUNK_S + lx;
            int wz = cz * CHUNK_S + lz;

            float ox = (float)wx - cam_x, oz = (float)wz - cam_z;
            float d2 = ox*ox + oz*oz;
            if (d2 < 49.0f || d2 > 225.0f) continue;   // 距玩家 7..15 格
            if (in_spawn_platform(wx, wz)) continue;

            int gy = world.surface_y(wx, wz);
            if (gy < 0) continue;
            if (world.get_block(wx, gy, wz) == BLK_WATER) continue;

            for (int i = SPAWN_ENTITY_N; i < MAX_ENTITIES; i++) {
                if (entities[i].type != ENT_NONE) continue;
                Entity &e = entities[i];
                e.type = ENT_ZOMBIE;
                e.home_cx = (int16_t)cx;
                e.home_cz = (int16_t)cz;
                e.seq = 0;
                e.x = (float)wx; e.z = (float)wz; e.y = (float)(gy + 1);
                e.yaw = 0.0f; e.rng = (uint8_t)(h | 1);
                e.timer = 0; e.moving = 0;
                e.hp = ZOMBIE_HP; e.atk_cd = 0;
                return;   // 每帧最多 1
            }
        }
    }
}

void EntitySystem::update(const VoxelWorld &world, uint32_t seed,
                         float cam_x, float cam_z, uint32_t tick) {
    (void)tick;
    int pcx = floor_div((int)floorf(cam_x), CHUNK_S);
    int pcz = floor_div((int)floorf(cam_z), CHUNK_S);

    // ===== 动物 chunk 扫描 =====
    bool seen[MAX_ENTITIES] = {};
    for (int i = 0; i < SPAWN_ENTITY_N; i++) seen[i] = true;
    for (int i = 0; i < MAX_ENTITIES; i++)
        if (entities[i].type == ENT_ZOMBIE) seen[i] = true;

    for (int ddz = -1; ddz <= 1; ddz++) {
        for (int ddx = -1; ddx <= 1; ddx++) {
            int cx = pcx + ddx, cz = pcz + ddz;
            uint32_t h = ent_hash(cx, cz, seed);
            if ((h % 100u) >= 38u) continue;

            uint8_t type = pick_type(h);
            int lx = 4 + (int)((h >> 8)  % 9u);
            int lz = 4 + (int)((h >> 20) % 9u);
            int wx = cx * CHUNK_S + lx;
            int wz = cz * CHUNK_S + lz;

            int idx = -1;
            for (int i = SPAWN_ENTITY_N; i < MAX_ENTITIES; i++) {
                if (entities[i].type != ENT_NONE && entities[i].type != ENT_ZOMBIE &&
                    entities[i].home_cx == cx && entities[i].home_cz == cz) { idx = i; break; }
            }
            if (idx < 0) {
                for (int i = SPAWN_ENTITY_N; i < MAX_ENTITIES; i++) {
                    if (entities[i].type == ENT_NONE) { idx = i; break; }
                }
            }
            if (idx < 0) continue;

            Entity &e = entities[idx];
            if (e.type == ENT_NONE) {
                e.type = type;
                e.home_cx = (int16_t)cx;
                e.home_cz = (int16_t)cz;
                e.seq = 0;
                e.x = (float)wx; e.z = (float)wz; e.yaw = 0.0f;
                e.rng = (uint8_t)(h | 1u); e.timer = 10u; e.moving = 0;
                e.hp = 0; e.atk_cd = 0;
                int gy = world.surface_y(wx, wz);
                e.y = (gy >= 0 ? (float)(gy + 1) : 1.0f);
            }
            seen[idx] = true;
        }
    }

    // 离开范围的动物释放
    for (int i = 0; i < MAX_ENTITIES; i++) {
        if (entities[i].type != ENT_NONE && entities[i].type != ENT_ZOMBIE && !seen[i])
            entities[i].type = ENT_NONE;
    }

    // ===== 僵尸：夜晚生成，白天清除 =====
    if (is_night_time()) {
        if (zombie_count() < MAX_ZOMBIES)
            spawn_zombies(entities, world, seed, cam_x, cam_z);
    } else {
        for (int i = 0; i < MAX_ENTITIES; i++)
            if (entities[i].type == ENT_ZOMBIE) entities[i].type = ENT_NONE;
    }

    // ===== AI 推进 =====
    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity &e = entities[i];
        if (e.type == ENT_NONE) continue;

        if (e.type == ENT_ZOMBIE) {
            if (e.atk_cd > 0) e.atk_cd--;
            float dx = cam_x - e.x, dz = cam_z - e.z;
            float dist = sqrtf(dx*dx + dz*dz);
            if (dist > 0.05f) {
                float nx = e.x + dx/dist * 0.035f;
                float nz = e.z + dz/dist * 0.035f;
                int gy_now = world.surface_y((int)e.x, (int)e.z);
                int gy_next = world.surface_y((int)nx, (int)nz);
                if (gy_next <= gy_now + 1) { e.x = nx; e.z = nz; }
                int gy = world.surface_y((int)e.x, (int)e.z);
                if (gy >= 0) e.y = (float)(gy + 1);
            }
            // 攻击玩家（受玩家无敌帧保护）
            if (dist < 1.15f && e.atk_cd == 0 && player.hurt_cooldown == 0) {
                player.hp = (int8_t)(player.hp - ZOMBIE_ATTACK);
                e.atk_cd = ZOMBIE_ATTACK_CD;
                player.hurt_cooldown = 16;
                if (player.hp <= 0) {
                    player.hp = 0;
                    player.respawn_pending = true;
                }
            }
        } else {
            // 动物游走
            e.timer--;
            if (e.timer == 0) {
                e.rng = (uint8_t)(e.rng * 73u + 97u);
                uint8_t r = e.rng;
                if (r % 3u == 0u) {
                    e.moving = 0;
                    e.timer = 20u + (r % 30u);
                } else {
                    e.moving = 1;
                    e.yaw = (float)(r % 8u) * (PI / 4.0f);
                    e.timer = 8u + (r % 16u);
                }
            }
            if (e.moving) {
                float sp = (e.type == ENT_COW || e.type == ENT_SHEEP) ? 0.018f : 0.024f;
                e.x += -sinf(e.yaw) * sp;
                e.z += -cosf(e.yaw) * sp;
            }
            int gy = world.surface_y((int)e.x, (int)e.z);
            if (gy >= 0) e.y = (float)(gy + 1);
        }
    }
}

// ---------------- 渲染 ----------------
static inline color_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (color_t)(((g & 15) << 12) | ((b & 15) << 8) | (r & 15));
}

static color_t shade(color_t c, float f) {
    int r = (int)((c & 15) * f);
    int b = (int)(((c >> 8) & 15) * f);
    int g = (int)(((c >> 12) & 15) * f);
    if (r > 15) r = 15;
    if (g > 15) g = 15;
    if (b > 15) b = 15;
    return rgb((uint8_t)r, (uint8_t)g, (uint8_t)b);
}

static void emit_box(float x0, float y0, float z0,
                     float x1, float y1, float z1, color_t col) {
    const float p[8][3] = {
        {x0,y0,z0},{x1,y0,z0},{x1,y0,z1},{x0,y0,z1},
        {x0,y1,z0},{x1,y1,z0},{x1,y1,z1},{x0,y1,z1},
    };
    static const struct { uint8_t c[4]; float br; } faces[6] = {
        {{4,7,6,5}, 1.00f}, {{0,1,2,3}, 0.50f},
        {{3,2,6,7}, 0.80f}, {{0,4,5,1}, 0.80f},
        {{2,1,5,6}, 0.65f}, {{3,7,4,0}, 0.65f},
    };
    triangle_32 t;
    t.shader_id = 1;
    t.texture_id = 0;
    for (int f = 0; f < 6; f++) {
        if (number_triangles >= MAX_RENDER_TRIANGLES - 2) return;
        color_t fc = shade(col, faces[f].br);
        const int tri[2][3] = {{0,1,2},{0,2,3}};
        for (int ti = 0; ti < 2; ti++) {
            vertex_32 *v[3] = {&t.vertex1,&t.vertex2,&t.vertex3};
            for (int k = 0; k < 3; k++) {
                const float *q = p[faces[f].c[tri[ti][k]]];
                v[k]->x = float_to_int(q[0]);
                v[k]->y = float_to_int(q[1]);
                v[k]->z = float_to_int(q[2]);
            }
            t.vertex_parameter1.color = fc;
            t.vertex_parameter2.color = fc;
            t.vertex_parameter3.color = fc;
            render_triangle(t);
        }
    }
}

void EntitySystem::render(float cam_x, float cam_z) {
    for (int i = 0; i < MAX_ENTITIES; i++) {
        const Entity &e = entities[i];
        if (e.type == ENT_NONE) continue;

        float dx = e.x - cam_x, dz = e.z - cam_z;
        if (dx*dx + dz*dz > (float)(ENTITY_DRAW_RADIUS * ENTITY_DRAW_RADIUS))
            continue;

        const float x = e.x, y = e.y, z = e.z;

        switch (e.type) {
        case ENT_COW: {
            color_t body = rgb(14,14,13), dark = rgb(8,6,5);
            const float lx[2]={-0.28f,0.22f}, lz[2]={-0.15f,0.15f};
            for (int a=0;a<2;a++) for (int b=0;b<2;b++)
                emit_box(x+lx[a], y+0.05f, z+lz[b],
                         x+lx[a]+0.10f, y+0.50f, z+lz[b]+0.10f, dark);
            emit_box(x-0.35f,y+0.50f,z-0.20f, x+0.35f,y+0.82f,z+0.20f, body);
            emit_box(x+0.32f,y+0.62f,z-0.14f, x+0.52f,y+0.90f,z+0.14f, dark);
        } break;

        case ENT_SHEEP: {
            color_t wool = rgb(14,14,14), blk = rgb(7,7,7);
            const float lx[2]={-0.24f,0.20f}, lz[2]={-0.13f,0.13f};
            for (int a=0;a<2;a++) for (int b=0;b<2;b++)
                emit_box(x+lx[a], y+0.05f, z+lz[b],
                         x+lx[a]+0.07f, y+0.45f, z+lz[b]+0.07f, blk);
            emit_box(x-0.30f,y+0.45f,z-0.18f, x+0.30f,y+0.75f,z+0.18f, wool);
            emit_box(x+0.28f,y+0.55f,z-0.12f, x+0.44f,y+0.75f,z+0.12f, blk);
        } break;

        case ENT_CHICKEN: {
            color_t wh = rgb(14,14,13), org = rgb(15,12,3);
            emit_box(x-0.05f,y+0.02f,z-0.02f, x-0.01f,y+0.20f,z+0.02f, org);
            emit_box(x+0.01f,y+0.02f,z-0.02f, x+0.05f,y+0.20f,z+0.02f, org);
            emit_box(x-0.14f,y+0.20f,z-0.12f, x+0.14f,y+0.40f,z+0.12f, wh);
            emit_box(x+0.08f,y+0.40f,z-0.07f, x+0.20f,y+0.52f,z+0.07f, wh);
            emit_box(x+0.20f,y+0.43f,z-0.03f, x+0.26f,y+0.47f,z+0.03f, org);
        } break;

        case ENT_DUCK: {
            color_t gry = rgb(12,13,13), grn = rgb(4,10,4), org = rgb(15,12,3);
            emit_box(x-0.05f,y+0.02f,z-0.02f, x-0.01f,y+0.15f,z+0.02f, org);
            emit_box(x+0.01f,y+0.02f,z-0.02f, x+0.05f,y+0.15f,z+0.02f, org);
            emit_box(x-0.16f,y+0.15f,z-0.13f, x+0.16f,y+0.36f,z+0.13f, gry);
            emit_box(x+0.08f,y+0.36f,z-0.08f, x+0.20f,y+0.50f,z+0.08f, grn);
            emit_box(x+0.20f,y+0.40f,z-0.04f, x+0.27f,y+0.44f,z+0.04f, org);
        } break;

        case ENT_NPC: {
            color_t robe = rgb(11,7,4), pants = rgb(4,4,9), skin = rgb(14,11,9);
            emit_box(x-0.12f,y+0.05f,z-0.10f, x-0.02f,y+0.65f,z+0.10f, pants);
            emit_box(x+0.02f,y+0.05f,z-0.10f, x+0.12f,y+0.65f,z+0.10f, pants);
            emit_box(x-0.22f,y+0.65f,z-0.14f, x+0.22f,y+1.20f,z+0.14f, robe);
            emit_box(x-0.30f,y+0.70f,z-0.12f, x-0.22f,y+1.12f,z+0.12f, robe);
            emit_box(x+0.22f,y+0.70f,z-0.12f, x+0.30f,y+1.12f,z+0.12f, robe);
            emit_box(x-0.14f,y+1.20f,z-0.14f, x+0.14f,y+1.45f,z+0.14f, skin);
            emit_box(x+0.14f,y+1.28f,z-0.03f, x+0.18f,y+1.36f,z+0.03f, rgb(12,9,7));
        } break;

        case ENT_ZOMBIE: {
            color_t zskin = rgb(8,13,6), dark = rgb(5,9,4), shirt = rgb(7,11,6);
            emit_box(x-0.12f,y+0.05f,z-0.10f, x-0.02f,y+0.60f,z+0.10f, dark);
            emit_box(x+0.02f,y+0.05f,z-0.10f, x+0.12f,y+0.60f,z+0.10f, dark);
            emit_box(x-0.22f,y+0.60f,z-0.14f, x+0.22f,y+1.15f,z+0.14f, shirt);
            emit_box(x-0.30f,y+0.95f,z-0.12f, x-0.22f,y+1.12f,z+0.30f, zskin);
            emit_box(x+0.22f,y+0.95f,z-0.12f, x+0.30f,y+1.12f,z+0.30f, zskin);
            emit_box(x-0.14f,y+1.15f,z-0.14f, x+0.14f,y+1.42f,z+0.14f, zskin);
        } break;

        default: break;
        }
    }
}

// ---------------- 玩家攻击 ----------------
AttackResult EntitySystem::attack_entities(float px, float py, float pz,
                                          float yaw, uint8_t damage) {
    (void)py;
    AttackResult r;
    r.hit = false; r.killed = false; r.drop = 0;
    r.kx = r.ky = r.kz = 0.0f;

    int best = -1;
    float bestd = 3.2f;
    for (int i = 0; i < MAX_ENTITIES; i++) {
        if (entities[i].type != ENT_ZOMBIE) continue;
        float dx = entities[i].x - px, dz = entities[i].z - pz;
        float d = sqrtf(dx*dx + dz*dz);
        if (d > 3.0f) continue;
        float ang = atan2f(-dx, -dz);
        float diff = ang - yaw;
        while (diff > PI) diff -= 2*PI;
        while (diff < -PI) diff += 2*PI;
        if (fabsf(diff) > 0.7f) continue;
        if (d < bestd) { bestd = d; best = i; }
    }

    if (best < 0) return r;
    Entity &e = entities[best];
    r.hit = true;
    e.hp = (int8_t)(e.hp - damage);
    if (e.hp <= 0) {
        r.killed = true;
        r.drop = ITEM_COAL;
        r.kx = e.x; r.ky = e.y + 0.8f; r.kz = e.z;
        e.type = ENT_NONE;
    }
    return r;
}

void EntitySystem::damage_in_radius(float cx, float cy, float cz,
                                   float radius, uint8_t damage) {
    for (int i = 0; i < MAX_ENTITIES; i++) {
        if (entities[i].type != ENT_ZOMBIE) continue;
        float dx = entities[i].x - cx;
        float dy = (entities[i].y + 0.8f) - cy;
        float dz = entities[i].z - cz;
        float d = sqrtf(dx*dx + dy*dy + dz*dz);
        if (d > radius) continue;
        entities[i].hp = (int8_t)(entities[i].hp - damage);
        if (entities[i].hp <= 0) entities[i].type = ENT_NONE;
    }
}
