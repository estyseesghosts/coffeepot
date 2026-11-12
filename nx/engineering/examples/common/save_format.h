// An explicit, versioned, checksummed save format with two generation slots.
// Pure C, no libnx. File access goes through SaveIo so the host tests can inject failures at every step.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "world.h"

// On disk, little-endian, no padding:
//   0  magic "CPSV"      4
//   4  version (=1)      2
//   6  payload_len       2   (= SAVE_PAYLOAD_BYTES)
//   8  generation        4   (higher is newer)
//  12  x * 256, s32      4   fixed point: pixels * 256
//  16  y * 256, s32      4
//  20  score             4
//  24  collected bits    110
// 134  CRC-32 of bytes 0..133   4
#define SAVE_VERSION 1
#define SAVE_PAYLOAD_BYTES (12 + WORLD_COIN_BYTES)
#define SAVE_FILE_BYTES (12 + SAVE_PAYLOAD_BYTES + 4)   // 12-byte header, payload, CRC

typedef struct {
    int32_t x_fp, y_fp;                 // position in 1/256 pixel
    uint32_t score;
    uint8_t collected[WORLD_COIN_BYTES];
} SaveState;

typedef enum {
    SAVE_OK = 0,
    SAVE_ERR_SIZE,      // wrong length (truncated or oversized)
    SAVE_ERR_MAGIC,
    SAVE_ERR_VERSION,
    SAVE_ERR_LENGTH,    // payload_len field disagrees with this version
    SAVE_ERR_CRC,
    SAVE_ERR_RANGE,     // position outside the map, or bits/score inconsistent with the map
} SaveStatus;

uint32_t save_crc32(const void* data, size_t len);
void save_encode(uint8_t out[SAVE_FILE_BYTES], const SaveState* s, uint32_t generation);
SaveStatus save_decode(const uint8_t* in, size_t len, SaveState* s, uint32_t* generation);

// Storage abstraction: two slots, 0 and 1. Return 0 on success.
typedef struct {
    void* ctx;
    // Missing file: set *len = 0 and return 0.
    int (*read_slot)(void* ctx, int slot, uint8_t* buf, size_t cap, size_t* len);
    // Create/truncate, write, flush, sync, close. Non-zero if ANY step failed.
    int (*write_slot)(void* ctx, int slot, const uint8_t* buf, size_t len);
} SaveIo;

// Reads both slots, returns true with the newest valid state. *slot_out is the slot that holds it (-1 if none).
bool save_load(const SaveIo* io, SaveState* s, uint32_t* generation, int* slot_out);
// Writes the new state into the slot that does NOT hold the newest valid save, so that save is never touched.
// Returns true only if every write step reported success. On failure the previous save is still loadable.
bool save_store(const SaveIo* io, const SaveState* s);
