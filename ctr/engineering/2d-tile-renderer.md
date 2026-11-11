# 2D tile renderer: a worked example and experiment rig

A scrolling tilemap is the standard test of a 2D engine: a world far larger than the screen, a camera that follows the player, animated tiles, characters that walk in front of and behind each other, text, and a way to see what the frame costs. This page walks through one complete program that does all of that with [citro2d](rendering-2d.md), using only art generated in code, so it builds with no asset files. The program also has compile-time switches that change *how the tiles reach the GPU* (four strategies) and a scripted mode that records per-phase timings to a file on the SD card, so the crossover questions on [2D rendering](rendering-2d.md#design-alternatives-and-how-to-choose) can be answered on a console.

> **Status: this is an experiment rig, not a performance reference.** Nothing on this page has been run on an Old 3DS, a New 3DS or an emulator. Every number below is arithmetic from sizes or a `[hypothesis]`; no frame time, speed-up or crossover has been measured, and none may be quoted from here. What *has* been done: the program compiles without warnings in 30 configurations ([build matrix](#verification-status)). Expect to debug the first run (orientation of the cached layer, the manual-draw state hand-over; see [Pitfalls](#pitfalls)). The result of a run is a CSV file whose format is fixed in [Output records](#output-records) so that results from different people can be compared.

Read [2D rendering](rendering-2d.md) first for the setup, coordinate and texture rules this program relies on. This page assumes you know what an atlas, a draw call and a batch are (defined there).

## Before you start

- Target: **Old 3DS** (the program needs nothing from the New 3DS). Built with devkitARM r68 (GCC 16.1.0), citro2d 1.7.0, citro3d 1.7.1, libctru 2.7.1 ([source versions](source-versions.md)). **New 3DS:** runs the same code; record the model and note the speed-up state yourself (the program cannot read it) and keep New 3DS results in a separate data set.
- Default build (no `-D` flags) is the original tile renderer: direct `C2D_DrawImageAt` per visible tile, debug overlay, no files written.
- Controls (interactive builds): Circle Pad or D-pad moves the red character, SELECT shows or hides the debug text on the bottom screen, START quits. In a `BENCH_AUTO=1` build only START works; the camera follows a scripted path.
- Strategies 1 and 2 need one extra file, `source/vshader.v.pica` (listing [below](#the-shader-for-strategies-1-and-2)), assembled by the template `Makefile`'s picasso rule into `vshader_shbin.h`. The other builds do not use it.

## What the program does

| Part | How it is done | Why |
| ---- | -------------- | --- |
| World | 128×96 tiles of `TILE` pixels (default 16: 2048×1536 pixels), generated from sine waves and a hash | No map file |
| Atlas | One 128×32 RGBA8 texture: row 0 has 4 ground tiles and 4 water frames, row 1 has sprites; painted and swizzled in code | One texture means tiles and sprites can share batches |
| Camera | Follows the player, clamped to the world, converted to whole pixels with `floorf` | Pixel art shimmers at fractional positions |
| Tiles | Loop over the visible tile range only, then submit by the chosen [strategy](#strategies-and-compile-time-modes) | At most 26×16 = 416 quads at 16 px instead of 12,288 |
| Tile animation | Water tiles pick one of four atlas frames from the frame counter | Keeps the animation in the atlas, no per-vertex work |
| Sprites | 40 wandering characters plus the player, insertion-sorted by y each frame, a shadow drawn under each | Painter's algorithm for overlap; same atlas, so no batch break |
| HUD | A static title parsed once; the tile position text re-parsed only when the visible tile range changes | Text parsing costs CPU every time; cache it |
| Bottom screen | World overview rectangles, a camera box, a player dot, and the debug text | Shows primitives and a second target |
| Timing | Per-phase `svcGetSystemTick` timers; a 30-frame overlay, or per-frame samples written as CSV | See [Per-phase timing](#per-phase-timing) |

## The program

Single file `main.c`. Build flags and exact compiler: [Verification status](#verification-status). With no `-D` flag the behaviour is that of the original program (one more guarded feature: the program installs an [APT](../glossary.md#apt) hook (suspend/resume notification) only when the cache strategy or `BENCH_AUTO` is on).

```c
/* tile_renderer: scrolling tilemap + y-sorted sprites + cached HUD text + debug overlay.
 * No art assets: the atlas is painted in code. Needs citro2d, citro3d, libctru.
 * Compile-time modes (all default to the original behaviour) select the tile strategy and turn
 * the program into a scripted experiment that writes CSV records to sdmc. See the page. */
#include <3ds.h>
#include <citro2d.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- compile-time modes ---- */
#ifndef TILE_STRATEGY
#if defined(USE_TILE_CACHE) && USE_TILE_CACHE
#define TILE_STRATEGY 3
#else
#define TILE_STRATEGY 0      /* 0 direct C2D_DrawImageAt per tile, 1 manual vertex buffer rebuilt every frame,
                                2 chunked retained vertex buffers, 3 cached background texture */
#endif
#endif
#ifndef CACHE_FLIP_V
#define CACHE_FLIP_V 0       /* 1 if the cached layer shows up upside down on the first run (orientation is unverified) */
#endif
#ifndef TILE
#define TILE 16              /* drawn tile size in pixels (8..32); the atlas art stays 16x16 and is scaled */
#endif
#ifndef CHUNK_TILES
#define CHUNK_TILES 16       /* strategy 2: chunk edge in tiles; must divide 128 and 96 */
#endif
#ifndef TEX_MODE
#define TEX_MODE 0           /* strategy 0 only. 0 one atlas, 1 two identical atlases alternating per tile, 2 same two atlases drawn as two runs */
#endif
#ifndef MAP_EDIT_PERIOD
#define MAP_EDIT_PERIOD 0    /* N > 0: every N frames change one visible tile (forces a chunk rebuild or a cache redraw) */
#endif
#ifndef SHAPE_MODE
#define SHAPE_MODE 0         /* bottom screen: 0 off, 1 rectangle and circle interleaved, 2 all rectangles then all circles */
#endif
#ifndef SHAPE_PAIRS
#define SHAPE_PAIRS 32
#endif
#ifndef BOTTOM_PERIOD
#define BOTTOM_PERIOD 1      /* redraw the bottom screen every N frames; other frames leave it untouched */
#endif
#ifndef CMDBUF_SIZE
#define CMDBUF_SIZE C3D_DEFAULT_CMDBUF_SIZE   /* raise (for example 0x100000) when thousands of separate draw calls are expected */
#endif
#ifndef NPC_COUNT
#define NPC_COUNT 40
#endif
#ifndef CAM_SPEED
#define CAM_SPEED 2.0f       /* player speed in pixels per frame */
#endif
#ifndef BENCH_AUTO
#define BENCH_AUTO 0         /* 1: scripted camera path, fixed frame count, CSV file, no input except START */
#endif
#ifndef BENCH_PATH
#define BENCH_PATH 0         /* 0 square loop (legs of BENCH_LEG frames), 1 camera stands still */
#endif
#ifndef BENCH_LEG
#define BENCH_LEG 120
#endif
#ifndef BENCH_WARMUP
#define BENCH_WARMUP 120
#endif
#ifndef BENCH_FRAMES
#define BENCH_FRAMES 600
#endif
#define MISSED_FRAME_MS 25.0f /* a loop period longer than this counts as a missed frame (nominal period 16.7 ms) */

#define TOP_W 400
#define BOT_W 320
#define SCR_H 240
#define CELL 16               /* atlas cell size in pixels */
#define MAP_W 128
#define MAP_H 96
#define WORLD_W (MAP_W * TILE)
#define WORLD_H (MAP_H * TILE)
#define TSCALE ((float)TILE / (float)CELL)
#define Z_ALL 0.5f           /* one depth for every layer: C2D_Prepare sets the depth test to GEQUAL, so equal depths pass and draw order decides */

#if TILE < 8 || TILE > 32
#error "TILE must be 8..32"
#endif
#if TILE_STRATEGY == 3 && (TILE != 16 && TILE != 32)
#error "the cache strategy supports TILE 16 or 32 (a 512 px window must hold a whole number of tiles and a rebuild must fit the object limit)"
#endif
#if TILE_STRATEGY == 2 && (MAP_W % CHUNK_TILES || MAP_H % CHUNK_TILES)
#error "CHUNK_TILES must divide the map size"
#endif
#if TEX_MODE && TILE_STRATEGY != 0
#error "TEX_MODE is only implemented for the direct strategy"
#endif

#if TILE_STRATEGY == 1 || TILE_STRATEGY == 2
#include "vshader_shbin.h"   /* built from vshader.v.pica (listed on the page) by the template Makefile rule */
#endif

enum { T_GRASS, T_SAND, T_STONE, T_BUSH, T_WATER0, T_WATER1, T_WATER2, T_WATER3, TILE_KINDS };
enum { S_PLAYER0 = 8, S_PLAYER1, S_NPC0, S_NPC1, S_SHADOW, ATLAS_IMAGES };

#define ATLAS_W 128           /* 8 images of 16 px per row */
#define ATLAS_H 32            /* row 0: tiles, row 1: sprites */

static u8 gMap[MAP_H][MAP_W];

/* ---------- texture helpers ---------- */
static const u8 kMortonSpread[8] = { 0, 1, 4, 5, 16, 17, 20, 21 };

/* top-down row-major 0xRRGGBBAA -> 8x8 tiles, Morton order inside a tile (w, h multiples of 8). */
static void swizzleRGBA8(u32 *dst, const u32 *src, u32 w, u32 h)
{
	for (u32 y = 0; y < h; y++)
		for (u32 x = 0; x < w; x++) {
			u32 tile  = (y >> 3) * (w >> 3) + (x >> 3);
			u32 inner = kMortonSpread[x & 7] | (kMortonSpread[y & 7] << 1);
			dst[tile * 64 + inner] = src[y * w + x];
		}
}

static u32 rgba(u8 r, u8 g, u8 b, u8 a) { return ((u32)r << 24) | ((u32)g << 16) | ((u32)b << 8) | a; }
static u32 hash2(u32 x, u32 y) { u32 h = x * 374761393u + y * 668265263u; h = (h ^ (h >> 13)) * 1274126177u; return h ^ (h >> 16); }

static void paintTile(u32 *img, int index, int kind, int variant)
{
	int ox = (index % 8) * CELL, oy = (index / 8) * CELL;
	for (int y = 0; y < CELL; y++)
		for (int x = 0; x < CELL; x++) {
			int n = (int)(hash2((u32)(x + 31 * kind), (u32)(y + 17 * variant)) & 15);
			u32 c;
			switch (kind) {
			case T_GRASS: c = rgba((u8)(40 + n), (u8)(120 + 2 * n), 50, 255); break;
			case T_SAND:  c = rgba((u8)(210 + n), (u8)(190 + n), 120, 255); break;
			case T_STONE: c = rgba((u8)(110 + 3 * n), (u8)(110 + 3 * n), (u8)(120 + 3 * n), 255); break;
			case T_BUSH:  c = (x - 8) * (x - 8) + (y - 8) * (y - 8) < 36 ? rgba(20, (u8)(70 + 4 * n), 30, 255) : rgba((u8)(40 + n), (u8)(120 + 2 * n), 50, 255); break;
			default: { /* water: diagonal bands that move with the frame number */
				int band = ((x + y + variant * 4) >> 2) & 1;
				c = rgba(30, (u8)(80 + 30 * band + n), (u8)(190 + 3 * n), 255);
			} }
			img[(oy + y) * ATLAS_W + ox + x] = c;
		}
}

static void paintSprite(u32 *img, int index, u32 body, int step, int shadow)
{
	int ox = (index % 8) * CELL, oy = (index / 8) * CELL;
	for (int y = 0; y < CELL; y++)
		for (int x = 0; x < CELL; x++) {
			u32 c = 0;
			if (shadow) {
				if ((x - 8) * (x - 8) + (y - 12) * (y - 12) * 4 < 40) c = rgba(0, 0, 0, 110);
			} else {
				int inside = (x - 8) * (x - 8) + (y - 8 + step) * (y - 8 + step) < 28;
				int legs = y >= 12 + step && (x == 5 + step || x == 10 - step);
				if (inside || legs) c = body;
				if (inside && y == 6 + step && (x == 6 || x == 10)) c = rgba(10, 10, 10, 255);
			}
			img[(oy + y) * ATLAS_W + ox + x] = c;
		}
}

/* UV box of image `index` (16x16 at column index%8, row index/8) in the atlas. v = 1 is the top of the stored image. */
static void atlasSub(Tex3DS_SubTexture *s, int index)
{
	int px = (index % 8) * CELL, py = (index / 8) * CELL;
	s->width = CELL; s->height = CELL;
	s->left = (float)px / ATLAS_W;            s->right  = (float)(px + CELL) / ATLAS_W;
	s->top  = 1.0f - (float)py / ATLAS_H;     s->bottom = 1.0f - (float)(py + CELL) / ATLAS_H;
}

/* ---------- world ---------- */
static void buildMap(void)
{
	for (int y = 0; y < MAP_H; y++)
		for (int x = 0; x < MAP_W; x++) {
			float v = sinf(x * 0.15f) + cosf(y * 0.2f) + sinf((x + y) * 0.07f);
			u8 t = T_GRASS;
			if (v > 1.35f) t = T_WATER0;
			else if (v > 1.15f) t = T_SAND;
			else if (hash2((u32)x, (u32)y) % 23 == 0) t = T_BUSH;
			else if (v < -1.4f) t = T_STONE;
			gMap[y][x] = t;
		}
}

typedef struct { float x, y, vx, vy; int image; int timer; } Actor;
static Actor gActor[NPC_COUNT + 1];   /* [0] is the player */
static u32 gRng = 12345;
static u32 rnd(void) { gRng = gRng * 1664525u + 1013904223u; return gRng >> 8; }

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static float ticksToMs(u64 t) { return (float)t * 1000.0f / (float)SYSCLOCK_ARM11; }

#if TEX_MODE
static C3D_Tex gAtlas2;                  /* an identical second texture */
#endif
static Tex3DS_SubTexture gSub[ATLAS_IMAGES];
static C2D_Image gImg[ATLAS_IMAGES], gImg2[ATLAS_IMAGES];
static int gDropped __attribute__((unused));   /* C2D tile draw calls that returned false (object limit) */

#if TILE_STRATEGY == 3
#define CACHE_TILES (512 / TILE)             /* 32 x 16 px (or 16 x 32 px) = 512 px square */
static C3D_Tex gCacheTex;
static C3D_RenderTarget *gCacheTarget;
static int gCacheTX, gCacheTY;
static bool gCacheValid;
#endif

#if TILE_STRATEGY == 3 || BENCH_AUTO
static int gResumeEvents;                /* HOME-restore and sleep-wake notifications seen */
static aptHookCookie gHook;
static void onApt(APT_HookType hook, void *param)
{
	(void)param;
	if (hook == APTHOOK_ONRESTORE || hook == APTHOOK_ONWAKEUP) {
		gResumeEvents++;
#if TILE_STRATEGY == 3
		gCacheValid = false;             /* VRAM contents across suspend are not established: redraw the cache */
#endif
	}
}
#endif

#if TILE_STRATEGY == 1 || TILE_STRATEGY == 2
/* ---- own vertex buffers: one linear allocation holding [retained chunks][dynamic tiles] ---- */
typedef struct { float x, y, z, u, v; } Vertex;
#define DYN_TILES (((TOP_W / TILE) + 3) * ((SCR_H / TILE) + 3))
static DVLB_s *gDvlb;
static shaderProgram_s gProg;
static C3D_AttrInfo gAttr;
static C3D_BufInfo gBuf;
static C3D_Mtx gProj;
static int gUProj, gUOff;
static Vertex *gVbo, *gDyn;
#endif
#if TILE_STRATEGY == 2
#define CHUNKS_X (MAP_W / CHUNK_TILES)
#define CHUNKS_Y (MAP_H / CHUNK_TILES)
#define CHUNK_VERTS (CHUNK_TILES * CHUNK_TILES * 6)
static int gChunkCount[CHUNKS_Y][CHUNKS_X];
#endif

#if TILE_STRATEGY == 1 || TILE_STRATEGY == 2
static Vertex *emitQuad(Vertex *p, float x, float y, int image)
{
	const Tex3DS_SubTexture *t = &gSub[image];
	float x1 = x + (float)TILE, y1 = y + (float)TILE;
	p[0] = (Vertex){ x,  y,  Z_ALL, t->left,  t->top };
	p[1] = (Vertex){ x,  y1, Z_ALL, t->left,  t->bottom };
	p[2] = (Vertex){ x1, y1, Z_ALL, t->right, t->bottom };
	p[3] = p[0];
	p[4] = p[2];
	p[5] = (Vertex){ x1, y,  Z_ALL, t->right, t->top };
	return p + 6;
}

/* Bind our shader, vertex layout, atlas and combiner; depth test off so citro2d's later sprites pass. */
static void manualBegin(C3D_Tex *atlas, float offX, float offY)
{
	C2D_Flush();
	C3D_BindProgram(&gProg);
	C3D_SetAttrInfo(&gAttr);
	C3D_SetBufInfo(&gBuf);
	C3D_TexBind(0, atlas);
	C3D_TexEnv *env = C3D_GetTexEnv(0);
	C3D_TexEnvInit(env);
	C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, 0, 0);
	C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
	C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
	C3D_CullFace(GPU_CULL_NONE);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, gUProj, &gProj);
	C3D_FVUnifSet(GPU_VERTEX_SHADER, gUOff, offX, offY, 0.0f, 0.0f);
}

/* Give the state back to citro2d (documented: C2D_Prepare after changing citro3d state yourself). */
static void manualEnd(C3D_RenderTarget *target)
{
	C3D_CullFace(GPU_CULL_BACK_CCW);
	C2D_Prepare();
	C2D_SceneBegin(target);   /* re-applies citro2d's projection for this target */
}
#endif

#if TILE_STRATEGY == 2
static void buildChunk(int cx, int cy)
{
	Vertex *start = gVbo + (cy * CHUNKS_X + cx) * CHUNK_VERTS, *p = start;
	for (int ty = cy * CHUNK_TILES; ty < (cy + 1) * CHUNK_TILES; ty++)
		for (int tx = cx * CHUNK_TILES; tx < (cx + 1) * CHUNK_TILES; tx++) {
			int id = gMap[ty][tx];
			if (id == T_WATER0) continue;          /* animated tiles are drawn by the dynamic pass */
			p = emitQuad(p, (float)(tx * TILE), (float)(ty * TILE), id);
		}
	gChunkCount[cy][cx] = (int)(p - start);
}
#endif

/* ---------- experiment output: bench.h-style records written to sdmc ---------- */
#if BENCH_AUTO
enum { PH_PERIOD, PH_UPDATE, PH_BUILD, PH_RECORD, PH_BEGIN, PH_END, PH_PROC, PH_GPU, PH_COUNT };
static const char *const kPhaseName[PH_COUNT] = { "period", "update", "build", "record", "framebegin", "frameend", "c3d_proc", "c3d_gpu" };
static float gSamp[PH_COUNT][BENCH_FRAMES];   /* microseconds */
static FILE *gCsv;
static int gCsvErrors;

static const char *strategyName(void)
{
	static const char *const n[4] = { "direct", "manual", "chunks", "cache" };
	return n[TILE_STRATEGY];
}

static int cmpFloat(const void *a, const void *b) { float x = *(const float *)a, y = *(const float *)b; return (x > y) - (x < y); }

static void csvLine(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void csvLine(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	if (gCsv) { vfprintf(gCsv, fmt, ap); fputc('\n', gCsv); fflush(gCsv); }
	va_end(ap);
}

static void csvError(const char *where, Result rc) { gCsvErrors++; csvLine("error,%s,rc=0x%08lx", where, (unsigned long)rc); }

/* Same meta keys as ctr/benchmarks/common/bench.h. The file is opened for append: several runs can share it. */
static void csvBegin(void)
{
	static const char *const models[] = { "Old 3DS", "Old 3DS XL", "New 3DS", "2DS", "New 3DS XL", "New 2DS XL" };
	char path[96];
	snprintf(path, sizeof path, "sdmc:/coffeepot-tilerenderer.csv");
	gCsv = fopen(path, "a");
	if (!gCsv) gCsvErrors++;
	bool isNew = false;
	Result rc = APT_CheckNew3DS(&isNew);
	if (R_FAILED(rc)) csvError("APT_CheckNew3DS", rc);
	u8 model = 0xFF;
	rc = cfguInit();
	if (R_SUCCEEDED(rc)) { rc = CFGU_GetSystemModel(&model); if (R_FAILED(rc)) csvError("CFGU_GetSystemModel", rc); cfguExit(); }
	else csvError("cfguInit", rc);
	csvLine("meta,program,tilerenderer");
	csvLine("meta,build_date,%s %s", __DATE__, __TIME__);
	csvLine("meta,compiler,%s", __VERSION__);
	csvLine("meta,libctru_version,not-readable");
	csvLine("meta,citro3d_version,not-readable");
	csvLine("meta,model,%s", model < sizeof models / sizeof models[0] ? models[model] : "unknown");
	csvLine("meta,new3ds,%d", isNew ? 1 : 0);
	u32 fw = osGetFirmVersion(), kv = osGetKernelVersion();
	csvLine("meta,firm_version,%lu.%lu.%lu", (unsigned long)GET_VERSION_MAJOR(fw), (unsigned long)GET_VERSION_MINOR(fw), (unsigned long)GET_VERSION_REVISION(fw));
	csvLine("meta,kernel_version,%lu.%lu.%lu", (unsigned long)GET_VERSION_MAJOR(kv), (unsigned long)GET_VERSION_MINOR(kv), (unsigned long)GET_VERSION_REVISION(kv));
	csvLine("meta,speedup_l2_state,not-readable");
	csvLine("meta,cpu_ticks_per_usec,%.4f", CPU_TICKS_PER_USEC);
	csvLine("meta,linear_free_bytes,%lu", (unsigned long)linearSpaceFree());
	csvLine("meta,vram_free_bytes,%lu", (unsigned long)vramSpaceFree());
	csvLine("meta,tile_strategy,%s", strategyName());
	csvLine("meta,tile_px,%d", TILE);
	csvLine("meta,chunk_tiles,%d", TILE_STRATEGY == 2 ? CHUNK_TILES : 0);
	csvLine("meta,tex_mode,%d", TEX_MODE);
	csvLine("meta,map_edit_period,%d", MAP_EDIT_PERIOD);
	csvLine("meta,shape_mode,%d", SHAPE_MODE);
	csvLine("meta,shape_pairs,%d", SHAPE_PAIRS);
	csvLine("meta,bottom_period,%d", BOTTOM_PERIOD);
	csvLine("meta,npc_count,%d", NPC_COUNT);
	csvLine("meta,cam_speed_px_per_frame,%.2f", (double)CAM_SPEED);
	csvLine("meta,bench_path,%d", BENCH_PATH);
	csvLine("meta,warmup_frames,%d", BENCH_WARMUP);
	csvLine("meta,measured_frames,%d", BENCH_FRAMES);
}

static void csvResults(int visibleTiles, int rebuilds, int droppedBase, float cmdPeak, u32 vblanks)
{
	char variant[96];
	snprintf(variant, sizeof variant, "%s-t%d-c%d-x%d-e%d-s%dx%d-b%d-v%.1f-n%d-p%d", strategyName(), TILE, TILE_STRATEGY == 2 ? CHUNK_TILES : 0, TEX_MODE,
		MAP_EDIT_PERIOD, SHAPE_MODE, SHAPE_PAIRS, BOTTOM_PERIOD, (double)CAM_SPEED, NPC_COUNT, BENCH_PATH);
	int missed = 0;
	for (int i = 0; i < BENCH_FRAMES; i++) if (gSamp[PH_PERIOD][i] > MISSED_FRAME_MS * 1000.0f) missed++;
	for (int ph = 0; ph < PH_COUNT; ph++) {
		static float tmp[BENCH_FRAMES];
		memcpy(tmp, gSamp[ph], sizeof tmp);
		qsort(tmp, BENCH_FRAMES, sizeof tmp[0], cmpFloat);
		csvLine("result,tilerenderer,%s,%s,%d,%d,%.2f,%.2f,%.2f", kPhaseName[ph], variant, visibleTiles, BENCH_FRAMES,
			(double)tmp[0], (double)tmp[BENCH_FRAMES / 2], (double)tmp[BENCH_FRAMES - 1]);
		double sum = 0;
		for (int i = 0; i < BENCH_FRAMES; i++) sum += tmp[i];
		csvLine("summary,tilerenderer,%s,mean_%s_us,%.2f", variant, kPhaseName[ph], sum / BENCH_FRAMES);
		csvLine("summary,tilerenderer,%s,p99_%s_us,%.2f", variant, kPhaseName[ph], (double)tmp[(BENCH_FRAMES * 99) / 100]);
	}
	double cpu = 0;   /* CPU cost of recording one frame: build + record + frameend */
	for (int i = 0; i < BENCH_FRAMES; i++) cpu += gSamp[PH_BUILD][i] + gSamp[PH_RECORD][i] + gSamp[PH_END][i];
	csvLine("summary,tilerenderer,%s,mean_cpu_build_record_frameend_us,%.2f", variant, cpu / BENCH_FRAMES);
	csvLine("summary,tilerenderer,%s,missed_frames_over_%dms,%d", variant, (int)MISSED_FRAME_MS, missed);
	csvLine("summary,tilerenderer,%s,vblanks_elapsed,%lu", variant, (unsigned long)vblanks);
	csvLine("summary,tilerenderer,%s,rebuilds,%d", variant, rebuilds);
	csvLine("summary,tilerenderer,%s,dropped_objects,%d", variant, gDropped - droppedBase);
	csvLine("summary,tilerenderer,%s,cmdbuf_peak_pct,%.1f", variant, (double)cmdPeak * 100.0);
	csvLine("summary,tilerenderer,%s,resume_events,%d", variant, gResumeEvents);
	csvLine("summary,tilerenderer,%s,linear_free_end_bytes,%lu", variant, (unsigned long)linearSpaceFree());
	csvLine("summary,tilerenderer,%s,vram_free_end_bytes,%lu", variant, (unsigned long)vramSpaceFree());
	csvLine("note,done errors=%d", gCsvErrors);
}
#endif

int main(void)
{
	gfxInitDefault();
	C3D_Init(CMDBUF_SIZE);
	if (!C2D_Init(C2D_DEFAULT_MAX_OBJECTS)) { C3D_Fini(); gfxExit(); return 1; }
	C2D_Prepare();
	C3D_RenderTarget *top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
	C3D_RenderTarget *bot = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
#if TILE_STRATEGY == 3 || BENCH_AUTO
	aptHook(&gHook, onApt, NULL);
#endif
#if BENCH_AUTO
	csvBegin();
#endif

	/* ---- atlas ---- */
	static u32 pixels[ATLAS_W * ATLAS_H];
	for (int i = 0; i < 4; i++) paintTile(pixels, i, i, 0);
	for (int f = 0; f < 4; f++) paintTile(pixels, T_WATER0 + f, T_WATER0, f);
	paintSprite(pixels, S_PLAYER0, rgba(240, 90, 60, 255), 0, 0);
	paintSprite(pixels, S_PLAYER1, rgba(240, 90, 60, 255), 1, 0);
	paintSprite(pixels, S_NPC0, rgba(240, 220, 80, 255), 0, 0);
	paintSprite(pixels, S_NPC1, rgba(240, 220, 80, 255), 1, 0);
	paintSprite(pixels, S_SHADOW, 0, 0, 1);

	C3D_Tex atlas;
	if (!C3D_TexInit(&atlas, ATLAS_W, ATLAS_H, GPU_RGBA8)) { C2D_Fini(); C3D_Fini(); gfxExit(); return 1; }
	swizzleRGBA8((u32 *)atlas.data, pixels, ATLAS_W, ATLAS_H);
	C3D_TexFlush(&atlas);
	C3D_TexSetFilter(&atlas, GPU_NEAREST, GPU_NEAREST);
	C3D_TexSetWrap(&atlas, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);

	for (int i = 0; i < ATLAS_IMAGES; i++) { atlasSub(&gSub[i], i); gImg[i].tex = &atlas; gImg[i].subtex = &gSub[i]; gImg2[i] = gImg[i]; }
#if TEX_MODE
	if (!C3D_TexInit(&gAtlas2, ATLAS_W, ATLAS_H, GPU_RGBA8)) { C3D_TexDelete(&atlas); C2D_Fini(); C3D_Fini(); gfxExit(); return 1; }
	swizzleRGBA8((u32 *)gAtlas2.data, pixels, ATLAS_W, ATLAS_H);
	C3D_TexFlush(&gAtlas2);
	C3D_TexSetFilter(&gAtlas2, GPU_NEAREST, GPU_NEAREST);
	C3D_TexSetWrap(&gAtlas2, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
	for (int i = 0; i < ATLAS_IMAGES; i++) gImg2[i].tex = &gAtlas2;
#endif

	buildMap();

#if TILE_STRATEGY == 3
	if (!C3D_TexInitVRAM(&gCacheTex, 512, 512, GPU_RGBA8)) { C3D_TexDelete(&atlas); C2D_Fini(); C3D_Fini(); gfxExit(); return 1; }
	C3D_TexSetFilter(&gCacheTex, GPU_NEAREST, GPU_NEAREST);
	C3D_TexSetWrap(&gCacheTex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
	gCacheTarget = C3D_RenderTargetCreateFromTex(&gCacheTex, GPU_TEXFACE_2D, 0, GPU_RB_DEPTH16);
	if (!gCacheTarget) { C3D_TexDelete(&gCacheTex); C3D_TexDelete(&atlas); C2D_Fini(); C3D_Fini(); gfxExit(); return 1; }
	static Tex3DS_SubTexture cacheSub = { 512, 512, 0.0f, 1.0f, 1.0f, 0.0f };
	C2D_Image cacheImg = { &gCacheTex, &cacheSub };
#endif
#if TILE_STRATEGY == 1 || TILE_STRATEGY == 2
	{
		size_t retained = (TILE_STRATEGY == 2) ? (size_t)MAP_W * MAP_H * 6 : 0;
		gDvlb = DVLB_ParseFile((u32 *)vshader_shbin, vshader_shbin_size);
		gVbo = linearAlloc(sizeof(Vertex) * (retained + (size_t)DYN_TILES * 6));
		if (!gDvlb || !gVbo) { C3D_TexDelete(&atlas); C2D_Fini(); C3D_Fini(); gfxExit(); return 1; }
		gDyn = gVbo + retained;
		shaderProgramInit(&gProg);
		shaderProgramSetVsh(&gProg, &gDvlb->DVLE[0]);
		gUProj = shaderInstanceGetUniformLocation(gProg.vertexShader, "projection");
		gUOff = shaderInstanceGetUniformLocation(gProg.vertexShader, "offset");
		AttrInfo_Init(&gAttr);
		AttrInfo_AddLoader(&gAttr, 0, GPU_FLOAT, 3);   /* v0 position */
		AttrInfo_AddLoader(&gAttr, 1, GPU_FLOAT, 2);   /* v1 texcoord */
		BufInfo_Init(&gBuf);
		BufInfo_Add(&gBuf, gVbo, sizeof(Vertex), 2, 0x10);
		Mtx_OrthoTilt(&gProj, 0.0f, (float)TOP_W, (float)SCR_H, 0.0f, 0.0f, 1.0f, true);
	}
#endif
#if TILE_STRATEGY == 2
	u64 chunkT0 = svcGetSystemTick();
	for (int cy = 0; cy < CHUNKS_Y; cy++)
		for (int cx = 0; cx < CHUNKS_X; cx++) buildChunk(cx, cy);
	float chunkBuildAllMs = ticksToMs(svcGetSystemTick() - chunkT0);
#if BENCH_AUTO
	csvLine("meta,chunk_build_all_ms,%.3f", (double)chunkBuildAllMs);
#else
	(void)chunkBuildAllMs;
#endif
#endif

	gActor[0] = (Actor){ WORLD_W / 2.0f, WORLD_H / 2.0f, 0, 0, S_PLAYER0, 0 };
	for (int i = 1; i <= NPC_COUNT; i++)
		gActor[i] = (Actor){ (float)(rnd() % (WORLD_W - 32) + 16), (float)(rnd() % (WORLD_H - 32) + 16), 0, 0, S_NPC0, (int)(rnd() % 60) };

	/* ---- text: static strings parsed once; dynamic strings re-parsed only when their value changes ---- */
	C2D_TextBuf staticBuf = C2D_TextBufNew(128);
	C2D_TextBuf posBuf    = C2D_TextBufNew(64);
	C2D_TextBuf dbgBuf    = C2D_TextBufNew(512);
	C2D_Text title, posText, dbgText;
	C2D_TextParse(&title, staticBuf, "Tile renderer   Pad/D-pad: move   SELECT: debug   START: quit");
	C2D_TextOptimize(&title);
	int lastTX = -1, lastTY = -1;
	C2D_TextParse(&posText, posBuf, "");
	C2D_TextParse(&dbgText, dbgBuf, "");
#if BENCH_AUTO
	C2D_TextParse(&dbgText, dbgBuf, "bench running");
	C2D_TextOptimize(&dbgText);
#endif

	bool showDebug = true;
	u32 frameNo = 0;
	u64 prevLoopStart = svcGetSystemTick();
	/* debug accumulators (milliseconds, averaged over 30 frames) */
	float accPeriod = 0, accUpdate = 0, accRecord = 0, accEnd = 0;
	int visibleTiles = 0, rebuilds = 0;
#if BENCH_AUTO
	float cmdPeak = 0;
	u32 vblankStart = 0;
	int rebuildBase = 0, droppedBase = 0;
	bool benchDone = false;
#endif

	while (aptMainLoop()) {
		u64 loopStart = svcGetSystemTick();
		float periodMs = ticksToMs(loopStart - prevLoopStart);
		accPeriod += periodMs;
		prevLoopStart = loopStart;

		hidScanInput();
		u32 down = hidKeysDown(), held = hidKeysHeld();
		if (down & KEY_START) break;
		if (down & KEY_SELECT) showDebug = !showDebug;

		/* ---- update ---- */
		circlePosition pad;
		hidCircleRead(&pad);
		float ix = (abs(pad.dx) > 15) ? pad.dx / 156.0f : 0.0f, iy = (abs(pad.dy) > 15) ? -pad.dy / 156.0f : 0.0f;
		if (held & KEY_DLEFT)  ix = -1.0f;
		if (held & KEY_DRIGHT) ix = 1.0f;
		if (held & KEY_DUP)    iy = -1.0f;
		if (held & KEY_DDOWN)  iy = 1.0f;
#if BENCH_AUTO
		{   /* scripted path: right, down, left, up; legs of BENCH_LEG frames */
			int leg = (int)((frameNo / BENCH_LEG) % 4);
			ix = (leg == 0) ? 1.0f : (leg == 2) ? -1.0f : 0.0f;
			iy = (leg == 1) ? 1.0f : (leg == 3) ? -1.0f : 0.0f;
#if BENCH_PATH == 1
			ix = iy = 0.0f;
#endif
		}
#endif
		Actor *pl = &gActor[0];
		pl->x = clampf(pl->x + ix * CAM_SPEED, 8, WORLD_W - 8);
		pl->y = clampf(pl->y + iy * CAM_SPEED, 8, WORLD_H - 8);
		pl->image = (((frameNo / 8) & 1) && (ix != 0 || iy != 0)) ? S_PLAYER1 : S_PLAYER0;
		for (int i = 1; i <= NPC_COUNT; i++) {
			Actor *a = &gActor[i];
			if (--a->timer <= 0) { a->timer = 30 + (int)(rnd() % 60); a->vx = ((int)(rnd() % 3) - 1) * 0.6f; a->vy = ((int)(rnd() % 3) - 1) * 0.6f; }
			a->x = clampf(a->x + a->vx, 8, WORLD_W - 8);
			a->y = clampf(a->y + a->vy, 8, WORLD_H - 8);
			a->image = ((frameNo / 10) & 1) ? S_NPC1 : S_NPC0;
		}

		/* camera: follow the player, clamp to the world, then snap to whole pixels */
		float camXf = clampf(pl->x - TOP_W / 2, 0, WORLD_W - TOP_W);
		float camYf = clampf(pl->y - SCR_H / 2, 0, WORLD_H - SCR_H);
		int camX = (int)floorf(camXf), camY = (int)floorf(camYf);

		/* depth order of the sprite layer: sort by feet position, back (small y) to front */
		int order[NPC_COUNT + 1];
		for (int i = 0; i <= NPC_COUNT; i++) order[i] = i;
		for (int i = 1; i <= NPC_COUNT; i++) {
			int k = order[i], j = i - 1;
			while (j >= 0 && gActor[order[j]].y > gActor[k].y) { order[j + 1] = order[j]; j--; }
			order[j + 1] = k;
		}

		int tx0 = camX / TILE, ty0 = camY / TILE;
		int tx1 = (camX + TOP_W - 1) / TILE, ty1 = (camY + SCR_H - 1) / TILE;
		visibleTiles = (tx1 - tx0 + 1) * (ty1 - ty0 + 1);
		int waterFrame = T_WATER0 + (int)((frameNo / 12) & 3);

#if !BENCH_AUTO
		/* text caches: only when the value changed */
		if (tx0 != lastTX || ty0 != lastTY) {
			char s[40];
			snprintf(s, sizeof s, "tile %d,%d", (int)(pl->x / TILE), (int)(pl->y / TILE));
			C2D_TextBufClear(posBuf);
			C2D_TextParse(&posText, posBuf, s);
			C2D_TextOptimize(&posText);
			lastTX = tx0; lastTY = ty0;
		}
		if (frameNo % 30 == 29) {
			char s[200];
			snprintf(s, sizeof s,
				"period %.1f ms (vsync-locked)\nupdate %.2f ms\nrecord %.2f ms\nFrameEnd %.2f ms\n"
				"C3D proc %.2f ms  draw %.2f ms\ntiles %d  rebuilds %d",
				accPeriod / 30, accUpdate / 30, accRecord / 30, accEnd / 30,
				C3D_GetProcessingTime(), C3D_GetDrawingTime(), visibleTiles, rebuilds);
			C2D_TextBufClear(dbgBuf);
			C2D_TextParse(&dbgText, dbgBuf, s);
			C2D_TextOptimize(&dbgText);
			accPeriod = accUpdate = accRecord = accEnd = 0;
		}
#else
		(void)lastTX; (void)lastTY; (void)posText; (void)showDebug;
		(void)accPeriod; (void)accUpdate; (void)accRecord; (void)accEnd;
#endif
		u64 updateEnd = svcGetSystemTick();
		float updateMs = ticksToMs(updateEnd - loopStart);
		accUpdate += updateMs;

		/* ---- draw ---- */
#if BENCH_AUTO
		if (frameNo == BENCH_WARMUP) { vblankStart = C3D_FrameCounter(0); rebuildBase = rebuilds; droppedBase = gDropped; }
#endif
		u64 beginStart = svcGetSystemTick();
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		u64 recStart = svcGetSystemTick();
		u64 buildTicks = 0;                        /* time spent generating geometry or redrawing the cache: the "build" phase */

#if MAP_EDIT_PERIOD > 0
		if (frameNo % MAP_EDIT_PERIOD == 0) {      /* the previous frame's GPU work is finished here, so the CPU may rewrite geometry */
			u64 b0 = svcGetSystemTick();
			int ex = tx0 + 4, ey = ty0 + 4;
			gMap[ey][ex] = (gMap[ey][ex] == T_STONE) ? T_GRASS : T_STONE;
#if TILE_STRATEGY == 2
			buildChunk(ex / CHUNK_TILES, ey / CHUNK_TILES);
#elif TILE_STRATEGY == 3
			gCacheValid = false;
#endif
			buildTicks += svcGetSystemTick() - b0;
#if TILE_STRATEGY == 2
			rebuilds++;
#endif
		}
#endif

#if TILE_STRATEGY == 3
		if (!gCacheValid || tx0 < gCacheTX || tx1 >= gCacheTX + CACHE_TILES || ty0 < gCacheTY || ty1 >= gCacheTY + CACHE_TILES) {
			u64 b0 = svcGetSystemTick();
			gCacheTX = (int)clampf((float)(tx0 - (CACHE_TILES - (tx1 - tx0 + 1)) / 2), 0, MAP_W - CACHE_TILES);
			gCacheTY = (int)clampf((float)(ty0 - (CACHE_TILES - (ty1 - ty0 + 1)) / 2), 0, MAP_H - CACHE_TILES);
			C3D_RenderTargetClear(gCacheTarget, C3D_CLEAR_ALL, 0x000000FF, 0);
			C2D_SceneBegin(gCacheTarget);
			for (int ty = 0; ty < CACHE_TILES; ty++)
				for (int tx = 0; tx < CACHE_TILES; tx++)
					if (!C2D_DrawImageAt(gImg[gMap[gCacheTY + ty][gCacheTX + tx]], (float)(tx * TILE), (float)(ty * TILE), Z_ALL, NULL, TSCALE, TSCALE)) gDropped++;
			gCacheValid = true;
			rebuilds++;
			buildTicks += svcGetSystemTick() - b0;
		}
#endif
		C2D_TargetClear(top, C2D_Color32(0, 0, 0, 255));
		C2D_SceneBegin(top);

#if TILE_STRATEGY == 3
		{
			float cx = (float)(gCacheTX * TILE - camX), cy = (float)(gCacheTY * TILE - camY);
#if CACHE_FLIP_V
			C2D_DrawParams p = { { cx, cy + 512.0f, 512.0f, -512.0f }, { 0, 0 }, Z_ALL, 0.0f };
			C2D_DrawImage(cacheImg, &p, NULL);
#else
			C2D_DrawImageAt(cacheImg, cx, cy, Z_ALL, NULL, 1.0f, 1.0f);
#endif
			/* animated tiles cannot live in the cache: overlay only the visible water tiles */
			for (int ty = ty0; ty <= ty1; ty++)
				for (int tx = tx0; tx <= tx1; tx++)
					if (gMap[ty][tx] == T_WATER0)
						if (!C2D_DrawImageAt(gImg[waterFrame], (float)(tx * TILE - camX), (float)(ty * TILE - camY), Z_ALL, NULL, TSCALE, TSCALE)) gDropped++;
		}
#elif TILE_STRATEGY == 1
		{   /* manual: rebuild one vertex buffer of screen-space quads every frame, one draw call */
			u64 b0 = svcGetSystemTick();
			Vertex *p = gDyn;
			for (int ty = ty0; ty <= ty1; ty++)
				for (int tx = tx0; tx <= tx1; tx++) {
					int id = gMap[ty][tx];
					if (id == T_WATER0) id = waterFrame;
					p = emitQuad(p, (float)(tx * TILE - camX), (float)(ty * TILE - camY), id);
				}
			buildTicks += svcGetSystemTick() - b0;
			manualBegin(&atlas, 0.0f, 0.0f);
			C3D_DrawArrays(GPU_TRIANGLES, (int)(gDyn - gVbo), (int)(p - gDyn));
			manualEnd(top);
		}
#elif TILE_STRATEGY == 2
		{   /* chunks: retained geometry per chunk, offset by the camera in the shader; water rebuilt every frame */
			u64 b0 = svcGetSystemTick();
			Vertex *p = gDyn;
			for (int ty = ty0; ty <= ty1; ty++)
				for (int tx = tx0; tx <= tx1; tx++)
					if (gMap[ty][tx] == T_WATER0) p = emitQuad(p, (float)(tx * TILE), (float)(ty * TILE), waterFrame);
			buildTicks += svcGetSystemTick() - b0;
			manualBegin(&atlas, (float)-camX, (float)-camY);
			for (int cy = ty0 / CHUNK_TILES; cy <= ty1 / CHUNK_TILES; cy++)
				for (int cx = tx0 / CHUNK_TILES; cx <= tx1 / CHUNK_TILES; cx++)
					if (gChunkCount[cy][cx] > 0)
						C3D_DrawArrays(GPU_TRIANGLES, (cy * CHUNKS_X + cx) * CHUNK_VERTS, gChunkCount[cy][cx]);
			if (p > gDyn) C3D_DrawArrays(GPU_TRIANGLES, (int)(gDyn - gVbo), (int)(p - gDyn));
			manualEnd(top);
		}
#else
#if TEX_MODE == 2
		for (int pass = 0; pass < 2; pass++)
#endif
		for (int ty = ty0; ty <= ty1; ty++)
			for (int tx = tx0; tx <= tx1; tx++) {
				int id = gMap[ty][tx];
				if (id == T_WATER0) id = waterFrame;
#if TEX_MODE == 1
				const C2D_Image *src = ((tx + ty) & 1) ? gImg2 : gImg;      /* alternate textures tile by tile */
#elif TEX_MODE == 2
				if (((tx + ty) & 1) != pass) continue;
				const C2D_Image *src = pass ? gImg2 : gImg;                 /* one run per texture */
#else
				const C2D_Image *src = gImg;
#endif
				if (!C2D_DrawImageAt(src[id], (float)(tx * TILE - camX), (float)(ty * TILE - camY), Z_ALL, NULL, TSCALE, TSCALE)) gDropped++;
			}
#endif
		/* sprites in sorted order; shadow first so the body overlaps it; all share the atlas, so no batch break */
		for (int n = 0; n <= NPC_COUNT; n++) {
			const Actor *a = &gActor[order[n]];
			float sx = floorf(a->x) - camX - 8, sy = floorf(a->y) - camY - 12;
			if (sx < -CELL || sx > TOP_W || sy < -CELL || sy > SCR_H) continue;   /* cull off-screen sprites */
			C2D_DrawImageAt(gImg[S_SHADOW], sx, sy + 4, Z_ALL, NULL, 1.0f, 1.0f);
			C2D_DrawImageAt(gImg[a->image], sx, sy, Z_ALL, NULL, 1.0f, 1.0f);
		}

		/* HUD: rectangles and text break the batch, so they come last */
		C2D_DrawRectSolid(0, 0, Z_ALL, TOP_W, 14, C2D_Color32(0, 0, 0, 160));
		C2D_DrawText(&title, C2D_WithColor, 4, 1, Z_ALL, 0.4f, 0.4f, C2D_Color32(255, 255, 255, 255));
#if !BENCH_AUTO
		C2D_DrawText(&posText, C2D_WithColor, 4, 16, Z_ALL, 0.5f, 0.5f, C2D_Color32(255, 255, 160, 255));
#endif

		if (frameNo % BOTTOM_PERIOD == 0) {   /* a screen whose target is not drawn keeps its last image and is not transferred */
			C2D_TargetClear(bot, C2D_Color32(20, 20, 28, 255));
			C2D_SceneBegin(bot);
			/* world overview: the whole world scaled to 128x96 */
			C2D_DrawRectSolid(10, 10, Z_ALL, MAP_W, MAP_H, C2D_Color32(40, 90, 60, 255));
			C2D_DrawRectangle(10 + camX * (float)MAP_W / WORLD_W, 10 + camY * (float)MAP_H / WORLD_H, Z_ALL, TOP_W * (float)MAP_W / WORLD_W, SCR_H * (float)MAP_H / WORLD_H,
				C2D_Color32(255, 255, 255, 120), C2D_Color32(255, 255, 255, 120), C2D_Color32(255, 255, 255, 60), C2D_Color32(255, 255, 255, 60));
			C2D_DrawCircleSolid(10 + pl->x * (float)MAP_W / WORLD_W, 10 + pl->y * (float)MAP_H / WORLD_H, Z_ALL, 2.0f, C2D_Color32(240, 90, 60, 255));
#if SHAPE_MODE == 1
			for (int i = 0; i < SHAPE_PAIRS; i++) {     /* rectangle, circle, rectangle, circle ... */
				C2D_DrawRectSolid(150 + (i % 16) * 10, 10 + (i / 16) * 20, Z_ALL, 8, 8, C2D_Color32(200, 80, 80, 255));
				C2D_DrawCircleSolid(154 + (i % 16) * 10, 24 + (i / 16) * 20, Z_ALL, 4, C2D_Color32(80, 80, 200, 255));
			}
#elif SHAPE_MODE == 2
			for (int i = 0; i < SHAPE_PAIRS; i++)       /* all rectangles, then all circles */
				C2D_DrawRectSolid(150 + (i % 16) * 10, 10 + (i / 16) * 20, Z_ALL, 8, 8, C2D_Color32(200, 80, 80, 255));
			for (int i = 0; i < SHAPE_PAIRS; i++)
				C2D_DrawCircleSolid(154 + (i % 16) * 10, 24 + (i / 16) * 20, Z_ALL, 4, C2D_Color32(80, 80, 200, 255));
#endif
			if (showDebug)
				C2D_DrawText(&dbgText, C2D_WithColor, 10, 116, Z_ALL, 0.5f, 0.5f, C2D_Color32(190, 255, 190, 255));
		}

		u64 endStart = svcGetSystemTick();
#if BENCH_AUTO
		float cmdUse = C3D_GetCmdBufUsage();
#endif
		C3D_FrameEnd(0);
		u64 endStop = svcGetSystemTick();
		float recordMs = ticksToMs(endStart - recStart - buildTicks);
		float endMs = ticksToMs(endStop - endStart);
		accRecord += recordMs;
		accEnd += endMs;

#if BENCH_AUTO
		if (!benchDone) {
			if (frameNo >= BENCH_WARMUP && frameNo < BENCH_WARMUP + BENCH_FRAMES) {
				int i = (int)(frameNo - BENCH_WARMUP);
				gSamp[PH_PERIOD][i] = periodMs * 1000.0f;
				gSamp[PH_UPDATE][i] = updateMs * 1000.0f;
				gSamp[PH_BUILD][i] = ticksToMs(buildTicks) * 1000.0f;
				gSamp[PH_RECORD][i] = recordMs * 1000.0f;
				gSamp[PH_BEGIN][i] = ticksToMs(recStart - beginStart) * 1000.0f;
				gSamp[PH_END][i] = endMs * 1000.0f;
				gSamp[PH_PROC][i] = C3D_GetProcessingTime() * 1000.0f;   /* these two describe the previous finished frame */
				gSamp[PH_GPU][i] = C3D_GetDrawingTime() * 1000.0f;
				if (cmdUse > cmdPeak) cmdPeak = cmdUse;
			}
			if (frameNo == BENCH_WARMUP + BENCH_FRAMES - 1) {
				csvResults(visibleTiles, rebuilds - rebuildBase, droppedBase, cmdPeak, C3D_FrameCounter(0) - vblankStart);
				if (gCsv) { fclose(gCsv); gCsv = NULL; }
				C2D_TextBufClear(dbgBuf);
				C2D_TextParse(&dbgText, dbgBuf, gCsvErrors ? "bench done WITH ERRORS - no result" : "bench done - results on SD card. START quits");
				C2D_TextOptimize(&dbgText);
				benchDone = true;
			}
		}
#else
		(void)beginStart;
#endif
		frameNo++;
	}

#if TILE_STRATEGY == 3 || BENCH_AUTO
	aptUnhook(&gHook);
#endif
#if TILE_STRATEGY == 3
	C3D_RenderTargetDelete(gCacheTarget);
	C3D_TexDelete(&gCacheTex);
#endif
#if TILE_STRATEGY == 1 || TILE_STRATEGY == 2
	linearFree(gVbo);
	shaderProgramFree(&gProg);
	DVLB_Free(gDvlb);
#endif
#if TEX_MODE
	C3D_TexDelete(&gAtlas2);
#endif
	C2D_TextBufDelete(dbgBuf);
	C2D_TextBufDelete(posBuf);
	C2D_TextBufDelete(staticBuf);
	C3D_TexDelete(&atlas);
	C2D_Fini();
	C3D_Fini();
	gfxExit();
	return 0;
}
```

### The shader for strategies 1 and 2

`source/vshader.v.pica` (the same program [drawbench](../benchmarks/README.md#drawbench) uses: position plus an offset uniform through a 4×4 projection, texture coordinate passed through, white vertex colour). Strategy 1 sets the offset to 0 and bakes the camera into the vertices; strategy 2 keeps world-space vertices and sets the offset to `(-camX, -camY)`.

```text
.fvec projection[4]
.fvec offset
.constf ones(1.0, 1.0, 1.0, 1.0)
.alias  one ones.x
.out outpos position
.out outtc0 texcoord0
.out outclr color
.alias inpos v0
.alias intc  v1
.proc main
	add r0.xyz, offset.xyz, inpos.xyz   ; a uniform may only be the first source here
	mov r0.w,   one
	dp4 outpos.x, projection[0], r0
	dp4 outpos.y, projection[1], r0
	dp4 outpos.z, projection[2], r0
	dp4 outpos.w, projection[3], r0
	mov outtc0, intc
	mov outclr, ones
	end
.end
```

## Strategies and compile-time modes

Every mode is a `-D` macro whose default reproduces the original program. Combinations that make no sense stop the build with `#error`.

| Macro (default) | Values | What it changes |
| --------------- | ------ | --------------- |
| `TILE_STRATEGY` (0; `USE_TILE_CACHE=1` still means 3) | 0 direct, 1 manual, 2 chunks, 3 cache | How tiles reach the GPU, below |
| `TILE` (16) | 8 to 32 | Drawn tile size in pixels; the 16×16 atlas art is scaled with nearest filtering. Changes the visible tile count without changing the code path (strategy 3: 16 or 32 only) |
| `CHUNK_TILES` (16) | divides 128 and 96 | Strategy 2 chunk edge in tiles |
| `TEX_MODE` (0) | 0 one atlas, 1 two identical atlases alternating per tile, 2 the same two atlases drawn as two runs | Strategy 0 only: how many texture changes the tile pass causes |
| `MAP_EDIT_PERIOD` (0) | N frames | Every N frames one visible tile changes: strategy 2 rebuilds its chunk, strategy 3 redraws the whole cache, strategies 0 and 1 read the new value for free |
| `SHAPE_MODE` (0), `SHAPE_PAIRS` (32) | 0 off, 1 rectangle and circle interleaved, 2 all rectangles then all circles | Extra shapes on the bottom screen, for [E6](#e6-shape-batching) |
| `BOTTOM_PERIOD` (1) | N | The bottom screen is drawn every N frames; in between its target is untouched |
| `CMDBUF_SIZE` (`C3D_DEFAULT_CMDBUF_SIZE`) | bytes | Command-buffer size; raise it when thousands of separate draw calls are expected |
| `NPC_COUNT` (40), `CAM_SPEED` (2.0f) | | Sprite count and player speed in pixels per frame |
| `BENCH_AUTO` (0), `BENCH_PATH` (0), `BENCH_LEG` (120), `BENCH_WARMUP` (120), `BENCH_FRAMES` (600) | | Scripted run: the player walks right, down, left, up in legs of `BENCH_LEG` frames (path 0) or stands still (path 1); `BENCH_WARMUP` frames are discarded, the next `BENCH_FRAMES` are recorded, the CSV is written, and the program keeps running until START |
| `CACHE_FLIP_V` (0) | 0, 1 | Strategy 3: draw the cached quad with a negative height if it comes out upside down |

The four strategies, by what the CPU does per frame (arithmetic and mechanism, not time):

| Strategy | Per frame | Draw calls from tiles | Memory |
| -------- | --------- | --------------------- | ------ |
| 0 direct | One `C2D_DrawImageAt` per visible tile: citro2d fills 4 vertices (128 bytes) and one of its 4,096 object slots per call ([object limit](rendering-2d.md#the-object-limit-and-how-batching-works)) | One batch while the texture stays the same | None extra |
| 1 manual | Write 6 vertices (20 bytes each: position 3 floats, texture coordinate 2 floats) per visible tile into a linear buffer | 1 | `DYN_TILES` × 6 × 20 bytes, about 62 KiB at 16 px |
| 2 chunks | Nothing for static tiles; visible water tiles are written to the dynamic buffer; one `C3D_DrawArrays` per visible chunk, plus one for water | 1 per visible chunk (at most 6 at 16-tile chunks) + 1 | 12,288 tiles × 6 × 20 = 1,474,560 bytes of linear memory whatever the chunk size, because a chunk reserves room for every tile |
| 3 cache | One quad of the 512×512 cache texture, plus the visible water tiles; a full redraw of every tile in the window (1,024 at 16 px) when the view leaves it | 1 + water | 1,048,576 bytes colour + 524,288 bytes depth in VRAM |

Visible tile counts for the five suggested `TILE` values at the worst-case camera offset (arithmetic: columns ⌊(399+T−1)/T⌋+1, rows ⌊(239+T−1)/T⌋+1): 32 px gives 126, 24 gives 198, 16 gives 416, 12 gives 735, 8 gives 1,581. The result files record the count of the last measured frame, which may be lower.

Chunk over-submission, by the same kind of arithmetic: a 400×240 window touches at most ⌊399/C⌋+2 chunk columns and ⌊239/C⌋+2 rows of C pixels. At 8-tile chunks (128 px at 16 px tiles) that is 5×3 = 15 chunks and 5,760 vertices; at 16-tile chunks 3×2 = 6 and 9,216; at 32-tile chunks 2×2 = 4 and 24,576. The visible tiles alone are 416 × 6 = 2,496 vertices. Whether the GPU's cost for the extra vertices matters is `[hypothesis]`; [E4](#e4-chunk-size-and-rebuild-on-change) measures it.

## How it works

### Textures made in code, camera, visible tiles

`paintTile`, `paintSprite`, `swizzleRGBA8` and `atlasSub` are explained, and the layout checked against `tex3ds`, on [2D rendering](rendering-2d.md#generating-a-texture-in-code). The atlas lives in linear memory; `C3D_TexFlush` writes the cache back before the GPU reads it. The camera centres on the player, is clamped to the world (`WORLD_W - TOP_W` is the largest left edge) and converted to whole pixels; everything drawn is `world position − camX/camY`, so citro2d's view matrix is never touched and never breaks a batch.

```c
int tx0 = camX / TILE, ty0 = camY / TILE;
int tx1 = (camX + TOP_W - 1) / TILE, ty1 = (camY + SCR_H - 1) / TILE;
```

These are the first and last tile column and row touched by the window: 25×15 = 375 tiles when the camera is aligned to a 16-pixel tile, at most 26×16 = 416 otherwise (arithmetic). The map is small enough to index directly; a map larger than memory needs streamed chunks, which this program does not do.

### Tile animation, sprites, text

Tile id `T_WATER0` means "water"; at draw time it becomes `T_WATER0 + ((frameNo / 12) & 3)`, so four atlas frames cycle five times a second at 60 frames per second. Sprites are sorted by `y` (insertion sort: nearly sorted from frame to frame) and drawn back to front with one shared `Z_ALL`, because citro2d's depth test passes equal depths and transparent texels still write depth, so a z-per-sprite scheme would cut holes in farther sprites ([depth rules](rendering-2d.md#2-screens-coordinates-and-depth)); the shadow is drawn before the body, and off-screen sprites are skipped before the draw call. `C2D_TextParse` lays out glyphs on the CPU, so unchanged text is not re-parsed: the title is parsed once, the position text is rebuilt only when the visible tile range changes, and the debug text every 30 frames, each in its own buffer because `C2D_TextBufClear` invalidates every text parsed into that buffer ([text](rendering-2d.md#text)).

### Strategies 1 and 2: handing the GPU state over

The manual passes use citro3d directly while citro2d owns the rest of the frame. `manualBegin` calls `C2D_Flush()` (so nothing batched earlier is lost), binds the program, attribute layout, vertex buffer, the atlas and a replace-texture combiner, turns the depth test off (so citro2d's later sprites pass) and sets the projection and offset uniforms. `manualEnd` restores back-face culling, calls `C2D_Prepare()` (the documented way to re-establish citro2d's state after changing citro3d's) and `C2D_SceneBegin(top)` (which re-applies citro2d's projection). All of that is part of the strategy's cost and is counted in `record`. Whether citro2d's internal state (for example which texture it believes is bound) survives this hand-over is `[hypothesis]`: citro2d's `source/base.c` is not cloned here. The CPU writes the vertex buffer after `C3D_FrameBegin(C3D_FRAME_SYNCDRAW)` returned, which has waited for the previous frame's GPU work ([frame functions](rendering-2d.md#the-frame-loop)), so it never overwrites data the GPU is reading; the whole-heap flush in `C3D_FrameEnd(0)` makes the writes visible to the GPU ([FrameEnd flush](memory-and-cache.md#the-c3d_frameend-flush)).

### Strategy 3: render-to-texture cache

1. A 512×512 RGBA8 texture is created in VRAM with `C3D_TexInitVRAM` and turned into a render target with `C3D_RenderTargetCreateFromTex(..., GPU_RB_DEPTH16)`: 1 MiB colour plus 512 KiB depth, beside the screen targets' about 1 MiB (arithmetic; [VRAM table](rendering-2d.md#vram-or-linear-memory)).
2. The cache covers a window of `512/TILE` tiles square. When the visible tile range is no longer inside it, the window is re-centred on the camera and every tile in it is drawn into the texture, with water drawn as its first frame.
3. Each frame the screen gets one quad of the cache at `(window origin − camera)`, then the visible water tiles over it, then sprites and HUD.

By counting quads (arithmetic): the direct path submits up to 416 tile quads per frame; the cached path submits 1 plus the visible water tiles, and a redraw frame submits 1,024 more. With 3 tiles of slack on each side at 16 px, 2 pixels per frame forces a redraw about every 24 frames. Whether it wins in time is [E2](#e2-cached-background-versus-direct-tiles). Anything animated or sprite-like must stay out of the cache. A 1,024×1,024 cache would need 4 MiB colour + 2 MiB depth, all of the 6 MiB VRAM, so it does not fit beside the screen targets (arithmetic). Switching render target mid-frame has a cost ([draw-call costs](gpu-rendering.md#what-a-draw-call-costs)), so the redraw happens before the screen pass. The cached quad is wider than the screen (512 against 400) and the hidden part is clipped; its fill cost has not been measured. After a HOME suspend or sleep the cache is marked invalid by an APT hook, because whether VRAM contents survive is [not established](application-lifecycle.md#open-questions).

## Per-phase timing

Every frame is split into phases with `svcGetSystemTick()` (268,111,856 ticks per second, [system tick](../../glossary.md#system-tick)):

| Phase (CSV `test`) | Measured from to | Does not include |
| ------------------ | ---------------- | ---------------- |
| `period` | Start of one loop iteration to the start of the next | Nothing; with `C3D_FRAME_SYNCDRAW` it is quantised to display refreshes, so it shows missed frames, not headroom |
| `update` | Loop start to just before `C3D_FrameBegin`: input, logic, camera, sort, text caching | Drawing |
| `framebegin` | The `C3D_FrameBegin` call | CPU work: it is mostly a wait for vblank and the previous frame's GPU work, a stall indicator |
| `build` | Geometry generation or cache redraw: strategy 1's vertex writing, strategy 2's water vertices and chunk rebuilds, strategy 3's cache redraw, the map edit | The draw calls that consume it |
| `record` | `C3D_FrameBegin` returning to just before `C3D_FrameEnd`, minus `build`: the `C2D_Draw*`, `C3D_DrawArrays` and state hand-over | `C3D_FrameBegin`'s wait, `C3D_FrameEnd`, GPU work |
| `frameend` | Wall clock around `C3D_FrameEnd(0)` | GPU execution, which continues after it returns |
| `c3d_proc` | `C3D_GetProcessingTime()` of the previous finished frame | The linear-heap cache flush inside `C3D_FrameEnd`, which citro3d does after this timer stops |
| `c3d_gpu` | `C3D_GetDrawingTime()` of the previous finished frame: GPU drawing plus display transfer | CPU work |

**Compare strategies with `build + record + frameend`** (the summary line `mean_cpu_build_record_frameend_us`), never `c3d_proc` alone, because `C3D_FrameEnd(0)` flushes the whole linear heap and `C3D_GetProcessingTime` stops before that ([citro3d's two timers](performance.md#citro3ds-two-timers), [drawbench](../benchmarks/README.md#drawbench)). The interactive overlay (absent in `BENCH_AUTO` builds) shows the same CPU numbers averaged over the previous 30 frames, because drawing the text is itself part of the frame being measured.

## Experiments

Nothing here has been run. Each experiment names its variables, metrics, build lines and the rule that turns numbers into a decision. Only the listed macros vary; everything else stays at its default. The table that places these experiments among all the design choices is [Design alternatives and how to choose](rendering-2d.md#design-alternatives-and-how-to-choose).

**Protocol for all of them.** Old 3DS first (the primary target), then a New 3DS as a separate data set. Fresh boot, close other homebrew, detach the debugger, no game plugins ([benchmark README](../benchmarks/README.md#before-you-start)). Build with `BENCH_AUTO=1`, run from the Homebrew Menu, wait for "bench done", press START, copy `sdmc:/coffeepot-tilerenderer.csv` (opened for append: delete it between sessions, or keep it; each run starts with its own `meta` block). At least three runs per variant. **Discard** a run that has any `error,` line, `dropped_objects` above 0, or `cmdbuf_peak_pct` of 98 or more. Per run use the median of each phase; report the spread across runs. Emulator output is never a result.

Why not decide on `period`: the loop is locked to the display, so any variant whose CPU and GPU work fit in 16.7 ms shows the same `period`. The crossover questions are about cost, so they are decided on `mean_cpu_build_record_frameend_us` and `c3d_gpu`; `period` and the missed-frame count only say whether a variant fits the budget at all.

### E1: manual vertex buffer versus `C2D_DrawImageAt`

*Question:* at what visible-tile count does one manually built vertex buffer and one `C3D_DrawArrays` beat one `C2D_DrawImageAt` per tile?

| | |
| --- | --- |
| Variables | `TILE_STRATEGY` in {0, 1}; `TILE` in {32, 24, 16, 12, 8} (126 to 1,581 visible tiles); `NPC_COUNT` in {0, 40} (0 isolates the tile pass) |
| Fixed | `BENCH_PATH=0`, `CAM_SPEED=2`, `TEX_MODE=0`, `MAP_EDIT_PERIOD=0` |
| Build line | `-DBENCH_AUTO=1 -DTILE_STRATEGY={0,1} -DTILE={32,24,16,12,8} -DNPC_COUNT={0,40}` (20 variants, 3 runs each) |
| Metrics | `mean_cpu_build_record_frameend_us`; medians of `build`, `record`, `frameend`, `c3d_gpu`; missed frames |
| Decision | Fit a line (cost against visible tiles) to each strategy over the five points. The crossover N\* is the smallest tile count at which strategy 1 is cheaper than strategy 0 by more than the spread across runs and stays cheaper for every larger count. If the lines do not cross within 126 to 1,581 say so; do not extrapolate |
| Confounds, stated | Strategy 1 pays `C2D_Flush` + `C2D_Prepare` + `C2D_SceneBegin` once per frame (inside `record`); it draws with the depth test off, so depth-write cost differs; its vertex is 20 bytes against citro2d's 32 (inferred from 128 bytes per quad). The experiment compares end-to-end submission paths, not the library call alone |

### E2: cached background versus direct tiles

*Question:* when does the render-target cache pay for its VRAM and its redraw spikes?

| | |
| --- | --- |
| Variables | `TILE_STRATEGY` in {0, 3}; camera speed: `BENCH_PATH=1` (still), `CAM_SPEED` in {1, 2, 4, 8} (redraw interval about 48 ÷ speed frames, arithmetic); `MAP_EDIT_PERIOD` in {0, 30, 5}; `TILE` in {16, 32} |
| Fixed | `NPC_COUNT=0` for the primary table, 40 for a second pass |
| Build line | `-DBENCH_AUTO=1 -DTILE_STRATEGY={0,3} [-DBENCH_PATH=1 or -DCAM_SPEED={1,2,4,8}] -DMAP_EDIT_PERIOD={0,30,5}` |
| Metrics | `mean_`, `p99_` and max of `build`; `mean_cpu_build_record_frameend_us`; `c3d_gpu` median and max; `rebuilds`; missed frames; VRAM cost as `meta,vram_free_bytes` of the strategy-0 build minus the strategy-3 build (arithmetic expectation 1,572,864 bytes plus allocator rounding) |
| Decision | The cache pays at a camera speed and edit rate if its mean cost is lower **and** its p99 and missed-frame count are no worse than strategy 0's. Report the largest speed and highest edit rate at which that holds, beside the VRAM bytes: the answer is "a win for x bytes of VRAM" |
| Also read | The cached quad streams 400×240×4 = 384,000 bytes of texture per frame, against a 16 KiB hot atlas for strategy 0 (arithmetic); if `c3d_gpu` rises in strategy 3 on a still camera, that is the bandwidth effect `[hypothesis]` |

### E3: alternating textures

*Question:* what does each texture change cost inside a citro2d batch?

| | |
| --- | --- |
| Variables | `TEX_MODE` in {0, 1, 2}; `TILE` in {32, 16, 12} |
| Fixed | `TILE_STRATEGY=0`, `NPC_COUNT=0`, `CMDBUF_SIZE=0x100000` (a texture change per tile writes many commands; discard any run with `cmdbuf_peak_pct` of 98 or more) |
| Build line | `-DBENCH_AUTO=1 -DTEX_MODE={0,1,2} -DTILE={32,16,12} -DNPC_COUNT=0 -DCMDBUF_SIZE=0x100000` |
| Expected structure | Mode 0: one run of tiles; mode 1: a texture change at almost every tile (about N runs); mode 2: two runs. `[hypothesis]` that each change ends a draw call (mechanism `[documented upstream, not re-verified locally]`) |
| Metrics | `record`, `frameend`, `c3d_gpu` medians; `cmdbuf_peak_pct` |
| Decision | Per-switch cost ≈ (mode 1 − mode 2) ÷ (N − 2), for CPU (`build + record + frameend`) and GPU (`c3d_gpu`) separately, per `TILE`. Mode 2 against mode 0 isolates the cost of merely having two textures of equal content. Compare with [drawbench](../benchmarks/README.md#drawbench) `+texswap`, which measures the same thing one level lower (citro3d, 8×8 textures) |

### E4: chunk size and rebuild-on-change

*Question:* which chunk size balances over-submitted vertices against rebuild cost, and does retained geometry beat E1's manual path?

| | |
| --- | --- |
| Variables | `TILE_STRATEGY=2`, `CHUNK_TILES` in {8, 16, 32}; `MAP_EDIT_PERIOD` in {0, 30, 1}; compare with E1's strategy 1 at `TILE=16` |
| Build line | `-DBENCH_AUTO=1 -DTILE_STRATEGY=2 -DCHUNK_TILES={8,16,32} -DMAP_EDIT_PERIOD={0,30,1} -DNPC_COUNT=0` |
| Metrics | `build` mean, p99 and max (chunk rebuild and water vertices); `record`; `c3d_gpu`; `meta,chunk_build_all_ms` (start-up cost of building the whole world) |
| Decision | With no edits: the chunk size with the lowest `mean_cpu_build_record_frameend_us` and `c3d_gpu`. With an edit every frame: rebuild cost per edited chunk = `build` mean at `MAP_EDIT_PERIOD=1` minus at 0. Retained chunks beat the manual path only if their total is lower at the same `TILE` |

### E5: static bottom-screen content

*Question:* what does leaving the bottom screen untouched save?

| | |
| --- | --- |
| Variables | `BOTTOM_PERIOD` in {1, 2, 4, 30}, default strategy |
| Metrics | `record`, `frameend`, `c3d_gpu` means (a skipped frame omits the bottom clear, scene and transfer; the mean captures the average) |
| Decision | Saving per skipped frame = (mean at period 1 − mean at period N) × N ÷ (N − 1). Mechanism: a target that is not drawn is neither transferred nor swapped `[documented: renderqueue.c]`; the size of the saving is `[hypothesis]` |

### E6: shape batching

*Question:* is the citro2d note that circles need "an expensive state change" worth acting on?

The claim was checked against the local sources: `include/c2d/base.h` of the installed citro2d 1.7.0 says of every ellipse and circle function "Switching to and from "circle mode" internally requires an expensive state change. As such, the recommended usage of this feature is to draw all non-circular objects first, then draw all circular objects", and `3ds-examples/graphics/gpu/2d_shapes/source/main.c` repeats it in a comment ("They should either be drawn before all non-circular objects, or afterwards"). Both are accurate as quoted by the review. Neither says how expensive the change is or which state it touches, so the benefit is `[hypothesis]` until measured.

| | |
| --- | --- |
| Variables | `SHAPE_MODE` in {0, 1, 2}; `SHAPE_PAIRS` in {8, 32, 128} |
| Metrics | `record` mean and `c3d_gpu` |
| Decision | Mode 1 makes about 2 × pairs switches into and out of circle mode, mode 2 about 2. Cost per switch ≈ (mode 1 − mode 2) ÷ (2 × pairs − 2). If it is below the run-to-run noise, shape grouping does not matter for this workload |

## Output records

`BENCH_AUTO=1` writes the same kinds of lines as [`bench.h`](../benchmarks/README.md#output-format-and-metadata) to `sdmc:/coffeepot-tilerenderer.csv` (append mode). The program is self-contained and does not include `bench.h`; it copies the field layout. One extra line kind, `summary`, carries scalars.

```text
meta,<key>,<value>
result,tilerenderer,<phase>,<variant>,<visible_tiles>,<frames>,<min_us>,<med_us>,<max_us>
summary,tilerenderer,<variant>,<key>,<value>
note,<text>
error,<where>,<detail>
```

| Line | Content |
| ---- | ------- |
| `meta` | The `bench.h` keys (program, build date, compiler, model, `new3ds`, firmware, kernel, ticks per microsecond, free linear memory; libctru/citro3d versions and the New 3DS speed-up/L2 state as `not-readable`) plus `vram_free_bytes`, every macro above (`tile_strategy`, `tile_px`, `chunk_tiles`, `tex_mode`, `map_edit_period`, `shape_mode`, `shape_pairs`, `bottom_period`, `npc_count`, `cam_speed_px_per_frame`, `bench_path`, `warmup_frames`, `measured_frames`) and, for strategy 2, `chunk_build_all_ms` |
| `result` | One line per phase (`period`, `update`, `build`, `record`, `framebegin`, `frameend`, `c3d_proc`, `c3d_gpu`). `<variant>` is `<strategy>-t<TILE>-c<chunk>-x<TEX_MODE>-e<edit period>-s<shape mode>x<pairs>-b<bottom period>-v<speed>-n<npcs>-p<path>`. The fifth field is the visible tile count of the last measured frame; times are microseconds over `<frames>` frames |
| `summary` | `mean_<phase>_us`, `p99_<phase>_us`, `mean_cpu_build_record_frameend_us`, `missed_frames_over_25ms` (loop periods longer than 25 ms), `vblanks_elapsed` (`C3D_FrameCounter(0)` advance over the measured frames; more than `frames` means skipped refreshes), `rebuilds`, `dropped_objects`, `cmdbuf_peak_pct`, `resume_events` (HOME-restore and sleep-wake notifications), `linear_free_end_bytes`, `vram_free_end_bytes` |
| `error` | A failed call or file open; a run with any error line is not a result |

A row in the wiki's results tables is one variant on one console: the median of the runs' medians, with the min and max across runs.

## Technical details

- Object count per frame, by arithmetic, direct strategy at 16 px: up to 416 tile quads + 82 sprite quads (41 actors × shadow and body) + 1 HUD rectangle + glyph quads for about 230 characters if each glyph is one object (`[hypothesis]` on the glyph accounting) + 3 on the bottom screen: roughly 730 of the 4,096 allowed by `C2D_DEFAULT_MAX_OBJECTS`. At `TILE=8` the tiles alone are up to 1,581. A cache redraw frame adds `(512/TILE)²` (1,024 at 16 px).
- Memory by arithmetic: atlas 128×32×4 = 16,384 bytes of linear memory (twice with `TEX_MODE`); map 12,288 bytes; citro2d buffers 573,440 bytes; strategy 3 adds 1,572,864 bytes of VRAM; strategy 2 adds 1,474,560 bytes of linear memory plus the dynamic buffer; the sample arrays of a `BENCH_AUTO` build take 8 phases × `BENCH_FRAMES` × 4 bytes (19,200 bytes at 600 frames, 1,152,000 at 36,000).
- Batch structure of one frame, assuming citro2d's rules from [2D rendering](rendering-2d.md#the-object-limit-and-how-batching-works): tiles and sprites share the atlas (one run); the rectangle breaks it; each text object ends with font-sheet texture changes; the bottom screen starts a new scene. `[hypothesis]` for the exact draw-call count: nothing here counted them ([command capture](pica200-command-pipeline.md) would).
- `Z_ALL = 0.5f` lies inside citro2d's usable z range −1..1 ([depth](rendering-2d.md#2-screens-coordinates-and-depth)).
- Input: ±156 is treated as full Circle Pad deflection and values within ±15 of centre are ignored; both numbers are assumptions made for this example, not read from libctru (Unknown: the real range). The D-pad overrides the pad.
- The 25 ms missed-frame threshold is 1.5 nominal periods of 16.7 ms; the exact LCD refresh rate is not confirmed in the sources used ([frame pacing](frame-pacing.md#open-questions)).

## Pitfalls

- **Upside-down or mirrored atlas.** If tiles and sprites appear flipped vertically, the V convention (v = 1 at the top of the stored image) does not match what the GPU does with this layout. Check the swizzle first, then `atlasSub`. Strategies 1 and 2 use the same UV table, so they flip together.
- **Upside-down cache.** Rebuild strategy 3 with `-DCACHE_FLIP_V=1`; if it is flipped horizontally instead, the transfer assumptions differ and the cache needs more investigation.
- **Manual strategies draw nothing, or sprites vanish after them.** The [hand-over to citro2d](#strategies-1-and-2-handing-the-gpu-state-over) is the part most likely to need a fix on the first run. Check first that tiles appear with `NPC_COUNT=0`, then that sprites and text still draw.
- **Wrong colours.** Texels are `0xRRGGBBAA`; `C2D_Color32` is `0xAABBGGRR`. Do not swap them when editing.
- **Cache texture sampled in the pass that renders into it.** The redraw draws the atlas into the cache; the screen pass reads the cache. Keep them separate.
- **Object limit.** More sprites or smaller tiles: count the objects and watch `dropped_objects`; a run with drops is not a result.
- **Command buffer.** Thousands of separate draw calls (`TEX_MODE=1` at small tiles) can fill the default buffer: raise `CMDBUF_SIZE` and watch `cmdbuf_peak_pct`.
- **A `BENCH_AUTO` run shows no overlay.** By design: re-parsing text every 30 frames would add spikes to `update`.
- **Reading emulator timings.** Never use them.

## Open questions

Every item is an experiment above; nothing has run.

- E1 to E6: the numbers, on an Old 3DS and a New 3DS.
- Whether the cached layer is upright and pixel-exact with a standard subtexture, and whether strategies 0 to 3 produce identical pixels (the [validation checklist](rendering-2d.md#validation-checklist-for-a-2d-engine) describes the comparison; this program has no framebuffer dump).
- Whether the cache redraw spike shows as a visible hitch at 60 frames per second.
- Whether citro2d's internal state survives the strategy 1 and 2 hand-over unchanged.
- Whether the number of draw calls per frame matches the expected structure.
- Whether any of this differs between Old 3DS and New 3DS. Unknown: GPU-side costs on the New 3DS.

## Verification status

All rows: Emulator not run, Old 3DS not run, New 3DS not run.

| Configuration (`-D` flags) | Builds |
| -------------------------- | ------ |
| (none): direct tiles, interactive | yes |
| `USE_TILE_CACHE=1`; `USE_TILE_CACHE=1 CACHE_FLIP_V=1` (the original cache variants) | yes (both) |
| `TILE_STRATEGY=1`; `=2`; `=2 CHUNK_TILES=8`; `=2 CHUNK_TILES=32` (with `vshader.v.pica`) | yes (all four) |
| `TILE_STRATEGY=3`; `=3 TILE=32` | yes (both) |
| `TEX_MODE=1`; `TEX_MODE=2` | yes (both) |
| `MAP_EDIT_PERIOD=30` with strategy 0, 2 and 3 | yes (all three) |
| `SHAPE_MODE=1`; `SHAPE_MODE=2`; `BOTTOM_PERIOD=30` | yes (all three) |
| `TILE=8`; `TILE=32`; strategy 1 with `TILE=8`; strategy 2 with `TILE=8` | yes (all four); strategy 1 with `TILE=12` also builds |
| `BENCH_AUTO=1` with strategy 0, 1, 2 and 3; with `TEX_MODE=1` | yes (all five) |
| `BENCH_AUTO=1 TILE_STRATEGY=2 MAP_EDIT_PERIOD=30 SHAPE_MODE=1 BOTTOM_PERIOD=2 BENCH_PATH=1 NPC_COUNT=0`; `BENCH_AUTO=1 TILE_STRATEGY=3 NPC_COUNT=0 MAP_EDIT_PERIOD=30 CAM_SPEED=4.0f`; `BENCH_AUTO=1 CMDBUF_SIZE=0x100000` | yes (all three) |
| Guards: `TEX_MODE=1 TILE_STRATEGY=1`; `TILE_STRATEGY=3 TILE=8`; `TILE_STRATEGY=2 CHUNK_TILES=10` | stop with `#error` as intended |

"Builds": `arm-none-eabi-gcc` (devkitARM r68, GCC 16.1.0) invoked directly with `DEVKITPRO=C:\devkitPro`, `-D__3DS__ -Wall -Wextra -O2 -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -mword-relocations -ffunction-sections -fno-math-errno -std=gnu11`, include path `libctru/include`, linked to an ELF with `-specs=3dsx.specs -lcitro2d -lcitro3d -lctru -lm`; no warnings in any of the 30 configurations built (28 in a scripted matrix, plus `BENCH_AUTO=1 CMDBUF_SIZE=0x100000` and strategy 1 with `TILE=12` as spot builds). The shader was assembled with the installed picasso, converted with `bin2s` and assembled into an object that was linked in (the template `Makefile` does the same). No `.3dsx` was packaged and nothing was run.

## Sources

- [2D rendering](rendering-2d.md) — the setup, depth, texture-layout and batching facts this program uses, the design-alternatives table the experiments belong to, and the validation checklist.
- [citro2d](https://github.com/devkitPro/citro2d) — installed 1.7.0 headers (`base.h`, `sprite.h`, `text.h`) for every call used, including the circle-mode notes on the ellipse and circle functions.
- [citro3d](https://github.com/devkitPro/citro3d) — `renderqueue.c` (frame functions, `C3D_FrameCounter`, render-target creation), `texture.c`, installed headers `attribs.h`, `buffers.h`, `effect.h` for the manual draw state.
- [libctru](https://github.com/devkitPro/libctru) — `hid.h` (`hidCircleRead`), `os.h` (`SYSCLOCK_ARM11`, `svcGetSystemTick`), `services/apt.h` (hooks), `allocator/vram.h`.
- [3ds-examples](https://github.com/devkitPro/3ds-examples) — `graphics/gpu/2d_shapes/source/main.c` (circle-grouping comment, read locally).
- [CTR benchmarks](../benchmarks/README.md) — the `C3D_FrameEnd` timing caveat and the record format this page copies.
