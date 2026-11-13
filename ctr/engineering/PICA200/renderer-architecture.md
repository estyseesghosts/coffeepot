# Renderer architecture for the Old 3DS: choosing between strategies

A renderer for the 3DS has to decide who builds the GPU's work each frame and how often: a general library call per object, a compact list of commands that your own code turns into GPU commands, or geometry that sits on the GPU and is only re-pointed. This page is a decision guide for that choice. It covers draw-call and command-buffer cost, vertex processing, how to re-express desktop pixel effects on the fixed-function fragment stage, and the memory and cache habits that go with each choice. It says what is documented, what a third party reported, and what nobody (including this wiki) has measured yet. The wiki has run no benchmarks, so every "faster" below is either a mechanism read from source or a `[reported]` figure from someone else's project.

## Before you start

- Console model: everything describes the [Old 3DS](../../glossary.md#old-new-3ds) unless a **New 3DS:** note says otherwise. The Old 3DS has two 268 MHz [ARM11](../../glossary.md#arm11) cores, a 16 KB instruction cache and no L2 cache ([CPU](../../hardware/cpu.md)); **New 3DS:** the CPU can run about three times faster with a 2 MB L2 cache, which moves the balance between CPU-side recording cost and GPU time, so a decision made on a New 3DS may not hold on an Old 3DS.
- Library versions: citro3d `9f21cf7` (v1.7.1) and libctru `62b7c42` (v2.7.1), the commits on [source versions](../source-versions.md). citro3d behaviour below was read from those files; nothing here was run on a console or an emulator.
- Terms: a **[draw call](../../glossary.md#draw-call)** is one request to draw; a **[batch](../../glossary.md#batch)** is a group of draws merged into one call; the **[command buffer](../../glossary.md#command-buffer)** is the memory region that receives the GPU register writes (a finished one is a **[command list](../../glossary.md#command-list)**); a **[render target](../../glossary.md#render-target)** is the image being drawn into. The pipeline from draw call to GPU is on [PICA200 command pipeline](pica200-command-pipeline.md); the citro3d API on [GPU rendering](gpu-rendering.md).
- Sibling pages: stereo rendering is on [stereo engineering](stereo-engineering.md); how shipped games and ports structured their engines is on [case studies and ports](../case-studies/case-studies-ports.md).

## The central idea: the CPU describes the frame, the GPU draws it

On an Old 3DS the CPU is slow and the GPU can take on vertex transformation, so the cost to minimise is usually the CPU work needed to *describe* a frame, not the triangle count. That is an inference from the hardware balance and from one project's report (see [What the one detailed Old 3DS report says](#what-the-one-detailed-old-3ds-report-says)); on a given game it is a `[hypothesis]` until the frame is split into CPU recording, `C3D_FrameEnd`, GPU time and waiting, as described on [performance](../performance.md#a-frame-level-breakdown). Do not restructure a renderer before that split says the recording side is the bottleneck.

## Retained or immediate rendering

| | Immediate (rebuild each frame) | Retained (build once, re-point) |
| --- | --- | --- |
| What the CPU does per frame | Generates vertices and often a draw per object | Updates a few uniforms, a camera, small dynamic buffers; issues draws against buffers that already exist |
| What it costs in memory | Little persistent data; dynamic buffers need rings ([below](#vertex-buffer-rings-and-double-buffering)) | Persistent vertex/index buffers in [linear memory](../../glossary.md#linear-heap); hard to change one vertex |
| Fits | Particles, text, debug shapes, animated or procedurally generated geometry, scenes that change every frame | Static maps, level geometry, characters with a skeleton done in the shader, repeated props |
| Fails when | Vertex generation or encoding dominates the frame | Content changes often enough that rebuilding the retained buffers costs more than streaming |

Mechanism `[documented: citro3d base.c, drawArrays.c, immediate.c]`: citro3d's own `C3D_ImmDrawBegin`/`C3D_ImmSendAttrib` path writes each attribute into the command buffer itself, so it is the most CPU-expensive way to send vertices; `C3D_DrawArrays`/`C3D_DrawElements` against buffers that already exist send only a fixed sequence of register writes ([what a draw call writes](pica200-command-pipeline.md#what-a-draw-call-writes)). That retained draws cost *less CPU time* than rebuilt ones is therefore mechanism, but the size of the difference for your scene is not known `[hypothesis]`; [`drawbench`](../../benchmarks/README.md#drawbench) is the planned measurement for the draw-call side.

A retained 2.5D map is a common fit: keep the map's geometry on the GPU and update only the camera, animated objects and small dynamic buffers (the pattern also discussed on [2D rendering](rendering-2d.md#building-layers-and-tilemaps)).

## Specialised compact render commands or a generic dispatch layer

A **generic layer** is an engine path of the shape *game object, then a generic renderer, then a display-list or scene interpreter, then a material abstraction, then vertex conversion, then the graphics library, then the GPU*. A **specialised path** is *game object, then compact render commands, then retained GPU geometry, then the GPU*, with one small loop per kind of object.

Why this might matter on an Old 3DS: the instruction cache is 16 KB per core and there is no L2 ([CPU](../../hardware/cpu.md#clocks-and-core-counts)), so a hot path that jumps between many large functions and indirect calls can stall on instruction fetch. Whether that is why a specialised path wins in a given engine is an inference; the one detailed account is `[reported]` below and describes its own synthetic probe, not your renderer. See also [CPU optimisation](../cpu-optimization.md) for the cache facts.

| Situation | Choose | Reason and cost |
| --- | --- | --- |
| A new renderer for a 3DS-only game | Compact per-category commands from the start | You control what reaches the GPU; you own the correctness burden |
| A port of an engine whose renderer talks a desktop-style API | Keep the generic layer first; profile; specialise only the categories that dominate the frame | Specialising everything is a rewrite; the postmortem below specialised the categories a race spends its frame on and left menus and one-off screens interpreted |
| A game with few draws per frame | citro3d or citro2d as they are | The library cost is not the bottleneck `[hypothesis]` |

### What the one detailed Old 3DS report says

`[reported]` G-Diffuser (a 3DS port of F-Zero X), postmortem *Road to 60, All Over Again* at <https://hypnotize.works/gdx-3ds/postmortem/native.html> and its repository document `docs/NATIVE-RENDERER.md`; both fetched in November 2026. The page labels entries 2026-09-07 to 2026-10-26 and does not give one publication date. Console: Old 3DS hardware unless stated; the page also reports emulator runs and New 3DS runs, labelled as such below. Statements, with conditions as the source gives them:

- Design: a hybrid. Menus, the editor, the ending and one-off screens stay on an interpreter; a race frame is drawn through a "native race renderer" that writes precomputed PICA200 register words straight into the command buffer, with "one small loop per race category, each sized to the 16 KB L1-I", and retained meshes drawn as one draw per material run.
- Draw cost: "10.9 µs per clean citro3d draw on Old 3DS; 72–98 µs per game draw through the interpreter" (a probe in the fork; the page gives no scene or draw state).
- Instruction cache probe (log dated 09-30): render CPI 4.34 with 43.6% of cycles in instruction-fetch stalls; a synthetic probe loop measured CPI 1.06 at 16 KB of hot code, 4.10 at 24 KB and 5.62 at 32 KB; a miss "~37 cycles (measured)"; the v0.9.0 snapshot had "~219 KB of hot code". The probe is synthetic and says nothing about other workloads.
- Per-frame CPU, Old 3DS, one rendered 2D race frame: the 10-01 interpreter build with early levers against native-renderer card 3 on 10-02, summed over the page's categories: "Sum 21.1 → 9.1 ms; logic per tick 1.51 → 1.58 ms, unchanged by design". The page raw-downloaded and text-matched on 11 November 2026 (figure caption). Per-category rows and the other race-frame tables are on [case studies and ports](../case-studies/case-studies-ports.md), not repeated here.
- Frame rate, Old 3DS: first boot "about 10 fps" on 09-29 (10.4 fps in its chart, legacy interpreter) to a 60 fps median in 2D races on 10-02 (card 3: "median frame 16.5 ms, 0 hitches"). These are two builds on two days, not one continuous run, and one game.

> Note: the review that prompted this page quotes these figures (10 to 60 fps in 2D racing, 59.7 to 59.8 fps in stereo racing, 21.1 to 9.1 ms) and they are in the postmortem as `[reported]`, with the conditions above. The review does not mention that the stereo race figure is the postmortem's own "suggestive" result (56.4 / 59.3 fps baseline, "two blocks per arm"), that Old 3DS 3D "remains CPU-bound" (idling the GPU first did not make the frame cheaper: 13.5 to 14.2 ms, 11.8 to 12.3 ms) and that 3D multiplayer is locked to 30. The stereo figures are on [stereo engineering](stereo-engineering.md) and the replay mechanism is on [PICA200 command pipeline](pica200-command-pipeline.md#advanced-right-eye-command-replay-within-one-gpu-submission-reported).

> Unknown: whether the New 3DS behaves the same for the instruction-cache effect. The source reports its New 3DS stereo result as "neutral" and gives no instruction-cache data for it; the L2 cache on the New 3DS ([ARM11 MPCore](../../hardware/arm11-mpcore.md)) is the obvious reason it might differ, which is a `[hypothesis]`.

### A second report: a GPU-native renderer for a DS game

`[reported]` The kohjoh port of Pokémon HeartGold (private repository, no public revision yet; `docs/architecture.md`, `docs/status.md`, read 2026-11-13) replaces a CPU software renderer with GPU drawing of the DS's 2D layers and 3D scene. Every result below was checked in Azahar against reference renders, not on an Old 3DS, except where marked; the project's Old 3DS re-test of the same code was still pending when the notes were written. Design points that carry over to other tile- and sprite-based sources:

- **One publish point.** The game loop runs on one thread; at the DS V-blank it publishes one coherent snapshot (registers, palettes, sprite table, video memory banks, per-line registers, last 3D scene), and the renderer reads nothing from emulated memory afterwards. This removed the drawer/game races of the earlier thread-split port.
- **Texture atlas slots reused only on byte equality.** Each DS 8×8 tile is expanded through its palette into one 8×8 tile of a 1024×1024 RGBA5551 atlas. A slot is reused only if its source bytes are identical and its palette version is unchanged: no write tracking and no hashing.
- **Stencil bits carry blend and window membership.** Layers are drawn in DS priority order; stencil bits mark blend-target membership (an alpha blend takes two draws) and window regions, and brightness effects are full-screen passes. This needs a render target created with a stencil buffer ([2D rendering](rendering-2d.md#layers-and-composition)).
- **Retained layers and whole-screen skip.** A background or sprite layer is retained when its registers, palette, map or sprite table, extended palettes and the character bytes it read are byte-identical. In one Goldenrod window, 557 of 600 bottom-screen pictures needed no draw or transfer at all.
- **Per-line register changes become spans.** A frame whose registers change mid-frame is drawn as spans of identical lines, not per line. The project's feared "retention cliff" under scanline effects did not occur in one idle Goldenrod window (one span a frame); walking and route scenes were unmeasured.
- **Matrix palette for 3D.** Vertices stay in object space; each batch carries up to 16 clip matrices, each multiplied by one fixup matrix (viewport, widescreen scale, screen rotation, depth range), and the vertex shader transforms on the GPU. Matrix stacks, lighting and geometry tests stay on the CPU because the game reads their results back.
- **Unsupported effects are counted, not dropped.** Fog, toon shading, edge marking and shadow volumes were counted when used so a missing effect cannot hide.

Costs the project recorded (Azahar instruction counts, Goldenrod at rest): logic 25.6 to 13.7 ms per frame over its optimisation sessions, of which a display-list replay cache gave the largest single step (21.0 to 13.8 ms) once command streams stopped being split mid-command ([porting](../porting.md#lessons-from-eleven-source-read-ports)). These are emulator instruction counts, a floor rather than an estimate for an Old 3DS ([debugging](../debugging.md#calibrate-the-emulators-clock-before-reading-its-timings)). On the Old 3DS the same port's profiled build ran Route 34 near 30 FPS and Goldenrod at about half speed before several later fixes, as its own log reported.

## Draw-call and command-buffer optimisation

What citro3d does on each action is listed in [what a draw call costs](gpu-rendering.md#what-a-draw-call-costs). The techniques below remove a specific cost named in that table, so each can be checked against the source.

### Batching by texture and material

Sort and group draws by texture, then shader, then combiner configuration, then draw each group with one call. citro3d's `C3D_TexBind` always marks the unit dirty, and the next draw then rewrites the texture registers and writes a texture-cache clear (bit 16 of `GPUREG_TEXUNIT_CONFIG`) `[documented: citro3d texture.c, base.c lines 225-243]`. A change of shader program, attribute layout or buffer set causes more register writes than a uniform change ([GPU rendering](gpu-rendering.md#what-a-draw-call-costs)). Atlases (several images in one texture, see [asset pipeline](asset-pipeline.md)) let different sprites share one bind. Cost of the technique: sorting work on the CPU and, for translucent geometry, a conflict with back-to-front order (translucent objects must be drawn after opaque ones in depth order, which limits batching).

**Batching correctness includes state lifetime.** A compatible batch must preserve texture bindings, vertex ranges, depth, stencil, blending and ordering. In one HGSS development build, batch merging plus a stencil change produced duplicated characters and halos; the team reverted both changes while retaining separate state caching [reported: Old 3DS and New 3DS, project session 20, November 2026; [case study](../case-studies/case-study-hgss-port.md#state-lifetime-and-batch-boundaries)]. Test overlapping geometry and effects as well as simple opaque objects. Clear retained admission metadata when its enabling state changes; old object-list counts can otherwise nominate stale GPU work even when software drawing checks the current enable bit.

### Renderer-side state caching

Keep your own record of what is bound and skip redundant calls. Mechanism: citro3d does not compare a new texture, program or buffer with the old one before marking state dirty (`C3D_TexBind` is the documented example above), so a renderer-side check is the only place a redundant bind can be avoided. Combiner (`C3D_TexEnv*`) and depth/blend/alpha calls only emit what changed ([GPU rendering](gpu-rendering.md#what-a-draw-call-costs)); texture binds are the case worth caching. Cost: your cache must be invalidated whenever other code (citro2d, a library, a menu) changes the same state; citro2d changes citro3d state itself and the docs say to call `C2D_Prepare()` again afterwards ([GPU rendering](gpu-rendering.md#2d-games-citro2d)).

### Avoiding redundant render-target selection

`C3D_FrameDrawOn(target)` sets the framebuffer and viewport and marks the target used `[documented: citro3d renderqueue.c lines 179-187]`; switching to a different target after drawing adds a framebuffer flush and invalidate and an early-depth clear at the next split ([pipeline](pica200-command-pipeline.md#what-a-draw-call-writes), `base.c` `C3Di_SplitFrame`). Draw everything for one target before moving to the next, and do not select a target you do not draw to. Stereo is the unavoidable case where two top targets are used each frame ([stereo engineering](stereo-engineering.md)).

### Avoiding redundant texture-cache invalidation

The invalidation comes from the dirty texture flag above, not from the texture data. Do not call `C3D_TexBind` for a unit whose texture has not changed. For a texture whose *contents* the CPU changed, the CPU data cache must also be flushed ([memory and cache](../memory-and-cache.md#texture-uploads)); that is a different cache from the GPU's texture cache.

### Keeping submissions coarse

citro3d submits one command list per frame unless you call `C3D_FrameSplit`. Each additional list is another GX command in the queue; libctru's queue runs up to three commands at a time and each completion interrupt releases the next ([pipeline](pica200-command-pipeline.md#frame-end-gx-and-the-gsp)). The only quantitative statement available is `[reported]`: G-Diffuser's probes measured "0.4–0.5 ms of GPU dead time per backlogged queue entry" on Old 3DS (about 40 µs on New 3DS), which is why it merged its per-chunk lists. That is one project's probe, with the conditions stated on its page, and it is not extended here to other workloads.

> Correctness: split lists do not start early. `C3D_FrameSplit` queues the command; the queue is stopped and cleared at `C3D_FrameBegin` and only started by `gxCmdQueueRun` at the end of `C3D_FrameEnd`, after the heap flush `[documented: citro3d renderqueue.c lines 92-100, 197-232; libctru gxqueue.c lines 62-91]`. So a flush done before `C3D_FrameEnd` covers every list of the frame. If you submit lists yourself while a queue is active and idle, `gxCmdQueueAdd` starts the first command immediately, and then your flush must precede the submit.

### Warnings for bypassing citro3d

Writing PICA200 register words yourself (a "native" path) gives control and removes library work, and gives up the protections the library provides. All of the following are consequences of what citro3d does for you, read from the pinned source:

- citro3d tracks which state is dirty and re-emits it; a hand-written path has to re-send everything the next citro3d draw assumes. After changing the shader program the attribute format, input-buffer, attribute count and permutation registers must be reconfigured or the GPU can crash ([shading features](../../hardware/gpu-shading-features.md#behaviours-and-quirks-found-while-reverse-engineering)); whether citro3d re-sends these on a program change was not checked.
- citro3d's draw sequence ends with `VTX_FUNC` and two `PRIMITIVE_CONFIG` writes that the wiki lists as required; a missing step is a possible hang, and which extra steps in the official SDK's sequence are *required* is unknown ([pipeline](pica200-command-pipeline.md#what-a-draw-call-writes)).
- citro3d keeps `C3DiF_DrawUsed` so that the next target change adds framebuffer flush, invalidate and early-depth clear; direct writers must do the same or get stale framebuffer or depth data `[documented: citro3d base.c C3Di_SplitFrame]`.
- Command lists need `FINALIZE`, 16-byte alignment and no overrun; `GPUCMD_AddRawCommands` has no bounds check ([diagnosis checklist](pica200-command-pipeline.md#diagnosis-checklist)). NaN or invalid parameters can hang the GPU and the GSP module with it.
- The wiki's [decoder](pica200-command-pipeline.md#host-side-command-list-decoder) has seen only synthetic data. There is no hardware-validated way here to prove a hand-built list is right; the G-Diffuser project used bit-exact comparison of vertex bytes against the stock path plus frame hashes and hardware replays `[reported]`.
- A hand-built path may hang the GPU on one model and not another; applicability to the New 3DS of the register findings is unknown ([pipeline](pica200-command-pipeline.md#open-questions)).

Take the specialised-command path only when the library's overhead is a measured bottleneck, keep the library path available behind a switch, and compare output against it.

## Vertex processing

### GPU transformations and packed formats

Upload object-space vertices once and pass the matrix as a uniform; do not transform on the CPU and upload screen positions every frame ([GPU rendering](gpu-rendering.md#do-work-in-the-vertex-shader)). Vertex attributes can be `GPU_BYTE`, `GPU_UNSIGNED_BYTE`, `GPU_SHORT` or `GPU_FLOAT`, one to four components, up to 12 attributes; bytes and shorts reach the shader as numbers you scale with uniforms `[documented: libctru gpu.h, citro3d attribs.c]`. A 16-bit position plus byte normal and byte colour is a third the size of an all-float vertex (arithmetic: 3x2 + 4 + 4 = 14 bytes padded to 16 versus 3x4 + 3x4 + 4x4 = 40 bytes, with the stride rule that the buffer stride must equal the real vertex size, [pitfalls](../../hardware/gpu.md#known-pitfalls)). Trade-off: lost precision (coordinates must fit the quantised range), a scale/offset uniform per mesh, and more shader instructions. Whether the saved bytes matter for GPU fetch or only for CPU writing of dynamic data is not known `[hypothesis]`.

**Fixed attributes** (`AttrInfo_AddFixed`, `C3D_FixedAttribSet`) supply a value that is constant for a whole draw (a tint, a normal) without storing it per vertex `[documented: citro3d attribs.c]`.

### The geometry shader trade-off: three or four vertex processors

The review that prompted this page says the PICA200 has four vertex processors, or three when the geometry stage is used. Checked against local copies of the sources:

| Source | What it says |
| --- | --- |
| Rodrigo Copetti, *Nintendo 3DS Architecture*, Graphics (local mirror of the open-source article text) | Four Vertex Processors operate in parallel; if the geometry shader is activated "only three" can be used; the geometry stage "steals one of the four Vertex Processors" |
| 3dbrew *Hardware* | "PICA200 has three programmable vertex processors" plus a *Primitive Engine*, a geometry shader unit that, with its Primitive Engine functionality disabled, "acts as a fourth vertex processor" |
| 3dbrew *GPU/Primitive Engine* | The Primitive Engine "is one of the PICA200's four vertex processor units" |
| 3dbrew *GPU/Internal Registers*, `GPUREG_SH_ENTRYPOINT` | Vertex-shader units can be "all 4 shader units or just 3 of them" depending on the geometry stage mode; one unit can run a different entry point |

> Conflict: Copetti and the 3dbrew Primitive Engine and Internal Registers pages count four units that become three vertex processors when the geometry stage is active; the 3dbrew Hardware page counts three vertex processors plus a geometry unit that can serve as a fourth. These describe the same arrangement with different counting, so the "3 or 4" claim is consistent across sources. What no source states is the throughput cost: how much slower vertex work becomes when a geometry shader is bound, or whether the geometry shader's own cost outweighs the saving from expanding a point to a quad. That is unmeasured.

> Unknown: on which console model Copetti's and 3dbrew's descriptions were checked, and any New 3DS difference; neither mentions one. A vertex-throughput run with and without a geometry shader on an Old 3DS and a New 3DS would settle it (experiment G2 below).

Practical use: a geometry shader can expand a point into a quad (sprites, particles; the `3ds-examples` `geoshader`, `particles`, `gpusprites` samples) so one vertex is sent instead of four, and so the CPU writes and the memory reads shrink. The price is losing a vertex processor while it is on; whether the trade pays is a `[hypothesis]` to benchmark per workload.

## Fragment-stage reformulation: desktop effect to PICA200 fixed-function

The PICA200 has no programmable pixel shader; the fragment stage is texture units, six colour-combiner (**[TEV](../../glossary.md#tev)**) stages, fragment lighting driven by lookup tables (**[LUTs](../../../glossary.md#lut)**), fog, procedural texture, blending, depth and stencil ([GPU shading features](../../hardware/gpu-shading-features.md)). The table says what each effect has to become. Every row is checked against the wiki's shading-features page (and, for registers, [GPU internal registers](../../hardware/gpu-internal-registers.md)); wording such as "can" means a unit exists, not that the result looks right or was tried.

| Desktop effect | PICA200 reformulation | What exists (evidence) | Limit or cost |
| --- | --- | --- | --- |
| Textured, vertex-tinted surface | TEV stage `tex0 x primary colour` | `[documented]` combiner recipes on [GPU rendering](gpu-rendering.md#colour-combiners-tev-recipes) | Six stages shared by everything in the draw |
| Per-pixel Blinn-Phong lighting | Fragment lighting, LUT D0 holds the specular exponent curve | `[reverse-engineered]` 3dbrew, DMP papers ([lighting](../../hardware/gpu-shading-features.md#fragment-lighting)) | Surface frame is passed as a quaternion from the vertex shader; there is no normal or tangent output semantic |
| Cook-Torrance or Schlick-like specular, Fresnel rim | Lighting LUT D1 plus reflection and Fresnel LUTs | same | Tables must be built by the CPU; sizes are 256 entries (citro3d `lightlut.h`) |
| Toon / cel shading | Make adjacent LUT entries equal | same page | Banding is set by the table |
| Spotlight falloff | Spotlight LUT with the light/spot-axis input | same | One setup per light |
| Normal-mapped lighting | Fragment lighting with a perturbed normal from a texture | The wiki's shading page records normal-mapping and tangent-mapping behaviour found by wwylele; no citro3d walkthrough or example was verified for this page | `[hypothesis]` for practical use until a test scene is built; behaviour with the `CP` input is quirky |
| Lightmaps | Precomputed texture, multiplied by the base texture in a TEV stage | `[documented]` TEV modulate | Costs a texture unit; baked, not dynamic |
| Ambient occlusion | Bake into textures or vertex colour; the vertex colour reaches the combiner as the primary colour | `[documented]` TEV | Static scenes only |
| Fog | Fixed-function fog unit with a 128-entry fog LUT, or blend fog in a combiner with a constant colour | Fog registers `GPUREG_FOG_COLOR`, `FOG_LINEAR_SLOPE`, `FOG_LUT_*` (3dbrew Internal Registers); `[reported]` G-Diffuser switched the fog unit off and blended fog in a combiner, falling back when a track needed fog it could not express | The two routes differ in what they can express; compare on your scene |
| Real-time shadows | Shadow-map mode (`GPU_FRAGOPMODE_SHADOW`), only texture unit 0 samples shadows; or projected translucent polygons; or baked shadow textures | `[reverse-engineered]` fincs notes ([shadows](../../hardware/gpu-shading-features.md#shadows)) | The colour buffer is used as the shadow depth buffer; soft shadows normally use a silhouette geometry shader |
| Environment reflections | Cube-map texture type, or an authored reflection texture | Texture unit types include cube map and shadow cube (3dbrew Internal Registers `GPUREG_TEXUNIT_i_TYPE`); cube maps are on texture unit 0 only ([GPU rendering](gpu-rendering.md#textures)) | Memory for six faces |
| Water, flowing surfaces | UV animation through uniforms, layered textures blended in TEV, procedural texture | `[documented]` TEV and UV uniforms; procedural unit `[reverse-engineered]` ([procedural textures](../../hardware/gpu-shading-features.md#procedural-textures-texture-unit-3)) | The procedural unit is reported as very hard to use ("only four people at the company can use it fully", CEDEC 2012 talk) |
| Volumetric smoke / clouds | Gas rendering (accumulation) | Registers known; the wiki calls gas undeciphered ([gas](../../hardware/gpu-shading-features.md#gas)) | Not recommended without an experiment |
| Bloom, depth of field, colour grading | No fragment hook. Render to a texture and composite a quad with TEV and blending, or pre-grade assets | `[documented]` render-to-texture API ([GPU rendering](gpu-rendering.md#render-to-texture)) | VRAM and fill cost grow with the extra target; **benchmark** (G3, G5); no hardware equivalent of a post-process shader |
| Many alpha-blended particles | Batch by texture; use alpha test where hard edges are enough; geometry shader point expansion | `[documented]` `C3D_AlphaTest`, `geoshader` example | Overdraw is fill-rate bound (G3) |
| Palette swap | Direct-colour atlases, vertex tint or TEV colour; no hardware palette lookup | [GPU rendering](gpu-rendering.md#what-the-pica200-cannot-do) | CPU conversion when the palette changes |

> Conflict: the review lists "bloom: selective render-to-texture composition" and "colour grading: lookup-based or fixed-function approximations" as implementation strategies. The wiki sources document render-to-texture and the combiner stages but nothing that implements a lookup-based colour grade; the procedural-texture unit builds textures from coordinates, not a grade of the framebuffer. Treat the grading row as "pre-grade assets", and any in-engine grade as untested `[hypothesis]`.

> Unknown: whether the lighting and procedural-texture findings were observed on an Old 3DS or a New 3DS. The shading page says no source states it.

## Memory and cache habits that go with each strategy

The full treatment is on [memory and cache](../memory-and-cache.md); the architecture-level consequences are:

### Vertex-buffer rings and double buffering

Data the CPU rewrites each frame must not be overwritten while the GPU still reads last frame's copy. With citro3d, "GPU finished" for frame N is when `C3D_FrameBegin` returns for frame N+1 ([buffer lifetime](../memory-and-cache.md#buffer-lifetime)). Two designs follow:

- **Double buffer:** two copies, alternate each frame. Memory cost is 2x the dynamic data.
- **Ring:** one large buffer, write forward each frame, wrap when the oldest region is certainly done. A ring must be large enough to hold at least the data of the frames still in flight, and the wrap point must be checked against the GPU-finished moment. Preallocate it at start-up ([pitfalls](../memory-and-cache.md#pitfalls)).

G-Diffuser's source warns that skipping the GPU wait is unsafe when "one command buffer and a VBO pool both restart at offset 0 every frame" `[reported]`, which is exactly this hazard.

### Flush-range tracking, the C3D_FrameEnd flush and GX_CMDLIST_FLUSH

Read from citro3d `renderqueue.c` and libctru `gx.c` at the pinned commits:

| Item | What it does | Source |
| --- | --- | --- |
| `C3D_FrameEnd(0)` | Calls `GSPGPU_FlushDataCache((void*)__ctru_linear_heap, __ctru_linear_heap_size)`: an IPC request that cleans and invalidates the whole linear heap, however much of it you touched. The comment says: "Flush the entire linear memory if the user did not explicitly mandate to flush the command list" | citro3d `renderqueue.c` lines 210-216 `[documented]` |
| `GX_CMDLIST_UPDATE_GAS_ACC` (bit 0) | Copied to word 3 of the GX command; the libctru comment says it is "written to GSP module state". It is about gas accumulation, unrelated to caches | libctru `gx.c` line 53, `gx.h` `[documented]` |
| `GX_CMDLIST_FLUSH` (bit 1) | Copied to word 7 of the GX command; the libctru comment says that when non-zero the GSP module calls `svcFlushProcessDataCache()` "with the specified buffer", i.e. the command list itself. In citro3d it also **suppresses the whole-heap flush** | libctru `gx.c` lines 47-55, citro3d `renderqueue.c` line 211 `[documented]` |
| Your own data | With `GX_CMDLIST_FLUSH`, nothing flushes your vertex, index or texture bytes. The flag does not flush them, and that is why you must | derived from the two rows above |

What the GSP module does with word 7 is not in the sources read; the libctru comment is the only statement. `C3D_FrameSplit(flags)` passes `flags` through but contains no heap flush; only `C3D_FrameEnd` does. See [memory and cache](../memory-and-cache.md#the-c3d_frameend-flush) and the tracking design in its [flush-range tracking section](../memory-and-cache.md#flush-range-tracking).

> Correctness: `C3D_FrameEnd(GX_CMDLIST_FLUSH)` without flushing your own ranges produces stale vertices or texels, often intermittently. Never ship it before the default path and the tracked path produce identical frames on a test scene. The cost of the default flush has not been measured on any model; the planned measurement is [`flushbench`](../../benchmarks/README.md#flushbench).

`[reported]` G-Diffuser ships a "narrow flush" (`onesubmit_narrow`): with its one-submission stereo path, a 3D frame's end flushes only the dirty ranges its `[overlap]` module noted, instead of the whole linear heap. Its repository document (`docs/NATIVE-RENDERER.md`, raw-downloaded 11 November 2026) states the effect as "another ~0.3 ms on the Old 3DS, -0.4 ms GX CPU on the New 3DS", and `0` restores the whole-heap flush. That is a project-stated estimate, not a wiki measurement, for one game; the document does not describe the range bookkeeping or any bug it hit.

### Upload cost versus VRAM placement

The CPU cannot write VRAM in homebrew; filling it takes a GPU copy from a flushed linear buffer, so a VRAM texture costs an upload and saves, possibly, texture-read time. Do not assume VRAM placement is a win: the upload cost, the sampling frequency and the free VRAM (6 MiB shared with render targets) all matter, and no measurement exists here. The planned experiment is G4. See [memory and cache](../memory-and-cache.md#texture-formats-and-sizes-for-a-512x512-texture) for sizes.

## Decision table

| Situation | Recommended approach | Evidence label | What to measure |
| --- | --- | --- | --- |
| Frame misses 60 fps and you do not know why | Split CPU recording, `C3D_FrameEnd`, GPU time and waiting before changing anything | `[documented]` APIs ([performance](../performance.md#a-frame-level-breakdown)) | `C3D_GetProcessingTime`, `C3D_GetDrawingTime`, time in `C3D_FrameBegin` |
| Hundreds of small draws sharing a texture | Merge into one vertex buffer and one draw per material | `[documented]` mechanism; magnitude `[measured]` on an Old 3DS: 9.7 µs per separate draw against 0.21 ms for 1000 batched quads ([GPU rendering](gpu-rendering.md#what-a-draw-call-costs)) | `drawbench` `batched` vs `per-quad`; `record_ms` |
| Many sprites with several textures | Atlas, sort by atlas, bind once | `[documented]` (`C3D_TexBind` always dirties the unit); cost `[measured]` on an Old 3DS: 2.1 µs per redundant bind or texture swap, about a fifth of a separate draw | `drawbench` `+texbind`, `+texswap` |
| Static geometry rebuilt every frame | Build once in linear memory, retain | `[documented]` mechanism; `[hypothesis]` gain | Vertex-generation time per frame |
| Draw call count high even after batching, library overhead confirmed | Specialised compact commands for the dominant categories only | `[reported]` G-Diffuser, single project | CPU frame time before and after; bit-exact output comparison |
| Large generic hot path with indirect calls | Group by operation type; keep hot loops small | `[hypothesis]` (cache mechanism); `[reported]` for one engine | Instruction-fetch stalls with [performance counters](../../../glossary.md#performance-counters) (G6) |
| `C3D_FrameEnd` appears expensive | Compare against the full-heap flush cost; consider flush-range tracking and `GX_CMDLIST_FLUSH` | `[documented]` mechanism; the heap flush itself is `[measured]` on an Old 3DS at about 90 µs per frame ([memory and cache](../memory-and-cache.md#measured-costs-old-3ds)), so look elsewhere first | `flushbench`; time around `C3D_FrameEnd(0)` |
| Dynamic vertex data every frame | Double buffer or ring, preallocated | `[documented]` hazard | Visual comparison with one buffer vs two; buffer size vs frames in flight |
| Point sprites or particles | Geometry-shader expansion | `[documented]` capability; `[hypothesis]` net gain | Vertex throughput with and without the geometry shader (G2) |
| Memory tight, art allows lossy | ETC1 / 16-bit formats | `[documented]` sizes (arithmetic) | Texture bandwidth and quality (G4) |
| Post-process effect wanted | Render to a texture and composite; budget VRAM first | `[documented]` API | Fill rate and render-target switch cost (G3, G5) |
| 3D stereo race/scene near the budget | Reuse visible lists; consider one-submission replay | `[reported]` | See [stereo engineering](stereo-engineering.md) |

## Planned GPU workload characterisation benchmarks

This is a design. The programs that exist (`drawbench`, `flushbench`, `membench`, `icachebench`, `corebench` and the tile renderer) have hardware results for part of G1 (draw overhead) and G6 (flush cost); G2 to G5 have no program and no result. Each experiment states what would be varied and what would be recorded; a result is `[measured]` only after it is run on a named model and recorded in the benchmarks README ([how to record results](../../benchmarks/README.md#recording-results)). Emulator timings are never results ([testing on hardware](../testing-on-hardware.md)).

| ID | Question | Independent variable | Held constant | Record |
| --- | --- | --- | --- | --- |
| G1 draw overhead | What does a draw and each state change cost the CPU and the GPU? | Draw count at fixed triangle count; state change type (texture bind, uniform, combiner, program, buffer) | Total pixels and triangles | CPU record time, `C3D_FrameEnd` time, GPU time, command-buffer use. Existing: [`drawbench`](../../benchmarks/README.md#drawbench) |
| G2 vertex processing | What does vertex shader length, attribute size and a bound geometry shader cost? | Shader instruction count; vertex format (float vs packed); geometry shader on/off with point-to-quad expansion | Pixel coverage kept tiny so fill is not the limit | GPU time per frame; vertices per second |
| G3 fill rate | How fast can the GPU fill pixels? | Colour format (RGBA8, RGB565), depth format (24-bit/8-bit stencil vs 16-bit), blend on/off, TEV stage count, texture units | Few large triangles | GPU time per frame, converted to pixels per second |
| G4 texture bandwidth | What does texture format, size, minification and placement cost? | Format (see [sizes](../memory-and-cache.md#texture-formats-and-sizes-for-a-512x512-texture)), mipmaps, linear memory vs VRAM, magnification vs minification | Same screen coverage | GPU time per frame; upload time (`GX_TextureCopy`) |
| G5 render-target changes | What does switching targets cost, and when is a cached background texture worth its target? | Targets per frame; draws per target; render-to-texture vs direct draw | Total pixels | CPU and GPU time; VRAM use. Existing 2D design question: [2D rendering](rendering-2d.md#6-performance-decisions) |
| G6 synchronisation | What do waits, list splits and cache flushes cost? | Lists per frame; `C3D_FrameEnd(0)` vs `GX_CMDLIST_FLUSH` with tracked flushes; blocking vs `C3D_FRAME_NONBLOCK` | Same draw content | Time in `C3D_FrameBegin`, `C3D_FrameEnd`; missed refreshes. Existing: [`flushbench`](../../benchmarks/README.md#flushbench), [`membench`](../../benchmarks/README.md#membench) |

Method rules for all: Old 3DS first, then New 3DS (New 3DS results are never merged into the Old 3DS table); fixed core-1 share; report median, p95 and p99 frame time and missed frames, not only an average; run the CPU work in a separate build with the GPU work removed to see which side limits; record the build, library commits and system version. A verification step (screenshot or framebuffer comparison) is part of each variant so a fast variant that draws nothing is not counted. The instruction-cache question (G-Diffuser's probe idea) is a CPU experiment and belongs with [CPU optimisation](../cpu-optimization.md).

## Technical details

- Where each statement was read: citro3d `source/renderqueue.c` (`C3D_FrameBegin` 163-177, `C3D_FrameDrawOn` 179-187, `C3D_FrameSplit` 189-195, `C3D_FrameEnd` 197-232, `C3Di_WaitAndClearQueue` 92-100); `source/base.c` (texture unit update and cache clear at lines 225-243, `C3Di_SplitFrame` 309-328); `source/texture.c` (`C3D_TexBind` 222); libctru `source/gpu/gx.c` (`GX_ProcessCommandList` 47-56) and `gxqueue.c` (`gxCmdQueueAdd`, `gxCmdQueueRun` 62-91). Line numbers are at the pinned commits.
- `GX_CMDLIST_UPDATE_GAS_ACC` is `BIT(0)` and `GX_CMDLIST_FLUSH` is `BIT(1)` (libctru `gx.h`).
- Vertex processor counting is a source disagreement in wording only (above).
- Depth-buffer format choice (16-bit versus 24-bit with 8-bit stencil) changes both memory and fill; see [GPU rendering](gpu-rendering.md#16-bit-or-24-bit-depth).

## Pitfalls

- **Specialising before profiling.** Symptom: weeks of work, no change in frame time because the bottleneck was GPU or waiting. Fix: split the frame first.
- **Calling `C3D_TexBind` every draw "to be safe".** Symptom: a texture-cache clear and texture register rewrite per draw. Fix: cache the bound texture yourself.
- **`GX_CMDLIST_FLUSH` without flushing your buffers.** Symptom: intermittent stale geometry or texels. Fix: track ranges, flush, and compare with the default path.
- **A ring that wraps over data the GPU has not finished.** Symptom: flicker or torn geometry that depends on load. Fix: size the ring for the frames in flight, wrap only after the GPU-finished point.
- **Hand-built command lists and library state.** Symptom: a hang after a library call that assumed state you changed. Fix: re-send the state after your path, or keep the paths in separate frames.
- **Assuming a desktop effect has a hardware twin.** Symptom: a fragment effect with no place to go. Fix: bake it, use render-to-texture, or drop it.
- **Quoting a developer figure as a general rule.** Symptom: advice that fails on your workload. Fix: keep the `[reported]` label and the stated conditions.

## Open questions

- All performance questions above are unmeasured: draw and state-change costs, the full-heap flush cost, geometry-shader throughput cost, VRAM versus linear sampling, render-target switch cost. The experiments G1 to G6 would settle them.
- What the GSP module does with word 7 of the GX command beyond the libctru comment.
- Whether the specialised-command approach carries over to a game whose cost is not a per-object render loop (a tile map, for example); the source concerns a racing game.

## Sources

- kohjoh project notes (private repository, no public revision yet; `docs/architecture.md`, `docs/status.md`, read 2026-11-13) — renderer design and Azahar results.
- [G-Diffuser: Road to 60, All Over Again](https://hypnotize.works/gdx-3ds/postmortem/native.html) — `[reported]` Old 3DS renderer account; fetched November 2026; entries dated 2026-09-07 to 2026-10-26.
- [G-Diffuser `docs/NATIVE-RENDERER.md`](https://github.com/cruxxxxxx/gdx-3ds/blob/main/docs/NATIVE-RENDERER.md) — `[reported]` configuration levers, retained meshes, flush and submission behaviour.
- [citro3d](https://github.com/devkitPro/citro3d) at `9f21cf7` — `source/renderqueue.c`, `base.c`, `texture.c`, `immediate.c`, `attribs.c`.
- [libctru](https://github.com/devkitPro/libctru) at `62b7c42` — `source/gpu/gx.c`, `gxqueue.c`, `include/3ds/gpu/gx.h`.
- Rodrigo Copetti, [Nintendo 3DS Architecture: Graphics](https://www.copetti.org/writings/consoles/nintendo-3ds/) — vertex-processor counting (read from a local copy of the article source).
- [3dbrew: Hardware](https://www.3dbrew.org/wiki/Hardware), [GPU/Primitive Engine](https://www.3dbrew.org/wiki/GPU/Primitive_Engine), [GPU/Internal Registers](https://www.3dbrew.org/wiki/GPU/Internal_Registers), [GPU/Fragment Lighting](https://www.3dbrew.org/wiki/GPU/Fragment_Lighting) — vertex processor counts, fog and texture-unit registers (read from local mirrors).
- [GPU shading features](../../hardware/gpu-shading-features.md) — lighting, procedural texture, shadow and gas findings with their own sources.
