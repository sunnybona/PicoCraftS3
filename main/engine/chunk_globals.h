// PicoCraft 精简版 chunk 定义。
// 本工程定义了 NO_GLOBAL_OFFSET，全局偏移相关代码会被预处理关闭，
// 这里仅保留引擎文件 #include "chunk_globals.h" 所需的最小宏，避免编译错误。
#include "render_math.h"

#define CHUNK_UNITS 10
#define CHUNK_SIZE (FIXED_POINT_FACTOR * CHUNK_UNITS)
