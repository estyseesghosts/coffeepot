# Case study: Xash3DS (Half-Life on the 3DS)

A port of Xash3D FWGS — a clean-room Gold Source–compatible engine — to the 3DS. All facts are `[documented]` in the vendored tree (`private_please_gitignore/Xash3DS/`); nothing was built or run. The strategy is containment: one static binary, a fixed 400x240 renderer through an OpenGL wrapper, stubbed platform features instead of emulated ones.

## Before you start

- Evidence: every behaviour is `[documented]` in the named file. No `[measured]` frame rates exist here.
- Model: Old 3DS is the default. New 3DS differences are flagged.
- Build: devkitARM, `-march=armv6k -mtune=mpcore -mfloat-abi=hard`, picaGL plus static `mainui`/`client`/`server` libs.

Jargon: the **[ARM11](../../glossary.md#arm11)** is the 3DS CPU; the **[PICA200](../../glossary.md#pica200)** is the 3DS GPU; a **[framebuffer](../../../glossary.md#framebuffer)** is the pixel buffer sent to a screen. picaGL (OpenGL over citro3d wrapper) has no glossary entry.

## Renderer: fixed-function OpenGL at 400x240

Video (`engine/platform/ctr/vid_ctr.c`) is a thin skin over picaGL. `VID_SetMode` hardcodes 400x240 — one top-screen framebuffer, no mode list, no second eye. `GL_InitExtensions` declares OpenGL 1.1 and disables nearly everything: no vertex buffer objects, no **[shaders](../../../glossary.md#shader)**, no NPOT textures, no compression, no anisotropy. Survivors: two texture units, edge clamping, 2D textures capped at 256 px (via the `gl_max_texture_size` cvar). `GL_GetProcAddress` returns `NULL`, so renderer code cannot reach past this subset. **New 3DS:** the code probes `APT_CheckNew3DS` but changes nothing; rendering is identical.

Tradeoff: no renderer redesign, at the price of 256 px textures and single-eye output.

## Client/server split without DLLs

Gold Source loads game logic as DLLs. The Makefile instead builds `-DSINGLE_BINARY -DXASH_INTERNAL_GAMELIBS -DXASH_GL_STATIC` and links everything statically, while `engine/platform/ctr/dll_ctr.c` fakes the loader: `dll_register` maps names to export tables and `dlopen`/`dlsym` resolve against the list. The DLL boundary survives as an in-process registry.

## Assets and filesystem: convert at build, keep paths exact

The Makefile converts assets up front: `.t3s` to `.t3x` via `tex3ds`, `.v.pica`/`.shlist` to `.shbin` **[shader](../../glossary.md#shbin)** blobs via `picasso`, linked in with `bin2o` or shipped under `engine/platform/ctr/assets`. Nothing converts at runtime. Filesystem (`engine/common/filesystem.c`) carries three 3DS carve-outs: case-insensitive matching forced off, POSIX-only branches excluded, no double slashes in built paths. Content must ship with exact-case paths.

## Input, audio, saves

Input (`engine/platform/ctr/in_ctr.c`) maps the pad onto engine keys (D-pad to arrows/wheel, L/R to mouse buttons, ZL to Ctrl, face buttons to Enter/Space/`f`/`e`) and feeds both sticks to `Joy_AxisMotionEvent` with dead zones. The bottom screen shows a touch overlay or full keyboard bitmap; taps become key events and drags elsewhere become motion events.

Audio (`engine/platform/ctr/snd_ctr.c`) is one looping stereo PCM16 stream through the audio DSP (a **[DSP](../../../glossary.md#dsp)** is a processor for streaming work like audio): a 4096-sample `linearAlloc`'d buffer (16 KB of **[linear heap](../../glossary.md#linear-heap)** — physically contiguous memory for hardware buffers) on ndsp channel 0. `SNDDMA_Submit` flushes the data cache so the DSP hears fresh samples. **New 3DS:** same channel and buffer.

Saves are the weak point: `SV_AutoSave_f` (`engine/server/sv_cmds.c`) is compiled out on `_3DS` — "Slow and causes crashes sometimes on the O3DS". Manual saves only.

## Packaging and technical details

`Makefile` uses `-O2 -ffast-math` with dead-section stripping. `cia.rsf` asks for 80 MB, core 0, priority 16, 0x200 handles. **New 3DS:** the extended block requests 124 MB, 804 MHz, L2 and core 2 — granted or not, no engine code branches on the model.

| Item | Value | File |
| ---- | ----- | ---- |
| Resolution | 400x240, single eye, hardcoded | `vid_ctr.c` (`VID_SetMode`) |
| GL subset | 1.1, 2 texture units, 256 px max, no VBO/GLSL/NPOT/S3TC | `vid_ctr.c` (`GL_InitExtensions`) |
| Gamelibs | static link + registry `dlopen`/`dlsym` | `dll_ctr.{c,h}`, `Makefile` |
| Audio | ndsp ch0 stereo PCM16, 4096-sample loop, flush per submit | `snd_ctr.c` |
| Input | 14-button map, dual sticks, bottom keyboard overlay | `in_ctr.c` |
| Saves | autosave disabled on `_3DS` | `sv_cmds.c` |
| CIA ask | 80 MB core 0; **New 3DS:** 124 MB/804 MHz/L2/core 2 | `cia.rsf` |
| FS quirks | case-sensitive, no `//` | `filesystem.c` |

## Lesson table

| # | Symptom | Fix (as done here) |
| - | ------- | ------------------ |
| 1 | Engine expects loadable DLLs; console has none | Link statically; keep a name→exports registry so `dlopen`/`dlsym` work (`dll_ctr.c`) |
| 2 | Renderer wants extensions the GPU lacks | Declare GL 1.1, disable all unsupported extensions, cap textures at 256 px (`vid_ctr.c`) |
| 3 | Runtime texture/shader conversion costs frames | Convert `.t3s`→`.t3x`, `.v.pica`→`.shbin` in the Makefile; link results in |
| 4 | DSP plays stale audio | Hold one looping `linearAlloc` buffer; `DSP_FlushDataCache` on every submit (`snd_ctr.c`) |
| 5 | No keyboard for console and saves | Draw the keyboard into the bottom framebuffer; taps become key events (`in_ctr.c`) |
| 6 | Autosave freezes/crashes on Old 3DS | Compile autosave out; manual saves only (`sv_cmds.c`) |
| 7 | File-not-found on SD but not PC | Ship exact-case paths; matching is case-sensitive and rejects `//` (`filesystem.c`) |

## Open questions

- What frame rate does a level reach on an Old 3DS, and where — scene traversal or picaGL submission? No figure is stated in the tree.
- Does the 256 px cap visibly degrade content? Untested here.
- Is the autosave crash filesystem latency or heap exhaustion? Unresolved.

## Sources

- `private_please_gitignore/Xash3DS/engine/platform/ctr/{vid_ctr.c,snd_ctr.c,in_ctr.c,dll_ctr.c,dll_ctr.h}` — video, audio, input, loader shim.
- `private_please_gitignore/Xash3DS/{Makefile,cia.rsf}` — flags, asset rules, title configuration.
- `private_please_gitignore/Xash3DS/engine/{common/filesystem.c,server/sv_cmds.c}` — filesystem carve-outs, autosave removal.
