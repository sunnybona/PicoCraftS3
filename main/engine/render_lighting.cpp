// PicoCraft 不使用 Pico3D 的动态点光源系统。
// 方向光照在体素面提取阶段直接烘焙进每个面的颜色（顶面亮、侧面中、底面暗），
// 因此这里把 render_triangle() 调用的 render_lighting 实现为空操作，
// 并定义全局声明的 light_falloff（本工程不使用，仅满足链接）。
#include "render_globals.h"
#ifdef ESP_PLATFORM
#include "esp_attr.h"
#endif

int8_t light_falloff = 0;

#ifdef ESP_PLATFORM
IRAM_ATTR
#endif
void render_lighting(struct triangle_32 &in) {
    (void)in;
}
