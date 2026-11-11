# 2D rendering on the PICA200

The 3DS has no 2D graphics hardware of its own for native software: unlike the DS, there are no tile backgrounds, sprite engine or palettes. Everything on screen — tilemaps, sprites, text, UI — is drawn by the 3D GPU (the **[PICA200](../glossary.md#pica200)**) as textured rectangles (quads), using **[tiled](../../glossary.md#tiling)** textures and the **[TEV](../glossary.md#tev)** colour combiners for effects. Done well, that is fast and flexible: the GPU scales, rotates, tints and blends sprites for free. Done naïvely — one draw call per sprite, a texture change per tile, or a CPU-drawn framebuffer uploaded every frame — it wastes most of the frame. This page shows how to build the usual 2D layers from quads, with citro2d where it fits and plain citro3d where it does not.

Background: [GPU rendering](gpu-rendering.md) (draw-call costs, combiners, citro2d batching), [asset pipeline](asset-pipeline.md) (atlases and formats).

## Before you start

- Checked against citro2d 1.7.0, citro3d 1.7.1 and libctru 2.7.1 headers and sources. API names are real; the techniques are standard GPU practice applied to this hardware, labelled where a performance claim has not been measured on a 3DS. Function names and signatures (`C2D_DrawText`, `C2D_Fade`, `C2D_SetTintMode`, `C2D_ViewTranslate`, `C2D_TextBufClear`, `C2D_DEFAULT_MAX_OBJECTS`, ...) were re-read in the installed citro2d 1.7.0 headers; the batching rules come from citro2d `source/base.c` on upstream `master`, which is not cloned locally. Measure your own scenes with [drawbench](../benchmarks/README.md#drawbench) and the [frame breakdown](performance.md#a-frame-level-breakdown).

## The rules that matter

1. **Few textures, many quads.** Put everything drawn together into one atlas, so that one draw call (citro2d does this automatically within a run of the same texture and state) covers hundreds of quads ([citro2d batching](gpu-rendering.md#2d-games-citro2d)).
2. **Do not redraw what did not change.** Static layers can be drawn once into a texture and reused ([render to texture](gpu-rendering.md#render-to-texture)).
3. **Move, don't rebuild.** Scrolling and camera motion should change a transform or texture coordinates, not regenerate geometry or pixels.
4. **No palettes on the GPU.** Convert palettised art to direct-colour textures; handle palette effects another way (below).

## Sprites

With citro2d:

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
- Tinting (`C2D_ImageTint`, `C2D_SetTintMode` with `C2D_TintSolid`, `C2D_TintMult`, `C2D_TintLuma`) and whole-scene fades (`C2D_Fade`) are done by the combiners at no texture cost, but changing them ends a batch.
- Depth: citro2d draws accept a depth value. With depth testing, opaque sprites can be drawn in any order (front to back is best, it lets the GPU reject hidden pixels); semi-transparent sprites must still be drawn back to front.

For very large sprite counts beyond citro2d's limits, write quads into your own vertex buffer and draw them with one `C3D_DrawArrays`/`C3D_DrawElements` per texture, or expand points into quads in a geometry shader ([`graphics/gpu/gpusprites`](https://github.com/devkitPro/3ds-examples/tree/master/graphics/gpu/gpusprites) shows the latter). Which approach is fastest for which count has not been measured here; [drawbench](../benchmarks/README.md#drawbench) measures the per-draw costs these trade-offs depend on.

## Tilemaps and scrolling backgrounds

A tile layer is a grid of quads whose texture coordinates point at tiles in an atlas.

- **Build the visible part once, scroll by transform.** Generate quads for the visible area plus one tile of margin in a static vertex buffer; scroll by changing the view offset (`C2D_ViewTranslate`, or a translation uniform in your own shader). Rebuild only when the camera crosses a tile boundary — and then only the new row or column, written into a ring of rows/columns in the buffer.
- **Big, static maps**: draw the whole layer once into a large texture (up to 1024×1024) or a set of chunk textures with render-to-texture, then draw one quad per chunk each frame. Costs memory, saves almost all per-frame work [technique; not measured here].
- **Animated tiles** (water, conveyor belts): change the texture coordinates in a few vertices, or keep animation frames side by side in the atlas and offset the UVs with a uniform for all tiles of that kind.
- **Seams**: with bilinear filtering or non-integer scroll positions, neighbouring atlas tiles bleed into each other. Use `tex3ds -b edge` padding, nearest-neighbour filtering (`C3D_TexSetFilter(tex, GPU_NEAREST, GPU_NEAREST)`), and integer scroll positions for pixel art.
- **Repeating backgrounds** (sky, parallax layers): a single quad with `C3D_TexSetWrap(tex, GPU_REPEAT, GPU_REPEAT)` and scrolled texture coordinates draws an endless layer. Repeat requires the texture to be a standalone power-of-two texture, not a sub-image of an atlas.

## Layers and composition

- Draw layers back to front: far backgrounds, tile layers, sprites, foreground, UI. Opaque layers can instead use the depth buffer and be drawn front to back to save fill.
- Each layer that covers the whole 400×240 screen costs fill rate; stacked full-screen semi-transparent layers multiply it. Keep translucent full-screen overlays to a minimum and prefer small quads [general GPU behaviour; the PICA200's fill rate has not been measured here].
- The bottom screen is a separate render target. If its content rarely changes (maps, menus), redraw it only when it changes and leave the previous framebuffer on screen — citro3d swaps a screen only if its target was drawn that frame ([GPU rendering](gpu-rendering.md#technical-details)).
- **Masks and windows**: rectangular clipping with `C3D_SetScissor`; arbitrary shapes with the stencil buffer (`C3D_StencilTest`, `C3D_StencilOp`) — draw the mask shape into stencil, then draw the masked layer with a stencil test.

## Transparency

| Need | Use | Cost |
| ---- | --- | ---- |
| Cut-out edges (pixel art, foliage) | `RGBA5551` or alpha-tested textures; `C3D_AlphaTest(true, GPU_GREATER, 0x80)` | No blending, any draw order |
| Soft edges, glow, shadows | Alpha blending (`C3D_AlphaBlend`, citro2d default) | Back-to-front order required |
| Additive light, particles | Additive blend (`GPU_BLEND_ADD` with `GPU_SRC_ALPHA`, `GPU_ONE`) | Order-independent |

## Text

citro2d renders text from the system font or a converted BCFNT font ([asset pipeline](asset-pipeline.md#fonts)):

```c
C2D_TextBuf buf = C2D_TextBufNew(4096);          // glyph capacity
C2D_Text label;
C2D_TextParse(&label, buf, "Score: 0");
C2D_TextOptimize(&label);                         // once, after parsing
/* each frame */
C2D_DrawText(&label, C2D_WithColor | C2D_AlignCenter, 200, 20, 0.5f, 0.6f, 0.6f, C2D_Color32(255, 255, 255, 255));
```

- Parse static strings once; re-parse only text that changed (call `C2D_TextBufClear` on a dedicated buffer for dynamic text each frame).
- Fonts store glyphs on several texture sheets, so a string can need several texture changes; `C2D_TextOptimize` is documented as optimising a text object "in order to be drawn more efficiently" — call it once after parsing static text.

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
- For crisp scaling, render the game at its native resolution into a texture, then draw that texture to the screen with nearest-neighbour filtering at an integer factor, or with linear filtering for a smoother non-integer fit.
- **Wide mode** (800×240 on the top screen, not on the 2DS) doubles horizontal resolution for sharper 2D at twice the fill cost; it cannot be combined with stereoscopic 3D ([GPU rendering](gpu-rendering.md#stereoscopic-3d-and-wide-mode)).

## Technical details

- `C2D_SpriteSheetLoad` (from a file path, also `…LoadFromMem`, `…LoadFromHandle`), `C2D_SpriteSheetCount`, `C2D_SpriteSheetGetImage`; `C2D_Sprite` holds an image plus position, scale, rotation and centre.
- `C2D_TextBufNew(maxGlyphs)`, `C2D_TextParse` / `C2D_TextFontParse` (with a custom `C2D_Font`), `C2D_TextOptimize`, `C2D_DrawText(text, flags, x, y, z, scaleX, scaleY, …)` with flags such as `C2D_WithColor`, `C2D_AlignCenter`, `C2D_AlignRight`, `C2D_WordWrap` (extra arguments follow in that order).
- Texture wrap modes: `GPU_CLAMP_TO_EDGE`, `GPU_CLAMP_TO_BORDER`, `GPU_REPEAT`, `GPU_MIRRORED_REPEAT`.
- citro2d ends its current batch on texture, blend mode, tint, fade or view changes ([GPU rendering](gpu-rendering.md#2d-games-citro2d)).

## Pitfalls

- **One texture per sprite.** Every sprite becomes its own batch.
- **Drawing the whole tilemap every frame**, including off-screen tiles.
- **Uploading a CPU-drawn full-screen image every frame** when only a few tiles changed.
- **Non-integer positions for pixel art**: shimmering and seams.
- **Exhausting citro2d's object limit**: sprites silently missing late in the frame.

## Open questions

- Measured quads per frame at 60 fps on Old 3DS for: citro2d sprites, a manual single-buffer batch, geometry-shader expansion — the comparison the review of this wiki asked for. [drawbench](../benchmarks/README.md#drawbench) is the starting point.
- Fill-rate cost of full-screen translucent layers on Old 3DS.

## Sources

- [citro2d](https://github.com/devkitPro/citro2d) — `include/c2d/base.h`, `sprite.h`, `spritesheet.h`, `text.h`; `source/base.c` (batching).
- [citro3d](https://github.com/devkitPro/citro3d) — `include/c3d/texture.h` (filters, wrap), `effect.h` (alpha test, blending, stencil, scissor), `renderqueue.c` (per-screen swap).
- [libctru](https://github.com/devkitPro/libctru) — `gpu/enums.h` (wrap modes, blend factors), `gfx.h` (wide mode).
- [3ds-examples](https://github.com/devkitPro/3ds-examples) — `graphics/gpu/gpusprites`, `graphics/gpu/2d_shapes`, `graphics/printing/*`, `graphics/gpu/composite_scene`.
