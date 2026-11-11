# Case study: SA2 Switch port (reported results)

A native Nintendo Switch port of the Xbox 360 version of Sonic Adventure 2. It statically recompiles the PowerPC game to C++ ahead of time, then draws natively through Renut on deko3d. This page summarises what the project's own docs say, with the scene and conditions each figure was taken on. None of the numbers were measured by Coffeepot; all are `[reported]`. It ends with what to copy first on a Tegra X1 homebrew renderer.

## Before you start

- Evidence: every figure below is `[reported]` by the sa2-nx project (README and `docs/`). Coffeepot has run no benchmark and no code from this project.
- Reference scene: City Escape, ~1,400 draws per frame, CPU at 1020 MHz unless stated otherwise.
- Renderer history: native Vulkan on NVK up to 1.0.2.1, Renut on deko3d from 1.0.3. Resolution 1280×720 handheld, 1920×1080 docked from 1.0.3.1; MSAA off by default.
- Companion pages: [rendering](../rendering.md), [performance and porting](../performance-and-porting.md), [NX GPU](../../hardware/gpu.md).

Jargon used here: the **[CPU](../../../glossary.md#cpu)** runs game and render-submission threads; the **[GPU](../../../glossary.md#gpu)** draws; a **draw** is one batch of geometry sent to the GPU; **PGO** is profile-guided optimisation (rebuild the game using profiles from a real run); **LTO** is link-time optimisation; a **DKSH cache** is deko3d's precompiled-shader store.

## What it is

| Layer | Approach |
| ----- | -------- |
| CPU | Ahead-of-time static recompilation (PowerPC to C++), then clang/GCC. No interpreter at run time [reported]. |
| GPU, to 1.0.2.1 | Native Vulkan renderer: reads the PM4 ring, models the register file and EDRAM, draws with translated shaders [reported]. |
| GPU, 1.0.3 on | Renut renderer on deko3d: hooks ~20 Direct3D entry points, reads device state at each draw, replays on one render thread. No PM4 ring or Vulkan driver [reported]. |
| Shaders | Translated ahead of time to GLSL packages; compiled on-console once with uam and cached to `deko3d_shaders.bin` [reported]. |

## What it gets right

| Technique | Reported effect | Conditions as stated |
| --------- | --------------- | -------------------- |
| Renut on deko3d replacing NVK | Game thread 19.2 ms to 15.5 ms; render thread 17.6 ms to 11.1 ms; GPU 7–9 ms at 307 MHz, 5–6 ms at 460.8 MHz | City Escape, 1020 MHz, ~1,400 draws, before new PGO profile |
| PGO from console session | ~15% faster (1.0.0); ~15% estimated again (1.0.1) | Released NRO built with `-fprofile-use`, shipped profile in `pgo/` |
| Snapshot deltas | 13,088 B to ~590 B per draw; later only 40 chunks compared, then ~6.5 chunks per draw with setter marks | 64 B chunks vs shadow; 0 of ~740,000 rebuilt devices differed |
| Store cards vs hashing | Content hashing 16.4% of render thread to 0% | 179,700 stores marked; 412,760 copies proven in 10 s, 0 changed under clean cards |
| Batching and wakes | Wake batch 16, capture batches of 32, one clock read per batch, waiter-only done signals | Each wake is a kernel call on both threads |
| Resolve, mip, and clear savings | 2 full-screen copies per frame removed via colour hand-over (~33 MB traffic at 1080p); lazy and direct resolve mips; repeated whole clears not sent | Per-shader GPU split put colour resolves at ~1/5 of GPU time |
| Shader-burst control | DKSH cache from pipeline list (100 variants prebuilt); fsync so closing the game does not lose records; 103 shaders compile with uam | List made from emulator run; first session otherwise stutters |
| Large pages | ~4.3% in one GPU-heavy spot at +47 MB (38.8 MB text + 6.3 MB rodata) | 2 MB blocks vs 4 KB pages, version 1.0.6 |
| Relaxed vsync, core pinning | Missed 60 fps frame shows at 50–59 fps instead of 30 fps; game main on core 0, render on core 1, others on 1–2 preferring 2 | 3 usable cores; priorities 0x3B time-sliced in 10 ms slices |

Single-player City Escape then held 59–60 fps (7–11 ms game-thread busy, 5–11 ms render, 600–1,500 draws); 2-player race ran ~38–54 fps, limited by game drawing code (~2,900 draws in the heaviest spot) [reported].

## What it gets wrong (costs and traps)

| Cost / trap | Detail |
| ----------- | ------ |
| NVK detour | ~3 µs per pipeline bind, 2.4–2.9 µs per draw, 1.3–1.6 µs per descriptor update on the A57 [reported]; dropping NVK cut the NRO 58.2 MB to 43.8 MB [reported]. |
| Emulator timing | PC 2.3–2.7 ms render vs 17.6 ms on console for the same scene. Only console profiles decide [reported]. |
| Snapshot and verify machinery | Delta chains, chain-reuse memos, setter scopes, lock-free 16,384-slot queue, every-32nd-draw full checks. First lock-free build regressed 28.7 ms to 32.4 ms before an ARM audit fixed prefetch and load/store ordering [reported]. |
| Overclock dependence | Heavy scenes (notably 2-player) still drop frames; CPU overclock is the documented recourse, with storage-corruption risk if unstable [reported]. |

## Lesson: what to copy first

| # | Symptom to fix |
| - | -------------- |
| 1 | Whole-device copy per draw (~19.3% of game thread) — compare 64 B chunks against a shadow, narrow to chunks the render thread reads, then to setter-marked chunks only; verify every rebuilt device byte-for-byte [reported]. |
| 2 | Re-hashing unchanged data (16.4%) — one card byte per 4 KB page cleared by every store path; prove clean cards instead of hashing, with a hash-everything verify mode [reported]. |
| 3 | Wake and kernel-call storm — batch wakes (16) and captures (32), one clock per batch, signal done only when a waiter exists; pin game and render threads to separate cores [reported]. |
| 4 | First-session shader stutter and 58.2 MB NRO — move GLSL out of the NRO (191 KB package via installer), prebuild the DKSH cache from a pipeline list, fsync it, compile on a background core [reported]. |
| 5 | 60-to-30 fps cliff on one slow frame — relaxed vsync (release at next ready time, never sooner than 1/60 s after previous); measure threads by CPU time not only fps [reported]. |

## Open questions

- DKSH cache coverage beyond the attract demo (Chao Garden, rest of game) was still pending at 1.0.3 [reported].
- A depth target smaller than the colour target is a Maxwell fault; SA2 never bound such a pair in 10 min but other games might [reported].
- Large-page (2 MB) gain beyond the one spot is unquantified for SA2 [reported].

## Sources

- sa2-nx `README.md` — what the port is, install, settings, performance summary, credits.
- sa2-nx `docs/porting-guide.md` — end-to-end method, traps, proving changes, shipping.
- sa2-nx `docs/deko3d-renderer-guide.md` — NVK-to-deko3d road, per-draw costs, checklist.
- sa2-nx `docs/renut-renderer.md` — hooks, device layout, items 1–43 with console measurements.
- sa2-nx `docs/cpu-rounds.md` — three CPU rounds, link and PGO, 2 MB blocks.
- sa2-nx `docs/gpu-optimisation-candidates.md` — Tegra X1 and NVK cost model and ranked candidates.
