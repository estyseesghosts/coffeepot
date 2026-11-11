# Case study: Super Mario 64 DS Enhanced (3DS port of the DS original)

A native 3DS port of Super Mario 64 DS — the DS game, not the N64 one, and not the N64 `sm64_3ds` port in [case studies: large ports](case-studies-ports.md). From tangosdev's DS decompilation and PC port plus a 3DS renderer and host (`port3ds/`), shipped per console as `.cia`. Logic runs at 30 Hz; interpolated frames present 60, with stereo 3D, 2x anti-aliasing and analog controls.

## Before you start

- Port facts are `[documented]` in the file named; comment timings are `[reported]`. Coffeepot ran nothing.
- Old 3DS is the default; **New 3DS:** differences follow. `--option console=old` bakes Old 3DS defaults in.
- Companion pages: [porting](../porting.md), [performance](../performance.md), [memory and cache](../memory-and-cache.md), [threading](../threading.md), [GPU rendering](../PICA200/gpu-rendering.md).

Jargon: the **[ARM11](../../glossary.md#arm11)** is the 3DS CPU (two cores; **New 3DS:** four); **[FCRAM](../../glossary.md#fcram)** is main memory (~64 MB for games; **New 3DS:** up to 124 MB); the **[linear heap](../../glossary.md#linear-heap)** is contiguous GPU-readable memory; **[PICA200](../../glossary.md#pica200)** is the 3DS GPU; a **[draw call](../../glossary.md#draw-call)** draws triangles with the current state.

## What makes it tick: the DS geometry engine replayed on the PICA200

The DS geometry stream is decoded to triangles under exact DS rasterizer rules and drawn via citro3d: winding cull, alpha discard, vertex-colour textures, translucent second pass, DS depth rules, fog, overlays (`src/gpu3ds.h`) `[documented]`. Two paths: `draw` submits decoded triangles; `draw_record` runs the DS transform, clip and cull on the GPU (`gx_rec.v.pica`), falling back when a record does not fit `[documented]`.

## Sixty frames from thirty ticks: interpolation

Logic is untouched at 30 Hz; smoothness is interpolated in-between frames. `interp3ds` blends each record's ids with the previous tick's (plus morphing vertices); pairing runs on the present worker at 3-6 ms a tick (`card3ds.cpp`) `[reported]`. Dry frames skip work when behind (`GAME3DS_DRY=1`, Old 3DS default) `[documented]`. Options expose 60 FPS, the counter (green 55+, yellow 28+), anti-aliasing, analog controls (`options3ds.cpp`) `[documented]`.

## ARM11 use: the present runs beside the next tick

Frames build one ahead: a worker presents while the game thread ticks on (`card3ds.cpp`), sharing two job slots `[documented]`. The worker runs at game priority on core 2 — **New 3DS:** a core the Old 3DS lacks — below the audio mix; on Old 3DS it runs on core 0 one above the game thread `[documented]`. The ~7 ms bottom-screen scan moves to a double-buffered worker, landing one frame behind; menus stay inline (caches not thread-safe) with a join before the compositor (`sub_async.cpp`) `[documented]`. A watchdog above game priority reports hangs (4 s) and flushes the SD log itself, since a spinner starves the log writer (`watchdog3ds.cpp`) `[documented]`.

## The PICA200 picture: anti-aliasing, stereo and overlays

Anti-aliasing renders the scene 2x and averages down on resolve (~1 MB plus 0.8 MB of VRAM, kept once made); stereo draws once per eye shifted except at screen depth; 2D sits at screen depth - no anti-aliasing while 3D is on (`gpu3ds.h`) `[documented]`. The DS 2D picture arrives as overlays with kept-frame rules (at most 4 a frame, each its own texture), reused across frames so an in-between need not re-upload (`frame_keep_overlays`) `[documented]`. Overlay uploads for the GPU transfer engine come from linear memory (`sub_async.cpp`) `[documented]`.

## Streaming and draw distance on FCRAM budgets

No city to stream, but the port still budgets like one. Assets arrive via manifest and `files_pack.cpp`, reading files up to 1 MB whole on open `[documented]`. A New 3DS launch sees ~110 MB free, while libctru's default 24/24 MB split leaves too little heap on Old 3DS, so `early_io.cpp` installs its own split with a `kOldLinear` linear heap for GPU use, capping emulator tests at retail 64 MB (`game3ds_old_budget`) `[documented]`. Every scene change frees caches, decoded textures and waves - else 1-2 MB leak per scene (`main_game.cpp`) `[documented]`. Draw distance stays fixed by DS design (fog, far clip as uniforms): a longer far plane would grow buffers, overdraw and residency against 64 MB `[hypothesis]`.

## Technical details

- Present worker: 64 KB stack; waits named in `g_card3ds_wait` for the watchdog `[documented]`.
- Sub-screen worker: two linear buffers; skips the 192 KB copy; 30 Hz screens halve scans when joins lag `[documented]`.
- Slow-frame log 250 ms; counters 55/28; stereo hysteresis 0.03; boot trace to `sdmc:/sm64ds3ds/game3ds_early.log` `[documented]`.

## Lesson table

| Symptom | Likely cause | Fix to try first |
| ------- | ------------ | ---------------- |
| 1-2 MB lost per scene | Textures, waves survive | Free caches, textures, waves at scene end |
| Bottom scan ~7 ms inline | Scan-out on game thread | Worker with double buffers; join before compositor |
| GPU record path draws nothing | Record too big | `record_fits`, else CPU triangles |
| Heap exhaustion only on Old 3DS | Default 24/24 MB split | Custom split, sized linear heap, test at 64 MB |
| Log stops in a hang, music plays | Spinner starves log writer | Watchdog above game priority, own SD flush |
| Pressure to extend draw distance | Budgets fixed together | Keep DS fog, far clip |

## Open questions

- No `[measured]` frame-rate series from hardware exists; the "24 fps on Old 3DS" comment describes dry-frame pacing.
- CPU triangles or `draw_record`: which wins per scene? `GPU3DS_REC_PROF=1` would tell.

## Sources

- `private_please_gitignore/sm64ds-enhanced/`: README, `docs/BUILDING.md`, `port3ds/src/`, `port3ds/game/`, `port3ds/shaders/gx_rec.v.pica`.
- [CTR CPU](../../hardware/cpu.md), [CTR memory](../../hardware/memory.md).
