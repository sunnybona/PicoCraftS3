// PicoCraft 体素世界：基于 chunk 的流式世界（无 PSRAM，全部驻留 SRAM）。
// 世界在水平方向无限（受 int 坐标范围限制），高度固定 16。
// 玩家周围固定数量的 chunk 常驻内存，离开的 chunk 卸载；挖/放为稀疏编辑。
#ifndef VOXEL_WORLD_H
#define VOXEL_WORLD_H

#include <cstdint>

// 方块类型
enum BlockType : uint8_t {
    BLK_AIR = 0,
    BLK_GRASS = 1,
    BLK_DIRT = 2,
    BLK_STONE = 3,
    BLK_LOG = 4,
    BLK_LEAVES = 5,
    BLK_SAND = 6,
    BLK_PLANK = 7,
    BLK_WATER = 8,
    BLK_COBBLE = 9,
    BLK_BRICK = 10,
    BLK_GLASS = 11,
    BLK_BEDROCK = 12,
    BLK_SNOW = 13,
    BLK_GOLD = 14,
    BLK_COAL_ORE = 15,      // 煤矿石（浅层）
    BLK_IRON_ORE = 16,      // 铁矿石（中层）
    BLK_DIAMOND_ORE = 17,   // 钻石矿石（深层）
    BLK_CRAFT_TABLE = 18,   // 工作台
    BLK_TORCH = 19,         // 火炬（自发光，非完整方块）
    BLK_TNT = 20,           // TNT（可引爆）
};

// 方块类型总数
#define BLOCK_TYPE_COUNT 21

// 海平面高度（方块 y）；低于该高度的空气格生成水
#define SEA_LEVEL 4

// chunk 尺寸：水平 16x16，高度 16，整 chunk 4096 字节
#define CHUNK_S 16
#define CHUNK_H 16
#define CHUNK_VOL (CHUNK_S * CHUNK_H * CHUNK_S)

// 常驻 chunk 池：中心 chunk 外 LOAD_RADIUS 圈，共 (2R+1)^2 个
#define CHUNK_LOAD_RADIUS 1
#define CHUNK_POOL_N (2 * CHUNK_LOAD_RADIUS + 1)
#define CHUNK_POOL_COUNT (CHUNK_POOL_N * CHUNK_POOL_N)

// 稀疏编辑表容量（运行时内存，flash 另存）
#define MAX_EDITS 256

struct Chunk {
    int cx, cz;                 // chunk 坐标；世界方块 x = cx*CHUNK_S + lx
    bool valid;
    uint8_t blocks[CHUNK_VOL];  // 本地索引 [lx][y][lz]
};

// 一条稀疏编辑（世界坐标 + 方块）
struct EditEntry {
    int32_t x, y, z;
    uint8_t block;
};

class VoxelWorld {
public:
    Chunk chunks[CHUNK_POOL_COUNT];
    EditEntry edits[MAX_EDITS];
    int edit_count;
    uint32_t seed;

    VoxelWorld();

    // 重置管理器（清池与编辑表），记录种子
    void generate(uint32_t s);

    // 每帧驱动：以玩家位置为中心，分帧加载新进入的 chunk
    void update(float cam_x, float cam_z);

    // 世界坐标读取方块；未加载的 chunk 按空气处理（交互范围外不影响）
    int get_block(int x, int y, int z) const;

    // 世界坐标写入方块：更新 chunk 并记录稀疏编辑
    void set_block(int x, int y, int z, uint8_t b);

    // 某 (x,z) 列最高实心方块 y；整列空返回 -1
    int surface_y(int x, int z) const;

    static bool is_solid(uint8_t b) {
        return b != BLK_AIR && b != BLK_WATER && b != BLK_TORCH;
    }
    static bool is_water(uint8_t b) { return b == BLK_WATER; }

private:
    // 取已加载 chunk；未加载返回 nullptr
    Chunk* find_chunk(int cx, int cz);
    const Chunk* find_chunk(int cx, int cz) const;
    // 生成/重新生成一个 chunk（程序化地形 + 应用其内已记录的编辑）
    void generate_chunk(Chunk &c, int cx, int cz);
    // 记录/更新一条稀疏编辑
    void record_edit(int x, int y, int z, uint8_t b);
    // 应用编辑表中属于该 chunk 的条目
    void apply_edits(Chunk &c);
};

#endif
