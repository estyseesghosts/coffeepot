# Case studies: three large 3DS ports (reported results)

Three public N64-to-3DS ports have written down what they changed and what it bought: a native renderer for F-Zero X (G-Diffuser), an optimized Super Mario 64 port, and research notes for an Ocarina of Time port. This page summarises what each source says, with the console model and conditions each figure was taken on. None of the numbers were measured by Coffeepot; all are `[reported]`. The page ends with a techniques table and a short list of what to try first on an Old 3DS.

## Before you start

- Evidence: every figure below is `[reported]` by the named project. Coffeepot has run no benchmark and no code from these projects. See [evidence labels](../../../AGENTS.md).
- Sources were downloaded raw and figures matched by text search on 11 November 2026 (see Technical details). The 10 -> 60 fps headline is first boot 10.4 fps (09-29) to a 60 fps median in 2D races (10-02), not one continuous run.
- Dates are written as the sources give them (month-day). The G-Diffuser page shows no year in the parts read; the year 2026 comes from the OOT document and the review that prompted this page.
- Old 3DS is the default model here. Where a source measured a New 3DS, or simulated Old 3DS speed on a New 3DS, that is stated in the row.
- Companion pages: [porting](../porting.md), [performance](../performance.md), [CPU optimization](../cpu-optimization.md), [memory and cache](../memory-and-cache.md), [threading](../threading.md), [audio](../audio.md), [frame pacing](../PICA200/frame-pacing.md), [GPU rendering](../PICA200/gpu-rendering.md), [testing on hardware](../testing-on-hardware.md).

Jargon used here: the **[ARM11](../../glossary.md#arm11)** is the 3DS CPU; a **[cache](../../../glossary.md#cache)** is small fast memory in front of main memory; the **L1 instruction cache (L1-I)** holds recently executed code; **CPI** is cycles per instruction (lower is better); a **display list** is the N64 game's command stream describing what to draw; **[PICA200](../../glossary.md#pica200)** is the 3DS GPU. CPI, L1-I and display list have no glossary entries yet.

## G-Diffuser: F-Zero X native renderer

**What it is.** A 3DS port of F-Zero X built on the libultraship Fast3D display-list interpreter, later given a native renderer that bypasses the interpreter for most race drawing. Postmortem: ["Road to 60, Again"](https://hypnotize.works/gdx-3ds/postmortem/native.html); repo document `docs/NATIVE-RENDERER.md` in [cruxxxxxx/gdx-3ds](https://github.com/cruxxxxxx/gdx-3ds).

**Models.** The postmortem reports Old 3DS hardware results (its timeline and the instruction-cache probe) and New 3DS results (a 59.6 fps median at the v0.9.0 baseline, reached only by running the renderer one frame ahead on a second core). Where a row below says "Old 3DS" the source says it was taken on that model.

### Reported starting point (v0.9.0, 09-04)

| Item | Reported value |
| ---- | -------------- |
| Hot code size | about 219 KB against a 16 KB L1-I |
| Old 3DS render cycles stalled on instruction fetch | 43.6% |
| Cost per clean citro3d draw / per game draw via interpreter (Old 3DS) | 10.9 us / 72-98 us |
| First Old 3DS boot | 96 ms frame (about 10 fps), GPU busy 3.6 ms |
| Interpreter lines run in a race at default settings after the native work | 0 of 10,466 |

### Instruction-cache probe (Old 3DS hardware, 09-30)

A synthetic probe, not a game workload. The source also reports a render-code CPI of 4.34 and an L1-I miss cost of about 37 cycles.

| Hot-code footprint | CPI |
| ------------------ | --- |
| 16 KB | 1.06 |
| 24 KB | 4.10 |
| 32 KB | 5.62 |

The postmortem itself describes the Old 3DS as "268 MHz, no L2 cache, 64 MB" and states "~219 KB of hot code against a 16 KB L1-I". That matches the review; hardware facts are on [ARM11 MPCore](../../hardware/arm11-mpcore.md). Verbatim check: the probe line reads "probe CPI 1.06 at 16 KB of hot code, 4.10 at 24 KB, 5.62 at 32 KB".

### CPU time per rendered 2D race frame (Old 3DS, 10-01 interpreter vs 10-02 native)

| Category | Before (ms) | After (ms) |
| -------- | ----------- | ---------- |
| Machines | 4.05 | 1.09 |
| Track | 4.13 | 1.35 |
| Sky | 3.02 | 0.40 |
| HUD | 5.14 | 3.17 |
| Effects, shadows, dressing (summed) | 3.37 | 1.72 |
| Still on interpreter | 0.82 | 0.86 |
| Sum as charted | 21.1 | 9.1 |

Per-tick game logic was 1.51 -> 1.58 ms (unchanged by design). A cheaper alternative tried first, replaying cached poses (REL4, 10-01), gave 20 rendered + 20 replayed images per second at 26.8 ms CPU per rendered frame and was abandoned.

### Old 3DS frame-rate timeline (race frames per second, as reported)

| Date | Build or condition | Reported result |
| ---- | ------------------ | --------------- |
| 09-29 | First boot | about 10 fps (p50 96.0 ms) |
| 09-29 | Audio lock held too widely, then narrowed | audio-lock wait 25.8 -> 0.2 ms; wall 88 -> 67.8 ms (11.3 -> 14.7 fps) |
| 09-29 | Frame skip plus an Old-3DS preset | logic 57-60 ticks/s, 19-28 fps drawn; the source notes users reported nausea at that rate |
| 09-30 | REL3 (async logging, see below) | about 20 fps rendered |
| 10-02 | 2D race, native renderer | median 16.5 ms = 60 fps, 0 race hitches |
| 10-03 | First stereo build | about 40 fps (3D race 19.8 ms vs 16.5 ms in 2D) |
| 10-03 | Right eye drawn in chunks | steady 3D about 48-57 fps (source estimate 52) |
| 10-08 | 3D, courses 12-16 | work about 15.5 ms, about 59-60 fps (estimate) |
| 10-24 | 1P 3D, 16 machines | 18.6 ms = 53.7 fps |
| 10-25 | 2D multiplayer (2 players + 14 CPU) | locked 60 fps; 3.8-4.1 ms vblank slack |
| 10-26 | 1P 3D, one submission for both eyes | 59.7-59.8 fps (baseline 56.4 / 59.3; "suggestive", two blocks per arm) |

Stated limits: multiplayer 3D is locked at 30 by design; 3D is not a locked 60 on either console; Old 3DS 3D is CPU-bound; two courses (Silence, Sand Ocean) can step down to 30.

> Conflict: the review summarises "approximately 59.7-59.8 FPS in tested stereo racing cases". The postmortem gives that range for one condition (1P 3D race, one-submission arms, 10-26) and reports other stereo cases lower (48-57 and 53.7 fps above). Do not read it as a general stereo figure.

### Right-eye / one-submission experiment (Old 3DS, 1P 3D race, 10-26)

| Arm | GX entries per frame | Completion interrupts | Race fps | Headroom |
| --- | -------------------- | --------------------- | -------- | -------- |
| Baseline | 990 | 550 | 56.4 / 59.3 | n/a |
| One submit for both eyes | 771 | 331 | 59.7-59.8 | about 1.5 ms |
| Plus dropping the mid-frame kick | 662 | 222 | 59.7-59.8 | about 0 (CPU saved, CPU/GPU overlap lost) |
| Plus narrow cache flush (shipped) | 773 | 333 | 59.7-59.8 | about 1.8 ms |

The 3D menu went 40.2 -> 44.6 fps on all arms (postmortem: "Old 3DS 3D menu 40.2 → 44.6 fps"; the repo document repeats it for the `onesubmit` setting). Mechanism as stated: "both eyes go in one GPU submission: the GPU runs the body, jumps to a right-eye prologue, runs it again". The GX-entry and interrupt counts are read from chart labels (990/550, 771/331, 662/222, 773/333) and are the least certain digits on this page. The postmortem also reports 0.4-0.5 ms of GPU dead time per backlogged queue entry on Old 3DS (about 40 us on a New 3DS), and that on a New 3DS the one-submission change was neutral. A plan to defer GPU waiting was dropped after an idle-GPU probe (13.5 -> 14.2 ms) showed idling first made frames no cheaper.

### Audio, logging, memory and scheduling

| Finding (all Old 3DS unless noted) | Reported detail | Helped? |
| ---------------------------------- | --------------- | ------- |
| Audio-lock contention | Render path waited 25.8 ms on a lock held by audio; narrowing it gave 0.2 ms (09-29) | Yes |
| Synchronous SD-card log writes | 26-270 ms main-thread hitches (REL2, 09-30); an asynchronous file log fixed them (REL3) | Yes |
| ARMv6 SIMD audio synthesis | 2.2x cheaper | Yes |
| Frame skip (auto / steady / race0) | Playable at 19-28 fps, nausea reported | Partly; interim |
| Frame-rate governor drift rule | Fixed 60/30 flapping | Yes |
| Requesting 80% of core 1 CPU time | Starved audio and loading; the repo document caps the request lower (a "safe" setting) | No at 80% |
| Cold `-Os` codegen | Crashed boot (09-29) | No |
| Deferred GPU wait | Dropped after probe | No |
| LCD framebuffers placed in VRAM | Crashed boot until a `lcd_vram=1` setting | Fix needed |
| Memory | 16 MiB emulated-RAM default ran out of memory in boot preload (09-28); halved to 8 MiB; heap fragmentation after six course loads fixed with a 777,360 B reserved arena | Yes |
| GX queue buffer 2x -> 4x citro3d default | Raised "on both consoles"; the failure it addressed was not fully read | Unknown |

Related wiki pages: [audio](../audio.md), [threading](../threading.md), [filesystem and saves](../filesystem-and-saves.md), [memory and cache](../memory-and-cache.md).

### What did not get fixed

From the postmortem's own known-issues list: 3D below locked 60; heavy post-race results table; Old 3DS memory ceiling in a worst-case emulator run; brief audio gaps during loads (a vblank-rate crackle theory was measured and refuted); the optional Expansion Kit is New 3DS only in practice (disk courses stuttered on Old 3DS in the postmortem; the repo document calls an Old 3DS build experimental and unreleased); not every mode was tested on every model.

> Unknown: the repo document gives hardware-log commit hashes for Old 3DS tuning but no dates; which results transfer to other engines is not stated by the source.

## Super Mario 64 3DS port (Wyatt-James)

**What it is.** A 3DS build of the decompilation-based PC port of Super Mario 64: [Wyatt-James/sm64-3ds-port](https://github.com/Wyatt-James/sm64-3ds-port).

**What the README says (read 11 November 2026).**

| Topic | Statement |
| ----- | --------- |
| Core assignment | The audio thread runs on core 1 on Old 3DS and core 2 on New 3DS. Requires Luma v10.1.1 or newer. |
| Audio | Enhanced emulation of the N64 audio processor is on by default; `DISABLE_ENHANCED_RSPA=1` turns the minor optimizations off, `FORCE_REFERENCE_RSPA=1` uses the PC port's original audio code, `AUDIO_USE_ACCURATE_MATH=1` removes inaccurate-but-unnoticeable math, `DISABLE_AUDIO=1` removes audio. |
| Frame skipping | `ENABLE_N3DS_FRAMESKIP=1` skips when a frame takes longer than 33.3 ms; the README calls it "no longer very useful" and keeps it for posterity. Naming note: the option name says N3DS, not Old 3DS. |
| Numbers | No frame-rate or timing measurements stated (the 33.3 ms threshold is the only figure). |

> Conflict: the review says the README documents "moving N64-style vertex processing into PICA200 shaders". The README text read here does not mention GPU vertex processing. That design (called Emu64) and its per-step savings are attributed to this port only second-hand, by the OOT research document (below). Treat the GPU-vertex claim as `[reported]` by a third party, not by the SM64 README.

**Second-hand figures** from the OOT document, for this port's branch `cleanup-and-optimization`. "Near 60 fps on Old 3DS" is attributed there to a different project, "Super Mario 64 3DS Port Ultimate" (Epic0522), not to this repo: whole N64 vertex stage in the PICA200 vertex shader; GPU texture coordinates -1.4 ms; colour combiner 11.54 -> 11.05 ms; vertex buffer creation 5.6 -> 4.8 ms; removing function-pointer API -0.8 ms; custom citro3d/libctru -1.6 ms; no render-target reselect per draw (20-30% fewer GPU commands); "near 60 fps on Old 3DS". Units, scene and per-frame basis are not stated there. `[reported]` second-hand; model and conditions: Unknown.

## Ocarina of Time port native-speed research (Andresac90)

**What it is.** Research notes for an N64-to-3DS Ocarina of Time port: [docs/3ds-native-speed-research.md](https://github.com/Andresac90/OOT-64-3DS-Port/blob/main/docs/3ds-native-speed-research.md), dated 2026-10-01 with entries through 2026-10-04.

**Which hardware the numbers came from.** This document separates three kinds of evidence and the page must not blur them:

| Kind | What it is | Examples in the document |
| ---- | ---------- | ------------------------ |
| Real New 3DS | Hardware logs on a New 3DS at normal speed | v29 GPU shading attempt: textures smeared |
| Simulated Old 3DS speed on hardware | A New 3DS run at 268 MHz without L2 cache, one core | Baseline profile; v47 profile; v49 vertex-write cost. Described as "Old 3DS speed", not as an Old 3DS |
| Emulator (Azahar) | Instruction-count emulation, set to 25% CPU in one experiment | Frame-skip rates, split-threshold tuning, v53 per-frame gain |

The document states these items are not verified on a real Old 3DS: frame skip, the render thread, the v53 raw-vertex path, save-write timing, and "drawing every update". No row of this page is an Old 3DS measurement from this source. The document itself says Azahar cannot validate precision-sensitive GPU techniques or threading gains.

| Reported item | Value and conditions |
| ------------- | -------------------- |
| Baseline update cost, simulated Old 3DS speed | about 98 ms per update vs 50 ms on the N64; drawing about 80% of an update |
| Biggest stages | triangle emit 15%, triangle split 14%, logic 14%, vertex+lighting 11%, submit/flush 10%, triangle setup 9% |
| Corrected drawn-frame cost (simulated Old 3DS speed, 10-04) | about 75-80 ms; an earlier 38 ms figure was averaged over skipped updates |
| v48 CPU cuts (hardware, simulated Old 3DS speed) | renderer CPU per frame -16% to -22% on one demo scene |
| Vertex-buffer writes (v49, simulated Old 3DS speed, one core) | 12-17% of all time, against about 1% at New 3DS speed; cause given as linear memory without L2 |
| v53 raw vertex path: 16 B per vertex instead of 56 B | Azahar only: 13.05 -> 11.10 ms per frame; image error 5.92 -> 5.84; hardware result pending |
| Frame skip | Azahar at 25% CPU: 10.25 -> 19.5 updates/s, 11.4 frames shown/s; hardware check pending |
| GPU-side screen-linear shading | Matched in Azahar, failed on a real New 3DS (smeared textures); off by default |
| Dropping triangle splitting | about 7% less work but visible flattening (one scene's error 8.9 -> 30.8); not adopted |

The document also records second-hand claims about other ports (G-Diffuser, SM64, Ship of Harkinian, Majora's Mask) that predate or conflict with their own pages.

> Conflict: the OOT document lists G-Diffuser as "59.6 fps median on New 3DS; Old 3DS unsupported". The G-Diffuser postmortem reports Old 3DS hardware results (60 fps in 2D races, 10-02). The 59.6 median is the New 3DS baseline in the postmortem. Prefer the postmortem; treat the OOT table as an early summary.

> Unknown: whether a 268 MHz, no-L2 New 3DS reproduces real Old 3DS memory and instruction-cache behaviour. The document calls it a proxy.

## Techniques catalogue

Every row is `[reported]` by the cited project, on the conditions in its section above. "Applies when" is Coffeepot's reading, not the sources' claim, and is a `[hypothesis]` until measured here.

| Technique | Where used | Reported effect | Applies when | Risk |
| --------- | ---------- | --------------- | ------------ | ---- |
| Keep hot render path small (native per-category renderer instead of generic display-list interpreter) | G-Diffuser | CPU per 2D race frame 21.1 -> 9.1 ms; 2D racing 10 -> 60 fps (Old 3DS) | Hot code well above 16 KB L1-I and many similar draws | Large rewrite; correctness drift versus the original |
| Retained meshes / precompiled draw lists | G-Diffuser | The postmortem says a machine becomes a retained mesh after its first draw, but also that "a retained renderer won in the emulator and did almost nothing on hardware"; no isolated figure | Geometry repeats frame to frame | Memory use; invalidation bugs |
| Move whole vertex stage into the PICA200 vertex shader | SM64 (second-hand), OOT research | SM64 per-step savings of 0.8-1.6 ms; OOT estimates about 40% of an update (simulated Old 3DS speed; not tested) | CPU spent preparing vertices | Lighting and shading differences; precision limits |
| Shrink CPU-written vertex data (16 B vs 56 B) | OOT | Azahar 13.05 -> 11.10 ms; real effect unmeasured | Vertex stores are a large share of time (reported 12-17% at simulated Old 3DS speed) | Fallback paths for edge cases |
| One GPU submission for both eyes | G-Diffuser | 1P 3D race 56.4 / 59.3 -> 59.7-59.8 fps; 3D menu 40.2 -> 44.6 (Old 3DS) | Stereo with many small queue entries | Dropping the mid-frame kick lost CPU/GPU overlap (net zero) |
| Narrow audio lock | G-Diffuser | wait 25.8 -> 0.2 ms; 11.3 -> 14.7 fps | Render or game code shares a lock with audio | Race conditions if narrowed wrongly |
| Asynchronous file logging | G-Diffuser | removed 26-270 ms hitches | Debug logs written to SD during play | Lost log lines on crash |
| Audio on a separate core (core 1 Old 3DS, core 2 New 3DS) | SM64, OOT port, G-Diffuser (core-1 time request) | No isolated number stated | Audio emulation is heavy | Core 1 shares time with system; a high CPU-time request starved audio and loading in G-Diffuser |
| Enhanced / SIMD audio mixing | SM64, G-Diffuser | G-Diffuser: ARMv6 SIMD synthesis 2.2x cheaper | Software mixing | Small accuracy changes |
| Frame skipping | SM64 option, G-Diffuser, OOT (Azahar only) | G-Diffuser 19-28 fps drawn with 57-60 logic ticks/s; users reported nausea; SM64 README: "no longer very useful" | Last resort when drawing cannot keep up | Judder, nausea; interim only |
| Render on a second core one frame ahead | G-Diffuser baseline, OOT plan | G-Diffuser: 59.6 fps median on New 3DS | New 3DS only | **New 3DS:** needs shared-state locking; not available on Old 3DS |
| Reserve arenas, avoid fragmentation | G-Diffuser | Fixed a 96,768 B allocation failure with 1.6 MB free | Repeated level loads | Fixed budget must be sized |
| Avoid per-draw render-target reselect, function-pointer APIs | SM64 (second-hand) | 20-30% fewer GPU commands; -0.8 ms | Layered cross-platform graphics API | Needs a custom library fork |
| GPU "screen-linear" shading emulation | OOT | Worked in Azahar, failed on real New 3DS | Do not assume | Texture smearing; emulator cannot validate it |

## What to try first on an Old 3DS

None of this is proven for your workload; each step ends in a measurement, so follow [performance](../performance.md) and [testing on hardware](../testing-on-hardware.md).

1. **Measure on the real console first.** Note that two of three sources used a New 3DS or an emulator for part of their numbers. Use the profiling steps in [performance](../performance.md) and the counters in [frame pacing](../PICA200/frame-pacing.md).
2. **Separate CPU frame time from GPU time.** Both ports found CPU-side display-list translation, not GPU work, to be the bottleneck (G-Diffuser: GPU busy 3.6 ms in a 96 ms frame; OOT: GPU 5-7 ms per frame). Check yours before touching shaders: [GPU rendering](../PICA200/gpu-rendering.md), [PICA200 command pipeline](../PICA200/pica200-command-pipeline.md).
3. **Look for lock contention and synchronous I/O** between audio, render and logging; these produced the largest cheap wins in G-Diffuser. See [audio](../audio.md), [threading](../threading.md), [filesystem and saves](../filesystem-and-saves.md).
4. **Shrink and localise hot code and per-vertex data** before micro-tuning. See [CPU optimization](../cpu-optimization.md) and [memory and cache](../memory-and-cache.md).
5. **Budget stereo submissions** if you ship stereo 3D; see [frame pacing](../PICA200/frame-pacing.md).
6. **Pick one frame-rate target** and treat frame skipping as a fallback; see [frame pacing](../PICA200/frame-pacing.md).
7. Keep memory ceilings explicit: [memory and cache](../memory-and-cache.md).

**New 3DS:** a second core for rendering or audio is available; the sources report it as the largest single gain for G-Diffuser's baseline, but it is not an Old 3DS technique. See [threading](../threading.md).

## Technical details

- Evidence classes: statements are by the project authors (`[reported]`); the techniques table "applies when" column is inference; nothing is `[measured]` by Coffeepot.
- The postmortem's headline scale: about 1,600 commits on the integration branch over 09-07 to 10-19 and two consoles from one codebase (as stated).
- Probe results are for a synthetic instruction-fetch test and say nothing about arbitrary game code.
- Verification: on 11 November 2026 the postmortem, `NATIVE-RENDERER.md`, the SM64 README and the OOT document were downloaded raw and the figures in the tables above were matched by text search (CPI 1.06/4.10/5.62; 21.1 -> 9.1 ms; 40.2 -> 44.6; 25.8 -> 0.2 ms; 26-270 ms; 48-57, 53.7 and 59.7-59.8 fps; 1P-race headroom; all OOT numbers; all SM64 README statements). Not matched: the SM64 figures in the OOT document are second-hand; none of the commit hashes were resolved.

## Pitfalls

- Quoting a New 3DS or simulated-speed result as an Old 3DS result.
- Reading the 59.7-59.8 fps stereo figure as general.
- Treating an emulator timing as a result; the OOT document itself warns against it.
- Copying a technique whose effect is "expected, not measured" in its source.

## Open questions

- Do these techniques reproduce on workloads other than N64 display-list ports? No measurement exists in this wiki; a benchmark that varies code footprint on an Old 3DS would settle the instruction-cache question.
- Does the 268 MHz no-L2 proxy match an Old 3DS? A paired run on a real Old 3DS would settle it.

## Sources

- [Road to 60, Again (G-Diffuser native-renderer postmortem)](https://hypnotize.works/gdx-3ds/postmortem/native.html) — Old 3DS timeline, cache probe, per-category CPU times, submission experiment.
- [cruxxxxxx/gdx-3ds](https://github.com/cruxxxxxx/gdx-3ds) — `docs/NATIVE-RENDERER.md`: architecture, settings and figures.
- [Wyatt-James/sm64-3ds-port](https://github.com/Wyatt-James/sm64-3ds-port) — README: audio core assignment, audio options, frame skip.
- [Andresac90/OOT-64-3DS-Port native-speed research](https://github.com/Andresac90/OOT-64-3DS-Port/blob/main/docs/3ds-native-speed-research.md) — profile, simulated-Old-speed notes, second-hand SM64 figures.
