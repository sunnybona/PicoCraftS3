#include "voxel_world.h"
#include "structures.h"
#include "spawn_platform.h"
#include <math.h>
#include <string.h>
#include <cmath>
#include <cstdlib>

// ---------------- 轻量值噪声（确定性，无需外部库） ----------------

static uint32_t hash2(int x, int z, uint32_t seed) {
    uint32_t h = seed;
    h = h * 374761393u + (uint32_t)(x * 668265263);
    h = h * 374761393u + (uint32_t)(z * 668265263);
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static float value_noise(float x, float z, uint32_t seed) {
    int x0 = (int)floorf(x);
    int z0 = (int)floorf(z);
    float fx = x - x0;
    float fz = z - z0;
    float u = fx * fx * (3.0f - 2.0f * fx);
    float v = fz * fz * (3.0f - 2.0f * fz);

    float a = (hash2(x0,     z0,     seed) & 0xffff) / 65535.0f;
    float b = (hash2(x0 + 1, z0,     seed) & 0xffff) / 65535.0f;
    float c = (hash2(x0,     z0 + 1, seed) & 0xffff) / 65535.0f;
    float d = (hash2(x0 + 1, z0 + 1, seed) & 0xffff) / 65535.0f;

    float ab = a * (1.0f - u) + b * u;
    float cd = c * (1.0f - u) + d * u;
    return ab * (1.0f - v) + cd * v;
}

// 分形叠加（3 个倍频）
static float fbm(float x, float z, uint32_t seed) {
    float total = 0.0f, amp = 1.0f, freq = 1.0f, maxv = 0.0f;
    for (int o = 0; o < 3; o++) {
        total += value_noise(x * freq, z * freq, seed + (uint32_t)o * 1013u) * amp;
        maxv += amp;
        amp *= 0.5f;
        freq *= 2.0f;
    }
    return total / maxv;
}

// 向负无穷取整的除法（处理负坐标）
static int floor_div(int a, int b) {
    int q = a / b;
    int r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) q--;
    return q;
}

// ---------------- 3D 哈希与 3D 值噪声（洞穴/矿石） ----------------
static uint32_t hash3(int x, int y, int z, uint32_t seed) {
    uint32_t h = seed;
    h = h * 374761393u + (uint32_t)(x * 668265263);
    h = h * 374761393u + (uint32_t)(y * 2246822519u);
    h = h * 374761393u + (uint32_t)(z * 3266489917u);
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static float vnoise3(float x, float y, float z, uint32_t seed) {
    int x0 = (int)floorf(x), y0 = (int)floorf(y), z0 = (int)floorf(z);
    float fx = x - x0, fy = y - y0, fz = z - z0;
    float u = fx*fx*(3.0f-2.0f*fx), v = fy*fy*(3.0f-2.0f*fy), w = fz*fz*(3.0f-2.0f*fz);

    float corner[8];
    for (int i = 0; i < 8; i++) {
        int dx = i & 1, dy = (i >> 1) & 1, dz = (i >> 2) & 1;
        corner[i] = (hash3(x0+dx, y0+dy, z0+dz, seed) & 0xffff) / 65535.0f;
    }
    float x00 = corner[0]*(1-u)+corner[1]*u;
    float x10 = corner[2]*(1-u)+corner[3]*u;
    float x01 = corner[4]*(1-u)+corner[5]*u;
    float x11 = corner[6]*(1-u)+corner[7]*u;
    float y0v = x00*(1-v)+x10*v;
    float y1v = x01*(1-v)+x11*v;
    return y0v*(1-w)+y1v*w;
}

// 矿石替换：在原本是石头的格子按深度与概率替换；返回最终方块
static uint8_t maybe_ore(int wx, int y, int wz, uint32_t seed) {
    // 钻石（最深，1..3）
    if (y <= 3 && hash3(wx, y, wz, seed ^ 0xd1a) % 26u == 0u)
        return BLK_DIAMOND_ORE;
    // 铁（1..7）
    if (y <= 7 && hash3(wx, y, wz, seed ^ 0x1fe) % 30u == 0u)
        return BLK_IRON_ORE;
    // 煤（1..10）
    if (y <= 10 && hash3(wx, y, wz, seed ^ 0xc0a1) % 22u == 0u)
        return BLK_COAL_ORE;
    return BLK_STONE;
}

// 世界坐标某列的地形高度（不含树）
static int column_height(int wx, int wz, uint32_t seed) {
    float n = fbm((float)wx * 0.12f, (float)wz * 0.12f, seed);
    int h = 2 + (int)(n * 8.0f);
    if (h > CHUNK_H - 6) h = CHUNK_H - 6;
    return h;
}

static inline int local_index(int lx, int y, int lz) {
    return lx * CHUNK_H * CHUNK_S + y * CHUNK_S + lz;
}

// ---------------- 生命周期 ----------------

VoxelWorld::VoxelWorld() : edit_count(0), seed(0) {
    for (int i = 0; i < CHUNK_POOL_COUNT; i++) {
        chunks[i].valid = false;
        chunks[i].cx = chunks[i].cz = 0;
    }
}

void VoxelWorld::generate(uint32_t s) {
    seed = s;
    edit_count = 0;
    for (int i = 0; i < CHUNK_POOL_COUNT; i++) {
        chunks[i].valid = false;
    }
}

Chunk* VoxelWorld::find_chunk(int cx, int cz) {
    for (int i = 0; i < CHUNK_POOL_COUNT; i++) {
        if (chunks[i].valid && chunks[i].cx == cx && chunks[i].cz == cz) {
            return &chunks[i];
        }
    }
    return nullptr;
}

const Chunk* VoxelWorld::find_chunk(int cx, int cz) const {
    for (int i = 0; i < CHUNK_POOL_COUNT; i++) {
        if (chunks[i].valid && chunks[i].cx == cx && chunks[i].cz == cz) {
            return &chunks[i];
        }
    }
    return nullptr;
}

void VoxelWorld::apply_edits(Chunk &c) {
    for (int i = 0; i < edit_count; i++) {
        const EditEntry &e = edits[i];
        int ecx = floor_div(e.x, CHUNK_S);
        int ecz = floor_div(e.z, CHUNK_S);
        if (ecx != c.cx || ecz != c.cz) continue;
        if (e.y < 0 || e.y >= CHUNK_H) continue;
        int lx = e.x - ecx * CHUNK_S;
        int lz = e.z - ecz * CHUNK_S;
        c.blocks[local_index(lx, e.y, lz)] = e.block;
    }
}

void VoxelWorld::generate_chunk(Chunk &c, int cx, int cz) {
    // 1) 地形 + 水
    for (int lx = 0; lx < CHUNK_S; lx++) {
        for (int lz = 0; lz < CHUNK_S; lz++) {
            int wx = cx * CHUNK_S + lx;
            int wz = cz * CHUNK_S + lz;
            int h = column_height(wx, wz, seed);
            if (in_spawn_platform(wx, wz)) h = SPAWN_PLATFORM_HIGH;  // 出生平台强制平整
            bool platform = in_spawn_platform(wx, wz);

            for (int y = 0; y < CHUNK_H; y++) {
                uint8_t b;
                if (y <= h) {
                    if (y == 0) {
                        b = BLK_BEDROCK;
                    } else if (y < h) {
                        if (y < h - 2) {
                            // 石头层（1..h-3）：洞穴挖空，否则按矿石分层
                            if (!platform &&
                                vnoise3((float)wx * 0.18f, (float)y * 0.18f,
                                        (float)wz * 0.18f, seed ^ 0xcafe) > 0.74f) {
                                b = (y <= SEA_LEVEL) ? BLK_WATER : BLK_AIR;
                            } else {
                                b = maybe_ore(wx, y, wz, seed);
                            }
                        } else {
                            b = BLK_DIRT;   // h-2, h-1 为泥土
                        }
                    } else {
                        b = (h <= SEA_LEVEL) ? BLK_SAND : BLK_GRASS;
                    }
                } else {
                    b = (y <= SEA_LEVEL) ? BLK_WATER : BLK_AIR;
                }
                c.blocks[local_index(lx, y, lz)] = b;
            }
        }
    }

    // 2) 树：仅在本地 [2..13]，保证树冠不跨 chunk
    for (int lx = 2; lx <= CHUNK_S - 3; lx++) {
        for (int lz = 2; lz <= CHUNK_S - 3; lz++) {
            int wx = cx * CHUNK_S + lx;
            int wz = cz * CHUNK_S + lz;
            if (in_spawn_platform(wx, wz)) continue;   // 出生平台保持开阔，不种树
            int gy = column_height(wx, wz, seed);
            if (gy <= SEA_LEVEL || gy >= CHUNK_H - 6) continue;
            if (c.blocks[local_index(lx, gy, lz)] != BLK_GRASS) continue;
            if ((hash2(wx, wz, seed ^ 0x9e3779b9u) % 14u) != 0u) continue;

            // 树干
            for (int i = 1; i <= 4; i++) {
                c.blocks[local_index(lx, gy + i, lz)] = BLK_LOG;
            }
            // 树冠
            int top = gy + 4;
            for (int dy = -1; dy <= 2; dy++) {
                int r = (dy <= 0) ? 2 : 1;
                for (int dx = -r; dx <= r; dx++) {
                    for (int dz = -r; dz <= r; dz++) {
                        if (dx == 0 && dz == 0 && dy <= 0) continue;
                        if (r == 2 && (std::abs(dx) == 2 && std::abs(dz) == 2)) continue;
                        int ax = lx + dx, az = lz + dz, ay = top + dy;
                        if (ax < 0 || ax >= CHUNK_S || az < 0 || az >= CHUNK_S ||
                            ay < 0 || ay >= CHUNK_H) continue;
                        if (c.blocks[local_index(ax, ay, az)] == BLK_AIR) {
                            c.blocks[local_index(ax, ay, az)] = BLK_LEAVES;
                        }
                    }
                }
            }
        }
    }

    // 3) 人工结构（房子）：基于世界坐标的确定性覆盖，跨 chunk 自动对齐
    for (int lx = 0; lx < CHUNK_S; lx++) {
        for (int lz = 0; lz < CHUNK_S; lz++) {
            int wx = cx * CHUNK_S + lx;
            int wz = cz * CHUNK_S + lz;
            for (int y = 0; y < CHUNK_H; y++) {
                int sb = structure_block(wx, y, wz, seed);
                if (sb >= 0) c.blocks[local_index(lx, y, lz)] = (uint8_t)sb;
            }
        }
    }

    // 4) 应用该 chunk 已记录的编辑（玩家挖/放可覆盖结构）
    apply_edits(c);
}

void VoxelWorld::update(float cam_x, float cam_z) {
    int pcx = floor_div((int)floorf(cam_x), CHUNK_S);
    int pcz = floor_div((int)floorf(cam_z), CHUNK_S);

    // 期望范围内找一个缺失 chunk，本帧只生成一个（分帧摊销，不掉帧）
    for (int dz = -CHUNK_LOAD_RADIUS; dz <= CHUNK_LOAD_RADIUS; dz++) {
        for (int dx = -CHUNK_LOAD_RADIUS; dx <= CHUNK_LOAD_RADIUS; dx++) {
            int cx = pcx + dx, cz = pcz + dz;
            if (find_chunk(cx, cz) != nullptr) continue;

            // 选槽：先无效槽，再选不在当前期望范围的槽
            int slot = -1;
            for (int i = 0; i < CHUNK_POOL_COUNT; i++) {
                if (!chunks[i].valid) { slot = i; break; }
            }
            if (slot < 0) {
                for (int i = 0; i < CHUNK_POOL_COUNT; i++) {
                    int ex = (chunks[i].cx - pcx);
                    int ez = (chunks[i].cz - pcz);
                    if (ex < -CHUNK_LOAD_RADIUS || ex > CHUNK_LOAD_RADIUS ||
                        ez < -CHUNK_LOAD_RADIUS || ez > CHUNK_LOAD_RADIUS) {
                        slot = i;
                        break;
                    }
                }
            }
            if (slot < 0) return;

            chunks[slot].cx = cx;
            chunks[slot].cz = cz;
            chunks[slot].valid = true;
            generate_chunk(chunks[slot], cx, cz);
            return;
        }
    }
}

// ---------------- 访问 ----------------

int VoxelWorld::get_block(int x, int y, int z) const {
    if (y < 0) return BLK_STONE;
    if (y >= CHUNK_H) return BLK_AIR;

    int cx = floor_div(x, CHUNK_S);
    int cz = floor_div(z, CHUNK_S);
    const Chunk *c = find_chunk(cx, cz);
    if (c == nullptr) return BLK_AIR;   // 未加载 chunk 按空气

    int lx = x - cx * CHUNK_S;
    int lz = z - cz * CHUNK_S;
    return c->blocks[local_index(lx, y, lz)];
}

void VoxelWorld::record_edit(int x, int y, int z, uint8_t b) {
    for (int i = 0; i < edit_count; i++) {
        if (edits[i].x == x && edits[i].y == y && edits[i].z == z) {
            edits[i].block = b;
            return;
        }
    }
    if (edit_count < MAX_EDITS) {
        edits[edit_count].x = x;
        edits[edit_count].y = y;
        edits[edit_count].z = z;
        edits[edit_count].block = b;
        edit_count++;
    }
}

void VoxelWorld::set_block(int x, int y, int z, uint8_t b) {
    if (y < 0 || y >= CHUNK_H) return;

    int cx = floor_div(x, CHUNK_S);
    int cz = floor_div(z, CHUNK_S);
    Chunk *c = find_chunk(cx, cz);
    if (c != nullptr) {
        int lx = x - cx * CHUNK_S;
        int lz = z - cz * CHUNK_S;
        c->blocks[local_index(lx, y, lz)] = b;
    }
    record_edit(x, y, z, b);
}

int VoxelWorld::surface_y(int x, int z) const {
    for (int y = CHUNK_H - 1; y >= 0; y--) {
        if (is_solid((uint8_t)get_block(x, y, z))) return y;
    }
    return -1;
}
