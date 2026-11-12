// A small native deko3d renderer used by nx/engineering/deko3d-renderer.md and
// nx/engineering/gpu-memory-and-synchronization.md.
//
// One textured, depth-tested, spinning cube is drawn into an offscreen image (pass A); a fullscreen pass (pass B)
// then samples that image and writes the swapchain image. Frames are pipelined through DKR_SLOTS slots, each owning
// its command memory, uniform memory, GPU report memory and one fence. GPU time comes from counter reports.
//
// Build:   make                          (link: -ldeko3dd while developing, see the Makefile)
// Options: -DDKR_SLOTS=3                 frame slots (default 2)
//          -DDKR_SELFTEST                scripted run that checks the picture and writes report.csv (see the page)
// Not a framework: every deko3d object is created and destroyed here, in order, so the ownership rules are visible.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <math.h>
#include <unistd.h>
#include <sys/stat.h>
#include <switch.h>
#include <deko3d.h>

#ifndef DKR_SLOTS
#define DKR_SLOTS 2
#endif
#define FB_NUM 2                        // swapchain images (independent of DKR_SLOTS)
#define W 1280
#define H 720
#define TEX 64
#define CMD_BYTES (64 * 1024)           // command memory per slot
#define OUT_DIR "sdmc:/switch/dkrenderer"

// ------------------------------------------------------------------ vertex data
typedef struct { float pos[3]; float uv[2]; } Vertex;
static Vertex g_verts[24];
static uint16_t g_idx[36];

// One face per row: u axis, v axis, normal (u x v = n, so the quad is counter-clockwise seen from outside).
static const float FACES[6][9] = {
    { 1, 0, 0,   0, 1, 0,   0, 0, 1 }, { -1, 0, 0,  0, 1, 0,   0, 0, -1 },
    { 0, 0, -1,  0, 1, 0,   1, 0, 0 }, { 0, 0, 1,   0, 1, 0,  -1, 0, 0 },
    { 1, 0, 0,   0, 0, -1,  0, 1, 0 }, { 1, 0, 0,   0, 0, 1,   0, -1, 0 },
};

static void buildCube(void)
{
    static const float QU[4] = { -1, 1, 1, -1 }, QV[4] = { -1, -1, 1, 1 };
    for (int f = 0; f < 6; f++) {
        const float* u = &FACES[f][0], *v = &FACES[f][3], *n = &FACES[f][6];
        for (int k = 0; k < 4; k++) {
            Vertex* o = &g_verts[f * 4 + k];
            for (int a = 0; a < 3; a++) { o->pos[a] = 0.5f * (n[a] + QU[k] * u[a] + QV[k] * v[a]); }
            o->uv[0] = 0.5f * (QU[k] + 1.0f);
            o->uv[1] = 0.5f * (QV[k] + 1.0f);
        }
        static const uint16_t Q[6] = { 0, 1, 2, 0, 2, 3 };
        for (int i = 0; i < 6; i++) { g_idx[f * 6 + i] = (uint16_t)(f * 4 + Q[i]); }
    }
}

// ------------------------------------------------------------------ matrices (column-major, like GLSL)
typedef struct { float m[16]; } Mat4;
static Mat4 mul(Mat4 a, Mat4 b)
{
    Mat4 r;
    for (int c = 0; c < 4; c++) {
        for (int rr = 0; rr < 4; rr++) {
            float s = 0;
            for (int k = 0; k < 4; k++) { s += a.m[k * 4 + rr] * b.m[c * 4 + k]; }
            r.m[c * 4 + rr] = s;
        }
    }
    return r;
}
static Mat4 identity(void) { Mat4 r = {{0}}; r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1; return r; }
static Mat4 rotY(float a) { Mat4 r = identity(); r.m[0] = cosf(a); r.m[8] = sinf(a); r.m[2] = -sinf(a); r.m[10] = cosf(a); return r; }
static Mat4 rotX(float a) { Mat4 r = identity(); r.m[5] = cosf(a); r.m[9] = -sinf(a); r.m[6] = sinf(a); r.m[10] = cosf(a); return r; }
static Mat4 translate(float x, float y, float z) { Mat4 r = identity(); r.m[12] = x; r.m[13] = y; r.m[14] = z; return r; }
static Mat4 perspective(float fovy, float aspect, float n, float f)   // right-handed, clip z in [0, 1]
{
    const float t = 1.0f / tanf(fovy * 0.5f);
    Mat4 r = {{0}};
    r.m[0] = t / aspect; r.m[5] = t; r.m[10] = f / (n - f); r.m[11] = -1.0f; r.m[14] = f * n / (n - f);
    return r;
}

// ------------------------------------------------------------------ debug callback
static FILE* g_log;
static void debugCallback(void* user, const char* context, DkResult result, const char* message)
{
    (void)user;
    fprintf(stderr, "deko3d %s in %s: %s\n", result == DkResult_Success ? "warning" : "error", context, message);
    if (g_log) { fprintf(g_log, "%s,deko3d,%s,%d,%s\n", result == DkResult_Success ? "warning" : "error", context, (int)result, message); fflush(g_log); }
    if (result != DkResult_Success) { abort(); }             // the library expects an error callback not to return
}

// ------------------------------------------------------------------ objects and memory layout
// Offsets inside g_data, each aligned for its use.
enum {
    OFF_VERTS = 0,
    OFF_IDX = 0x1000,
    OFF_IMGDESC = 0x2000,                      // DK_IMAGE_DESCRIPTOR_ALIGNMENT (0x20): 2 descriptors
    OFF_SMPDESC = 0x2100,                      // DK_SAMPLER_DESCRIPTOR_ALIGNMENT (0x20): 1 descriptor
    OFF_STAGING = 0x3000,                      // texture upload, TEX*TEX*4 = 16 KiB
    OFF_READBACK = 0x8000,                     // selftest readback of the final image, W*H*4
    OFF_SLOTS = OFF_READBACK + ((W * H * 4 + 0xFFF) & ~0xFFF),
    SLOT_STRIDE = 0x1000,                      // per slot: uniform at +0, reports at +0x100
    DATA_BYTES = OFF_SLOTS + DKR_SLOTS * SLOT_STRIDE,
};
#define REPORT_BYTES 0x100

typedef struct {
    DkCmdBuf cmdbuf;
    DkFence done;                              // signalled by the GPU when this slot's work has finished
    bool inFlight;                             // a submission exists that `done` will signal
} Slot;

static DkDevice g_device;
static DkQueue g_queue;
static DkSwapchain g_swapchain;
static DkMemBlock g_imageMem, g_codeMem, g_data, g_cmdMem;
static DkImage g_swap[FB_NUM], g_depth, g_offscreen, g_texture, g_checkOut;
static DkImageLayout g_swapLayout, g_depthLayout, g_colorLayout, g_texLayout;
static DkShader g_cubeVs, g_cubeFs, g_fsVs, g_fsFs;
static DkCmdBuf g_setupCmd;
static DkCmdList g_bindSwap[FB_NUM];
static Slot g_slots[DKR_SLOTS];
static unsigned g_frame;

static uint8_t* dataCpu(void) { return (uint8_t*)dkMemBlockGetCpuAddr(g_data); }
static DkGpuAddr dataGpu(uint32_t off) { return dkMemBlockGetGpuAddr(g_data) + off; }
static uint32_t alignUp(uint32_t v, uint32_t a) { return (v + a - 1) & ~(a - 1); }

static bool loadShader(DkShader* out, uint32_t* codeOff, const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) { printf("missing shader file %s\n", path); return false; }
    fseek(f, 0, SEEK_END);
    const uint32_t size = (uint32_t)ftell(f);
    rewind(f);
    if (*codeOff + alignUp(size, DK_SHADER_CODE_ALIGNMENT) + DK_SHADER_CODE_UNUSABLE_SIZE > 64 * 1024) { fclose(f); return false; }
    const bool ok = fread((uint8_t*)dkMemBlockGetCpuAddr(g_codeMem) + *codeOff, size, 1, f) == 1;
    fclose(f);
    if (!ok) { return false; }
    DkShaderMaker maker;
    dkShaderMakerDefaults(&maker, g_codeMem, *codeOff);
    dkShaderInitialize(out, &maker);
    *codeOff += alignUp(size, DK_SHADER_CODE_ALIGNMENT);
    return true;
}

static void colorLayout(DkImageLayout* l, uint32_t flags, DkImageFormat fmt, uint32_t w, uint32_t h)
{
    DkImageLayoutMaker m;
    dkImageLayoutMakerDefaults(&m, g_device);
    m.flags = flags;
    m.format = fmt;
    m.dimensions[0] = w;
    m.dimensions[1] = h;
    dkImageLayoutInitialize(l, &m);
}

// ------------------------------------------------------------------ setup
static bool graphicsInitialize(void)
{
    DkDeviceMaker dm;
    dkDeviceMakerDefaults(&dm);
    dm.cbDebug = debugCallback;                               // only the -ldeko3dd library ever calls it
    dm.flags = DkDeviceFlags_DepthZeroToOne | DkDeviceFlags_OriginLowerLeft;
    g_device = dkDeviceCreate(&dm);

    // Images: swapchain x FB_NUM, depth, offscreen colour, check target, texture. One block, each at its own alignment.
    colorLayout(&g_swapLayout, DkImageFlags_UsageRender | DkImageFlags_UsagePresent | DkImageFlags_HwCompression, DkImageFormat_RGBA8_Unorm, W, H);
    colorLayout(&g_depthLayout, DkImageFlags_UsageRender | DkImageFlags_HwCompression, DkImageFormat_ZF32, W, H);
    colorLayout(&g_colorLayout, DkImageFlags_UsageRender, DkImageFormat_RGBA8_Unorm, W, H);   // sampled and copied: uncompressed
    colorLayout(&g_texLayout, 0, DkImageFormat_RGBA8_Unorm, TEX, TEX);                          // block-linear by default

    uint32_t off = 0, swapOff[FB_NUM], depthOff, offOff, chkOff, texOff;
    for (int i = 0; i < FB_NUM; i++) {
        off = alignUp(off, dkImageLayoutGetAlignment(&g_swapLayout)); swapOff[i] = off; off += (uint32_t)dkImageLayoutGetSize(&g_swapLayout);
    }
    off = alignUp(off, dkImageLayoutGetAlignment(&g_depthLayout)); depthOff = off; off += (uint32_t)dkImageLayoutGetSize(&g_depthLayout);
    off = alignUp(off, dkImageLayoutGetAlignment(&g_colorLayout)); offOff = off; off += (uint32_t)dkImageLayoutGetSize(&g_colorLayout);
    off = alignUp(off, dkImageLayoutGetAlignment(&g_colorLayout)); chkOff = off; off += (uint32_t)dkImageLayoutGetSize(&g_colorLayout);
    off = alignUp(off, dkImageLayoutGetAlignment(&g_texLayout)); texOff = off; off += (uint32_t)dkImageLayoutGetSize(&g_texLayout);

    DkMemBlockMaker mb;
    dkMemBlockMakerDefaults(&mb, g_device, alignUp(off, DK_MEMBLOCK_ALIGNMENT));
    mb.flags = DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image;
    g_imageMem = dkMemBlockCreate(&mb);

    const DkImage* swapPtrs[FB_NUM];
    for (int i = 0; i < FB_NUM; i++) { dkImageInitialize(&g_swap[i], &g_swapLayout, g_imageMem, swapOff[i]); swapPtrs[i] = &g_swap[i]; }
    dkImageInitialize(&g_depth, &g_depthLayout, g_imageMem, depthOff);
    dkImageInitialize(&g_offscreen, &g_colorLayout, g_imageMem, offOff);
    dkImageInitialize(&g_checkOut, &g_colorLayout, g_imageMem, chkOff);
    dkImageInitialize(&g_texture, &g_texLayout, g_imageMem, texOff);

    DkSwapchainMaker sm;
    dkSwapchainMakerDefaults(&sm, g_device, nwindowGetDefault(), swapPtrs, FB_NUM);
    g_swapchain = dkSwapchainCreate(&sm);

    // Shader code: its own block with the Code flag. The last DK_SHADER_CODE_UNUSABLE_SIZE bytes must stay free.
    dkMemBlockMakerDefaults(&mb, g_device, 64 * 1024);
    mb.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Code;
    g_codeMem = dkMemBlockCreate(&mb);
    uint32_t codeOff = 0;
    if (!loadShader(&g_cubeVs, &codeOff, "romfs:/shaders/cube_vsh.dksh") || !loadShader(&g_cubeFs, &codeOff, "romfs:/shaders/cube_fsh.dksh") ||
        !loadShader(&g_fsVs, &codeOff, "romfs:/shaders/fullscreen_vsh.dksh") || !loadShader(&g_fsFs, &codeOff, "romfs:/shaders/fullscreen_fsh.dksh")) {
        return false;
    }

    // Vertex/index data, descriptors, staging, readback, per-slot uniforms and reports: CpuUncached | GpuCached (the defaults).
    dkMemBlockMakerDefaults(&mb, g_device, alignUp(DATA_BYTES, DK_MEMBLOCK_ALIGNMENT));
    g_data = dkMemBlockCreate(&mb);
    buildCube();
    memcpy(dataCpu() + OFF_VERTS, g_verts, sizeof g_verts);
    memcpy(dataCpu() + OFF_IDX, g_idx, sizeof g_idx);

    // Checkerboard texture, uploaded through a staging buffer and the GPU copy engine (which does the block-linear swizzle).
    uint32_t* px = (uint32_t*)(dataCpu() + OFF_STAGING);
    for (int y = 0; y < TEX; y++) {
        for (int x = 0; x < TEX; x++) { px[y * TEX + x] = (((x / 8) + (y / 8)) & 1) ? 0xFF2080F0u : 0xFFF0E0C0u; }   // ABGR bytes: R,G,B,A in memory
    }

    // Command memory: one region per slot, one command buffer per slot, plus a setup command buffer.
    dkMemBlockMakerDefaults(&mb, g_device, (DKR_SLOTS + 1) * CMD_BYTES);
    g_cmdMem = dkMemBlockCreate(&mb);
    DkCmdBufMaker cm;
    dkCmdBufMakerDefaults(&cm, g_device);
    for (int i = 0; i < DKR_SLOTS; i++) {
        g_slots[i].cmdbuf = dkCmdBufCreate(&cm);
        dkCmdBufAddMemory(g_slots[i].cmdbuf, g_cmdMem, (uint32_t)i * CMD_BYTES, CMD_BYTES);
        memset(&g_slots[i].done, 0, sizeof g_slots[i].done);  // a zeroed fence may be waited on before its first signal
    }
    g_setupCmd = dkCmdBufCreate(&cm);
    dkCmdBufAddMemory(g_setupCmd, g_cmdMem, DKR_SLOTS * CMD_BYTES, CMD_BYTES);

    DkQueueMaker qm;
    dkQueueMakerDefaults(&qm, g_device);
    qm.flags = DkQueueFlags_Graphics;
    g_queue = dkQueueCreate(&qm);

    // Descriptors: written once while no GPU work exists. Image 0 = checker texture, image 1 = offscreen colour.
    DkImageView view;
    dkImageViewDefaults(&view, &g_texture);
    dkImageDescriptorInitialize((DkImageDescriptor*)(dataCpu() + OFF_IMGDESC), &view, false, false);
    dkImageViewDefaults(&view, &g_offscreen);
    dkImageDescriptorInitialize((DkImageDescriptor*)(dataCpu() + OFF_IMGDESC + sizeof(DkImageDescriptor)), &view, false, false);
    DkSampler sampler;
    dkSamplerDefaults(&sampler);
    sampler.minFilter = sampler.magFilter = DkFilter_Linear;
    dkSamplerDescriptorInitialize((DkSamplerDescriptor*)(dataCpu() + OFF_SMPDESC), &sampler);

    // The setup command buffer is never cleared, so lists recorded in it stay valid for the whole run.
    // 1. Per-swapchain-image "bind as render target" lists, reused every frame.
    for (int i = 0; i < FB_NUM; i++) {
        DkImageView sv;
        dkImageViewDefaults(&sv, &g_swap[i]);
        dkCmdBufBindRenderTarget(g_setupCmd, &sv, NULL);
        g_bindSwap[i] = dkCmdBufFinishList(g_setupCmd);
    }
    // 2. One-time texture upload through the staging buffer, then wait for it (the only dkQueueWaitIdle outside teardown).
    DkImageView texView;
    dkImageViewDefaults(&texView, &g_texture);
    const DkCopyBuf src = { dataGpu(OFF_STAGING), 0, 0 };     // 0 = tightly packed
    const DkImageRect rect = { 0, 0, 0, TEX, TEX, 1 };
    dkCmdBufCopyBufferToImage(g_setupCmd, &src, &texView, &rect, 0);
    dkQueueSubmitCommands(g_queue, dkCmdBufFinishList(g_setupCmd));
    dkQueueFlush(g_queue);
    dkQueueWaitIdle(g_queue);                                 // the staging memory is only reusable after this
    return true;
}

// ------------------------------------------------------------------ per-frame recording
typedef struct { double cpuRecordUs, fenceWaitUs; uint64_t gpuNs; uint64_t fragInvocations; } FrameStats;

static void recordFrame(Slot* s, unsigned slotIdx, unsigned frame, bool check, DkCmdList* outA, DkCmdList* outB)
{
    DkCmdBuf cb = s->cmdbuf;
    dkCmdBufClear(cb);                                        // only legal because this slot's fence has been waited on

    // Uniform for this slot, written by the CPU into the slot's own region.
    const float a = (float)frame * 0.02f;
    const Mat4 mvp = mul(perspective(0.9f, (float)W / (float)H, 0.1f, 20.0f), mul(translate(0, 0, -2.2f), mul(rotX(a * 0.7f), rotY(a))));
    memcpy(dataCpu() + OFF_SLOTS + slotIdx * SLOT_STRIDE, mvp.m, sizeof mvp.m);
    const DkGpuAddr ubo = dataGpu(OFF_SLOTS + slotIdx * SLOT_STRIDE);
    const DkGpuAddr rep = dataGpu(OFF_SLOTS + slotIdx * SLOT_STRIDE + 0x100);

    DkRasterizerState rs; DkColorState cs; DkColorWriteState cws; DkDepthStencilState ds;
    dkRasterizerStateDefaults(&rs);
    dkColorStateDefaults(&cs);
    dkColorWriteStateDefaults(&cws);
    dkDepthStencilStateDefaults(&ds);
    ds.depthTestEnable = true; ds.depthWriteEnable = true; ds.depthCompareOp = DkCompareOp_Less;
    const DkViewport vp = { 0, 0, (float)W, (float)H, 0.0f, 1.0f };
    const DkScissor sc = { 0, 0, W, H };
    const DkVtxAttribState attribs[2] = {
        { 0, 0, offsetof(Vertex, pos), DkVtxAttribSize_3x32, DkVtxAttribType_Float, 0 },
        { 0, 0, offsetof(Vertex, uv), DkVtxAttribSize_2x32, DkVtxAttribType_Float, 0 },
    };
    const DkVtxBufferState bufState = { sizeof(Vertex), 0 };
    DkImageView offView, depthView;
    dkImageViewDefaults(&offView, &g_offscreen);
    dkImageViewDefaults(&depthView, &g_depth);

    // ---- pass A: cube into the offscreen image ----
    dkCmdBufBindImageDescriptorSet(cb, dataGpu(OFF_IMGDESC), 2);
    dkCmdBufBindSamplerDescriptorSet(cb, dataGpu(OFF_SMPDESC), 1);
    dkCmdBufReportCounter(cb, DkCounter_TimestampPipelineTop, rep);          // report 0: start (two 64-bit values: counter, timestamp)
    dkCmdBufResetCounter(cb, DkCounter_FragmentShaderInvocations);
    dkCmdBufBindRenderTarget(cb, &offView, &depthView);
    dkCmdBufSetViewports(cb, 0, &vp, 1);
    dkCmdBufSetScissors(cb, 0, &sc, 1);
    dkCmdBufClearColorFloat(cb, 0, DkColorMask_RGBA, 0.125f, 0.294f, 0.478f, 1.0f);
    dkCmdBufClearDepthStencil(cb, true, 1.0f, 0xFF, 0);
    const DkShader* cubeShaders[2] = { &g_cubeVs, &g_cubeFs };
    dkCmdBufBindShaders(cb, DkStageFlag_GraphicsMask, cubeShaders, 2);
    dkCmdBufBindRasterizerState(cb, &rs);                                    // default: cull back faces, counter-clockwise front
    dkCmdBufBindColorState(cb, &cs);
    dkCmdBufBindColorWriteState(cb, &cws);
    dkCmdBufBindDepthStencilState(cb, &ds);
    dkCmdBufBindVtxAttribState(cb, attribs, 2);
    dkCmdBufBindVtxBufferState(cb, &bufState, 1);
    dkCmdBufBindVtxBuffer(cb, 0, dataGpu(OFF_VERTS), sizeof g_verts);
    dkCmdBufBindIdxBuffer(cb, DkIdxFormat_Uint16, dataGpu(OFF_IDX));
    dkCmdBufBindUniformBuffer(cb, DkStage_Vertex, 0, ubo, 0x100);
    const DkResHandle texHandle = dkMakeTextureHandle(0, 0);
    dkCmdBufBindTextures(cb, DkStage_Fragment, 0, &texHandle, 1);
    dkCmdBufDrawIndexed(cb, DkPrimitive_Triangles, 36, 1, 0, 0, 0);
    dkCmdBufReportCounter(cb, DkCounter_FragmentShaderInvocations, rep + 0x10);   // report 1
    dkCmdBufDiscardDepthStencil(cb);                                         // depth is not needed after this pass
    // The offscreen image was just written as a render target and is about to be sampled: finish the writes, then
    // invalidate the texture cache so the sampler does not see stale data.
    dkCmdBufBarrier(cb, DkBarrier_Fragments, DkInvalidateFlags_Image);
    *outA = dkCmdBufFinishList(cb);

    // ---- pass B: fullscreen triangle sampling the offscreen image into the swapchain image (and the check image) ----
    // The swapchain image is bound by the g_bindSwap[image] list submitted between the two lists.
    const DkShader* fsShaders[2] = { &g_fsVs, &g_fsFs };
    DkImageView chkView;
    dkImageViewDefaults(&chkView, &g_checkOut);
    dkCmdBufSetViewports(cb, 0, &vp, 1);
    dkCmdBufSetScissors(cb, 0, &sc, 1);
    dkCmdBufClearColorFloat(cb, 0, DkColorMask_RGBA, 0.0f, 0.0f, 0.0f, 1.0f);
    dkCmdBufBindShaders(cb, DkStageFlag_GraphicsMask, fsShaders, 2);
    ds.depthTestEnable = false; ds.depthWriteEnable = false;
    dkCmdBufBindDepthStencilState(cb, &ds);
    rs.cullMode = DkFace_None;
    dkCmdBufBindRasterizerState(cb, &rs);
    const DkResHandle srcHandle = dkMakeTextureHandle(1, 0);
    dkCmdBufBindTextures(cb, DkStage_Fragment, 0, &srcHandle, 1);
    dkCmdBufDraw(cb, DkPrimitive_Triangles, 3, 1, 0, 0);
    if (check) {                                                             // selftest only: same pass into an image we can read back
        dkCmdBufBindRenderTarget(cb, &chkView, NULL);
        dkCmdBufClearColorFloat(cb, 0, DkColorMask_RGBA, 0.0f, 0.0f, 0.0f, 1.0f);
        dkCmdBufDraw(cb, DkPrimitive_Triangles, 3, 1, 0, 0);
        dkCmdBufBarrier(cb, DkBarrier_Fragments, 0);
        const DkCopyBuf dst = { dataGpu(OFF_READBACK), 0, 0 };
        const DkImageRect full = { 0, 0, 0, W, H, 1 };
        dkCmdBufCopyImageToBuffer(cb, &chkView, &full, &dst, 0);
    }
    dkCmdBufReportCounter(cb, DkCounter_Timestamp, rep + 0x20);              // report 2: end of everything above
    *outB = dkCmdBufFinishList(cb);
}

// ------------------------------------------------------------------ statistics and checks
#define MAX_SAMPLES 4096
static double g_cpuUs[MAX_SAMPLES], g_waitUs[MAX_SAMPLES], g_gpuUs[MAX_SAMPLES];
static unsigned g_samples, g_gpuSamples;

static double ticksUs(uint64_t t) { return (double)armTicksToNs(t) / 1000.0; }

static int cmpd(const void* a, const void* b) { const double x = *(const double*)a, y = *(const double*)b; return (x > y) - (x < y); }
static double percentile(const double* v, unsigned n, double p)
{
    if (n == 0) { return 0; }
    double* c = malloc(n * sizeof *c);
    if (!c) { return 0; }
    memcpy(c, v, n * sizeof *c);
    qsort(c, n, sizeof *c, cmpd);
    const double r = c[(unsigned)(p * (n - 1))];
    free(c);
    return r;
}

// Reads the reports of a finished slot. Valid only after that slot's fence has been waited on.
// A report is two 64-bit values: the counter, then the timestamp at which it was written.
static bool harvest(unsigned slotIdx, double* gpuUs, uint64_t* frag)
{
    const uint64_t* r = (const uint64_t*)(dataCpu() + OFF_SLOTS + slotIdx * SLOT_STRIDE + 0x100);
    *frag = r[2];                                             // report 1 (at +0x10): fragment shader invocations in pass A
    *gpuUs = (double)dkTimestampToNs(r[5] - r[1]) / 1000.0;   // timestamp of report 2 (+0x20) minus timestamp of report 0
    return r[5] > r[1];
}

#ifdef DKR_SELFTEST
static int g_fail;
static void check(const char* name, bool ok)
{
    if (g_log) { fprintf(g_log, "%s,%s\n", name, ok ? "pass" : "FAIL"); }
    if (!ok) { g_fail++; }
}

static void checkPicture(uint64_t frag)
{
    const uint8_t* px = dataCpu() + OFF_READBACK;
    static uint8_t seen[8 * 8 * 8];
    unsigned nonBg = 0, nDistinct = 0;
    const int bg[3] = { 122, 75, 32 };                        // clear colour (32, 75, 122) with red and blue swapped by pass B
    for (unsigned i = 0; i < W * H; i++) {
        const uint8_t* p = px + 4u * i;
        const bool isBg = abs(p[0] - bg[0]) <= 2 && abs(p[1] - bg[1]) <= 2 && abs(p[2] - bg[2]) <= 2;
        if (!isBg) {
            nonBg++;
            const unsigned q = (unsigned)((p[0] >> 5) * 64 + (p[1] >> 5) * 8 + (p[2] >> 5));
            if (!seen[q]) { seen[q] = 1; nDistinct++; }
        }
    }
    const uint8_t* c = px + 4u * ((H / 2) * W + W / 2);
    const uint8_t* k = px + 4u * (2 * W + 2);
    if (g_log) {
        fprintf(g_log, "info,non_background_pixels,%u\ninfo,distinct_colours,%u\ninfo,fragment_invocations,%llu\n",
                nonBg, nDistinct, (unsigned long long)frag);
    }
    check("corner_is_swapped_clear_colour", abs(k[0] - bg[0]) <= 2 && abs(k[1] - bg[1]) <= 2 && abs(k[2] - bg[2]) <= 2);
    check("cube_covers_5_to_60_percent", nonBg > W * H / 20 && nonBg < W * H * 6 / 10);
    check("cube_centre_is_not_background", abs(c[0] - bg[0]) > 2 || abs(c[1] - bg[1]) > 2 || abs(c[2] - bg[2]) > 2);
    check("texture_sampled_two_or_more_colours", nDistinct >= 2);
    check("fragment_counter_covers_picture", frag >= nonBg && frag <= (uint64_t)W * H * 2);
    FILE* f = fopen(OUT_DIR "/check.rgba", "wb");
    if (f) { fwrite(px, 4, (size_t)W * H, f); fclose(f); }
}
#endif

// ------------------------------------------------------------------ one frame
static void frame(bool check_frame)
{
    const unsigned si = g_frame % DKR_SLOTS;
    Slot* s = &g_slots[si];
    const uint64_t t0 = armGetSystemTick();

    if (s->inFlight) {
        dkFenceWait(&s->done, -1);                            // block until the GPU has finished this slot's previous frame
        s->inFlight = false;
        double gpuUs;                                         // its reports are readable now, and are overwritten by the next use
        uint64_t frag;
        if (harvest(si, &gpuUs, &frag) && g_gpuSamples < MAX_SAMPLES) { g_gpuUs[g_gpuSamples++] = gpuUs; }
    }
    const uint64_t t1 = armGetSystemTick();

    DkCmdList listA, listB;
    recordFrame(s, si, g_frame, check_frame, &listA, &listB);
    const int image = dkQueueAcquireImage(g_queue, g_swapchain);
    dkQueueSubmitCommands(g_queue, listA);
    dkQueueSubmitCommands(g_queue, g_bindSwap[image]);
    dkQueueSubmitCommands(g_queue, listB);
    dkQueueSignalFence(g_queue, &s->done, true);              // flush = true: GPU cache lines (the reports) reach memory
    s->inFlight = true;
    dkQueuePresentImage(g_queue, g_swapchain, image);         // also flushes the queue
    const uint64_t t2 = armGetSystemTick();

    if (g_frame >= DKR_SLOTS && g_samples < MAX_SAMPLES) {    // skip the first cycle, when no slot has been reused yet
        g_cpuUs[g_samples] = ticksUs(t2 - t1);
        g_waitUs[g_samples] = ticksUs(t1 - t0);
        g_samples++;
    }
#ifdef DKR_SELFTEST
    if (check_frame) {
        dkFenceWait(&s->done, -1);                            // selftest only: wait now so the readback can be inspected
        s->inFlight = false;
        double gpuUs;
        uint64_t frag;
        const bool ok = harvest(si, &gpuUs, &frag);
        check("gpu_report_timestamps_ordered", ok && gpuUs > 0.0);
        if (g_log) { fprintf(g_log, "info,gpu_us_this_frame,%.0f\n", gpuUs); }
        checkPicture(frag);
    }
#else
    (void)check_frame;
#endif
    g_frame++;
}

static void graphicsExit(void)
{
    dkQueueWaitIdle(g_queue);                                 // once, before destroying anything the GPU might still read
    dkQueueDestroy(g_queue);
    for (int i = 0; i < DKR_SLOTS; i++) { if (g_slots[i].cmdbuf) { dkCmdBufDestroy(g_slots[i].cmdbuf); } }
    if (g_setupCmd) { dkCmdBufDestroy(g_setupCmd); }
    if (g_cmdMem) { dkMemBlockDestroy(g_cmdMem); }
    if (g_data) { dkMemBlockDestroy(g_data); }
    if (g_codeMem) { dkMemBlockDestroy(g_codeMem); }
    dkSwapchainDestroy(g_swapchain);                          // before the image memory that backs its images
    if (g_imageMem) { dkMemBlockDestroy(g_imageMem); }
    dkDeviceDestroy(g_device);
}

int main(void)
{
    int rc = 0;
    const bool sockets = R_SUCCEEDED(socketInitializeDefault());
    const int nxlinkFd = sockets ? nxlinkStdio() : -1;
    const Result romfsRc = romfsInit();
    if (R_FAILED(romfsRc)) {
        printf("romfsInit failed: 0x%x\n", romfsRc);
        rc = 1;
        goto done;
    }
#ifdef DKR_SELFTEST
    mkdir(OUT_DIR, 0777);
    g_log = fopen(OUT_DIR "/report.csv", "w");
    if (g_log) { fprintf(g_log, "info,slots,%d\n", DKR_SLOTS); fflush(g_log); }
#endif
    if (!graphicsInitialize()) {
        printf("graphics setup failed\n");
        rc = 1;
        goto done_romfs;
    }

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);

    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) { break; }
        if (appletGetFocusState() != AppletFocusState_InFocus) {     // nothing to draw while unfocused
            svcSleepThread(20000000ULL);
            continue;
        }
#ifdef DKR_SELFTEST
        frame(g_frame == 100);
        if (g_frame >= 240) { break; }
#else
        frame(false);
        if (g_frame % 120 == 0 && g_samples > 0) {
            printf("slots %d  us p50/p95  record+submit %.0f/%.0f  fence wait %.0f/%.0f  gpu %.0f/%.0f\n", DKR_SLOTS,
                   percentile(g_cpuUs, g_samples, 0.5), percentile(g_cpuUs, g_samples, 0.95),
                   percentile(g_waitUs, g_samples, 0.5), percentile(g_waitUs, g_samples, 0.95),
                   percentile(g_gpuUs, g_gpuSamples, 0.5), percentile(g_gpuUs, g_gpuSamples, 0.95));
            g_samples = g_gpuSamples = 0;
        }
#endif
    }
#ifdef DKR_SELFTEST
    if (g_log) {
        fprintf(g_log, "info,record_submit_us_p50,%.0f\ninfo,fence_wait_us_p50,%.0f\ninfo,gpu_us_p50,%.0f\n",
                percentile(g_cpuUs, g_samples, 0.5), percentile(g_waitUs, g_samples, 0.5), percentile(g_gpuUs, g_gpuSamples, 0.5));
        check("frames_run", g_frame >= 240);
        fprintf(g_log, "note,done\n");
        fclose(g_log);
        g_log = NULL;
    }
#endif
    graphicsExit();
done_romfs:
    romfsExit();
done:
    if (nxlinkFd >= 0) { close(nxlinkFd); }
    if (sockets) { socketExit(); }
    return rc;
}
