# CTR GPU internal registers (field reference)

The 3DS [GPU](gpu.md) is controlled by about 700 small "registers" that are never touched directly by games. Instead a program records register writes in a **[command list](../glossary.md#command-list)** and the GPU replays them. This page is the lookup table for what each of those internal registers does, bit by bit: culling, textures, colour combiners, blending, lighting, vertex input and the shader processors. It is meant for people writing drivers, emulators, command-list tools or low-level graphics code. Most homebrew never needs it, because `citro3d` writes these registers for you.

Everything here is a **register of the PICA200**, the GPU in the Old 3DS and the New 3DS alike. The sources do not say on which console model the behaviours were observed; see the unknown below.

> Unknown: 3dbrew's GPU register pages do not state which console model each behaviour was tested on. Nothing in them marks a register as New 3DS-specific, and the external register `0x10400000` "new model" bit ([external registers](gpu-external-registers.md#map)) is the only model flag the pages mention. Running a register test on an Old 3DS and a New 3DS would settle it.

## Where it fits

The ARM11 CPU writes commands into a buffer in memory; the GPU's command-list processor (P3D, at physical `0x104018E0`, see [external registers](gpu-external-registers.md#command-list-p3d)) reads them and updates these registers. How `citro3d` produces the command stream is on [PICA200 command pipeline](../engineering/pica200-command-pipeline.md); the high-level meaning of the blocks is on [GPU](gpu.md) and [GPU shading features](gpu-shading-features.md).

## How it works

A register write is a **parameter word followed by a header word**. The header holds the register id, a 4-bit byte-lane mask, a count of extra parameters and a "consecutive writing" flag. With the flag set, extra parameters go to the following register ids; with it clear, they are all written to the same register (which is how the data registers act as queues). Several ids can alias one register, so one command can start at an index register and stream into its data registers. A write to `GPUREG_FINALIZE` (id `0x010`) ends the list; without it the GPU hangs.

There are three kinds of register: **configuration** registers (a rendering property), **data transfer** registers (queues for shader code, uniforms and look-up tables) and **action** registers (writing them does something, such as drawing).

## Technical details

### Command header

| Bits | Meaning |
| ---- | ------- |
| 0-15 | Command (register) id; libctru masks it to bits 0-9, see the [conflict](../engineering/pica200-command-pipeline.md#how-a-write-is-encoded) |
| 16-19 | Parameter mask; bit 16+k = 1 lets byte k of the word change. `0xF` writes the whole word. Commands `0x00010107` and `0x00020107` hit the same register but write different bytes |
| 20-27 | Number of extra parameters (may be zero) |
| 28-30 | Unused |
| 31 | Consecutive writing mode |

Example from 3dbrew: `0xAAAAAAAA 0x802F011C 0xBBBBBBBB 0xCCCCCCCC` writes `0xAAAAAAAA` to `0x11C`, `0xBBBBBBBB` to `0x11D` and `0xCCCCCCCC` to `0x11E`. With the consecutive flag clear, all three would go to `0x11C`. Rules from 3dbrew: a command is at least 8 bytes; the buffer size must be 16-byte aligned (the low 3 bits of the size are cleared), and a `FINALIZE` that ends up in the last 8 bytes of a non-16-byte-aligned buffer is not executed, which hangs the GPU; invalid parameters including NaN floats can hang the GPU, and that hangs the GSP module too `[reverse-engineered: 3dbrew]`.

Number types used below: `floatS.E.M` = S sign bits, E exponent bits, M mantissa bits (for example `float1.7.16` is the 24-bit shader float, `float1.5.10` is a 16-bit float); `fixedS.I.F` = S sign, I integer, F fraction bits.

### Register map by group

| Ids | Group |
| --- | ----- |
| `0x000-0x03F` | Miscellaneous (only `FINALIZE` is named) |
| `0x040-0x07F` | Rasterizer |
| `0x080-0x0FF` | Texturing, procedural texture, colour combiners (TexEnv), fog |
| `0x100-0x13F` | Framebuffer, gas, shadow |
| `0x140-0x1FF` | Fragment lighting |
| `0x200-0x27F` | Geometry pipeline (vertex arrays, draw triggers, command-buffer jumps) |
| `0x280-0x2DF` | Shader units: geometry shader `0x280-0x2AD`, vertex shader `0x2B0-0x2DD` |
| `0x2E0-0x2FF` | Unknown |

Names below follow 3dbrew. Where libctru's `registers.h` uses another name or none, it is noted; the [decoder tool](../engineering/pica200-command-pipeline.md#host-side-command-list-decoder) prints the libctru names plus the extra ids listed under "Names that differ".

### Miscellaneous and rasterizer registers (`0x010`, `0x040-0x06F`)

| Id | Register | Fields |
| -- | -------- | ------ |
| `0x010` | `FINALIZE` | Any non-zero value triggers the P3D interrupt and stops command processing; commands after it are ignored. Commercial software writes `0x12345678`. Missing in a list: GPU hangs |
| `0x040` | `FACECULLING_CONFIG` | bits 0-1 mode: 0 none, 1 cull front (counter-clockwise), 2 cull back (counter-clockwise) |
| `0x041` | `VIEWPORT_WIDTH` | bits 0-23 `float1.7.16`: width / 2 |
| `0x042` | `VIEWPORT_INVW` | bits 1-31 `float1.7.23`: 2 / width |
| `0x043`, `0x044` | `VIEWPORT_HEIGHT`, `VIEWPORT_INVH` | same for height |
| `0x047` | `FRAGOP_CLIP` | bit 0 enable clipping planes |
| `0x048-0x04B` | `FRAGOP_CLIP_DATA0-3` | bits 0-23 `float1.7.16` plane coefficient |
| `0x04C` | `DEPTHMAP_TRANSFORM_ENABLE` | bit 0. On: depth = `DEPTHMAP_SCALE * z / w + DEPTHMAP_OFFSET`. Off: scale and offset ignored, depth = `-z / w` (scale -1.0, offset 0.0; polygon offset is lost). In both cases multiplied by clip-space w when W-buffering is used. libctru name `GPUREG_004C` |
| `0x04D` | `DEPTHMAP_SCALE` | `float1.7.16`: Near - Far |
| `0x04E` | `DEPTHMAP_OFFSET` | `float1.7.16`: Near + polygon offset |
| `0x04F` | `SH_OUTMAP_TOTAL` | bits 0-2: number of following output map registers |
| `0x050-0x056` | `SH_OUTMAP_O0-O6` | four 5-bit semantic fields per register (bits 0-4 x, 8-12 y, 16-20 z, 24-28 w); see [semantics](#shader-output-semantics) |
| `0x058` | `FLAT_SHADING` | bit 0: use the last vertex's colour (including alpha) for the whole triangle in all modes. Bit 8: no known effect |
| `0x060` | `RASTER_PIXEL_MASK` | bits 8-9 mode, per 2x2 pixel block (even coordinates): 0 all, 1 even X, 2 even X and odd Y, 3 all. Mode 2 misbehaves when `TEXUNIT_LOD_ENABLE` is 1 (most fragments dropped, then written during later draws with the same configuration). Bit 0: no known effect |
| `0x061` | `EARLYDEPTH_FUNC` | bits 0-1: 0 `>=`, 1 `>`, 2 `<=`, 3 `<` |
| `0x062` | `EARLYDEPTH_TEST1` | bit 0 enable |
| `0x063` | `EARLYDEPTH_CLEAR` | bit 0: write 1 to clear the early-depth data |
| `0x064` | `TEXUNIT_LOD_ENABLE` | bit 0: enable level-of-detail calculation for units 0-2. On: level = round(log2(texels per pixel)) + bias, minification filter used when minified. Off: level 0, the bias in `TEXUNITi_LOD` is ignored (min level still applies), magnification filter always used. libctru name `SH_OUTATTR_MODE` |
| `0x065` | `SCISSORTEST_MODE` | bits 0-1: 0 disabled, 3 enabled |
| `0x066`, `0x067` | `SCISSORTEST_POS`, `_DIM` | bits 0-9 X1 / X2, bits 16-25 Y1 / Y2 |
| `0x068` | `VIEWPORT_XY` | bits 0-9 signed X, bits 16-25 signed Y |
| `0x06A` | `EARLYDEPTH_DATA` | bits 0-23 clear value |
| `0x06D` | `DEPTHMAP_MODE` | bit 0: 0 = W-buffer, 1 = Z-buffer. libctru name `DEPTHMAP_ENABLE` |
| `0x06E` | `RENDERBUF_DIM` | bits 0-10 width, bits 12-21 height - 1, bit 24 = 1 |
| `0x06F` | `SH_OUTATTR_CLOCK` | clock gating per attribute: bit 0 position.z, 1 colour, 8/9/10 texcoord0/1/2, 16 texcoord0.w, 24 normquat or view present |

#### Shader output semantics

Each 5-bit field of `SH_OUTMAP_Oi` maps one component of a shader output register to a fixed pipeline input. Unmapped semantics default to 1. `0x1F` marks an unused component.

| Value | Semantic | Value | Semantic |
| ----- | -------- | ----- | -------- |
| `0x00-0x03` | position x, y, z, w | `0x10` | texcoord0.w |
| `0x04-0x07` | normquat x, y, z, w (quaternion for fragment lighting) | `0x12-0x14` | view x, y, z (fragment lighting) |
| `0x08-0x0B` | colour r, g, b, a | `0x16`, `0x17` | texcoord2 u, v |
| `0x0C`, `0x0D` | texcoord0 u, v | `0x1F` | unused component |
| `0x0E`, `0x0F` | texcoord1 u, v | | |

Worked example (3dbrew Pitfalls): `OUTMAP_TOTAL = 2`, `OUTMAP_MASK = 0x11` (o0 and o4 enabled), `OUTMAP_O0 = 0x03020100` (o0 = position xyzw), `OUTMAP_O1 = 0x0B0A0908` (o4 = colour rgba). A disabled output takes no slot in the `OUTMAP_Oi` registers.

### Texturing registers (`0x080-0x0B7`)

Texture units 0-2 are 2D/cube/shadow capable; unit 0 has extra registers (`ADDR2-6`, `SHADOW`, extra `PARAM` bits). Unit `i` base ids: 0 at `0x081`, 1 at `0x091`, 2 at `0x099`. All fields apply per unit unless noted.

| Id | Register | Fields |
| -- | -------- | ------ |
| `0x080` | `TEXUNIT_CONFIG` | bits 0, 1, 2 enable units 0, 1, 2; bit 10 enable unit 3 (procedural); bits 8-9 texcoord source for unit 3 (0 tex0, 1 tex1, 2 tex2); bit 13 texcoord source for unit 2 (0 tex2, 1 tex1); bit 12 = 1; bit 16 clear texture cache |
| unit +0 | `BORDER_COLOR` | bits 0-7 R, 8-15 G, 16-23 B, 24-31 A |
| unit +1 | `DIM` | bits 0-10 height, bits 16-26 width |
| unit +2 | `PARAM` | bit 1 mag filter, bit 2 min filter (0 nearest, 1 linear); bits 4-5 ETC1 (2 = ETC1, still 0 for ETC1A4); bits 8-10 wrap T, 12-14 wrap S (0 clamp to edge, 1 clamp to border, 2 repeat, 3 mirrored repeat); bit 20 shadow (unit 0 only); bit 24 mipmap filter; bits 28-30 type (unit 0 only): 0 2D, 1 cube, 2 shadow 2D, 3 projection, 4 shadow cube, 5 disabled |
| unit +3 | `LOD` | bits 0-12 `fixed1.4.8` bias; bits 16-19 max level; bits 24-27 min level |
| unit +4 | `ADDR1` (units 1, 2: only one) | bits 0-27 physical address >> 3. Unit 0 also has `ADDR2-ADDR6` (bits 0-21, upper 6 bits reused from `ADDR1`). Cube map faces: ADDR1 +X, 2 -X, 3 +Y, 4 -Y, 5 +Z, 6 -Z; addresses must be ordered lowest to highest or faces may not render |
| `0x08B` | `TEXUNIT0_SHADOW` | bit 0 perspective (0 = perspective, 1 = not), bits 1-23 `fixed0.0.24` Z bias |
| `0x08E`, `0x096`, `0x09E` | `TEXUNITi_TYPE` | bits 0-3 format |
| `0x08F` | `LIGHTING_ENABLE0` | bit 0 enable lighting (see also `LIGHTING_ENABLE1` at `0x1C6`, which is set to 1 when lighting is disabled) |

Texels are laid out as a Z-order (Morton) curve, 8x8 tiled; mipmaps follow the base level directly.

| `TYPE` | Format | GL equivalent | `TYPE` | Format | GL equivalent |
| ------ | ------ | ------------- | ------ | ------ | ------------- |
| `0x0` | RGBA8888 | `GL_RGBA`/`UNSIGNED_BYTE` | `0x7` | I8 | `GL_LUMINANCE` |
| `0x1` | RGB888 | `GL_RGB`/`UNSIGNED_BYTE` | `0x8` | A8 | `GL_ALPHA` |
| `0x2` | RGBA5551 | `UNSIGNED_SHORT_5_5_5_1` | `0x9` | IA44 | `GL_LUMINANCE_ALPHA`/`UNSIGNED_BYTE_4_4_EXT` |
| `0x3` | RGB565 | `UNSIGNED_SHORT_5_6_5` | `0xA` | I4 | none |
| `0x4` | RGBA4444 | `UNSIGNED_SHORT_4_4_4_4` | `0xB` | A4 | `GL_ALPHA`/`UNSIGNED_NIBBLE_EXT` |
| `0x5` | IA8 | `GL_LUMINANCE_ALPHA` | `0xC` | ETC1 | `GL_ETC1_RGB8_OES` |
| `0x6` | HILO8 | none | `0xD` | ETC1A4 | none |

(`citro3d` calls IA8/I8/A8/IA44/I4/A4 "LA8/L8/A8/LA4/L4/A4".)

#### Procedural texture registers (texture unit 3)

The model of the feature is on [shading features](gpu-shading-features.md#procedural-textures-texture-unit-3).

| Id | Register | Fields |
| -- | -------- | ------ |
| `0x0A8` | `PROCTEX0` | bits 0-2 U clamp, 3-5 V clamp (0 clamp to zero, 1 clamp to edge, 2 symmetrical repeat, 3 mirrored repeat, 4 pulse); 6-9 RGB mapping function, 10-13 alpha mapping function (0 U, 1 U², 2 V, 3 V², 4 (U+V)/2, 5 (U²+V²)/2, 6 sqrt(U²+V²), 7 min, 8 max, 9 rmax); bit 14 handle alpha separately; bit 15 noise enabled; bits 16-17 U shift, 18-19 V shift (0 none, 1 odd, 2 even); bits 20-27 texture bias low 8 bits (`float1.5.10`) |
| `0x0A9` | `PROCTEX1` | bits 0-15 U noise amplitude `fixed1.3.12`; bits 16-31 U noise phase `float1.5.10` |
| `0x0AA` | `PROCTEX2` | same for V |
| `0x0AB` | `PROCTEX3` | bits 0-15 U noise frequency, 16-31 V noise frequency (`float1.5.10`) |
| `0x0AC` | `PROCTEX4` | bits 0-2 min filter (0 nearest, 1 linear, 2 nearest/mip-nearest, 3 linear/mip-nearest, 4 nearest/mip-linear, 5 linear/mip-linear); bits 3-6 min LOD (usually 0); 7-10 max LOD (usually 6); 11-18 texture width; 19-26 texture bias high 8 bits |
| `0x0AD` | `PROCTEX5` | colour-table offsets: bits 0-7 base level; 8-15 level 1 (usually `0x80`); 16-23 level 2 (usually `0xC0`); 24-31 level 3 (usually `0xE0`). Levels 4-7 appear hard-wired to `0xF0`, `0xF8`, `0xFC`, `0xFE` |
| `0x0AF` | `PROCTEX_LUT` | bits 0-7 start index; bits 8-11 table: 0 noise, 2 RGB mapping, 3 alpha mapping, 4 colour, 5 colour difference |
| `0x0B0-0x0B7` | `PROCTEX_LUT_DATA0-7` | write queue into the selected table |

| Table | Entries | Entry layout |
| ----- | ------- | ------------ |
| Noise, RGB mapping, alpha mapping | 128 | bits 0-11 `fixed0.0.12` value; bits 12-23 `fixed0.0.12` two's-complement difference to the next entry |
| Colour | 256 | bits 0-7 R, 8-15 G, 16-23 B, 24-31 A |
| Colour difference | 256 | four signed bytes: half of the R, G, B, A difference between this and the next colour entry |

citro3d's defaults for the unknown parts of `PROCTEX4`/`PROCTEX5` are `0x60` and `0xE0C080` (`proctex.c`, `C3D_ProcTexInit`) `[documented: citro3d proctex.c]`.

### Colour combiner (TexEnv) registers (`0x0C0-0x0FD`)

Six stages. Stages 0-3 start at `0x0C0 + 8*n`; stages 4 and 5 start at `0x0F0` and `0x0F8`. Each stage has five registers at offsets 0-4: `SOURCE`, `OPERAND`, `COMBINER`, `COLOR`, `SCALE`.

| Offset | Register | Fields |
| ------ | -------- | ------ |
| +0 | `SOURCE` | bits 0-3, 4-7, 8-11 RGB source 0-2; bits 16-19, 20-23, 24-27 alpha source 0-2. Values: 0 primary colour, 1 fragment primary colour, 2 fragment secondary colour, 3-6 texture 0-3, 13 previous buffer, 14 constant (`COLOR`), 15 previous. "Previous" in stage 0 returns source 3; if source 3 is also previous it returns zero. "Previous buffer" always returns zero in stage 0 |
| +1 | `OPERAND` | RGB operands bits 0-3, 4-7, 8-11; alpha operands bits 12-14, 16-18, 20-22. RGB values: 0 source colour, 1 one minus colour, 2 source alpha, 3 one minus alpha, 4 red, 5 one minus red, 8 green, 9 one minus green, 12 blue, 13 one minus blue. Alpha values: 0 alpha, 1 one minus alpha, 2 red, 3 one minus red, 4 green, 5 one minus green, 6 blue, 7 one minus blue |
| +2 | `COMBINER` | bits 0-3 RGB function, bits 16-19 alpha function: 0 replace, 1 modulate, 2 add, 3 add signed, 4 interpolate, 5 subtract, 6 dot3 RGB, 7 dot3 RGBA, 8 multiply then add, 9 add then multiply |
| +3 | `COLOR` | constant colour, bytes R, G, B, A (bits 0-7 is R) |
| +4 | `SCALE` | bits 0-1 RGB scale, bits 16-17 alpha scale: 0 1x, 1 2x, 2 4x |

| Id | Register | Fields |
| -- | -------- | ------ |
| `0x0E0` | `TEXENV_UPDATE_BUFFER` | shared by fog/gas mode and combiner buffer inputs. Bits 0-2 fog mode (0 off, 1 linear, 5 fog, 7 gas); bit 3 shading density source (0 plain, 1 depth); bits 8-11 stage 1-4 RGB buffer input, bits 12-15 stage 1-4 alpha buffer input (0 previous buffer, 1 previous); bit 16 Z flip. Buffer inputs are typically written with write mask `0x2`, fog/gas mode with mask `0x5` |
| `0x0FD` | `TEXENV_BUFFER_COLOR` | buffer colour R, G, B, A bytes |
| `0x0E1` | `FOG_COLOR` | bits 0-7 R, 8-15 G, 16-23 B |
| `0x0E2`, `0x0E3` | `FOG_LINEAR_SLOPE`, `_OFFSET` | `fixed1.4.11` two's complement. Linear fog (mode 1): f = clamp(slope * depth + offset, 0, 1), depth being the depth-map output (1 - depth with Z flip); RGB = f * colour + (1 - f) * `FOG_COLOR`, alpha unchanged; the fog LUT is not used; about 8 bits of precision. No effect in other fog modes |
| `0x0E4` | `GAS_ATTENUATION` | `float1.5.10` gas density attenuation |
| `0x0E5` | `GAS_ACCMAX` | `float1.5.10` gas maximum density accumulation |
| `0x0E6` | `FOG_LUT_INDEX` | start index |
| `0x0E8-0x0EF` | `FOG_LUT_DATA0-7` | write queue; 128 entries: bits 0-12 `fixed1.1.11` difference to the next entry, bits 13-23 `fixed0.0.11` value |

### Framebuffer, gas and shadow registers (`0x100-0x130`)

| Id | Register | Fields |
| -- | -------- | ------ |
| `0x100` | `COLOR_OPERATION` | bits 0-1 fragment operation mode (0 default, 1 gas, 3 shadow); bit 8 blend mode (0 logic op, 1 blend); bits 16-25 = `0x0E4` |
| `0x101` | `BLEND_FUNC` | bits 0-2 RGB equation, 8-10 alpha equation (0 add, 1 subtract, 2 reverse subtract, 3 min, 4 max; 5-7 behave as add); bits 16-19 RGB source function, 20-23 RGB destination, 24-27 alpha source, 28-31 alpha destination. Functions: 0 zero, 1 one, 2 source colour, 3 one minus source colour, 4 destination colour, 5 one minus destination colour, 6 source alpha, 7 one minus source alpha, 8 destination alpha, 9 one minus destination alpha, 10 constant colour, 11 one minus constant colour, 12 constant alpha, 13 one minus constant alpha, 14 source alpha saturate |
| `0x102` | `LOGIC_OP` | bits 0-3: 0 clear, 1 and, 2 reverse and, 3 copy, 4 set, 5 inverted copy, 6 noop, 7 invert, 8 nand, 9 or, 10 nor, 11 xor, 12 equivalent, 13 inverted and, 14 reverse or, 15 inverted or |
| `0x103` | `BLEND_COLOR` | R, G, B, A bytes |
| `0x104` | `FRAGOP_ALPHA_TEST` | bit 0 enable; bits 4-6 function; bits 8-15 reference |
| `0x105` | `STENCIL_TEST` | bit 0 enable; bits 4-6 function; bits 8-15 buffer mask; bits 16-23 signed reference; bits 24-31 mask |
| `0x106` | `STENCIL_OP` | bits 0-2 fail, 4-6 z-fail, 8-10 z-pass: 0 keep, 1 zero, 2 replace, 3 increment, 4 decrement, 5 invert, 6 increment and wrap, 7 decrement and wrap |
| `0x107` | `DEPTH_COLOR_MASK` | bit 0 depth test enable; bits 4-6 depth function; bits 8-11 R, G, B, A write enable; bit 12 depth write enable. Test-enable 0 behaves as function "always" and does not disable writes: clear both bits to switch depth off |
| `0x110` | `FRAMEBUFFER_INVALIDATE` | bit 0: write 1 to invalidate the framebuffer cache. Does not flush; precede it with a flush when changing or clearing the framebuffer |
| `0x111` | `FRAMEBUFFER_FLUSH` | bit 0: write 1 to flush the framebuffer cache to memory (after rendering, before using the result) |
| `0x112`, `0x113` | `COLORBUFFER_READ`, `_WRITE` | bits 0-3: `0xF` allow, 0 disable |
| `0x114`, `0x115` | `DEPTHBUFFER_READ`, `_WRITE` | bit 0 stencil, bit 1 depth |
| `0x116` | `DEPTHBUFFER_FORMAT` | bits 0-1: 0 16-bit depth, 2 24-bit depth, 3 24-bit depth + 8-bit stencil |
| `0x117` | `COLORBUFFER_FORMAT` | bits 0-1 pixel size (3dbrew lists 0 = 16-bit, 2 = 32-bit); bits 16-18 format: 0 RGBA8/gas, 2 RGB5A1, 3 RGB565, 4 RGBA4 |
| `0x118` | `EARLYDEPTH_TEST2` | bit 0 enable early depth test |
| `0x11B` | `FRAMEBUFFER_BLOCK32` | bit 0: 0 = 8x8 blocks, 1 = 32x32 blocks (untile with bit 16 of the display transfer flags) |
| `0x11C`, `0x11D` | `DEPTHBUFFER_LOC`, `COLORBUFFER_LOC` | bits 0-27 physical address >> 3 |
| `0x11E` | `FRAMEBUFFER_DIM` | bits 0-10 width, bits 12-21 height - 1, bit 24 "vertical flip?" (0 = enable, 1 = disable) |
| `0x11F` | unnamed | writing 0 hangs the GPU |
| `0x120` | `GAS_LIGHT_XY` | bytes: planar shading min intensity, max intensity, density attenuation |
| `0x121` | `GAS_LIGHT_Z` | bytes: view shading min, max, density attenuation |
| `0x122` | `GAS_LIGHT_Z_COLOR` | bits 0-7 view shading effect in the line-of-sight direction; bit 8 gas colour LUT input (0 gas density, 1 light factor) |
| `0x123`, `0x124` | `GAS_LUT_INDEX`, `_DATA` | write queue; 16 entries: the first 8 hold signed RGB differences, the last 8 unsigned RGB colours |
| `0x125` | `GAS_ACCMAX_FEEDBACK` | bits 0-15 initial maximum density; bits 16-31 maximum plain density written by gas density rendering (read-only), compared as unsigned 16-bit integers. Writing bits 0-15 while gas mode is selected resets the maximum; the change shows after the next gas fragment. Writes in other modes do not reset it |
| `0x126` | `GAS_DELTAZ_DEPTH` | bits 0-23 `fixed0.16.8` depth-direction attenuation proportion; bits 24-25 gas depth function: 0 never, 1 always, 2 greater/greater-or-equal, 3 less/less-or-equal/equal/not-equal |
| `0x130` | `FRAGOP_SHADOW` | bits 0-15 `float1.5.10` penumbra scale + bias; bits 16-31 penumbra scale with reversed sign |

Test functions (alpha, stencil, depth share the encoding): 0 never, 1 always, 2 equal, 3 not equal, 4 less, 5 less or equal, 6 greater, 7 greater or equal.

> Conflict: colour-buffer pixel size. 3dbrew lists only pixel-size values 0 (16-bit) and 2 (32-bit) for `COLORBUFFER_FORMAT` bits 0-1. citro3d's `framebuffer.c` writes a size table `{2, 1, 0, 0, 0}` for RGBA8, RGB8, RGB5A1, RGB565, RGBA4, so a 24-bit target uses pixel size 1. This page follows citro3d for what works in practice; 3dbrew's table is incomplete rather than contradicted. libctru's `GPU_RB_RGB8` has no counterpart in the 3dbrew format list either, so RGB8 render targets are `[documented: citro3d]` only.

### Fragment lighting registers (`0x140-0x1D9`)

Each of the eight lights has a block at `0x140 + 0x10 * i`:

| Offset | Register | Fields |
| ------ | -------- | ------ |
| +0 | `LIGHTi_SPECULAR0` | colour bytes: bits 0-7 B, 10-17 G, 20-27 R. Usually material specular0 * light specular0 |
| +1 | `LIGHTi_SPECULAR1` | same, specular1 |
| +2 | `LIGHTi_DIFFUSE` | same, diffuse |
| +3 | `LIGHTi_AMBIENT` | same, ambient |
| +4 | `LIGHTi_XY` | `float1.5.10` X (bits 0-15) and Y (16-31): position (positional light) or direction vector (directional light) |
| +5 | `LIGHTi_Z` | `float1.5.10` Z |
| +6 | `LIGHTi_SPOTDIR_XY` | `fixed1.1.11` X (bits 0-12) and Y (16-28), negated unit spot direction |
| +7 | `LIGHTi_SPOTDIR_Z` | `fixed1.1.11` Z (bits 0-12), negated |
| +9 | `LIGHTi_CONFIG` | bit 0 type (0 positional, 1 directional); bit 1 two-sided diffuse; bit 2 use geometric factor 0; bit 3 use geometric factor 1 |
| +A | `LIGHTi_ATTENUATION_BIAS` | bits 0-19 `float1.7.12` |
| +B | `LIGHTi_ATTENUATION_SCALE` | bits 0-19 `float1.7.12`. Distance attenuation factor = DA(clip(bias + scale * distance, 0, 1)) |

| Id | Register | Fields |
| -- | -------- | ------ |
| `0x1C0` | `LIGHTING_AMBIENT` | initial primary colour before per-light terms (B 0-7, G 10-17, R 20-27); usually material emission + material ambient * scene ambient |
| `0x1C2` | `LIGHTING_NUM_LIGHTS` | bits 0-2: active lights - 1 |
| `0x1C3` | `LIGHTING_CONFIG0` | see below |
| `0x1C4` | `LIGHTING_CONFIG1` | per-aspect disable bits, see below |
| `0x1C5` | `LIGHTING_LUT_INDEX` | bits 0-7 start index; bits 8-12 table: 0 D0, 1 D1, 3 FR, 4 RB, 5 RG, 6 RR, 8-15 SP0-7, 16-23 DA0-7 |
| `0x1C6` | `LIGHTING_ENABLE1` | bit 0 = 1 when lighting is disabled, 0 when enabled |
| `0x1C8-0x1CF` | `LIGHTING_LUT_DATA0-7` | write queue; bits 0-23 entry |
| `0x1D0` | `LIGHTING_LUTINPUT_ABS` | bit 1 D0, 5 D1, 9 SP, 13 FR, 17 RB, 21 RG, 25 RR: abs() flag, 0 = take abs, 1 = disabled |
| `0x1D1` | `LIGHTING_LUTINPUT_SELECT` | nibbles (bits 0-2 D0, 4-6 D1, 8-10 SP, 12-14 FR, 16-18 RB, 20-22 RG, 24-26 RR): 0 N·H, 1 V·H, 2 N·V, 3 L·N, 4 -L·P (spotlight), 5 cos φ (CP) |
| `0x1D2` | `LIGHTING_LUTINPUT_SCALE` | same nibbles: 0 1x, 1 2x, 2 4x, 3 8x, 6 0.25x, 7 0.5x |
| `0x1D9` | `LIGHTING_LIGHT_PERMUTATION` | 3-bit light ids of the 1st to 8th enabled light in nibbles 0-7 |

`LIGHTING_CONFIG0` fields:

| Bits | Meaning |
| ---- | ------- |
| 0 | Shadow factor enabled (usually with bit 16, 17 or 18) |
| 2-3 | Fresnel selector: 0 none, 1 primary alpha, 2 secondary alpha, 3 both |
| 4-7 | Light environment configuration (see below) |
| 8-11 | = 4 |
| 16, 17 | Apply shadow attenuation to primary / secondary colour |
| 18 | Invert shadow attenuation |
| 19 | Apply shadow attenuation to alpha |
| 22-23 | Bump map texture unit |
| 24-25 | Shadow map texture unit |
| 27 | Clamp highlights |
| 28-29 | Bump mode: 0 not used, 1 bump map, 2 tangent map |
| 30 | Recalculate bump vectors (0 = enabled, 1 = disabled; usually 1 when bump mode is not 0) |
| 31 | = 1 |

Light environment configuration decides which LUTs exist. A LUT that is unavailable in the chosen configuration always reads as 1.0 regardless of `CONFIG1`. If RR is enabled but RG and RB are not, RR's output is used for red, green and blue.

| Configuration | Value in bits 4-7 | Available LUTs |
| ------------- | ----------------- | -------------- |
| 0 | 0 | D0, RR, SP, DA |
| 1 | 1 | FR, RR, SP, DA |
| 2 | 2 | D0, D1, RR, DA |
| 3 | 3 | D0, D1, FR, DA |
| 4 | 4 | all except FR |
| 5 | 5 | all except D1 |
| 6 | 6 | all except RB and RG |
| 7 | 8 | all (CP input only works here, see [shading features](gpu-shading-features.md#the-gpu_lutinput_cp-quirk-anisotropic-reflection)) |

`LIGHTING_CONFIG1` disables parts of the environment (1 = disabled): bits 0-7 shadows for lights 0-7; bits 8-15 spot light 0-7; bit 16 D0, 17 D1, 19 FR, 20 RB, 21 RG, 22 RR; bit 18 is the spotlight (SP) LUT slot and reads 1 in 3dbrew's description (citro3d treats it as the SP bit when choosing a light environment configuration but never clears it in the register); bits 24-31 distance attenuation for lights 0-7.

Lighting LUT data: a LUT covers the input domain [-1.0, 1.0], indexed by a signed 8-bit number, so it has 256 entries, index `(int)(x / 127.0f) & 0xFF` (as printed by 3dbrew). DA tables use [0.0, 1.0] with an unsigned index. Entry format: bits 0-11 `fixed0.0.12` value; bits 12-23 `fixed1.0.11` "absolute value of the difference between the next entry and this entry" for linear interpolation.

> Conflict: sign of the lighting-LUT difference field. 3dbrew calls bits 12-23 the absolute difference (`fixed1.0.11`) but gives the type a sign bit. citro3d's `LightLut_FromArray` (`lightlut.c`) writes sign-magnitude: bit 11 of the field (bit 23 of the word) set for a negative difference, low 11 bits = |difference| * 0x800 (saturating at 0x7FF). The procedural-texture tables, by contrast, use a plain 12-bit two's-complement difference (`proctex.c`), and the fog table a 13-bit two's complement. This page follows citro3d for the lighting LUT because homebrew uses it daily `[documented: citro3d lightlut.c]`; whether the hardware treats the difference as sign-magnitude or two's complement is not settled by the sources.

### Geometry pipeline registers (`0x200-0x25F`)

| Id | Register | Fields |
| -- | -------- | ------ |
| `0x200` | `ATTRIBBUFFERS_LOC` | bits 1-28 base address of all vertex arrays (physical >> 3 in citro3d) |
| `0x201` | `ATTRIBBUFFERS_FORMAT_LOW` | attributes 0-7: per attribute bits 0-1 type (0 byte, 1 unsigned byte, 2 short, 3 float) and bits 2-3 "size" (see conflict) |
| `0x202` | `ATTRIBBUFFERS_FORMAT_HIGH` | attributes 8-11 as above in bits 0-15; bits 16-27 fixed-attribute mask; bits 28-31 total attribute count - 1 |
| `0x203 + 3n` | `ATTRIBBUFFERn_OFFSET` | bits 0-27 offset from the base address (n = 0-11) |
| `0x204 + 3n` | `ATTRIBBUFFERn_CONFIG1` | eight 4-bit component ids: 0-11 vertex attribute, 12 4-byte padding, 13 8-byte, 14 12-byte, 15 16-byte |
| `0x205 + 3n` | `ATTRIBBUFFERn_CONFIG2` | bits 0-15 components 9-12; bits 16-23 bytes per vertex; bits 28-31 number of components |
| `0x227` | `INDEXBUFFER_CONFIG` | bits 0-27 offset from the base address; bit 31 index type (0 byte, 1 short; also 1 for draw arrays) |
| `0x228` | `NUMVERTICES` | vertex count |
| `0x229` | `GEOSTAGE_CONFIG` | bits 0-1 geometry shader in use (0 no, 2 yes); bit 8 drawing triangle elements; bit 31 use reserved geometry-shader subdivision |
| `0x22A` | `VERTEX_OFFSET` | first vertex |
| `0x22D` | `POST_VERTEX_CACHE_NUM` | bits 0-7 entries (usually `0x4` or `0x84`) |
| `0x22E`, `0x22F` | `DRAWARRAYS`, `DRAWELEMENTS` | write non-zero to draw |
| `0x231` | `VTX_FUNC` | write non-zero to clear the post-vertex cache |
| `0x232` | `FIXEDATTRIB_INDEX` | bits 0-3 fixed attribute 0-11, or `0xF` for immediate-mode submission |
| `0x233-0x235` | `FIXEDATTRIB_DATA0-2` | four float24 values packed into three words (see below) |
| `0x238`, `0x239` | `CMDBUF_SIZE0/1` | bits 0-20 size >> 3 |
| `0x23A`, `0x23B` | `CMDBUF_ADDR0/1` | bits 0-28 physical address >> 3 |
| `0x23C`, `0x23D` | `CMDBUF_JUMP0/1` | write non-zero to execute buffer 0 / 1 |
| `0x242` | `VSH_NUM_ATTR` | bits 0-3 vertex shader input attributes - 1 |
| `0x244` | `VSH_COM_MODE` | bit 0: use the geometry-shader configuration (1) or reuse the vertex-shader configuration for the geometry shader unit (0, when the geometry unit is not in use: uniforms, outmap mask, code and swizzle data propagate) |
| `0x245` | `START_DRAW_FUNC0` | bit 0 mode: 0 drawing, 1 configuration. In mode 1 rendering fails; in mode 0 changes to vertex-shader configuration registers are not applied correctly, so drivers keep it at 1, set 0 just before a draw trigger and 1 again right after |
| `0x24A`, `0x251` | `VSH_OUTMAP_TOTAL1/2` | bits 0-3 vertex shader output map registers - 1 |
| `0x252` | `GSH_MISC0` | `0x00000001` reserved subdivision in use, `0x01004302` particle system in use, else 0 |
| `0x253` | `GEOSTAGE_CONFIG2` | bit 0 function indicator (with vertex buffers: 0 draw elements, 1 draw arrays; without: 1 while inputting immediate-mode attributes); bit 8 drawing triangle elements. While bit 0 is 1, register writes outside `0x200-0x254` and `0x280-0x2DF` may be processed incorrectly |
| `0x254` | `GSH_MISC1` | bits 0-4 reserved subdivision type (2 Loop, 3 Catmull-Clark); ignored when no subdivision |
| `0x25E` | `PRIMITIVE_CONFIG` | bits 0-3 output map registers - 1; bits 8-9 primitive mode: 0 triangles, 1 strip, 2 fan, 3 geometry primitive |
| `0x25F` | `RESTART_PRIMITIVE` | write 1 to restart (breaks a strip or fan) |

The vertex-attribute "size" field and its encoding are disputed; see the [conflict on the command-pipeline page](../engineering/pica200-command-pipeline.md#technical-details): 3dbrew prints 8/16/24/32 bits, libctru and citro3d use components - 1.

`FIXEDATTRIB_DATA` packing (four `float1.7.16` values X, Y, Z, W written in three words): `DATA0` (the first word written) holds Z bits 16-23 in bits 0-7 and W in bits 8-31; `DATA1` holds Y bits 8-23 in bits 0-15 and Z bits 0-15 in bits 16-31; `DATA2` holds X in bits 0-23 and Y bits 0-7 in bits 24-31. citro3d builds this as 12 bytes (x, y, z, w each 3 bytes little-endian) with the first and third words swapped (`immediate.c`) `[documented: citro3d]`.

### Shader unit registers (`0x280-0x2DD`)

Geometry shader (GSH) ids are `0x280` onward, vertex shader (VSH) ids `0x2B0` onward; the layouts are identical, offset by `0x30`. 3dbrew's names use `SH_` for the shared register description.

| VSH id | Register (GSH id) | Fields |
| ------ | ----------------- | ------ |
| `0x2B0` (`0x280`) | `BOOLUNIFORM` | bits 0-15 boolean registers b0-b15; bits 16-31 = `0x7FFF` |
| `0x2B1-0x2B4` (`0x281-0x284`) | `INTUNIFORM_I0-I3` | four bytes X, Y, Z, W |
| `0x2B9` (`0x289`) | `INPUTBUFFER_CONFIG` | bits 0-3 input attributes - 1; bits 8-15 use reserved subdivision (always 0 for vertex shaders); bits 24-31 use geometry shader (`0x8` use, `0xA0` don't; always `0xA0` for vertex shaders) |
| `0x2BA` (`0x28A`) | `ENTRYPOINT` | bits 0-15 entry offset in words; bits 16-31 = `0x7FFF`. The geometry register also sets the single shader unit that can be dedicated to geometry shading, even when running a vertex shader; the vertex register applies to every unit in vertex mode (all 4, or 3 when one unit is geometry) |
| `0x2BB`, `0x2BC` (`0x28B`, `0x28C`) | `ATTRIBUTES_PERMUTATION_LOW/HIGH` | nibble n = input register index (v0-v15) that receives attribute n. LOW attributes 0-7, HIGH 8-15 |
| `0x2BD` (`0x28D`) | `OUTMAP_MASK` | bit n enables output register on (n = 0-15). o7-o15 exist for vertex shaders only |
| `0x2BF` (`0x28F`) | `CODETRANSFER_END` | write 1 to finish a code transfer |
| `0x2C0` (`0x290`) | `FLOATUNIFORM_INDEX` | bits 0-7 target register (0-95); bit 31 transfer mode: 0 `float1.7.16` (three words), 1 `float1.8.23` (four words) |
| `0x2C1-0x2C8` (`0x291-0x298`) | `FLOATUNIFORM_DATA0-7` | write queue; the register index auto-increments after each full 4-component register |
| `0x2CB` (`0x29B`) | `CODETRANSFER_INDEX` | bits 0-11 word offset |
| `0x2CC-0x2D3` (`0x29C-0x2A3`) | `CODETRANSFER_DATA0-7` | write queue of instruction words |
| `0x2D5` (`0x2A5`) | `OPDESCS_INDEX` | bits 0-11 offset |
| `0x2D6-0x2DD` (`0x2A6-0x2AD`) | `OPDESCS_DATA0-7` | write queue of [operand descriptors](../glossary.md#operand-descriptor) |

Float uniform upload: in `float1.7.16` mode the four components X, Y, Z, W are sent in reverse order, concatenated, as three words laid out `ZZWWWWWW`, `YYYYZZZZ`, `XXXXXXYY` (each letter 4 bits) `[reverse-engineered: 3dbrew]`. In `float1.8.23` mode four plain float32 words are sent in the order W, Z, Y, X.

### Names that differ between sources

| Id | 3dbrew name | libctru `registers.h` name |
| -- | ----------- | -------------------------- |
| `0x04C` | `DEPTHMAP_TRANSFORM_ENABLE` | `GPUREG_004C` |
| `0x064` | `TEXUNIT_LOD_ENABLE` | `SH_OUTATTR_MODE` |
| `0x06D` | `DEPTHMAP_MODE` | `DEPTHMAP_ENABLE` |
| `0x0AC`, `0x0AD` | `TEXUNIT3_PROCTEX4`, `PROCTEX5` | `PROCTEX4` defined at the wrong id `0x00A`, no `PROCTEX5` |
| `0x0E2`, `0x0E3` | `FOG_LINEAR_SLOPE`, `_OFFSET` | unnamed |

The decoder prints libctru's names, adding the five 3dbrew-only ids above; the other 3dbrew names are the same as libctru's.

## Homebrew notes

`citro3d` sets nearly all of these for you ([citro3d fixed-function API](../engineering/citro3d-fixed-function-api.md) shows which call writes which register). Read this page when a command list from a capture needs decoding, when writing an emulator, or when a hang points at one of the listed quirks ([GPU pitfalls](gpu.md#known-pitfalls)).

## Open questions

- Registers `0x000-0x00F`, `0x011-0x03F`, `0x2E0-0x2FF` and most gaps are undocumented. The three `LIGHTING_CONFIG0` constant bits (8-11, 31) and several "= constant" fields are what drivers write, not necessarily what hardware requires.
- Whether the header's register id really spans bits 0-15, and which model each behaviour was checked on.

## Sources

- [3dbrew: GPU/Internal Registers](https://www.3dbrew.org/wiki/GPU/Internal_Registers) — register list, bit fields, enumerations, LUT entry formats, shader unit registers.
- [3dbrew: GPU/Pitfalls](https://www.3dbrew.org/wiki/GPU/Pitfalls) — output map example.
- [3dbrew: GPU/Programming Guide](https://www.3dbrew.org/wiki/GPU/Programming_Guide) — fixed attributes, immediate mode, draw sequence.
- citro3d `source/lightlut.c`, `proctex.c`, `fog.c`, `gas.c`, `framebuffer.c`, `immediate.c`, `effect.c` at `9f21cf7` — how the registers are actually filled.
- libctru `include/3ds/gpu/registers.h` at `62b7c42` — alternative register names.
