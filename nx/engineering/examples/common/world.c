#include "world.h"

const char* const WORLD_MAP[WORLD_H] = {
    "########################################",
    "#......................................#",
    "#..o.....#####........o.......####.....#",
    "#........#...#................#..#.....#",
    "#...###..#.o.#....######......#..#..o..#",
    "#...#.#..#...#....#....#......####.....#",
    "#...#.#..#####....#.o..#...............#",
    "#.o.###...........#....#.....o.........#",
    "#.................######...............#",
    "#.....o.......................#####....#",
    "#.............###.............#...#....#",
    "#.....####....#.#.....o.......#.o.#....#",
    "#.....#..#....#.#.............#...#....#",
    "#.....#.o#....###.............#####....#",
    "#.....####.............................#",
    "#..................o...................#",
    "#...........#######....................#",
    "#...o.......#.....#.........o..........#",
    "#...........#..o..#....................#",
    "#...........#######....................#",
    "#......................................#",
    "########################################",
};

static bool in_world(int x, int y) { return x >= 0 && y >= 0 && x < WORLD_W && y < WORLD_H; }

bool world_is_coin_cell(int x, int y) { return in_world(x, y) && WORLD_MAP[y][x] == 'o'; }

bool world_coin_collected(const uint8_t bits[WORLD_COIN_BYTES], int x, int y)
{
    if (!in_world(x, y)) { return false; }
    const int i = y * WORLD_W + x;
    return (bits[i / 8] >> (i % 8)) & 1u;
}

void world_collect_coin(uint8_t bits[WORLD_COIN_BYTES], int x, int y)
{
    if (!in_world(x, y)) { return; }
    const int i = y * WORLD_W + x;
    bits[i / 8] = (uint8_t)(bits[i / 8] | (1u << (i % 8)));
}

bool world_coin_present(const uint8_t bits[WORLD_COIN_BYTES], int x, int y)
{
    return world_is_coin_cell(x, y) && !world_coin_collected(bits, x, y);
}

uint32_t world_count_collected(const uint8_t bits[WORLD_COIN_BYTES])
{
    uint32_t n = 0;
    for (int i = 0; i < WORLD_COIN_BYTES; i++) {
        for (uint8_t b = bits[i]; b; b >>= 1) { n += b & 1u; }
    }
    return n;
}
