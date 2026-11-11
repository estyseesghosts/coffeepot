// A complete, tiny Switch homebrew game used by nx/engineering/first-game.md.
// Moves a square over a tile map with the left stick or D-pad, collects coins,
// beeps on pickup, saves progress, logs frame times over nxlink, and writes a
// crash dump if the program faults.
//
// Built with devkitA64 r30 / libnx 4.12.0 under -Wall (see source-versions.md).
// Not run on hardware.
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

// ------------------------------------------------------------------ constants
#define FB_W 1280
#define FB_H 720
#define TILE 32
#define MAP_W 40
#define MAP_H 22
#define PLAYER 24
#define SAVE_DIR  "sdmc:/switch/firstgame"
#define SAVE_FILE SAVE_DIR "/save.dat"
#define SAVE_TMP  SAVE_DIR "/save.tmp"
#define SAVE_BAK  SAVE_DIR "/save.bak"

// '#' wall, '.' floor, 'o' coin
static const char* MAP[MAP_H] = {
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

typedef struct { float x, y; uint32_t coins; } Save;

// ------------------------------------------------------------------ crash dump
// libnx calls this on a CPU exception if an exception stack is provided.
alignas(16) u8 __nx_exception_stack[0x1000];
u64 __nx_exception_stack_size = sizeof(__nx_exception_stack);

void __libnx_exception_handler(ThreadExceptionDump* ctx)
{
    FILE* f = fopen(SAVE_DIR "/crash.txt", "w");
    if (!f) return;
    fprintf(f, "error_desc: 0x%x\n", ctx->error_desc);
    fprintf(f, "pc: 0x%lx\nlr: 0x%lx\nsp: 0x%lx\nfar: 0x%lx\n",
            ctx->pc.x, ctx->lr.x, ctx->sp.x, ctx->far.x);
    fclose(f);
}

// ------------------------------------------------------------------ applet state
static volatile bool g_docked;
static volatile bool g_focused = true;

static void onApplet(AppletHookType type, void* param)
{
    (void)param;
    if (type == AppletHookType_OnOperationMode)
        g_docked = (appletGetOperationMode() == AppletOperationMode_Console);
    else if (type == AppletHookType_OnFocusState)
        g_focused = (appletGetFocusState() == AppletFocusState_InFocus);
}

// ------------------------------------------------------------------ save / load
static bool loadSave(Save* out)
{
    FILE* f = fopen(SAVE_FILE, "rb");
    if (!f) f = fopen(SAVE_BAK, "rb");           // fall back to the previous generation
    if (!f) return false;
    bool ok = fread(out, sizeof *out, 1, f) == 1;
    fclose(f);
    return ok;
}

static bool writeSave(const Save* s)
{
    mkdir(SAVE_DIR, 0777);                         // harmless if it exists
    FILE* f = fopen(SAVE_TMP, "wb");
    if (!f) return false;
    if (fwrite(s, sizeof *s, 1, f) != 1) { fclose(f); return false; }
    fflush(f);
    fsync(fileno(f));
    fclose(f);
    remove(SAVE_BAK);
    rename(SAVE_FILE, SAVE_BAK);                   // fails harmlessly the first time
    return rename(SAVE_TMP, SAVE_FILE) == 0;
}

// ------------------------------------------------------------------ audio
#define RATE 48000
#define BEEP_FRAMES (RATE / 10)                   // 100 ms
static AudioOutBuffer g_beep;
static u8* g_beepData;
static bool g_audioOk;

static bool audioInit(void)
{
    u32 dataSize = BEEP_FRAMES * 2 * sizeof(s16);
    u32 bufSize = (dataSize + 0xfff) & ~0xfffu;    // must be 0x1000 aligned
    g_beepData = memalign(0x1000, bufSize);
    if (!g_beepData) return false;
    memset(g_beepData, 0, bufSize);
    s16* p = (s16*)g_beepData;
    for (int i = 0; i < BEEP_FRAMES; i++) {
        float env = 1.0f - (float)i / BEEP_FRAMES;
        s16 v = (s16)(0.25f * 0x7FFF * env * sinf(2.0f * (float)M_PI * 880.0f * i / RATE));
        p[2 * i] = v; p[2 * i + 1] = v;            // interleaved L, R
    }
    g_beep.next = NULL;
    g_beep.buffer = g_beepData;
    g_beep.buffer_size = bufSize;
    g_beep.data_size = dataSize;
    g_beep.data_offset = 0;
    if (R_FAILED(audoutInitialize())) return false;
    if (R_FAILED(audoutStartAudioOut())) { audoutExit(); return false; }
    return true;
}

static void playBeep(void)
{
    if (!g_audioOk) return;
    // Recycle the buffer if the system has finished with it; skip the beep otherwise.
    AudioOutBuffer* released = NULL;
    u32 count = 0;
    audoutGetReleasedAudioOutBuffer(&released, &count);
    static bool queued;
    if (queued && count == 0) return;              // still playing
    queued = true;
    audoutAppendAudioOutBuffer(&g_beep);
}

// ------------------------------------------------------------------ drawing
static void fillRect(u32* fb, u32 stride, int x, int y, int w, int h, u32 colour)
{
    for (int j = y; j < y + h && j < FB_H; j++) {
        if (j < 0) continue;
        u32* row = fb + (size_t)j * (stride / sizeof(u32));
        for (int i = x; i < x + w && i < FB_W; i++)
            if (i >= 0) row[i] = colour;
    }
}

static bool solidAt(int px, int py)
{
    int tx = px / TILE, ty = py / TILE;
    if (tx < 0 || ty < 0 || tx >= MAP_W || ty >= MAP_H) return true;
    return MAP[ty][tx] == '#';
}

static bool boxHitsWall(float x, float y)
{
    return solidAt((int)x, (int)y) || solidAt((int)(x + PLAYER - 1), (int)y) ||
           solidAt((int)x, (int)(y + PLAYER - 1)) || solidAt((int)(x + PLAYER - 1), (int)(y + PLAYER - 1));
}

// ------------------------------------------------------------------ main
int main(int argc, char* argv[])
{
    (void)argc; (void)argv;

    // Logging: after this, printf/fprintf(stderr) go to the PC when started with `nxlink -s`.
    socketInitializeDefault();
    nxlinkStdio();

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);

    static AppletHookCookie cookie;
    appletHook(&cookie, onApplet, NULL);
    g_docked = (appletGetOperationMode() == AppletOperationMode_Console);

    // 1280x720 logical framebuffer. Whether the system scales it for a 1080p dock output
    // was not tested here.
    NWindow* win = nwindowGetDefault();
    Framebuffer fb;
    framebufferCreate(&fb, win, FB_W, FB_H, PIXEL_FORMAT_RGBA_8888, 2);
    framebufferMakeLinear(&fb);

    g_audioOk = audioInit();
    printf("audio %s, %s\n", g_audioOk ? "ok" : "unavailable", g_docked ? "docked" : "handheld");

    // Coin state from the map, then progress from the save file.
    bool coin[MAP_H][MAP_W];
    Save save = { 2.0f * TILE, 2.0f * TILE + 4.0f, 0 };
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++) coin[y][x] = (MAP[y][x] == 'o');
    if (loadSave(&save)) printf("loaded save: %u coins\n", save.coins);

    float px = save.x, py = save.y;
    u32 coins = save.coins;

    u64 last = armGetSystemTick();
    double worstMs = 0.0, sumMs = 0.0;
    int frames = 0;

    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 down = padGetButtonsDown(&pad);
        u64 held = padGetButtons(&pad);
        if (down & HidNpadButton_Plus) break;

        u64 now = armGetSystemTick();
        double dt = (double)armTicksToNs(now - last) / 1e9;
        last = now;
        if (dt > 0.1) dt = 0.1;                    // clamp after a stall (sleep, Home menu)

        if (g_focused) {
            HidAnalogStickState st = padGetStickPos(&pad, 0);
            float ax = st.x / 32767.0f, ay = -st.y / 32767.0f;     // stick up is positive y
            if (held & HidNpadButton_Left)  ax = -1.0f;
            if (held & HidNpadButton_Right) ax =  1.0f;
            if (held & HidNpadButton_Up)    ay = -1.0f;
            if (held & HidNpadButton_Down)  ay =  1.0f;
            if (fabsf(ax) < 0.15f) ax = 0.0f;                      // dead zone
            if (fabsf(ay) < 0.15f) ay = 0.0f;

            const float speed = 220.0f;                            // pixels per second
            float nx = px + ax * speed * (float)dt;
            if (!boxHitsWall(nx, py)) px = nx;                     // axis-separated collision
            float ny = py + ay * speed * (float)dt;
            if (!boxHitsWall(px, ny)) py = ny;

            int cx = (int)(px + PLAYER / 2) / TILE, cy = (int)(py + PLAYER / 2) / TILE;
            if (cx >= 0 && cy >= 0 && cx < MAP_W && cy < MAP_H && coin[cy][cx]) {
                coin[cy][cx] = false;
                coins++;
                playBeep();
            }
            if (down & HidNpadButton_A) {                          // manual save
                Save s = { px, py, coins };
                printf("save %s\n", writeSave(&s) ? "ok" : "FAILED");
            }
        }

        u32 stride;
        u32* buf = (u32*)framebufferBegin(&fb, &stride);
        fillRect(buf, stride, 0, 0, FB_W, FB_H, RGBA8_MAXALPHA(20, 24, 40));
        for (int y = 0; y < MAP_H; y++)
            for (int x = 0; x < MAP_W; x++) {
                if (MAP[y][x] == '#')
                    fillRect(buf, stride, x * TILE, y * TILE, TILE, TILE, RGBA8_MAXALPHA(90, 90, 120));
                else
                    fillRect(buf, stride, x * TILE, y * TILE, TILE - 1, TILE - 1, RGBA8_MAXALPHA(40, 60, 50));
                if (coin[y][x])
                    fillRect(buf, stride, x * TILE + 10, y * TILE + 10, 12, 12, RGBA8_MAXALPHA(240, 200, 40));
            }
        fillRect(buf, stride, (int)px, (int)py, PLAYER, PLAYER, RGBA8_MAXALPHA(230, 90, 90));
        for (u32 i = 0; i < coins && i < 100; i++)                 // coin counter along the bottom
            fillRect(buf, stride, 8 + (int)i * 10, FB_H - 14, 8, 8, RGBA8_MAXALPHA(240, 200, 40));
        framebufferEnd(&fb);                                       // presents; paced by the swap

        // Frame-time statistics, printed every 120 frames when nxlink is attached.
        double ms = dt * 1000.0;
        sumMs += ms; if (ms > worstMs) worstMs = ms;
        if (++frames == 120) {
            printf("frame ms: avg %.2f worst %.2f\n", sumMs / frames, worstMs);
            frames = 0; sumMs = 0.0; worstMs = 0.0;
        }
    }

    Save s = { px, py, coins };
    writeSave(&s);

    if (g_audioOk) { audoutStopAudioOut(); audoutExit(); }
    free(g_beepData);
    framebufferClose(&fb);
    socketExit();
    return 0;
}
