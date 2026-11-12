// The first-game tile map and its coin bookkeeping. Pure C, no libnx: the host tests in nx/tests use it too.
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define WORLD_W 40
#define WORLD_H 22
#define WORLD_CELLS (WORLD_W * WORLD_H)            // 880 cells
#define WORLD_COIN_BYTES ((WORLD_CELLS + 7) / 8)   // 110 bytes of "collected" bits

extern const char* const WORLD_MAP[WORLD_H];       // '#' wall, '.' floor, 'o' coin

bool world_is_coin_cell(int x, int y);             // does the map put a coin here?
bool world_coin_collected(const uint8_t bits[WORLD_COIN_BYTES], int x, int y);
void world_collect_coin(uint8_t bits[WORLD_COIN_BYTES], int x, int y);
bool world_coin_present(const uint8_t bits[WORLD_COIN_BYTES], int x, int y);  // map coin and not collected
uint32_t world_count_collected(const uint8_t bits[WORLD_COIN_BYTES]);
