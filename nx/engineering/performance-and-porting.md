# NX performance and porting

This page is for a developer whose Switch homebrew game runs slower than it should, or who is bringing an existing game over from a PC or another console. The short answer: decide what you are measuring before you change anything, measure on the clocks your players will have, change one thing at a time, and treat the reported numbers on this page as examples of what other people found on their scenes, not as budgets for yours.

## Before you start

- Toolchain: devkitA64 r30-1, `libnx` 4.12.0-1, `deko3d` 0.5.0-1, checked as described on [source versions](source-versions.md). Code fragments compile with `-Wall` and no warnings; none has been run on a console.
- Applies to retail Switch homebrew. Clock figures are stock retail clocks unless the text says overclock; they are cited to a port, not measured by this wiki (see [Stock clocks](#stock-clocks-a-port-reported-table)).
- Start with [a first Switch game](first-game.md) if you have not built a program yet, and use [debugging](debugging.md) when something fails. Read alongside [rendering](rendering.md) for the `deko3d` frame loop and [NX CPU](../hardware/cpu.md) and [NX GPU](../hardware/gpu.md) for the hardware.
- Most numbers below come from open-source ports. None was reproduced by Coffeepot, so each is labelled `[reported]` and names the project, its commit and the conditions its authors state.

## Stock clocks (a port-reported table)

The following is what one port documents as its target hardware. It is the only complete clock table in the saved sources, and it is `[reported]` rather than measured here.

| Mode | CPU | GPU | Memory |
| ---- | --- | --- | ------ |
| Handheld | 1020 MHz | 460.8 MHz (307.2 MHz at start; 460.8 MHz after asking for the official performance mode) | 1331.2 MHz |
| Docked | 1020 MHz | 768 MHz | 1600 MHz |

`[reported]` (`nfsmw-nx` `README.md` stock-clock table and `docs/platform-notes.md` "In short", commit `20f8dc1`). That port also states a game gets three CPU cores. The same figures appear as the stock 1020 MHz CPU, 307.2/384/460.8 MHz handheld GPU and 768 MHz docked GPU in `sa2-nx`, `usf4-nx` and `SwitchWakerHD` documents. Overclocked runs in those trees (CPU 1785 MHz, GPU 768 MHz or higher) are labelled by their authors as non-stock.

> Unknown: the SwitchBrew "Clocks" page, the usual primary reference for these values, was not available when this page was checked. Until it is read, every clock figure on the wiki is a port report.

## Decide what you are measuring

What to do: pick one scene, one launch mode (title override or Album applet; see [Atmosphere and running](../homebrew/atmosphere-and-running.md)), one clock setting, one build, and record all four with every number. Pin the clocks for the whole session. A clock manager such as `sys-clk` can do this; ports that report their numbers say which clocks they used, and several state that they test at stock clocks on purpose `[reported]` (`nfsmw-nx` `docs/platform-notes.md`: "Plan for the normal clocks, without overclocking").

- Expected result: repeated runs of the same scene agree within noise, so a later change is real.
- How it fails: if the same build reports 15 ms and then 19 ms with no change, clocks or launch mode moved. If a "gain" disappears with cheats or file overlays disabled, or on a second console, the setup changed and not the code.
- Overclocked results can hide the real limit. NFSMW's authors found a scene that ran at 34 fps overclocked and 15 fps at stock because the bottleneck moved from the CPU to the GPU, and adopted stock clocks from the start `[reported]` (`nfsmw-nx` `docs/performance-history.md`, commit `20f8dc1`). Never size an asset budget from an overclocked run.

## Time CPU work and GPU work separately

What to do: record how long the CPU spends preparing and submitting a frame and how long the GPU takes to finish it, as two numbers. Use fences for the GPU side (see [rendering](rendering.md)) and a CPU timer for the CPU side, and keep the two buckets exclusive; do not add a bucket that already contains another.

- Expected result: you can say whether a slow frame is CPU-bound (CPU time near the frame budget, GPU idle) or GPU-bound.
- How it fails: always-on timers inflate the pass they measure. If removing a timer speeds the pass up, the timer was the cost. Nested buckets that "add up" to more than the frame time are overlapping, not additive.

`[reported]` examples of the split, each for one scene and not transferable: in SA2's City Escape (about 1,400 draws, CPU 1020 MHz) the game thread took 15.5 ms and the render thread 11.1 ms while the GPU needed 7-9 ms even at 307 MHz, so the CPU was the limit (`sa2-nx` `README.md` and `docs/deko3d-renderer-guide.md`, commit `baf523b`). In NFSMW the reverse was true once the CPU cost per draw had been cut: the GPU was the limit at stock clocks (`nfsmw-nx` `docs/performance-history.md`).

A frame at 60 fps is 16.7 ms. The 1020 MHz CPU clock gives about 17 million cycles per core per frame by arithmetic (1020 MHz ÷ 60), shared by every thread on that core; real throughput is lower.

## Where reported speed-ups came from

These are the changes the SA2 port credits, in the order its author applied them. They are one project's history on one game, not a recipe, and your order should come from your own profile.

| Change | Reported effect | Conditions as stated | Source |
| ------ | --------------- | -------------------- | ------ |
| Profile-guided optimisation (PGO), profile recorded in a console session | "about 15 % faster" for 1.0.0 than the same build without it | Released SA2 NRO, player's console test, 2026-10-08 | `sa2-nx` `README.md`, `docs/enhancements.md` line 1691 |
| Direct calls, link-time optimisation (LTO), PGO and function ordering together | NFSMW 30.3 fps to about 34 fps | NFSMW race, stock clocks; step 6 of its history table (step 5 is 30.3 fps) | `nfsmw-nx` `docs/performance-history.md` |
| 2 MB memory blocks instead of 4 KB pages | about 4.3 % in one GPU-heavy spot, at a cost of about 47 MB | **measured on Kameo (kameo-nx), not SA2**; SA2 version 1.0.6 adopted the change and did not quantify its own gain | `sa2-nx` `README.md` (1.0.6 paragraph), `docs/cpu-rounds.md` section "2 MB blocks" |
| Render-thread work (device deltas, store cards instead of content hashing, chain reuse, NEON compares, batched wake-ups, core layout) | Render thread 17.6 ms to 11.1 ms and game thread 19.2 ms to 15.5 ms | City Escape, 1020 MHz, about 1,400 draws, before a later PGO profile; this is the improvement across the work in section 7 of the guide, **not** a before/after of switching from Vulkan to `deko3d` | `sa2-nx` `docs/deko3d-renderer-guide.md`, `README.md` |

`[reported]` for every row. Do not extend any row to another game, another model or another clock setting. Do not repeat a row without its conditions.

Whether PGO and 2 MB pages help a small homebrew scene at stock clocks is an open question (see below). Large pages also cost memory and change how the executable is loaded in the SA2 port (it copies its code to the heap and remaps it), so they are not a free flag.

```sh
# PGO shape for GCC (flag names are GCC's; check your devkitA64 version's documentation)
# 1. build with:  -fprofile-generate    and run a representative session on the console
# 2. rebuild with: -fprofile-use
# Link-time optimisation: -flto
```

Verification: flag names are GCC's documented options; no PGO build was made or run for this wiki. How a profile is moved from the console back to the PC is project-specific (`sa2-nx` keeps its profile in `pgo/`).

## Threads and cores

What to do: if profiling shows one thread saturating a core, split the work. A single-threaded loop is a good starting point for a small game, and a separate render or audio thread without a measured bottleneck adds work and makes bugs harder to find.

- Facts from the ports `[reported]`: a game gets three usable CPU cores; only thread priority `0x3B` is time-sliced (10 ms slices in SA2's notes), so a busy thread at another priority can starve threads of the same or lower priority on its core; each thread must be given an explicit core mask or it will share a core with others (`sa2-nx` `docs/porting-guide.md` line 304 and `docs/renut-cpu-work-for-banjo.md` line 114; `nfsmw-nx` `docs/platform-notes.md`, "Only thread priority 0x3B takes turns on a core").
- SA2 pins its game thread to core 0, its render thread to core 1 and puts other threads on cores 1-2, preferring 2 `[reported]` (`sa2-nx` `docs/deko3d-renderer-guide.md`, key `sa2_renut_core_layout`). That is a layout for a game with a heavy game thread; it is not a general rule.
- `[hypothesis]` A lock-free queue between a simulation thread and a render thread helps when the render thread's per-draw work is large. SA2's first build of one was reported as a regression in the game thread (32.4 ms against 28.7 ms in the 2-player City Escape scene of 2,929 draws), and its author traced the push cost to cache misses on a large ring of slots; but the first figure was taken docked and the earlier one handheld, so the two numbers are not a clean comparison `[reported]` (`sa2-nx` `docs/renut-renderer.md` item 42). Measure with and without it. What would settle it: a benchmark with a producer and a consumer thread using a mutex queue, a lock-free queue and no queue, on retail clocks.

For a simple fixed-step loop, one that caps catch-up steps stops a single slow frame from causing seconds of slow motion:

```c
const double DT = 1.0 / 60.0;
int steps = 0;
acc += frameSeconds;
while (acc >= DT && steps < 5) { stepSimulation(DT); acc -= DT; steps++; }
if (steps == 5) acc = 0;     // drop the backlog instead of spiralling
alpha = acc / DT;            // interpolation factor for rendering
```

Verification: compiles inside a stub function, `-Wall`, no warnings; not run on hardware. A fixed 60 Hz step is a design choice, not a Switch requirement: use it if your game logic needs deterministic steps, and use a variable step if it does not.

## Port checklist (things ports reported tripping over)

Each item is a reported lesson, not a rule. Check the ones that apply to your game.

1. **Launch mode and memory.** Several ports require title override (launching in place of a full game) because the Album applet's memory budget is too small and the symptom is out-of-memory aborts `[reported]` (`srb2-switch` `README.md` line 15; `SwitchWakerHD` `README.md`; taisei warns of crashes from the photo applet; see [Atmosphere and running](../homebrew/atmosphere-and-running.md)). Log the launch mode with every test.
2. **Pre-convert game data.** Ports ship assets converted at build time instead of decoding PNG or JSON at run time to shorten loads `[reported]` (`SwitchWakerHD` build path; `sa2-nx` installer approach). `[hypothesis]` that runtime decoding is slower on the A57 than on a PC for your assets; what would settle it: time the decode on the console.
3. **Audio.** Ports resolve output to 48 kHz (godot_switch configures `audren` at `AudioRendererOutputRate_48kHz` with 24 voices, 2 mix buffers `[documented]`, `godot_switch` `platform/switch/audio_driver_switch.cpp`; `SwitchWakerHD` mod SDK supports 48000 Hz only `[reported]`). Mix on a thread whose work is bounded and keep long music streaming rather than resident; `[hypothesis]` that this is needed for your game.
4. **Saves.** See [Crash-safe saves](#crash-safe-saves).
5. **Frame pacing.** Choose between tearing and dropped frames on purpose. SA2 shows a missed 60 fps frame as soon as it is ready (50-59 fps) instead of waiting a full refresh and falling to 30 fps; the option is `sa2_vsync_relaxed`, default on `[reported]` (`sa2-nx` `README.md`). Which is right depends on your game.
6. **Lifecycle.** See [Handheld, docked and suspend](#handheld-docked-and-suspend).
7. **Network and logging.** Do not let a blocking socket call run on the game thread. USF4 froze after about 37 seconds because a debug-log send blocked while holding a mutex; a non-blocking socket fixed it `[reported]` (`usf4-nx` `docs/deko3d-renderer-usf4.md`, "Traps"). `nxlinkStdio` itself sets its socket to non-blocking mode `[documented]` (`libnx` `nx/source/runtime/nxlink_stdio.c`, `fcntl(... O_NONBLOCK)`), but a `printf` to a full buffer is still a cost on the frame; log sparingly.

## Handheld, docked and suspend

What to do: gate the main loop on `appletMainLoop()`, and register an applet hook to learn when the operation mode (handheld or docked) or focus changes, instead of polling every frame.

```c
static void onApplet(AppletHookType type, void* param)
{
    switch (type) {
        case AppletHookType_OnOperationMode:   // handheld <-> docked
            g_docked = (appletGetOperationMode() == AppletOperationMode_Console);
            break;                             // re-pick resolution and budgets here
        case AppletHookType_OnFocusState:      // Home menu, overlay, sleep
            g_focused = (appletGetFocusState() == AppletFocusState_InFocus);
            break;                             // pause audio, stop submitting while not in focus
        default: break;
    }
}
/* once at startup: static AppletHookCookie cookie; appletHook(&cookie, onApplet, NULL); */
```

Verification: compiles, `-Wall`, no warnings; not run on hardware, including docked/handheld transitions and sleep.

`appletMainLoop` returns false when an exit is requested, which lets the app clean up after the loop `[documented]` (`libnx` `nx/include/switch/services/applet.h`, `appletMainLoop` and the note at line 704). `appletGetOperationMode` returns `AppletOperationMode_Handheld` or `AppletOperationMode_Console` and is updated by `appletMainLoop` when notifications arrive `[documented]` (same header, lines 35-36 and 2760-2763). Hook types include operation mode, performance mode, focus state, exit request and resume (same header, lines 41-48).

Controllers: `padConfigureInput(1, HidNpadStyleSet_NpadStandard)` and `padInitializeDefault(&pad)` once, then `padUpdate(&pad)` once per frame, as in the upstream template. `padIsConnected`, `padIsHandheld`, `padGetButtons` (held) and `padGetButtonsDown` read the state `[documented]` (`libnx` `nx/include/switch/runtime/pad.h`; `switch-examples` `templates/application/source/main.c`). The old `hidScanInput`/`hidKeysHeld` API was deprecated in libnx 4.0.0 and removed in 4.1.3 `[documented]` (`libnx` `Changelog.md`).

> Unknown: how a game should behave when a controller disconnects mid-play, and how touch, motion and rumble should be handled across handheld/docked transitions. The `switch-examples` `hid/` tree has `read-controls`, `touch-screen`, `sixaxis`, `vibration` and `abstracted-pad`, but this page has not worked through them. A test on hardware is needed.

## Crash-safe saves

What to do: write the new save to a temporary file, flush it to the card, then replace the old save in a way that never leaves you without a valid file.

```c
FILE* f = fopen(tmp, "wb");
if (!f) return -1;
if (fwrite(data, 1, len, f) != len) { fclose(f); return -1; }
fflush(f);
fsync(fileno(f));       // push the data to the card before touching the old save
fclose(f);
remove(bak);            // rename() will not replace an existing destination here
rename(cur, bak);       // fails harmlessly on the very first save
return rename(tmp, cur);
```

Verification: compiles, `-Wall`, no warnings. **Not run.** Two behaviours are assumed and unverified: that `rename` to an existing destination fails through libnx's filesystem layer (hence the `remove(bak)`), and that `fsync` reaches the SD card. Test both on a console, including pulling power mid-write, before relying on this.

This is a file-on-SD pattern. Applications with their own save data use `fsdevMountSaveData` and must call `fsdevCommitDevice` after writing for changes to take effect `[documented]` (`switch-examples` `fs/save/source/main.c` lines 88-96). Where each kind of save lives is on the [firmware page](../software/firmware.md).

## Measure the way the ports learned to

These rules come from the postmortems and test rigs of the ports in the [case studies](case-studies/case-study-sa2.md). Each is a working practice that its authors report, not something this wiki has tested.

1. **One hypothesis, one patch, one hardware pair.** Write down the symptom, the files, a falsifiable mechanism, the change and the expected delta; run the same save, scene, clocks and build hash before and after. Two failed, comparable attempts close the hypothesis; do not stack a third fix on it (Eidos engineering guide, [case study](case-studies/case-study-eidos-ouroboros.md)).
2. **Interleave runs, do not batch them.** SwitchWakerHD's performance test boots each of two builds four times in alternation with fixed time of day and records clock lines and a hitch profile, so drift in temperature or state hits both sides equally (`SwitchWakerHD` `docs/perf-results.md`, [case study](case-studies/case-study-wind-waker-hd.md)). NFSMW alternates the two variants every N seconds inside one session and includes a no-op control to measure the noise itself (`nfsmw-nx` `docs/measuring.md`).
3. **Report percentiles and hitches, not just a mean.** Eidos and NFSMW publish p50/p95/p99 and hitch counts, and NFSMW found a "busy 97%" counter that was really 76% CPU plus 21% waiting, and a 200-entry stutter log that hid second-half races (`nfsmw-nx` docs; [case study](case-studies/case-studies-demanding-ports.md)). Separate simulated frames from presented frames, and keep timing buckets exclusive.
4. **Echo the effective settings in every log.** NFSMW found a config file silently overriding code defaults for several builds, so every log now prints the settings in force (`nfsmw-nx`).
5. **Prove the comparison is real.** Take a screenshot and a draw count for each presented frame before comparing two renderers (USF4's empty-cache trap, [rendering](rendering.md)).
6. **Acceptance is on hardware, at stock clocks, in the shipped launch mode.** An emulator timing, an overclocked run, a no-wait run or an effects-off run does not pass. Eidos uses a ladder: baseline integrity, then GPU coverage, then full speed at 1×, then the heavier configuration (Eidos engineering guide).
7. **Vendor timestamps can be wrong.** NFSMW multiplies raw NVK GPU timestamps by 1.627 on the Switch to correct the clock period (`nfsmw-nx` `docs/measuring.md`). That is specific to NVK; check how your own timestamps scale before trusting them.

## Lessons for audio, memory, startup and services

- **Pull audio in fixed blocks and pad explicitly.** melonDS's audio thread asks its emulated sound unit for 768-sample blocks at a 32823 Hz source rate, sleeps 10 µs while the unit has nothing, and pads a short block with the last frame before queueing it `[documented]` (`melonDS` `src/frontend/switch/main.cpp` lines 216-243; `Util_Audio.cpp` line 55). Late video therefore stretches audio rather than dropping out cleanly. The padding idiom compiles as a fragment in the compile-check program.
- **Run the audio pump on its own thread at a deliberate priority.** godot_switch sets its audio thread to priority `0x2B` and sizes `audren` for 48 kHz, 24 voices, 2 mix buffers `[documented]` (`godot_switch` `audio_driver_switch.cpp` lines 40-45, 117); melonDS's audio thread is created at priority `0x20` on core 0 and its emulation threads at `0x30` from core 1 (`melonDS` `main.cpp` line 312, `Platform.cpp` line 80). Priority and core layout interact with the time-slicing note above; pick them on purpose and profile.
- **Data authored for another sample rate plays at the wrong speed.** 44.1 kHz data played as 48 kHz runs 48/44.1, about 9% fast, by arithmetic; resample or declare the right rate. max_nx forces its OpenAL context to 44,100 Hz and lets the Switch mixer resample (`max_nx` `source/hooks/openal.c` line 28).
- **Split the heap from the image and refuse the applet.** max_nx reserves a 512 MB newlib heap, gives the rest to the mapped game, and states that applet mode cannot work because nothing is left after the heap is allocated (`max_nx` `source/config.h` line 13, `README.md` lines 34-35). Decide your heap size explicitly and test it under both launch modes.
- **Check the startup preconditions and say what is wrong.** max_nx checks each data archive and each privileged syscall and shows an on-screen error naming the missing file or call (`max_nx` `source/main.c` lines 81-95). A one-function version, `requireFile`, is in the compile-check program.
- **Follow a hot/warm/cold split for services.** Connect to each service once at start and close at exit (see [services and IPC](../software/services-and-ipc.md)); call per-frame services (`padUpdate`, the vsync wait, audio append, the applet loop) from the main loop; keep file opens and network checks off the frame and on a worker thread with an event for completion; keep one-time work (service connects, display layer creation, `romfs` mount) at start.
- **Mind the focus-handling mode.** By default the system suspends the process when Home opens or the console sleeps. nxquake2 sets `AppletFocusHandlingMode_NoSuspend`, installs an applet hook and calls `appletLockExit()` so single-player can auto-pause itself and skip server timeouts across sleep `[documented]` (`nxquake2` `src/backends/nx/system.c` lines 80-90). Both calls compile in the compile-check program; the effect on hardware is untested here.
- **SDL may report the wrong display size.** nxquake2 notes that `SDL_GetDesktopDisplayMode` always returns 1920×1080 on Switch and so asks `appletGetOperationMode()` to choose 1920×1080 docked or 1280×720 handheld `[documented]` (`nxquake2` `src/client/vid/glimp_sdl.c` lines 724-736).
- **Portable saves and crash logs.** nxquake2 forces portable mode so configs, saves and `crash.log` sit next to the NRO on the SD card, which makes bug reports possible `[documented]` (`nxquake2` `src/backends/unix/main.c` line 63). Strip verbose `nxlink` logging from release builds (godot_switch leaves it on).
- **Ship a CI build.** STK builds its NRO in a devkitA64 container on every push (`stk-code` `.github/workflows/switch.yml`), which is the behaviour `tools/build_benchmarks.sh` provides here for benchmarks.

## Lessons for emulators and ports of non-native games

- **Interleave guest CPUs to one target.** melonDS advances the DS ARM9, ARM7, timers, DMA and 3D-stall events to a shared timestamp each step in `NDS::RunFrame`, which keeps timing-sensitive games stable on three host cores `[documented]` (`melonDS` `src/NDS.cpp` lines 947-1022). Get determinism first; pin threads afterwards.
- **Static recompilation is not a free CPU win.** The recompiled ports spent their effort on the per-draw cost of the translated draw stream (SA2's device copies, USF4's texture hashing). Those techniques solve that problem and are not needed by a game written natively (see [Where reported speed-ups came from](#where-reported-speed-ups-came-from)).
- **Running an Android binary in place needs only a small seam.** max_nx hooks four EGL functions and about 130 OpenAL symbols and spoofs the device so the game picks cheaper defaults (`max_nx` `source/hooks/`). Its author flags the stack-guard workaround in the source as "POSSIBLY VERY BAD" (`source/hooks/game.c` line 388): treat any such hack as unproven.
- **Content scalers beat blind tuning.** max_nx and nxquake2 expose detail knobs (decal and debris multipliers, MSAA samples with halving retry on context failure) instead of one fixed quality; the cost is that nothing in either tree says what each knob buys (`max_nx` `source/config.c`; `nxquake2` `glimp_sdl.c`).
- **Crowded-scene CPU spikes (hypotheses from Serious Engine's structure).** `[hypothesis]` Cap per-entity ticking by distance, stagger AI over frames and pool projectiles; keep the visible set small by tightening portals instead of speeding up the culler; preload the working set under title override and never fault textures from the render path. What would settle each: a console profile of the worst arena scene with the change on and off.
- **Exposing clocks inside a game.** dhewm3 offers an `nx_overclock` setting (0-3) that selects CPU, GPU and memory clocks up to 1.785 GHz / 768 MHz / 1.6 GHz and restores them on exit `[documented]` (`dhewm3` `neo/sys/nx/nx_main.cpp` line 74). If you do this, restore on exit, log which clock moved the bottleneck, and keep acceptance on stock clocks, as the earlier sections require.
- **Missing SIMD paths.** dhewm3's `Simd` layer has SSE and AltiVec paths and no NEON path, and nxquake2 has no NEON intrinsics; compiler flags `-mcpu=cortex-a57+crc+fp+simd` alone do not vectorise that code `[hypothesis]`. What would settle it: profile a hot geometry or math routine and compare a NEON version.

## Technical details

- Core count and priority: three usable cores, only priority `0x3B` is time-sliced `[reported]` (`sa2-nx`, `nfsmw-nx` as above).
- Launch mode: the title-override and Album-applet distinction, and the 39-bit address-space default for the homebrew menu under override, are on [Atmosphere and running](../homebrew/atmosphere-and-running.md).
- Bandwidth: 25.6 GB/s is the theoretical docked figure (1600 MHz, 64-bit bus), 21.3 GB/s the arithmetic for 1331.2 MHz; both are shared by CPU and GPU. See the arithmetic on [rendering](rendering.md).
- Snapshot deltas, content hashing with "store cards", depth-target clipping and shader-prebuild lists are techniques from recompilation ports (SA2, USF4, Wind Waker HD). They solve problems that a recompiled game's draw stream creates, so read them in the [case studies](case-studies/case-study-sa2.md) before copying any of them to a game written natively.

## Pitfalls

- **Profiling with unpinned clocks.** Symptom: irreproducible deltas. Fix: pin clocks and log them.
- **No known-good reference build.** Symptom: "faster but wrong" results. Fix: keep a reference build (a PC build for ports, a previous tag for homebrew) and compare output on the same scene.
- **Uncapped fixed-step catch-up.** Symptom: seconds of slow motion after one hitch. Fix: cap catch-up steps.
- **Album-applet memory.** Symptom: out-of-memory on the console, not on PC. Fix: test under title override.
- **Blocking I/O on the game thread.** Symptom: periodic multi-second freezes. Fix: non-blocking sockets and background threads for file and network work.
- **Quoting someone else's number without its conditions.** Symptom: a budget that does not fit your game. Fix: keep the label and the conditions with the number.

## Open questions

- Do PGO and 2 MB pages help a minimal homebrew scene at stock clocks? Needs a same-scene benchmark with the flag on and off, clocks pinned, model recorded.
- What queue depth between simulation and rendering keeps three cores busy without adding latency? Needs a sweep of 1-4 frames against measured input-to-display latency.
- Does `rename`-over-existing fail on the SD card through newlib/libnx, and does `fsync` flush to the card? Needs a small test program run on hardware.
- Is Erista or Mariko faster under sustained load at stock clocks? Unmeasured; USF4 and NFSMW call their thermal comparison anecdotal.

## Sources

- [libnx](https://github.com/switchbrew/libnx) at `b156b4774fb2df7b93402169509266da7547fc4c` — `Changelog.md` (HID API removal), `nx/include/switch/runtime/pad.h`, `nx/include/switch/services/applet.h`, `nx/source/runtime/nxlink_stdio.c`.
- [switch-examples](https://github.com/switchbrew/switch-examples) at `669786898205b7beb25ff1731e72982e6d0397d3` — `templates/application`, `fs/save`, `hid/`.
- [sa2-nx](https://github.com/ChanseyIsTheBest/sa2-nx) at `baf523b` — `README.md`, `docs/deko3d-renderer-guide.md`, `docs/enhancements.md`, `docs/cpu-rounds.md`, `docs/porting-guide.md` `[reported]`.
- [usf4-nx](https://github.com/Thorhax/usf4-nx) at `30ffae0` — `docs/deko3d-renderer-usf4.md` `[reported]`.
- [nfsmw-nx](https://github.com/StevensND/nfsmw-nx) at `20f8dc1` — `README.md`, `docs/platform-notes.md`, `docs/performance-history.md` `[reported]`.
- [godot_switch](https://github.com/redthing1/godot_switch) at `0918643f6` — `platform/switch/audio_driver_switch.cpp`.
- [melonDS](https://github.com/RSDuck/melonDS) at `6c6f1e8`, [max_nx](https://github.com/fgsfdsfgs/max_nx) at `264a0cc`, [nxquake2](https://github.com/fgsfdsfgs/nxquake2) at `a8fdda02`, [dhewm3](https://github.com/fgsfdsfgs/dhewm3) at `920e759`, [stk-code](https://github.com/supertuxkart/stk-code) at `b2a9f22` — files named in each lesson; see the matching case studies.
- [srb2-switch](https://github.com/P-AS/srb2-switch) at `90194061a` — `README.md`.
- [SwitchWakerHD](https://github.com/centollOS/SwitchWakerHD) at `9aec4e3` — build path and audio notes `[reported]`.
