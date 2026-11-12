// A complete, tiny Switch homebrew game used by nx/engineering/first-game.md.
// Moves a square over a tile map with the left stick or D-pad, collects coins, beeps on pickup, saves progress to
// two checksummed slots, logs per-phase frame times over nxlink, and writes a best-effort crash dump on a fault.
//
// Build with `make`. `make CFLAGS_EXTRA=-DFG_SELFTEST` builds the scripted acceptance run (see first-game.md).
// Verification status is on nx/engineering/source-versions.md.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <malloc.h>
#include <math.h>
#include <unistd.h>
#include <sys/stat.h>
#include <switch.h>
#include "save_format.h"
#include "world.h"

// ------------------------------------------------------------------ constants
#define FB_W 1280
#define FB_H 720
#define TILE 32
#define PLAYER 24
#define SAVE_DIR "sdmc:/switch/firstgame"
#define SAVE_SLOT_PATH(n) (SAVE_DIR "/save" #n ".dat")

// ------------------------------------------------------------------ crash dump (best effort)
// libnx calls this on a CPU exception if an exception stack is provided. It runs after an arbitrary fault, so
// the filesystem, the C library and any lock may be in a bad state: treat the file as a bonus. The reliable
// record is Atmosphere's crash report (see debugging.md).
alignas(16) u8 __nx_exception_stack[0x1000];
u64 __nx_exception_stack_size = sizeof(__nx_exception_stack);

void __libnx_exception_handler(ThreadExceptionDump* ctx)
{
    FILE* f = fopen(SAVE_DIR "/crash.txt", "w");
    if (!f) { return; }
    fprintf(f, "error_desc: 0x%x\npc: 0x%lx\nlr: 0x%lx\nsp: 0x%lx\nfar: 0x%lx\n",
            ctx->error_desc, ctx->pc.x, ctx->lr.x, ctx->sp.x, ctx->far.x);
    fclose(f);
}

// ------------------------------------------------------------------ applet state
static volatile bool g_docked;
static volatile bool g_focused = true;
static volatile bool g_focus_changed;

static void onApplet(AppletHookType type, void* param)
{
    (void)param;
    if (type == AppletHookType_OnOperationMode) {
        g_docked = (appletGetOperationMode() == AppletOperationMode_Console);
    } else if (type == AppletHookType_OnFocusState) {
        g_focused = (appletGetFocusState() == AppletFocusState_InFocus);
        g_focus_changed = true;
    }
}

// ------------------------------------------------------------------ save slots on the SD card
static int slotRead(void* ctx, int slot, uint8_t* buf, size_t cap, size_t* len)
{
    (void)ctx;
    FILE* f = fopen(slot == 0 ? SAVE_SLOT_PATH(0) : SAVE_SLOT_PATH(1), "rb");
    if (!f) { *len = 0; return 0; }                         // a missing slot is normal, not an error
    *len = fread(buf, 1, cap, f);
    const int bad = ferror(f);
    fclose(f);
    return bad ? -1 : 0;
}

static int slotWrite(void* ctx, int slot, const uint8_t* buf, size_t len)
{
    (void)ctx;
    mkdir(SAVE_DIR, 0777);                                  // harmless if it exists
    FILE* f = fopen(slot == 0 ? SAVE_SLOT_PATH(0) : SAVE_SLOT_PATH(1), "wb");
    if (!f) { return -1; }
    int rc = 0;
    if (fwrite(buf, 1, len, f) != len) { rc = -1; }
    if (fflush(f) != 0) { rc = -1; }
    if (fsync(fileno(f)) != 0) { rc = -1; }                 // reaching the card is not verified, see first-game.md
    if (fclose(f) != 0) { rc = -1; }
    return rc;
}

static const SaveIo g_saveIo = { NULL, slotRead, slotWrite };

// ------------------------------------------------------------------ audio
#define RATE 48000
#define BEEP_FRAMES (RATE / 10)                             // 100 ms
static AudioOutBuffer g_beep;
static u8* g_beepData;
static bool g_audioOk;
static bool g_beepInFlight;
static u32 g_beepsDropped;

static bool audioInit(void)
{
    const u32 dataSize = BEEP_FRAMES * 2 * sizeof(s16);
    const u32 bufSize = (dataSize + 0xfff) & ~0xfffu;       // must be 0x1000 aligned
    g_beepData = memalign(0x1000, bufSize);
    if (!g_beepData) { return false; }
    memset(g_beepData, 0, bufSize);
    s16* p = (s16*)g_beepData;
    for (int i = 0; i < BEEP_FRAMES; i++) {
        const float env = 1.0f - (float)i / BEEP_FRAMES;
        const s16 v = (s16)(0.25f * 0x7FFF * env * sinf(2.0f * (float)M_PI * 880.0f * i / RATE));
        p[2 * i] = v;
        p[2 * i + 1] = v;                                   // interleaved L, R
    }
    g_beep.next = NULL;
    g_beep.buffer = g_beepData;
    g_beep.buffer_size = bufSize;
    g_beep.data_size = dataSize;
    g_beep.data_offset = 0;
    if (R_FAILED(audoutInitialize())) { free(g_beepData); g_beepData = NULL; return false; }
    if (R_FAILED(audoutStartAudioOut())) { audoutExit(); free(g_beepData); g_beepData = NULL; return false; }
    return true;
}

// Call once per frame: the system hands the buffer back when it has finished playing it.
static void audioPump(void)
{
    if (!g_audioOk || !g_beepInFlight) { return; }
    AudioOutBuffer* released = NULL;
    u32 count = 0;
    if (R_SUCCEEDED(audoutGetReleasedAudioOutBuffer(&released, &count)) && count > 0) { g_beepInFlight = false; }
}

static void playBeep(void)
{
    if (!g_audioOk) { return; }
    audioPump();
    if (g_beepInFlight) { g_beepsDropped++; return; }       // the buffer is still owned by the audio service
    if (R_SUCCEEDED(audoutAppendAudioOutBuffer(&g_beep))) { g_beepInFlight = true; }
}

// ------------------------------------------------------------------ drawing
static void fillRect(u32* fb, u32 stride, int x, int y, int w, int h, u32 colour)
{
    for (int j = y; j < y + h && j < FB_H; j++) {
        if (j < 0) { continue; }
        u32* row = fb + (size_t)j * (stride / sizeof(u32));
        for (int i = x; i < x + w && i < FB_W; i++) {
            if (i >= 0) { row[i] = colour; }
        }
    }
}

static bool solidAt(int px, int py)
{
    const int tx = px / TILE, ty = py / TILE;
    if (px < 0 || py < 0 || tx >= WORLD_W || ty >= WORLD_H) { return true; }
    return WORLD_MAP[ty][tx] == '#';
}

static bool boxHitsWall(float x, float y)
{
    return solidAt((int)x, (int)y) || solidAt((int)(x + PLAYER - 1), (int)y) ||
           solidAt((int)x, (int)(y + PLAYER - 1)) || solidAt((int)(x + PLAYER - 1), (int)(y + PLAYER - 1));
}

// ------------------------------------------------------------------ timing
// Each phase is timed around the call that can block, so a long frame can be traced to one of them:
//   begin    framebufferBegin: dequeues a buffer, which waits if the display still holds all of them
//   draw     input, update and pixel writes on the CPU
//   present  framebufferEnd: in linear mode this also copies and converts the image, then queues it
//   loop     time between the starts of two loop iterations (everything above plus appletMainLoop and input)
typedef struct { double sum, worst; } Stat;
static void statAdd(Stat* s, double ms) { s->sum += ms; if (ms > s->worst) { s->worst = ms; } }
static double ticksMs(u64 ticks) { return (double)armTicksToNs(ticks) / 1e6; }

// ------------------------------------------------------------------ game state
typedef struct {
    float x, y;
    SaveState save;              // collected bits and score live here, so saving never rebuilds them
} Game;

static void gameFromSave(Game* g, const SaveState* s)
{
    g->save = *s;
    g->x = (float)s->x_fp / 256.0f;
    g->y = (float)s->y_fp / 256.0f;
}

static void gameNew(Game* g)
{
    memset(g, 0, sizeof *g);
    g->x = 2.0f * TILE;
    g->y = 2.0f * TILE + 4.0f;
}

static bool gameSave(Game* g)
{
    g->save.x_fp = (int32_t)(g->x * 256.0f);
    g->save.y_fp = (int32_t)(g->y * 256.0f);
    g->save.score = world_count_collected(g->save.collected);
    return save_store(&g_saveIo, &g->save);
}

#ifdef FG_SELFTEST
// Scripted acceptance run: holds Right for 100 frames (the square reaches the coin at column 3, row 2), saves, reloads
// through the real SD path and checks that the coin is still gone, then saves again and writes selftest.txt.
static FILE* g_report;
static void reportLine(const char* name, bool ok) { if (g_report) { fprintf(g_report, "%s,%s\n", name, ok ? "pass" : "FAIL"); } }
#endif

// ------------------------------------------------------------------ main
int main(int argc, char* argv[])
{
    (void)argc; (void)argv;
    int rc = 0;

    // Logging is optional: with no nxlink host, nxlinkStdio() returns a negative value and the game runs without it.
    int nxlinkFd = -1;
    const bool sockets = R_SUCCEEDED(socketInitializeDefault());
    if (sockets) { nxlinkFd = nxlinkStdio(); }

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);

    static AppletHookCookie cookie;
    appletHook(&cookie, onApplet, NULL);
    g_docked = (appletGetOperationMode() == AppletOperationMode_Console);

    // 1280x720 logical framebuffer. Whether the system scales it for a 1080p dock output was not tested.
    NWindow* win = nwindowGetDefault();
    Framebuffer fb;
    Result fbRc = framebufferCreate(&fb, win, FB_W, FB_H, PIXEL_FORMAT_RGBA_8888, 2);
    if (R_FAILED(fbRc)) {
        printf("framebufferCreate failed: 0x%x\n", fbRc);
        rc = 1;
        goto cleanup_no_fb;
    }
    fbRc = framebufferMakeLinear(&fb);
    if (R_FAILED(fbRc)) {
        printf("framebufferMakeLinear failed: 0x%x\n", fbRc);
        rc = 1;
        goto cleanup;
    }

    g_audioOk = audioInit();
    printf("audio %s, %s, nxlink %s\n", g_audioOk ? "ok" : "unavailable", g_docked ? "docked" : "handheld",
           nxlinkFd >= 0 ? "connected" : "off");

    Game game;
    gameNew(&game);
#ifdef FG_SELFTEST
    mkdir(SAVE_DIR, 0777);
    remove(SAVE_SLOT_PATH(0));
    remove(SAVE_SLOT_PATH(1));
    g_report = fopen(SAVE_DIR "/selftest.txt", "w");
#else
    {
        SaveState loaded;
        uint32_t generation;
        int slot;
        if (save_load(&g_saveIo, &loaded, &generation, &slot)) {
            gameFromSave(&game, &loaded);
            printf("loaded generation %u from slot %d: %u coins\n", generation, slot, loaded.score);
        }
    }
#endif

    u64 last = armGetSystemTick();
    Stat sInterval = {0}, sBegin = {0}, sDraw = {0}, sPresent = {0};
    int frames = 0;
#ifdef FG_SELFTEST
    int frameNo = 0;
#endif

    while (appletMainLoop()) {
        const u64 t0 = armGetSystemTick();
        padUpdate(&pad);
        const u64 down = padGetButtonsDown(&pad);
        const u64 held = padGetButtons(&pad);
        if (down & HidNpadButton_Plus) { break; }

        if (g_focus_changed) { g_focus_changed = false; last = t0; }    // do not count time spent unfocused
        const double intervalMs = ticksMs(t0 - last);       // time since the previous iteration started
        double dt = intervalMs / 1000.0;
        last = t0;
        if (dt > 0.1) { dt = 0.1; }                         // clamp the simulation step after a stall (sleep, Home menu)
#ifdef FG_SELFTEST
        dt = 1.0 / 60.0;                                    // deterministic steps
#endif

        const u64 t1 = armGetSystemTick();
        u32 stride;
        u32* buf = (u32*)framebufferBegin(&fb, &stride);
        const u64 t2 = armGetSystemTick();

        if (g_focused) {
            const HidAnalogStickState st = padGetStickPos(&pad, 0);
            float ax = st.x / 32767.0f, ay = -st.y / 32767.0f;          // stick up is positive y
            if (held & HidNpadButton_Left)  { ax = -1.0f; }
            if (held & HidNpadButton_Right) { ax =  1.0f; }
            if (held & HidNpadButton_Up)    { ay = -1.0f; }
            if (held & HidNpadButton_Down)  { ay =  1.0f; }
#ifdef FG_SELFTEST
            ax = (frameNo < 100) ? 1.0f : 0.0f;
            ay = 0.0f;
#endif
            if (fabsf(ax) < 0.15f) { ax = 0.0f; }                       // dead zone
            if (fabsf(ay) < 0.15f) { ay = 0.0f; }

            const float speed = 220.0f;                                 // pixels per second
            const float nx = game.x + ax * speed * (float)dt;
            if (!boxHitsWall(nx, game.y)) { game.x = nx; }              // axis-separated collision
            const float ny = game.y + ay * speed * (float)dt;
            if (!boxHitsWall(game.x, ny)) { game.y = ny; }

            const int cx = (int)(game.x + PLAYER / 2) / TILE, cy = (int)(game.y + PLAYER / 2) / TILE;
            if (world_coin_present(game.save.collected, cx, cy)) {
                world_collect_coin(game.save.collected, cx, cy);
                game.save.score = world_count_collected(game.save.collected);
                playBeep();
            }
            if (down & HidNpadButton_A) { printf("save %s\n", gameSave(&game) ? "ok" : "FAILED"); }
        }
        audioPump();

        fillRect(buf, stride, 0, 0, FB_W, FB_H, RGBA8_MAXALPHA(20, 24, 40));
        for (int y = 0; y < WORLD_H; y++) {
            for (int x = 0; x < WORLD_W; x++) {
                if (WORLD_MAP[y][x] == '#') {
                    fillRect(buf, stride, x * TILE, y * TILE, TILE, TILE, RGBA8_MAXALPHA(90, 90, 120));
                } else {
                    fillRect(buf, stride, x * TILE, y * TILE, TILE - 1, TILE - 1, RGBA8_MAXALPHA(40, 60, 50));
                }
                if (world_coin_present(game.save.collected, x, y)) {
                    fillRect(buf, stride, x * TILE + 10, y * TILE + 10, 12, 12, RGBA8_MAXALPHA(240, 200, 40));
                }
            }
        }
        fillRect(buf, stride, (int)game.x, (int)game.y, PLAYER, PLAYER, RGBA8_MAXALPHA(230, 90, 90));
        for (u32 i = 0; i < game.save.score && i < 100; i++) {         // coin counter along the bottom
            fillRect(buf, stride, 8 + (int)i * 10, FB_H - 14, 8, 8, RGBA8_MAXALPHA(240, 200, 40));
        }
        const u64 t3 = armGetSystemTick();

#ifdef FG_SELFTEST
        if (frameNo == 150 && g_report) {                               // picture check: dump the linear buffer
            FILE* shot = fopen(SAVE_DIR "/frame150.rgba", "wb");
            if (shot) {
                for (int y = 0; y < FB_H; y++) { fwrite((u8*)buf + (size_t)y * stride, 4, FB_W, shot); }
                fclose(shot);
            }
        }
#endif
        framebufferEnd(&fb);
        const u64 t4 = armGetSystemTick();

        statAdd(&sInterval, intervalMs);
        statAdd(&sBegin, ticksMs(t2 - t1));
        statAdd(&sDraw, ticksMs(t3 - t2));
        statAdd(&sPresent, ticksMs(t4 - t3));
        if (++frames == 120) {
            printf("ms avg/worst  loop %.2f/%.2f  begin %.2f/%.2f  draw %.2f/%.2f  present %.2f/%.2f  beeps dropped %u\n",
                   sInterval.sum / frames, sInterval.worst, sBegin.sum / frames, sBegin.worst,
                   sDraw.sum / frames, sDraw.worst, sPresent.sum / frames, sPresent.worst, g_beepsDropped);
            frames = 0;
            sInterval = sBegin = sDraw = sPresent = (Stat){0};
        }

#ifdef FG_SELFTEST
        if (frameNo == 120) {
            reportLine("coin_collected", game.save.score == 1 && !world_coin_present(game.save.collected, 3, 2));
            reportLine("save_ok", gameSave(&game));
            SaveState back;
            uint32_t gen = 0;
            int slot = -1;
            const bool loaded = save_load(&g_saveIo, &back, &gen, &slot);
            reportLine("load_ok", loaded && gen == 1 && slot == 0);
            reportLine("coin_stays_collected", loaded && back.score == 1 && !world_coin_present(back.collected, 3, 2) &&
                                                  world_coin_present(back.collected, 22, 2));
            Game restarted;                                             // what a fresh start does with the loaded save
            gameNew(&restarted);
            if (loaded) { gameFromSave(&restarted, &back); }
            reportLine("restart_position", fabsf(restarted.x - game.x) < 0.01f && fabsf(restarted.y - game.y) < 0.01f);
            reportLine("second_save_other_slot", gameSave(&game) && save_load(&g_saveIo, &back, &gen, &slot) &&
                                                     gen == 2 && slot == 1);
            reportLine("audio_ok", g_audioOk);
        }
        if (frameNo == 160) { break; }
#endif
#ifdef FG_SELFTEST
        frameNo++;
#endif
    }

    if (g_focused) { gameSave(&game); }
#ifdef FG_SELFTEST
    if (g_report) { fprintf(g_report, "note,done\n"); fclose(g_report); }
#endif

    if (g_audioOk) { audoutStopAudioOut(); audoutExit(); }
    free(g_beepData);
cleanup:
    framebufferClose(&fb);
cleanup_no_fb:
    appletUnhook(&cookie);
    if (nxlinkFd >= 0) { close(nxlinkFd); }
    if (sockets) { socketExit(); }
    return rc;
}
