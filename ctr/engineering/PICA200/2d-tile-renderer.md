# 2D tile renderer: a worked example and experiment rig

A scrolling tilemap is the standard test of a 2D engine: a world far larger than the screen, a camera that follows the player, animated tiles, characters that walk in front of and behind each other, text, and a way to see what the frame costs. This page walks through one complete program that does all of that with [citro2d](rendering-2d.md), using only art generated in code, so it builds with no asset files. The program also has compile-time switches that change *how the tiles reach the GPU* (four strategies) and a scripted mode that records per-phase timings to a file on the SD card, so the crossover questions on [2D rendering](rendering-2d.md#design-alternatives-and-how-to-choose) can be answered on a console.

> **Status: this is an experiment rig, not a performance reference.** Nothing on this page has been run on an Old 3DS, a New 3DS or an emulator. Every number below is arithmetic from sizes or a `[hypothesis]`; no frame time, speed-up or crossover has been measured, and none may be quoted from here. What *has* been done: the program compiles without warnings in 30 configurations ([build matrix](#verification-status)). Expect to debug the first run (orientation of the cached layer, the manual-draw state hand-over; see [Pitfalls](#pitfalls)). The result of a run is a CSV file whose format is fixed in [Output records](#output-records) so that results from different people can be compared.

Read [2D rendering](rendering-2d.md) first for the setup, coordinate and texture rules this program relies on. This page assumes you know what an atlas, a draw call and a batch are (defined there).

## Before you start

- Target: **Old 3DS** (the program needs nothing from the New 3DS). Built with devkitARM r68 (GCC 16.1.0), citro2d 1.7.0, citro3d 1.7.1, libctru 2.7.1 ([source versions](../source-versions.md)). **New 3DS:** runs the same code; record the model and note the speed-up state yourself (the program cannot read it) and keep New 3DS results in a separate data set.
- Default build (no `-D` flags) is the original tile renderer: direct `C2D_DrawImageAt` per visible tile, debug overlay, no files written.
- Controls (interactive builds): Circle Pad or D-pad moves the red character, SELECT shows or hides the debug text on the bottom screen, START quits. In a `BENCH_AUTO=1` build only START works; the camera follows a scripted path.
- Strategies 1 and 2 need the included `source/vshader.v.pica` (described [below](#the-shader-for-strategies-1-and-2)), assembled by the project Makefile's picasso rule into `vshader_shbin.h`. The other builds do not use it.

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

The complete implementation is in [the tile-renderer project](../../benchmarks/tile-renderer/source/main.c), with its [PICA shader](../../benchmarks/tile-renderer/source/vshader.v.pica) and [Makefile](../../benchmarks/tile-renderer/Makefile). Build it with the standard devkitPro environment using `make -C ctr/benchmarks/tile-renderer`. Pass compile-time switches through `EXTRA_CFLAGS`, for example `make -C ctr/benchmarks/tile-renderer EXTRA_CFLAGS="-DTILE_STRATEGY=1 -DBENCH_AUTO=1"`; clean before changing switches. The project has not been run on a console or emulator; see [verification status](#verification-status).

### The shader for strategies 1 and 2

The shader transforms position by a projection and offset, passes texture coordinates through and supplies white vertex colour. Strategy 1 sets the offset to zero and bakes the camera into the vertices; strategy 2 keeps world-space vertices and sets the offset to `(-camX, -camY)`.

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

Tile id `T_WATER0` means "water"; at draw time it becomes `T_WATER0 + ((frameNo / 12) & 3)`, so at 60 rendered frames per second it advances five animation frames per second and completes 1.25 four-frame cycles per second. The animation uses the render-frame counter: at 30 fps it completes 0.625 cycles per second; a game needing a fixed animation rate should use simulation time instead. Sprites are sorted by `y` (insertion sort: nearly sorted from frame to frame) and drawn back to front with one shared `Z_ALL`, because citro2d's depth test passes equal depths and transparent texels still write depth, so a z-per-sprite scheme would cut holes in farther sprites ([depth rules](rendering-2d.md#2-screens-coordinates-and-depth)); the shadow is drawn before the body, and off-screen sprites are skipped before the draw call. `C2D_TextParse` lays out glyphs on the CPU, so unchanged text is not re-parsed: the title is parsed once, the position text is rebuilt only when the visible tile range changes, and the debug text every 30 frames, each in its own buffer because `C2D_TextBufClear` invalidates every text parsed into that buffer ([text](rendering-2d.md#text)).

### Strategies 1 and 2: handing the GPU state over

The manual passes use citro3d directly while citro2d owns the rest of the frame. `manualBegin` calls `C2D_Flush()` (so nothing batched earlier is lost), binds the program, attribute layout, vertex buffer, the atlas and a replace-texture combiner, turns the depth test off (so citro2d's later sprites pass) and sets the projection and offset uniforms. `manualEnd` restores back-face culling, calls `C2D_Prepare()` (the documented way to re-establish citro2d's state after changing citro3d's) and `C2D_SceneBegin(top)` (which re-applies citro2d's projection). All of that is part of the strategy's cost and is counted in `record`. Whether citro2d's internal state (for example which texture it believes is bound) survives this hand-over is `[hypothesis]`: citro2d's `source/base.c` is not cloned here. The CPU writes the vertex buffer after `C3D_FrameBegin(C3D_FRAME_SYNCDRAW)` returned, which has waited for the previous frame's GPU work ([frame functions](rendering-2d.md#the-frame-loop)), so it never overwrites data the GPU is reading; the whole-heap flush in `C3D_FrameEnd(0)` makes the writes visible to the GPU ([FrameEnd flush](../memory-and-cache.md#the-c3d_frameend-flush)).

### Strategy 3: render-to-texture cache

1. A 512×512 RGBA8 texture is created in VRAM with `C3D_TexInitVRAM` and turned into a render target with `C3D_RenderTargetCreateFromTex(..., GPU_RB_DEPTH16)`: 1 MiB colour plus 512 KiB depth, beside the screen targets' about 1 MiB (arithmetic; [VRAM table](rendering-2d.md#vram-or-linear-memory)).
2. The cache covers a window of `512/TILE` tiles square. When the visible tile range is no longer inside it, the window is re-centred on the camera and every tile in it is drawn into the texture, with water drawn as its first frame.
3. Each frame the screen gets one quad of the cache at `(window origin − camera)`, then the visible water tiles over it, then sprites and HUD.

By counting quads (arithmetic): the direct path submits up to 416 tile quads per frame; the cached path submits 1 plus the visible water tiles, and a redraw frame submits 1,024 more. With 3 tiles of slack on each side at 16 px, 2 pixels per frame forces a redraw about every 24 frames. Whether it wins in time is [E2](#e2-cached-background-versus-direct-tiles). Anything animated or sprite-like must stay out of the cache. A 1,024×1,024 cache would need 4 MiB colour + 2 MiB depth, all of the 6 MiB VRAM, so it does not fit beside the screen targets (arithmetic). Switching render target mid-frame has a cost ([draw-call costs](gpu-rendering.md#what-a-draw-call-costs)), so the redraw happens before the screen pass. The cached quad is wider than the screen (512 against 400) and the hidden part is clipped; its fill cost has not been measured. After a HOME suspend or sleep the cache is marked invalid by an APT hook, because whether VRAM contents survive is [not established](../application-lifecycle.md#open-questions).

## Per-phase timing

Every frame is split into phases with `svcGetSystemTick()` (268,111,856 ticks per second, [system tick](../../../glossary.md#system-tick)):

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

**Compare strategies with `build + record + frameend`** (the summary line `mean_cpu_build_record_frameend_us`), never `c3d_proc` alone, because `C3D_FrameEnd(0)` flushes the whole linear heap and `C3D_GetProcessingTime` stops before that ([citro3d's two timers](../performance.md#citro3ds-two-timers), [drawbench](../../benchmarks/README.md#drawbench)). The interactive overlay (absent in `BENCH_AUTO` builds) shows the same CPU numbers averaged over the previous 30 frames, because drawing the text is itself part of the frame being measured.

## Experiments

Nothing here has been run. Each experiment names its variables, metrics, build lines and the rule that turns numbers into a decision. Only the listed macros vary; everything else stays at its default. The table that places these experiments among all the design choices is [Design alternatives and how to choose](rendering-2d.md#design-alternatives-and-how-to-choose).

**Protocol for all of them.** Old 3DS first (the primary target), then a New 3DS as a separate data set. Fresh boot, close other homebrew, detach the debugger, no game plugins ([benchmark README](../../benchmarks/README.md#before-you-start)). Build with `BENCH_AUTO=1`, run from the Homebrew Menu, wait for "bench done", press START, copy `sdmc:/coffeepot-tilerenderer.csv` (opened for append: delete it between sessions, or keep it; each run starts with its own `meta` block). At least three runs per variant. **Discard** a run that has any `error,` line, `dropped_objects` above 0, or `cmdbuf_peak_pct` of 98 or more. Per run use the median of each phase; report the spread across runs. Emulator output is never a result.

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
| Decision | Per-switch cost ≈ (mode 1 − mode 2) ÷ (N − 2), for CPU (`build + record + frameend`) and GPU (`c3d_gpu`) separately, per `TILE`. Mode 2 against mode 0 isolates the cost of merely having two textures of equal content. Compare with [drawbench](../../benchmarks/README.md#drawbench) `+texswap`, which measures the same thing one level lower (citro3d, 8×8 textures) |

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

`BENCH_AUTO=1` writes the same kinds of lines as [`bench.h`](../../benchmarks/README.md#output-format-and-metadata) to `sdmc:/coffeepot-tilerenderer.csv` (append mode). The program is self-contained and does not include `bench.h`; it copies the field layout. One extra line kind, `summary`, carries scalars.

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

The extracted project was also built with its Makefile in the default configuration and strategies 1–3 with `BENCH_AUTO=1`; these builds packaged `.3dsx` files. No run was performed. The earlier configuration table means: "Builds": `arm-none-eabi-gcc` (devkitARM r68, GCC 16.1.0) invoked directly with `DEVKITPRO=C:\devkitPro`, `-D__3DS__ -Wall -Wextra -O2 -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -mword-relocations -ffunction-sections -fno-math-errno -std=gnu11`, include path `libctru/include`, linked to an ELF with `-specs=3dsx.specs -lcitro2d -lcitro3d -lctru -lm`; no warnings in any of the 30 configurations built (28 in a scripted matrix, plus `BENCH_AUTO=1 CMDBUF_SIZE=0x100000` and strategy 1 with `TILE=12` as spot builds). The shader was assembled with the installed picasso, converted with `bin2s` and assembled into an object that was linked in (the template `Makefile` does the same). Those earlier direct-compiler checks did not package a `.3dsx` or run the program.

## Sources

- [2D rendering](rendering-2d.md) — the setup, depth, texture-layout and batching facts this program uses, the design-alternatives table the experiments belong to, and the validation checklist.
- [citro2d](https://github.com/devkitPro/citro2d) — installed 1.7.0 headers (`base.h`, `sprite.h`, `text.h`) for every call used, including the circle-mode notes on the ellipse and circle functions.
- [citro3d](https://github.com/devkitPro/citro3d) — `renderqueue.c` (frame functions, `C3D_FrameCounter`, render-target creation), `texture.c`, installed headers `attribs.h`, `buffers.h`, `effect.h` for the manual draw state.
- [libctru](https://github.com/devkitPro/libctru) — `hid.h` (`hidCircleRead`), `os.h` (`SYSCLOCK_ARM11`, `svcGetSystemTick`), `services/apt.h` (hooks), `allocator/vram.h`.
- [3ds-examples](https://github.com/devkitPro/3ds-examples) — `graphics/gpu/2d_shapes/source/main.c` (circle-grouping comment, read locally).
- [CTR benchmarks](../../benchmarks/README.md) — the `C3D_FrameEnd` timing caveat and the record format this page copies.
