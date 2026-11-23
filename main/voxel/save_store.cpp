#include "save_store.h"
#include <string.h>

#include "nvs.h"
#include "nvs_flash.h"
#include "player_state.h"

// Single programming buffer: header(16) + up to 256 records(4096) + player
// snapshot, rounded up to a 256-page boundary = 4352.
#define SAVE_PROGRAM_BYTES 4352u

static_assert(SAVE_PLAYER_OFFSET + PLAYER_BLOB_V3_BYTES <= SAVE_PROGRAM_BYTES,
              "save image too small for player snapshot");

static const char *NVS_NAMESPACE = "picocraft";
static const char *SLOT_KEY[SAVE_SLOT_COUNT] = {"sv0", "sv1", "sv2"};
static const char *LEGACY_KEY = "save";   // pre-slot single-blob key

// FNV-1a over n bytes.
static uint32_t fnv(const uint8_t *p, size_t n) {
    uint32_t s = 0x811c9dc5u;
    for (size_t i = 0; i < n; i++) {
        s ^= p[i];
        s *= 16777619u;
    }
    return s;
}

// Apply the fields shared by v2 and v3 blobs.
static void apply_common(const PlayerSaveBlob &b) {
    for (int i = 0; i < ITEM_ID_COUNT; i++) {
        player.inv[i] = b.inv[i];
    }

    int hp = b.hp;
    if (hp < 1) hp = 1;
    if (hp > PLAYER_MAX_HP) hp = PLAYER_MAX_HP;
    player.hp = static_cast<int8_t>(hp);

    uint32_t dt = b.day_time;
    if (dt >= DAY_LENGTH) dt = DAY_LENGTH - 1;
    player.day_time = static_cast<uint16_t>(dt);

    player.achievements = b.achievements;

    int hi = b.hand_index;
    if (hi < 0 || hi >= HAND_BAR_N) hi = 0;
    player.hand_index = hi;

    player.hurt_cooldown = 0;
    player.toast_time = 0;
    player.respawn_pending = false;
}

// Load one NVS key into the world/player.
static bool load_from_key(const char *key, VoxelWorld &world, bool *restored) {
    if (restored) *restored = false;

    static uint8_t buf[SAVE_PROGRAM_BYTES];
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }
    size_t len = sizeof(buf);
    esp_err_t err = nvs_get_blob(handle, key, buf, &len);
    nvs_close(handle);

    if (err != ESP_OK || len < sizeof(SaveHeader)) {
        return false;
    }

    SaveHeader hdr;
    memcpy(&hdr, buf, sizeof(hdr));
    if (hdr.magic != SAVE_MAGIC || (hdr.version != 2u && hdr.version != 3u)) {
        return false;
    }

    uint32_t count = hdr.count;
    if (count > MAX_EDITS) count = MAX_EDITS;

    world.edit_count = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (sizeof(SaveHeader) + (i + 1) * sizeof(SaveRecord) > len) break;
        SaveRecord rec;
        memcpy(&rec, buf + sizeof(SaveHeader) + i * sizeof(rec), sizeof(rec));
        if (rec.y < 0 || rec.y >= CHUNK_H) continue;
        world.edits[world.edit_count].x = rec.x;
        world.edits[world.edit_count].y = rec.y;
        world.edits[world.edit_count].z = rec.z;
        world.edits[world.edit_count].block = rec.block;
        world.edit_count++;
    }

    if (len < SAVE_PLAYER_OFFSET + 4) {
        return true;
    }
    const uint8_t *pb = buf + SAVE_PLAYER_OFFSET;
    uint32_t pmagic;
    memcpy(&pmagic, pb, 4);
    if (pmagic != PLAYER_BLOB_MAGIC) {
        return true;
    }

    if (hdr.version == 3u && len >= SAVE_PLAYER_OFFSET + PLAYER_BLOB_V3_BYTES) {
        if (fnv(pb, 96) != *reinterpret_cast<const uint32_t *>(pb + 96)) {
            return true;
        }
        PlayerSaveBlob b;
        memset(&b, 0, sizeof(b));
        memcpy(&b, pb, sizeof(b));
        apply_common(b);
        player.cam_x = b.px;
        player.cam_y = b.py;
        player.cam_z = b.pz;
        player.cam_yaw = b.yaw;
        player.cam_pitch = b.pitch;
        player.have_cam_pos = true;
        if (restored) *restored = true;
    } else if (hdr.version == 2u &&
               len >= SAVE_PLAYER_OFFSET + PLAYER_BLOB_V2_BYTES) {
        if (fnv(pb, 76) != *reinterpret_cast<const uint32_t *>(pb + 76)) {
            return true;
        }
        PlayerSaveBlob b;
        memset(&b, 0, sizeof(b));
        memcpy(&b, pb, 76);   // common prefix only; position stays default
        apply_common(b);
        player.have_cam_pos = false;
        if (restored) *restored = true;
    }
    return true;
}

bool SaveStore::slot_exists(int slot) {
    if (slot < 0 || slot >= SAVE_SLOT_COUNT) return false;
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }
    size_t len = 0;
    esp_err_t err = nvs_get_blob(handle, SLOT_KEY[slot], nullptr, &len);
    nvs_close(handle);
    return err == ESP_OK && len >= sizeof(SaveHeader);
}

bool SaveStore::load_slot(int slot, VoxelWorld &world, bool *player_restored) {
    if (slot < 0 || slot >= SAVE_SLOT_COUNT) return false;
    if (load_from_key(SLOT_KEY[slot], world, player_restored)) {
        return true;
    }
    // Slot 0 transparently adopts the pre-slot legacy blob if present.
    if (slot == 0) {
        return load_from_key(LEGACY_KEY, world, player_restored);
    }
    return false;
}

bool SaveStore::save_slot(int slot, const VoxelWorld &world) {
    if (slot < 0 || slot >= SAVE_SLOT_COUNT) return false;

    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        return false;
    }

    static uint8_t program_buf[SAVE_PROGRAM_BYTES];
    memset(program_buf, 0xFF, sizeof(program_buf));

    SaveHeader hdr;
    hdr.magic = SAVE_MAGIC;
    hdr.version = SAVE_VERSION;
    hdr.seed = world.seed;
    hdr.count = (uint32_t)world.edit_count;
    memcpy(program_buf, &hdr, sizeof(hdr));

    for (int i = 0; i < world.edit_count; i++) {
        SaveRecord rec;
        rec.x = world.edits[i].x;
        rec.y = world.edits[i].y;
        rec.z = world.edits[i].z;
        rec.block = world.edits[i].block;
        rec.pad[0] = rec.pad[1] = rec.pad[2] = 0xFF;
        memcpy(program_buf + sizeof(hdr) + i * sizeof(rec), &rec, sizeof(rec));
    }

    PlayerSaveBlob pb;
    memset(&pb, 0, sizeof(pb));
    pb.magic = PLAYER_BLOB_MAGIC;
    for (int i = 0; i < ITEM_ID_COUNT; i++) {
        pb.inv[i] = player.inv[i];
    }

    int hp = player.hp;
    if (hp < 1) hp = 1;
    if (hp > PLAYER_MAX_HP) hp = PLAYER_MAX_HP;
    pb.hp = static_cast<int8_t>(hp);

    pb.day_time = static_cast<uint16_t>(player.day_time % DAY_LENGTH);
    pb.achievements = player.achievements;

    int hi = player.hand_index;
    if (hi < 0 || hi >= HAND_BAR_N) hi = 0;
    pb.hand_index = hi;

    // Camera position/orientation are supplied by the game via the player
    // camera fields (set each frame before the debounced save fires).
    pb.px = player.cam_x;
    pb.py = player.cam_y;
    pb.pz = player.cam_z;
    pb.yaw = player.cam_yaw;
    pb.pitch = player.cam_pitch;

    pb.checksum = fnv(reinterpret_cast<const uint8_t *>(&pb), 96);
    memcpy(program_buf + SAVE_PLAYER_OFFSET, &pb, sizeof(pb));

    esp_err_t err = nvs_set_blob(handle, SLOT_KEY[slot],
                                 program_buf, SAVE_PROGRAM_BYTES);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err == ESP_OK;
}
