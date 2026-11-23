#include "day_night.h"
#include "player_state.h"
#include "voxel/voxel_world.h"
#include "engine/render_globals.h"

uint8_t g_light_shade = 15;

static color_t mk_sky(uint8_t r, uint8_t g, uint8_t b) {
    return (color_t)(((g & 15) << 12) | ((b & 15) << 8) | (r & 15));
}

static color_t lerp_sky(color_t a, color_t b, int f, int fmax) {
    uint8_t ar=a&15, ag=(a>>12)&15, ab=(a>>8)&15;
    uint8_t br=b&15, bg=(b>>12)&15, bb=(b>>8)&15;
    uint8_t r = ar + (int)(br-ar)*f/fmax;
    uint8_t g = ag + (int)(bg-ag)*f/fmax;
    uint8_t bl = ab + (int)(bb-ab)*f/fmax;
    return mk_sky(r, g, bl);
}

bool is_night_time() {
    int t = player.day_time;
    return t >= 1250 && t < 2150;
}

void update_day_night(const VoxelWorld &world, float px, float py, float pz) {
    player.day_time = (uint16_t)((player.day_time + 1) % DAY_LENGTH);
    int t = player.day_time;

    // 基础亮度
    int shade;
    if (t < 1000)       shade = 15;
    else if (t < 1250)  shade = 15 - (t - 1000) * 10 / 250;
    else if (t < 2150)  shade = 5;
    else                shade = 5 + (t - 2150) * 10 / 250;

    // 天空色（含黄昏/黎明过渡）
    const color_t c_day = mk_sky(7, 11, 13);
    const color_t c_night = mk_sky(2, 3, 7);
    const color_t c_dusk = mk_sky(13, 7, 5);
    if (t < 1000) {
        sky = c_day;
    } else if (t < 1125) {
        sky = lerp_sky(c_day, c_dusk, t - 1000, 125);
    } else if (t < 1250) {
        sky = lerp_sky(c_dusk, c_night, t - 1125, 125);
    } else if (t < 2150) {
        sky = c_night;
    } else if (t < 2275) {
        sky = lerp_sky(c_night, c_dusk, t - 2150, 125);
    } else {
        sky = lerp_sky(c_dusk, c_day, t - 2275, 125);
    }

    // 火炬加成：仅在非全亮时检测玩家周围 3 格
    if (shade < 15) {
        int ix = (int)px, iy = (int)py, iz = (int)pz;
        bool found = false;
        for (int dx = -3; dx <= 3 && !found; dx++) {
            for (int dz = -3; dz <= 3 && !found; dz++) {
                for (int dy = -1; dy <= 2 && !found; dy++) {
                    if (world.get_block(ix+dx, iy+dy, iz+dz) == BLK_TORCH)
                        found = true;
                }
            }
        }
        if (found && shade < 11) shade = 11;
    }

    g_light_shade = (uint8_t)shade;
}

uint16_t apply_light(uint16_t c, bool emissive) {
    if (emissive || g_light_shade >= 15) return c;
    uint8_t r = (c & 15), b = ((c >> 8) & 15), g = ((c >> 12) & 15);
    r = (uint8_t)((r * g_light_shade) >> 4);
    b = (uint8_t)((b * g_light_shade) >> 4);
    g = (uint8_t)((g * g_light_shade) >> 4);
    return (uint16_t)((g << 12) | (b << 8) | (c & 0x00F0) | r);
}
