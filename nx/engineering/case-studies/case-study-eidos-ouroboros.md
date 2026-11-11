# Case study: Eidos on Switch (Ouroboros) — a cautionary tale (reported results)

Ouroboros tried to ship a DS reimplementation as a native Switch title at stock clocks. The first attempt took the expedient path (desktop GL on Maxwell via Nouveau plus driver patches) and stalled on copies, not on the GPU. The second attempt broke cleanly to native libnx plus deko3d with a strict measurement protocol — the right direction — but with no stock-clock full-speed run yet. All figures are `[reported]` with conditions as stated; do not cite as fresh measurement.

## Before you start

- Attempt 1 (ouroboros-1): SDL3 plus EGL/Mesa/OpenGL ES on Maxwell via Nouveau, plus 5 local Mesa patches [reported].
- Mid-course profiling revolt (Nov 3): Mistralton field reference scene, no-wait, outlines off — **61.2 ms per frame (~16 game fps)**: render 39.2 ms with **fullscreen readback 22.5 ms**, main-2D 8.8 ms, UI ~6 ms, game 10.0 ms [reported]. A fence fix changed nothing (61.9 ms, same 22.5 ms readback, 0 late reads) — proving CPU-copy bound, not GPU wait [reported].
- Attempt 2 (ouroboros-2, Nov 6): clean break to native libnx plus deko3d with a versioned frame-packet ABI, GPU 2D and 3D paths, persistent residency, 3-slot frame queue, and an acceptance ladder [reported]. Stock-clock full speed explicitly **not yet established**; emulator timings are diagnostic only [reported].
- Companion pages: [rendering](../rendering.md), [performance and porting](../performance-and-porting.md).

## What the postmortem gets right

1. **Frame-packet ABI.** Describe DS hardware state as a versioned immutable packet — guest offsets vs host pointers vs GPU addresses never conflated, bounded sizes, hashes, explicit owner and fence. The backend consumes packets; it never reaches into mutable guest memory [reported].
2. **Ownership and fence discipline.** Command memory, staging, descriptors, and render targets live until their fence signals; finishing a command list is not GPU completion. Two cheats banned: wait-idle per frame and arbitrary barriers everywhere [reported].
3. **Measurement protocol.** One decisive hardware A/B per ticket: identical save, scene, clocks, and build hash, NRO hash, title-takeover launch (not Album applet), p50/p95/p99 plus hitch counts, simulation vs presented frames separated, exclusive (non-nested) buckets [reported]. No-wait measures throughput; only paced full-effects stock-clock runs pass acceptance.
4. **Acceptance ladder.** Gates 0–4: baseline integrity, ~100% GPU coverage with named fallbacks, full speed at 1× DS 3D, 2× 3D with 2D held native, rebase readiness. A fast wrong frame and a correct slow frame both fail [reported].
5. **One hypothesis at a time.** Symptom, owner and files, falsifiable mechanism, patch, proof, one hardware pair, verdict. Two failed comparable attempts close the hypothesis. No multi-stream patches, no emulator-loop mutation [reported].
6. **Scoped contrast.** The sibling byte-matched decomp project is cited as the model: fix interfaces, do not fork truth [reported].

## What the stack and habits got wrong

- **Wrong stack for Maxwell.** OpenGL-on-Nouveau plus a compatibility shim meant every frame paid translation and readback mapping plus a second full copy in the shim — before Maxwell did anything interesting. Five driver patches tuned the wrong layer [reported].
- **Fullscreen readback as the hot path.** Copying two GPU-composed screens (~400 KiB) to the CPU cost 22.5 ms per frame through uncached mappings — over a third of the 61 ms frame [reported].
- **Consumers forcing readback every field frame.** UI checks read pixels to answer yes/no questions (dark-top, busy-bottom, white-flash detection) plus menu-only fullscreen readers. A GPU-accelerated renderer with a downstream CPU pixel consumer every frame is a CPU renderer with extra steps [reported].
- **Effects-off numbers that could not ship.** Outlines-off, unpaced, high-clock, interior-vs-field substitutions produced better averages with ~1 s hitches and wrong visuals [reported].
- **Album-applet vs title-mode confusion.** The large heap requires title takeover; Album context is not an equivalent test. Memory, clocks, and launch mode must be logged or the run is invalid [reported].
- **Emulator-loop debugging.** Repeated NRO-to-emulator-to-crash-to-unrelated-GPU-edit cycles, with emulator fps quoted as hardware progress. Bring-up tests prove bring-up, not game frames [reported].

## Lesson: symptom to fix rules

1. **Fullscreen readback on a steady frame → serve a GPU summary.** Answer UI questions from scene flags or a tiny GPU reduction; full-screen CPU copy only as an explicit exceptional capture [reported].
2. **Nested buckets that "add up" → exclusive timing only.** Publish a nesting legend, mark exclusive time, and never conclude "GPU slow" from submission timestamps or readback time [reported].
3. **No acceptance without ladder.** Gate-0 manifest (tree, revision, NRO hash, clocks, scene) and gates 1–4; emulator, overclock, and no-wait runs never pass [reported].
4. **Stacked fixes with no verdict → bisect.** Revert to a known build, re-apply one file-owned change, one hardware pair, commit or revert [reported].
5. **Per-frame wait-idle or full barriers → scope the sync.** Per-slot fences, minimal pass barriers from actual read/write sets, full wait only at teardown, resize, and lifetime boundaries [reported].
6. **Keep the CPU oracle off the hot path.** A CPU compositor as reference and replayable packet harness; count GPU vs CPU frames and refuse to ship disabled fog, edge, shadow, or capture [reported].

The moral is short: Maxwell was never the bottleneck. The copies, the consumers, and the clocks were.

## Open questions

- What does the native branch measure on stock clocks in the Mistralton field scene? Pending a hardware run [reported].
- Which atlas size and queue depth hold under retail clocks for a DS-tile workload? Pending the same-scene sweep.
- How much of the frame-packet ABI survives contact with audio, save, and sleep handling? Untested at the time of writing.

## Sources

- bw-switch `EIDOS_SWITCH_ENGINEERING_GUIDE.md` and `docs/` — frame-packet ABI, ownership and fence discipline, measurement protocol, acceptance ladder.
- bw-switch ouroboros-1 tree (platform GL sources, Mesa patches, status and bottleneck notes) — wrong-stack costs, readback numbers, effects-off caveats.
- bw-switch ouroboros-2 tree (native libnx plus deko3d branch, packet and queue design) — clean-break direction, pending hardware runs.
