# GPU rendering with citro3d: a practical guide

This page is a cookbook for drawing with the 3DS's **[PICA200](../glossary.md#pica200)** GPU through **citro3d**, the standard homebrew GPU library: how a frame is put together, what each draw call really costs, how to batch, how to set up textures and the colour combiners, how to handle the stereoscopic and wide screen modes, and what the hardware simply cannot do. It assumes you have built one of the `3ds-examples` GPU samples.

Background lives elsewhere: the pipeline and registers on [GPU](../hardware/gpu.md), lighting/procedural textures on [GPU shading features](../hardware/gpu-shading-features.md), the shader assembler and library overview on [picasso and citro3d](../homebrew/picasso-and-citro3d.md), and buffer placement and flushing on [memory and cache](memory-and-cache.md).

## Before you start

- Checked against citro3d 1.7.1, citro2d 1.7.0, libctru 2.7.1, picasso 2.7.2, tex3ds 2.3.0 and the `3ds-examples` repository (January 2026). Code fragments use real API names from those versions; they are fragments, not complete programs, and have not been timed on hardware.

## Anatomy of a frame

The minimal program (`graphics/gpu/simple_tri`) shows the order of operations:

```c
gfxInitDefault();                                  // LCD framebuffers (linear memory)
C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);                 // command buffer, 0x40000 bytes

// Render target: note 240×400 — the top LCD is a portrait panel turned sideways.
C3D_RenderTarget *top = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
C3D_RenderTargetSetOutput(top, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);
// DISPLAY_TRANSFER_FLAGS in the examples: RGBA8 in, RGB8 out, no flip, not tiled, no scaling

/* once: load shader (DVLB_ParseFile on the .shbin), shaderProgramInit/SetVsh, C3D_BindProgram,
   describe vertex attributes (C3D_GetAttrInfo / AttrInfo_AddLoader),
   point at vertex buffers (C3D_GetBufInfo / BufInfo_Add), configure combiners (C3D_GetTexEnv) */

while (aptMainLoop()) {
    hidScanInput();
    update();                                       // outside the frame: overlaps with GPU work
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C3D_RenderTargetClear(top, C3D_CLEAR_ALL, 0x68B0D8FF, 0);
        C3D_FrameDrawOn(top);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);
        C3D_DrawArrays(GPU_TRIANGLES, 0, vertex_count);
    C3D_FrameEnd(0);
}
C3D_Fini();
gfxExit();
```

What happens underneath [documented: citro3d `renderqueue.c`, `base.c`, `framebuffer.c`]:

1. `C3D_FrameBegin` waits for the previous frame's GPU work, then starts recording into the command buffer.
2. `C3D_RenderTargetClear` queues a hardware memory fill (`GX_MemoryFill`) of the colour and depth buffers.
3. Each draw call writes the *changed* GPU state plus the draw command into the command buffer.
4. `C3D_FrameEnd` closes the command list, (by default) flushes the linear heap from the CPU cache, queues a display transfer from each used render target to its screen's LCD framebuffer, and starts the queue. The buffers are swapped when the queue finishes.

The render target is **tiled** (8×8 blocks, [glossary](../../glossary.md#tiling)) and in VRAM; the LCD framebuffer is linear and in FCRAM. The display transfer converts between them, can drop alpha (RGBA8 → RGB8 in the examples) and can downscale 2×1 or 2×2 for anti-aliasing (`GX_TRANSFER_SCALING`), so rendering at 480×800 and downscaling 2×2 gives supersampled anti-aliasing at four times the fill cost.

For projection matrices, citro3d provides "tilt" variants (`Mtx_OrthoTilt`, `Mtx_PerspTilt`, `Mtx_PerspStereoTilt`) that include the 90° rotation, so you can think in landscape 400×240 coordinates.

## What a draw call costs

A citro3d draw is not free even if the GPU is idle. Calling `C3D_DrawArrays`, `C3D_DrawElements` or `C3D_ImmDrawBegin` first runs `C3Di_UpdateContext`, which writes every piece of state marked dirty since the last draw into the command buffer [documented: citro3d `base.c`]. Costs that follow:

| Action | What citro3d emits | Advice |
| ------ | ------------------ | ------ |
| `C3D_TexBind(unit, tex)` | Marks the unit dirty **even when `tex` is already bound**; the next draw rewrites the unit's texture registers *and requests a texture-cache clear* | Keep your own "currently bound" record and skip redundant binds; sort draws by texture |
| `C3D_FrameDrawOn` to a different target after drawing | A framebuffer flush and early-depth clear, then the new target's setup | Draw everything for one target before switching |
| Changing the shader program (`C3D_BindProgram`) | Reconfigures the shader units, attribute and buffer setup | Group draws by shader |
| Uniform updates | Only dirty uniform ranges are uploaded, at draw time | Upload a matrix once per object, not per triangle |
| Combiner (`C3D_TexEnv*`), depth/blend/alpha (`C3D_DepthTest`, `C3D_AlphaBlend`, `C3D_AlphaTest`, …) | Only the changed stages/state | Cheap per change; still group by material |
| The draw itself | Primitive config, vertex count, offset, kick | Fewer, larger draws |

The practical rule: **one draw per material, not per object**. Put many objects that share a texture, shader and combiner setup into the same vertex buffer and draw them with one call. The CPU cost of a thousand tiny draws is easily larger than the GPU cost of drawing the triangles [hypothesis — the crossover point has not been measured; use the [performance](performance.md#a-frame-level-breakdown) breakdown to see where your frame goes].

### Vertex data

- Up to **12 attributes** per vertex (`AttrInfo_AddLoader` returns -1 beyond that) and up to 12 buffer configurations (`C3D_BufInfo`) [documented: `attribs.c`, `buffers.h`].
- Attribute component types are `GPU_BYTE`, `GPU_UNSIGNED_BYTE`, `GPU_SHORT` and `GPU_FLOAT`. Bytes and shorts arrive in the shader as numbers you scale with a uniform: a 16-bit position plus a scale/offset uniform, a byte normal and byte colours can make a vertex a third the size of an all-float one. Fewer bytes means less CPU writing (for dynamic data) and less memory traffic for the GPU.
- The buffer stride must exactly equal the real vertex size, including padding — otherwise the GPU hangs ([GPU pitfalls](../hardware/gpu.md#known-pitfalls)).
- **Fixed attributes** (`AttrInfo_AddFixed` + `C3D_FixedAttribSet`) supply a value that is constant for a draw (a tint colour, a normal) without storing it per vertex.
- **Indexed drawing** (`C3D_DrawElements` with 8- or 16-bit indices in linear memory) avoids transforming shared vertices twice and shrinks meshes; it does not reduce the per-draw cost above.
- **Immediate mode** (`C3D_ImmDrawBegin`, `C3D_ImmSendAttrib`, `C3D_ImmDrawEnd`) writes vertices into the command buffer itself — fine for a few debug quads, wasteful for real geometry because every attribute costs a command word on the CPU.
- Static meshes: build once into linear memory at load time. Dynamic geometry: write it after `C3D_FrameBegin` returns, or double-buffer it ([memory and cache](memory-and-cache.md#buffer-lifetime)).

### Do work in the vertex shader

The vertex shader units do 4-wide math on 24-bit floats in parallel with the CPU. Transforming vertices on the CPU and uploading screen positions every frame wastes the 268 MHz CPU on what the GPU does for free; upload object-space vertices once and pass the matrix as a uniform. A geometry shader can expand one point into a quad (sprites, particles — see `graphics/gpu/geoshader`, `particles`, `gpusprites`), trading vertex-buffer size for shader time.

## Textures

- Dimensions are powers of two from 8 to 1024 per side ([picasso and citro3d](../homebrew/picasso-and-citro3d.md#citro3d-gpu-library)); data is stored **tiled** in 8×8 blocks, not in rows — use `tex3ds` to convert at build time ([asset pipeline](asset-pipeline.md)).
- Load `.t3x` files with `Tex3DS_TextureImport` (from memory), `Tex3DS_TextureImportStdio` or `Tex3DS_TextureImportFD` (from a file); the last argument chooses VRAM instead of linear memory. For atlases, `Tex3DS_GetSubTexture` returns per-image coordinates.
- Pick the smallest format that looks right: RGB565 for opaque art, RGBA5551 for cut-out sprites, RGBA4 for soft alpha, LA8/L8/A8/LA4/L4/A4 for masks, fonts and lightmaps, **ETC1/ETC1A4** (compressed, 4 or 8 bits per pixel) for photographic and large textures. RGBA8 is four times the memory and bandwidth of a 4-bit ETC1 texture. ETC1 is lossy: not for pixel art.
- Mipmaps (`tex3ds -m`, `C3D_TexInitMipmap`) cost a third more memory but reduce texture-cache misses for minified textures.
- Three units sample ordinary textures (units 0–2); unit 0 also does cube, shadow and projection textures; unit 3 is procedural ([GPU](../hardware/gpu.md#summary)).

## Colour combiners (TEV) recipes

The fragment stage has six combiner stages ([glossary](../glossary.md#tev)). Each takes up to three sources (`GPU_PRIMARY_COLOR` = interpolated vertex colour, `GPU_TEXTURE0..3`, `GPU_CONSTANT`, `GPU_PREVIOUS`, `GPU_PREVIOUS_BUFFER`, fragment-lighting outputs), applies operand selectors and one function, for RGB and alpha separately [documented: libctru `gpu/enums.h`, citro3d `texenv.h`].

```c
C3D_TexEnv *env = C3D_GetTexEnv(0);
C3D_TexEnvInit(env);

/* Textured and tinted by vertex colour: tex0 × primary */
C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);

/* Flat colour from a constant (e.g. a fade overlay): */
C3D_TexEnvSrc(env, C3D_Both, GPU_CONSTANT, 0, 0);
C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
C3D_TexEnvColor(env, 0x80000000);      // ABGR: half-transparent black

/* Blend two textures by the second texture's alpha: */
C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE1, GPU_TEXTURE0, GPU_TEXTURE1);
C3D_TexEnvOpAlpha(env, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA);
C3D_TexEnvFunc(env, C3D_RGB, GPU_INTERPOLATE);   // src0 * src2 + src1 * (1 - src2)
```

Unused stages must pass the previous result through; `C3D_TexEnvInit` sets a stage to "replace with previous". Other functions include `GPU_ADD`, `GPU_ADD_SIGNED`, `GPU_SUBTRACT`, `GPU_DOT3_RGB`/`GPU_DOT3_RGBA`, `GPU_MULTIPLY_ADD` and `GPU_ADD_MULTIPLY`. Use `C3D_AlphaTest` to discard transparent texels of cut-out sprites (cheaper than blending and needs no back-to-front sort), and `C3D_AlphaBlend` for real translucency.

## 2D games: citro2d

citro2d sits on citro3d and batches sprites, images, text and shapes: `C2D_Init(C2D_DEFAULT_MAX_OBJECTS)` (4096), `C2D_Prepare()`, `C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT)`, then per frame `C2D_SceneBegin(target)` and `C2D_Draw*` calls between `C3D_FrameBegin`/`C3D_FrameEnd`; sprite sheets come from `tex3ds` atlas files (`.t3s` → `.t3x`, loaded with `C2D_SpriteSheetLoad`) [documented: citro2d `base.h`, `3ds-examples/graphics/printing`, `graphics/gpu/gpusprites`]. citro2d collects quads in a vertex/index buffer and submits them with one `C3D_DrawElements` per run; a run ends when the texture, blend mode, fade/tint or view transform changes, or at `C2D_Flush`, `C2D_TargetClear` and frame end [documented: citro2d `source/base.c`]. So **put sprites that are drawn together in the same atlas** and draw them consecutively. The buffer holds the `maxObjects` given to `C2D_Init` for the whole frame; once it is full, further draw calls return `false` and draw nothing — size it for your worst frame. You can mix citro2d and your own citro3d drawing on the same target (`composite_scene` example); call `C2D_Prepare()` again after changing citro3d state yourself.

## Stereoscopic 3D and wide mode

- Enable with `gfxSet3D(true)`, create a second top render target for `GFX_RIGHT`, and render the scene once per eye with opposite camera offsets (`Mtx_PerspStereoTilt` takes the interocular distance). Read the slider each frame with `osGet3DSliderState()`.
- When the slider is at zero, **skip the right eye**: the `composite_scene` example does exactly this, and citro3d then presents the top screen in 2D (it only swaps in stereo when a right-eye target was used that frame) [documented: example, `renderqueue.c`]. That halves top-screen GPU work for players who play in 2D.
- Stereo doubles the top screen's fill cost and VRAM for render targets. Budget it from the start.
- **Wide mode** (`gfxSetWide(true)`) drives the top LCD at 800×240 with non-square pixels — sharper 2D, at twice the fill. It is mutually exclusive with stereo and **does not work on the original 2DS** [documented: libctru `gfx.h`]. See the `wide_mode_3d` example.

## What the PICA200 cannot do

Plan around these instead of discovering them late:

- **No programmable fragment shaders.** Per-pixel effects are built from the six combiner stages, lighting LUTs, fog and procedural textures ([shading features](../hardware/gpu-shading-features.md)). A desktop pixel shader must be redesigned, not ported.
- **No palette lookup in hardware.** A texel cannot index another texture. Expand palettised art to direct-colour texture formats when the art or the palette changes, and cache the result.
- **No instancing, no compute.** Batch geometry yourself; use the geometry shader for simple expansion.
- **Small VRAM.** 6 MB holds render targets and a few textures; most textures live in linear FCRAM.
- **24-bit floats in shaders.** Large world coordinates lose precision; keep coordinates relative to the camera or object.

## Render to texture

`C3D_RenderTargetCreateFromTex(&tex, GPU_TEXFACE_2D, 0, depthFmt)` makes a texture a render target, for shadows, reflections, post-processing or a 2D layer you redraw only when it changes. A static background or UI that is redrawn into a texture once and then drawn as one quad per frame saves both CPU and GPU time [documented API; benefit depends on the scene]. Do not sample a texture in the same pass that renders into it.

## Technical details

- `C3D_DEFAULT_CMDBUF_SIZE` = `0x40000` bytes (256 KiB) in linear memory; pass a larger size to `C3D_Init` for heavy frames.
- Render target creation: colour buffer from `vramAlloc`; depth buffer from the opposite VRAM bank if possible, else the same bank.
- `C3D_FrameEnd` presents targets in index order 2, 1, 0 (bottom, top-right, top-left) and only those drawn to during the frame.
- Texture unit configuration writes set bit 16 of `GPUREG_TEXUNIT_CONFIG` to clear the texture cache whenever any texture binding was marked dirty.
- `C3D_RenderTargetClear` → `C3D_FrameBufClear` → `GX_MemoryFill` on colour and depth buffers (the PSC0/PSC1 fill units).

## Pitfalls

- **Forgetting the 240×400 orientation** or using non-tilt matrices: the picture appears rotated.
- **Clearing with the wrong colour format**: the clear colour is packed as RGBA (`0xRRGGBBAA`) for an RGBA8 target.
- **Binding the same texture before every draw** (common in naïve sprite loops): extra state writes and texture-cache clears every draw.
- **Alpha blending without sorting**: translucent geometry must be drawn back to front after the opaque geometry.
- **Assuming an emulator's tolerance**: a configuration that renders in an emulator can still hang a real GPU ([debugging](debugging.md#gpu-hangs)).

## Open questions

- Draw-call CPU cost in citro3d per state change on Old and New 3DS — [`drawbench`](../benchmarks/README.md#drawbench) measures exactly this; no results yet.
- Fill-rate figures for RGBA8 vs RGB565 targets and for each texture format, with and without the texture cache warm.
- Whether placing static textures in VRAM gives a measurable speed-up over linear FCRAM for typical scenes.

## Sources

- [citro3d](https://github.com/devkitPro/citro3d) — `source/base.c` (`C3Di_UpdateContext`), `source/renderqueue.c`, `source/texture.c` (`C3D_TexBind`), `source/attribs.c`, `include/c3d/*.h`.
- [citro2d](https://github.com/devkitPro/citro2d) — `include/c2d/base.h` (init, scene API), `source/base.c` (batch flushing, buffer-full behaviour).
- [libctru](https://github.com/devkitPro/libctru) — `gfx.h` (stereo, wide mode restrictions), `gpu/enums.h` (attribute formats, combiner sources and functions).
- [3ds-examples](https://github.com/devkitPro/3ds-examples) — `graphics/gpu/simple_tri`, `textured_cube`, `composite_scene` (stereo, slider, citro2d mixing), `wide_mode_3d`, `gpusprites`, `geoshader`, `particles`.
- [3dbrew: GPU](https://www.3dbrew.org/wiki/GPU) — hardware limits referenced from the GPU page.
