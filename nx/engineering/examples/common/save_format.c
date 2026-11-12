#include "save_format.h"
#include <string.h>

uint32_t save_crc32(const void* data, size_t len)
{
    const uint8_t* p = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (int k = 0; k < 8; k++) { crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u))); }
    }
    return ~crc;
}

static void put16(uint8_t* p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t* p, uint32_t v) { put16(p, v & 0xFFFFu); put16(p + 2, v >> 16); }
static uint32_t get16(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8); }
static uint32_t get32(const uint8_t* p) { return get16(p) | (get16(p + 2) << 16); }

void save_encode(uint8_t out[SAVE_FILE_BYTES], const SaveState* s, uint32_t generation)
{
    memcpy(out, "CPSV", 4);
    put16(out + 4, SAVE_VERSION);
    put16(out + 6, SAVE_PAYLOAD_BYTES);
    put32(out + 8, generation);
    put32(out + 12, (uint32_t)s->x_fp);
    put32(out + 16, (uint32_t)s->y_fp);
    put32(out + 20, s->score);
    memcpy(out + 24, s->collected, WORLD_COIN_BYTES);
    put32(out + 24 + WORLD_COIN_BYTES, save_crc32(out, 24 + WORLD_COIN_BYTES));
}

SaveStatus save_decode(const uint8_t* in, size_t len, SaveState* s, uint32_t* generation)
{
    if (len != SAVE_FILE_BYTES) { return SAVE_ERR_SIZE; }
    if (memcmp(in, "CPSV", 4) != 0) { return SAVE_ERR_MAGIC; }
    if (get16(in + 4) != SAVE_VERSION) { return SAVE_ERR_VERSION; }
    if (get16(in + 6) != SAVE_PAYLOAD_BYTES) { return SAVE_ERR_LENGTH; }
    if (get32(in + 24 + WORLD_COIN_BYTES) != save_crc32(in, 24 + WORLD_COIN_BYTES)) { return SAVE_ERR_CRC; }

    SaveState t;
    t.x_fp = (int32_t)get32(in + 12);
    t.y_fp = (int32_t)get32(in + 16);
    t.score = get32(in + 20);
    memcpy(t.collected, in + 24, WORLD_COIN_BYTES);

    // A valid CRC only proves the bytes are the ones that were written, so also check what they mean.
    const int64_t max_x = (int64_t)WORLD_W * 32 * 256, max_y = (int64_t)WORLD_H * 32 * 256;   // 32 px tiles
    if (t.x_fp < 0 || t.y_fp < 0 || t.x_fp >= max_x || t.y_fp >= max_y) { return SAVE_ERR_RANGE; }
    for (int y = 0; y < WORLD_H; y++) {
        for (int x = 0; x < WORLD_W; x++) {
            if (world_coin_collected(t.collected, x, y) && !world_is_coin_cell(x, y)) { return SAVE_ERR_RANGE; }
        }
    }
    if (t.score != world_count_collected(t.collected)) { return SAVE_ERR_RANGE; }

    *s = t;
    *generation = get32(in + 8);
    return SAVE_OK;
}

static bool read_valid(const SaveIo* io, int slot, SaveState* s, uint32_t* gen)
{
    uint8_t buf[SAVE_FILE_BYTES + 1];                   // one spare byte so an oversized file is seen as oversized
    size_t len = 0;
    if (io->read_slot(io->ctx, slot, buf, sizeof buf, &len) != 0) { return false; }
    return save_decode(buf, len, s, gen) == SAVE_OK;
}

bool save_load(const SaveIo* io, SaveState* s, uint32_t* generation, int* slot_out)
{
    SaveState a, b;
    uint32_t ga = 0, gb = 0;
    const bool va = read_valid(io, 0, &a, &ga);
    const bool vb = read_valid(io, 1, &b, &gb);
    if (!va && !vb) {
        if (slot_out) { *slot_out = -1; }
        return false;
    }
    const bool use_b = vb && (!va || gb > ga);
    *s = use_b ? b : a;
    if (generation) { *generation = use_b ? gb : ga; }
    if (slot_out) { *slot_out = use_b ? 1 : 0; }
    return true;
}

bool save_store(const SaveIo* io, const SaveState* s)
{
    SaveState cur;
    uint32_t gen = 0;
    int slot = -1;
    const bool have = save_load(io, &cur, &gen, &slot);
    const int target = have ? 1 - slot : 0;             // never overwrite the newest valid save
    uint8_t buf[SAVE_FILE_BYTES];
    save_encode(buf, s, have ? gen + 1 : 1);
    return io->write_slot(io->ctx, target, buf, sizeof buf) == 0;
}
