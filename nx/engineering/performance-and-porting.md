# NX performance and porting

On Switch the main processor (the **[CPU](../../glossary.md#cpu)**) usually runs out first, not the graphics chip (the **[GPU](../../glossary.md#gpu)**). This page is for someone bringing a PC or older-console game to Switch whose frame rate collapses even though the scene looks simple. Short answer: lock clocks first, keep a PC frame-exact reference, change one thing at a time behind a flag, then work the payoff order below — and port like an applet-memory, pre-converted-data, 48 kHz title from day one.

## Before you start

- Toolchain: devkitA64 via devkitPro pacman `switch-dev`, with libnx and deko3d. Versions checked against latest `switch-dev` at time of writing (exact commits not pinned for this draft).
- Applies to retail Switch homebrew titles; clock figures assume handheld/docked retail clocks unless stated as overclock.
- A fixed 60 Hz step with accumulator (see below) is assumed; examples are illustrative and **not run on hardware**.
- Read alongside [rendering](rendering.md) for batching and texture work and [NX CPU](../hardware/cpu.md) / [NX GPU](../hardware/gpu.md) for clock tables.

## Lock clocks before measuring anything

What to do: fix CPU/GPU/memory clocks for the test session so numbers repeat. The stock scheduler moves clocks with load and heat, which hides real gains. Profile with retail content mods off: disable LayeredFS interception, cheats, and overclock sysmodules, and note the exact NRO, scene, launch mode (applet vs. title override), and clocks with every number [reported] (community Switch port practice).

- Expected result: repeated runs of the same scene agree within noise; a later speedup is real [reported].
- How it fails: unlocked clocks make every profile a weather report. If the same build reports 15 ms then 19 ms with no change, clocks moved. If a "gain" disappears with LayeredFS/cheats off or on a second console, the setup changed, not the code.

Why this matters here: one port report shows game logic taking 19→15 ms and render submission 17→11 ms while the GPU took only 7–9 ms at 307 MHz — the CPU was the limiter [reported] (community Switch port notes, conditions as stated in report). A second report reaches 60 fps only with a CPU/GPU overclock around 1785/768 MHz [reported] (community Switch port notes, overclock, conditions as stated). Label overclock numbers as non-retail; never bake them into asset budgets.

Why this matters here: one port report shows game logic taking 19→15 ms and render submission 17→11 ms while the GPU took only 7–9 ms at 307 MHz — the CPU was the limiter [reported] (community Switch port notes, conditions as stated in report). A second report reaches 60 fps only with a CPU/GPU overclock around 1785/768 MHz [reported] (community Switch port notes, overclock, conditions as stated).

## Work the payoff order, one flag at a time

What to do: keep a PC-first frame-exact reference build, put each optimisation behind a runtime flag, and apply in this order: profile-guided optimisation (~15% in reports), link-time optimisation plus hot-code-first layout, large 2 MB pages (~4.3% in one spot plus 47 MB cost in reports), a lock-free queue feeding the render submission, and finally cut per-draw setter cost.

```sh
# Shape only — flag names vary by toolchain, not run on hardware.
CFLAGS_PGO_GEN="-fprofile-generate"   # 1. instrument, run reference scene
CFLAGS_PGO_USE="-fprofile-use"        # 2. rebuild with profiles (~15% [reported])
CFLAGS_LTO="-flto -ffunction-sections -fdata-sections"
```

```c
// One change, one flag: large pages, lock-free hand-off.
// Illustrative — not run on hardware.
bool use_large_pages = getFlag("use_large_pages");
bool use_lockfree_queue = getFlag("use_lockfree_queue");
```

- Expected result: each flag shows its own delta against the same reference scene; PGO around 15%, large pages a few percent in hot spots at a memory cost, each as quoted [reported] (port notes, conditions as stated; not reproduced here).
- How it fails: stacking five changes at once teaches nothing. If the flag-off build does not match the reference frame for frame, the reference drifted — fix that before profiling.

## Split sim, render, and audio across three cores

What to do: the app sees three CPU cores (four ARM cores minus one reserved for the system). Give simulation, render-submission, and audio their own threads, pass frames through the lock-free queue, latch input once per tick, and step simulation at fixed 60 Hz with an accumulator capped at 5 steps. Pin guest threads explicitly — time-slicing at the same priority is ~10 ms, so unpinned threads alias — and make sure every thread is registered in the runtime's thread table or only the main thread actually moves [reported] (community Switch port notes, conditions as stated).

```c
// Fixed-step accumulator, max 5 catch-up steps; render interpolates.
// Illustrative — not run on hardware.
double acc = 0; const double DT = 1.0/60.0;
acc += frameTime; int steps = 0;
while (acc >= DT && steps < 5) { latchInput(); stepSim(DT); acc -= DT; steps++; }
if (steps == 5) acc = 0; // drop backlog, don't spiral
submitRender(interpolationAlpha);
```

- Expected result: steady 60 Hz logic with at most 5 catch-up steps; render and audio never block simulation [reported].
- How it fails: uncapped catch-up spirals after one hitch. If a single slow frame causes seconds of slow motion, the cap is missing.
- Collisions stay simple: axis-separated box overlap tests plus short grace timers for jumps and buffered presses (coyote time and input buffering) as used in the reports [reported].

What to do: drive audio from its own thread keyed on the audio release event (`audrvUpdate` per pump), never synthesized in the sim loop, with assets authored at 48 kHz, short effects resident in the pool, and music streamed [documented] (libnx `audren` / `audout` examples). Poll battery/power state at ~1 Hz on a worker or on state-change events, not per frame.

```c
// Fixed-step accumulator, max 5 catch-up steps; render interpolates.
// Illustrative — not run on hardware.
double acc = 0; const double DT = 1.0/60.0;
acc += frameTime; int steps = 0;
while (acc >= DT && steps < 5) { latchInput(); stepSim(DT); acc -= DT; steps++; }
if (steps == 5) acc = 0; // drop backlog, don't spiral
submitRender(interpolationAlpha);
```

- Expected result: steady 60 Hz logic with at most 5 catch-up steps; render and audio never block simulation [reported].
- How it fails: uncapped catch-up spirals after one hitch. If a single slow frame causes seconds of slow motion, the cap is missing.
- Collisions stay simple: axis-separated box overlap tests plus short grace timers for jumps and buffered presses (coyote time and input buffering) as used in the reports [reported].

## Measure the right thing, briefly

What to do: time CPU submission and GPU busy separately with sparse GPU timestamps; compare bind versus draw cost per pass. One third-party report quotes roughly 3 µs binds and 2.5 µs draws on an alternate driver stack [reported] (conditions as stated, not reproduced) — use it only as a "binds are not free" reminder, not a budget.

```c
// Time one pass at a time, then remove the timers.
// Illustrative — not run on hardware.
beginGpuTimestamp(passA); drawPassA(); endGpuTimestamp(passA);
```

- Expected result: you can name the longest pass and whether CPU or GPU owns it [documented] (timestamp mechanism) plus [reported] (figures above).
- How it fails: always-on timers inflate results. If removing timers speeds up the pass, the timers were the cost.

Maxwell notes that shape shader and memory choices on this GPU generation (**[Maxwell](../glossary.md#maxwell)**): merge memory accesses, prefer cached loads, aim for high occupancy (up to 64 warps per multiprocessor, 255 registers per thread, 32 blocks per multiprocessor as limits), shared memory 64 KB per unit on this chip with 48 KB per block and ≤32 KB recommended, keep shaders register-lean, use packed half-precision where verified (explicit `half2` paths — the vendor compiler does not reliably auto-pack [documented]), and compress plus mipmap for the shared bus whose docked peak is 25.6 GB/s with the CPU (handheld lower) [documented] (NVIDIA Maxwell tuning guide; Tegra X1 shipping-clock analysis).

## Compare honestly, one hypothesis at a time

What to do: keep a PC-first frame-exact reference, change one thing behind one flag, and re-measure both sides on the same scene, same NRO, same clocks, with screenshots plus draw counts — key-only changes still count as changes [reported] (community Switch port practice). After two failed runs on the same hypothesis, close the hypothesis instead of adding a third fix on top.

What to do: follow the acceptance ladder — device clear, then textured draw, then synthetic packet, then recorded real 2D/3D packet, then live title and field, then saves and transitions, then repeated hardware frame-times. Emulator, overclock, no-wait, or effects-off numbers never pass a rung [reported] (Switch homebrew postmortem: emulator field timings in the seconds-per-frame range were diagnostic only).

- Expected result: each flag shows its own p50/p95/p99 plus hitch counts against the reference; "faster but wrong" is caught by the frame-exact check [reported].
- How it fails: stacked changes with nested overlapping buckets summed as if exclusive, or `-O0` debug modules mixed into "optimized" builds. If the flag-off build does not match the reference frame for frame, the reference drifted — fix that before profiling.

## Port like a Switch title from day one

What to do: run through this checklist before optimising further.

1. Always request full memory via title override (replacing a full game slot instead of the small applet slot), or the title dies with out-of-memory in the small applet sandbox [reported].
2. Ship read-only game data as pre-converted binaries — no PNG/JSON decoding at runtime [reported].
3. Mix audio at 48 kHz; keep short effects in memory and stream music [reported].
4. Write versioned saves via temp-file plus backup so a crash never wipes progress [reported].
5. Choose the sync-to-screen (the short pause between screen refreshes, or **[V-blank](../../glossary.md#vblank)**) tradeoff on purpose: relaxed sync tears visibly but keeps input fresh; strict sync drops whole frames but stays clean [reported].
6. Handle lifecycle from day one: gate the loop on the applet status call, pause audio and stop submission on focus loss/suspend, and re-pick resolution and budgets on handheld/docked transitions instead of polling per frame [documented] (libnx applet behavior; every switch-example gates on it).
7. Keep the network off gameplay: init sockets once, use non-blocking calls or a dedicated net thread with timeouts, and check link status before traffic [documented] (libnx `bsd`/`nifm` behavior). Debug logging goes over a non-blocking socket — one project froze at ~37 s because a blocked send stalled the game [reported].

```c
// Versioned save shape — write temp, rename, keep backup.
// Illustrative — not run on hardware.
writeFile("save.tmp", data); fsyncFile("save.tmp");
renameFile("save.dat", "save.bak"); renameFile("save.tmp", "save.dat");
```

- Expected result: no applet out-of-memory kills, short loads, clean audio, crash-safe saves [reported].
- How it fails: applet mode plus runtime PNG decode plus strict sync gives long loads, low frame rate, and stutter together. If any checklist item is skipped, fix it before blaming the renderer.

## Technical details

- Clock evidence: game 19→15 ms, render 17→11 ms, GPU 7–9 ms at 307 MHz [reported]; 60 fps only at ~1785 MHz CPU / ~768 MHz GPU overclock [reported]. Both state CPU as limiter on their scenes; do not extend to other scenes.
- Payoff figures: PGO ~15%, large 2 MB pages ~4.3% in one hot spot at +47 MB, lock-free render queue, per-draw setter cuts — all [reported] from port notes, conditions as stated.
- Threading: app-visible cores are three of four ARM cores (one reserved); sim/render/audio split with latched input and accumulator cap 5 is the reported stable pattern [reported]; fixed step DT 1/60 s.
- Audio/save/ROM data rules (full memory via title override, pre-converted data, 48 kHz on its own thread, temp-plus-backup saves off the frame path, sync tradeoff) are [reported] port practice, consistent with libnx applet and filesystem behaviour [documented] (libnx sources).
- Maxwell limits above are [documented] (tuning guide); the 25.6 GB/s figure is the docked shared CPU+GPU peak, handheld lower.
- Port extensions worth stealing: snapshot deltas with verify keys instead of whole-device copies; content-hash store-cards (or sampled plus staggered full-hash fallback with a staleness caveat for glyph atlases); depth-target clipping (skip binding a depth target smaller than color when depth test is off — emulators will not catch it); compile-queue priority plus pipeline prewarm with fsync'd DKSH records; wake/lock batching counts — all [reported] (community Switch port notes, conditions as stated).

## Pitfalls

- Profiling with unlocked clocks: symptom is irreproducible deltas; fix by pinning clocks for the session.
- No frame-exact reference: symptom is "faster but wrong" or drifting demos; fix with a PC-first reference and flag-gated changes.
- Uncapped accumulator: symptom is death spirals after hitches; fix with max-5 cap and backlog drop.
- Applet memory mode: symptom is early out-of-memory on real hardware but not on PC; fix with title override requesting full memory.
- Runtime asset decode: symptom is long loads plus hitches; fix by pre-converting at build time.
- Strict-vs-relaxed sync confusion: symptom is blaming tearing on shaders or drops on input; fix by choosing the tradeoff explicitly per title.

## Open questions

- Do the PGO/large-page deltas hold on retail (non-overclock) clocks for a minimal homebrew scene? Needs same-scene flag on/off runs on hardware with pinned clocks.
- What queue depth keeps three cores fed without extra latency? Needs a sweep of 1–4 frames of queue against input-latency measurement.
- Which Maxwell shared-memory/register settings survive the driver on current deko3d? Needs a small compute-shader sweep with hardware counters.

## Sources

- [libnx](https://github.com/switchbrew/libnx) — applet memory modes, clocks, threads, audio [documented].
- [deko3d](https://github.com/switchbrew/deko3d) — submission, fences, image layout [documented].
- [NVIDIA Maxwell tuning guide](https://docs.nvidia.com/cuda/maxwell-tuning-guide/) — coalescing, caching, occupancy, shared memory, FP16, bandwidth [documented].
- [SwitchBrew](https://switchbrew.org/wiki/Main_Page) — clocks, memory-layout, HID, service background for figures above.
