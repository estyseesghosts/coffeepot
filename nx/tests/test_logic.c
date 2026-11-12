// Host-side logic tests for nx/engineering/examples/common: save format, coin persistence, PCM padding.
// Build and run: bash nx/tests/run.sh   (plain C11, no libnx, no console)
#include <stdio.h>
#include <string.h>
#include "save_format.h"
#include "pcm_pad.h"
#include "world.h"

static int g_checks, g_failed;
#define CHECK(cond) do { g_checks++; if (!(cond)) { g_failed++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

// ---- an in-memory "SD card" with fault injection -----------------------------------------------------------
typedef struct {
    uint8_t data[2][SAVE_FILE_BYTES + 8];
    size_t len[2];
    long fail_after;     // < 0: never. Otherwise write_slot stores only this many bytes, then reports failure.
    long torn_silently;  // < 0: never. Otherwise write_slot stores only this many bytes and reports SUCCESS (power loss).
} Card;

static int card_read(void* ctx, int slot, uint8_t* buf, size_t cap, size_t* len)
{
    Card* c = ctx;
    *len = c->len[slot] < cap ? c->len[slot] : cap;
    memcpy(buf, c->data[slot], *len);
    return 0;
}

static int card_write(void* ctx, int slot, const uint8_t* buf, size_t len)
{
    Card* c = ctx;
    size_t keep = len;
    int rc = 0;
    if (c->fail_after >= 0 && (size_t)c->fail_after < len) { keep = (size_t)c->fail_after; rc = -1; }
    if (c->torn_silently >= 0 && (size_t)c->torn_silently < len) { keep = (size_t)c->torn_silently; }
    memcpy(c->data[slot], buf, keep);
    c->len[slot] = keep;                 // create/truncate: the old contents are gone even if the write then fails
    return rc;
}

static void card_init(Card* c, SaveIo* io)
{
    memset(c, 0, sizeof *c);
    c->fail_after = -1;
    c->torn_silently = -1;
    io->ctx = c;
    io->read_slot = card_read;
    io->write_slot = card_write;
}

static SaveState make_state(int32_t x, int32_t y, int coins)
{
    SaveState s;
    memset(&s, 0, sizeof s);
    s.x_fp = x * 256;
    s.y_fp = y * 256;
    int n = 0;
    for (int cy = 0; cy < WORLD_H && n < coins; cy++) {
        for (int cx = 0; cx < WORLD_W && n < coins; cx++) {
            if (world_is_coin_cell(cx, cy)) { world_collect_coin(s.collected, cx, cy); n++; }
        }
    }
    s.score = world_count_collected(s.collected);
    return s;
}

static bool same(const SaveState* a, const SaveState* b) { return memcmp(a, b, sizeof *a) == 0; }

static void test_basics(void)
{
    CHECK(save_crc32("123456789", 9) == 0xCBF43926u);             // standard CRC-32 check value
    CHECK(WORLD_COIN_BYTES == 110 && SAVE_FILE_BYTES == 138);

    int coin_cells = 0;
    for (int y = 0; y < WORLD_H; y++) { for (int x = 0; x < WORLD_W; x++) { coin_cells += world_is_coin_cell(x, y); } }
    CHECK(coin_cells == 15);

    SaveState s = make_state(70, 72, 3), out;
    uint8_t buf[SAVE_FILE_BYTES];
    uint32_t gen = 0;
    save_encode(buf, &s, 7);
    CHECK(save_decode(buf, sizeof buf, &out, &gen) == SAVE_OK && gen == 7 && same(&s, &out));
    CHECK(memcmp(buf, "CPSV", 4) == 0 && buf[4] == 1 && buf[5] == 0 && buf[6] == 122 && buf[7] == 0);   // byte order is fixed
}

// The bug this example had: coins collected before saving came back after loading.
static void test_collected_coins_stay_collected(void)
{
    SaveIo io; Card c; card_init(&c, &io);
    SaveState before = make_state(70, 72, 0);
    CHECK(world_coin_present(before.collected, 3, 2));            // fresh game: the coin at column 3, row 2 is there
    world_collect_coin(before.collected, 3, 2);
    before.score = world_count_collected(before.collected);
    CHECK(save_store(&io, &before));

    SaveState after; uint32_t gen; int slot;
    CHECK(save_load(&io, &after, &gen, &slot));
    CHECK(after.score == 1 && !world_coin_present(after.collected, 3, 2));
    CHECK(world_coin_present(after.collected, 22, 2));            // an uncollected coin is still there
}

static void test_single_bit_flips_are_rejected(void)
{
    SaveState s = make_state(100, 100, 5), out;
    uint8_t buf[SAVE_FILE_BYTES];
    uint32_t gen;
    save_encode(buf, &s, 1);
    int accepted = 0;
    for (int bit = 0; bit < SAVE_FILE_BYTES * 8; bit++) {
        buf[bit / 8] ^= (uint8_t)(1u << (bit % 8));
        if (save_decode(buf, sizeof buf, &out, &gen) == SAVE_OK) { accepted++; }
        buf[bit / 8] ^= (uint8_t)(1u << (bit % 8));
    }
    CHECK(accepted == 0);
    CHECK(save_decode(buf, sizeof buf - 1, &out, &gen) == SAVE_ERR_SIZE);      // truncated
    uint8_t big[SAVE_FILE_BYTES + 1];
    memcpy(big, buf, sizeof buf); big[SAVE_FILE_BYTES] = 0;
    CHECK(save_decode(big, sizeof big, &out, &gen) == SAVE_ERR_SIZE);          // oversized
}

static void reseal(uint8_t buf[SAVE_FILE_BYTES])      // fix the CRC after editing a field, to reach the semantic checks
{
    uint32_t c = save_crc32(buf, SAVE_FILE_BYTES - 4);
    for (int i = 0; i < 4; i++) { buf[SAVE_FILE_BYTES - 4 + i] = (uint8_t)(c >> (8 * i)); }
}

static void test_semantic_checks(void)
{
    SaveState s = make_state(100, 100, 2), out;
    uint8_t buf[SAVE_FILE_BYTES];
    uint32_t gen;

    save_encode(buf, &s, 1); buf[12 + 3] = 0x80; reseal(buf);                 // x_fp negative
    CHECK(save_decode(buf, sizeof buf, &out, &gen) == SAVE_ERR_RANGE);
    save_encode(buf, &s, 1); buf[20] ^= 1; reseal(buf);                       // score no longer matches the bits
    CHECK(save_decode(buf, sizeof buf, &out, &gen) == SAVE_ERR_RANGE);
    save_encode(buf, &s, 1); buf[24 + 5] |= 0xFF; reseal(buf);                // bits set on cells that hold no coin
    CHECK(save_decode(buf, sizeof buf, &out, &gen) == SAVE_ERR_RANGE);
    save_encode(buf, &s, 1); buf[4] = 2; reseal(buf);                         // future version
    CHECK(save_decode(buf, sizeof buf, &out, &gen) == SAVE_ERR_VERSION);
    save_encode(buf, &s, 1); buf[0] = 'X'; reseal(buf);
    CHECK(save_decode(buf, sizeof buf, &out, &gen) == SAVE_ERR_MAGIC);
}

static void test_generations_alternate(void)
{
    SaveIo io; Card c; card_init(&c, &io);
    SaveState out; uint32_t gen; int slot;
    CHECK(!save_load(&io, &out, &gen, &slot) && slot == -1);                  // nothing saved yet
    for (int i = 1; i <= 5; i++) {
        SaveState s = make_state(40 + i, 40, i);
        CHECK(save_store(&io, &s));
        CHECK(save_load(&io, &out, &gen, &slot) && gen == (uint32_t)i && slot == (i - 1) % 2 && same(&s, &out));
    }
    c.data[0][10] ^= 0xFF; c.data[1][10] ^= 0xFF;                             // both slots damaged
    CHECK(!save_load(&io, &out, &gen, &slot));
}

// Interrupt the second save at every byte count. The loader must return the first or the second state, never anything else.
static void test_interrupted_second_save(void)
{
    const SaveState first = make_state(40, 40, 2), second = make_state(80, 90, 4);
    for (long keep = 0; keep <= SAVE_FILE_BYTES; keep++) {
        for (int mode = 0; mode < 2; mode++) {                                // 0: error reported, 1: silent torn write
            SaveIo io; Card c; card_init(&c, &io);
            CHECK(save_store(&io, &first));
            if (mode == 0) { c.fail_after = keep; } else { c.torn_silently = keep; }
            const bool reported_ok = save_store(&io, &second);
            SaveState got; uint32_t gen; int slot;
            CHECK(save_load(&io, &got, &gen, &slot));
            if (keep < SAVE_FILE_BYTES) {
                CHECK(same(&got, &first) && gen == 1);                        // incomplete: the old save survives
                if (mode == 0) { CHECK(!reported_ok); }
            } else {
                CHECK(same(&got, &second) && gen == 2);
            }
        }
    }
}

// The failed save must not poison the next one: after a failure, a retry still ends with a valid newest save.
static void test_retry_after_failure(void)
{
    SaveIo io; Card c; card_init(&c, &io);
    const SaveState a = make_state(40, 40, 1), b = make_state(50, 50, 2), d = make_state(60, 60, 3);
    CHECK(save_store(&io, &a));
    c.fail_after = 20; CHECK(!save_store(&io, &b));
    c.fail_after = -1; CHECK(save_store(&io, &d));
    SaveState got; uint32_t gen; int slot;
    CHECK(save_load(&io, &got, &gen, &slot) && same(&got, &d));
}

// ---- PCM -----------------------------------------------------------------------------------------------------
static void test_pcm_fill(void)
{
    enum { WANT = 8, GUARD = 4 };
    int16_t src[2 * WANT], buf[2 * (WANT + GUARD)];
    for (int i = 0; i < 2 * WANT; i++) { src[i] = (int16_t)(100 + i); }

    for (size_t have = 0; have <= WANT + 2; have++) {
        memset(buf, 0x7F, sizeof buf);                                         // poison, including the guard frames
        pcm_fill(buf, WANT, src, have <= WANT ? have : WANT);                  // `have` > WANT is covered just below
        const size_t n = have < WANT ? have : WANT;
        for (size_t i = 0; i < WANT; i++) {
            const int16_t el = n ? (i < n ? src[2 * i] : src[2 * (n - 1)]) : 0;
            const int16_t er = n ? (i < n ? src[2 * i + 1] : src[2 * (n - 1) + 1]) : 0;
            CHECK(buf[2 * i] == el && buf[2 * i + 1] == er);
        }
        for (int i = 2 * WANT; i < 2 * (WANT + GUARD); i++) { CHECK(buf[i] == 0x7F7F); }   // nothing written past `want`
    }
    memset(buf, 0x7F, sizeof buf);
    pcm_fill(buf, 4, src, 100);                                                // more source than requested: copy only `want`
    CHECK(buf[6] == src[6] && buf[7] == src[7] && buf[8] == 0x7F7F);
    pcm_fill(buf + 1, 3, src, 2);                                              // odd (2-byte) alignment is fine for int16_t
    CHECK(buf[1 + 4] == src[2] && buf[1 + 5] == src[3]);
}

int main(void)
{
    test_basics();
    test_collected_coins_stay_collected();
    test_single_bit_flips_are_rejected();
    test_semantic_checks();
    test_generations_alternate();
    test_interrupted_second_save();
    test_retry_after_failure();
    test_pcm_fill();
    printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
