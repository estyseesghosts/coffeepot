# Frame pacing on the 3DS

Frame pacing is about *when* frames appear, not only how many: a game that shows a new picture at a steady 30 per second feels smoother than one that alternates between 60 and 40. On the 3DS both screens refresh at about 60 Hz, the GPU works asynchronously from the CPU, and **[citro3d](../../glossary.md#libctru)** decides when to wait for each. This page explains the timing model, how to structure the main loop so CPU and GPU overlap, how to lock to 60 or 30 fps, how to keep game speed independent of rendering speed, and how to detect missed frames.

Measuring where frame time goes is on [performance](../performance.md); buffer ownership rules are on [memory and cache](../memory-and-cache.md#buffer-lifetime).

## Before you start

- Checked against citro3d 1.7.1 and libctru 2.7.1 source. The loop structures below use real API names but are fragments; nothing on this page has been timed on hardware (Old 3DS: not run; New 3DS: not run).

## The timing model

Four things happen at their own pace:

1. **The CPU** runs your loop: input, game logic, recording GPU commands.
2. **The GPU** runs the recorded command list after `C3D_FrameEnd`, then the display transfers that copy each render target to its screen's framebuffer.
3. **The displays** scan out a framebuffer about 60 times per second and signal a **[V-blank](../../../glossary.md#vblank)** interrupt between frames, one per screen (`GSPGPU_EVENT_VBlank0` top, `VBlank1` bottom).
4. **Audio** is consumed by the DSP in 160-sample frames of about 4.9 ms, independent of all of the above ([audio](../audio.md)).

citro3d ties them together [documented: citro3d `renderqueue.c`]:

- `C3D_FrameBegin` waits until the GPU has finished **the previous frame's whole queue** (draws and transfers). With `C3D_FRAME_SYNCDRAW` it first waits for a V-blank on *both* screens (`C3D_FrameSync`). With `C3D_FRAME_NONBLOCK` it returns `false` instead of waiting for the GPU.
- `C3D_FrameEnd` submits the frame and returns at once.
- When the queue finishes, a callback swaps the screens' framebuffers; the new picture appears at the next V-blank.

So with `C3D_FRAME_SYNCDRAW`, at most one frame is in flight and the CPU never gets more than one frame ahead of the GPU — low latency, and simple ownership of GPU buffers.

## How many images the GPU draws

The GPU draws one image per render target per frame, not a fixed three. With the 3D slider off, a typical frame is a top image plus a bottom image (two); with 3D on, a left-eye and a right-eye top image plus the bottom (three) ([GPU](../../hardware/gpu.md)). A screen whose contents did not change needs no image at all: citro3d swaps a screen's framebuffer only if a target for it was drawn that frame (see Technical details), so a mostly static bottom screen can be skipped by not drawing its target [documented: citro3d `renderqueue.c`; the saving is unmeasured]. Check `gfxIs3D()` or the slider and skip the right-eye target when it is off. The 2DS family has no stereoscopic 3D, so this path is always the two-image case there.

## Overlapping CPU and GPU

Everything before `C3D_FrameBegin` runs while the GPU is still drawing the previous frame:

```c
while (aptMainLoop()) {
    hidScanInput();
    update_game();                      // overlaps with GPU work of the previous frame
    prepare_cpu_side_data();            // must not touch buffers the GPU is reading

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW); // GPU idle from here: safe to rewrite its buffers
    upload_dynamic_buffers();
    record_draws();
    C3D_FrameEnd(0);
}
```

Putting game logic between `C3D_FrameBegin` and `C3D_FrameEnd` serialises the two processors: the GPU idles while the CPU updates, then the CPU idles in the next `C3D_FrameBegin` [documented mechanism; the size of the gain depends on the workload and is unmeasured here]. If you must prepare GPU-visible data before `C3D_FrameBegin`, write it into a second copy of the buffer (double-buffering) and swap.

### Frame-ahead ownership

A more aggressive pipeline can prepare a later command buffer while an earlier frame executes. Keep each command buffer and every referenced texture or vertex buffer immutable until GPU completion; separately keep a framebuffer unavailable for reuse while it is pending presentation or scanout. Track these as distinct ownership transitions and measure CPU wait, GPU completion, presentation and input latency. More buffers can improve overlap but can also increase latency and memory use; the net result is [hypothesis] until measured on the target.

SM64DS Enhanced's [case study](../case-studies/case-studies-rendering-tradeoffs.md#ds-geometry-translated-to-native-draws) describes such a pipeline and a hardware flicker problem absent in its emulator. Its implementation depends on citro3d internals, so it is an example of the ownership problem, not a stable API recipe. [reported: project renderer, revision and test model unspecified]

### Resource ownership across a frame

For the conservative loop above, prepare ordinary CPU-only state while the previous frame executes, wait for the library's synchronization point, then write GPU-visible data, submit and preserve it until the next completion. Cache visibility and lifetime are separate: flushing makes CPU writes visible but does not grant permission to overwrite data still in use. Track display presentation separately from GPU completion when reusing scanout buffers.

For a ring or double-buffered design, assign each slot an owner and a completion condition: CPU-free → CPU-writing → submitted/GPU-reading → GPU-complete → CPU-free. Use a distinct slot when building ahead; wait or skip safely if no slot is free. Record which submission each timing value describes, and ensure a result from the previous frame is not attributed to the current one. An audio or loading worker needs bounded queues and must not hold a lock across blocking I/O; the system core also runs services, so thread placement needs [its own validation](../threading.md#proving-a-worker-improves-frame-time). These ownership rules do not constitute a tested asynchronous implementation; the advanced citro3d-internal approach in the case study is version-sensitive.

## Locking the frame rate

| Goal | How |
| ---- | --- |
| 60 fps, tied to the display | `C3D_FrameBegin(C3D_FRAME_SYNCDRAW)` every frame |
| Steady 30 fps (or 20, 15) | `C3D_FrameRate(30.0f)` once, then `C3D_FRAME_SYNCDRAW` as usual: the V-blank counters `C3D_FrameSync` waits on now advance only every second V-blank |
| As fast as possible (benchmarks only) | Omit `C3D_FRAME_SYNCDRAW`; the loop is then limited only by GPU completion, and frames may be shown unevenly |
| Never block the game thread on the GPU | `C3D_FrameBegin(C3D_FRAME_NONBLOCK)`; when it returns `false`, skip rendering this iteration (keep simulating) |

`C3D_FrameRate(fps)` accepts values above 0 and up to 60 and returns the previous setting. It works by a per-screen accumulator: each V-blank subtracts `fps` from it, and whenever it drops to zero or below, 60 is added back and the frame counter advances, so non-divisor rates such as 45 produce an uneven cadence [documented: `renderqueue.c`]. Prefer 60, 30, 20 or 15.

**Choose 30 deliberately.** If the slowest target cannot hold 60, a locked 30 avoids the judder of a rate that drifts between 40 and 60.

## Separating simulation from rendering

If the game advances one step per rendered frame, every slow frame slows the game down. Run the simulation on a fixed time step and let rendering show whatever state is current:

```c
const u64 STEP = (u64)(CPU_TICKS_PER_MSEC * 1000.0 / 60.0);   // one 60 Hz tick in system ticks
u64 previous = svcGetSystemTick(), lag = 0;

while (aptMainLoop()) {
    u64 now = svcGetSystemTick();
    lag += now - previous;
    previous = now;
    if (lag > 8 * STEP) lag = 8 * STEP;      // after a long stall, do not try to catch up forever

    while (lag >= STEP) {                     // fixed-rate game logic
        hidScanInput();
        update_game();
        lag -= STEP;
    }
    render(/* optionally interpolate with alpha = */ (float)lag / STEP);
}
```

- `svcGetSystemTick` runs at 268,111,856 Hz on all models, including New 3DS fast mode ([performance](../performance.md#cpu-time-the-system-tick)).
- Cap the catch-up (here 8 steps) so a long stall — loading, the HOME Menu, a debugger break — does not cause a burst of fast-forwarded steps.
- For games ported from a console with its own refresh rate (the DS refreshes at about 59.83 Hz, not 60), decide explicitly whether to run the logic at the original rate or at 60; small differences accumulate in music and timers.
- Report two numbers separately: logic ticks per second and distinct frames shown per second.
- If a separate render thread produces pictures that the display consumes (a pattern from a large port, [porting](../porting.md#lessons-reported-by-one-large-port)), also log how old the picture on screen is in logic ticks, and keep the hand-over queue short (one pending plus one in flight was the starting point in that port's notes); show the newest finished picture and drop unstarted older ones under load, but never drop logic ticks or audio `[hypothesis]` (from the unpublished notes of that port; not tested for this wiki).

Interpolation can show an intermediate pose without running another game tick. When the source does not expose stable object identities, matching across frames becomes part of the cost and can fail: test object creation and deletion, camera cuts, teleports and animation discontinuities, and fall back to an uninterpolated frame when a match is unreliable. Count simulation ticks, displayed frames and matching time separately. SM64DS Enhanced describes this approach; its default disables interpolation on Old 3DS. **New 3DS:** the supplied research reports it enabled by default. [reported: project interpolation implementation, revision and test conditions unspecified; [case study](../case-studies/case-studies-rendering-tradeoffs.md#ds-geometry-translated-to-native-draws)]

## Detecting missed frames

- `C3D_FrameCounter(0)` and `C3D_FrameCounter(1)` return the top and bottom screens' rate-limited V-blank counters. If the counter advanced by more than one between your `C3D_FrameBegin` calls, the frame took longer than its slot.
- `gspWaitForVBlank()` (top screen) blocks until the next V-blank; useful for loops that do not use citro3d.
- Keep a histogram of frame intervals (ticks between successive `C3D_FrameBegin` returns), not only an average: 59 good frames and one 100 ms hitch average out to "fine".

## Loading hitches

A load inside the frame loop is a missed-frame generator: file reads, decompression, texture uploads and allocation can each take many frames. Guidance, `[hypothesis]` unless labelled:

- **Convert offline, load raw.** Do format conversion, decompression-friendly packing and texture swizzling in the build ([asset pipeline](asset-pipeline.md)); at run time read large chunks into their final layout.
- **Move loads off the frame thread** and pass results through a bounded queue; the frame thread only checks "ready". A load worker on the Old 3DS's shared core 1 competes with system services ([threading](../threading.md#when-borrowing-core-1-can-make-the-game-slower)).
- **Budget per frame.** If a load must run on the frame thread, split it into slices that fit a time budget (for example 2 ms per frame, measured with ticks) and resume next frame.
- **Never log synchronously from the frame thread.** `[reported]` the G-Diffuser postmortem records main-thread log flushes of 26-270 ms on an Old 3DS build and "no hitches" after moving to an asynchronous log ([source](https://hypnotize.works/gdx-3ds/postmortem/native.html); its game only). Details in the [profiling cookbook](../performance.md#synchronous-sd-logging-causes-hitches).
- **Keep audio and input alive during loads** ([audio](../audio.md#choosing-buffer-sizes)); a loading screen still calls `aptMainLoop()`.
- **Measure the load, not the average.** Log the longest frame during a load with the stage that caused it, using the [frame-time helper](../performance.md#frame-time-distribution-and-missed-frames).

## Latency

Input read at the top of the loop is shown, at best, after the current frame's GPU work, the transfer and the next V-blank — roughly one to two display periods with `C3D_FRAME_SYNCDRAW` [hypothesis from the pipeline above; not measured]. Read input as late as practical before recording the frame if latency matters (rhythm games, fast action), and avoid adding extra queued frames.

See the [rendering trade-off case studies](../case-studies/case-studies-rendering-tradeoffs.md) for real port designs, fidelity compromises and suggested hardware comparisons.

## Lessons from ported games

- **Skip the picture, keep the tick.** sm64ds-enhanced's "dry" mode skips building a frame (no raster, no compose, no present) when a tick starts more than half a period behind the game clock, never two in a row, so logic catches up while the screens keep the last frame; the default on an Old 3DS with 60 fps off is "when behind" `[documented]` (`port3ds/game/main_game.cpp` near line 1236; [case study](../case-studies/case-study-sm64ds-enhanced.md)).
- **One gate can pace video, audio and frameskip.** Daedalus treats a change of the emulated video origin as a finished frame, derives a sync ratio from it, enters frameskip when the ratio is below 0.965, and clamps its audio sync rate to 1,500-4,000 `[documented]` (`GraphicsPluginCTR.cpp` lines 46, 202-206, 227; [case study](../case-studies/case-study-daedalus-3ds.md)).
- **Vsync through a library, no governor.** DXX's `gr_flip` blits and calls `SDL_Flip`; Quake2CTR's loop has an empty `Sys_Sleep` and swaps through picaGL. Neither skips frames, so a missed vsync is a visible hitch `[documented]` ([DXX](../case-studies/case-study-dxx-3ds.md), [Quake2CTR](../case-studies/case-study-quake2ctr.md)).
- **Fixed-rate logic.** The Sonic CD port adds frame limiting to its fixed-timestep engine `[reported]` (README credits). With a fixed logic rate, slow drawing shows as stutter rather than slow motion `[hypothesis]`.

## Technical details

- `C3D_FrameBegin(u8 flags)`: `C3D_FRAME_SYNCDRAW` = bit 0, `C3D_FRAME_NONBLOCK` = bit 1; returns `false` if already inside a frame or (non-blocking) if the GPU queue has not finished.
- `C3D_FrameSync()` loops on `gspWaitForAnyEvent()` until both screens' frame counters have changed.
- The swap happens in `onQueueFinish` via `gfxScreenSwapBuffers`, only for screens whose targets were drawn in that frame; the top screen is swapped in stereo mode only if the right-eye target was used.
- V-blank callbacks are registered on `GSPGPU_EVENT_VBlank0` and `GSPGPU_EVENT_VBlank1` with `gspSetEventCallback`.

## Pitfalls

- **Game logic inside the frame** (between `C3D_FrameBegin` and `C3D_FrameEnd`): no CPU/GPU overlap.
- **Using `C3D_FrameRate` for a fractional rate** (e.g. 45): uneven cadence.
- **Assuming one frame = 1/60 s in game logic** while the game runs at 30: everything moves at half speed.
- **Catching up without a cap** after HOME or sleep: the game fast-forwards.

## Open questions

- Measured input-to-photon latency with `C3D_FRAME_SYNCDRAW` versus a non-blocking loop.
- Exact refresh rate of the 3DS LCDs (often quoted as about 59.83 Hz) — not confirmed in the sources used here.

## Sources

- [citro3d](https://github.com/devkitPro/citro3d) — `source/renderqueue.c` (`C3D_FrameBegin`, `C3D_FrameSync`, `C3D_FrameRate`, `C3D_FrameCounter`, queue callback), `include/c3d/renderqueue.h`.
- [libctru](https://github.com/devkitPro/libctru) — `services/gspgpu.h` (events, `gspWaitForVBlank`), `os.h` (tick rate), `source/ndsp/ndsp.c` (sound frame length).
- [G-Diffuser: Road to 60, All Over Again](https://hypnotize.works/gdx-3ds/postmortem/native.html) — `[reported]` synchronous-logging hitches and the effect of an asynchronous log (its game only).
- [GBATEK](https://problemkaputt.de/gbatek.htm) — DS display timing (59.8261 Hz) for ports.
