// drawbench: what does a citro3d draw call cost?
//
// Draws N small textured quads per frame in several ways and reports, per
// variant, the average CPU time to record the frame (C3D_GetProcessingTime),
// the GPU time of the frame (C3D_GetDrawingTime), and the extra CPU cost per
// quad relative to drawing all quads with one call. Variants:
//   batched    one C3D_DrawArrays for all N quads
//   per-quad   one C3D_DrawArrays per quad, nothing else changes
//   +uniform   per-quad draw, plus a uniform (offset) update before each draw
//   +texbind   per-quad draw, plus C3D_TexBind of the *same* texture each time
//   +texenv    per-quad draw, alternating between two combiner setups
//   +texswap   per-quad draw, alternating between two different textures
//
// Up/Down double/halve N. Results print on the bottom screen.
#include <3ds.h>
#include <citro3d.h>
#include <stdio.h>
#include <string.h>
#include "vshader_shbin.h"

#define MAX_QUADS 4096
#define FRAMES_PER_VARIANT 120
#define SKIP_FRAMES 4

typedef struct { float x, y, z, u, v; } Vertex;

static DVLB_s *dvlb;
static shaderProgram_s prog;
static int uProjection, uOffset;
static Vertex *vbo;
static C3D_Tex tex[2];

enum { V_BATCHED, V_PERQUAD, V_UNIFORM, V_TEXBIND, V_TEXENV, V_TEXSWAP, V_COUNT };
static const char *variant_names[V_COUNT] = { "batched", "per-quad", "+uniform", "+texbind", "+texenv", "+texswap" };

static void make_texture(C3D_Tex *t, u32 color)
{
    C3D_TexInit(t, 8, 8, GPU_RGBA8);
    u32 *p = (u32 *)t->data;   // 8x8 RGBA8 is one tile; layout is irrelevant for a flat colour
    for (int i = 0; i < 64; i++) p[i] = color;
    C3D_TexFlush(t);
    C3D_TexSetFilter(t, GPU_NEAREST, GPU_NEAREST);
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
    GSPGPU_FlushDataCache(vbo, sizeof(Vertex) * 6 * MAX_QUADS);
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
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE * 4);   // per-quad variants need a large command buffer

    C3D_RenderTarget *top = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    C3D_RenderTargetSetOutput(top, GFX_TOP, GFX_LEFT,
        GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8));

    dvlb = DVLB_ParseFile((u32 *)vshader_shbin, vshader_shbin_size);
    shaderProgramInit(&prog);
    shaderProgramSetVsh(&prog, &dvlb->DVLE[0]);
    C3D_BindProgram(&prog);
    uProjection = shaderInstanceGetUniformLocation(prog.vertexShader, "projection");
    uOffset = shaderInstanceGetUniformLocation(prog.vertexShader, "offset");

    C3D_AttrInfo *attr = C3D_GetAttrInfo();
    AttrInfo_Init(attr);
    AttrInfo_AddLoader(attr, 0, GPU_FLOAT, 3);   // v0 position
    AttrInfo_AddLoader(attr, 1, GPU_FLOAT, 2);   // v1 texcoord

    vbo = linearAlloc(sizeof(Vertex) * 6 * MAX_QUADS);
    build_quads();
    C3D_BufInfo *buf = C3D_GetBufInfo();
    BufInfo_Init(buf);
    BufInfo_Add(buf, vbo, sizeof(Vertex), 2, 0x10);

    make_texture(&tex[0], 0xFF8040FF);
    make_texture(&tex[1], 0xFF40C080);

    C3D_Mtx proj;
    Mtx_OrthoTilt(&proj, 0.0f, 400.0f, 240.0f, 0.0f, 0.0f, 1.0f, true);

    bool isNew = false;
    APT_CheckNew3DS(&isNew);

    int nquads = 1000, variant = 0, frame = 0;
    double cpu_sum = 0, gpu_sum = 0;
    double cpu_avg[V_COUNT] = { 0 }, gpu_avg[V_COUNT] = { 0 };
    printf("drawbench: measuring...\n");

    while (aptMainLoop()) {
        hidScanInput();
        u32 down = hidKeysDown();
        if (down & KEY_START) break;
        if (down & (KEY_UP | KEY_DOWN)) {
            nquads = (down & KEY_UP) ? nquads * 2 : nquads / 2;
            if (nquads > MAX_QUADS) nquads = MAX_QUADS;
            if (nquads < 125) nquads = 125;
            variant = 0; frame = 0; cpu_sum = gpu_sum = 0;
        }

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C3D_RenderTargetClear(top, C3D_CLEAR_ALL, 0x101010FF, 0);
        C3D_FrameDrawOn(top);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uProjection, &proj);
        draw_variant(variant, nquads);
        C3D_FrameEnd(0);

        // The timers describe the last completed frame; skip the first frames
        // of each variant so every sample belongs to this variant.
        if (frame >= SKIP_FRAMES) {
            cpu_sum += C3D_GetProcessingTime();
            gpu_sum += C3D_GetDrawingTime();
        }
        if (++frame == FRAMES_PER_VARIANT) {
            cpu_avg[variant] = cpu_sum / (FRAMES_PER_VARIANT - SKIP_FRAMES);
            gpu_avg[variant] = gpu_sum / (FRAMES_PER_VARIANT - SKIP_FRAMES);
            frame = 0; cpu_sum = gpu_sum = 0;
            if (++variant == V_COUNT) {
                variant = 0;
                printf("\x1b[2J\x1b[1;1Hdrawbench %s 3DS, %d quads/frame\n", isNew ? "New" : "Old", nquads);
                printf("%-9s %8s %8s %9s\n", "variant", "cpu ms", "gpu ms", "us/quad");
                for (int v = 0; v < V_COUNT; v++)
                    printf("%-9s %8.2f %8.2f %9.2f\n", variant_names[v], cpu_avg[v], gpu_avg[v],
                           v == V_BATCHED ? 0.0 : (cpu_avg[v] - cpu_avg[V_BATCHED]) * 1000.0 / nquads);
                printf("\nUp/Down: quads x2 or /2. START: exit.\n");
            }
        }
    }

    C3D_TexDelete(&tex[0]);
    C3D_TexDelete(&tex[1]);
    linearFree(vbo);
    shaderProgramFree(&prog);
    DVLB_Free(dvlb);
    C3D_Fini();
    gfxExit();
    return 0;
}
