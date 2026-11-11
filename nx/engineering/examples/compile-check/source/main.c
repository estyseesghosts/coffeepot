// Compile-check for every code fragment published on the nx/ engineering pages.
// Built with devkitA64 r30 / libnx 4.12.0 / deko3d 0.5.0 under -Wall (see nx/engineering/source-versions.md).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <switch.h>
#include <deko3d.h>

// ---------------------------------------------------------------- input-loop
static void inputLoop(void)
{
    padConfigureInput(1, HidNpadStyleSet_NpadStandard); // one player, standard pads
    PadState pad;
    padInitializeDefault(&pad);                         // handheld + first connected pad

    while (appletMainLoop())                            // false once the system asks the app to exit
    {
        padUpdate(&pad);                                // once per frame
        u64 down = padGetButtonsDown(&pad);             // newly pressed this frame
        u64 held = padGetButtons(&pad);                 // currently held
        if (down & HidNpadButton_Plus)
            break;                                      // return to hbmenu
        if (held & HidNpadButton_A) { /* ... */ }
    }
}

// ---------------------------------------------------------------- nxlink
static void nxlinkSetup(void)
{
    consoleInit(NULL);
    socketInitializeDefault();   // sockets first,
    nxlinkStdio();               // then redirect stdout and stderr to the nxlink host
    printf("this line appears in the PC terminal when started with nxlink -s\n");
    // ... main loop ...
    socketExit();
    consoleExit(NULL);
}

// ---------------------------------------------------------------- static-upload
typedef struct { float pos[3]; float uv[2]; } Vertex;

static DkMemBlock createStaticBlock(DkDevice device, const Vertex* verts, uint32_t bytes)
{
    DkMemBlockMaker maker;
    // size must be a multiple of DK_MEMBLOCK_ALIGNMENT (0x1000)
    uint32_t size = (bytes + DK_MEMBLOCK_ALIGNMENT - 1) & ~(DK_MEMBLOCK_ALIGNMENT - 1);
    dkMemBlockMakerDefaults(&maker, device, size);      // CpuUncached | GpuCached
    DkMemBlock block = dkMemBlockCreate(&maker);
    memcpy(dkMemBlockGetCpuAddr(block), verts, bytes);  // written once, at load time
    return block;
}

static void bindCameraUniforms(DkCmdBuf cmdbuf, DkMemBlock uniformBlock, uint32_t offset)
{
    // DK_UNIFORM_BUF_ALIGNMENT (0x100) applies to the buffer address
    dkCmdBufBindUniformBuffer(cmdbuf, DkStage_Vertex, 0,
        dkMemBlockGetGpuAddr(uniformBlock) + offset, 0x100);
}

static void updateSmallUniform(DkCmdBuf cmdbuf, DkMemBlock uniformBlock, const float mvp[16])
{
    // inline update recorded into the command stream (no CPU write to GPU-visible memory)
    dkCmdBufPushConstants(cmdbuf, dkMemBlockGetGpuAddr(uniformBlock), 0x100, 0, 16 * sizeof(float), mvp);
}

// ---------------------------------------------------------------- frame-ring
#define SLOTS 2

typedef struct {
    DkCmdBuf  cmdbuf;     // memory added with dkCmdBufAddMemory, one region per slot
    DkFence   done;       // signalled by the GPU when this slot's work has finished
} FrameSlot;

static FrameSlot g_slots[SLOTS];   // static storage: fences start zeroed, so the first wait returns at once
static unsigned  g_frameNo;

static void recordScene(DkCmdBuf cmdbuf, unsigned frame)
{
    (void)frame;
    dkCmdBufDraw(cmdbuf, DkPrimitive_Triangles, 3, 1, 0, 0);
}

static void frame(DkQueue queue, DkSwapchain swapchain, const DkCmdList bindTarget[])
{
    FrameSlot* s = &g_slots[g_frameNo++ % SLOTS];

    // CPU-side wait: block until the GPU has finished the frame that last used this slot.
    // (dkQueueWaitFence would only make the GPU wait; it never blocks the CPU.)
    dkFenceWait(&s->done, -1);                // -1 = no timeout (same default as dk::Fence::wait)

    dkCmdBufClear(s->cmdbuf);                 // safe now: the GPU is done with the old list
    recordScene(s->cmdbuf, g_frameNo);
    DkCmdList scene = dkCmdBufFinishList(s->cmdbuf);

    int image = dkQueueAcquireImage(queue, swapchain);
    dkQueueSubmitCommands(queue, bindTarget[image]);
    dkQueueSubmitCommands(queue, scene);
    dkQueueSignalFence(queue, &s->done, false);   // GPU signals when it reaches this point
    dkQueuePresentImage(queue, swapchain, image); // also flushes the queue
}

// ---------------------------------------------------------------- teardown
static void teardown(DkQueue queue)
{
    dkQueueWaitIdle(queue);   // once, before destroying anything the GPU might still read
    // dkQueueDestroy(...), dkCmdBufDestroy(...), dkMemBlockDestroy(...), dkSwapchainDestroy(...), dkDeviceDestroy(...)
}

// ---------------------------------------------------------------- capture-replay
static void captureAndReplay(DkCmdBuf cmdbuf)
{
    static uint32_t words[256];                        // generous: overrunning the buffer is fatal
    dkCmdBufBeginCaptureCmds(cmdbuf, words, 256);
    // ... bind shaders, rasterizer/colour state, vertex state ...
    uint32_t n = dkCmdBufEndCaptureCmds(cmdbuf);
    // later, for every draw that needs the same state:
    dkCmdBufReplayCmds(cmdbuf, words, n);
}

// ---------------------------------------------------------------- image-layout
static uint64_t imageBytes(DkDevice device, uint32_t w, uint32_t h, uint32_t* align)
{
    DkImageLayoutMaker maker;
    dkImageLayoutMakerDefaults(&maker, device);
    // flags left at the default: block-linear layout (add DkImageFlags_PitchLinear only for linear images)
    maker.format = DkImageFormat_RGBA8_Unorm;
    maker.dimensions[0] = w;
    maker.dimensions[1] = h;
    DkImageLayout layout;
    dkImageLayoutInitialize(&layout, &maker);
    *align = dkImageLayoutGetAlignment(&layout);
    return dkImageLayoutGetSize(&layout);
}

// ---------------------------------------------------------------- barriers
static void passToPass(DkCmdBuf cmdbuf)
{
    dkCmdBufBarrier(cmdbuf, DkBarrier_Fragments, 0);                 // finish pass A fragments before pass B
    dkCmdBufBarrier(cmdbuf, DkBarrier_Tiles, DkInvalidateFlags_Image); // tile-local hand-over (Example08)
    dkCmdBufTiledCacheOp(cmdbuf, DkTiledCacheOp_Enable);
    dkCmdBufTiledCacheOp(cmdbuf, DkTiledCacheOp_Flush);
}

// ---------------------------------------------------------------- fixed-step
static void tick(double frameSeconds, double* acc, double* alpha)
{
    const double DT = 1.0 / 60.0;
    int steps = 0;
    *acc += frameSeconds;
    while (*acc >= DT && steps < 5) { /* stepSim(DT); */ *acc -= DT; steps++; }
    if (steps == 5) *acc = 0;       // drop the backlog instead of spiralling
    *alpha = *acc / DT;             // render interpolation factor
}

// ---------------------------------------------------------------- crash-safe-save
static int saveSafely(const char* dir, const void* data, size_t len)
{
    char tmp[256], cur[256], bak[256];
    snprintf(tmp, sizeof tmp, "%s/save.tmp", dir);
    snprintf(cur, sizeof cur, "%s/save.dat", dir);
    snprintf(bak, sizeof bak, "%s/save.bak", dir);

    FILE* f = fopen(tmp, "wb");
    if (!f) return -1;
    if (fwrite(data, 1, len, f) != len) { fclose(f); return -1; }
    fflush(f);
    fsync(fileno(f));               // push the data to the card before touching the old save
    fclose(f);

    remove(bak);                    // rename() will not replace an existing destination here
    rename(cur, bak);               // fails harmlessly on the very first save
    return rename(tmp, cur);        // on failure the old save is still in save.bak
}


// ---------------------------------------------------------------- lifecycle
static volatile bool g_docked, g_focused = true;

static void onApplet(AppletHookType type, void* param)
{
    (void)param;
    switch (type) {
        case AppletHookType_OnOperationMode:   // handheld <-> docked
            g_docked = (appletGetOperationMode() == AppletOperationMode_Console);
            break;                             // re-pick resolution / budgets here, not every frame
        case AppletHookType_OnFocusState:      // Home menu, overlay, sleep
            g_focused = (appletGetFocusState() == AppletFocusState_InFocus);
            break;                             // pause audio, stop submitting while not in focus
        default: break;
    }
}

static void lifecycleSetup(void)
{
    static AppletHookCookie cookie;
    appletHook(&cookie, onApplet, NULL);
    g_docked = (appletGetOperationMode() == AppletOperationMode_Console);
}


// ---------------------------------------------------------------- focus-mode
static void keepRunningInBackground(void)
{
    // default is to suspend on Home/sleep; NoSuspend keeps the process running (nxquake2 uses it)
    appletSetFocusHandlingMode(AppletFocusHandlingMode_NoSuspend);
    appletLockExit();                 // lets the app finish saving when an exit is requested
}

// ---------------------------------------------------------------- gpu-health
static bool gpuHealthy(DkQueue queue)
{
    // after a GPU error the queue is unusable (only dkQueueDestroy is legal); save, then exit
    return !dkQueueIsInErrorState(queue);
}

// ---------------------------------------------------------------- startup-checks
static bool requireFile(const char* path)
{
    struct stat st;
    if (stat(path, &st) == 0) return true;
    printf("missing data file: %s\n", path);   // name the file; do not fail silently
    return false;
}

// ---------------------------------------------------------------- pull-audio-block
static void padBlock(int16_t* stereoFrames, int have, int want)
{
    // when the source runs dry, repeat the last frame explicitly instead of leaving garbage (melonDS does this)
    if (have <= 0) return;
    uint32_t last = ((uint32_t*)stereoFrames)[have - 1];
    while (have < want) ((uint32_t*)stereoFrames)[have++] = last;
}

// ---------------------------------------------------------------- scale
static int integerScale(int destW, int renderW) { return destW / renderW; }

int main(int argc, char* argv[])
{
    (void)argc; (void)argv;
    // Fragments are compile-checked only; nothing here is run.
    if (argc > 99) {
        inputLoop(); nxlinkSetup();
        (void)createStaticBlock; (void)bindCameraUniforms; (void)updateSmallUniform; (void)frame;
        (void)teardown; (void)captureAndReplay; (void)imageBytes; (void)passToPass; (void)tick;
        (void)saveSafely; (void)integerScale; lifecycleSetup(); keepRunningInBackground(); (void)gpuHealthy; (void)requireFile; (void)padBlock;
    }
    return 0;
}
