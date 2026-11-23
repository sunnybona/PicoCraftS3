// 玩家物理：AABB 分轴碰撞 + 重力 + 自动越过一格障碍（auto-jump）+ 水中浮力。
// 位置直接读写全局 camera_position（眼睛位置），脚位置由眼睛高度反推。
#ifndef PLAYER_PHYSICS_H
#define PLAYER_PHYSICS_H

#include "voxel_world.h"

// 玩家身体尺寸（米）
#define PLAYER_HALF_WIDTH 0.30f
#define PLAYER_HEIGHT     1.80f
#define PLAYER_EYE        1.62f   // 脚到眼睛的高度

// 每帧（固定步长）物理参数
#define GRAVITY         0.045f
#define MAX_FALL        0.90f
#define JUMP_SPEED      0.42f
#define WATER_GRAVITY   0.012f
#define WATER_BUOYANCY  0.030f
#define WATER_MAX_FALL  0.15f
#define WATER_MAX_RISE  0.20f

class PlayerPhysics {
public:
    float vx, vy, vz;
    bool on_ground;
    bool in_water;
    bool just_landed;   // 本帧发生向下碰撞（落地），用于音效

    PlayerPhysics();
    void reset();

    // 请求一次跳跃（边沿触发调用）；地面起跳，水中向上游
    void request_jump();

    // 期望水平位移（世界系，调用方已按 yaw 计算）；本函数直接更新全局 camera_position
    void step(const VoxelWorld &world, float wish_x, float wish_z);

private:
    bool jump_requested;

    // 给定中心 (x,z) 与脚 y 的 AABB 是否与任何实心方块相交
    static bool collides(const VoxelWorld &world, float x, float feet_y, float z);
    // 玩家身体是否处于水中（检查眼睛与脚两个采样点）
    static bool body_in_water(const VoxelWorld &world, float x, float feet_y, float z);
};

#endif
