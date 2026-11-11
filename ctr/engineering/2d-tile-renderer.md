# 2D tile renderer: a worked example

A scrolling tilemap is the standard test of a 2D engine: a world far larger than the screen, a camera that follows the player, animated tiles, characters that walk in front of and behind each other, text, and a way to see what the frame costs. This page walks through one complete program that does all of that with [citro2d](rendering-2d.md), using only art generated in code, so it builds with no asset files. It also has a switch that replaces the per-frame tile drawing with a cached background drawn into a texture, so you can compare the two on your own console.

Read [2D rendering](rendering-2d.md) first for the setup, coordinate and texture rules this program relies on. This page assumes you know what an atlas, a draw call and a batch are (defined there).

## Before you start

- Target: **Old 3DS** (the program needs nothing from the New 3DS). Built with devkitARM r68 (GCC 16.1.0), citro2d 1.7.0, citro3d 1.7.1, libctru 2.7.1 ([source versions](source-versions.md)).
- **Verification status:** builds in all three configurations (see [Verification status](#verification-status)); **not run on Old 3DS, New 3DS or emulator.** No timing on this page is a measurement. The first run may show the textures or the cached layer mirrored; the checks are listed in [Pitfalls](#pitfalls).
- Controls: Circle Pad or D-pad moves the red character, SELECT shows or hides the debug text on the bottom screen, START quits.

## What the program does

| Part | How it is done | Why |
| ---- | -------------- | --- |
| World | 128×96 tiles of 16×16 pixels (2048×1536 pixels), generated from sine waves and a hash | No map file |
| Atlas | One 128×32 RGBA8 texture: row 0 has 4 ground tiles and 4 water frames, row 1 has sprites; painted and swizzled in code | One texture means tiles and sprites can share batches |
| Camera | Follows the player, clamped to the world, converted to whole pixels with `floorf` | Pixel art shimmers at fractional positions |
| Tiles | Loop over the visible tile range only; one `C2D_DrawImageAt` per tile | At most 26×16 = 416 quads instead of 12,288 |
| Tile animation | Water tiles pick one of four atlas frames from the frame counter | Keeps the animation in the atlas, no per-vertex work |
| Sprites | 40 wandering characters plus the player, insertion-sorted by y each frame, a shadow drawn under each | Painter's algorithm for overlap; same atlas, so no batch break |
| HUD | A static title parsed once; the tile position text re-parsed only when the visible tile range changes | Text parsing costs CPU every time; cache it |
| Bottom screen | World overview rectangles, a camera box, a player dot, and the debug text | Shows primitives and a second target |
| Debug overlay | Update, record and `C3D_FrameEnd` times in milliseconds, averaged over 30 frames | See [below](#the-debug-overlay) |
| Cache variant | `-DUSE_TILE_CACHE=1`: tiles are drawn into a 512×512 VRAM texture and reused | See [below](#render-to-texture-cached-background) |

## The program

Single file `main.c`. Build flags and exact compiler: [Verification status](#verification-status). Add `-DUSE_TILE_CACHE=1` for the cached variant, and `-DCACHE_FLIP_V=1` as well if the cached layer comes out upside down.

```c
/* tile_renderer: scrolling tilemap + y-sorted sprites + cached HUD text + debug overlay.
 * No art assets: the atlas is painted in code. Needs citro2d, citro3d, libctru.
 * Build with -DUSE_TILE_CACHE=1 for the render-to-texture background variant. */
#include <3ds.h>
#include <citro2d.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef USE_TILE_CACHE
#define USE_TILE_CACHE 0     /* 1: draw static tiles into a 512x512 VRAM texture, reuse it until the camera leaves it */
#endif
#ifndef CACHE_FLIP_V
#define CACHE_FLIP_V 0       /* 1 if the cached layer shows up upside down on the first run (orientation is unverified) */
#endif

#define TOP_W 400
#define BOT_W 320
#define SCR_H 240
#define TILE 16
#define MAP_W 128
#define MAP_H 96
#define WORLD_W (MAP_W * TILE)
#define WORLD_H (MAP_H * TILE)
#define NPC_COUNT 40
#define Z_ALL 0.5f           /* one depth for every layer: C2D_Prepare sets the depth test to GEQUAL, so equal depths pass and draw order decides */

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
	int ox = (index % 8) * TILE, oy = (index / 8) * TILE;
	for (int y = 0; y < TILE; y++)
		for (int x = 0; x < TILE; x++) {
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
	int ox = (index % 8) * TILE, oy = (index / 8) * TILE;
	for (int y = 0; y < TILE; y++)
		for (int x = 0; x < TILE; x++) {
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
	int px = (index % 8) * TILE, py = (index / 8) * TILE;
	s->width = TILE; s->height = TILE;
	s->left = (float)px / ATLAS_W;            s->right  = (float)(px + TILE) / ATLAS_W;
	s->top  = 1.0f - (float)py / ATLAS_H;     s->bottom = 1.0f - (float)(py + TILE) / ATLAS_H;
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

#if USE_TILE_CACHE
#define CACHE_TILES 32                       /* 32 x 16 px = 512 px square */
static C3D_Tex gCacheTex;
static C3D_RenderTarget *gCacheTarget;
static int gCacheTX, gCacheTY;
static bool gCacheValid;
#endif

int main(void)
{
	gfxInitDefault();
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	if (!C2D_Init(C2D_DEFAULT_MAX_OBJECTS)) { C3D_Fini(); gfxExit(); return 1; }
	C2D_Prepare();
	C3D_RenderTarget *top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
	C3D_RenderTarget *bot = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

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

	static Tex3DS_SubTexture sub[ATLAS_IMAGES];
	static C2D_Image img[ATLAS_IMAGES];
	for (int i = 0; i < ATLAS_IMAGES; i++) { atlasSub(&sub[i], i); img[i].tex = &atlas; img[i].subtex = &sub[i]; }

#if USE_TILE_CACHE
	if (!C3D_TexInitVRAM(&gCacheTex, 512, 512, GPU_RGBA8)) { C3D_TexDelete(&atlas); C2D_Fini(); C3D_Fini(); gfxExit(); return 1; }
	C3D_TexSetFilter(&gCacheTex, GPU_NEAREST, GPU_NEAREST);
	C3D_TexSetWrap(&gCacheTex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
	gCacheTarget = C3D_RenderTargetCreateFromTex(&gCacheTex, GPU_TEXFACE_2D, 0, GPU_RB_DEPTH16);
	if (!gCacheTarget) { C3D_TexDelete(&gCacheTex); C3D_TexDelete(&atlas); C2D_Fini(); C3D_Fini(); gfxExit(); return 1; }
	static Tex3DS_SubTexture cacheSub = { 512, 512, 0.0f, 1.0f, 1.0f, 0.0f };
	C2D_Image cacheImg = { &gCacheTex, &cacheSub };
#endif

	buildMap();
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

	bool showDebug = true;
	u32 frameNo = 0;
	u64 prevLoopStart = svcGetSystemTick();
	/* debug accumulators (milliseconds, averaged over 30 frames) */
	float accPeriod = 0, accUpdate = 0, accRecord = 0, accEnd = 0;
	int visibleTiles = 0, rebuilds = 0;

	while (aptMainLoop()) {
		u64 loopStart = svcGetSystemTick();
		accPeriod += ticksToMs(loopStart - prevLoopStart);
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
		Actor *pl = &gActor[0];
		pl->x = clampf(pl->x + ix * 2.0f, 8, WORLD_W - 8);
		pl->y = clampf(pl->y + iy * 2.0f, 8, WORLD_H - 8);
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
		u64 updateEnd = svcGetSystemTick();
		accUpdate += ticksToMs(updateEnd - loopStart);

		/* ---- draw ---- */
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		u64 recStart = svcGetSystemTick();

#if USE_TILE_CACHE
		if (!gCacheValid || tx0 < gCacheTX || tx1 >= gCacheTX + CACHE_TILES || ty0 < gCacheTY || ty1 >= gCacheTY + CACHE_TILES) {
			gCacheTX = (int)clampf((float)(tx0 - (CACHE_TILES - (tx1 - tx0 + 1)) / 2), 0, MAP_W - CACHE_TILES);
			gCacheTY = (int)clampf((float)(ty0 - (CACHE_TILES - (ty1 - ty0 + 1)) / 2), 0, MAP_H - CACHE_TILES);
			C3D_RenderTargetClear(gCacheTarget, C3D_CLEAR_ALL, 0x000000FF, 0);
			C2D_SceneBegin(gCacheTarget);
			for (int ty = 0; ty < CACHE_TILES; ty++)
				for (int tx = 0; tx < CACHE_TILES; tx++)
					C2D_DrawImageAt(img[gMap[gCacheTY + ty][gCacheTX + tx]], (float)(tx * TILE), (float)(ty * TILE), Z_ALL, NULL, 1.0f, 1.0f);
			gCacheValid = true;
			rebuilds++;
		}
#endif
		C2D_TargetClear(top, C2D_Color32(0, 0, 0, 255));
		C2D_SceneBegin(top);

#if USE_TILE_CACHE
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
						C2D_DrawImageAt(img[waterFrame], (float)(tx * TILE - camX), (float)(ty * TILE - camY), Z_ALL, NULL, 1.0f, 1.0f);
		}
#else
		for (int ty = ty0; ty <= ty1; ty++)
			for (int tx = tx0; tx <= tx1; tx++) {
				int id = gMap[ty][tx];
				if (id == T_WATER0) id = waterFrame;
				C2D_DrawImageAt(img[id], (float)(tx * TILE - camX), (float)(ty * TILE - camY), Z_ALL, NULL, 1.0f, 1.0f);
			}
#endif
		/* sprites in sorted order; shadow first so the body overlaps it; all share the atlas, so no batch break */
		for (int n = 0; n <= NPC_COUNT; n++) {
			const Actor *a = &gActor[order[n]];
			float sx = floorf(a->x) - camX - 8, sy = floorf(a->y) - camY - 12;
			if (sx < -TILE || sx > TOP_W || sy < -TILE || sy > SCR_H) continue;   /* cull off-screen sprites */
			C2D_DrawImageAt(img[S_SHADOW], sx, sy + 4, Z_ALL, NULL, 1.0f, 1.0f);
			C2D_DrawImageAt(img[a->image], sx, sy, Z_ALL, NULL, 1.0f, 1.0f);
		}

		/* HUD: rectangles and text break the batch, so they come last */
		C2D_DrawRectSolid(0, 0, Z_ALL, TOP_W, 14, C2D_Color32(0, 0, 0, 160));
		C2D_DrawText(&title, C2D_WithColor, 4, 1, Z_ALL, 0.4f, 0.4f, C2D_Color32(255, 255, 255, 255));
		C2D_DrawText(&posText, C2D_WithColor, 4, 16, Z_ALL, 0.5f, 0.5f, C2D_Color32(255, 255, 160, 255));

		C2D_TargetClear(bot, C2D_Color32(20, 20, 28, 255));
		C2D_SceneBegin(bot);
		/* world overview: 2048x1536 px scaled by 1/16 -> 128x96 */
		C2D_DrawRectSolid(10, 10, Z_ALL, MAP_W, MAP_H, C2D_Color32(40, 90, 60, 255));
		C2D_DrawRectangle(10 + camX / 16.0f, 10 + camY / 16.0f, Z_ALL, TOP_W / 16.0f, SCR_H / 16.0f,
			C2D_Color32(255, 255, 255, 120), C2D_Color32(255, 255, 255, 120), C2D_Color32(255, 255, 255, 60), C2D_Color32(255, 255, 255, 60));
		C2D_DrawCircleSolid(10 + pl->x / 16.0f, 10 + pl->y / 16.0f, Z_ALL, 2.0f, C2D_Color32(240, 90, 60, 255));
		if (showDebug)
			C2D_DrawText(&dbgText, C2D_WithColor, 10, 116, Z_ALL, 0.5f, 0.5f, C2D_Color32(190, 255, 190, 255));

		u64 endStart = svcGetSystemTick();
		accRecord += ticksToMs(endStart - recStart);
		C3D_FrameEnd(0);
		accEnd += ticksToMs(svcGetSystemTick() - endStart);
		frameNo++;
	}

#if USE_TILE_CACHE
	C3D_RenderTargetDelete(gCacheTarget);
	C3D_TexDelete(&gCacheTex);
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

## How it works

### Textures made in code

`paintTile` and `paintSprite` write `0xRRGGBBAA` pixels into a top-down 128×32 array; `swizzleRGBA8` rearranges them into the GPU's tiled order (8×8 blocks, Morton order inside a block); `atlasSub` computes each image's texture coordinates. The layout and the v = 1 = top convention are explained and checked against `tex3ds` on [2D rendering](rendering-2d.md#generating-a-texture-in-code). The atlas is in linear memory, so the CPU writes it directly, and `C3D_TexFlush` writes the cache back before the GPU ever reads it. Filtering is `GPU_NEAREST` and wrapping `GPU_CLAMP_TO_EDGE`; with whole-pixel positions no border pixels are needed.

### Camera, clamp and whole pixels

```c
float camXf = clampf(pl->x - TOP_W / 2, 0, WORLD_W - TOP_W);
float camYf = clampf(pl->y - SCR_H / 2, 0, WORLD_H - SCR_H);
int camX = (int)floorf(camXf), camY = (int)floorf(camYf);
```

The camera centres on the player, then is clamped so the view never leaves the world (`WORLD_W - TOP_W` is the largest allowed left edge). Everything drawn is `world position − camX/camY`, all integers for the tiles and `floorf`-ed for the sprites. The view matrix is not used, so the batch never breaks for it.

### Only the visible tiles

```c
int tx0 = camX / TILE, ty0 = camY / TILE;
int tx1 = (camX + TOP_W - 1) / TILE, ty1 = (camY + SCR_H - 1) / TILE;
```

These are the first and last tile columns and rows touched by the 400×240 window. With a camera aligned to a tile that is 25×15 = 375 tiles; otherwise at most 26×16 = 416 (arithmetic). The map is small enough to index directly; for a map larger than memory you would stream chunks, which this example does not do.

### Tile animation

Tile id `T_WATER0` in the map means "water"; at draw time it is replaced by `T_WATER0 + ((frameNo / 12) & 3)`, so four atlas frames cycle five times a second at 60 frames per second. The atlas holds the four frames side by side (index 4 to 7).

### Sprites with depth order

Characters sit on the same ground plane, so one should cover another when it is further down the screen. The program sorts an index array by `y` (insertion sort: nearly sorted from frame to frame, so it is cheap), then draws back to front. Every layer uses the same `Z_ALL`. That is deliberate: citro2d turns the depth test on with `GPU_GEQUAL`, so equal depths pass and the draw order decides ([depth rules](rendering-2d.md#2-screens-coordinates-and-depth)); and transparent texels still write depth, so a z-per-sprite scheme would let the transparent corners of a nearer sprite cut holes in a farther one. Sorting avoids both problems. Sprites outside the screen are skipped before the draw call to save object slots and CPU work.

Order inside a character: the shadow quad first, then the body, so the body overlaps its own shadow.

### Text that is parsed once

```c
if (tx0 != lastTX || ty0 != lastTY) { /* re-parse "tile x,y" into posBuf */ }
```

`C2D_TextParse` lays out glyphs on the CPU; doing it every frame for unchanged text is wasted work. The title is parsed once into `staticBuf`; the position text lives in its own buffer `posBuf` and is rebuilt only when the visible tile range changes; the debug text is rebuilt every 30 frames in `dbgBuf`. Separate buffers matter because `C2D_TextBufClear` invalidates every text parsed into that buffer.

### The debug overlay

Four numbers are timed with `svcGetSystemTick()` (268,111,856 ticks per second, [system tick](../../glossary.md#system-tick)) and averaged over 30 frames, and two more come from citro3d:

| Row | What it measures | Does not include |
| --- | ---------------- | ---------------- |
| `period` | Time between the start of consecutive loop iterations | Nothing: but with `C3D_FRAME_SYNCDRAW` it is quantised to the display refresh (16.7 ms steps), so it shows missed frames, not headroom |
| `update` | Input, game logic, camera, sort and text caching, before `C3D_FrameBegin` | Drawing |
| `record` | From `C3D_FrameBegin` returning to just before `C3D_FrameEnd`: the cost of the `C2D_Draw*` calls (filling vertex data) | `C3D_FrameBegin`'s wait, `C3D_FrameEnd`, GPU work |
| `FrameEnd` | Wall-clock around `C3D_FrameEnd(0)` | GPU execution, which continues after it returns |
| `C3D proc` | `C3D_GetProcessingTime()` | The linear-heap cache flush inside `C3D_FrameEnd`, which happens after citro3d stops this timer |
| `C3D draw` | `C3D_GetDrawingTime()`: GPU drawing plus display transfer of the last finished frame | CPU work |

The caveat from [benchmarks](../benchmarks/README.md#drawbench) applies: **compare variants with `record + FrameEnd`, not `C3D proc` alone**, because `C3D_FrameEnd(0)` flushes the whole linear heap and `C3D_GetProcessingTime` stops before that ([citro3d's two timers](performance.md#citro3ds-two-timers), [the FrameEnd flush](memory-and-cache.md#the-c3d_frameend-flush)). The overlay shows the previous 30 frames, because drawing the text is itself part of the frame being measured. The text uses ticks around the whole `C3D_FrameEnd` call, which includes both command-list finalisation and the flush.

Do not read emulator output as a result. On hardware, record the model, the build flags and the camera path, and put results in the [benchmark README format](../benchmarks/README.md#output-format-and-metadata).

### Render to texture: cached background

`-DUSE_TILE_CACHE=1` changes the tile pass:

1. A 512×512 RGBA8 texture is created in VRAM with `C3D_TexInitVRAM` and turned into a render target with `C3D_RenderTargetCreateFromTex(..., GPU_RB_DEPTH16)`. That is 1 MiB for colour plus 512 KiB of depth, beside the screen targets' about 1 MiB (arithmetic; [VRAM table](rendering-2d.md#vram-or-linear-memory)).
2. The cache covers a 32×32 tile window (512×512 pixels) of the world. When the visible tile range is no longer inside the window, the window is re-centred on the camera and all 1,024 tiles are drawn into the texture (`C2D_SceneBegin(cacheTarget)`), with water drawn as its first frame.
3. Each frame the screen gets one quad of the cache at `(window origin − camera)`, then only the visible water tiles are drawn over it with the current animation frame, then sprites and HUD as before.

What it saves and what it costs, by counting quads (arithmetic, not time): per frame the direct path submits up to 416 tile quads; the cached path submits 1 plus the visible water tiles. A rebuild costs 1,024 quads in one frame. With 3 tiles of slack on each side horizontally (32 − 26 = 6), running at 2 pixels per frame forces a rebuild about every 24 frames, so over such a run the cached path submits roughly 10× fewer tile quads but moves the work into spike frames. Whether it wins in time is a `[hypothesis]` until measured: compare `record`, `FrameEnd`, `C3D proc` and `C3D draw` from the overlay, and watch the `rebuilds` counter.

Caveats:

- Anything animated or sprite-like must stay out of the cache (done here for water).
- A larger cache means fewer rebuilds but more VRAM; a 16-bit colour format would halve it `[hypothesis]`, and this example does not try it.
- Switching the render target mid-frame has a cost ([draw-call costs](gpu-rendering.md#what-a-draw-call-costs)); the rebuild happens before the screen pass for that reason, and every `C2D_SceneBegin` flushes the batch.
- The cached quad is bigger than the screen (512 wide against 400); the part outside is clipped. Fill cost of the hidden part has not been measured.
- Orientation of the cached image is unverified (below).

## Technical details

- Object count per frame, by arithmetic: up to 416 tile quads + 82 sprite quads (41 actors × shadow and body) + 1 HUD rectangle + glyph quads for about 230 characters if each glyph is one object (`[hypothesis]` on the glyph accounting) + 3 on the bottom screen: roughly 730 of the 4,096 allowed by `C2D_DEFAULT_MAX_OBJECTS`. A cache rebuild frame adds 1,024.
- Memory by arithmetic: atlas 128×32×4 = 16,384 bytes of linear memory; map 12,288 bytes; citro2d buffers 573,440 bytes; cache variant +1,572,864 bytes of VRAM.
- Batch structure of one frame, assuming citro2d's rules from [2D rendering](rendering-2d.md#the-object-limit-and-how-batching-works): tiles and sprites share the atlas (one run); the rectangle breaks it; each text object ends with font-sheet texture changes; the bottom screen starts a new scene. `[hypothesis]` for the exact number of draw calls: nothing here counted them.
- `Z_ALL = 0.5f` lies inside citro2d's usable z range −1..1 ([depth](rendering-2d.md#2-screens-coordinates-and-depth)).
- Input: the code treats ±156 as full Circle Pad deflection and ignores values within ±15 of centre; both numbers are assumptions made for this example, not read from libctru (Unknown: the real range). The D-pad overrides the pad. Movement speed is 2 pixels per frame.

## Pitfalls

- **Upside-down or mirrored atlas.** If tiles and sprites appear flipped vertically, the V convention (v = 1 at the top of the stored image) does not match what the GPU does with this layout. Check the swizzle first, then `atlasSub`.
- **Upside-down cache.** Rebuild with `-DCACHE_FLIP_V=1`; it draws the quad with a negative height. If it is flipped horizontally instead, the transfer assumptions differ and the cache needs more investigation.
- **Wrong colours.** Texels are `0xRRGGBBAA`; `C2D_Color32` is `0xAABBGGRR`. The program uses each in the right place; do not swap them when editing.
- **Cache texture sampled in the pass that renders into it.** The rebuild draws the atlas into the cache; the screen pass reads the cache. Keep them separate.
- **Object limit.** If you raise the world detail (more sprites, smaller tiles) count the objects before assuming `C2D_DEFAULT_MAX_OBJECTS` is enough, and check the return values.
- **Reading emulator timings.** Never use them.

## Open questions

- Frame times (`record`, `FrameEnd`, `C3D proc`, `C3D draw`) on an Old 3DS and a New 3DS for the direct and cached variants over the same camera path — the program is the experiment; nothing has been run.
- Whether the cached layer is upright and pixel-exact with a standard subtexture.
- Whether the rebuild spike (1,024 quads plus a target switch) shows as a visible hitch at 60 frames per second.
- Whether the number of draw calls per frame matches the expected structure (a command-list capture would show it: [PICA200 command pipeline](pica200-command-pipeline.md)).

## Verification status

| Configuration | Builds | Emulator | Old 3DS | New 3DS |
| ------------- | ------ | -------- | ------- | ------- |
| Direct tiles (default) | yes | not run | not run | not run |
| `-DUSE_TILE_CACHE=1` | yes | not run | not run | not run |
| `-DUSE_TILE_CACHE=1 -DCACHE_FLIP_V=1` | yes | not run | not run | not run |

"Builds": `arm-none-eabi-gcc` (devkitARM r68, GCC 16.1.0) invoked directly with `DEVKITPRO=C:\devkitPro`, `-D__3DS__ -Wall -Wextra -O2 -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -mword-relocations -ffunction-sections -fno-math-errno -std=gnu11`, include path `libctru/include`, linked to an ELF with `-specs=3dsx.specs -lcitro2d -lcitro3d -lctru -lm`; no warnings. No `.3dsx` was packaged and nothing was run.

## Sources

- [2D rendering](rendering-2d.md) — the setup, depth, texture-layout and batching facts this program uses, with their sources.
- [citro2d](https://github.com/devkitPro/citro2d) — installed 1.7.0 headers (`base.h`, `sprite.h`, `text.h`) for every call used.
- [citro3d](https://github.com/devkitPro/citro3d) — `renderqueue.c` (frame functions, render-target creation), `texture.c` (`C3D_TexInit`, `C3D_TexFlush`, `C3D_TexInitVRAM`).
- [libctru](https://github.com/devkitPro/libctru) — `hid.h` (`hidCircleRead`), `os.h` (`SYSCLOCK_ARM11`, `svcGetSystemTick`).
- [CTR benchmarks](../benchmarks/README.md) — the `C3D_FrameEnd` timing caveat and result format.
