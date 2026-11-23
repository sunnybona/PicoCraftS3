// Persistence for sparse block edits + player snapshot, in multiple NVS slots.
// ESP32-S3 port: each save slot is one NVS blob under the same namespace.
// The on-disk record layout for block edits stays identical to the
// RP2040/RP2350 original; v3 adds the player camera position/orientation.
#ifndef SAVE_STORE_H
#define SAVE_STORE_H

#include <cstdint>
#include <cstddef>
#include "voxel_world.h"
#include "items.h"

#define SAVE_MAGIC   0x504342Cu   // "PCBL"
#define SAVE_VERSION 3u

// Number of real user save slots (NVS keys sv0..sv2).
#define SAVE_SLOT_COUNT 3

// Local flash sector size used only to size the save image.
#define FLASH_SECTOR_SIZE 4096u

// Reserve 2 sectors (8192)
#define SAVE_SECTORS 2u
#define SAVE_SIZE    (SAVE_SECTORS * FLASH_SECTOR_SIZE)

struct SaveHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t seed;
    uint32_t count;
};

struct SaveRecord {
    int32_t x, y, z;
    uint8_t block;
    uint8_t pad[3];
};

// Records occupy the region after the header (MAX_EDITS slots); the player
// snapshot is stored at a fixed offset beyond that region.
#define SAVE_RECORDS_MAX  256u
#define SAVE_PLAYER_OFFSET (sizeof(SaveHeader) + SAVE_RECORDS_MAX * sizeof(SaveRecord))

// Fixed-layout player snapshot. Only fields worth keeping across reboots are
// stored; transient combat/UI state (hurt cooldown, toast, respawn flag) is
// excluded. v3 adds camera position (px,py,pz) and orientation (yaw,pitch).
#define PLAYER_BLOB_MAGIC 0x50534156u   // "PSAV"
struct PlayerSaveBlob {
    uint32_t magic;                 // 0
    uint16_t inv[ITEM_ID_COUNT];    // 4..61
    int8_t   hp;                    // 62
    uint8_t  pad0;                  // 63
    uint16_t day_time;              // 64..65
    uint16_t pad1;                  // 66..67 (keep 4-byte alignment)
    uint32_t achievements;          // 68..71
    int32_t  hand_index;            // 72..75
    float    px, py, pz;            // 76..87
    float    yaw, pitch;            // 88..95
    uint32_t checksum;              // 96..99
};

// Byte length of the v2 player blob (no position fields); used to read saves
// written by the previous firmware.
#define PLAYER_BLOB_V2_BYTES 80u
#define PLAYER_BLOB_V3_BYTES (unsigned)sizeof(PlayerSaveBlob)

class SaveStore {
public:
    // True if a valid blob exists in the given slot (0..SAVE_SLOT_COUNT-1).
    static bool slot_exists(int slot);

    // Load a slot into the world and, when present, the player snapshot.
    // player_restored is set true only when a valid player blob was restored.
    static bool load_slot(int slot, VoxelWorld &world,
                          bool *player_restored = nullptr);

    // Write the world edits and player snapshot into a slot.
    static bool save_slot(int slot, const VoxelWorld &world);
};

#endif
