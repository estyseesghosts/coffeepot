# The devkitPro 3ds-examples catalogue

The `3ds-examples` repository is devkitPro's collection of small, public-domain sample programs for 3DS homebrew: print text, read the buttons, draw a triangle, play a sound, talk to the network. Each one is a complete project with a Makefile, so it is the quickest way to see a libctru, citro3d or citro2d call used correctly. This page lists every example, says what it demonstrates and which model it needs, and points to the page in this wiki that explains the topic in depth (or says plainly that there is none yet).

It is for anyone starting a project who asks "is there a sample for this?", and for an agent that needs to know which wiki page backs a given sample.

## What you need

- The toolchain from [development workflow](development-workflow.md). The repository's `templates/application` Makefile is the one that guide builds from.
- To run on hardware: a console with [Luma3DS](luma3ds.md) and the Homebrew Menu, or an emulator ([testing on hardware](../engineering/testing-on-hardware.md)).
- Audio examples need the DSP firmware dump `sdmc:/3ds/dspfirm.cdc` (see [Audio](#audio)).

## Catalogue

Each row is one example directory. "Model" is Old 3DS or New 3DS availability as far as the example's own README or source says; where it says nothing the cell reads "not stated". The wiki guide column links to the best matching page; "none yet" means the sources read for this sweep found no page on the topic.

### Templates and basics

| Example | Demonstrates | Model | Wiki guide |
| ------- | ------------ | ----- | ---------- |
| `templates/application` | Starter project for a libctru application (the Makefile used throughout the wiki) | not stated | [development workflow](development-workflow.md#4-build-your-first-3dsx), [asset pipeline](../engineering/asset-pipeline.md) |
| `templates/library` | Starter project for a 3DS static library | not stated | [development workflow](development-workflow.md#6-grow-the-project) (library packaging itself: none yet) |
| `get_system_language` | `cfguInit`, `CFGU_GetSystemLanguage`: reads the language code from the config savegame | not stated | [IPC and services](../engineering/ipc-and-services.md), [services](../software/services.md) |
| `time/rtc` | `time(NULL)` / `gmtime` for the clock, with a hand-written day-of-week calculation; redraws once per frame | not stated | [libctru](libctru.md) (clock); none yet for date handling |
| `threads/thread-basic` | `threadCreate` with priority one higher than the main thread (a lower number); the source comment says child threads must have higher priority than main or stdio locking starves them | not stated | [threading](../engineering/threading.md) |
| `threads/event` | A worker thread blocked on `svcWaitSynchronization` on an event, woken by `svcSignalEvent`, joined at exit | not stated | [threading](../engineering/threading.md) |
| `app_launch` | `aptSetChainloader` with a hard-coded camera-app title ID (the EUR one; other regions must edit it). README: only works launched as a regular app from HOME, not from the homebrew menu | not stated | [application lifecycle](../engineering/application-lifecycle.md) |
| `libapplet_launch` | `aptLaunchLibraryApplet` for extrapad, error, Mii editor ("appleted"), memolib, photo and sound selectors, eShop ("mint") and the software keyboard; defines `_aptDebug` to trace APT events | not stated | [application lifecycle](../engineering/application-lifecycle.md#software-keyboard-and-other-library-applets) |
| `mii_selector` | `miiSelectorInit`, `SetTitle`/`SetOptions`/`SetInitialIndex`/`BlacklistUserMii`, `miiSelectorLaunch`, then `miiSelectorChecksumIsValid` and reading name, author, birthday, sex and shirt colour from the return buffer. The config must be initialised with `miiSelectorInit` first or behaviour is undefined (source comment) | not stated | none yet; launching library applets is in [application lifecycle](../engineering/application-lifecycle.md#software-keyboard-and-other-library-applets) |

### Graphics: text and bitmaps

| Example | Demonstrates | Wiki guide |
| ------- | ------------ | ---------- |
| `graphics/printing/hello-world`, `both-screen-text`, `colored-text`, `multiple-windows-text`, `wide-console` | libctru's text console (`consoleInit`) on one or both screens, ANSI colour escapes, several console windows, and the 800-pixel-wide console | [debugging](../engineering/debugging.md) for text output as a debug tool; [gpu rendering](../engineering/gpu-rendering.md) for wide mode; the console API itself: none yet |
| `graphics/printing/system-font` | Using the system's shared font | none yet |
| `graphics/printing/custom-font` | Loading a BCFNT font (the Makefile converts a Liberation Serif font) | [asset pipeline](../engineering/asset-pipeline.md) (`mkbcfnt`) |
| `graphics/bitmap/24bit-color` | Drawing a 24-bit image to the bottom-screen framebuffer; the Makefile needs ImageMagick (`convert` or `magick`) | [rendering 2D](../engineering/rendering-2d.md) (framebuffer layout in [display](../hardware/display.md)) |
| `sdmc` | Reading `test.bin` (a raw 240x320 RGB image) from the SD card with plain `fopen`, then copying it to the bottom framebuffer while a cosine-table effect runs on the top one | [filesystem and saves](../engineering/filesystem-and-saves.md) |
| `romfs` | `romfsInit`, then reading files from `romfs:/` including a path with non-ASCII (Japanese) names | [filesystem and saves](../engineering/filesystem-and-saves.md), [asset pipeline](../engineering/asset-pipeline.md) |

### Graphics: citro2d and citro3d

All `graphics/gpu` examples use citro3d (`C3D_*`) except those marked citro2d (`C2D_*`). Shader sources are `.pica` files assembled by picasso ([picasso and citro3d](picasso-and-citro3d.md)). The frame structure they share is explained in [gpu rendering](../engineering/gpu-rendering.md).

| Example | Demonstrates | Wiki guide |
| ------- | ------------ | ---------- |
| `2d_shapes` (citro2d) | Untextured circles, ellipses, rectangles and triangles on screen targets | [rendering 2D](../engineering/rendering-2d.md) |
| `gpusprites` (citro2d) | Sprite sheets built by `tex3ds` from many PNGs, `C2D_Sprite` move/rotate | [rendering 2D](../engineering/rendering-2d.md), [asset pipeline](../engineering/asset-pipeline.md) |
| `stereoscopic_2d` (citro2d, C++) | One render target per eye on the top screen with per-eye offsets for a 3D depth effect | [gpu rendering](../engineering/gpu-rendering.md) (stereo mode), [display](../hardware/display.md) |
| `simple_tri` | Minimal citro3d: vertex shader, attribute info, a VBO, a fixed attribute for white colour, `C3D_DrawArrays` | [gpu rendering](../engineering/gpu-rendering.md), [PICA200 shader programming](../engineering/pica200-shader-programming.md) |
| `both_screens` | Drawing to the top and bottom screens (`C3D_Both` clear, per-target frame drawing) | [gpu rendering](../engineering/gpu-rendering.md) |
| `multiple_buf` | Position and colour kept in two separate vertex buffers, each added to `C3D_BufInfo` (`BufInfo_Add`) | [gpu rendering](../engineering/gpu-rendering.md) |
| `immediate` | Immediate-mode drawing (`C3D_ImmDrawBegin`); the source notes attribute format and element count are ignored in immediate mode | [gpu rendering](../engineering/gpu-rendering.md) (batching); immediate mode specifically: none yet |
| `textured_cube` | A textured, rotating cube; texture from `kitten.t3s` via tex3ds | [gpu rendering](../engineering/gpu-rendering.md), [asset pipeline](../engineering/asset-pipeline.md) |
| `lenny` | Stereoscopic 3D (`gfxSet3D(true)`): the scene is rendered twice with an eye offset taken from the 3D slider (`osGet3DSliderState()`, divided by 3), first colour-combiner stage blends the primary colour | [gpu rendering](../engineering/gpu-rendering.md) |
| `wide_mode_3d` | `gfxSetWide(true)`: the 800-pixel-wide top-screen mode with citro3d rendering | [gpu rendering](../engineering/gpu-rendering.md), [display](../hardware/display.md) |
| `composite_scene` | citro3d scene plus citro2d text and sprites on the same targets (`C2D_Flush`, text buffers); stereo and the 3D slider | [gpu rendering](../engineering/gpu-rendering.md) |
| `fragment_light` | Per-pixel lighting through `normalquat` and `view` outputs and the citro3d light environment | [PICA200 shader programming](../engineering/pica200-shader-programming.md), [GPU shading features](../hardware/gpu-shading-features.md#fragment-lighting) |
| `toon_shading` | Toon-style lighting of a teapot model (indexed draw with `C3D_DrawElements`) | [GPU shading features](../hardware/gpu-shading-features.md) |
| `normal_mapping` | Bump mapping: diffuse and normal textures, both converted by tex3ds | [GPU shading features](../hardware/gpu-shading-features.md) (lighting inputs); the technique itself: none yet |
| `cubemap` | Skybox using a cube-map texture (`skybox.t3s`) | none yet (texture formats in [GPU shading features](../hardware/gpu-shading-features.md)) |
| `mipmap_fog` | Mipmapped texture plus the GPU fog unit (`C3D_FogColor`) | [GPU shading features](../hardware/gpu-shading-features.md), [asset pipeline](../engineering/asset-pipeline.md) (mipmaps) |
| `proctex` | Procedural textures (`C3D_ProcTexCombiner` and friends), drawn in immediate mode | [GPU shading features](../hardware/gpu-shading-features.md) |
| `geoshader` | A geometry shader with stride 6 (a triangle at a time) | [PICA200 shader programming](../engineering/pica200-shader-programming.md) |
| `loop_subdivision` | Loop subdivision of a model in a geometry shader, indexed drawing (`C3D_DrawElements`) | [PICA200 shader programming](../engineering/pica200-shader-programming.md) (geometry shaders) |
| `particles` | A geometry-shader particle system with alpha blending, depth test and a noise smooth-step in the shader | [PICA200 shader programming](../engineering/pica200-shader-programming.md), [gpu rendering](../engineering/gpu-rendering.md) |
| `physics/box2d` (C++) | Box2D physics driven by touch and buttons (a library port, not a GPU example) | [porting](../engineering/porting.md#libraries-that-shorten-a-port) |

### Input

| Example | Demonstrates | Wiki guide |
| ------- | ------------ | ---------- |
| `input/read-controls` | `hidScanInput`, `hidKeysDown/Held/Up`, `hidCircleRead` (the Circle Pad) | [libctru](libctru.md) (HID subsystem); [frame pacing](../engineering/frame-pacing.md) for when to poll |
| `input/touch-screen` | `hidTouchRead` | none yet |
| `input/software-keyboard` | `swkbdInit`/`swkbdInputText`, validation, filter callback, dictionary and learning data, password mode, numpad keys; uses `aptSetChainloaderToSelf` | [application lifecycle](../engineering/application-lifecycle.md#software-keyboard-and-other-library-applets) |
| `qtm/eye_position` | **New 3DS** head tracking: `qtmInit`, tracking data (eye coordinates, confidence, pitch/yaw/roll deviation), `qtmEstimateEyeToCameraDistance`, `qtmComputeHeadTiltAngle` | [libctru](libctru.md#qtm-new-3ds-head-tracking), [services](../software/services.md) |
| `qtm/lenny_qtm_movement_naive` | Moves a rendered object with the tracked head position (a naive mapping, per its name) | [libctru](libctru.md#qtm-new-3ds-head-tracking) |

### Audio

| Example | Demonstrates | Wiki guide |
| ------- | ------------ | ---------- |
| `audio/streaming` | `ndsp` wave buffers streamed from memory | [audio](../engineering/audio.md) |
| `audio/filters` | `ndspChnIirBiquadSet*` filters (low, high, band-pass, notch, peaking equaliser), `ndspChnSetMix`, `ndspSetOutputMode` | [audio](../engineering/audio.md) |
| `audio/ogg-vorbis-decoding` | Threaded Ogg Vorbis streaming from RomFS with `libvorbisidec`; README says it is fast enough "even on O3DS" (Old 3DS) | [audio](../engineering/audio.md), [asset pipeline](../engineering/asset-pipeline.md#audio) |
| `audio/opus-decoding` | Same design with `opusfile`; README also says fast "even on O3DS" | [audio](../engineering/audio.md) |
| `audio/modplug-decoding` | Tracker module playback with `libmodplug` | [audio](../engineering/audio.md) |
| `audio/mic` | `micInit` with a `0x30000`-byte buffer, `MICU_StartSampling` (PCM16 signed, 16360 Hz sample-rate constant), reading `micGetLastSampleOffset`; plays the recording back through **CSND**, not ndsp. README: about 32 seconds fit in the default buffer | none yet for the microphone ([I/O and peripherals](../hardware/io-and-peripherals.md), [services](../software/services.md)) |

The audio README states the shared requirement: homebrew needs `sdmc:/3ds/dspfirm.cdc`, obtainable from Luma3DS via Rosalina *Miscellaneous options... > Dump DSP firmware* ([Luma3DS](luma3ds.md#rosalina-menu-map)); an emulator that emulates the DSP at high level (the README names Citra) accepts a 0-byte file.

### Camera, video, NFC

| Example | Demonstrates | Model | Wiki guide |
| ------- | ------------ | ----- | ---------- |
| `camera/image` | `camInit`, `CAMU_SetSize(SIZE_CTR_TOP_LCD)`, RGB565 output, both outer cameras (`PORT_BOTH`), `CAMU_SetReceiving` into a buffer and `svcWaitSynchronization` on the receive events, shutter sound; shows a 3D picture on the top screen with the slider read from `0x1FF81080`. README: does not use the camera calibration data, so the 3D effect is not corrected | not stated | none yet ([I/O and peripherals](../hardware/io-and-peripherals.md), [services](../software/services.md)) |
| `camera/video` | The same capture in a loop (`svcWaitSynchronizationN`) | not stated | none yet |
| `mvd` | **New 3DS** hardware video decoder (`mvdstdInit`): colour-format conversion (YUYV422 to BGR565 for 320x240) and H.264 video processing at 240x400. README conditions: needs modded FIRM with `MVDSTD` access, or ninjhax 1.x (the \*hax 2.x payloads then had no way to reach it); the process needs kernel release version >= 2.44 (FIRM 8.0.0) in its exheader, such as System Settings; playback is "very slow" because each frame is shown immediately without buffering | New 3DS | [services](../software/services.md), [I/O and peripherals](../hardware/io-and-peripherals.md) |
| `nfc` | `nfcInit`, scanning for an amiibo and reading/writing its application data (hard-coded to the Super Smash Bros. AppID; saves the data to SD). README: the \*hax payload picks the right host process for `nfc:u` on **New 3DS**; for **Old 3DS** the host title "hasn't been determined", because Old 3DS has no system application that could be used, and the target would need `nfc:u` access and bits `0xF0000000` clear in the title-ID low word | New 3DS (Old 3DS undetermined per README) | [services](../software/services.md); `amiitool` in [development workflow](development-workflow.md#other-libraries-and-tools) |

### Network

| Example | Demonstrates | Wiki guide |
| ------- | ------------ | ---------- |
| `network/3dslink-demo` | `socInit` with a 1 MiB (`0x100000`) buffer and `link3dsStdio()` so `printf` goes to the host when `3dslink -s` is used | [debugging](../engineering/debugging.md), [networking](../engineering/networking.md) |
| `network/sockets` | BSD sockets over the `soc` service | [networking](../engineering/networking.md) |
| `network/http`, `network/http_post` | `httpc` GET (a LAN-only URL that must be edited) and POST; the GET example disables certificate verification, see the warning in the networking page | [networking](../engineering/networking.md) |
| `network/sslc` | Raw TLS with the built-in root CA (`builtin_rootca.der`) and verification enabled | [networking](../engineering/networking.md) |
| `network/uds` | Local wireless (`nwm::UDS`): create, scan, connect, send and receive packets, spectators | [networking](../engineering/networking.md) (stack mentioned; no walkthrough yet) |
| `network/boss` | SpotPass (BOSS) task registration and reading content. README: needs `bosshaxx` from `ctr-httpwn` >= v1.2 or a CFW (for unsigned content); under a \*hax payload >= v2.8 the task is tied to Home Menu, so per-region content URLs are needed; alternatively `bossInit(0, true)` binds to your own title | [services](../software/services.md), [exploits](exploits.md#full-standalone-and-secondary-table-3dbrew) (ctr-httpwn) |

## Technical details

- **Licence.** The repository README carries a Public Domain Mark; the benchmarks in this wiki reuse the template Makefile on that basis ([benchmarks](../benchmarks/README.md)).
- **Sample age.** Several README notes (\*hax payload versions, Citra, "ninjhax 1.x") describe the 2015 to 2016 situation and are historical. They are kept here because they record which model and which privilege each service needed at the time; whether current Luma3DS lifts the `mvd`/`nfc` restrictions is not established by these sources.
- **Which are citro3d samples.** Every directory under `graphics/gpu` has a `source/` folder and a `.pica` shader except the citro2d ones listed above; only `stereoscopic_2d` is C++.
- **Pinned commit.** The commit the rest of the wiki cites is on [source versions](../engineering/source-versions.md); this catalogue was written from the local clone and does not claim to match a newer upstream layout.

## Limits and risks

- The samples are demonstrations, not hardened code: the HTTP sample turns off certificate checking, `app_launch` hard-codes a region's title ID, and several print raw `Result` codes without handling errors.
- Examples that touch the camera, microphone, NFC, BOSS or the video decoder have not been run for this wiki; the descriptions are from source and READMEs only.

## Open questions

- No wiki guide yet for: microphone, cameras, input reading and touch, Mii selector, the text console and system font, cube maps, normal mapping, immediate mode, NFC, BOSS/SpotPass, UDS walkthrough, date and time handling.
- Whether the `mvd` and `nfc` samples run under today's Luma3DS and on which models is not established by the sources.

## Sources

- [devkitPro/3ds-examples](https://github.com/devkitPro/3ds-examples) — every directory's source and README, local clone read in full for this page.
- [3dbrew: Homebrew Libraries and Tools](https://www.3dbrew.org/wiki/Homebrew_Libraries_and_Tools) — `amiitool` and related tools.
