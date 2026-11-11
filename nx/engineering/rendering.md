# Rendering on NX with deko3d

The Switch draws plenty fast enough, but every separate draw call costs the main processor that runs program instructions (the **[CPU](../../glossary.md#cpu)**) time. This page is for a homebrew developer whose 3D scene runs slow on Switch while the graphics chip that draws images (the **[GPU](../../glossary.md#gpu)**) looks idle. Short answer: merge draws into a few batches, pack textures into atlases, upload static data once, and pre-record command lists — do not send thousands of small draws per frame.

## Before you start

- Toolchain: devkitA64 via devkitPro pacman `switch-dev`, with libnx and deko3d. Versions checked against latest `switch-dev` at time of writing (exact libnx/deko3d commits not pinned for this draft).
- Applies to all retail Switch models running homebrew via deko3d on the NVIDIA chip the Switch is built on (**[Tegra X1](../glossary.md#tegra-x1)**).
- Code below is illustrative only: checked for shape against deko3d examples, **not run on hardware**. Build flags assume `switch-dev` defaults with `-Wall`.
- Companion page: [NX performance and porting](performance-and-porting.md) for clocks, threading, and measurement.

## Batch draws or stay slow

What to do: merge small pieces of the world into a few large draws before touching anything else. A 16×16-tile chunk merge plus grouping by material/atlas is the pattern port reports credit. Add per-chunk visibility testing (skip chunks outside the camera) so merging does not mean drawing the whole level.

```c
// Do this: one draw per visible chunk+material, not one per tile.
// Illustrative — not run on hardware.
for (each visible chunk) {
    for (each material in chunk) {
        bind_atlas(material.atlas);       // one bind per material
        dkCmdBufDraw(cmdbuf, ...chunkVerts...);
    }
}
```

- Expected result: draw-call count drops by 10–100× on tile maps; frame time falls while GPU busy time barely moves, because the limiter was CPU submission [reported].
- How it fails: thousands of `dkCmdBufDraw` calls per frame keeps frame time high even at low resolution. If lowering resolution does not help, you are CPU-bound on submission — merge more.

## Pack atlases, keep depth on

What to do: pack many small sprites/tiles into one large texture (an atlas), group draws by that atlas, and leave depth testing on. Use cutout-by-discarding (pixels whose alpha is below a threshold are skipped) for fences and leaves instead of see-through blending, which is slower and sorts badly.

```c
// Do this in the pixel shader: cutout, not blending, for foliage.
// Illustrative — not run on hardware.
if (texColor.a < 0.5) discard;
```

- Expected result: fewer texture binds and correct hiding of far objects with no sort code [reported].
- How it fails: one texture per tile, or blending everywhere, restores the many-binds problem and adds sort glitches. If binds per frame exceed draws per frame, repack the atlas.

## Upload static data once, send only uniforms per frame

What to do: put geometry and textures that never change into GPU-visible **[memory](../../glossary.md#memory)** once at load. Per frame, update only small uniform blocks (camera matrices, light values).

```c
// Load time: create block, upload, done. Per frame: update uniforms only.
// Illustrative — not run on hardware.
dkMemBlockCreate(&staticBlock, ...);
dkMemBlockUpload(staticBlock, vertices, size); // once
// per frame:
dkCmdBufBindUniformBuffer(cmdbuf, cameraUbo);  // small update only
```

- Expected result: no per-frame re-upload spikes; frame pacing steadies [reported].
- How it fails: re-uploading vertex data every frame shows up as high CPU time plus bus stalls. If a profiler shows copies every frame, move the buffer to load time.

## Pre-record command lists, use rings and fences

What to do: record command lists for things that do not change (static world) once, and keep two or three per-frame lists in a ring (a fixed set of buffers reused in turn) for dynamic objects. Use a fence (a GPU "I finished up to here" marker) per ring slot to know when a slot is safe to reuse. Queue submission is lazy — nothing reaches the GPU until a flush, present, or list-full [documented] (deko3d Primer, Queues). Never stop the whole GPU to wait (`dkQueueWaitIdle` per frame); wait only the slot being reused [documented] (same source).

```c
// Static list recorded once; dynamic ring of 3 slots guarded by fences.
// Illustrative — not run on hardware.
dkCmdBufFinish(staticList);                    // once
slot = frame % 3;
dkQueueWaitFence(queue, fence[slot]);          // wait for that slot only
dkCmdBufClear(dynamic[slot]); /* record */ dkQueueSubmit(queue, dynamic[slot]);
dkQueueSignalFence(queue, fence[slot]);
```

- Expected result: CPU can build the next frame while the GPU draws the current one; full-device waits disappear [reported].
- How it fails: calling full-device wait per frame serialises CPU and GPU and caps the frame rate near 30 fps or worse. If frame time equals CPU time plus GPU time instead of the larger of the two, the pipeline is serialised.

Bake repeated bind-state with command capture and replay (`dkCmdBufBeginCaptureCmds` / `End` / `ReplayCmds`) instead of re-emitting it per draw [documented] (deko3d Primer, Command capture and replay). Capture cannot contain compute dispatches, fences, indirect draws, full barriers, or nested call-lists — overrun is fatal, so size capture buffers generously.

## Use block-linear textures, mips, and precompiled shaders

What to do: store GPU textures in the tiled block layout the GPU's small fast memory next to the processor (its **[cache](../../glossary.md#cache)**) likes. Query it with `DkImageLayoutMaker` + `dkImageLayoutInitialize` as shown in deko3d Example04 (`Example04_TexturedCube`) [documented] — never assume pitch, since block-linear units are 64×8-byte GOBs regardless of format [documented] (tegra_swizzle tiling notes plus the Maxwell unified L1/texture behavior). Start with one atlas per material with mipmaps (pre-shrunk copies) and block compression [hypothesis] (atlas size is a tuning choice, not a documented optimum — see open questions), and ship precompiled shader caches (DKSH files) because deko3d has no runtime GLSL compiler by design [documented] (deko3d README + Primer shader section).

```c
// Query the tiled layout instead of assuming row-by-row.
// Illustrative — see deko3d Example04_TexturedCube, not run on hardware.
DkImageLayoutMaker maker = { ...imageDesc... };
DkImageLayout layout;
dkImageLayoutInitialize(&layout, &maker); // gives block-linear pitch/offsets
```

- Expected result: tiled images sample from the fast path; mips plus compression cut bandwidth toward the docked shared peak of 25.6 GB/s (handheld lower; CPU+GPU share one bus) [documented]; DKSH avoids first-encounter stutter by construction [documented].
- How it fails: linear textures or missing mips show as high bandwidth plus shimmering at distance. Missing DKSH shows as one long hitch per new material — the compile has nowhere else to happen.

Per-stage shader limits to design around [documented] (`uam` / dekotools docs): 16 uniform + 16 storage blocks, 32 samplers, 8 images per stage. Exceeding them fails at pipeline creation, not at draw time.

## Order barriers, tiled-cache ops, and compression cheapest-first

What to do: pick the cheapest barrier that orders each hazard — tile-to-tile within one target (`Tiles`), pass-to-pass (`Fragments`), compute-to-graphics or full drain (`Primitives`/`Full`) [documented] (`deko3d.h` barrier enum; `Full` also disables command-list prefetch). Do not stack a manual full invalidate on top of the one deko3d already inserts after every queue flush — the flush point is the sanctioned CPU-update window for vertex/uniform/descriptor memory [documented] (deko3d Primer, Queues).

What to do: enable the tiled cache around tiled render-target passes and flush before that target is sampled as a texture or presented (`dkCmdBufTiledCacheOp` Enable/Flush) [documented] (`deko3d.h`); set `HwCompression` on render targets and `Z16EnableZbc` for depth clears, and prefer `Clear`/`Discard` when a tile will not be re-read [documented] (same header). Never sample or render to `PitchLinear`, and never present a compressed target without a resolve/decompress path [documented] (`DkImageFlags` in `deko3d.h`).

What to do: treat hardware depth-culling (Zcull) as opt-in state — enable per queue, invalidate on state changes, and check `ZcullStats` when tuning [documented] (`DkQueueFlags`, `DkInvalidateFlags_Zcull`, `DkCounter_ZcullStats`). Toggling mid-frame without invalidation reads as stale culling, not as a driver bug.

- Expected result: fewer pipeline drains, correct compressed-target display, measurable cull rates on depth-heavy scenes [documented].
- How it fails: `Full` everywhere drains the pipeline each pass; presenting a compressed target without resolve shows garbage; Zcull toggled without invalidate culls the wrong geometry.

## Never read the frame back to answer a gameplay question

What to do: serve UI and logic questions (is the top dark, did the screen flash white, which tiles changed) from structured facts computed on the GPU or CPU — small summary buffers, per-chunk counters, region-of-interest reads — instead of copying the composed frame to the CPU every frame. One project paid 22.5 ms/frame copying two small screens through uncached memory plus a second copy in a compatibility shim, over a third of its 61 ms frame, and a fence fix proved the cost was CPU copy time with zero late reads [reported] (Switch homebrew postmortem, stock-clock field scene, conditions as stated).

- Expected result: no full-screen readback on the steady path; exceptional captures only on explicit request [reported].
- How it fails: any per-frame fullscreen copy dominates the frame no matter how fast the renderer gets. If a "GPU slow" profile is really queue-submit or readback time, the buckets are mislabeled — separate CPU-submit vs. GPU-complete with fences/timestamps and keep buckets exclusive, never summed when nested.

## Pick a resolution strategy on purpose

What to do: choose one. Either render the 3D view at a small fixed size (a **[framebuffer](../../glossary.md#framebuffer)** holding one image, e.g. 320×180 logical) and upscale with whole-number scaling, or render at a true low-resolution target texture and upscale once. Do not mix the two by accident.

```c
// Integer upscale factors: 320x180 -> 1280x720 is 4x, -> 1920x1080 is 6x.
// Illustrative — not run on hardware.
const int SCALE = destWidth / renderWidth; // require integer, e.g. 4 or 6
```

- Expected result: pixel look with crisp edges and a fixed, predictable fill cost [reported].
- How it fails: fractional scaling blurs pixels unevenly. If one axis shimmers while the other looks crisp, the scale factor is not an integer.

## Validate in debug, time sparingly in release

What to do: link the deko3d validation layer (`-ldeko3dd`) in debug builds to catch layout and barrier mistakes, and sample the GPU's own timestamps only around suspect passes in release — leaving them always on perturbs what you measure.

```sh
# Debug link only — illustrative, not run on hardware.
# link with -ldeko3dd for debug, -ldeko3d for release
```

- Expected result: validation prints the bad call at creation time instead of a black screen; sparse timestamps show which pass is long [documented].
- How it fails: shipping validation to users costs speed; always-on timestamps inflate the pass they measure. If timings change when you add more timers, you are measuring overhead.

## Technical details

- Submission cost dominates: port reports credit batching before anything else; per-draw setter cost is covered on the performance page [reported].
- Static/dynamic split: static lists hold world geometry; ring size 2–3 covers one frame being drawn while the next is built; fences scope the wait to one slot; never `dkQueueWaitIdle` per frame [documented] (deko3d Primer, Queues/Fences).
- Fence structs must not move while in-flight lists reference them; zero-init fences that may be waited before first signal [documented] (`deko3d.h` fence notes). Reuse shader code blocks instead of churning mappings: the code segment has a fixed cap with an unusable tail and freed mappings are never reused [documented] (same header).
- Image layout: query with `DkImageLayoutMaker` + `dkImageLayoutInitialize` (Example04 usage is the proof); GOB is 64×8 bytes regardless of format [documented] (tegra_swizzle tiling notes). Storing images already tiled at build time avoids runtime converts [documented].
- Shader cache: DKSH is the only path — deko3d cannot compile GLSL at runtime by design [documented] (deko3d README + Primer). Ship prebuilt DKSH plus the control/code split the header recipe describes.
- Bandwidth math (RGBA8 + D32): a 2×color + depth + resolve set is ~11 MB at 720p vs ~25 MB at 1080p before overdraw; at 3× overdraw plus texture fetch, 1080p lands near 150–300 MB/frame against a ~427 MB/frame total-system budget at 60 fps on the docked peak — 720p leaves headroom [documented] (arithmetic from the docked 25.6 GB/s shared peak; handheld lower).
- Small programs that run on the GPU should stay register-lean; Maxwell notes recommend packed half-precision types where verified (the vendor compiler does not reliably auto-pack — write explicit `half2`/`f16vec4` paths and measure [documented] (Maxwell tuning guide + Chips and Cheese notes)), plus compressed mipmapped textures for the shared-bus ceiling — see performance page [documented].

## Pitfalls

- Thousands of tiny draws: symptom is flat GPU busy time with high frame time; fix by chunk-merging and grouping by material/atlas.
- Rebinding per draw: symptom is CPU time scaling with draw count even after merging vertices; fix by binding atlas and state once per material.
- Full-device wait per frame: symptom is frame time near CPU-plus-GPU instead of overlapping; fix with ring plus per-slot fences.
- Runtime texture converts (PNG/JSON at load): symptom is long loads and linear images; fix by pre-converting to tiled binaries at build time.
- Always-on GPU timers: symptom is the measured pass shrinking when timers are removed; fix by timing one pass at a time, briefly.

## Open questions

- What draw-call count per frame tips a typical homebrew scene from GPU-bound to CPU-bound on retail clocks? Needs a tiny benchmark sweeping draws at fixed vertex load, run on hardware.
- Which atlas size (512 px vs 1024/2048 px) balances binds against cache pressure on handheld clocks? Needs same-scene A/B with bandwidth counters.
- How much DKSH precompilation covers a full level without gaps? Needs a stutter log over a complete playthrough.

## Sources

- [deko3d](https://github.com/switchbrew/deko3d) — command lists, capture/replay, barriers, tiled-cache ops, Zcull, image layout and flags, validation layer [documented].
- [deko3d headers](https://github.com/switchbrew/deko3d/blob/master/include/deko3d.h) — barrier tiers, `DkImageFlags`, queue flags, fence and code-segment rules [documented].
- [libnx](https://github.com/switchbrew/libnx) — fences, queues, applet memory modes [documented].
- [switch-examples](https://github.com/switchbrew/switch-examples) — Example04 image layout, Example08 deferred barriers, Example09 compute fences, ring patterns [documented].
- [tegra_swizzle](https://github.com/switchbrew/tegra_swizzle) — GOB 64×8-byte block-linear layout and test vectors [documented].
- [NVIDIA Maxwell tuning guide](https://docs.nvidia.com/cuda/maxwell-tuning-guide/) — coalescing, caching, occupancy, shared-memory sizing, shared-bus bandwidth [documented].
- [Chips and Cheese Tegra X1 analysis](https://chipsandcheese.com/) — 768 MHz shipping clock, 64 KB shared, L2 rates, FP16 no-auto-pack note [reported] (third-party measurement, conditions as stated).
