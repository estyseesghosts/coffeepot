// drawbench: what does a citro3d draw call cost?
//
// Draws N small textured quads per frame in several ways and reports, per
// variant, several separately-timed costs (see README for what each covers):
//   record_ms    C3D_GetProcessingTime(): CPU time citro3d reports for recording
//                the frame's draw commands. It does NOT include C3D_FrameEnd.
//   frameend_ms  our own svcGetSystemTick() around C3D_FrameEnd(0): command
//                list finalisation, linear-heap cache flush, GX queueing.
//   begin_ms     svcGetSystemTick() around C3D_FrameBegin(SYNCDRAW): mostly
//                waiting for the previous frame's GPU work, not CPU cost.
//   gpu_ms       C3D_GetDrawingTime() of the last completed frame.
//   cmdbuf_pct   peak C3D_GetCmdBufUsage() before FrameEnd; near 100 means the
//                command buffer may have overflowed and the row is invalid.
// Variants:
//   batched    one C3D_DrawArrays for all N quads
//   per-quad   one C3D_DrawArrays per quad, nothing else changes
//   +uniform   per-quad draw, plus a uniform (offset) update before each draw
//   +texbind   per-quad draw, plus C3D_TexBind of the *same* texture each time
//   +texenv    per-quad draw, alternating between two combiner setups
//   +texswap   per-quad draw, alternating between two different textures
//
// The program sweeps N through 125, 250, 500, 1000, 2000 and 4096 quads by itself and then
// finishes. Output: a table on the bottom screen and CSV-style lines on stdout and in
// sdmc:/coffeepot/s<NNN>/drawbench.csv. START aborts the sweep.
#include <3ds.h>
#include <citro3d.h>
#include <stdio.h>
#include <string.h>
#include "bench.h"
#include "vshader_shbin.h"

#define MAX_QUADS 4096
#define FRAMES_PER_VARIANT 120
#define SKIP_FRAMES 4
#define CMDBUF_SIZE (C3D_DEFAULT_CMDBUF_SIZE * 4)   // per-quad variants need a large command buffer
#define CMDBUF_LIMIT 0.98f                          // usage above this is treated as overflow risk

typedef struct { float x, y, z, u, v; } Vertex;

static DVLB_s *dvlb;
static shaderProgram_s prog;
static int uProjection, uOffset;
static Vertex *vbo;
static C3D_Tex tex[2];

enum { V_BATCHED, V_PERQUAD, V_UNIFORM, V_TEXBIND, V_TEXENV, V_TEXSWAP, V_COUNT };
static const char *variant_names[V_COUNT] = { "batched", "per-quad", "+uniform", "+texbind", "+texenv", "+texswap" };

static bool make_texture(C3D_Tex *t, u32 color)
{
    if (!C3D_TexInit(t, 8, 8, GPU_RGBA8)) return false;
    u32 *p = (u32 *)t->data;   // 8x8 RGBA8 is one tile; layout is irrelevant for a flat colour
    for (int i = 0; i < 64; i++) p[i] = color;
    C3D_TexFlush(t);
    C3D_TexSetFilter(t, GPU_NEAREST, GPU_NEAREST);
    return true;
}

static void set_texenv(int which)
{
    C3D_TexEnv *env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    if (which == 0) {
        C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
        C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
    } else {
        C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, 0, 0);
        C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
    }
}

static void build_quads(void)
{
    // 4x4-pixel quads on a grid over the 400x240 screen, two triangles each.
    for (int q = 0; q < MAX_QUADS; q++) {
        float x = (q % 100) * 4.0f, y = ((q / 100) % 60) * 4.0f;
        Vertex *v = &vbo[q * 6];
        Vertex a = { x, y, 0.5f, 0, 0 }, b = { x + 4, y, 0.5f, 1, 0 };
        Vertex c = { x, y + 4, 0.5f, 0, 1 }, d = { x + 4, y + 4, 0.5f, 1, 1 };
        v[0] = a; v[1] = b; v[2] = c; v[3] = c; v[4] = b; v[5] = d;
    }
    Result rc = GSPGPU_FlushDataCache(vbo, sizeof(Vertex) * 6 * MAX_QUADS);
    if (R_FAILED(rc)) bench_error("GSPGPU_FlushDataCache(vbo)", rc);
}

static void draw_variant(int variant, int nquads)
{
    C3D_FVUnifSet(GPU_VERTEX_SHADER, uOffset, 0, 0, 0, 0);
    C3D_TexBind(0, &tex[0]);
    set_texenv(0);

    if (variant == V_BATCHED) {
        C3D_DrawArrays(GPU_TRIANGLES, 0, nquads * 6);
        return;
    }
    for (int q = 0; q < nquads; q++) {
        switch (variant) {
        case V_UNIFORM: C3D_FVUnifSet(GPU_VERTEX_SHADER, uOffset, (float)(q & 1), 0, 0, 0); break;
        case V_TEXBIND: C3D_TexBind(0, &tex[0]); break;
        case V_TEXENV:  set_texenv(q & 1); break;
        case V_TEXSWAP: C3D_TexBind(0, &tex[q & 1]); break;
        default: break;
        }
        C3D_DrawArrays(GPU_TRIANGLES, q * 6, 6);
    }
}

int main(void)
{
    gfxInitDefault();
    consoleInit(GFX_BOTTOM, NULL);

    bool isNew = false;
    bench_begin("drawbench", &isNew);
    bench_line("meta,cmdbuf_bytes,%lu", (unsigned long)CMDBUF_SIZE);
    bench_line("meta,frames_per_variant,%d", FRAMES_PER_VARIANT);
    bench_line("meta,skip_frames,%d", SKIP_FRAMES);
    bench_line("meta,quad_counts,125 250 500 1000 2000 %d", MAX_QUADS);
    bench_line("columns,result,drawbench,variant,quads,frames,record_ms,frameend_ms,begin_ms,gpu_ms,cmdbuf_pct");

    bool tex_ok[2] = { false, false };
    bool prog_ok = false;
    bool citro_ok = C3D_Init(CMDBUF_SIZE);
    if (!citro_ok) bench_error("C3D_Init", -1);

    C3D_RenderTarget *top = NULL;
    if (citro_ok) {
        top = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
        if (!top) bench_error("C3D_RenderTargetCreate", -1);
        dvlb = DVLB_ParseFile((u32 *)vshader_shbin, vshader_shbin_size);
        if (!dvlb) bench_error("DVLB_ParseFile", -1);
        vbo = linearAlloc(sizeof(Vertex) * 6 * MAX_QUADS);
        if (!vbo) bench_error("linearAlloc(vbo)", -1);
        tex_ok[0] = make_texture(&tex[0], 0xFF8040FF);
        tex_ok[1] = make_texture(&tex[1], 0xFF40C080);
        if (!tex_ok[0] || !tex_ok[1]) bench_error("C3D_TexInit", -1);
    }

    bool ready = top && dvlb && vbo && tex_ok[0] && tex_ok[1];
    if (ready) {
        C3D_RenderTargetSetOutput(top, GFX_TOP, GFX_LEFT,
            GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8));

        Result rc = shaderProgramInit(&prog);
        if (R_FAILED(rc)) { bench_error("shaderProgramInit", rc); ready = false; }
        else {
            prog_ok = true;
            rc = shaderProgramSetVsh(&prog, &dvlb->DVLE[0]);
            if (R_FAILED(rc)) { bench_error("shaderProgramSetVsh", rc); ready = false; }
        }
    }
    if (ready) {
        C3D_BindProgram(&prog);
        uProjection = shaderInstanceGetUniformLocation(prog.vertexShader, "projection");
        uOffset = shaderInstanceGetUniformLocation(prog.vertexShader, "offset");
        if (uProjection < 0 || uOffset < 0) { bench_error("shader uniform lookup", -1); ready = false; }

        C3D_AttrInfo *attr = C3D_GetAttrInfo();
        AttrInfo_Init(attr);
        AttrInfo_AddLoader(attr, 0, GPU_FLOAT, 3);   // v0 position
        AttrInfo_AddLoader(attr, 1, GPU_FLOAT, 2);   // v1 texcoord

        build_quads();
        C3D_BufInfo *buf = C3D_GetBufInfo();
        BufInfo_Init(buf);
        if (BufInfo_Add(buf, vbo, sizeof(Vertex), 2, 0x10) < 0) { bench_error("BufInfo_Add", -1); ready = false; }
    }

    C3D_Mtx proj;
    Mtx_OrthoTilt(&proj, 0.0f, 400.0f, 240.0f, 0.0f, 0.0f, 1.0f, true);

    static const int counts[] = { 125, 250, 500, 1000, 2000, MAX_QUADS };
    const int ncounts = (int)(sizeof counts / sizeof counts[0]);
    int ci = 0, nquads = counts[0], variant = 0, frame = 0;
    bool emit = true;
    double rec_sum = 0, end_sum = 0, beg_sum = 0, gpu_sum = 0;
    float cmd_max = 0;
    double rec_avg[V_COUNT] = { 0 }, end_avg[V_COUNT] = { 0 }, beg_avg[V_COUNT] = { 0 };
    double gpu_avg[V_COUNT] = { 0 }, cmd_peak[V_COUNT] = { 0 };
    printf(ready ? "drawbench: measuring...\n" : "drawbench: setup failed, see errors\n");

    while (aptMainLoop()) {
        hidScanInput();
        u32 down = hidKeysDown();
        if (down & KEY_START) break;
        if (!ready) { gspWaitForVBlank(); continue; }

        u64 t0 = svcGetSystemTick();
        if (!C3D_FrameBegin(C3D_FRAME_SYNCDRAW)) { bench_error("C3D_FrameBegin", -1); continue; }
        u64 t1 = svcGetSystemTick();
        C3D_RenderTargetClear(top, C3D_CLEAR_ALL, 0x101010FF, 0);
        C3D_FrameDrawOn(top);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uProjection, &proj);
        draw_variant(variant, nquads);
        float usage = C3D_GetCmdBufUsage();
        u64 t2 = svcGetSystemTick();
        C3D_FrameEnd(0);
        u64 t3 = svcGetSystemTick();

        // The citro3d timers describe the last completed frame; skip the first
        // frames of each variant so every sample belongs to this variant.
        if (frame >= SKIP_FRAMES) {
            rec_sum += C3D_GetProcessingTime();
            gpu_sum += C3D_GetDrawingTime();
            end_sum += (t3 - t2) / (CPU_TICKS_PER_USEC * 1000.0);
            beg_sum += (t1 - t0) / (CPU_TICKS_PER_USEC * 1000.0);
            if (usage > cmd_max) cmd_max = usage;
        }
        if (++frame == FRAMES_PER_VARIANT) {
            int n = FRAMES_PER_VARIANT - SKIP_FRAMES;
            rec_avg[variant] = rec_sum / n;
            end_avg[variant] = end_sum / n;
            beg_avg[variant] = beg_sum / n;
            gpu_avg[variant] = gpu_sum / n;
            cmd_peak[variant] = cmd_max * 100.0;
            if (cmd_max >= CMDBUF_LIMIT)
                bench_line("error,cmdbuf_overflow_risk,%s quads=%d usage=%.1f%%",
                           variant_names[variant], nquads, cmd_max * 100.0);
            frame = 0; rec_sum = end_sum = beg_sum = gpu_sum = 0; cmd_max = 0;
            if (++variant == V_COUNT) {
                variant = 0;
                if (emit) {   // machine-readable lines once per quad count
                    for (int v = 0; v < V_COUNT; v++)
                        bench_line("result,drawbench,%s,%d,%d,%.3f,%.3f,%.3f,%.3f,%.1f", variant_names[v],
                                   nquads, n, rec_avg[v], end_avg[v], beg_avg[v], gpu_avg[v], cmd_peak[v]);
                    emit = false;
                }
                printf("\x1b[2J\x1b[1;1Hdrawbench %s 3DS, %d quads/frame\n", isNew ? "New" : "Old", nquads);
                printf("%-9s %6s %6s %6s %6s %5s\n", "variant", "rec", "fend", "begin", "gpu", "cb%");
                for (int v = 0; v < V_COUNT; v++)
                    printf("%-9s %6.2f %6.2f %6.2f %6.2f %5.1f\n", variant_names[v], rec_avg[v], end_avg[v],
                           beg_avg[v], gpu_avg[v], cmd_peak[v]);
                printf("ms. rec excludes FrameEnd. us/quad vs batched\n(rec+fend), per-quad..texswap:\n");
                for (int v = 1; v < V_COUNT; v++)
                    printf("%.1f ", (rec_avg[v] + end_avg[v] - rec_avg[V_BATCHED] - end_avg[V_BATCHED])
                                        * 1000.0 / nquads);
                printf("\nsweep %d/%d. START: abort.\n", ci + 1, ncounts);
                if (++ci == ncounts) break;   // sweep complete
                nquads = counts[ci];
                emit = true;
            }
        }
    }

    if (tex_ok[0]) C3D_TexDelete(&tex[0]);
    if (tex_ok[1]) C3D_TexDelete(&tex[1]);
    if (vbo) linearFree(vbo);
    if (prog_ok) shaderProgramFree(&prog);
    if (dvlb) DVLB_Free(dvlb);
    if (citro_ok) C3D_Fini();
    bench_wait_exit();
    gfxExit();
    return 0;
}
