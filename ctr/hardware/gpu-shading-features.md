# CTR GPU shading features

The 3DS [GPU](gpu.md) has no programmable pixel shader. Instead, DMP packed the pixel stage with *reconfigurable fixed-function* units that cover the effects developers actually use: lighting, shadows, procedural textures and fog. These were hard to understand from outside — DMP gave a few public talks and papers, and the rest was reverse-engineered by watching real games — so this page collects what is known, and says where it is uncertain.

Most of these units work through **lookup tables** (**[LUTs](../../glossary.md#lut)**): the hardware computes a number (an angle, a distance), uses it as an index into a table the program supplied, and the table's contents are the result. Change the table and you change the lighting model, the banding of a toon shader, or the colours of a wood grain.

## Where it fits

These units sit in the fragment stage of the [GPU pipeline](gpu.md#how-it-works), after the rasteriser and before the colour combiners. They are driven by [shader](../../glossary.md#shader) outputs (a quaternion describing the surface, a view vector, texture coordinates) and by GPU registers set through the command list. Programs normally use `citro3d`'s `lightlut`, `proctex`, `fog` and `gas` modules ([development tools](../homebrew/picasso-and-citro3d.md)).

## How it works

### Why a "fixed" pixel stage

DMP's CEDEC 2012 talk ("Thinking behind the fixed-shader hardware PICA200") argued that although programmable shaders have existed for over ten years, "the features you actually use are mostly settled", so adding a few reconfigurable options to fixed hardware gets nearly everything done. Running the same effect on a fixed unit took 1–4 cycles per fragment, versus tens of cycles on a Shader Model 3.0-style programmable unit. Fewer circuits, deeper pipelining, lower clocks and fine-grained clock gating (including near the clock root) saved power. The PICA200 was announced in 2006; DMP later developed a successor core.

### Fragment lighting

Lighting is evaluated per pixel. The vertex shader outputs a **quaternion** describing the surface frame (normal, tangent and bitangent) because interpolating a quaternion across a triangle is cheaper than interpolating separate vectors. The hardware then derives the angle terms (such as N·H or V·H, where N is the normal, H the half-angle, V the view direction) and uses each as an index into a LUT. Common setups:

| Model | LUT D0 | LUT D1 | Reflection LUTs | Geometry factors |
| ----- | ------ | ------ | --------------- | ---------------- |
| Blinn-Phong | N·H → x^s | — | disabled | spotlight or none |
| Cook-Torrance | — | N·H → D(x) | V·H → F(x) | enabled |
| Schlick-like | — | N·H → Schlick Z(x) | V·H → F(x) | enabled, with cos φ → Schlick A(x) |

Cel shading ("toon") is achieved simply by making adjacent LUT entries equal.

How quaternions are interpolated (plain linear, slerp, nlerp) is unknown.

### The `GPU_LUTINPUT_CP` quirk (anisotropic reflection)

`CP` means cos(φ) and drives anisotropic reflection after Schlick's model, where the input should be T·H′ (tangent vector dotted with the half-angle vector projected onto the tangent plane). Findings from emulating real games:

- It is only available under lighting configuration 7.
- H′ is **not normalised**; normalising it gives different (wrong) output. So φ is not the angle between T and H′.
- When normal mapping is used, the tangent vector stays at (1, 0, 0) in surface-local space instead of rotating with the normal; normal and tangent are then no longer perpendicular.
- Even so, the *perturbed* normal is still used when projecting H onto the "tangent plane", so H′ does not actually lie in the plane of the tangent.
- *Tangent mapping* perturbs the tangent vector (as normal mapping perturbs the normal). Since only `CP` uses the tangent vector, other LUT inputs are unaffected by it. The "recompute z-component" register bit has no effect on a tangent map, which is explained either by the hardware not recomputing, or by the recomputation having no mathematical effect on the dot product `CP` takes.

### Procedural textures (texture unit 3)

Texture unit 3 can generate a texture by calculation instead of reading an image: no memory traffic, no stalls, resolution-independent, animatable by changing parameters alone. DMP's demo "MIKAGE" used it for the wood grain on its floor. The pipeline for inputs (u, v):

1. Take the absolute value of both.
2. **Noise:** add Perlin-style 2D noise (amplitude, frequency and phase parameters).
3. Take the absolute value again.
4. **Shift** by one of three modes.
5. **Repeat/clip** to [0, 1].
6. **Combine** to one value g with a fixed function (the "G function": a line, a square, a circle and so on).
7. **Map** g through a user LUT (the "F function"), which can create bands or repeats.
8. **Colour lookup** in a 1D colour table.

Noise details (reverse-engineered by MerryMage, wwylele and fincs): coordinates are multiplied by `9 × frequency`, the integer lattice point gets a pseudo-random value in [−1, 1] from a PRNG that yields 4-bit numbers (period 144 = 16 × 9 in both directions), and the four surrounding lattice values are blended with the user's *noise LUT* (typically 3x² − 2x³), as in classic Perlin noise.

| Stage | Modes |
| ----- | ----- |
| Shift (`GPU_PT_*`) | `NONE`; `ODD`: `u += offset × ((floor(v)/2) % 2)`; `EVEN`: `u += offset × (((floor(v)+1)/2) % 2)` (and vice versa). Offset is 1 for mirrored repeat, else 0.5 |
| Clip/repeat | `CLAMP_TO_ZERO`, `CLAMP_TO_EDGE`, `REPEAT`, `MIRRORED_REPEAT`, `PULSE` (1 if ≥ 0.5) |
| Combiner | `U`, `U2`, `V`, `V2`, `ADD` ((u+v)/2), `ADD2`, `SQRT2` (√(u²+v²)), `MIN`, `MAX`, `RMAX` (average of ADD and SQRT2); results above 1 clamp to 1 |
| Mapper | One LUT, or two (one for RGB via the colour table, one for alpha) |
| Colour table | 256 colours plus deltas for interpolation; starting index and length are set by registers; a 1D mipmap mode exists but is untested |

DMP said at CEDEC that the feature is very hard to use ("only four people at the company can use it fully"), so few games use it.

### Shadows

The PICA200 renders shadow maps in a special mode (`GPU_FRAGOPMODE_SHADOW`):

- The *colour* buffer is used as the depth buffer and the real depth buffer must be disabled or unbound, or the output is glitched.
- The buffer format is declared `RGBA8` but really holds 3 bytes of big-endian depth and 1 byte of soft-shadow strength (0 = fully shadowed, 255 = no shadow), like a D24S8 format.
- The fragment outputs a colour but only green matters: green = 0 renders a hard shadow (updates depth); green ≠ 0 renders a soft shadow (writes the strength and leaves depth alone).
- Output-stage settings are ignored; the depth test is hard-wired to "less than", so the depth-map registers must map [−1, 0] to [0, 1], and perspective divide on depth must be off (W-buffering).
- Soft shadows normally use DMP's *silhouette* geometry shader.

When a shadow is applied during lighting, **only texture unit 0** supports shadow textures. If a non-shadow texture is bound as the shadow map, the light colour is multiplied directly by the texture, bypassing all shadow logic. A Z bias prevents "shadow acne", and the penumbra scale/bias multiplies the soft-shadow strength by `1 / (L + H × zDraw/zRef)`, where L and H are the low/high halves of `GPUREG_FRAGOP_SHADOW`, `zDraw` is the fragment's raw depth and `zRef` the stored shadow-map depth. The shadow texture's coordinates arrive as `texcoord0` in [0, 1] (so the vertex shader converts from [−1, 1]) and `texcoord0w` carries the light-space depth. Shadow cube maps also exist (the greatest vector component selects the face and gives the comparison depth; faces must be stored upside down).

### Geometry: subdivision and the secondary vertex cache

The geometry stage has a **Primitive Engine** that works with a *secondary vertex cache* (SVC in DMP's slides — not to be confused with the kernel's supervisor calls). Because processed vertices are cached, they are reused without memory access, and the cache lets primitives vary in size, which allows Catmull-Clark and Loop subdivision (comparable to Direct3D 11 tessellation). The same machinery generates soft-shadow silhouettes and toon-shading outlines. The geometry unit cannot read textures, so there is no displacement mapping.

### Gas

The PICA200 can render "accumulative gas" (volumetric clouds and smoke). DMP filed a patent describing the algorithm, but the Citra notes call the document messy, OCR-damaged and in Japanese, and the page is marked to-do. Gas is exposed by `citro3d`'s `gas` module.

## Technical details

### Behaviours and quirks found while reverse-engineering

- After changing the shader program (writing to vertex/geometry shader code or operand-descriptor memory), the program must **reconfigure** `GPUREG_ATTRIBBUFFERS_FORMAT_LOW`, `GPUREG_VSH_INPUTBUFFER_CONFIG`, `GPUREG_VSH_NUM_ATTR` and `GPUREG_VSH_ATTRIBUTES_PERMUTATION_LOW`, or the GPU can crash.
- A draw-elements command sequence must end with **two dummy writes** to `GPUREG_PRIMITIVE_CONFIG`; without them the GPU can crash when the shader configuration changes next.
- Updating a LUT while its feature is switched off appears to fail (found while preloading procedural-texture LUTs with the unit disabled); how each LUT is affected is not known.
- Registers: LUT sizes in `citro3d`: light LUT 256 entries, fog LUT 128, procedural-texture LUT 128 (plus 256 colour and 256 delta entries).

### How the knowledge was gathered

The PICA200 was reverse-engineered from: two DMP papers (Ohbuchi & Unno on LUT-based shading; Kazakov & Ohbuchi on primitive processing), the gas patent, the CEDEC 2012 procedural-texture talk, and software — *Steel Diver: Sub Wars* (a free eShop download with symbols for DMP's OpenGL ES driver), *Brunswick Pro Bowling* (an ELF with DWARF2 debug information), and *CTRAging*, Nintendo's factory test program (recovered complete from some New 3DS units; it tests gas, subdivision and particles), plus the list of reserved `DMP`-suffixed GL enums.

## Homebrew notes

Use `citro3d`'s `C3D_LightLut`, `C3D_ProcTex` and `C3D_FogLut` APIs rather than writing registers by hand, and keep the quirks above in mind when writing a driver or emulator. Emulator authors (Citra) are the main source of these findings.

## Open questions

- Gas rendering is undeciphered.
- Quaternion interpolation method is unknown.
- The 1D-mipmap mode of the procedural colour table has not been tested.
- 3dbrew documents the same procedural-texture pipeline from DMP's talk; the Citra notes add the PRNG and per-mode formulas.

## Sources

- Citra project wiki, "Random PICA200 RE Notes" (procedural textures, shadows, gas, quirks, sources) and "GPU_LUTINPUT_CP" (anisotropic reflection) — the `citra-emu/citra` repository that hosted them has since been removed from GitHub, so no live link could be verified.
- [4Gamer.net: ［CEDEC 2012］3DSはまだその実力を100％発揮できていない!? 3DSが搭載するGPUコア「PICA200」の詳細](https://www.4gamer.net/games/017/G001762/20120822007/) — DMP's talk on procedural textures, secondary vertex cache and subdivision, fixed-function fragment stage, power saving (in Japanese).
- [3dbrew: GPU/Fragment Lighting](https://www.3dbrew.org/wiki/GPU/Fragment_Lighting) and [GPU/Procedural Texture Generation](https://www.3dbrew.org/wiki/GPU/Procedural_Texture_Generation) — lighting equation setups and the procedural pipeline.
- [citro3d source (devkitPro)](https://github.com/devkitPro/citro3d) — LUT sizes and module names (`lightlut.h`, `proctex.h`, `fog.h`, `gas.h`).
