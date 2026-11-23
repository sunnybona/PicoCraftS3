// PicoCraft：纹理表由 voxel/block_textures 在启动时程序化生成（驻留 SRAM）。
// render_rasterize 通过这里的 extern 声明访问 chunk_texture_list（shader 100/101）。
#include "voxel/block_textures.h"

extern const struct texture chunk_texture_list[TEX_COUNT];
