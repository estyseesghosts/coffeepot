# 2D rendering on the PICA200

The 3DS has no 2D graphics hardware of its own for native software: unlike the DS, there are no tile backgrounds, sprite engine or palettes. Everything on screen — tilemaps, sprites, text, UI — is drawn by the 3D GPU (the **[PICA200](../glossary.md#pica200)**) as textured rectangles (**quads**: two triangles making a rectangle), using **[tiled](../../glossary.md#tiling)** textures and the **[TEV](../glossary.md#tev)** colour combiners for effects. Done well, that is fast and flexible: the GPU scales, rotates, tints and blends sprites for free. Done naïvely — one draw call per sprite, a texture change per tile, or a CPU-drawn framebuffer uploaded every frame — it wastes most of the frame.

This page is the end-to-end guide for a developer new to the 3DS: setting the libraries up, the frame loop, screens and coordinates, textures and memory, what citro2d offers beyond sprites, a complete compile-checked sprite program, performance decisions, and a troubleshooting table. A second, larger worked example (a scrolling tile renderer with four tile-submission strategies and a scripted experiment mode) is on [2D tile renderer](2d-tile-renderer.md); the choice between techniques, and the experiments that would settle it, are in [Design alternatives and how to choose](#design-alternatives-and-how-to-choose) and the [validation checklist](#validation-checklist-for-a-2d-engine). Palette-based originals (DS-style ports) are covered near the end.

Background pages: [GPU rendering](gpu-rendering.md) (draw-call costs, combiners, citro2d batching), [asset pipeline](asset-pipeline.md) (atlases and formats), [memory and cache](memory-and-cache.md) (where buffers live, when to flush).

## The whole thing in 10 lines

1. Start `gfxInitDefault()`, `C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)`, `C2D_Init(C2D_DEFAULT_MAX_OBJECTS)`, `C2D_Prepare()`; make one `C2D_CreateScreenTarget` per screen.
2. Every frame: read input, update the game, then `C3D_FrameBegin(C3D_FRAME_SYNCDRAW)`, `C2D_TargetClear` + `C2D_SceneBegin` per screen, draw, `C3D_FrameEnd(0)`.
3. Coordinates are pixels, origin top-left, y down: top screen 400×240, bottom 320×240. The panels are physically rotated, citro2d hides that.
4. Every texture is power-of-two (8..1024 per side), **tiled** (8×8 blocks in Morton order). Use `tex3ds` to make them; generate them in code only with a correct swizzle.
5. Put everything drawn together in one atlas, and draw it consecutively: citro2d batches runs of the same texture and state.
6. Things that end a batch: a different texture, text, an untextured shape after a textured one, tint/fade/view changes, a screen switch.
7. `C2D_Init(maxObjects)` caps the quads per frame; draw calls return `false` when it is full. Check the result in debug builds.
8. Only draw what is visible; snap pixel art to whole pixels; use nearest filtering.
9. Static layers: render once into a texture and draw one quad per frame (with caveats, below).
10. Measure with ticks around your own phases and read `C3D_FrameEnd`'s cost separately; never trust emulator timings.

## Before you start

- Checked against citro2d 1.7.0 (installed headers and library), citro3d 1.7.1 and libctru 2.7.1 (cloned sources), devkitARM r68 (GCC 16.1.0), tex3ds 2.3.0 (see [source versions](source-versions.md)). Old 3DS is the default target; New 3DS differences are in **New 3DS:** notes.
- citro2d's source (`source/base.c`) is **not cloned** in this repository. Where this page says what citro2d does internally, the evidence is one of: the installed 1.7.0 headers `[documented]`; a disassembly of `base.o` inside the installed `libcitro2d.a` (`arm-none-eabi-objdump`) `[reverse-engineered]` — these are the wiki author's readings of compiled code and name the function; or upstream `master` `base.c` as cited by earlier pages `[documented, not re-verified locally]`.
- All code on this page and on [2D tile renderer](2d-tile-renderer.md) **builds; not run on Old 3DS, New 3DS or an emulator** (details in [Verification status](#verification-status)). Expect to debug the first run: orientation and colour conventions below were derived from tool output and library sources, not seen on a screen.
- Jargon: a **draw call** is one request to the GPU to draw a list of triangles; a **batch** is a run of quads that citro2d submits in one draw call; an **atlas** (sprite sheet) is one texture holding many images; a **render target** is a buffer the GPU draws into.

## 1. Setup and the frame loop

### Starting up

```c
gfxInitDefault();                           // libctru: LCD framebuffers in linear memory
C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);          // citro3d: GPU command buffer, 0x40000 bytes, linear memory
if (!C2D_Init(C2D_DEFAULT_MAX_OBJECTS)) { /* out of linear memory */ }   // 4096 objects
C2D_Prepare();                              // bind citro2d's shader and 2D render state

C3D_RenderTarget *top    = C2D_CreateScreenTarget(GFX_TOP,    GFX_LEFT);
C3D_RenderTarget *bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
```

What each call does `[documented: citro2d base.h; citro3d base.c]`:

| Call | Effect | Cost / ownership |
| ---- | ------ | ---------------- |
| `gfxInitDefault()` | Allocates the LCD framebuffers (linear memory) and starts the GSP service connection ([GSP](../glossary.md#gsp)) | Pair with `gfxExit()` |
| `C3D_Init(size)` | Allocates the command buffer with `linearAlloc`, sets citro3d's default state (depth test on, alpha blending on) | Pair with `C3D_Fini()` |
| `C2D_Init(maxObjects)` | Allocates citro2d's vertex and index buffers in linear memory and loads its built-in vertex shader | Pair with `C2D_Fini()`. Returns `false` on failure |
| `C2D_Prepare()` | Binds citro2d's shader, vertex layout and combiner stages | Call again after you change citro3d state yourself |
| `C2D_CreateScreenTarget(screen, side)` | Creates a render target and links it to a screen | Owned by citro3d; see teardown |

`C2D_CreateScreenTarget` is a helper. In the installed 1.7.0 library it creates a target 240 pixels wide and 400 tall for the top screen (800 if `gfxIsWide()`), 240×320 for the bottom, with an RGBA8 colour buffer and a **16-bit depth buffer** (no stencil), and links it with the transfer flags `0x1000` (RGBA8 in, RGB8 out, no flip, not tiled, no scaling) `[reverse-engineered: disassembly of C2D_CreateScreenTarget in libcitro2d.a 1.7.0]`. Note the 240-wide: the physical panel is rotated, see [section 2](#2-screens-coordinates-and-depth).

The manual equivalent, for a custom depth format or stereo, is what the `3ds-examples` GPU samples do `[documented: graphics/gpu/composite_scene]`:

```c
#define DISPLAY_TRANSFER_FLAGS \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	 GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
	 GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

C3D_RenderTarget *left  = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
C3D_RenderTarget *right = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
C3D_RenderTargetSetOutput(left,  GFX_TOP, GFX_LEFT,  DISPLAY_TRANSFER_FLAGS);
C3D_RenderTargetSetOutput(right, GFX_TOP, GFX_RIGHT, DISPLAY_TRANSFER_FLAGS);   // only with gfxSet3D(true)
```

`C3D_RenderTargetSetOutput` decides which screen (and eye) the target is copied to when the frame ends. A target that is not linked is a pure off-screen target (render-to-texture, [section 4](#4-citro2d-beyond-sprites)).

### The frame loop

```c
while (aptMainLoop()) {
	hidScanInput();                                   // input + game update first: overlaps with the GPU
	update();

	C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		C2D_TargetClear(top, C2D_Color32(0, 0, 0, 255));
		C2D_SceneBegin(top);
		/* ... C2D_Draw* calls for the top screen ... */

		C2D_TargetClear(bottom, C2D_Color32(0, 0, 0, 255));
		C2D_SceneBegin(bottom);
		/* ... C2D_Draw* calls for the bottom screen ... */
	C3D_FrameEnd(0);
}
```

What the calls do `[documented: citro3d renderqueue.c, citro2d base.h]`:

| Call | What it does |
| ---- | ------------ |
| `C3D_FrameBegin(flags)` | Returns `false` if already inside a frame. With `C3D_FRAME_SYNCDRAW` it first calls `C3D_FrameSync()` (waits for a vertical blank event, [vblank](../../glossary.md#vblank)), then waits for the previous frame's GPU work to finish. With `C3D_FRAME_NONBLOCK` it returns `false` instead of waiting when the GPU is busy |
| `C2D_TargetClear(target, color)` | Submits anything already batched, splits the command list, and clears colour and depth of that target [reverse-engineered: disassembly of C2D_TargetClear, which calls C3D_FrameSplit(0) and then C3D_FrameBufClear with the colour and depth bits] |
| `C2D_SceneBegin(target)` | `C2D_Flush()`, `C3D_FrameDrawOn(target)`, then sets the 2D projection to the target's size. Marks the target as drawn this frame |
| `C2D_Draw*` | Append quads to citro2d's buffer; nothing reaches the GPU until the batch is flushed |
| `C3D_FrameEnd(flags)` | Flushes citro2d's last batch (it registers a frame-end hook), closes the command list, flushes the CPU cache over the whole linear heap unless `GX_CMDLIST_FLUSH` is passed, queues a display transfer for each target that was drawn, and starts the GPU `[documented: renderqueue.c]` |

Flags: `C3D_FRAME_SYNCDRAW` (bit 0) pins your loop to the display refresh; without it `C3D_FrameBegin` only waits for the previous frame's GPU work, so a light scene can run faster than 60 frames per second and a loop with its own timing ([frame pacing](frame-pacing.md)) controls the rate. `C3D_FRAME_NONBLOCK` (bit 1) lets you skip a frame instead of stalling. `C3D_FrameEnd(0)` is the safe default; `GX_CMDLIST_FLUSH` is an optimisation with strings attached ([the FrameEnd flush](memory-and-cache.md#the-c3d_frameend-flush)).

A screen is only swapped if its target was drawn during the frame, so a screen you do not touch keeps its last image `[documented: renderqueue.c]`. Both `C2D_SceneBegin` calls above count as drawing; skip them for a screen that did not change.

### Teardown, in this order

```c
/* free your own resources first: sprite sheets, textures, text buffers */
C2D_SpriteSheetFree(sheet);          // or C3D_TexDelete(&tex) for textures you created
C2D_TextBufDelete(buf);
C2D_Fini();                          // frees citro2d's buffers and shader
C3D_Fini();                          // frees the command buffer AND any render targets still alive
gfxExit();
```

Ownership rules `[documented: citro3d renderqueue.c, texture.c]`:

- `C3D_Fini` destroys every render target that still exists (it walks its list), so do not call `C3D_RenderTargetDelete` on the screen targets afterwards, and do not delete them twice.
- A render target you made from a texture (`C3D_RenderTargetCreateFromTex`) does not own the colour buffer: delete the target first, then `C3D_TexDelete` the texture. `C3D_RenderTargetDelete` panics if called inside a frame.
- `C3D_TexDelete` frees the texture memory with `vramFree` or `linearFree` as appropriate. It does not know about C2D_Image structures pointing at it; stop drawing with them first.
- `C2D_SpriteSheetLoad` allocates the texture and subtexture table; `C2D_SpriteSheetFree` releases both.

## 2. Screens, coordinates and depth

| Screen | Logical size (what you draw in) | Render target size (what you allocate) | Notes |
| ------ | ------------------------------- | -------------------------------------- | ----- |
| Top | 400×240 | 240 wide × 400 tall | Left-eye image when stereo is on |
| Top, wide mode | 800×240, non-square pixels | 240 × 800 | `gfxSetWide(true)` |
| Bottom (touch) | 320×240 | 240 × 320 | Touch coordinates use this space |

- **Pixel coordinates.** citro2d draws in pixels with the origin at the top-left of the screen, x to the right, y down, in the logical sizes above `[documented: base.h "X coordinate of the top-left vertex"; C2D_SceneSize]`. The projection is built from the target size and a tilt flag (`C2D_SceneSize(width, height, tilt)`), so you never see the rotation. Draw the same layout on top and bottom by changing only the width.
- **The rotated framebuffer.** The LCD panels are portrait screens mounted sideways; their framebuffers are 240 pixels wide and 400 or 320 tall and are scanned from left to right rather than top to bottom `[documented: libctru gfx.h; 3dbrew GPU/External Registers]`. For your code this matters in three places: (1) render-target dimensions are swapped (240×400), (2) a scissor rectangle is given in framebuffer space, not screen space, so its x and y are swapped relative to what you see (`C3D_SetScissor(mode, left, top, right, bottom)` writes the raw registers `[documented: citro3d base.c]`; the exact orientation is `> Unknown:` here — test with a full-screen rectangle), and (3) a render-to-texture target is *not* tilted, because it is not linked to a screen (`C2D_SceneTarget` passes `target->linked` as the tilt flag), so its content is upright in the texture's own space but its V orientation needs checking on the first run (see [render to texture](#render-to-texture)).
- **Touch.** `hidTouchRead` returns pixels in the 320×240 bottom-screen space; both fields are 0 when nothing touches the screen, so test `KEY_TOUCH` first ([input and sensors](input-and-sensors.md)).
- **Depth.** Every draw takes a `depth`/`z` argument. With citro2d 1.7.0: the projection is an orthographic matrix with near = 1 and far = −1, left-handed `[reverse-engineered: constants in C2D_SceneSize]`; citro3d's `Mtx_Ortho` maps that to a clip depth of −0.5·z − 0.5 `[documented: citro3d maths/mtx_ortho.c]` and citro3d's default depth map multiplies by −1 `[documented: base.c, C3D_DepthMap(true, -1.0f, 0.0f)]`, so the stored depth is 0 at z = −1, 0.5 at z = 0 and 1 at z = 1. `C2D_Prepare` sets the depth test to `GPU_GEQUAL` with all writes enabled `[reverse-engineered: disassembly of C2D_Prepare, call C3D_DepthTest(true, 7, 31)]`. Net effect by arithmetic: **larger z is in front; equal z passes, so with one shared z the draw order decides; z outside −1..1 is outside the clip range** (clipping behaviour itself `[hypothesis]`). The screen targets from `C2D_CreateScreenTarget` have a 16-bit depth buffer, so depths closer than 1/65536 of the range may tie.
- **Transparent texels still write depth.** No alpha test is on by default (`C3D_AlphaTest(false, ...)`), so a transparent texel of a nearer sprite writes depth and can hide a farther sprite drawn later `[documented mechanism: citro3d base.c defaults; effect unverified on hardware]`. For sprites with transparent edges, sort back to front with one shared z (the tile renderer sorts by y) rather than relying on z.

### Stereoscopic 2D layers

With the 3D slider up, the top screen shows a left-eye and a right-eye image ([stereoscopic 3D](../../glossary.md#stereoscopic-3d)). For 2D layers you fake depth with **parallax**: draw a layer shifted horizontally by opposite amounts in the two eyes; the bigger the shift, the farther the layer appears from the screen plane. The `stereoscopic_2d` example does it like this `[documented: graphics/gpu/stereoscopic_2d/source/main.cpp]`:

```c
gfxSet3D(true);                                   // before creating targets
C3D_RenderTarget *l = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
C3D_RenderTarget *r = C2D_CreateScreenTarget(GFX_TOP, GFX_RIGHT);
float slider = osGet3DSliderState();              // 0.0 .. 1.0, read every frame
/* per layer: its own integer 'offset' in pixels */
C2D_SceneBegin(l);  C2D_DrawImageAt(img, x + offset * slider, y, z, NULL, 1, 1);
C2D_SceneBegin(r);  C2D_DrawImageAt(img, x - offset * slider, y, z, NULL, 1, 1);
```

Scale each layer's offset by the slider, so everything collapses to a flat image at zero. Which sign pushes a layer in front of or behind the screen plane was not checked here (it needs a 3D screen); the example comments call the left-eye form "+ offset". At slider 0 you can skip the right-eye pass: citro3d only presents stereo when the right target was drawn that frame, so the top screen then runs as 2D ([GPU rendering](gpu-rendering.md#stereoscopic-3d-and-wide-mode)). Stereo doubles the top screen's draw work and render-target memory.

> **New 3DS:** the New 3DS adds head-tracked "Super Stable 3D" for the stereo image ([displays](../hardware/display.md#head-tracking-and-super-stable-3d-new-3ds)); the 2D layering above is the same.

### Wide mode

`gfxSetWide(true)` makes the top screen 800×240 with half-width (non-square) pixels, so 2D art looks sharper at twice the horizontal fill cost. It is exclusive with stereo `[documented: libctru gfx.h]`. Call it before creating the top screen target, because `C2D_CreateScreenTarget` reads `gfxIsWide()` when it creates the target `[reverse-engineered: disassembly]`, and use width 800 in your layout.

> Conflict: libctru's `gfx.h` says wide mode "does not work on Old 2DS consoles (however it does work on New 2DS XL consoles)". Other pages of this wiki used to say "the 2DS family has no wide mode". Follow libctru: the Nintendo 3DS and 3DS XL (Old 3DS) support it, the Nintendo 2DS (also Old 3DS in this wiki's terms) does not, the New Nintendo 2DS XL does. Detect the model before enabling it ([models](../hardware/models.md)). Not tested on any console here.

## 3. Textures and memory

### What the GPU accepts

| Constraint | Value | Source |
| ---------- | ----- | ------ |
| Width and height | Powers of two, 8 to 1024 each; need not be square | citro3d `texture.c` `checkTexSize` `[documented]`; `C3D_TexInit` returns `false` otherwise |
| Layout | Tiled: 8×8-pixel blocks, blocks row-major, pixels inside a block in Morton (Z) order | citro3d `texture.c` (mipmap code walks 8×8 blocks); layout of pixels inside the block checked against `tex3ds` output (below) |
| Mipmaps | Optional; each level is a quarter of the size; `C3D_TexCalcTotalSize` gives the chain size | `texture.h` `[documented]` |
| Where it lives | Linear memory ([FCRAM](../glossary.md#fcram), CPU-writable) or [VRAM](../glossary.md#vram) (GPU-local, 6 MiB shared with render targets) | `C3D_TexInit` vs `C3D_TexInitVRAM` `[documented]` |

> Conflict: 3dbrew's GPU/Internal Registers page documents the texture size fields as two 11-bit values (bits 0-10 height, 16-26 width), which could hold sizes up to 2047, and says nothing about powers of two. citro3d enforces 8..1024 and powers of two in software. Whether the hardware samples a non-power-of-two or larger texture correctly was not tested here; do not rely on it.

Bytes per pixel by format (sizes from citro3d's `fmtSize`, names from `tex3ds --help` and 3dbrew) `[documented]`:

| Format (`GPU_TEXCOLOR`) | tex3ds `-f` | Bits per pixel | Typical use for 2D |
| ----------------------- | ----------- | -------------- | ------------------ |
| `GPU_RGBA8` | `rgba8888` (default) | 32 | Soft-edged sprites, UI, anything that must be exact |
| `GPU_RGB8` | `rgb888` | 24 | Opaque images; rarely worth it over RGB565 |
| `GPU_RGBA5551` | `rgba5551` | 16 | Pixel art with 1-bit transparency |
| `GPU_RGB565` | `rgb565` | 16 | Opaque backgrounds, tilesets without transparency |
| `GPU_RGBA4` | `rgba4` | 16 | Soft edges at half the size of RGBA8, visible banding |
| `GPU_LA8` | `la88` | 16 | Greyscale + alpha |
| `GPU_HILO8` | `hilo88` | 16 | Two-channel data (normal maps) |
| `GPU_L8`, `GPU_A8` | `l8`, `a8` | 8 | Masks, glyphs |
| `GPU_LA4` | `la44` | 8 | Small greyscale + alpha |
| `GPU_L4`, `GPU_A4` | `l4`, `a4` | 4 | Tiny masks |
| `GPU_ETC1` | `etc1` | 4 | Lossy photographic art; blurs pixel edges |
| `GPU_ETC1A4` | `etc1a4` | 8 | ETC1 with a 4-bit alpha plane |

> Unknown: 3dbrew's GPU/Pitfalls page reports that binding a 4-bit-format (L4) texture located in VRAM to texture unit 1 can freeze the GPU and corrupt the display until reboot; the author could reproduce it only once and no model is stated. citro2d uses texture unit 0, so ordinary 2D code is not affected, but avoid L4/A4 textures in VRAM until someone re-tests it.

### Memory cost arithmetic

Plain arithmetic from the table above (bytes = width × height × bits per pixel ÷ 8; no mipmaps), not measurements:

| Texture | RGBA8 | 16-bit formats | 8-bit formats | ETC1 |
| ------- | ----- | -------------- | ------------- | ---- |
| 64×64 | 16 KiB | 8 KiB | 4 KiB | 2 KiB |
| 256×256 | 256 KiB | 128 KiB | 64 KiB | 32 KiB |
| 512×512 | 1 MiB | 512 KiB | 256 KiB | 128 KiB |
| 1024×1024 | 4 MiB | 2 MiB | 1 MiB | 512 KiB |

Examples: a 16×16-tile atlas of 256 tiles (256×256 px) in RGBA5551 is 128 KiB; the same art in RGBA8 is 256 KiB. A 300×200 background needs a 512×256 texture (RGB565: 256 KiB) because of the power-of-two rule, wasting 60% of it unless it shares an atlas. Mipmaps add one third. A pixel-art game rarely needs more than a few 256×256 atlases.

### VRAM or linear memory

VRAM is 6 MiB (6,291,456 bytes) and every render target lives there. With citro2d's own screen targets ([section 1](#starting-up)) the render targets cost, by arithmetic from the sizes found in the disassembly (RGBA8 colour + 16-bit depth):

| Targets | Bytes |
| ------- | ----- |
| Top 400×240 | 384,000 + 192,000 = 576,000 |
| Bottom 320×240 | 307,200 + 153,600 = 460,800 |
| Both | 1,036,800 (about 16% of VRAM) |
| Both plus a stereo right-eye top target | 1,612,800 |
| Wide-mode top 800×240 plus bottom | 1,152,000 + 460,800 = 1,612,800 |

The figures on [GPU rendering](gpu-rendering.md#render-target-memory-budget) assume the examples' 24-bit depth + 8-bit stencil buffer; citro2d's helper uses 16-bit depth, so its targets are smaller. A texture in VRAM is only worth it if it is sampled heavily and rarely changes; the CPU cannot write VRAM in homebrew, so filling it takes a GPU copy ([memory and cache](memory-and-cache.md#which-pool-for-which-buffer)). `[hypothesis]` that VRAM textures are measurably faster to sample than linear-memory textures for 2D; no measurement exists here.

The citro2d buffers themselves live in linear memory: `C2D_Init(n)` allocates `n × 128` bytes of vertices (4 vertices of 32 bytes per object) and `n × 12` bytes of indices (6 16-bit indices) `[reverse-engineered: disassembly of C2D_Init]`. For the default 4096 objects that is 524,288 + 49,152 = 573,440 bytes (560 KiB), next to the 256 KiB command buffer. Size `maxObjects` for your worst frame, not for the maximum.

### Upload and cache rules

- A texture in linear memory is written by the CPU (through the data cache) and read by the GPU (which bypasses it). After writing, **flush** it: `C3D_TexFlush(&tex)` or `GSPGPU_FlushDataCache`. `C3D_TexLoadImage`/`C3D_TexUpload` copy with `memcpy` for linear textures and do **not** flush `[documented: citro3d texture.c]`. A missing flush shows as stale or garbage texels, often intermittent. citro3d's whole-heap flush in `C3D_FrameEnd(0)` hides the mistake if you upload before the frame ends; a build that passes `GX_CMDLIST_FLUSH` does not ([memory and cache](memory-and-cache.md#texture-uploads)).
- Update dynamic textures in 8×8 tiles: a changed tile is 256 bytes in RGBA8 and 128 bytes in the 16-bit formats; flush only those ranges.
- Do not write a texture the GPU is still reading this frame; update between frames, or double-buffer.
- `C3D_TexBind` marks the unit dirty even for the texture already bound, and a rebind requests a texture-cache clear ([GPU rendering](gpu-rendering.md#what-a-draw-call-costs)); citro2d binds for you.

### Generating a texture in code

Useful for placeholder art, procedural tiles and tests. The conversion from a top-down, row-major `0xRRGGBBAA` pixel array to the tiled layout is:

```c
static const u8 kMortonSpread[8] = { 0, 1, 4, 5, 16, 17, 20, 21 };   // bits of x spread to even positions

static void swizzleRGBA8(u32 *dst, const u32 *src, u32 w, u32 h)     // w, h multiples of 8
{
	for (u32 y = 0; y < h; y++)
		for (u32 x = 0; x < w; x++) {
			u32 tile  = (y >> 3) * (w >> 3) + (x >> 3);                           // 8x8 blocks, row-major
			u32 inner = kMortonSpread[x & 7] | (kMortonSpread[y & 7] << 1);       // Morton order in the block
			dst[tile * 64 + inner] = src[y * w + x];
		}
}
```

How this was checked: a 16×16 test PNG was converted with the installed `tex3ds -f rgba8888 -z none -r` (raw data, after a 4-byte compression header) and compared word for word with the output of this function (re-implemented in Python on the PC): identical, with **no vertical flip** — the first word is the image's top-left pixel. An RGBA8 texel is the 32-bit word `0xRRGGBBAA` (red in the top byte), the opposite order from `C2D_Color32`, which returns `0xAABBGGRR` ([pitfalls](#pitfalls)). This is a host-side check against the converter, not a run on a 3DS.

The texture coordinates tex3ds attaches to a whole image are `left = 0, right = 1, top = 1, bottom = 0` (verified by parsing the `.t3x` of a one-image and a two-image atlas), so **v = 1 is the top of the stored image** and an image at pixel rectangle (x0, y0, w, h) of a W×H atlas has `left = x0/W, right = (x0+w)/W, top = 1 − y0/H, bottom = 1 − (y0+h)/H`. Never put `top < bottom` in a subtexture: `tex3ds.h` defines that as a quarter-turn-rotated image, and citro2d honours it. How the GPU turns "stored first row" into v = 1 is not shown in the local sources; the convention is what `tex3ds` and citro2d agree on, and the example programs rely on it.

A procedural sheet then looks like this (full program in [section 5](#5-a-complete-example-animated-sprite-with-touch)):

```c
C3D_Tex sheet;
if (!C3D_TexInit(&sheet, 64, 16, GPU_RGBA8)) { /* size/format rejected or out of memory */ }
swizzleRGBA8((u32 *)sheet.data, pixels, 64, 16);
C3D_TexFlush(&sheet);
C3D_TexSetFilter(&sheet, GPU_NEAREST, GPU_NEAREST);
```

### tex3ds and `.t3s` files

`tex3ds` converts PNGs to tiled textures; the Makefile rule is `tex3ds -i sprites.t3s -H sprites.h -d sprites.d -o sprites.t3x` ([asset pipeline](asset-pipeline.md#textures-with-tex3ds) has the full option table). A `.t3s` file holds options, then one image path per line. This one was converted with the installed tex3ds 2.3.0 on two small PNGs and produced `sprites.t3x` plus a header with `#define <name>_<image>_idx N`:

```text
--atlas -f rgba5551 -z auto --border edge
gfx/hero.png
gfx/enemy.png
```

| Option | Use in 2D |
| ------ | --------- |
| `--atlas` | One texture, many sub-images, each with UVs; `C2D_SpriteSheetGetImage(sheet, index)` returns one. Indices are in file order (the generated header names them) |
| `-f rgba5551` / `rgba8888` / `rgb565` / `etc1a4` … | Pixel format; see the table above |
| `--border edge` / `--border transparent` | One-pixel border around each atlas image so bilinear filtering or sub-pixel positions do not pull in the neighbour. Not needed for nearest filtering at whole-pixel positions |
| `-z auto` | Compresses the file only (the console decompresses on load; the GPU texture is not compressed). In the run above it chose `lz11` |
| `--trim` | Crop transparent margins of atlas images |

> Conflict: `tex3ds --help` (2.3.0) lists short forms `-a`, `-b` and `-c`, but the installed Windows binary rejects them ("unknown option -- a" / "-b" / "-c") and accepts the long forms `--atlas`, `--border`, `--cubemap`. The `3ds-examples` `.t3s` files also use `--atlas`. The asset pipeline page lists `-a` and `-b` as written in the help; use the long forms in `.t3s` files. Only the installed Windows build was tried.

Atlas rules: images drawn together go into one atlas so they batch; keep an atlas at or below 1024×1024; an atlas holds no repeating (`GPU_REPEAT`) texture, because wrap applies to the whole texture, not to a sub-image.

### Sprite sheets, frames and animation

A sprite sheet loaded with `C2D_SpriteSheetLoad("romfs:/gfx/sprites.t3x")` is a numbered list of images. Animation is a table of frame indices that you step with a counter:

```c
static const int walk[4] = { 0, 1, 2, 1 };               // indices into the sheet
int frame = walk[(tick / 6) % 4];                         // 6 display frames per animation frame
C2D_SpriteFromSheet(&sprite, sheet, frame);               // resets the image and size; keep position yourself
```

Notes:

- `C2D_SpriteFromImage`/`FromSheet` reset the sprite completely: position (0, 0), size = the image's pixel size, centre (0, 0), angle 0, depth 0 `[documented: sprite.h]`, so the demo in section 5 sets centre, position, depth and scale again after calling it. Cheaper: keep one `C2D_Sprite` per actor and only swap `sprite.image = C2D_SpriteSheetGetImage(sheet, i)`, which leaves the transform alone `[documented: sprite.h; C2D_Sprite has the fields image and params]`.
- **Flipping** is a negative scale: `C2D_SpriteScale(&s, -1, 1)` flips horizontally, negative y flips vertically; `C2D_DrawImageAt` takes the same signs in `scaleX`/`scaleY` `[documented: sprite.h, base.h]`. The scale also negates the centre offset, so flip around the middle by setting the centre to (0.5, 0.5) first.
- **Sub-image UVs** by hand: fill a `Tex3DS_SubTexture { width, height, left, top, right, bottom }` as above and put it in a `C2D_Image { tex, subtex }`. The pixel width/height fields decide the drawn size at scale 1.
- Keep animation timing in game ticks, not in frames drawn, if the loop can drop frames ([frame pacing](frame-pacing.md#separating-simulation-from-rendering)).

## 4. citro2d beyond sprites

### Primitives

All take a depth `z`, return `bool` (false when the object buffer is full), and colours are `u32` from `C2D_Color32(r, g, b, a)` (or `C2D_Color32f`) `[documented: base.h]`:

| Function | Draws | Colour arguments |
| -------- | ----- | ---------------- |
| `C2D_DrawRectangle(x, y, z, w, h, clr0, clr1, clr2, clr3)` | Rectangle from its top-left corner | Four corners: top-left, top-right, bottom-left, bottom-right; the GPU blends between them (gradient) |
| `C2D_DrawRectSolid(x, y, z, w, h, clr)` | Same, one colour | One |
| `C2D_DrawTriangle(x0, y0, clr0, x1, y1, clr1, x2, y2, clr2, depth)` | Triangle | One colour per vertex |
| `C2D_DrawLine(x0, y0, clr0, x1, y1, clr1, thickness, depth)` | Line of the given pixel thickness | One per end |
| `C2D_DrawEllipse(x, y, z, w, h, clr0..clr3)` / `C2D_DrawEllipseSolid` | Ellipse inside the box at (x, y) | Four corners / one |
| `C2D_DrawCircle(x, y, z, radius, clr0..clr3)` / `C2D_DrawCircleSolid` | Circle centred on (x, y) | Four corners / one |

Alpha in the colour gives translucency with the default blending. Ellipses and circles switch citro2d into a special "circle mode"; the header notes this "requires an expensive state change" and recommends drawing all non-circular objects first, then all circles `[documented: base.h]`.

### Images and tint

`C2D_DrawImage(img, &params, tint)` draws any `C2D_Image` with a `C2D_DrawParams` (`pos {x, y, w, h}`, `center`, `depth`, `angle`); `C2D_DrawImageAt` and `C2D_DrawImageAtRotated` are shortcuts; `C2D_DrawSprite` draws a `C2D_Sprite`. A `C2D_ImageTint` holds one `{color, blend}` per corner: `C2D_PlainImageTint(&t, colour, 1.0f)` tints the whole image, `C2D_AlphaImageTint(&t, 0.5f)` makes it half transparent, and the top/bottom/left/right helpers make gradients. `C2D_SetTintMode` selects how the tint combines with the texture: `C2D_TintSolid`, `C2D_TintMult`, `C2D_TintLuma`, `C2D_TintAdd`, `C2D_TintSub`, `C2D_TintOneMinusAdd`, `C2D_TintOneMinusSub` `[documented: base.h]`. Tint and fade are done by the colour combiners ([TEV](../glossary.md#tev)), so they cost no extra texture, but changing the mode or tint ends the batch ([what ends a batch](#6-performance-decisions)).

### View transforms

`C2D_ViewReset()`, `C2D_ViewTranslate(x, y)`, `C2D_ViewRotate(radians)` / `C2D_ViewRotateDegrees`, `C2D_ViewShear(x, y)` and `C2D_ViewScale(x, y)` change a matrix applied to everything drawn afterwards; `C2D_ViewSave(&m)` / `C2D_ViewRestore(&m)` let you push and pop it `[documented: base.h]`. Use it for a camera (translate by the negative camera position) or a screen shake; it ends the current batch, so change it between layers, not between sprites. Use whole-pixel translations for pixel art; a fractional translation puts texels between screen pixels. The [tile renderer](2d-tile-renderer.md) instead subtracts the integer camera from each position, which keeps the view matrix untouched.

### Scissor (clipping)

citro2d 1.7.0 has no scissor function. Use citro3d's `C3D_SetScissor(GPU_SCISSOR_NORMAL, left, top, right, bottom)` (and `GPU_SCISSOR_DISABLE` to turn it off) `[documented: citro3d base.c, libctru gpu/enums.h]`. Call `C2D_Flush()` first so geometry drawn before the change is not clipped by the new rectangle (inferred from `C2D_Flush` "ensures all 2D objects so far have been drawn"), and remember that the rectangle is in framebuffer space (see [section 2](#2-screens-coordinates-and-depth); `> Unknown:` how it maps to rotated screens — draw a test rectangle).

### Fade

`C2D_Fade(color)` sets a fade colour whose alpha is the strength: 0 = no effect, 255 = everything is the fade colour; values in between blend `[documented: base.h]`. Fade to black by increasing the alpha of a black colour; reset with `C2D_Fade(0)`. It applies to everything drawn after it, including shapes, so set it before the layers it should affect and reset it before the HUD.

### Render to texture

`C3D_RenderTargetCreateFromTex(&tex, GPU_TEXFACE_2D, 0, depthFormat)` turns a VRAM texture (`C3D_TexInitVRAM`) into a render target `[documented: citro3d renderqueue.c]`; the call returns `NULL` if the texture is not in VRAM or the depth buffer does not fit. Then `C2D_SceneBegin(thatTarget)` draws 2D into it (the scene size is the texture's size, with no tilt because the target is not linked to a screen), and you later draw the texture as an ordinary image. Rules and caveats:

- Pass a depth format (the tile renderer uses `GPU_RB_DEPTH16`) and clear with `C2D_TargetClear` or `C3D_RenderTargetClear(..., C3D_CLEAR_ALL, ...)`. Both clear colour and depth (`C3D_FrameBufClear` issues a memory fill on the depth pointer when the depth bit is set `[documented: citro3d framebuffer.c]`), so a target without a depth buffer would get a fill through a NULL pointer; give it one, or clear colour only with `C3D_CLEAR_COLOR`.
- Do not sample the texture in the pass that renders into it.
- Switching targets inside a frame flushes the pending batch and costs a framebuffer flush ([GPU rendering](gpu-rendering.md#what-a-draw-call-costs)); do all cache redraws first, then the screens.
- Animated content cannot live in the cache; draw it on top each frame.
- **Orientation is unverified.** The texture is written by the GPU with its own row order; whether the cached image shows upright when drawn with a standard subtexture was not checked here. The tile renderer has a `CACHE_FLIP_V` switch (draws the quad with a negative height). If the first run shows the cache upside down, flip it.
- Memory: 1.5 MiB for a 512×512 RGBA8 cache with 16-bit depth, on top of the screen targets ([VRAM table](#vram-or-linear-memory)). The 512×512 cache covers a 32×32 tile window of 16-pixel tiles.

### The object limit, and how batching works

`C2D_Init(maxObjects)` fixes the number of objects per frame (sizes above). An image or rectangle uses 4 vertices and 6 indices; `C2D_DrawRectangle` checks that at least 6 index slots and 4 vertex slots remain and otherwise returns `false` without drawing `[reverse-engineered: disassembly of C2D_DrawRectangle]`; the headers document `bool` results as "true on success, false on failure". So when you exceed the limit, **the rest of the frame silently disappears** (later sprites, then the HUD, are the first to go). Guard it in debug builds:

```c
if (!C2D_DrawImageAt(img, x, y, z, NULL, 1.0f, 1.0f)) { dropped++; }   // show 'dropped' in the debug overlay
```

Batching: citro2d appends each quad to a vertex buffer and submits a run with one `C3D_DrawElements` when something forces it (symbols `C2Di_AppendQuad`, `C2Di_FlushVtxBuf`, `C2Di_Update` and the `C3D_DrawElements`/`C3D_TexBind` imports in `base.o` `[reverse-engineered: nm of the installed library]`). The exact triggers are in upstream `source/base.c`, which is not cloned here; earlier pages cite it for: texture change, blend mode, tint/fade/view change, `C2D_Flush`, `C2D_TargetClear` and frame end `[documented upstream, not re-verified locally]`. Section 6 lists them with evidence.

## 5. A complete example: animated sprite with touch

Procedurally generated 64×16 RGBA8 sheet (four 16×16 frames), a frame-table animation, horizontal flip by negative scale, and touch interaction on the bottom screen with `hidTouchRead`: touch to place animated stamps (up to 32) and make the hero on the top screen walk to the touch position's x. START quits, B clears the stamps. No assets are needed.

**Verification status:** builds (devkitARM r68 GCC 16.1.0, `arm-none-eabi-gcc` invoked directly, `-Wall -Wextra -O2 -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -mword-relocations -ffunction-sections -std=gnu11`, linked to an ELF with `-specs=3dsx.specs -lcitro2d -lcitro3d -lctru -lm`, no warnings); **not run on Old 3DS, New 3DS or emulator**. No `.3dsx` was packaged.

```c
/* sprite_demo: animated sprite + touch interaction, no art assets.
 * Textures are generated in code. Needs: citro2d, citro3d, libctru. */
#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <string.h>

#define TOP_W 400
#define BOT_W 320
#define SCR_H 240
#define FRAME_W 16
#define FRAME_COUNT 4
#define SHEET_W 64   /* 4 frames of 16 px; width and height must be powers of two, 8..1024 */
#define SHEET_H 16
#define MAX_STAMPS 32

/* Spread the low 3 bits of v to every second bit: x bits -> even positions of the in-tile index. */
static const u8 kMortonSpread[8] = { 0, 1, 4, 5, 16, 17, 20, 21 };

/* Convert a top-down, row-major 0xRRGGBBAA image to the GPU's tiled layout (8x8 tiles,
 * Morton order inside a tile, tiles row-major from the top-left). Width/height multiples of 8.
 * Layout verified on the host against `tex3ds -f rgba8888 -r` output (see the page). */
static void swizzleRGBA8(u32 *dst, const u32 *src, u32 w, u32 h)
{
	for (u32 y = 0; y < h; y++)
		for (u32 x = 0; x < w; x++) {
			u32 tile  = (y >> 3) * (w >> 3) + (x >> 3);
			u32 inner = kMortonSpread[x & 7] | (kMortonSpread[y & 7] << 1);
			dst[tile * 64 + inner] = src[y * w + x];
		}
}

static u32 rgba(u8 r, u8 g, u8 b, u8 a) { return ((u32)r << 24) | ((u32)g << 16) | ((u32)b << 8) | a; }

/* Frame f: a round creature standing on the bottom edge; the body gets squashed in some frames. */
static void paintFrame(u32 *img, int f)
{
	static const int squash[FRAME_COUNT] = { 0, 1, 2, 1 };
	int ry = 15 - 2 * squash[f];   /* vertical radius in half-pixel units */
	for (int y = 0; y < SHEET_H; y++)
		for (int x = 0; x < FRAME_W; x++) {
			int dx = x * 2 - 15, dy = y * 2 - 30 + ry;
			int inside = dx * dx * ry * ry + dy * dy * 225 <= 225 * ry * ry;
			u32 c = 0;   /* fully transparent */
			if (inside) c = rgba(230, (u8)(120 + 20 * f), 60, 255);
			if (inside && (y == 5 + squash[f] || y == 6 + squash[f]) && (x == 4 || x == 5 || x == 10 || x == 11))
				c = rgba(20, 20, 20, 255);   /* eyes */
			img[y * SHEET_W + f * FRAME_W + x] = c;
		}
}

typedef struct { float x, y; } Stamp;

int main(void)
{
	gfxInitDefault();
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	if (!C2D_Init(C2D_DEFAULT_MAX_OBJECTS)) { C3D_Fini(); gfxExit(); return 1; }
	C2D_Prepare();

	C3D_RenderTarget *top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
	C3D_RenderTarget *bot = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

	/* --- procedural sprite sheet --- */
	static u32 linearPixels[SHEET_W * SHEET_H];   /* CPU-side, top-down */
	for (int f = 0; f < FRAME_COUNT; f++) paintFrame(linearPixels, f);

	C3D_Tex sheet;
	if (!C3D_TexInit(&sheet, SHEET_W, SHEET_H, GPU_RGBA8)) { C2D_Fini(); C3D_Fini(); gfxExit(); return 1; }
	swizzleRGBA8((u32 *)sheet.data, linearPixels, SHEET_W, SHEET_H);   /* linear memory: CPU writes directly */
	C3D_TexFlush(&sheet);                                              /* write back the CPU cache before the GPU reads */
	C3D_TexSetFilter(&sheet, GPU_NEAREST, GPU_NEAREST);
	C3D_TexSetWrap(&sheet, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);

	/* Sub-image UVs: u grows to the right, v = 1 at the TOP of the stored image (same as tex3ds output). */
	Tex3DS_SubTexture sub[FRAME_COUNT];
	C2D_Image frame[FRAME_COUNT];
	for (int f = 0; f < FRAME_COUNT; f++) {
		sub[f].width  = FRAME_W;
		sub[f].height = FRAME_W;
		sub[f].left   = (float)(f * FRAME_W) / SHEET_W;
		sub[f].right  = (float)((f + 1) * FRAME_W) / SHEET_W;
		sub[f].top    = 1.0f;
		sub[f].bottom = 1.0f - (float)FRAME_W / SHEET_H;
		frame[f].tex    = &sheet;
		frame[f].subtex = &sub[f];
	}
	static const int walkCycle[4] = { 0, 1, 2, 1 };   /* frame table */

	C2D_TextBuf staticBuf  = C2D_TextBufNew(64);
	C2D_TextBuf dynamicBuf = C2D_TextBufNew(64);
	C2D_Text help;
	C2D_TextParse(&help, staticBuf, "Touch the bottom screen. START exits.");
	C2D_TextOptimize(&help);

	C2D_Sprite hero;
	C2D_SpriteFromImage(&hero, frame[0]);
	C2D_SpriteSetCenter(&hero, 0.5f, 0.5f);
	C2D_SpriteSetPos(&hero, TOP_W / 2, SCR_H / 2);
	C2D_SpriteSetDepth(&hero, 0.5f);
	float heroX = TOP_W / 2, targetX = TOP_W / 2;
	int faceLeft = 0;

	Stamp stamps[MAX_STAMPS];
	int stampCount = 0;
	u32 tick = 0;

	while (aptMainLoop()) {
		hidScanInput();
		u32 down = hidKeysDown();
		if (down & KEY_START) break;

		touchPosition touch;
		hidTouchRead(&touch);   /* px, py in bottom-screen pixels: 0..319, 0..239 */
		int touching = (hidKeysHeld() & KEY_TOUCH) != 0;   /* px/py are 0 when nothing touches the screen */
		if (touching) {
			targetX = (float)touch.px * TOP_W / BOT_W;   /* map 320 px wide to 400 px wide */
			if (down & KEY_TOUCH) {
				if (stampCount < MAX_STAMPS) stamps[stampCount++] = (Stamp){ (float)touch.px, (float)touch.py };
			}
		}
		if (down & KEY_B) stampCount = 0;

		float dx = targetX - heroX;
		int moving = dx > 1.0f || dx < -1.0f;
		if (moving) {
			heroX += dx > 0 ? 1.5f : -1.5f;
			faceLeft = dx < 0;
		}
		tick++;
		int idx = moving ? walkCycle[(tick / 6) % 4] : 0;

		C2D_SpriteFromImage(&hero, frame[idx]);   /* resets position, size, centre, angle, depth: set them again */
		C2D_SpriteSetCenter(&hero, 0.5f, 0.5f);
		C2D_SpriteSetPos(&hero, heroX, SCR_H / 2);
		C2D_SpriteSetDepth(&hero, 0.5f);
		C2D_SpriteScale(&hero, faceLeft ? -4.0f : 4.0f, 4.0f);   /* negative x scale flips horizontally */

		char line[48];
		snprintf(line, sizeof line, "stamps: %d  frame: %d", stampCount, idx);
		C2D_TextBufClear(dynamicBuf);
		C2D_Text info;
		C2D_TextParse(&info, dynamicBuf, line);
		C2D_TextOptimize(&info);

		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
			C2D_TargetClear(top, C2D_Color32(24, 32, 56, 255));
			C2D_SceneBegin(top);
			C2D_DrawRectSolid(0, SCR_H - 40, 0.2f, TOP_W, 40, C2D_Color32(60, 90, 50, 255));   /* ground */
			C2D_DrawSprite(&hero);
			C2D_DrawText(&help, C2D_WithColor, 8, 8, 0.6f, 0.5f, 0.5f, C2D_Color32(255, 255, 255, 255));
			C2D_DrawText(&info, C2D_WithColor, 8, 24, 0.6f, 0.5f, 0.5f, C2D_Color32(200, 255, 200, 255));

			C2D_TargetClear(bot, C2D_Color32(16, 16, 16, 255));
			C2D_SceneBegin(bot);
			for (int i = 0; i < stampCount; i++)
				C2D_DrawImageAt(frame[(i + tick / 10) % FRAME_COUNT], stamps[i].x - 16, stamps[i].y - 16, 0.5f, NULL, 2.0f, 2.0f);
			if (touching)
				C2D_DrawCircleSolid((float)touch.px, (float)touch.py, 0.6f, 4.0f, C2D_Color32(255, 255, 255, 255));
		C3D_FrameEnd(0);
	}

	C2D_TextBufDelete(dynamicBuf);
	C2D_TextBufDelete(staticBuf);
	C3D_TexDelete(&sheet);
	C2D_Fini();
	C3D_Fini();   /* also destroys the render targets that are still alive */
	gfxExit();
	return 0;
}
```

What to expect (unverified): a dark-blue top screen with a green ground bar and an orange creature standing in the middle; touching the bottom screen starts it walking toward the matching x position (flipping to face left when needed) and leaves a small animated creature where you touched. If the creature is upside down, the V convention above is wrong for your setup; if its colours are blue where they should be orange, the texel byte order is.

## Sprites

The short version, with citro2d and a tex3ds atlas:

```c
C2D_SpriteSheet sheet = C2D_SpriteSheetLoad("romfs:/gfx/sprites.t3x");   // a tex3ds atlas
C2D_Sprite hero;
C2D_SpriteFromSheet(&hero, sheet, 0);
C2D_SpriteSetCenter(&hero, 0.5f, 0.5f);
C2D_SpriteSetPos(&hero, 200, 120);

/* each frame, inside C3D_FrameBegin/C3D_FrameEnd after C2D_SceneBegin(top) */
C2D_SpriteSetRotation(&hero, angle);
C2D_DrawSprite(&hero);
```

- Draw sprites that share an atlas consecutively; interleaving two atlases ends citro2d's batch at every switch.
- `C2D_Init(maxObjects)` sizes the vertex buffer for the whole frame; once full, further draw calls return `false` and draw nothing. Size it for the worst scene.
- Tinting (`C2D_ImageTint`, `C2D_SetTintMode` with `C2D_TintSolid`, `C2D_TintMult`, `C2D_TintLuma`, and the add/subtract modes) and whole-scene fades (`C2D_Fade`) are done by the combiners at no texture cost, but changing them ends a batch.
- Depth: citro2d draws accept a depth value; larger z is in front and equal z passes ([section 2](#2-screens-coordinates-and-depth)). With one shared z, draw order decides. Because transparent texels still write depth, sprites with soft or cut-out edges are safest drawn back to front.

For very large sprite counts beyond citro2d's limits, write quads into your own vertex buffer and draw them with one `C3D_DrawArrays`/`C3D_DrawElements` per texture, or expand points into quads in a geometry shader ([`graphics/gpu/gpusprites`](https://github.com/devkitPro/3ds-examples/tree/master/graphics/gpu/gpusprites) shows the latter). Which approach is fastest for which count has not been measured here; experiment E1 compares them and [drawbench](../benchmarks/README.md#drawbench) measures the per-draw costs underneath.

## 6. Performance decisions

Old 3DS budget: 16.7 ms per frame at 60 fps, about 4.47 million core-0 cycles ([performance](performance.md#the-budget)). The GPU is a separate chip, so a frame is limited by whichever of CPU recording or GPU drawing is slower. Measure before changing anything ([measure the frame](performance.md#a-frame-level-breakdown)).

> **New 3DS:** the CPU runs three times faster with the speed-up enabled ([performance](performance.md#new-3ds-turn-on-the-fast-mode-yourself)), which shrinks the recording costs in the table.

> Unknown: whether GPU-side costs (fill, transfer) change on the New 3DS; nothing here has been measured on either model.

| Decision | Recommendation | Mechanism | Label |
| -------- | -------------- | --------- | ----- |
| Many images from one atlas | Draw them consecutively; they become one draw call per run | citro2d submits one `C3D_DrawElements` per run; each citro3d draw call writes dirty state into the command buffer ([draw-call costs](gpu-rendering.md#what-a-draw-call-costs)) | `[documented]` mechanism; saving size `[hypothesis]` — drawbench `batched` vs `per-quad` variants measure the per-draw cost |
| A different texture mid-run | Group by texture or merge atlases | Each texture change ends the run and `C3D_TexBind` rewrites the unit and clears the texture cache | `[documented: gpu-rendering.md]`; cost `[hypothesis]` — drawbench `+texswap` vs `+texbind` |
| Text between sprites | Draw all text after all sprites | Glyphs come from font sheets, a texture switch each time | `[documented: text.h glyph sheets; upstream base.c batching]`; cost `[hypothesis]` |
| Rectangles, lines, circles between textured quads | Draw shapes in one block (before or after the images) | `C2D_DrawRectangle` clears the texture-mode state bits, so the next textured draw re-applies it | `[reverse-engineered: disassembly]`; cost `[hypothesis]` |
| Circles and ellipses | Draw all other objects first, then all circles | "Expensive state change" per switch | `[documented: base.h note]` |
| Tint, tint mode, `C2D_Fade`, view matrix changes | Change between layers, not per sprite | End the current run | `[documented upstream base.c, not re-verified locally]` |
| Off-screen objects | Cull them before the draw call (tile ranges, sprite bounds) | Each call fills 4 vertices (128 bytes) on the CPU and takes one object slot, even if off-screen | `[reverse-engineered: C2D_Init sizes]`; CPU saving `[hypothesis]` |
| More than a few thousand objects per frame | Raise `maxObjects`, or move that layer to your own vertex buffer with one `C3D_DrawElements` per texture | Object limit; the default 4096 costs 560 KiB | `[documented]` limit; the crossover count `[hypothesis]` — experiment E1 |
| Static or slowly changing background | Render into a texture and draw one quad per frame (cache window larger than the screen) | Replaces hundreds of quads with one; costs 1.5 MiB of VRAM for 512×512 + depth | API `[documented]`; benefit `[hypothesis]`; settled by experiment E2 ([design alternatives](#design-alternatives-and-how-to-choose)) |
| `C2D_TargetClear` | Call once per target per frame; do not call mid-layer | It splits the command list (`C3D_FrameSplit(0)`) and clears colour and depth | `[reverse-engineered: disassembly of C2D_TargetClear]`; cost `[hypothesis]` |
| Screen content unchanged (menus, maps) | Do not call `C2D_SceneBegin` for that screen this frame | Targets not drawn are not transferred or swapped | `[documented: renderqueue.c]` |
| Full-screen translucent overlays | Keep them few; use small quads | Each covers every pixel, blending reads the framebuffer | general GPU behaviour `[hypothesis]` for the PICA200; no fill-rate measurement |
| Wide mode | Only if the sharper image matters | Doubles the top screen's pixels and render-target size | `[documented: libctru gfx.h]` for the mode; cost `[hypothesis]` |
| Stereo | Skip the right-eye pass when the slider is 0 | Right target is only drawn if you draw it | `[documented: composite_scene example, renderqueue.c]` |
| Whole-heap flush at `C3D_FrameEnd(0)` | Consider `GX_CMDLIST_FLUSH` plus targeted flushes only after measuring | The default flushes all linear memory | `[documented: memory-and-cache.md]`; cost `[hypothesis]` — flushbench |

The open `[hypothesis]` items in this table are turned into experiments in the next section (E1 to E6 implemented in the [tile renderer](2d-tile-renderer.md#experiments); P1 to P3 specified). Still without an experiment: the fill-rate cost of full-screen translucent layers (drawbench draws 4×4-pixel quads, so it does not measure fill; a variant with K stacked full-screen translucent quads reading `c3d_gpu` would), and the cost of `C2D_TargetClear` splits (one target against two, same total quads).

## Design alternatives and how to choose

Section 6 lists single decisions; this section lines the techniques up as alternatives for the same job, with the cost of each and the experiment that would settle where one overtakes another. **Nothing in the "benefit" column has been measured**: `[documented]` means the mechanism is in a named file, `[hypothesis]` means the size of the effect is unknown. Experiments E1 to E6 are implemented as compile-time modes of the [tile renderer](2d-tile-renderer.md#experiments) (they build; none has been run); P1 to P3 are specified here and not implemented. Record format: [output records](2d-tile-renderer.md#output-records).

| Technique | Expected benefit mechanism | Cost | Experiment that settles the crossover |
| --------- | -------------------------- | ---- | ------------------------------------- |
| **Visible-tile submission** (draw only the camera's tile range) | Every `C2D_Draw*` call writes 4 vertices (128 bytes) and takes one of the 4,096 object slots `[reverse-engineered: disassembly of C2D_Init, section 3]`; skipping off-screen tiles removes that CPU work `[hypothesis]` for the size of the saving. The whole 128×96 map (12,288 tiles) cannot be submitted at all under the default limit (arithmetic) | A range computation and a clamp per frame | **E1**, strategy 0 at `TILE` 32, 24, 16, 12, 8: the slope of CPU cost against visible tiles (126 to 1,581) is the per-tile cost every other technique is compared with |
| **Manual vertex buffer**, one draw call (versus `C2D_DrawImageAt` per tile) | One `C3D_DrawArrays` and a 20-byte vertex instead of a per-tile library call and a 32-byte vertex; no per-call bookkeeping `[hypothesis]` (citro2d's `base.c` is not read here) | Own shader (about 15 lines of PICA assembly), own state hand-over back to citro2d (`C2D_Flush`, `C2D_Prepare`, `C2D_SceneBegin` once per pass), no automatic batching with citro2d's other objects | **E1**: strategy 1 against 0 on the same five tile counts; the crossover is the first count where strategy 1 wins and keeps winning |
| **Chunked retained geometry** (build each chunk's vertices once, redraw by offset) | Static tiles cost no per-frame vertex writing `[hypothesis]`; the camera moves through a shader uniform, so the vertices do not change | Linear memory 12,288 × 6 × 20 = 1,474,560 bytes for this map (arithmetic). **Chunk size trade-off, arithmetic at 16 px tiles:** 8-tile chunks give up to 15 draw calls and 5,760 vertices submitted, a rebuild writes 384 vertices (7,680 bytes); 16-tile: 6 calls, 9,216 vertices, rebuild 1,536 vertices (30,720 bytes); 32-tile: 4 calls, 24,576 vertices, rebuild 6,144 vertices (122,880 bytes). A rebuilt chunk reaches the GPU through the whole-heap flush at `C3D_FrameEnd(0)`, or a targeted flush with `GX_CMDLIST_FLUSH` ([FrameEnd flush](memory-and-cache.md#the-c3d_frameend-flush)) | **E4**: `CHUNK_TILES` 8, 16, 32 with `MAP_EDIT_PERIOD` 0, 30, 1; reads `build` (rebuild spike), `record`, `c3d_gpu` (cost of the extra vertices). Compare with E1 strategy 1 at the same `TILE` |
| **Cached background texture** (render once, one quad per frame) | Replaces up to 416 tile quads with 1 `[documented structure]`; saves CPU recording time `[hypothesis]` | VRAM: 512×512 RGBA8 + 16-bit depth = 1,048,576 + 524,288 = 1,572,864 bytes (arithmetic; a 1,024×1,024 cache would be 4 MiB + 2 MiB, all 6 MiB of VRAM). A 16-bit colour target would halve the colour part `[hypothesis]`, untried here. Bandwidth: the cached quad reads 400×240×4 = 384,000 bytes of texture per frame against a 16 KiB hot atlas (arithmetic; the effect is `[hypothesis]`). Redraw: 1,024 quads plus a render-target switch ([draw-call costs](gpu-rendering.md#what-a-draw-call-costs)) about every 48 ÷ speed frames; animated tiles and sprites cannot live in it; contents after HOME or sleep are not established ([lifecycle](application-lifecycle.md#open-questions)) | **E2**: strategy 0 against 3 over camera speeds 0, 1, 2, 4, 8 px/frame and edit rates 0, 30, 5; reads mean and p99 of `build`, mean CPU cost, `c3d_gpu`, missed frames and the VRAM bytes. The cache pays at the highest speed and edit rate where its mean is lower and its p99 and missed frames are no worse |
| **Texture atlas / avoiding texture alternation** | A texture change ends citro2d's batch, and `C3D_TexBind` marks the unit dirty and clears the texture cache `[documented: gpu-rendering.md; upstream base.c for the batch break, not re-verified locally]`; size of the penalty `[hypothesis]` | Atlas packing effort; the 1024×1024 limit; no repeat wrap inside an atlas | **E3**: `TEX_MODE` 0 (one atlas), 1 (alternate every tile), 2 (same two atlases, two runs) at `TILE` 32, 16, 12; per-switch cost = (mode 1 − mode 2) ÷ (N − 2). [drawbench](../benchmarks/README.md#drawbench) `+texswap` measures the same thing one level lower |
| **Atlas animation** (frames side by side, chosen by UV) | Same texture, so no batch break and no texel writes `[documented structure: the tile renderer's water tiles]` | Atlas space: four 16×16 RGBA8 water frames are 4,096 bytes | UV selection needs no experiment of its own: its cost is part of E1. The alternative, rewriting texels each frame, is **P1** below |
| **Partial texture updates** by changed 8×8 tiles (versus a whole-texture upload) | A changed 8×8 block is one contiguous span in the tiled layout: 256 bytes in RGBA8, 128 bytes in 16-bit formats ([section 3](#3-textures-and-memory)), so a copy and a flush of only that span replace a whole-texture write `[documented: memory-and-cache.md]`; the saving is `[hypothesis]` | Dirty tracking. **Cache rules** ([memory and cache](memory-and-cache.md#keeping-caches-coherent)): flushes work on whole 32-byte lines, so a 256-byte tile is 8 lines and a 128-byte tile 4 lines, and a tile starts on a line boundary only if the texture base does (`linearAlloc` returns 0x80-aligned blocks `[documented]`; what `C3D_TexInit` requests was not read here, so check `(uintptr_t)tex.data % 32` at run time). One flush call per tile has a fixed per-call cost (an IPC for `GSPGPU_FlushDataCache`, a kernel call for `svcFlushProcessDataCache`, up to three ranges per `GX_FlushCacheRegions`). Do not write a texture the GPU is reading this frame | **P1** (not implemented): a 256×256 RGBA8 linear texture; k changed tiles, k in {1, 4, 16, 64, 256, 1,024 = all}, scattered and contiguous; methods (a) k × (256-byte copy + `svcFlushProcessDataCache` of that tile), (b) k copies + one flush of the whole texture, (c) full rewrite + `C3D_TexFlush`; time with ticks; report the k where (a) overtakes (c). [flushbench](../benchmarks/README.md#flushbench) starts at 1 KiB ranges and does not cover 256-byte flushes |
| **Palette effects** (tint, pre-converted variants, CPU re-expansion) | Tint is done by the combiners: no extra texture `[documented: base.h]`; variant atlases cost memory but no per-frame work; CPU re-expansion costs CPU plus a flush but keeps one texture `[hypothesis]` for the ranking | Tint changes end a batch; variants are 16 KiB each for a 128×32 RGBA8 atlas (arithmetic) times the number of palettes; re-expansion costs bytes × CPU cost + flush | **P2** (not implemented): 64 sprites, 4 palettes; (a) `C2D_ImageTint` grouped by palette, (b) four atlas variants grouped by palette, (c) CPU re-expansion of the atlas each frame with swizzle and flush; metric `build + record + frameend`; plus ungrouped versions of (a) and (b) to measure the batch break the way E3 does |
| **Cached text** | `C2D_TextParse` lays out glyphs on the CPU; a parsed `C2D_Text` is redrawn without that work `[documented: text.h]`; the cost of parsing is `[hypothesis]` | Memory in the `C2D_TextBuf`; clearing a buffer invalidates all its texts, so one buffer per update rate | **P3** (not implemented): time `C2D_TextParse` + `C2D_TextOptimize` of a 40-glyph string with ticks over 1,000 repetitions, and `C2D_DrawText` of the parsed text over 1,000 frames; the result is a cost per glyph and the number of glyphs that may be re-parsed per frame within the budget |
| **Static bottom-screen content** | A target that is not drawn in a frame is neither transferred nor swapped `[documented: citro3d renderqueue.c]` | Dirty tracking; after HOME restore both screens must be drawn again ([lifecycle](application-lifecycle.md#home-menu-suspend-and-restore)) | **E5**: `BOTTOM_PERIOD` 1, 2, 4, 30; saving per skipped frame from the means of `record`, `frameend`, `c3d_gpu` |
| **Pixel snapping and stereo offsets** | Whole-pixel positions keep texels aligned to screen pixels, so nearest filtering does not shimmer `[documented mechanism]`; this is a quality choice, not a speed one. Stereo parallax uses integer offsets per layer and eye ([stereo](#stereoscopic-2d-layers)) | Stereo doubles the top screen's draw work and render-target memory (arithmetic from [section 1](#starting-up)) | Quality: the screenshot check in the [validation checklist](#validation-checklist-for-a-2d-engine) at `CAM_SPEED=0.5f` with and without `floorf`. Cost: not implemented (the tile renderer is mono); define as the same scene with the right-eye target drawn, reading `c3d_gpu` and `mean_cpu_build_record_frameend_us` |
| **Shape batching** (rectangles and lines first, circles last) | The installed header says circle mode "requires an expensive state change" and recommends all non-circular objects first, then all circles; the `3ds-examples` `2d_shapes` example repeats it `[documented: c2d/base.h, 2d_shapes/source/main.c; both read locally]`; the size of the cost is `[hypothesis]` | Sorting the draw order, which may conflict with layering | **E6**: `SHAPE_MODE` 1 (interleaved) against 2 (grouped), `SHAPE_PAIRS` 8, 32, 128; cost per circle-mode switch = (mode 1 − mode 2) ÷ (2 × pairs − 2) |

How to use the table: start from the simplest row that fits (visible tiles with `C2D_DrawImageAt`, one atlas, cached text, a bottom screen redrawn only when it changes), measure `mean_cpu_build_record_frameend_us` and `c3d_gpu` for your own scene with the tile renderer's `BENCH_AUTO` build as the template, and move to the next row only when a measured cost exceeds your budget (16.7 ms per frame, [performance](performance.md#the-budget)). A technique that trades memory for time (chunks, cache, palette variants) also needs its memory figure checked against the [VRAM and linear budgets](#vram-or-linear-memory).

## Validation checklist for a 2D engine

A plan for proving a 2D renderer correct and stable. **Every row is `not run`**: no console, emulator or capture was used for this page, and the tile renderer has no framebuffer dump. The "How" column says what to build or press; the pass criteria are the project's own choices, not measured values.

| # | Check | How | Pass criterion | Status |
| - | ----- | --- | -------------- | ------ |
| 1 | **Pixel equality between strategies** | Scripted camera at fixed frames (`BENCH_AUTO`, fixed seed). Dump the top-screen colour buffer to the SD card after the GPU and the display transfer have finished, and invalidate the range before the CPU reads it ([reading GPU output](memory-and-cache.md#reading-gpu-output-on-the-cpu)). The buffer is 240 pixels wide and 400 tall, 3 bytes per pixel once transferred ([section 2](#2-screens-coordinates-and-depth)); which of the two swap buffers holds the newest frame is not established here, so verify the dump with a known pattern first. Compare dumps from `TILE_STRATEGY` 0, 1, 2, 3 on a PC | Byte-identical, or every differing pixel explained (the cached layer's V orientation, filtering at tile edges) | not run |
| 2 | **Pixel equality with a reference** | The map, atlas and sprites are deterministic (`hash2`, fixed seed, `rnd`); re-implement them in a PC script, render the same frame in software, compare with the dump from check 1 | Exact match, or a listed tolerance | not run |
| 3 | **Screenshot comparison** | Photograph or capture the screen at the same frames; an emulator screenshot is allowed for correctness only and never replaces a hardware pass ([testing on hardware](testing-on-hardware.md)) | Same content in the same places | not run |
| 4 | **Missed-frame counting** | `missed_frames_over_25ms` (loop periods above 25 ms) and `vblanks_elapsed` against `frames` in the `summary` lines ([output records](2d-tile-renderer.md#output-records)); keep a histogram of frame intervals rather than an average ([frame pacing](frame-pacing.md#detecting-missed-frames)) | 0 missed frames in a 600-frame scripted run, or each one explained (cache redraw, map edit) | not run |
| 5 | **Budget and limits** | `dropped_objects` is 0, `cmdbuf_peak_pct` is below 98, `linear_free_end_bytes` and `vram_free_end_bytes` against the `meta` values at the start | All three hold in every scene | not run |
| 6 | **10-minute soak** | `-DBENCH_AUTO=1 -DBENCH_WARMUP=300 -DBENCH_FRAMES=36000` (36,000 frames is 10 minutes at 60 frames per second, by arithmetic). The program records whole-run statistics only, so run a 600-frame build before and after the soak and compare `p99_period_us` to see drift | No growth of used linear or VRAM memory between the `meta` and the end `summary`; no new missed frames late in the run; no crash | not run |
| 7 | **HOME suspend and resume** | During the soak press HOME for 10 s and return. The GPU right is released and VRAM contents across the suspend are not established, so after `ONRESTORE` render a full new frame ([application lifecycle](application-lifecycle.md#home-menu-suspend-and-restore)). The tile renderer's APT hook invalidates the strategy 3 cache; `resume_events` must be 1 | Correct picture on both screens on the first frame after return; the one long period around the suspend is excluded from the missed-frame count by hand | not run |
| 8 | **Bottom screen after resume** | With `BOTTOM_PERIOD=30`, resume between bottom-screen frames. The tile renderer does not force a bottom redraw when the hook fires, so the bottom screen may show an old or empty image until its next scheduled draw `[hypothesis]` | The first frame after return draws both screens (the fix is to redraw everything when the hook fires) | not run |
| 9 | **Sleep** | Close the lid for 10 s and 60 s; keep `ONSLEEP` short ([sleep](application-lifecycle.md#sleep-lid-closed)) | As check 7; no audio or save work in the hook | not run |
| 10 | **Pixel snapping** | `CAM_SPEED=0.5f` scroll; photograph consecutive frames with whole-pixel snapping, and with a build that removes the `floorf` | No shimmer or seams with snapping; the unsnapped build is the control | not run |
| 11 | **Both models** | Repeat 1 to 9 on an Old 3DS and a New 3DS (separate data sets) | Same pictures; timings recorded separately | not run |

Each result goes into the benchmark README format and into the results of the page whose claim it settles, labelled `[measured]` with the console model.

## Building layers and tilemaps

Four rules cover most 2D work:

1. **Few textures, many quads.** Put everything drawn together into one atlas so that one draw call covers hundreds of quads.
2. **Do not redraw what did not change.** Static layers can be drawn once into a texture and reused.
3. **Move, don't rebuild.** Scrolling and camera motion should change a position or texture coordinates, not regenerate geometry or pixels.
4. **No palettes on the GPU.** Convert palettised art to direct-colour textures; handle palette effects another way (below).

### Tilemaps and scrolling backgrounds

A tile layer is a grid of quads whose texture coordinates point at tiles in an atlas. The worked, compile-checked version (camera clamp, integer scroll, animated water, y-sorted sprites, cached HUD text, debug overlay, and a cached-background variant) is on [2D tile renderer](2d-tile-renderer.md). The principles:

- **Draw only what is visible.** For a 400×240 screen of 16-pixel tiles, at most 26 columns × 16 rows = 416 tiles are visible (25 × 15 when the camera is aligned to a tile). Compute the tile range from the camera, not from the map size (arithmetic).
- **Scroll by changing the position or transform, not the data.** Keep the camera as an integer pixel offset. For very large layers in your own vertex buffer, build the visible area plus one tile of margin once and rebuild only the new row or column when the camera crosses a tile boundary (a ring of rows and columns).
- **Big, static maps**: render-to-texture caches, retained chunk geometry or a manual vertex buffer; which pays, and at what memory cost, is the [design alternatives table](#design-alternatives-and-how-to-choose) (all `[hypothesis]` until E1, E2 and E4 run). A 1024×1024 RGBA8 cache with depth would take all of VRAM.
- **Animated tiles** (water, conveyor belts): keep animation frames side by side in the atlas and choose the frame per tile kind per frame, or change texture coordinates in a few vertices.
- **Seams**: with bilinear filtering or non-integer scroll positions, neighbouring atlas tiles bleed into each other. Use `--border edge` padding, nearest filtering (`C3D_TexSetFilter(tex, GPU_NEAREST, GPU_NEAREST)`), and integer positions for pixel art.
- **Repeating backgrounds** (sky, parallax layers): one quad with `C3D_TexSetWrap(tex, GPU_REPEAT, GPU_REPEAT)` and scrolled texture coordinates draws an endless layer. Repeat requires a standalone power-of-two texture, not a sub-image of an atlas.

### Layers and composition

- Draw layers back to front: far backgrounds, tile layers, sprites, foreground, UI. Opaque layers may instead use the depth buffer in any order, since larger z is in front ([section 2](#2-screens-coordinates-and-depth)); translucent and cut-out sprites must still be drawn back to front.
- Each layer that covers the whole 400×240 screen costs fill rate; stacked full-screen semi-transparent layers multiply it. Keep translucent full-screen overlays to a minimum and prefer small quads [general GPU behaviour; the PICA200's fill rate has not been measured here].
- The bottom screen is a separate render target. If its content rarely changes (maps, menus), redraw it only when it changes and leave the previous framebuffer on screen — citro3d swaps a screen only if its target was drawn that frame ([GPU rendering](gpu-rendering.md#technical-details)).
- **Masks and windows**: rectangular clipping with `C3D_SetScissor`; arbitrary shapes with the stencil buffer (`C3D_StencilTest`, `C3D_StencilOp`) — draw the mask shape into stencil, then draw the masked layer with a stencil test. citro2d's own screen targets have no stencil bits (16-bit depth only), so a stencil mask needs targets you create yourself with `GPU_RB_DEPTH24_STENCIL8`.

### Transparency

| Need | Use | Cost |
| ---- | --- | ---- |
| Cut-out edges (pixel art, foliage) | `RGBA5551` or alpha-tested textures; `C3D_AlphaTest(true, GPU_GREATER, 0x80)` | No blending, any draw order; alpha test also stops transparent texels writing depth |
| Soft edges, glow, shadows | Alpha blending (`C3D_AlphaBlend`, citro2d default) | Back-to-front order required |
| Additive light, particles | Additive blend (`GPU_BLEND_ADD` with `GPU_SRC_ALPHA`, `GPU_ONE`) | Order-independent |

### Text

citro2d renders text from the system font or a converted BCFNT font ([asset pipeline](asset-pipeline.md#fonts)):

```c
C2D_TextBuf buf = C2D_TextBufNew(4096);          // glyph capacity
C2D_Text label;
C2D_TextParse(&label, buf, "Score: 0");
C2D_TextOptimize(&label);                         // once, after parsing
/* each frame */
C2D_DrawText(&label, C2D_WithColor | C2D_AlignCenter, 200, 20, 0.5f, 0.6f, 0.6f, C2D_Color32(255, 255, 255, 255));
```

- Parse static strings once; re-parse only text that changed. `C2D_TextBufClear(buf)` empties the whole buffer, invalidating every `C2D_Text` parsed into it, so give dynamic text its own buffer (the examples keep one buffer per update rate).
- Fonts store glyphs on several texture sheets, so a string can need several texture changes; `C2D_TextOptimize` is documented as optimising a text object "in order to be drawn more efficiently" — call it once after parsing.
- Text draws take `scaleX`/`scaleY` after `z`, then the colour when `C2D_WithColor` is set. The default system font works after `C2D_Init` without `romfsInit` (`graphics/printing/system-font`); loading a font from the system by region needs `cfguInit` (`custom-font` example).

## Palette effects without palettes

The PICA200 cannot look up a colour in a palette per pixel ([GPU rendering](gpu-rendering.md#what-the-pica200-cannot-do)). Alternatives:

| Effect | Technique |
| ------ | --------- |
| Whole-sprite colour change (hit flash, team colours) | Combiner tint (`C2D_ImageTint`, or `GPU_MODULATE` with a constant colour) |
| Recolouring specific regions (team colours on a uniform) | Store a greyscale mask in a second texture or in alpha, and combine tex0 × constant colour by mask with `GPU_INTERPOLATE` |
| Palette swaps with few variants | Pre-convert each variant to its own texture (or atlas page) at build or load time |
| Palette cycling (water, lava) | A few pre-converted frames swapped by UV offset, or redraw the affected tiles into a texture when the palette changes |
| Fades to black or white | `C2D_Fade` or a constant-colour combiner stage |
| Emulating a palette-based original exactly (ports) | Convert tiles on the CPU when their source data or palette changes, cache the result, upload only changed 8×8 tiles ([memory and cache](memory-and-cache.md#texture-uploads)) |

## Porting a tile- and palette-based original

Ports of DS-style games (tile backgrounds, palette banks, sprite tables) hit a specific problem: the original's pixels are produced from tiles *plus* palettes *plus* registers, so GPU textures must be derived from all three. The rules below come from one project's engineering notes (an unreleased HGSS port; local notes, not published, not reproduced by this wiki) and are `[hypothesis]` for other projects until measured:

- **Key every cached texture by tile data and effective palette.** A 4-bit tile drawn with palette bank 4 is a different texture from the same tile with bank 2. Expand to RGBA5551 or RGB565 only when the tile data or its palette changes, then reuse the result while the camera scrolls.
- **Track writers, and invalidate conservatively.** If you cannot observe every path that changes the source memory (CPU store, DMA, bank remap), treat the whole bank as dirty. Equal pointers do not mean equal content after a remap.
- **Scroll by changing UVs or a transform, not pixels.** Rebuild only the newly exposed row or column of tiles.
- **Keep the original's layer order.** Sort sprites by texture only inside runs where order does not matter; end the batch whenever priority, blend or clipping state changes, otherwise duplicated or missing sprites appear.
- **A cached static menu is valid only while the effect state is identical** (fade, window, blend) to the state it was rendered under.
- **Mind gutters.** Nearest sampling at non-integer scale (for example 1.25x to fill 400x240) gives uneven pixel widths; pad atlas tiles when you use bilinear.

DS 2D features that do not map one-to-one onto PICA200 quads (same source, same `[hypothesis]` status; they are cautions about a *pixel-exact* port, not about ordinary new games):

| DS behaviour | A PICA200 approach | Why it is not automatic |
| ------------ | ------------------ | ----------------------- |
| Text background scrolling | Persistent texture or cached tile quads, scrolled by UV offset | The texture must already hold the correct map content; rebuilding it on the CPU every update removes the gain |
| Layer priority between backgrounds and sprites | Draw in explicit order (layers or bands) | A depth test alone does not reproduce per-pixel priority and tie rules |
| Windows (including sprite windows) | Scissor for rectangles, stencil or an extra mask pass otherwise | A scissor is rectangular only; arbitrary window masks may need a fallback |
| Mosaic | Quantised UVs or pre-processed tiles | Sample origin and grouping must match the original; prototype it |
| Per-scanline scroll or register changes (H-blank effects) | Merge identical runs into a few scissored strips with per-run UVs | Drawing one strip per scanline (192) costs more than the CPU path it replaces |
| Alpha blending and brightness | Combiner constants, vertex colour, target blending in a proven order | Coefficient clamping and target masks need exact test cases |
| Capture, bitmap and other unusual modes | A separate proven path, or the software renderer | Do not reinterpret them as ordinary background quads |

Build such a renderer in steps: start with one narrow subset that you can prove exact against the software renderer (for example opaque scrolling text backgrounds), send everything else to the software path, and log one reason code per scene for each fallback. A hybrid that mixes CPU and GPU layers is safe only where both agree on layer order and alpha; otherwise fall back for the whole screen. Keep the software renderer: it is the oracle and the fallback.

Verify such a path by comparing both renderers on the same captured state, then on an Old 3DS and a New 3DS; an emulator pass is not enough ([testing on hardware](testing-on-hardware.md)).

## Pixel-perfect scaling

- The top screen is 400×240, the bottom 320×240. Original art designed for 256×192 (DS) or 320×240 can be shown 1:1, centred, or scaled by the GPU with filtering.
- For crisp scaling, render the game at its native resolution into a texture, then draw that texture to the screen with nearest-neighbour filtering at an integer factor, or with linear filtering for a smoother non-integer fit. The render-to-texture orientation caveat above applies.
- **Wide mode** (800×240 on the top screen; see the Conflict note on the 2DS in [section 2](#wide-mode)) doubles horizontal resolution for sharper 2D at twice the fill cost; it cannot be combined with stereoscopic 3D ([GPU rendering](gpu-rendering.md#stereoscopic-3d-and-wide-mode)).

## Troubleshooting

| Symptom | Likely cause | Check |
| ------- | ------------ | ----- |
| Black screen, nothing drawn | No `C2D_SceneBegin(target)` before drawing, drawing outside `C3D_FrameBegin`/`C3D_FrameEnd`, or `C2D_CreateScreenTarget` returned `NULL` | Check every return value in a debug build; print the target pointers on the other screen |
| Sprites missing late in the frame, HUD gone first | Object limit reached | Check `C2D_Draw*` for `false`; raise `C2D_Init` size or cull ([object limit](#the-object-limit-and-how-batching-works)) |
| Textured quad shows noise, stripes or 8×8 blocks scrambled | Pixels not swizzled to the tiled layout | Use `tex3ds` output or `swizzleRGBA8`; compare against a known-good `.t3x` |
| Texture is white, black or stale, sometimes only after a while | Missing `C3D_TexFlush` after writing a linear texture, or `C3D_TexInit` failed and you used the zeroed struct | Check the `C3D_TexInit` return value; flush before the frame that samples it |
| Image upside down | V convention: v = 1 is the top of the stored image; hand-written subtexture with top/bottom swapped, or a render-to-texture layer | Use the formulas in [section 3](#generating-a-texture-in-code); for a cached layer try `CACHE_FLIP_V` |
| Image drawn rotated a quarter turn | A subtexture with `top < bottom` is treated as rotated | Fix the subtexture; do not flip with it |
| Red and blue swapped, wrong alpha | Mixed up `0xRRGGBBAA` (texels) with `C2D_Color32` (`0xAABBGGRR`) | Use `C2D_Color32` only for citro2d colours |
| Shimmering lines or seams between tiles | Non-integer positions, bilinear filtering, no atlas border | Whole-pixel positions, `GPU_NEAREST`, `--border edge` |
| Transparent corners of a nearer sprite cut holes in a farther one | Transparent texels write depth (no alpha test) | Sort back to front with one z, or enable `C3D_AlphaTest` |
| Text garbled or missing after updating a string | `C2D_TextBufClear` invalidated other texts in the same buffer, or a `C2D_Text` was drawn after its buffer was deleted | One buffer per update rate; delete buffers after the last draw |
| Touch cursor stuck at the top-left | Used `px/py` when nothing touches | Test `KEY_TOUCH` first |
| Bottom screen shows an old image | Target not drawn this frame (by design) | Call `C2D_SceneBegin(bottom)` and draw when it must change |
| Crash or hang on exit | Render target deleted after `C3D_Fini`, texture freed while a render target still uses it, or buffers freed in the wrong order | Follow the [teardown order](#teardown-in-this-order) |
| 30 fps with little work | CPU work plus GPU work exceeds 16.7 ms, so `C3D_FRAME_SYNCDRAW` waits for the next vblank | Time your phases and `C3D_FrameEnd` separately ([tile renderer timing](2d-tile-renderer.md#per-phase-timing)) |
| Wide mode looks stretched or fails | Enabled after the target was created, wrong width, or running on a Nintendo 2DS | Call `gfxSetWide(true)` first; lay out for 800 px |
| Stereo layers at the wrong depth | Offsets not scaled by the slider, or the sign reversed between eyes | `osGet3DSliderState()` every frame; opposite signs per eye |

## Technical details

- `C2D_SpriteSheetLoad` (also `…LoadFromMem`, `…FromFD`, `…LoadFromHandle`), `C2D_SpriteSheetCount`, `C2D_SpriteSheetGetImage`, `C2D_SpriteSheetFree`; `C2D_Sprite` holds a `C2D_Image` plus `C2D_DrawParams`.
- `C2D_TextBufNew(maxGlyphs)`, `C2D_TextParse` / `C2D_TextFontParse`, `C2D_TextOptimize`, `C2D_TextGetDimensions`, `C2D_DrawText(text, flags, x, y, z, scaleX, scaleY, …)` with flags `C2D_AtBaseline`, `C2D_WithColor`, `C2D_AlignLeft/Right/Center/Justified`, `C2D_WordWrap` (extra arguments follow in that order: colour, then wrap width).
- Texture wrap modes: `GPU_CLAMP_TO_EDGE`, `GPU_CLAMP_TO_BORDER`, `GPU_REPEAT`, `GPU_MIRRORED_REPEAT`; filters `GPU_NEAREST`, `GPU_LINEAR`.
- citro3d defaults set in `C3D_Init` (`base.c`): depth map scale −1, offset 0; depth test on with `GPU_GREATER`; alpha test off; blending `GPU_BLEND_ADD` with `GPU_SRC_ALPHA`/`GPU_ONE_MINUS_SRC_ALPHA`; cull back-facing. `C2D_Prepare` then sets the depth test to `GPU_GEQUAL` `[reverse-engineered]`.
- Installed-library facts used on this page, all from `arm-none-eabi-objdump -d` of `base.o` in `libcitro2d.a` 1.7.0 (the wiki author's reading, repeatable with `arm-none-eabi-ar x` and `objdump`): `C2D_CreateScreenTarget` (240 × 400/800/320, `GPU_RB_RGBA8`, depth format value 0 = 16-bit, transfer flags `0x1000`); `C2D_SceneSize` (orthographic projection, near 1, far −1, left-handed, tilt variant when `tilt`); `C2D_Prepare` (`C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL)`); `C2D_Init` (128 bytes × n vertices, 12 bytes × n indices); `C2D_TargetClear` (draw pending, `C3D_FrameSplit(0)`, clear colour and depth); `C2D_DrawRectangle` (needs 4 vertex and 6 index slots free).
- Per-eye parallax and the slider: `osGet3DSliderState()` returns 0.0 to 1.0 ([libctru](../homebrew/libctru.md)).

## Verification status

| Code | Where | Builds | Emulator | Old 3DS | New 3DS |
| ---- | ----- | ------ | -------- | ------- | ------- |
| Sprite demo (section 5) | this page | yes (`-Wall -Wextra`, linked to ELF) | not run | not run | not run |
| Swizzle function | this page | yes (inside both programs); algorithm checked against `tex3ds -r` output on the host | not run | not run | not run |
| Setup/teardown and frame-loop fragments | this page | yes (the same calls are used in both programs) | not run | not run | not run |
| Manual render-target and stereo-eye fragments | this page | yes (compiled inside a stub `main`, `-Wall -Wextra`, linked) | not run | not run | not run |

The tile renderer (30 configurations: four strategies, texture, shape, bottom-screen, edit and scripted-run modes) builds and has not been run; its rows are on [its page](2d-tile-renderer.md#verification-status). The validation checklist above is a plan; all of its rows are `not run`.

## Pitfalls

- **One texture per sprite.** Every sprite becomes its own batch.
- **Drawing the whole tilemap every frame**, including off-screen tiles.
- **Uploading a CPU-drawn full-screen image every frame** when only a few tiles changed.
- **Non-integer positions for pixel art**: shimmering and seams.
- **Exhausting citro2d's object limit**: sprites silently missing late in the frame.
- **`C2D_Color32` is not the texel order**: colours for citro2d calls are `0xAABBGGRR`; RGBA8 texels are `0xRRGGBBAA`.
- **`C2D_SpriteFromImage` resets everything** (position, size, centre, angle, depth), so call it before `SetCenter`/`SetPos`/`Scale`, not after.
- **Deleting what `C3D_Fini` deletes**: the screen render targets.
- **Trusting emulator timings**: they are never results ([benchmarks](../benchmarks/README.md)).

## Open questions

- Measured crossovers: manual vertex buffer against `C2D_DrawImageAt` by visible-tile count (E1), cached background against direct tiles (E2), the penalty for alternating textures (E3), chunk size (E4). All specified in the [tile renderer](2d-tile-renderer.md#experiments); none run. Geometry-shader sprite expansion ([gpusprites](https://github.com/devkitPro/3ds-examples/tree/master/graphics/gpu/gpusprites)) is not in the comparison.
- Fill-rate cost of full-screen translucent layers on Old 3DS, and the P1 to P3 experiments (partial texture updates, palette effects, cached text).
- Orientation of a render-to-texture layer when drawn with a standard subtexture, and the scissor rectangle's mapping to rotated screens: both need one run on hardware.
- Which sign of the per-eye offset moves a 2D layer in front of the screen plane.
- Whether the four tile strategies draw identical pixels, and how a 2D engine behaves across HOME and sleep: the [validation checklist](#validation-checklist-for-a-2d-engine), every row `not run`.
- Whether citro2d's batch really ends on each of the triggers listed above: confirm by cloning citro2d (`source/base.c`) at the 1.7.0 tag and reading `C2Di_Update`.

## Sources

- [citro2d](https://github.com/devkitPro/citro2d) — `include/c2d/base.h`, `sprite.h`, `spritesheet.h`, `text.h`, `font.h` (installed 1.7.0 headers read locally); `source/base.c` (batching; not cloned here, behaviour read from the compiled `libcitro2d.a`).
- [citro3d](https://github.com/devkitPro/citro3d) — `include/c3d/texture.h`, `renderqueue.h`, `effect.h`; `source/texture.c` (size limit, formats, upload), `renderqueue.c` (frame functions, targets), `base.c` (defaults, scissor), `maths/mtx_ortho.c` (depth range), `framebuffer.c` (clear).
- [libctru](https://github.com/devkitPro/libctru) — `gfx.h` (wide mode, rotated screens, 3D), `hid.h` (`hidTouchRead`), `gpu/enums.h` (wrap modes, scissor modes, formats).
- [3ds-examples](https://github.com/devkitPro/3ds-examples) — `graphics/gpu/stereoscopic_2d`, `composite_scene`, `wide_mode_3d`, `gpusprites`, `2d_shapes` (its comment on grouping circles was read locally for the shape-batching row), `graphics/printing/system-font`, `custom-font`.
- [tex3ds](https://github.com/devkitPro/tex3ds) 2.3.0 — `tex3ds --help` and test conversions run with the installed binary (formats, long options, raw layout, subtexture values).
- [3dbrew: GPU/Internal Registers](https://www.3dbrew.org/wiki/GPU/Internal_Registers), [GPU/External Registers](https://www.3dbrew.org/wiki/GPU/External_Registers), [GPU/Pitfalls](https://www.3dbrew.org/wiki/GPU/Pitfalls) — texture size fields and format list, framebuffer orientation, L4-in-VRAM warning (local mirror).
- Local review notes `coffeepot*.md` and the HGSS implementer's guides — the porting cautions (a single project's notes; not published).
