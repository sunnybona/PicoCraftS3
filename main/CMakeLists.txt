# PicoCraft ESP32-S3 component.
#
# All game sources are compiled into this single component. The include
# directories are flattened so that engine/*.cpp and voxel/*.cpp can keep
# their original bare `#include "foo.h"` statements, while main.cpp uses the
# `engine/...` / `voxel/...` paths (resolved against the component root).
#
# Note: engine/shader_wireframe.cpp is #included directly by
# render_rasterize.cpp and must NOT be listed as a separate translation unit.

idf_component_register(
    SRCS
        # Entry point and platform shims
        app_main.cpp
        main.cpp
        input.cpp
        items.cpp
        player_state.cpp
        item_drops.cpp
        crafting.cpp
        day_night.cpp
        tnt.cpp
        game_menu.cpp
        test_scenes.cpp
        picosystem_compat.cpp
        st7789_display.cpp

        # Render engine (shader_wireframe.cpp is included, not compiled)
        engine/render_camera.cpp
        engine/render_clipping.cpp
        engine/render_culling.cpp
        engine/render_lighting.cpp
        engine/render_rasterize.cpp
        engine/render_triangle.cpp

        # Voxel game (audio sources excluded in stage 1; save_store uses NVS)
        voxel/block_textures.cpp
        voxel/entities.cpp
        voxel/player_physics.cpp
        voxel/save_store.cpp
        voxel/structures.cpp
        voxel/terrain_skyline.cpp
        voxel/voxel_mesh.cpp
        voxel/voxel_ray.cpp
        voxel/voxel_world.cpp

    INCLUDE_DIRS
        "."
        "engine"
        "voxel"

    PRIV_REQUIRES
        esp_driver_spi
        esp_driver_gpio
        nvs_flash
        esp_timer
)

# NO_GLOBAL_OFFSET: the world is small (32x32), so Pico3D's global coordinate
# offset path is disabled. RASTERIZER_IN_FLASH: compile the rasterizer from
# flash instead of the RP __scratch_x section (and avoid pulling in pico.h).
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    NO_GLOBAL_OFFSET
    RASTERIZER_IN_FLASH
)

# C++17 is used throughout the original codebase.
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)
