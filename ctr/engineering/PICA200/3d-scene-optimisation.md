# 3D scene optimisation: visibility, ordering, detail and baked effects

A 3D scene on the Old 3DS is cheap to describe and cheap to draw only if the program decides early what not to draw, draws what remains with few state changes, and moves work from run time into the art. This page covers the scene-side techniques that older consoles used and that apply to the PICA200: visibility culling, draw ordering, level of detail, animation budgets, baked lighting, and mixing 2D character quads into a 3D scene. For each it says what the wiki's sources support and what is still a guess.

## Before you start

- Checked against citro3d v1.7.1 (`effect.c`, `base.c`, `maths.h`), libctru v2.7.1, the 3dbrew GPU register pages and the `3ds-examples` GPU samples, at the commits on [source versions](../source-versions.md). Applies to the Old 3DS; the wiki has run no benchmark for anything below, so there are no `[measured]` results on this page.
- Evidence labels follow AGENTS.md. Most of the advice here is `[hypothesis]`: standard practice from other platforms that has not been measured on a 3DS. Where a source was fetched, it is named.
- Terms: **frustum culling** is skipping an object whose bounding volume lies outside the camera's view volume; **occlusion** is one object hiding another; a **portal** is an opening between rooms used to decide which rooms can be seen; **LOD** (level of detail) is a cheaper version of an asset used when it is far away or small; **overdraw** is drawing the same screen pixel more than once. Draw calls and batches: [draw call](../../glossary.md#draw-call), [batch](../../glossary.md#batch). Fragment pipeline: [TEV](../../glossary.md#tev), [PICA200](../../glossary.md#pica200).
- Related: [GPU rendering](gpu-rendering.md) (draw cost and state), [stereo engineering](stereo-engineering.md) (culling once for two eyes), [CPU optimisation](../cpu-optimization.md), [memory and cache](../memory-and-cache.md), [asset pipeline](asset-pipeline.md#asset-production-for-constrained-hardware) (baking and budgets), [2D rendering](rendering-2d.md) (sprite depth).

## Visibility: do not send what cannot be seen

The GPU is fast at triangles you give it; the Old 3DS CPU is slow at deciding things per object per frame. The aim is to make the decision cheap and coarse.

| Technique | What it does | Cost on the CPU | Fits |
| --------- | ------------ | --------------- | ---- |
| Object-level frustum culling | Test a bounding sphere or box against six planes; skip the draw if outside | A few multiply-adds per object per frame | Scenes with many separate objects |
| Spatial grid or tree over static objects | Reject whole groups at once | Cheap if the structure is flat and array-based | Large outdoor maps |
| Cells and portals | The world is rooms; draw the room the camera is in plus rooms visible through open portals | Small per-frame graph walk | Indoor, corridor, dungeon |
| Precomputed visibility | Offline, record which cells can see which; at run time look up the camera cell | A bitset lookup per frame | Fixed environments where camera paths are constrained |
| Distance cut-off and fade | Skip or simplify beyond a range | Almost free | Outdoor, with fog hiding the edge |

All of these are `[hypothesis]` for the Old 3DS as to how much they save: the technique is general graphics practice, and its benefit here depends on whether the scene is limited by CPU submission, vertex work or fill, which the wiki has not measured. Use [performance](../performance.md) to find which limit you hit before building a visibility system.

Practical points that follow from known costs:

- Cull at the level of objects or meshes (chunks of terrain), not triangles. Per-triangle CPU culling costs more than sending the triangles `[hypothesis]`.
- Each surviving object is at least one draw call, so culling and [batching](gpu-rendering.md) interact: more, smaller chunks cull better but batch worse. The right chunk size is a measurement.
- Keep the visible list in a flat array of indices into struct-of-arrays data, so the cull loop touches few cache lines ([CPU optimisation](../cpu-optimization.md), [memory and cache](../memory-and-cache.md)).
- For stereo, cull once with a frustum wide enough for both eyes ([stereo engineering](stereo-engineering.md#what-the-second-eye-costs)).
- Precomputed visibility trades authoring time and data size for run-time CPU. It breaks if geometry changes at run time `[hypothesis]`.

### Indoor against outdoor

Indoor scenes have strong occluders (walls), so portals or precomputed cell visibility can remove most of the world. Outdoor scenes have few occluders, so distance-based culling and LOD (below) usually matter more than occlusion tests. This is the review's suggestion (coffeepot4.md section 7), not a source-measured finding; no source fetched for this page contains a 3DS number for either.

## Front-to-back ordering and the depth test

The claim in circulation is that drawing opaque objects front to back lets the depth test reject later, hidden pixels before they are shaded, saving fill. Here is what the sources the wiki has actually say.

| Question | What the sources say | Label |
| -------- | -------------------- | ----- |
| Does the PICA200 have an early depth test? | 3dbrew lists `GPUREG_EARLYDEPTH_FUNC` (0x061, compare function), `GPUREG_EARLYDEPTH_TEST1` (0x062, enable), `GPUREG_EARLYDEPTH_CLEAR` (0x063, trigger a clear of the early depth data), `GPUREG_EARLYDEPTH_DATA` (0x06A, 24-bit clear value) and `GPUREG_EARLYDEPTH_TEST2` (0x118, enable) | `[reverse-engineered: 3dbrew GPU/Internal Registers]`, see [GPU internal registers](../../hardware/gpu-internal-registers.md) |
| Does citro3d use it? | `C3D_EarlyDepthTest(enable, function, ref)` exists, and citro3d's state initialisation calls `C3D_EarlyDepthTest(false, GPU_EARLYDEPTH_GREATER, 0)` (`base.c` line 107), so it is **off by default** | `[documented: citro3d effect.c, base.c]` |
| Does citro3d clear it? | It writes `GPUREG_EARLYDEPTH_CLEAR` when a frame is split or the framebuffer changes after a draw | `[documented: citro3d base.c]` |
| What does the early test reject, and does ordering change fill cost? | The 3dbrew pages only say what the registers configure ("configures the early depth test function", "sets whether the early depth test is enabled"). They give no statement of when pixels are rejected, at what granularity, or whether it is a hierarchical test | Not stated |
| Is there a measured benefit from front-to-back drawing on the 3DS? | None found in the sources available to this wiki | Not stated |

So: that the PICA200 has early-depth registers is documented; that sorting opaque draws front to back reduces shaded pixels on a 3DS is `[hypothesis]`. What would confirm it: draw a full-screen overdraw-heavy scene (for example 8 stacked opaque quads with a heavy TEV setup) back to front and front to back, with the normal depth test and with `C3D_EarlyDepthTest(true, ...)`, recording GPU time on an Old 3DS. What would refute it: no difference between the orders.

> Unknown: the early-depth data is a separate buffer with its own clear value, and the 3dbrew text does not say its resolution or whether it must be cleared every frame for correctness. Do not enable it without a test that confirms visuals are unchanged: citro3d clears it when it splits a frame, which suggests stale data is a hazard `[hypothesis]`.

The ordering rule that is safe regardless: opaque in any order with the depth test on, then alpha-blended draws sorted back to front with depth writes as needed. The hardware cannot do per-pixel order-independent transparency, and blending needs the right order for the picture to be correct, whatever the performance effect ([GPU rendering](gpu-rendering.md)).

Sorting opaque objects by material first (to batch) and by distance second is a choice between lower state-change cost and lower fill. Without a measurement, batch first `[hypothesis]`.

## Level of detail and animation budgets

| Technique | Idea | Notes |
| --------- | ---- | ----- |
| Mesh LOD | Two or three versions of a model switched by distance | Storage is the sum of actual LOD resources plus overhead; sharing textures/materials changes it. A 100% + 50% + 20% geometry example totals 170%, not 300%. Authoring effort and transition visibility vary. |
| Imposters (a flat textured quad in place of a distant model) | One quad instead of hundreds of triangles | Works on a handheld screen, where distant objects are a few pixels |
| Skeleton LOD | Fewer bones or no skinning when far | Skinning is vertex work; whether it is on the CPU or in a shader matters |
| Animation rate reduction | Update distant characters every 2nd or 4th frame, or use precomputed poses | The review lists this; the wiki has no measurement |
| Instance budget | A fixed cap of animated characters per frame | Set from a measured per-character cost, not a guess |

Where the cost lands depends on where skinning happens. The PICA200 vertex shader has 96 float uniform registers `c0-c95` ([shader programming](pica200-shader-programming.md)), which caps how many bone matrices a single draw can carry `[hypothesis]` on the exact limit for a skinning design; check that page before choosing a skeleton layout. CPU skinning keeps the shader simple but spends Old 3DS CPU time; the choice is a measurement.

A frame budget works better than a rule per technique: decide the milliseconds you can spend on animation, culling and submission at 60 or 30 fps ([frame pacing](frame-pacing.md)) and make LOD thresholds adapt to stay inside it. No budget numbers are given here because none are measured.

## Baked lighting, shadows and vertex colours

Lighting a scene dynamically costs fragment-lighting work and, for each light, extra setup ([citro3d fixed-function API](../citro3d-fixed-function-api.md), [GPU shading features](../../hardware/gpu-shading-features.md)). Baking moves it offline.

| Technique | How | Costs |
| --------- | --- | ----- |
| Vertex-colour lighting | Store pre-lit colour per vertex; the [TEV](../../glossary.md#tev) multiplies it with the texture | Needs a vertex attribute (4 bytes in 8-bit form); resolution limited by vertex density |
| Lightmaps | A second texture multiplied with the base texture, using a second UV set | Extra texture unit and texture memory; the PICA200 has three texture units ([GPU](../../hardware/gpu.md)), so one can carry the lightmap |
| Baked ambient occlusion | Bake into the lightmap or vertex colours for static geometry | Static only |
| Blob or projected shadow | A translucent dark quad under the character | One more draw; sort with other translucent draws |
| Painted highlights and fake reflections | Authored into the texture or an environment texture | Art time; no runtime cost beyond the texture |

Whether baked lighting is faster on the 3DS than the fragment-lighting hardware was not measured; the point of the table is that baked work costs no per-frame CPU time, and that is `[documented]` by construction (nothing is computed). Resident Evil: Revelations and Shovel Knight are cited in the review as examples of asset design lowering run-time cost; the Shovel Knight remark (replacing complex 3D backgrounds with flat images because of VRAM) and Capcom's effects list are secondary claims there and are not repeated here as facts; see the commercial case studies for any that were fetched.

## 2.5D: 2D character quads in a 3D scene

A flat character quad can be placed in a projected 3D scene (terrain and buildings in 3D, characters as textured quads). The challenge is depth ordering with the other geometry.

- **Billboard**: orient the quad to face the camera (or only rotate around the vertical axis to keep it upright). Put its position at the character's feet so the quad's depth is meaningful.
- **Depth test and transparent texels**: with no alpha test, transparent texels of a nearer quad still write depth and can hide geometry behind them ([2D rendering](rendering-2d.md#2-screens-coordinates-and-depth) describes this for citro2d; the mechanism is from citro3d defaults). For cut-out sprites enable the alpha test (`C3D_AlphaTest`) so transparent texels write neither colour nor depth, or sort sprites back to front and disable depth writes for them `[documented mechanism: citro3d base.c defaults; effect unverified on hardware]`.
- **Sprites against 3D geometry**: a character standing behind a wall must be hidden by it. Depth testing against the wall does this correctly if the quad is drawn after opaque geometry and has real depth. A sprite whose lower edge dips into the ground is cut by the terrain unless the quad is pushed toward the camera by a small bias ([hypothesis]: tune the bias by eye).
- **Sprite layers inside 3D**: groups of 2D sprites that must keep their own order (many characters standing at different depths) can use quad depth from their ground position and rely on the depth test, avoiding a sort for opaque pixels `[hypothesis]`.
- **Stereo**: quads at different scene depths get correct parallax automatically when drawn through the stereo projection; screen-space sprites need explicit offsets ([stereo engineering](stereo-engineering.md#hud-sprite-layers-and-parallax-offsets)).

The [direct developer interview](stereo-engineering.md#nintendos-accounts-reported) reports that stereoscopic backgrounds made earlier 2D placement shortcuts unsuitable. It does not establish a particular implementation of hybrid characters and backgrounds; a separate search-summary claim remains unverified. [reported: Iwata Asks, date and model not stated; see the linked page]

### A hybrid scene: coordinate, depth and pass contracts

Choose one world convention and document it: origin, up/forward axes, handedness, world units per tile and the camera transform. Convert source positions once at the boundary, then use the same world-to-view projection for terrain and character anchors. A character's anchor is normally the feet: place the bottom of each animated frame at that anchor, and store per-frame pivot and trimmed-rectangle offsets so varying artwork dimensions do not make the feet jump. A billboard must define whether it faces the camera fully or rotates only around the vertical axis.

A practical starting order is opaque terrain and buildings, depth-tested cut-out geometry, character quads, translucent shadows/effects in a suitable order, required foreground masks or overlays, and finally screen-space UI. This order is a design to validate, not a universal rule: a character that must walk both behind and in front of a building needs suitable geometry/depth or an explicit priority split, and transparent effects can require interleaving. A fully transparent texel must not leave an unwanted depth value; choose and verify alpha-test/depth-write policy for cut-outs separately from blended effects. Stable sort ties explicitly; a depth buffer cannot automatically recreate a DS engine's window, layer and blend rules.

After the 3D scene, hand GPU state to citro2d deliberately: finish pending submissions and configure its target, shader, view, clipping, blend and depth settings through the appropriate library entry points. Conversely, restore all state needed when returning to custom citro3d draws. See the [tile renderer's hand-over](2d-tile-renderer.md#strategies-1-and-2-handing-the-gpu-state-over). Test both eye views independently if stereo is enabled; sharing simulation or visibility does not make the eye images identical. Camera snapping may help orthographic pixel art but can shimmer or distort motion under perspective or stereo; check the result on the intended display rather than rounding every world coordinate. This wiki does not yet supply a complete, hardware-verified hybrid scene.

For a large static town, choose spatial cells, object or chunk bounds and an explicit visibility representation before merging meshes. Per-object culling preserves granularity; chunking reduces submissions but can draw hidden members; precomputed cell visibility uses extra storage and depends on the cell/portal model. These are trade-offs, not a universal chunk-size rule. Define object IDs, cell-to-object lists, bounding volumes, transition behavior and LOD thresholds; keep a conservative visibility margin and test camera movement across cell boundaries. LOD variants must include storage, texture/material changes and a policy for avoiding visible rapid switches [hypothesis until validated in the target scene].

## Decision table

| Situation | First thing to try | Why | Evidence |
| --------- | ------------------ | --- | -------- |
| CPU time high in submit, many small objects | Cull in coarse groups; merge static meshes | Reduces draw calls | `[hypothesis]`; confirm with draw-count versus frame-time test |
| GPU time high, overdraw heavy | Reduce overdraw first (fewer layers, smaller quads); then test front-to-back with early depth | Fill is the limit, not geometry | `[hypothesis]` |
| Indoor game | Cells and portals or precomputed visibility | Strong occluders | `[hypothesis]`, review |
| Large outdoor map | Distance cut, fog, LOD, imposters | Few occluders | `[hypothesis]`, review |
| Many characters | Animation rate and skeleton LOD, instance cap | Animation cost scales with count | `[hypothesis]` |
| Static scene lighting | Vertex colours and lightmaps | No per-frame work | `[documented]` mechanism |
| 2D characters in 3D | Billboard quads with alpha test | Correct depth, cheap | `[documented]` mechanism |
| Stereo on | Cull once, draw twice; lower density | Second eye cost | See [stereo engineering](stereo-engineering.md) |

## Experiments to run later

None of these has been run. Each uses an Old 3DS, a fixed scene, 60 s warm-up and median / p99 frame time from [performance](../performance.md).

1. **Draw-count scaling**: the same triangles in 1, 10, 100, 1000 draws, to find the CPU cost per draw (feeds the culling granularity).
2. **Front-to-back versus back-to-front** on a stacked-opaque-quad scene, early depth off and on (see above).
3. **Cull cost**: sphere test per object for 100, 1000, 5000 objects, to find where per-frame culling costs more than drawing.
4. **LOD switch**: a model at 3 LOD levels against a single level, with the camera path fixed, to find the switch distance where cost and appearance are both acceptable.
5. **Vertex-colour versus lightmap versus fragment lighting** on the same static room: CPU time, GPU time and VRAM.
6. **Billboard depth**: quads with and without alpha test crossing geometry; visual correctness and cost.

## What 3dbrew's Nintendo OpenGL page says

The 3dbrew page [Nintendo OpenGL](https://www.3dbrew.org/wiki/Nintendo_OpenGL) was reachable and was read on 2026-11-11 (page revision 13154; the local 3dbrew mirror does not contain it). It is a reverse-engineering work in progress, not an optimisation guide. What it states:

- The PICA200 is described as an OpenGL ES 1.1 compliant GPU with extensions that make it resemble OpenGL ES 2.0, and Nintendo wrapped it in an OpenGL ES 2.0 implementation "with some limitations". Because the fragment stage is not programmable, developers configure fragment values through `glGetUniformLocation` and `glUniformXX`.
- It lists the uniform names used for this (for example `dmp_Texture[n]...`, `dmp_FragmentLightSource[n]...`, `dmp_Point...`) with the handler addresses found in the free eShop game Steel Diver: Sub Wars, a partial structure of the shader-program object (including a table of 8 light-source structs) and a methodology for tracing each uniform to the GPU register it writes.
- It says explicitly that it is not exhaustive and that the Steel Diver observations are specific to that game.

Nothing on that page states a performance result, an ordering rule or a culling technique, so none of it supports a claim above. Its value here is that retail games reached the hardware through fixed-function uniforms (lights, texture units, point sprites), which supports the baked-versus-fragment-lighting trade as a real design choice rather than a performance fact. This paragraph is the page's own conclusion.

## Technical details

| Item | Value | Source |
| ---- | ----- | ------ |
| Early depth registers | 0x061 func, 0x062 enable, 0x063 clear, 0x06A data, 0x118 enable (second) | `[reverse-engineered: 3dbrew]`, [GPU internal registers](../../hardware/gpu-internal-registers.md) |
| citro3d early depth default | disabled, function `GPU_EARLYDEPTH_GREATER`, ref 0 | `[documented: citro3d base.c]` |
| citro3d clears early depth | on frame split / framebuffer change after a draw | `[documented: citro3d base.c]` |
| Alpha test default | off | `[documented: citro3d base.c]` |

## Pitfalls

- **Enabling early depth test without checking images**: stale early-depth data can reject pixels incorrectly; compare against a run with it off.
- **Culling bounds that are too small**: objects pop at screen edges; add a margin for animation and for both stereo eyes.
- **Per-frame allocation in the visible list**: allocate once ([memory and cache](../memory-and-cache.md#buffer-lifetime)).
- **Transparent sprite edges writing depth**: use alpha test or sort.
- **LOD popping**: switch distance too close; hide with fog or crossfade.

## Open questions

- Does front-to-back ordering reduce GPU time on the Old 3DS, with and without the early depth test? (Experiment 2.)
- What is the early depth data's resolution, and what does `GPUREG_EARLYDEPTH_TEST2` do beyond `TEST1`? A register experiment on hardware would settle it.
- Where does CPU culling cost exceed its saving?
- How many bone matrices fit a skinned draw, and is shader skinning faster than CPU skinning on Old 3DS?
- **New 3DS:** do any of these thresholds change with the faster CPU and larger L2 cache ([New 3DS](../new-3ds-qtm-and-mvd.md))? Not measured.

## Sources

- [3dbrew: GPU/Internal Registers](https://www.3dbrew.org/wiki/GPU/Internal_Registers) — early depth registers (local mirror in `private_please_gitignore/3dbrew`).
- [3dbrew: Nintendo OpenGL](https://www.3dbrew.org/wiki/Nintendo_OpenGL) — retail OpenGL wrapper notes, fetched 2026-11-11.
- [citro3d](https://github.com/devkitPro/citro3d) — `source/effect.c`, `source/base.c`.
- coffeepot4.md (external review, private) — section 7; treated as a secondary source.
