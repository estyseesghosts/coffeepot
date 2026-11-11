# Case study: DXX-3DS (Descent 1/2 port)

A Nintendo 3DS port of Descent I and II, forked from DXX-Rebirth via DXX-Switch/DXX-Retro by RossMeikleham. It keeps the original 6-degrees-of-freedom engine — portal-rendered mines, software texture mapping, MIDI-driven music — and runs it through SDL with a small set of `__3DS__` patches. Nothing below was measured by Coffeepot; all behaviour is `[reported]` by the port README or `[documented]` in its patched files.

## Before you start

- Evidence: all behaviour is `[reported]` by the [port README](https://github.com/RossMeikleham/DXX-3DS) or `[documented]` in a named file. No timing figure is stated in the repo.
- Scope: two builds from one tree (`d1/`, `d2/`). The README says it could not get a working CIA and suspects the stack size; 3DSX builds work (see below).
- Old 3DS is the default model. The repo states no New 3DS path; differences are marked where they would apply.
- Companion pages: [porting](../porting.md), [performance](../performance.md), [2D rendering](../PICA200/rendering-2d.md), [audio](../audio.md), [files and saves](../filesystem-and-saves.md), [threading](../threading.md).

Jargon used here: the **[ARM11](../../glossary.md#arm11)** is the 3DS CPU; main memory is **[FCRAM](../../glossary.md#fcram)**; the **[PICA200](../../glossary.md#pica200)** is the 3DS GPU; a **[framebuffer](../../../glossary.md#framebuffer)** holds the pixels of one image; a **[draw call](../../glossary.md#draw-call)** is one request to the GPU.

## What makes it tick

**Engine.** Descent's core is a portal renderer over cube-segment mines: visibility walk, then CPU texture-mapped spans into an 8-bit canvas (`d1/3d/`, `d1/texmap/`, `d1/2d/`). The port keeps that pipeline and replaces only the platform seams under `d1|d2/arch/sdl/` — video, events, audio, startup (`[documented]`).

**Software renderer, no GL path.** `gr_set_mode` forces 320x240 on `__3DS__` and draws into an 8-bit SDL canvas, blitted to screen (`[documented]` in `d1/arch/sdl/gr.c`). Per-pixel work stays on the ARM11 every frame.

**Audio.** HMP-to-MIDI fails on 3DS, so `mix_play_file` falls back to `/wav/` then `../mp3/` copies the player provides (`[documented]` in `d1/arch/sdl/digi_mixer_music.c`). libphysfs for ctru is a build dependency (`[reported]`).

**Networking removed.** Multiplayer is disabled (`[reported]`). The engine's IPX-era netcode has no 3DS backend here; single-player loopback is all that runs.

## How it pushes 3DS hardware

**ARM11 fill cost is the whole budget.** A 320x240x8-bit frame is ~77 KB of CPU-written pixels per frame plus span setup, palette, and HUD — all on the single game core. 320x240 is 76,800 pixels, 80% of the top screen's 96,000, and the engine's own output is an 8-bit canvas that SDL blits (`[documented]` in `gr.c`). **New 3DS:** a faster clock shortens the same loop, but the repo names no New 3DS path, so treat gains as `[hypothesis]`.

**Memory: stack, not heap, is the documented fight.** DXX-Rebirth uses ~4-8 MB of stack; the default 3DS stack (32 KB) crashes, so `d1/main/inferno.c` sets `__stacksize__ = 8 * 1024 * 1024` (`[documented]` in that file; commit `761079f`, "Added stack memory to D1X", and `e81c857`, which also changed the resolution to 320*240). The README reports that setting `__stacksize__` to 8 MB works from the homebrew browser but CIA builds crash, and it only suspects the stack as the cause (`[reported]`). libctru's default stack is 32 KB (`__stacksize__` in `libctru/source/system/stack_adjust.s`) (`[documented]`). > Unknown: peak FCRAM use and whether D2 exceeds the Old 3DS application budget.

**Frame pacing.** `gr_flip` is blit-then-`SDL_Flip`, i.e. vsync through SDL rather than an explicit limiter (`[documented]` in `gr.c`). No frame-skip or governor exists; missed vsyncs read as hitches, not slow motion.

**Input.** 3DS controls arrive as SDL joystick/key events; joystick-button handling is compiled out on `__3DS__` while axis motion is kept (`[documented]` in `arch/sdl/event.c`), and exit funnels into an `aptMainLoop` home-button wait (`[documented]` in `arch/sdl/init.c`). Level-select and multi-save paths are disabled, and the D1 end-of-level sequence plus D2 movies are skipped to avoid crashes (`[reported]`).

## What it got right and wrong

Right: forcing 320x240 avoided a GPU rewrite entirely; the WAV/MP3 music fallback turned an unfixable decoder gap into an asset instruction; the 8 MB stack fix is one line with a clear cause. Wrong: no working CIA was achieved; level exits and movies crash rather than degrade, so content was cut; saves are hardcoded to `player.plr`.

## Technical details

- Sources read: `README.md`, `d1/arch/sdl/gr.c` (320x240, canvas, palette), `init.c` (init order, exit loop), `digi_mixer_music.c` (WAV/MP3 fallback), `event.c`, `d1/main/inferno.c` (8 MB stack), `make_3ds.sh`, `Dockerfile`.
- Build: devkitARM SDL + SDL_mixer + physfs-3ds; `bannertool`/`makerom` for CIA (`[reported]`).
- Single-player only; `player.plr` hardcoded; Steam files tested, GOG/CD-ROM untested (`[reported]`).

## Lesson table: software-renderer FPS rules

Each rule is a `[hypothesis]` generalised from this port until measured on your workload.

| # | Symptom on Old 3DS | Fix |
| - | ------------------ | --- |
| 1 | CPU span rasteriser misses vsync | Render at 320x240 or below; centre at present, never per span |
| 2 | Crash when entering a level in a stack-hungry engine | Raise `__stacksize__` (libctru default 32 KB) into the MB range; test CIA and 3DSX builds separately, since the README reports they behaved differently |
| 3 | HMP/MIDI music silent or erroring | Ship WAV/OGG/MP3 alongside; fall back by extension at load time |
| 4 | Cutscene or level-transition crash blocks shipping | Stub the sequence behind a flag first, then fix; never let a movie gate the game |
| 5 | Inputs fire twice or stick buttons do nothing | Map 3DS controls at the SDL event layer; compile out unused paths |
| 6 | Save corruption or missing saves | Name one save file per slot explicitly; never hardcode a single path past prototype |

## Open questions

- No figure exists for Old 3DS frame time in a busy mine; a tick span around `gr_flip` would settle it.
- Why do CIA builds crash when 3DSX works? A CIA with the same 8 MB stack plus a memory report would settle it.

## Sources

- [RossMeikleham/DXX-3DS](https://github.com/RossMeikleham/DXX-3DS) commit `cdb4cc3` (2018-12-07; see [source versions](../source-versions.md)): `README.md` (assets, music fallback, known issues); `d1/arch/sdl/gr.c` (320x240 at line 97), `init.c`, `event.c`, `digi_mixer_music.c`; `d1/main/inferno.c` (8 MB `__stacksize__` at line 37); `make_3ds.sh`, `Dockerfile`.
- libctru `libctru/source/system/stack_adjust.s` — default 32 KB `__stacksize__` (see [source versions](../source-versions.md)).
