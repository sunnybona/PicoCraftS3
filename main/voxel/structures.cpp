#include "structures.h"
#include "voxel_world.h"
#include "spawn_platform.h"
#include <math.h>

// 必须与 voxel_world.cpp 完全一致的确定性原语，保证结构与地形对齐。
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
    return ab * (1.0f - v) + v * cd;
}

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

static int floor_div(int a, int b) {
    int q = a / b;
    int r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) q--;
    return q;
}

static int column_height(int wx, int wz, uint32_t seed) {
    float n = fbm((float)wx * 0.12f, (float)wz * 0.12f, seed);
    int h = 2 + (int)(n * 8.0f);
    if (h > CHUNK_H - 6) h = CHUNK_H - 6;
    return h;
}

// ---------------- 房子 ----------------
#define HOUSE_GRID  40    // 每隔 40 格一个候选单元
#define HOUSE_S      6    // 占地 6x6
#define HOUSE_MAGIC  0x484f5553u   // "HOUS"

// 查询 (wx,wz) 所在的房子；不在任何房子内返回 false。
// 返回锚点 (ax,az) 与统一地面高度 gh。
static bool house_info(int wx, int wz, uint32_t seed,
                       int &ax, int &az, int &gh) {
    int gx = floor_div(wx, HOUSE_GRID);
    int gz = floor_div(wz, HOUSE_GRID);
    int ox = gx * HOUSE_GRID;
    int oz = gz * HOUSE_GRID;

    // 出生点强制房子：固定在出生平台左后半（x:-8..-3, z:-12..-7），平台已平整、
    // 无地形遮挡，近边距玩家 7 格，开局前方即可见。
    if (gx == 0 && gz == -1) {
        ax = -8; az = -12;
        int lx0 = wx - ax, lz0 = wz - az;
        if (lx0 < 0 || lx0 >= HOUSE_S || lz0 < 0 || lz0 >= HOUSE_S) return false;
        gh = SPAWN_PLATFORM_HIGH;                // 与平台统一高度对齐
        return true;
    }

    uint32_t h = hash2(gx, gz, seed ^ HOUSE_MAGIC);
    if ((h % 4u) != 0u) return false;          // 仅约 1/4 单元有房子

    // 锚点：保证 6x6 房子完整落在单元内、不贴边
    ax = ox + 5 + (int)((h >> 8)  % (uint32_t)(HOUSE_GRID - HOUSE_S - 10));
    az = oz + 5 + (int)((h >> 20) % (uint32_t)(HOUSE_GRID - HOUSE_S - 10));

    int lx = wx - ax, lz = wz - az;
    if (lx < 0 || lx >= HOUSE_S || lz < 0 || lz >= HOUSE_S) return false;

    // 统一地面高度取房子中心列；水中或太高的单元不放房子
    gh = column_height(ax + HOUSE_S / 2, az + HOUSE_S / 2, seed);
    if (gh <= SEA_LEVEL || gh >= CHUNK_H - 7) return false;
    return true;
}

// ---------------- 塔楼 ----------------
#define TOWER_GRID 48
#define TOWER_S 5
#define TOWER_H 9
#define TOWER_MAGIC 0x544f5745u   // "TOWE"

static bool tower_info(int wx, int wz, uint32_t seed,
                       int &ax, int &az, int &gh) {
    int gx = floor_div(wx, TOWER_GRID), gz = floor_div(wz, TOWER_GRID);
    int ox = gx * TOWER_GRID, oz = gz * TOWER_GRID;

    uint32_t h = hash2(gx, gz, seed ^ TOWER_MAGIC);
    if (h % 5u != 1u) return false;          // 约 1/5 单元有塔

    ax = ox + 6 + (int)((h >> 8)  % (uint32_t)(TOWER_GRID - TOWER_S - 12));
    az = oz + 6 + (int)((h >> 20) % (uint32_t)(TOWER_GRID - TOWER_S - 12));

    int lx = wx - ax, lz = wz - az;
    if (lx < 0 || lx >= TOWER_S || lz < 0 || lz >= TOWER_S) return false;

    gh = column_height(ax + TOWER_S / 2, az + TOWER_S / 2, seed);
    if (gh <= SEA_LEVEL || gh >= CHUNK_H - TOWER_H - 1) return false;
    return true;
}

static int tower_block(int wx, int y, int wz, uint32_t seed) {
    int ax, az, gh;
    if (!tower_info(wx, wz, seed, ax, az, gh)) return -1;

    int lx = wx - ax, lz = wz - az;
    int nat = column_height(wx, wz, seed);

    // 地基：用圆石填平
    if (y <= gh) {
        if (nat < gh && y > nat) return BLK_COBBLE;
        return -1;
    }
    int ry = y - gh;
    if (ry > TOWER_H) return -1;

    bool edge = (lx == 0 || lx == TOWER_S - 1 ||
                 lz == 0 || lz == TOWER_S - 1);

    // 顶层雉堞（交错）
    if (ry == TOWER_H) {
        if (!edge) return -1;
        return ((lx + lz) % 2 == 0) ? BLK_COBBLE : -1;
    }
    if (!edge) return -1;   // 内部空心

    // 门：南墙中间，高 2
    if (lz == 0 && lx == TOWER_S / 2 && (ry == 1 || ry == 2)) return -1;

    // 高层小窗
    if (ry == TOWER_H - 2 && edge &&
        (lx == 0 || lx == TOWER_S - 1) && lz == TOWER_S / 2)
        return BLK_GLASS;

    return BLK_COBBLE;
}

int structure_block(int wx, int y, int wz, uint32_t seed) {
    int tb = tower_block(wx, y, wz, seed);
    if (tb >= 0) return tb;

    int ax, az, gh;
    if (!house_info(wx, wz, seed, ax, az, gh)) return -1;

    int lx = wx - ax, lz = wz - az;
    int nat = column_height(wx, wz, seed);

    // 地基：自然地面低于统一地面时，用木板填平，避免房子悬空
    if (y <= gh) {
        if (nat < gh && y > nat) return BLK_PLANK;
        return -1;
    }

    // 地板
    if (y == gh + 1) return BLK_PLANK;

    // 屋顶
    if (y == gh + 5) return BLK_PLANK;

    // 墙体区间（gh+2 .. gh+4）
    if (y >= gh + 2 && y <= gh + 4) {
        bool edge = (lx == 0 || lx == HOUSE_S - 1 ||
                     lz == 0 || lz == HOUSE_S - 1);
        if (!edge) return -1;                      // 室内空气

        // 门：南墙（lz=0）中间，门高 2 格
        if (lz == 0 && lx == HOUSE_S / 2 &&
            (y == gh + 2 || y == gh + 3)) return -1;

        // 窗：门两侧上方、对面墙、侧墙，玻璃
        if (y == gh + 3) {
            bool win =
                (lz == HOUSE_S - 1 && (lx == 1 || lx == HOUSE_S - 2)) ||
                ((lx == 0 || lx == HOUSE_S - 1) && lz == HOUSE_S / 2);
            if (win) return BLK_GLASS;
        }
        return BLK_PLANK;
    }

    return -1;
}
