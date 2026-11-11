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

What to do: record command lists for things that do not change (static world) once, and keep two or three per-frame lists in a ring (a fixed set of buffers reused in turn) for dynamic objects. Use a fence (a GPU "I finished up to here" marker) per ring slot to know when a slot is safe to reuse. Never stop the whole GPU to wait.

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

## Use block-linear textures, mips, and precompiled shaders

What to do: store GPU textures in the tiled block layout the GPU's small fast memory next to the processor (its **[cache](../../glossary.md#cache)**) likes — via the `DkImageLayout` helper from deko3d Example04 [documented] — start atlases around 512 px with mipmaps (pre-shrunk copies) and block compression, and ship precompiled shader caches (DKSH files) so the driver does not compile on first sight.

```c
// Query the tiled layout instead of assuming row-by-row.
// Illustrative — see deko3d Example04, not run on hardware.
DkImageLayout layout;
dkImageGetLayout(&imageDesc, &layout); // gives block-linear pitch/offsets
```

- Expected result: tiled images sample faster than linear ones on this GPU generation (**[Maxwell](../glossary.md#maxwell)**) [documented]; mips plus compression cut bandwidth toward the 25.6 GB/s ceiling [documented]; DKSH avoids first-encounter stutter [reported].
- How it fails: linear textures or missing mips show as high bandwidth plus shimmering at distance. Runtime shader compile shows as one long hitch per new material — missing DKSH.

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
- Static/dynamic split: static lists hold world geometry; ring size 2–3 covers one frame being drawn while the next is built; fences scope the wait to one slot [reported].
- Image layout: `DkImageLayout` from deko3d Example04 reports block-linear pitch and layer offsets; storing images already tiled at build time avoids runtime converts [documented].
- Shader cache: DKSH files are precompiled deko3d **[shaders](../../glossary.md#shader)** loaded from storage at boot to skip just-in-time compile [reported].
- Small programs that run on the GPU should stay register-lean; Maxwell notes recommend packed half-precision types where verified, plus compressed mipmapped textures for the bandwidth ceiling — see performance page [documented].

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

- [deko3d](https://github.com/switchbrew/deko3d) — command lists, image layout, validation layer [documented].
- [libnx](https://github.com/switchbrew/libnx) — fences, queues, applet memory modes [documented].
- [switch-examples](https://github.com/switchbrew/switch-examples) — Example04 image layout, ring patterns [documented].
- [NVIDIA Maxwell tuning guide](https://docs.nvidia.com/cuda/maxwell-tuning-guide/) — coalescing, caching, occupancy, shared-memory sizing, bandwidth ceiling [documented].
