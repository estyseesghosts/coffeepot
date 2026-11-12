# A first Switch game, start to finish

This page walks through one small, complete homebrew game: a square you move over a tile map with the left stick or D-pad, coins to collect, a beep on pickup, progress saved to the SD card, frame-time and error logging sent to your PC, and a best-effort crash dump. It is for a developer who has never built a Switch program and wants one working program that touches every basic system, before choosing between SDL2, a software framebuffer and [deko3d](deko3d-renderer.md). The program is `nx/engineering/examples/first-game/` (one `main.c` of about 400 lines plus the shared `examples/common/` code, no assets).

## Before you start

- Toolchain: devkitA64 r30-1, `libnx` 4.12.0-1, checked as described on [source versions](source-versions.md). Install steps are on [toolchain and workflow](../homebrew/toolchain-and-workflow.md). A console that can run homebrew is set up as on [Atmosphere and running](../homebrew/atmosphere-and-running.md).
- Verification: builds with `-Wall` and no warnings. 1,184 **host-side checks** pass for the save format and audio padding (`nx/tests/run.sh`). A scripted **self-test ran in the Eden 0.2.1 emulator**: the square walks to a coin, the game saves, reloads through the real SD path, and checks the coin is still collected and the second save goes to the other slot (all pass), and a frame dump shows the expected picture. **Not run on a console.** Audio output, docked behaviour, suspend, real frame times and power-loss behaviour remain untested; behaviour below that depends on them is `[hypothesis]`.
- It uses the libnx software framebuffer (`Framebuffer`, as in `graphics/simplegfx`), not SDL2 and not `deko3d`. For how SDL2 ports are structured, see the [open-engine case studies](case-studies/case-studies-open-engines.md).

## Build and run

```sh
cp -r nx/engineering/examples/first-game nx/engineering/examples/common ~/projects/   # a path without spaces; keep the two side by side
cd ~/projects/first-game
make                                                         # produces first-game.nro
nxlink -a <console-ip> -s first-game.nro                     # console in NetLoader mode (press Y in hbmenu)
```

Expected: `make` ends with `built ... first-game.nro`; with `nxlink` the PC terminal prints `audio ok, handheld, nxlink connected` (or `docked`) and, every 120 frames, one line of per-phase times `[hypothesis]` (not run). Without `nxlink` the game runs normally and `printf` goes nowhere. Copy the `.nro` to `/switch/` for the homebrew menu. How `nxlink` works is on [toolchain and workflow](../homebrew/toolchain-and-workflow.md#print-debug-text-to-the-pc-with-nxlink).

**Scripted acceptance run** (the same checks that passed in Eden, for a console): `make CFLAGS_EXTRA=-DFG_SELFTEST`, run it, then read `sdmc:/switch/firstgame/selftest.txt`. Every line should end `pass`: `coin_collected`, `save_ok`, `load_ok`, `coin_stays_collected`, `restart_position`, `second_save_other_slot`, `audio_ok`. It also writes `frame150.rgba` (the 1280×720 picture after the coin is collected).

## The loop in order

1. **Logging is optional.** `socketInitializeDefault()` (result checked), then `nxlinkStdio()`, which returns a socket descriptor, or a negative number when no `nxlink` host is attached. The program keeps the descriptor, runs normally if it is negative, and `close()`s it at shutdown `[documented]` (`libnx` `runtime/nxlink.h`: "the socket should be closed with close() during application cleanup").
2. **Input.** `padConfigureInput(1, HidNpadStyleSet_NpadStandard)`, `padInitializeDefault`, `padUpdate` once per frame. `padGetButtons` is "held", `padGetButtonsDown` "pressed this frame", `padGetStickPos(&pad, 0)` the left stick (about ±32767, up is positive y, so the program flips it); a 0.15 dead zone applies `[documented]` (`libnx` `pad.h`). The old `hidScanInput` calls are gone (see [libnx and deko3d](../homebrew/deko3d-and-libnx.md)).
3. **Focus and dock.** `appletHook` records operation mode and focus; movement stops while unfocused and the frame clock is reset on every focus change. See [lifecycle and threading](lifecycle-and-threading.md).
4. **Drawing.** `framebufferCreate(... 1280, 720, PIXEL_FORMAT_RGBA_8888, 2)` then `framebufferMakeLinear`. **Both can fail** (`Result`), and linear mode allocates another buffer, so the program checks them, prints the code and takes a controlled shutdown path instead of drawing into a missing buffer. Each frame `framebufferBegin` returns a pointer and byte stride, you write RGBA pixels, and `framebufferEnd` presents. The tile map is redrawn naively with `fillRect`; that is a fill test of the ARM cores, not a model to copy.
5. **Frame times.** Four numbers, each taken around the call that can block: `begin` (`framebufferBegin`, which dequeues a buffer and waits if the display holds them all), `draw` (update and pixel writes), `present` (`framebufferEnd`, which in linear mode also copies and converts the image, then queues it), and `loop` (time between iteration starts). They show *where* a long frame went on the CPU side; they do not measure GPU execution, which this program does not use. See [profiling](profiling-and-benchmarks.md).
6. **Movement and collision.** Speed in pixels per second times a measured frame time, clamped to 0.1 s so a stall (sleep, Home) does not teleport the player; X and Y are tested separately against the player box's four corners so the square slides along walls.
7. **Audio.** `audoutInitialize`, `audoutStartAudioOut`, then `audoutAppendAudioOutBuffer` with a 100 ms sine buffer generated once. 48,000 Hz, stereo, 16-bit; the buffer is `memalign`ed to `0x1000` with its size rounded up `[documented]` (`libnx` `audout.h`). The program polls `audoutGetReleasedAudioOutBuffer` every frame, does not re-queue a buffer the system still owns, and counts the beeps it had to skip. For continuous sound see [audio](audio.md).
8. **Saving.** Press `A`, or quit, to save. The state is position, score **and the set of collected coins** (a 110-byte bitset), in a versioned, checksummed 138-byte format written alternately to `save0.dat` and `save1.dat`; at start the newer valid slot loads. Every write step is checked, and the previous save is never overwritten. The first version of this game saved only the coin total, so collected coins respawned and could be collected again; the self-test now checks that regression. Design, failure tests and what is unverified: [storage and saves](storage-and-saves.md). These are SD-card files, not system save data (see [firmware](../software/firmware.md)).
9. **Crash dump (best effort).** The program defines `__nx_exception_stack` and `__libnx_exception_handler`, which libnx calls on a CPU exception; the handler tries to write `pc`, `lr`, `sp`, `far` and the error description to `crash.txt` `[documented]` (`switch-examples` `exception-handler`). After an arbitrary fault the filesystem and C library may be unusable, so treat the file as a bonus; Atmosphère's crash report is the dependable record ([debugging](debugging.md)).
10. **Quit.** `+` or an exit request ends the loop; the program saves, stops audio, closes the framebuffer, unhooks the applet callback, closes the nxlink socket and exits sockets.

## Make it yours

Replace `fillRect` tiles with textures only after you have measured the fill cost ([profiling](profiling-and-benchmarks.md)). For GPU drawing move to the [renderer](deko3d-renderer.md); for a mixer, to [audio](audio.md). The order that worked for the ports in the [case studies](case-studies/case-study-sa2.md): get a correct, slow frame first; add timing; then optimise one thing at a time.

## Technical details

- Files: `source/main.c`; `common/world.[ch]` (map, coin bitset), `common/save_format.[ch]` (format and slot logic, no `libnx`), `common/pcm_pad.[ch]`.
- Save paths: `sdmc:/switch/firstgame/save0.dat`, `save1.dat`; report and dumps in the same folder.
- A hardware acceptance list for this program (saves, crash, audio, controller loss, suspend/resume, dock transitions) is in [source versions](source-versions.md#hardware-test-manifest-first-game).

## Pitfalls

- **First save creates the folder.** `slotWrite` calls `mkdir` first; a path without a parent makes `fopen` fail and the program prints `save FAILED`.
- **Audio crackle or silence.** Buffer alignment or size, or `audoutInitialize` failing (the program continues without sound).
- **Black picture docked.** Log `g_docked` and the framebuffer size first; the dock path was never run.
- **`printf` shows nothing.** `nxlinkStdio()` returned negative, sockets were not initialised first, or the console is not in NetLoader mode.

## Open questions

- Does the 1280×720 framebuffer scale to a 1080p dock output? Run it docked.
- What do the four frame-time numbers look like on an Erista and a Mariko console?
- Is audio glitch-free over ten minutes with frequent pickups?
- What does a power cut during a save leave behind?

## Sources

- [switch-examples](https://github.com/switchbrew/switch-examples) at `669786898205b7beb25ff1731e72982e6d0397d3` — `graphics/simplegfx`, `audio/playtone`, `exception-handler`, `network/nxlink_stdio`, `templates/application`, `fs/save`.
- [libnx](https://github.com/switchbrew/libnx) at `b156b4774fb2df7b93402169509266da7547fc4c` — `runtime/pad.h`, `runtime/nxlink.h`, `services/applet.h`, `services/audout.h`, `display/framebuffer.h`, `arm/counter.h`.
- `nx/engineering/examples/first-game/`, `nx/engineering/examples/common/`, `nx/tests/` — the program, its shared code and its host tests.
