# Case studies: two demanding Switch ports (reported results)

Ultra Street Fighter IV and Need for Speed: Most Wanted are both static recompilations of Xbox 360 games for the Switch, built on ahead-of-time recompilation plus a native renderer. They took opposite paths once Vulkan-on-NVK became the limit: USF4 abandoned NVK for deko3d; NFSMW stayed on NVK and made it pay. Both trees document every step. None of the numbers were measured by Coffeepot; all are `[reported]`.

## Before you start

- Evidence: every figure below is `[reported]` by the named project. Coffeepot has run no benchmark and no code from these projects.
- USF4 target: locked 60 fps — frame pacing and input latency are correctness, not polish. NFSMW target: sustained ~30 fps while streaming an open city, at stock clocks with no overclock.
- Companion pages: [rendering](../rendering.md), [performance and porting](../performance-and-porting.md), [NX GPU](../../hardware/gpu.md).

## USF4: a 60 fps-locked fighter that outgrew NVK

**What it is.** Ultra Street Fighter IV recompiled to C++ ahead of time, drawn first by a native Vulkan renderer and from 0.8.0 by Renut on deko3d [reported].

**What it gets right.**

- **deko3d over NVK, A/B on one build.** Same Training fight, docked, CPU 1887 / GPU 844.8 / MEM 1862.4 MHz: native Vulkan render thread 18.6 ms (53.5 fps) at 720p vs Renut-on-deko3d 2.8 ms (59.9 fps) at 720p; NRO 66.7 MB to 56.3 MB with NVK unlinked [reported].
- **Depth-target clipping fix.** USF4 leaves a 320×180 depth target bound during full-screen 1280×736 passes; **[Maxwell](../../glossary.md#maxwell)** discards everything outside the smallest bound target, so only the top-left drew. Draws and clears that neither test nor write depth no longer bind the smaller depth target [reported]. Emulators never showed it.
- **Depth hand-over resolve kept.** Whole-target depth resolve sampled from the target image saves ~0.4 ms GPU per frame; first suspect in the clipping bug, bisected innocent, kept on [reported].
- **Static-after-60 plus staggered full hash.** Linear textures hashed 11.3 GB per 10 s (~19 MB per frame); after 60 unchanged full checks a texture gets sampled hash per frame plus full hash every 16 frames staggered by address: 11.3 to 0.7 GB per 10 s, render thread 6.4 to 2.8 ms per frame. Cost: a write missed by both hashes shows up to 16 frames late [reported].
- **Compile-queue priority plus prewarm plus fsync DKSH.** Waited-for compiles jump ahead of prebuild jobs; a pipeline list prebuilds known pipelines; the shader cache writes fsync before rename [reported].
- **Late-input limiter-slack plus double-buffer off.** The game's own limiter waits ~8 ms off-swap, so slack is counted at the limiter (16-frame minimum, +3 ms margin): input-to-display 34 ms to ~31 ms, 0 extra dropped frames in 150 s [reported].
- **First-fight shader burst quantified.** ~90 shaders at ~84 ms each is ~7.6 s on the first loading screen with empty caches; cached thereafter [reported].

**What it gets wrong.**

- **Overclock dependence.** Fights hold 60 fps only at CPU 1785 / GPU 768 MHz; stock clocks do not hold it. The README states this outright [reported].
- **NVK validation and descriptor overhead.** ~3 µs per pipeline bind, ~2.4–2.9 µs per draw on the A57; ~400–500 draws per frame saturated the ring thread at 18.6 ms while the GPU sat at 9.3 ms [reported].
- **Emulator-only bugs.** The Maxwell clipping and a log-socket freeze (~37 s, blocking send under mutex) never reproduced on PC; fixed with the smallest-target rule and a non-blocking log socket [reported].
- **Empty-cache comparison trap.** The first NVK-vs-Renut A/B ran the native renderer with no shader package beside the NRO: it drew 0 draws, presented black screenshots, and "won" at 60 fps. Rule adopted: screenshot plus draw count per presented frame before any comparison [reported].

## NFSMW: a streaming racer that stayed on NVK

**What it is.** Need for Speed: Most Wanted (2005) recompiled ahead of time, drawn by a native Vulkan renderer on NVK/Mesa. Target is stock clocks, no overclock: handheld CPU 1020 / GPU 460.8 / MEM 1331.2 MHz [reported]. Races average 32–35 fps; the heaviest spot runs 21–22 fps for ~20 s [reported].

**What it gets right.**

- **Native renderer replaced GPU emulation.** Emulated GPU: 1–3 fps in-race (~80 µs CPU per draw plus fault handling); native ring thread: ~1.5–1.8 µs per draw on PC; console stock-clock baseline 16.1 fps median [reported].
- **Submit and idle findings.** Two submits per frame left the GPU idle 3.6 ms per frame (each submit is IPC plus fence checks); rule: minimal submits per frame. Query-pool resets cut; resolve-without-copy saved 2.36 ms; ZCULL re-enabled in NVK saved ~2 ms scene time. Total GPU 60 to ~23 ms per frame [reported].
- **CPU, build, and native game functions.** Immediate-load shader caches, deduped uploads, no ring-thread logging, LTO plus PGO plus function ordering (~30.3 to ~34 fps); hottest frontend functions as guarded natives: 35.4 fps average, frames over 60 ms cut 24 to 7 [reported].
- **Streaming and pacing discipline.** Texture cache capped 512 MB (LRU 120 frames); 16 MB-block sub-allocation; presenter stays immediate; GPU timestamps corrected by ×1.627 (NVK period lies); A/B by alternating every N seconds in-session with a no-op control [reported].
- **First-race pipeline burst fixed by shipping the list.** ~59–82 ms per pipeline over 112 pipelines stuttered every fresh install; fixed by shipping a pipeline list plus background prebuild (113 of 113 in 0.3 s) [reported].

**What it gets wrong.**

- **Overclocked numbers hid the GPU limit for days** (34 fps overclocked vs 15 fps stock, bottleneck flipped CPU to GPU). Policy since: stock clocks from day one [reported].
- **Config overriding code defaults** silently disabled changes for several builds; now every log echoes effective settings [reported].
- **Counters that lied:** wall-clock "busy 97%" was 76% CPU plus 21% wait; cumulative-vs-interval mix; a 200-entry stutter log hiding second-half races; a guard reporting "0 conflicts" with the feature switched off [reported].

## Shared techniques

| Problem | USF4 answer | NFSMW answer | Transferable rule |
| ------- | ----------- | ------------ | ----------------- |
| Draw submission CPU | Drop NVK: deko3d user-space command words (2.8 ms vs 18.6 ms) | Keep NVK, cut per-draw cost and submits | Profile ring/render thread first; binds and submits dominate past ~400 draws |
| Hidden pixels | Depth hand-over resolve (~0.4 ms) | ZCULL in driver (~2 ms) | Early-z before shader cuts |
| Redundant copies | Consumer-tagged clear redirect | Resolve-by-swap (2.36 ms) | Never copy what a rename or stale-check avoids |
| Hashing and uploads | Static-after-60 plus staggered 1/16 hash | Fingerprint plus back-off to 1/32 frames | Trust-but-verify textures; stagger full checks |
| First-play stutter | Prebuild list plus DKSH cache, fsync | Pipeline list plus cache on SD | Ship the cache list, build it on a background thread |
| False measurements | Prove each side draws (screenshot plus draw count); same NRO, clocks, scene | In-session alternation plus no-op control; corrected GPU clock; pacing histograms | One variable, same log, pacing over mean |
| Latency vs throughput | Limiter-slack late input (34 to 31 ms); double-buffer off | Swapchain of 3 images; interval-2 only once 90% of frames fit 31 ms | Fighter: cut latency; racer: bound the tail |

## Open questions

- USF4 handheld (720p at 768 MHz) still unmeasured on console; expected hold from docked runs at same clocks needs a run [reported].
- Prebuilding the ~90-shader first-fight set from a shipped hash list during menus (no game data in NRO) is designed, not built, in both ports [reported].
- Erista-vs-Mariko thermal margin at these clocks is anecdotal in both trees [reported].

## Sources

- usf4-nx `docs/deko3d-renderer-usf4.md` — A/B table, clipping fix, hashing, latency, rig, traps.
- usf4-nx `docs/usf4-notes.md` — 0.1.0 to 0.8.1 timeline, ring limits, save-hitch, limiter fixes.
- usf4-nx `README.md` — 60 fps at 1785/768 claim, installer, 0.8.x update path.
- nfsmw-nx `docs/performance-history.md` — 1–3 to 32–35 fps timeline, stock-clock table.
- nfsmw-nx `docs/native-renderer.md` — ring design, per-draw pipeline, guards, texture and memory rules.
- nfsmw-nx `docs/mesa.md` — ZCULL, scheduling fixes, per-draw NVK cuts.
- nfsmw-nx `docs/platform-notes.md` — clocks, priorities, 2-submit idle, allocation limits, presentation caps.
- nfsmw-nx `docs/measuring.md` — timestamp correction, A/B alternation, pacing over mean.
