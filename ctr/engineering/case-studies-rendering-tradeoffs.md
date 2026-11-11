# Rendering trade-offs in three 3DS games

These examples connect renderer design to fidelity, memory and timing decisions. SM64DS Enhanced and Sonic CD 11 are homebrew ports; Super Smash Bros. for Nintendo 3DS is a commercial game. They are evidence of particular designs, not recipes guaranteed to work or run at a given speed on another project.

## Before you start

The descriptions below are drawn from the supplied research notes and linked upstream material; this wiki has not built these ports, checked a particular revision, or reproduced their results. Source links track moving branches, so pin a commit before relying on implementation details. Model, firmware, build and test conditions are unknown wherever not stated. See [hardware testing](testing-on-hardware.md) for recording them.

## DS geometry translated to native draws

The SM64DS Enhanced research notes describe a path from DS geometry commands to PICA200 draws: record geometry and state, retain original vertices with matrix identifiers, then transform vertices on the GPU. The described representation uses 20-byte vertices, 16-bit indices and a 20-slot shader-uniform matrix ring; draws must split when required matrices no longer fit, while compatible runs can be merged. [reported: project renderer and shader, date and test model unspecified] Compare position, texture coordinates, depth, polygon ordering and blending against a reference renderer before treating a translation as correct. See [porting](porting.md#65-porting-3d-geometry-and-material-pipelines) and [shader programming](pica200-shader-programming.md).

The notes also describe memoizing decoded display lists. A list's address alone is insufficient when its contents or inherited graphics state can change: track dependencies, invalidate on writes and preserve dynamic matrix operations. A reference mode that executes both paths and compares their outputs can expose missing dependencies. [reported: project patch, date and test model unspecified] The renderer's particle batching reportedly accepts occasional blending differences across polygon IDs; record such exceptions as fidelity compromises, not correctness-preserving optimizations.

The project also describes preparing commands ahead of GPU execution with alternating buffers. Treat command data, textures and framebuffers as owned until the relevant consumer has finished: GPU completion and display presentation are separate events. Its implementation uses citro3d internals, so do not treat it as a stable public API. The notes report a hardware flicker issue absent in Azahar; model and conditions are unspecified. See [frame pacing](frame-pacing.md#overlapping-cpu-and-gpu) and [memory ownership](porting.md#8-make-memory-ownership-and-cpugpu-synchronization-explicit).

The same port describes interpolating presentation between game ticks by matching objects and interpolating transforms, with fallbacks for cuts or poor matches. This does not increase simulation frequency. The supplied notes say interpolation is disabled by default on Old 3DS; **New 3DS:** it is enabled by default. [reported: project interpolation implementation, revision and conditions unspecified] Measure matching overhead and test teleports, camera cuts, animation changes and missing objects separately from game timing.

The notes describe hardware transfers for dynamic 2D texture tiling, unchanged-layer reuse, indexed asset packing to reduce file opens, and incremental sound decoding to reduce lock stalls. [reported: project source and patches, date and model unspecified] These are investigation targets rather than portable timing guarantees. Preserve transfer-source lifetime and cache synchronization, and compare dirty tracking against a reference image. The notes also describe an Old 3DS asynchronous bottom-screen path that can show mixed or skipped updates; a faster update is not necessarily an equivalent one.

## Sonic CD: convert a software renderer to GPU layers

The Sonic CD 11 port's research notes describe recording sprites and tiles into ordered layers and drawing them through citro2d instead of rasterizing the entire image in software. Sprite sheets and stage graphics are converted at resource load and reused. [reported: `render_3ds.cpp`, `Sprite.cpp` and `Scene.cpp`, revision and model unspecified] The developer reports full-speed operation with the hardware renderer on Old 3DS, while the more accurate software renderer needs New 3DS for full speed; the notes do not establish controlled timings or conditions. The hardware path omits or approximates some palette, deformation, parallax and special-stage effects. **New 3DS:** the software renderer is reported to run at full speed there.

The notes describe repacking a 16 × 16384 indexed tileset into a 512 × 512 direct-colour atlas. For source coordinates `(sx, sy)`, the proposed mapping is `dx = sx + 16 * (sy / 512)` and `dy = sy % 512`, using integer division. A 512 × 512 RGBA5551 image occupies 512 KiB before additional allocation overhead. Expand indices through the effective palette, arrange pixels in the GPU's tiled layout and check boundaries and orientation against the source. Changing a palette after conversion requires invalidating or rebuilding affected textures. See [asset pipeline](asset-pipeline.md#atlases-and-padding) and [2D palette effects](rendering-2d.md#palette-effects-without-palettes).

The port also approximates scanline scrolling with 16-pixel-high strips. [reported: `Drawing.cpp`, revision and test model unspecified] Compare reference images at strip boundaries and measure preparation and GPU time for different strip heights before choosing a fidelity/performance trade-off. Instrument both the application's fixed-size sprite and tile queues and citro2d capacity: a full queue may lose visible objects even when the GPU has room. Check the actual slider value when deciding whether to render a second eye; configured stereo mode alone is not the slider position. The port is marked deprecated in the supplied notes, so consult its successor for current development.

## Smash: prioritize the visible and interactive work

The CEDEC 2015 session abstract identifies stereo rendering, shaders, effects and model restrictions as topics in the game's rendering design. It does not disclose the exact algorithms or budgets. [reported: Hiroyuki Kobayashi, 2015, session abstract; model and measurement conditions not specified] Avoid inferring an outline algorithm, per-core schedule or shader sequence from the final image.

The supplied research cites developer interviews describing higher-detail fighter models for paused viewing and replacement of Magicant's Flying Men models with sprites to reduce load. It also reports that Ice Climbers were cut after attempts to reduce their cost. [reported: Sakurai interviews, dates and exact test conditions not specified in the notes] These examples motivate separate budgets for simulation, animation, geometry and effects; they do not identify the cost of each. A reported frame-rate analysis describes some secondary animations running at 30 frames per second while gameplay targeted 60, with occasional drops. [reported: Digital Foundry, date and model unspecified in the notes] Animation update rate, simulation rate and presentation rate are distinct measurements.

On Old 3DS the game reportedly uses an extended-memory configuration and limits access to background system applications; its exact allocation has not been verified from its executable header. **New 3DS:** the notes report that background applications remain available. Do not assume a particular memory allocation without checking the title. See [memory and cache](memory-and-cache.md) and [application lifecycle](application-lifecycle.md).

## Technical details: experiments to separate the effects

1. Fix the scene, camera, input, stereo setting and asset set. Record console model, firmware, build and warm-up conditions. Compare reference output and feature coverage first.
2. For a 2D port, compare software composition, GPU layers with uploads, persistent textures, and several strip heights. Record CPU preparation, GPU time, upload time, peak linear memory, missed frames and visual differences; change one variable per run.
3. For geometry translation, compare decoded and cached execution on identical input, including state changes outside the list. Count cache hits, invalidations, mismatches and batch splits. Test particle overlap and depth ordering explicitly.
4. For frame-ahead submission, record CPU wait, GPU completion and presentation separately, plus input latency. Stress buffer reuse on hardware. For interpolation, also record simulation ticks and matching cost.
5. For content detail, hold fighters, stage, effects and camera constant while comparing representations. Model changes may also alter materials, bones and draw calls, so a difference cannot automatically be attributed to triangle count.

None of these experiments has been run by this wiki. See [the measurement protocol](testing-on-hardware.md#running-a-performance-experiment).

## Pitfalls and open questions

- A cache that ignores palette, inherited state or another writer can produce stale but plausible images. Maintain a reference path and exercise invalidation.
- Faster batching can change blend order. Document the exact affected cases and evaluate them visually.
- An emulator can help with functional comparisons but cannot establish hardware timing or presentation safety.
- The Smash lecture itself, exact model budgets, memory allocation and subsystem timings remain unverified. The supplied source notes also do not establish reproducible timings for either port. Pin source revisions and run the proposed hardware comparisons to close these gaps.

## Sources

- [SM64DS Enhanced renderer](https://github.com/ethanseagull/sm64ds-enhanced/blob/main/port3ds/src/gpu3ds.cpp), [vertex shader](https://github.com/ethanseagull/sm64ds-enhanced/blob/main/port3ds/shaders/gx_rec.v.pica), [display-list cache patch](https://github.com/ethanseagull/sm64ds-enhanced/blob/main/port3ds/upstream-patches/0056-ntr-the-list-memo-a-display-list-seen-before-put-int.patch), and [interpolation](https://github.com/ethanseagull/sm64ds-enhanced/blob/main/port3ds/src/interp3ds.cpp) — implementation referenced by the supplied research.
- [SM64DS Enhanced asset packing](https://github.com/ethanseagull/sm64ds-enhanced/blob/main/port3ds/game/files_pack.cpp), [2D composition patch](https://github.com/ethanseagull/sm64ds-enhanced/blob/main/port3ds/upstream-patches/0073-hal-engine-A-s-full-2D-boards-composed-in-a-fraction.patch), [audio patch](https://github.com/ethanseagull/sm64ds-enhanced/blob/main/port3ds/upstream-patches/0065-sdat-a-wave-is-decoded-as-it-plays-not-whole-at-its-.patch), and [bottom-screen worker](https://github.com/ethanseagull/sm64ds-enhanced/blob/main/port3ds/game/sub_async.cpp) — project-specific optimizations and compromises.
- [Sonic CD 11 3DS](https://github.com/SaturnSH2x2/Sonic-CD-11-3DS), [renderer](https://github.com/SaturnSH2x2/Sonic-CD-11-3DS/blob/master/RSDKv3/3ds/render_3ds.cpp), and [drawing code](https://github.com/SaturnSH2x2/Sonic-CD-11-3DS/blob/master/RSDKv3/Drawing.cpp) — port design and feature limitations as described in the supplied research.
- [CEDEC 2015 session](https://cedec.cesa.or.jp/2015/session/ENG/4257.html) — abstract of the Smash rendering talk; not the full presentation.
- [Sakurai Nintendo Dream interview translation](https://sourcegaming.info/2015/01/03/first-two-pages-of-sakurais-nintendo-dream-interview/) and [Japan Game Awards statement](https://awards.cesa.or.jp/2015/en/prize/year/16.html) — developer context and trade-offs.
- [Digital Foundry comparison](https://www.youtube.com/watch?v=lhl976_xxtQ) — independent visual and frame-rate analysis referenced in the supplied research.
