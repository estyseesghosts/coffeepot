# Case study: Wind Waker HD port (SwitchWakerHD, reported results)

An unofficial native port of the Wii U Wind Waker HD to Horizon homebrew, forked from a static-recompilation project. PowerPC is recompiled to C ahead of time, the Cafe OS and GX2 are reimplemented natively, and graphics go through deko3d. There is no CPU emulation and no GPU-command emulation [reported]. None of the numbers were measured by Coffeepot; all are `[reported]` from the project's README and docs at commit `9aec4e307a15a8f19fe15e8b19890d9403213220` (2026-11-09; see [source versions](../source-versions.md)).

## When is this useful?

Read this for a worked example of barrier hygiene, shader preparation and disciplined hardware profiling on `deko3d`. Its recompiled draw stream is specific to that project.

## Before you start

- Build path [reported]: game dump to extracted files, recompiler (~39,713 functions, ~171 MB C), CMake plus devkitA64, NRO (~39 MB). Data, generated code, and shader caches stay on the player's machine and SD and are never committed.
- Launch: title mode (hbmenu override or forwarder); applet mode has far too little memory [reported].
- Companion pages: [rendering](../rendering.md), [performance and porting](../performance-and-porting.md).

## What it gets right

1. **deko3d only, no GL detour.** An earlier OpenGL path was removed once an offline-shader prototype proved the game's GLSL could be compiled with uam on-console and ahead of time [reported].
2. **Three-layer shader-burst defense.** Background compile list (~73.6% of whole-game pairs in ~19 min, gentle core-0 mode keeps ~30 fps), warp-based Prepare Graphics (~93%), and per-release sealed shader templates (~23,000 shaders built on PC from the player's own dump) [reported]. First-start stutter is attacked, not accepted.
3. **Per-draw CPU cut by ~1/5.** Heaviest view went 22 to ~29 fps: skip unchanged state, cache render-target and texture lookups, lay out shader data for cache, process ~half the GX2 commands, submit in larger batches [reported].
4. **Barrier and bandwidth hygiene.** Index lists expired only on attribute invalidates; native fan/quad/strip/loop paths; lazy fragment barriers (47 to 20 per frame in traces); depth-cull kept across the same depth target (12 to 5 invalidates); depth sampled in place (kills a 1280×720 blit plus 2 full barriers); shadow draws with no-op pixel shaders skipped; tiled cache and depth compression deliberately off (unverified) [reported].
5. **Player defaults and measurement.** Debug server, counters, diagnostics, captures, clocks, and mods ship off; `perf_test.py` compares against main or last tag over 4 interleaved boots (~35 min) with clock lines, per-5 s resource lines, and a hitch profiler [reported].
6. **Static upload plus rings.** Static geometry uploaded once, camera and uniforms via per-frame ring with fences — never wait-idle per frame, never overwrite GPU-live memory [reported].
7. **Resolution and clocks stated.** Handheld 720p, docked 1080p at 1.5×, auto switch on dock, dynamic resolution only below 30 fps; stock CPU 1020 MHz with handheld GPU on the 384 MHz profile (307 to 384 measured +4.7 fps in a grass scene) [reported].

## What it gets wrong (limits and traps)

- **Memory-mode assumption is a support trap.** Title mode or forwarder required; applet out-of-memory is the newcomer's first crash [reported].
- **Docked and handheld tiers are thin.** Single 384 MHz handheld profile and a present-scaled docked path; no full tier matrix [reported].
- **No clean atlas or chunk story.** Inherited Wii U submission limits the ceiling; texture unpack 5.5 to 1.7 ms and translation-record caches only mitigate [reported] — a new game should design chunked static meshes from day one instead.
- **Bespoke profiling.** Menu A/B switches, dev vars, debug-server warps and shots; no generic new-game profiler to reuse yet [reported].

## Lesson: build like the walkthroughs, operate like the port

Ship player defaults off; measure before claiming (interleaved boots, clocks, hitch profiler); precompile shaders (offline plus on-console cache), upload static data once, ring the dynamic data with per-slot fences, and batch submits. Atlases, culling, and compression plus mipmaps are common suggestions that this port does not itself quantify `[hypothesis]`. Add the port's discipline: sealed shader cache, dynamic resolution, lazy barriers with depth-cull retention. Do not copy its burden: recompiled draw stream, harvest-every-shader workflow, title-mode-only memory budget.

Input, audio, and saves follow the project's own choices: controllers as Off-TV Play, console touch as GamePad touch, gyro with recalibration, versioned saves with backup and temp-file plus flush plus rename on SD [reported]; these are one project's decisions, not Switch requirements.

## Open questions

- Exact audio backend and save atomicity guarantees on FAT?
- Docked 1080p GPU-busy delta vs handheld on the same scene?
- Which memoisation wins survive upstream syncs?
- Is profile-guided optimisation safe to ship?

## Sources

[SwitchWakerHD](https://github.com/centollOS/SwitchWakerHD) commit `9aec4e307a15a8f19fe15e8b19890d9403213220`. Many of its documents are written in Spanish; figures were checked against these files:

- `README.md` — title-mode launch (hold R while starting any installed game), build path.
- `docs/switch-port.md` — recompiler output (39,713 functions, ~171 MB of C; line 45), per-draw CPU cut (Outset 22 to ~29 fps; line 1501).
- `docs/background-shaders-plan.md` — 73.6% of vertex/pixel pairs, 74.4% of shaders (line 47).
- `docs/deko3d-plan.md` — `WWHD_DK_LAZY_BARRIERS` (47 to 20 Fragments barriers per frame in traces; line 87).
- `docs/perf-results.md` and `docs/performance.md` — interleaved boots, 384 MHz GPU profile (+4.7 fps in one grass scene), per-scene results.
- The earlier text also cited the "Switch 2D-platformer, 2.5D-game and 3D-game walkthroughs" for patterns. Those three files (`2dplatformer.md`, `25dgame.md`, `3dgame.md` in the research folder) are unreviewed tutorial text, not measurements; claims taken from them are `[hypothesis]`, not `[reported]`.
