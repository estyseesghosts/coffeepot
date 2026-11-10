# CTR GPU

The 3DS draws its graphics with a chip licensed from a small company, Digital Media Professionals (DMP): the **[PICA200](../glossary.md#pica200)**. It is a mid-point between the old fixed-function style (fixed circuits for lighting and texture blending) and modern programmable graphics (shaders for geometry). Games feed it lists of commands; it draws three images per frame — a left-eye and right-eye view for the top screen, and one for the bottom screen.

The PICA200 only works with [framebuffers](../../glossary.md#framebuffer): there are no hardware sprites or background layers as on the DS. For DS and GBA games, a separate legacy path produces the picture instead (see [displays](display.md)).

## Where it fits

The [ARM11 CPU](cpu.md) builds a **command list** and a **vertex buffer** in [memory](memory.md) and tells the GPU to run it, usually through the `gsp::Gpu` [system service](../software/services.md). The GPU renders into a buffer in [VRAM](../glossary.md#vram) or main memory; the result is then copied into an *LCD framebuffer* that the [display hardware](display.md) scans out. Audio has its own processor ([audio](audio.md)).

## How it works

### The pipeline in plain terms

1. **Commands.** The GPU reads a buffer of register writes. Vertex data can be embedded in the buffer or, much more efficiently, kept in a separate array in VRAM.
2. **Vertex stage.** Small programs ([shaders](../../glossary.md#shader)) transform every corner of every shape. There are four shader processors; one can be turned into a **geometry shader** that creates new shapes from the others' output, which leaves three for vertex work.
3. **Rasteriser.** Shapes are converted to pixels, with culling and clipping.
4. **Fragment stage.** Four texture units fetch textures; six *colour combiners* blend texture, vertex colour and constants in a chain. This stage is **not** programmable; instead DMP added fixed "Maestro" features (per-pixel lighting, shadows, bump mapping, fog, procedural textures).
5. **Post-processing.** Alpha, stencil and depth tests, then blending into the render target, optionally with 2× supersampling anti-aliasing.
6. **Copy-out.** A separate "transfer engine" converts the tiled render target into the linear LCD framebuffer format (and can downscale for anti-aliasing).

### What games must do themselves

The GPU is not stereoscopic-aware. A game renders the top screen twice (slightly shifted cameras), then the bottom screen. Games manage buffer placement in the 6 MB of VRAM themselves; a DMA unit moves data between VRAM and FCRAM. The official SDK offers four graphics layers, from simple to bare-metal: GL (OpenGL ES style, slowest), GD (faster, emits PICA commands), GR (closest to metal) and GX (general GPU management).

## Technical details

### Summary

| Item | Value |
| ---- | ----- |
| Chip | DMP PICA200 ("Maestro-2G" architecture), 268 MHz |
| API compatibility | OpenGL ES 1.1 with extras; some OpenGL ES 2.0 ideas |
| Vertex processors | 4 (3 vertex + 1 "Primitive Engine"/geometry unit when enabled) |
| Shader number format | 24-bit floats (not IEEE 754), 4-component vectors (96 bits) |
| Shader program limits | ≤ 512 instructions, ≤ 128 operand descriptors (inferred from SDK debug data) |
| Vertex output cache | "Sync control" block, 384 bytes (32 × 96-bit vectors) |
| Texture units | 4: three can do 2D textures, one of those can also do shadow, cube and projective mapping, and the fourth is used only for procedural textures |
| Texture cache | 256 B L1 per unit, 8 KB shared L2 |
| Colour combiners | 6 (each takes 3 inputs: previous combiner, texture unit or constant) |
| Memory | 6 MB VRAM + FCRAM; on-chip DMA between them |
| Bus | Open Core Protocol (OCP) on the GPU side, bridged to AXI |
| Output | 2 × 400×240 (top left/right eye), 1 × 320×240 (bottom); 8-bit-per-channel colour |

> Discrepancy: Copetti says the shader instruction set resembles Microsoft's `vs_2_0`; 3dbrew says it was heavily inspired by `vs_3_0`/Direct3D shader code. Both agree it is Direct3D-shader-like.

### Maestro features (DMP extensions)

| Name | Capability |
| ---- | ---------- |
| Lighting Maestro | Per-fragment lighting through lookup tables; Blinn-Phong, Cook-Torrance, Ward and microfacet-style models. The vertex shader outputs a quaternion describing the surface frame, interpolated across the triangle |
| Shadow Maestro | Hard and soft shadows |
| Figure Maestro | Polygon subdivision (variable-size primitives via the Primitive Engine) |
| Mapping Maestro | Bump mapping and procedural textures (noise generated from a random source plus a colour lookup table, fed to texture unit 3) |
| Particle Maestro | Rendering of gaseous/particle effects |

### Registers

The GPU has two register spaces.

**External registers** (ARM11 physical `0x10400000`, user VA `0x1EF00000`) control basic operation. Key blocks:

| Offset | Block |
| ------ | ----- |
| `0x000` | Hardware ID (bit 2: new model) |
| `0x010`, `0x020` | Memory fill units PSC0/PSC1 (start/end address ≫ 3, fill value, control: bit 0 start/busy, 1 finished, 8–9 width 16/24/32-bit); VRAM only |
| `0x030` | VRAM bank control |
| `0x034` | GPU busy flags (bit 26 PSC0, 27 PSC1, 30 PPF, 31 P3D) |
| `0x0C0` | Backlight control |
| `0x400`, `0x500` | Display controller registers for the top/bottom LCD ([PDC](../glossary.md#pdc)) |
| `0xC00` | Transfer engine (DisplayTransfer, TextureCopy, tiled↔linear conversion with box-filter downscale) |
| `0x1000–0x1C00` | Window onto the internal registers |
| `0x18E0` | Command-list processor "P3D" (size ≫ 3, address ≫ 3, bit 0 start) |

**Internal registers** (`GPUREG_*`, `0x000–0x2FF`) are written through the command list, not directly. They are grouped:

| Range | Group |
| ----- | ----- |
| `0x000–0x03F` | Miscellaneous (e.g. `GPUREG_FINALIZE`) |
| `0x040–0x07F` | Rasterizer (face culling, viewport, clipping) |
| `0x080–0x0FF` | Texturing |
| `0x100–0x13F` | Framebuffer |
| `0x140–0x1FF` | Fragment lighting |
| `0x200–0x27F` | Geometry pipeline (attribute buffers, draw commands) |
| `0x280–0x2DF` | Shader processors |

Draws use `GPUREG_DRAWARRAYS` or `GPUREG_DRAWELEMENTS`. Constant vertex attributes can use *fixed attributes*, and small draws can supply vertices inline by writing `0xF` to `GPUREG_FIXEDATTRIB_INDEX` followed by float data (immediate mode).

### Known pitfalls

- Vertex attributes are aligned to their element size; the stride in `GPUREG_ATTRIBBUFFERi_CONFIG2` must exactly equal the real vertex size (use padding attributes) or the GPU hangs or draws nothing.
- Output map registers (`GPUREG_SH_OUTMAP_O*`) consume slots only for outputs enabled in `GPUREG_SH_OUTMAP_MASK`.
- Texture unit 1 can corrupt or become unstable in some configurations.
- Shader outputs configured must be written exactly once; `MOVA` (address-register) instructions cannot be adjacent.

## Homebrew notes

Homebrew normally uses `citro3d` (state-tracking wrapper) and `citro2d`, with shaders written in `picasso` assembly (see [development workflow](../homebrew/development-workflow.md)). Emulator work needs the register-level pages on 3dbrew.

## Open questions

- Primitive Engine and variable-size primitives are only partly reverse-engineered; how they are enabled is unknown.
- Which texture unit supports shadow/cube/projective mapping is not stated in the sources used.
- Unknown registers in the external map ("writes `0x22221200` on init", etc.) are undocumented.

## Sources

- [3dbrew: GPU](https://www.3dbrew.org/wiki/GPU) and subpages: [External Registers](https://www.3dbrew.org/wiki/GPU/External_Registers), [Internal Registers](https://www.3dbrew.org/wiki/GPU/Internal_Registers), [Programming Guide](https://www.3dbrew.org/wiki/GPU/Programming_Guide), [Pitfalls](https://www.3dbrew.org/wiki/GPU/Pitfalls), [Primitive Engine](https://www.3dbrew.org/wiki/GPU/Primitive_Engine), [Shader Instruction Set](https://www.3dbrew.org/wiki/GPU/Shader_Instruction_Set), [Fragment Lighting](https://www.3dbrew.org/wiki/GPU/Fragment_Lighting), [Procedural Texture Generation](https://www.3dbrew.org/wiki/GPU/Procedural_Texture_Generation) — registers, shaders, lighting, pitfalls.
- [3dbrew: Hardware](https://www.3dbrew.org/wiki/Hardware) — chip identification and Maestro feature list.
- [Rodrigo Copetti, Nintendo 3DS Architecture: Graphics](https://www.copetti.org/writings/consoles/nintendo-3ds/) — pipeline walk-through, cache and combiner counts, SDK graphics layers.
