#include "voxel_mesh.h"
#include "../engine/render_globals.h"
#include "../engine/render_math.h"
#include "block_textures.h"
#ifdef ESP_PLATFORM
#include "esp_attr.h"
#define HOT_FUNC IRAM_ATTR
#else
#define HOT_FUNC
#endif

// 6 个面：带符号法线 + 4 个局部角（0/1）。
// 角点绕序已验证：从外侧看为 CCW，法线指向外侧，能正确通过背面剔除。
namespace {

struct FaceDef {
    int normal[3];
    int corner[4][3];
};

const FaceDef FACES[6] = {
    // TOP (+Y)
    { {0, 1, 0}, { {0,1,0},{0,1,1},{1,1,1},{1,1,0} } },
    // BOTTOM (-Y)
    { {0,-1, 0}, { {0,0,0},{1,0,0},{1,0,1},{0,0,1} } },
    // +Z
    { {0, 0, 1}, { {0,0,1},{1,0,1},{1,1,1},{0,1,1} } },
    // -Z
    { {0, 0,-1}, { {0,0,0},{0,1,0},{1,1,0},{1,0,0} } },
    // +X
    { {1, 0, 0}, { {1,0,1},{1,0,0},{1,1,0},{1,1,1} } },
    // -X
    { {-1, 0, 0}, { {0,0,1},{0,1,1},{0,1,0},{0,0,0} } },
};

// 方块基础 RGB（0..15），HUD 用
struct BaseRGB { uint8_t r, g, b; };
const BaseRGB BLOCK_COLOR[BLOCK_TYPE_COUNT] = {
    {0, 0, 0},      // AIR
    {4, 12, 4},     // GRASS
    {9, 6, 3},      // DIRT
    {9, 9, 10},     // STONE
    {10, 7, 4},     // LOG
    {3, 9, 3},      // LEAVES
    {13, 13, 9},    // SAND
    {12, 9, 5},     // PLANK
    {3, 6, 13},     // WATER
    {7, 7, 8},     // COBBLE
    {12, 5, 4},     // BRICK
    {12, 14, 15},   // GLASS
    {3, 3, 4},      // BEDROCK
    {14, 14, 15},   // SNOW
    {15, 12, 3},    // GOLD
    {6, 6, 7},      // COAL_ORE
    {10, 9, 9},     // IRON_ORE
    {8, 12, 13},    // DIAMOND_ORE
    {12, 9, 5},     // CRAFT_TABLE
    {15, 12, 4},    // TORCH
    {13, 4, 3},     // TNT
};

} // namespace

void block_rgb(uint8_t type, uint8_t &r, uint8_t &g, uint8_t &b) {
    if (type >= BLOCK_TYPE_COUNT) type = 0;
    const BaseRGB &c = BLOCK_COLOR[type];
    r = c.r; g = c.g; b = c.b;
}

// 顶点色（GBAR）
static inline color_t vrgb(uint8_t r, uint8_t g, uint8_t b) {
    return (color_t)(((g & 15) << 12) | ((b & 15) << 8) | (r & 15));
}

// 提交一个纯色立方体（sh 参数指定 shader：1=普通受光，3=自发光）
static HOT_FUNC void emit_plain_box(float x0, float y0, float z0,
                                    float x1, float y1, float z1,
                                    color_t col, int sh) {
    const float p[8][3] = {
        {x0,y0,z0},{x1,y0,z0},{x1,y0,z1},{x0,y0,z1},
        {x0,y1,z0},{x1,y1,z0},{x1,y1,z1},{x0,y1,z1},
    };
    static const uint8_t faces[6][4] = {
        {4,7,6,5},{0,1,2,3},{3,2,6,7},{0,4,5,1},{2,1,5,6},{3,7,4,0},
    };
    triangle_32 t;
    t.shader_id = (uint8_t)sh;
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

// 火炬：细柄 + 顶部火焰（自发光 shader 3，夜晚不被压暗）
static HOT_FUNC void emit_torch(int bx, int by, int bz) {
    float x = (float)bx, y = (float)by, z = (float)bz;
    emit_plain_box(x+0.42f, y+0.05f, z+0.42f,
                   x+0.58f, y+0.62f, z+0.58f, vrgb(9, 6, 2), 3);
    emit_plain_box(x+0.38f, y+0.62f, z+0.38f,
                   x+0.62f, y+0.82f, z+0.62f, vrgb(15, 9, 2), 3);
    emit_plain_box(x+0.42f, y+0.80f, z+0.42f,
                   x+0.58f, y+0.92f, z+0.58f, vrgb(15, 14, 6), 3);
}

// 纹理面：shader 100 + 该面的 UV 映射（文件内函数）
static HOT_FUNC void emit_textured_face(int bx, int by, int bz,
                               const FaceDef &fd, int face_idx,
                               uint8_t block) {
    // 法线轴与两个切向轴
    int naxis = 0;
    for (int a = 0; a < 3; a++) if (fd.normal[a] != 0) naxis = a;
    int t0 = (naxis + 1) % 3, t1 = (naxis + 2) % 3;

    int32_t p[4][3];
    uint8_t uv[4][2];
    for (int i = 0; i < 4; i++) {
        p[i][0] = float_to_int((float)(bx + fd.corner[i][0]));
        p[i][1] = float_to_int((float)(by + fd.corner[i][1]));
        p[i][2] = float_to_int((float)(bz + fd.corner[i][2]));
        uv[i][0] = (uint8_t)(fd.corner[i][t0] * TEX_SIZE);
        uv[i][1] = (uint8_t)(fd.corner[i][t1] * TEX_SIZE);
    }

    triangle_32 t;
    t.shader_id = 100;
    // 草顶面（face_idx==0）按世界坐标成片决定带花/无花，其余面用常规映射
    t.texture_id = (block == BLK_GRASS && face_idx == 0)
                   ? grass_top_texture_id(bx, bz)
                   : block_texture_id(block, face_idx);

    const int idx[2][3] = { {0,1,2}, {0,2,3} };
    for (int ti = 0; ti < 2; ti++) {
        vertex_32 *v[3] = {&t.vertex1,&t.vertex2,&t.vertex3};
        for (int k = 0; k < 3; k++) {
            int ci = idx[ti][k];
            v[k]->x = p[ci][0];
            v[k]->y = p[ci][1];
            v[k]->z = p[ci][2];
        }
        t.vertex_parameter1.uv[0] = uv[idx[ti][0]][0];
        t.vertex_parameter1.uv[1] = uv[idx[ti][0]][1];
        t.vertex_parameter2.uv[0] = uv[idx[ti][1]][0];
        t.vertex_parameter2.uv[1] = uv[idx[ti][1]][1];
        t.vertex_parameter3.uv[0] = uv[idx[ti][2]][0];
        t.vertex_parameter3.uv[1] = uv[idx[ti][2]][1];
        render_triangle(t);
    }
}

HOT_FUNC void VoxelMesh::build(const VoxelWorld &world, float cam_x, float cam_z, int radius) {
    int cx = (int)cam_x;
    int cz = (int)cam_z;

    for (int dx = -radius; dx <= radius; dx++) {
        for (int dz = -radius; dz <= radius; dz++) {
            int bx = cx + dx;
            int bz = cz + dz;
            if (dx * dx + dz * dz > radius * radius) continue;   // 圆形视距

            for (int by = 0; by < CHUNK_H; by++) {
                int b = world.get_block(bx, by, bz);
                if (b == BLK_AIR) continue;
                if (b == BLK_TORCH) { emit_torch(bx, by, bz); continue; }

                for (int f = 0; f < 6; f++) {
                    const FaceDef &fd = FACES[f];
                    int neighbor = world.get_block(bx + fd.normal[0],
                                                   by + fd.normal[1],
                                                   bz + fd.normal[2]);
                    if (neighbor != BLK_AIR) continue;

                    emit_textured_face(bx, by, bz, fd, f, (uint8_t)b);

                    if (number_triangles >= MAX_RENDER_TRIANGLES - 2) return;
                }
            }
        }
    }
}
