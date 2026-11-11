# A first Switch game, start to finish

This page walks through one small, complete homebrew game: a square you move over a tile map with the left stick or D-pad, coins to collect, a beep on pickup, progress saved to the SD card, frame-time and error logging sent to your PC, and a crash dump if the program faults. It is for a developer who has never built a Switch program and wants one working program that touches every basic system, before choosing between SDL2, a software framebuffer and [deko3d](rendering.md). The whole program is `nx/engineering/examples/first-game/source/main.c` (about 280 lines, one file, no assets).

## Before you start

- Toolchain: devkitA64 r30-1, `libnx` 4.12.0-1, checked as described on [source versions](source-versions.md). Install steps are on [toolchain and workflow](../homebrew/toolchain-and-workflow.md).
- Verification: the program compiles from a clean copy with `-Wall` and zero warnings. It has **not been run** on a console or an emulator, so every behaviour below that depends on running it is `[hypothesis]` and marked.
- It uses the libnx software framebuffer (`Framebuffer`, as in the `graphics/simplegfx` example), not SDL2 and not `deko3d`. SDL2 was not installed on the machine used to check this page, so there is no compiled SDL2 version here; the [open-engine case studies](case-studies/case-studies-open-engines.md) show how SDL2 ports are structured.
- A console that can run homebrew, set up as described on [Atmosphere and running](../homebrew/atmosphere-and-running.md).

## Build and run

```sh
cp -r nx/engineering/examples/first-game ~/projects/firstgame     # a path without spaces
cd ~/projects/firstgame
make                                                              # produces firstgame.nro
nxlink -a <console-ip> -s firstgame.nro                           # console in NetLoader mode (press Y in hbmenu)
```

Expected: `make` ends with `built ... firstgame.nro`; with `nxlink` the PC terminal prints `audio ok, handheld` (or `docked`) and, every 120 frames, a `frame ms: avg ... worst ...` line `[hypothesis]` (not run). Without `nxlink`, copy the `.nro` to `/switch/` on the SD card and start it from the homebrew menu; `printf` output goes nowhere. How the loop works and what the commands do is on [toolchain and workflow](../homebrew/toolchain-and-workflow.md#print-debug-text-to-the-pc-with-nxlink).

## The loop in order

1. **Logging first.** `socketInitializeDefault()` then `nxlinkStdio()`; from then on `printf` and `stderr` reach the PC when started with `nxlink -s`. Sockets must be initialised first `[documented]` (`libnx` `nx/include/switch/runtime/nxlink.h`; `switch-examples` `network/nxlink_stdio`).
2. **Input.** `padConfigureInput(1, HidNpadStyleSet_NpadStandard)`, `padInitializeDefault(&pad)`, and `padUpdate(&pad)` once per frame. `padGetButtons` is "held", `padGetButtonsDown` is "pressed this frame", `padGetStickPos(&pad, 0)` is the left stick (range about ±32767, up is positive y so the program flips it) `[documented]` (`libnx` `pad.h`; the `switch-examples` template). The program applies a 0.15 dead zone. The older `hidScanInput` calls are gone in current libnx (see [libnx and deko3d](../homebrew/deko3d-and-libnx.md)).
3. **Handheld, docked and focus.** `appletHook` registers a callback for operation-mode and focus changes; the program records `g_docked` and `g_focused` and stops moving the player while out of focus `[documented]` (`libnx` `applet.h`). The framebuffer is a fixed 1280×720; whether the system scales it for a 1080p dock output was not tested `[hypothesis]`.
4. **Drawing.** `framebufferCreate(&fb, nwindowGetDefault(), 1280, 720, PIXEL_FORMAT_RGBA_8888, 2)` then `framebufferMakeLinear`; each frame `framebufferBegin` returns a pointer and a byte stride, you write RGBA pixels, and `framebufferEnd` presents. The program redraws every tile each frame with a simple `fillRect`, which is deliberately naive: a tile map this size is a pixel-fill test of the ARM cores, not a model to copy `[documented]` for the API (`switch-examples` `graphics/simplegfx`), `[hypothesis]` for speed.
5. **Movement and collision.** Speed is in pixels per second multiplied by a frame time measured with `armGetSystemTick` and `armTicksToNs`, clamped to 0.1 s so a stall (sleep, the Home menu) does not teleport the player. X and Y are tested separately against the four corners of the player box so the square slides along walls.
6. **Audio.** `audoutInitialize`, `audoutStartAudioOut`, then `audoutAppendAudioOutBuffer` with a 100 ms sine buffer generated once at start. The output rate is 48,000 Hz stereo 16-bit; the buffer must be `memalign`ed to `0x1000` and its size rounded up to `0x1000` `[documented]` (`switch-examples` `audio/playtone`). A buffer already queued is not re-queued until the system returns it with `audoutGetReleasedAudioOutBuffer`. This is the simplest working path; `audren` is the richer mixer (see [services and IPC](../software/services-and-ipc.md)).
7. **Saving.** On `A` and on exit the program writes `{x, y, coins}` to `sdmc:/switch/firstgame/save.dat` through a temporary file, `fsync`, and a rename that keeps the previous generation as `save.bak`; at start it loads `save.dat`, or `save.bak` if that fails. This is the crash-safe sequence from [performance and porting](performance-and-porting.md#crash-safe-saves). Its two assumptions (rename-over-existing and `fsync` reaching the card) are untested, so test by pulling power mid-write before trusting it `[hypothesis]`. Files in `/switch/` are not system save data, and system save data needs a different API (`switch-examples` `fs/save`); see [firmware](../software/firmware.md) for where each lives.
8. **Crash dump.** The program defines `__nx_exception_stack` and `__libnx_exception_handler`, which libnx calls on a CPU exception; the handler writes the error description, `pc`, `lr`, `sp` and `far` to `sdmc:/switch/firstgame/crash.txt` `[documented]` (`switch-examples` `exception-handler`). See [debugging](debugging.md) for turning `pc` into a source line.
9. **Quit.** `+` exits the loop; the program saves, stops audio, closes the framebuffer and the socket. `appletMainLoop()` also returns false when the system asks the program to exit.

## Make it yours

Replace `fillRect` tiles with textures only after you have measured the fill cost (see [performance and porting](performance-and-porting.md#measure-the-way-the-ports-learned-to)). To get GPU drawing, move to [rendering](rendering.md); to get a mixer, move to `audren`. The order that worked for the ports in the [case studies](case-studies/case-study-sa2.md) was: get a correct, slow frame first; add timing; then optimise one thing at a time.

## Pitfalls

- **Pressing `A` saves instantly but the first save creates the folder.** `writeSave` calls `mkdir` first; if you change the path to somewhere without a parent directory, `fopen` fails and the program prints `save FAILED`.
- **Audio crackle or silence.** The buffer alignment and size rules above are the usual cause; also check that `audoutInitialize` returned success (the program falls back to no audio).
- **Black window docked.** If the picture is missing only in the dock, start by logging `g_docked` and the framebuffer size; this program never tested that path `[hypothesis]`.
- **`printf` shows nothing.** `nxlinkStdio()` was not reached, sockets were not initialised first, or the console is not in NetLoader mode.

## Open questions

- Does the 1280×720 framebuffer scale correctly on a 1080p dock output? Run it docked.
- How long does the naive per-pixel redraw take per frame on each model? The program already prints average and worst frame time; run it on an Erista and a Mariko console.
- Is the audio path glitch-free over several minutes? Run it for ten minutes with coin pickups.

## Sources

- [switch-examples](https://github.com/switchbrew/switch-examples) at `669786898205b7beb25ff1731e72982e6d0397d3` — `graphics/simplegfx`, `audio/playtone`, `exception-handler`, `network/nxlink_stdio`, `templates/application`, `fs/save`.
- [libnx](https://github.com/switchbrew/libnx) at `b156b4774fb2df7b93402169509266da7547fc4c` — `nx/include/switch/runtime/pad.h`, `services/applet.h`, `services/audout.h`, `display/framebuffer.h`, `arm/counter.h`.
- `nx/engineering/examples/first-game/` — the program built for this page.
