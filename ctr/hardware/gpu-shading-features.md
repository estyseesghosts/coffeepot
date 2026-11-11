# CTR GPU shading features

The 3DS [GPU](gpu.md) has no programmable pixel shader. Instead, DMP packed the pixel stage with *reconfigurable fixed-function* units that cover the effects developers actually use: lighting, shadows, procedural textures and fog. These were hard to understand from outside — DMP gave a few public talks and papers, and the rest was reverse-engineered by watching real games — so this page collects what is known, and says where it is uncertain.

Most of these units work through **lookup tables** (**[LUTs](../../glossary.md#lut)**): the hardware computes a number (an angle, a distance), uses it as an index into a table the program supplied, and the table's contents are the result. Change the table and you change the lighting model, the banding of a toon shader, or the colours of a wood grain.

These units are part of the PICA200 and are used the same way on the Old 3DS and the New 3DS as far as the sources show.

> Unknown: none of the cited sources (DMP papers and talk, 3dbrew, fincs, wwylele) says on which console model its observations were made, and none reports a New 3DS difference. A test of one lighting and one procedural-texture scene on each model would settle it.

## Where it fits

These units sit in the fragment stage of the [GPU pipeline](gpu.md#how-it-works), after the rasteriser and before the colour combiners. They are driven by [shader](../../glossary.md#shader) outputs (a quaternion describing the surface, a view vector, texture coordinates) and by GPU registers set through the command list. Programs normally use `citro3d`'s `lightlut`, `proctex`, `fog` and `gas` modules ([development tools](../homebrew/picasso-and-citro3d.md)).

## How it works

### Why a "fixed" pixel stage

DMP's CEDEC 2012 talk ("Thinking behind the fixed-shader hardware PICA200") argued that although programmable shaders have existed for over ten years, "the features you actually use are mostly settled", so adding a few reconfigurable options to fixed hardware gets nearly everything done. Running the same effect on a fixed unit took 1–4 cycles per fragment, versus tens of cycles on a Shader Model 3.0-style programmable unit. Fewer circuits, deeper pipelining, lower clocks and fine-grained clock gating (including near the clock root) saved power. The PICA200 was announced in 2006; DMP later developed a successor core.

### Fragment lighting

Lighting is evaluated per pixel. The vertex shader outputs a **quaternion** describing the surface frame (normal, tangent and bitangent) because interpolating a quaternion across a triangle is cheaper than interpolating separate vectors. The hardware then derives the angle terms (such as N·H or V·H, where N is the normal, H the half-angle, V the view direction) and uses each as an index into a LUT. Common setups:

| Model | LUT D0 | LUT D1 | Reflection LUTs | Geometry factors | Spotlight LUT |
| ----- | ------ | ------ | --------------- | ---------------- | ------------- |
| Blinn-Phong | N·H → x^s (s = specularity) | — | disabled | disabled | spotlight set-up or none |
| Cook-Torrance | — | N·H → D(x) | V·H → F(x) | enabled | spotlight set-up or none |
| Schlick-like | — | N·H → Schlick Z(x) | V·H → F(x) | enabled | cos φᵢ → Schlick A(x) |
| Subsurface scattering (3dbrew: "not entirely sure") | — | N·V → transmittance by angle | L·N → reflectance by angle | disabled | spotlight set-up or none |

"Spotlight set-up" means input −L·P (light vector against the spot axis) and LUT contents = the falloff. D and F are the distribution and Fresnel terms of Cook and Torrance; Schlick Z and A are from Schlick's 1994 paper. The Fresnel LUT can blend two colours by how oblique the view angle is, or add white with an exponential falloff for rim lighting. Cel shading ("toon") is achieved simply by making adjacent LUT entries equal.

**The vectors.** Lighting is evaluated per light source and the results are added. It needs the normal N, the tangent T and bitangent B (perpendicular to N and T), the view vector V (from the surface point toward the camera) and the light vector L. A quaternion describes the transformation between *surface-local space* (where, up to permutation, N = (0,0,1), T = (1,0,0), B = (0,1,0)) and eye space; the other direction works too. There is **no vertex output semantic for normals or tangents**: the vertex shader must output a value with the *quaternion* semantic (`normquat`, output-map values `0x04`–`0x07`), so something has to convert normal/tangent vectors to a quaternion in the shader, using the surface-local matrix. A geometry shader could even compute normals from raw positions first. Two lighting equations exist, one for the primary and one for the secondary colour (Kazakov & Ohbuchi, section 3.2.2); fragment lighting can also write the alpha of either colour from a selectable angle and a LUT. The results appear as two inputs to the colour combiners (primary and secondary fragment colours). Register fields: [GPU internal registers](gpu-internal-registers.md#fragment-lighting-registers-0x140-0x1d9), which also lists the light-environment configurations that decide which LUTs exist.

How quaternions are interpolated (plain linear, slerp, nlerp) is unknown; the architecture suggests plain linear interpolation `[reverse-engineered: 3dbrew, no test given]`.

Published background that 3dbrew lists for this unit: Everitt, "Per-Pixel Lighting" (GDC 2001; surface-local space); Kazakov & Ohbuchi, "Primitive Processing and Advanced Shading Architecture for Embedded Space" (both the paper and the shorter slides; the paper has the detail); Ohbuchi & Unno, "A Real-Time Configurable Shader Based on Lookup Tables" (the most detailed explanation of the implementation); Kuijk & Blake, "Faster Phong Shading via Angular Interpolation" (why quaternions interpolate faster than vectors).

### The `GPU_LUTINPUT_CP` quirk (anisotropic reflection)

`CP` means cos(φ) and drives anisotropic reflection after Schlick's model, where the input should be T·H′ (tangent vector dotted with the half-angle vector projected onto the tangent plane). `[reverse-engineered]` by wwylele, in ["GPU_LUTINPUT_CP"](https://gist.github.com/wwylele/f61d6653620bb763ab9dc7a60a9831ce) (gist, last active 2022-09-19); the basis is replicating 3DS output in the Citra emulator, with no other hardware testing described. Findings:

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

Noise details (reverse-engineered by MerryMage, wwylele and fincs): coordinates are multiplied by `9 × frequency`, the integer lattice point gets a pseudo-random value in [−1, 1] from a PRNG that yields 4-bit numbers (period 144 = 16 × 9 in both directions), and the four surrounding lattice values are blended with the user's *noise LUT* (typically 3x² − 2x³), as in classic Perlin noise. The 4-bit value is expanded with `-1.0 + val × 2.0 / 15.0`. Amplitude, frequency and phase are separate for U and V. The GLSL-style reference in the fincs notes (its `v11` lattice corner reads `point + vec2(1.0, 1.1)` there, almost certainly a typo for `1.0, 1.0` `[hypothesis]`; corrected below):

```glsl
int prng17(int v)
{
    const int h[] = int[](0,4,10,8,4,9,7,12,5,15,13,14,11,15,2,11);
    return ((v%9+2)*3 & 0xF) ^ h[(v/9) & 0xF];
}
float picarand(vec2 point)
{
    const int h[] = int[](10,2,15,8,0,7,4,5,5,13,2,6,13,9,3,14);
    int u2 = prng17(int(point.x));
    int v2 = prng17(int(point.y));
    v2 += ((u2 & 3) == 1) ? 4 : 0;
    v2 ^= (u2 & 1) * 6;
    v2 += 10 + u2;
    v2 &= 0xF;
    v2 ^= h[u2];
    return -1.0 + float(v2)*2.0/15.0;
}
float noise_coef(vec2 x, vec2 freq, vec2 phase)
{
    vec2 grid  = 9.0*freq*abs(x + phase);
    vec2 point = floor(grid), frac = grid - point;
    float v00 = picarand(point),                 v01 = picarand(point + vec2(0.0,1.0));
    float v10 = picarand(point + vec2(1.0,0.0)), v11 = picarand(point + vec2(1.0,1.0));
    float g0 = v00*(frac.x + frac.y),       g1 = v10*(frac.x + frac.y - 1.0);
    float g2 = v01*(frac.x + frac.y - 1.0), g3 = v11*(frac.x + frac.y - 2.0);
    float x0 = mix(g0, g1, noiselut(frac.x));
    float x1 = mix(g2, g3, noiselut(frac.x));
    return mix(x0, x1, noiselut(frac.y));
}
vec2 noise_engine(vec2 x, vec2 freq, vec2 ampl, vec2 phase)
{ return x + ampl * noise_coef(x, freq, phase); }
```

| Stage | Modes |
| ----- | ----- |
| Shift (`GPU_PT_*`) | `NONE`; `ODD`: `u += offset × ((floor(v)/2) % 2)`; `EVEN`: `u += offset × (((floor(v)+1)/2) % 2)` (and the same with u and v swapped). Offset is 1 for mirrored repeat, else 0.5. The coordinate that goes through `floor` is taken from the stage before noise is applied |
| Clip/repeat | `CLAMP_TO_ZERO` (values above 1 become 0), `CLAMP_TO_EDGE` (above 1 become 1), `REPEAT` (fractional part), `MIRRORED_REPEAT` (`int_part % 2 == 0 ? frac : 1 - frac`), `PULSE` (1 if greater than — or equal to? — 0.5, else 0) |
| Combiner | `U`, `U2`, `V`, `V2`, `ADD` ((u+v)/2), `ADD2`, `SQRT2` (√(u²+v²)), `MIN`, `MAX`, `RMAX` (average of ADD and SQRT2); results above 1 clamp to 1 |
| Mapper | One LUT, or two (one for RGB via the colour table, one for alpha) |
| Colour table | 256 colours plus deltas for interpolation; starting index and length are set by registers; a 1D mipmap mode exists but is untested |

> Conflict: size of the procedural-texture pipeline. 3dbrew's *Procedural Texture Generation* page (based on reports of a DMP presentation) describes four stages with a repeat module that "doesn't seem configurable beyond" symmetric and mirrored wrapping, **six** base shapes G(u″,v″) and **two** options for F (identity, triangle) before the colour lookup. The register tables on 3dbrew's *Internal Registers* page and fincs's notes instead show five clamp/repeat modes (`PROCTEX0` bits 0-5), **ten** combiner functions (bits 6-13), user-supplied mapping LUTs (RGB and alpha) rather than a fixed F, and a shift stage; libctru's `GPU_PT_*` enums agree with the registers. This page follows the register-level sources (3dbrew Internal Registers, fincs, libctru); the shorter 3dbrew page predates them. The register fields are in [GPU internal registers](gpu-internal-registers.md#procedural-texture-registers-texture-unit-3).

DMP said at CEDEC that the feature is very hard to use ("only four people at the company can use it fully"), so few games use it.

### Shadows

The PICA200 renders shadow maps in a special mode (`GPU_FRAGOPMODE_SHADOW`):

- The *colour* buffer is used as the depth buffer and the real depth buffer must be disabled or unbound, or the output is glitched.
- The buffer format is declared `RGBA8` but really holds 3 bytes of big-endian depth and 1 byte of soft-shadow strength (0 = fully shadowed, 255 = no shadow), like a D24S8 format.
- The fragment outputs a colour but only green matters: green = 0 renders a hard shadow (updates depth); green ≠ 0 renders a soft shadow (writes the strength and leaves depth alone).
- Output-stage settings are ignored; the depth test is hard-wired to "less than", so the depth-map registers must map [−1, 0] to [0, 1], and perspective divide on depth must be off (W-buffering).
- Soft shadows normally use DMP's *silhouette* geometry shader.

When a shadow is applied during lighting, **only texture unit 0** supports shadow textures. If a non-shadow texture is bound as the shadow map, the light colour is multiplied directly by the texture, bypassing all shadow logic. A Z bias prevents "shadow acne", and the penumbra scale/bias multiplies the soft-shadow strength by `1 / (L + H × zDraw/zRef)`, where L and H are the low/high halves of `GPUREG_FRAGOP_SHADOW`, `zDraw` is the fragment's raw depth and `zRef` the stored shadow-map depth. The shadow texture's coordinates arrive as `texcoord0` in [0, 1] (so the vertex shader converts from [−1, 1]) and `texcoord0w` carries the light-space depth. The penumbra formula can equally be written `1 / (bias + scale × (1 − zDraw/zRef))`, because normally H = −scale and L = scale + bias. Perspective divide can optionally be enabled for shadow lookups (u and v divided by w), but w is still the light-space depth, so the same scaling used when rendering the shadow map must be applied to w (and, to cancel in the division, to u and v: (u/k)/(w/k) = u/w); for orthographic light projections passing −z is enough. Shadow cube maps also exist: all of (u, v, w) is used as a direction, the greatest component selects the face and gives the comparison depth, and faces must be stored upside down in general for cube textures. Some of this was worked out by wwylele. Registers: `COLOR_OPERATION` mode 3, `FRAGOP_SHADOW`, `TEXUNIT0_SHADOW` and the shadow bits of `LIGHTING_CONFIG0` ([registers](gpu-internal-registers.md)); `citro3d` calls: [API page](../engineering/citro3d-fixed-function-api.md#render-state-blend-depth-stencil-alpha-cull).

### Geometry: subdivision and the secondary vertex cache

The geometry stage has a **Primitive Engine** that works with a *secondary vertex cache* (SVC in DMP's slides — not to be confused with the kernel's supervisor calls). Because processed vertices are cached, they are reused without memory access, and the cache lets primitives vary in size, which allows Catmull-Clark and Loop subdivision (comparable to Direct3D 11 tessellation). The same machinery generates soft-shadow silhouettes and toon-shading outlines. The geometry unit cannot read textures, so there is no displacement mapping.

### Gas

The PICA200 can render "accumulative gas" (volumetric clouds and smoke). DMP filed a patent describing the algorithm, but fincs's RE notes call the document messy, OCR-damaged and in Japanese, and the page is marked to-do. Gas is exposed by `citro3d`'s `gas` module ([API page](../engineering/citro3d-fixed-function-api.md#gas)); the registers are the `GAS_*` ones in [GPU internal registers](gpu-internal-registers.md#framebuffer-gas-and-shadow-registers-0x100-0x130) (a two-stage effect: a density pass with fragment operation mode 1 whose maximum can be read back through `GAS_ACCMAX_FEEDBACK`, then a lit pass with the fog unit in gas mode).

## Technical details

### Behaviours and quirks found while reverse-engineering

- After changing the shader program (writing to vertex/geometry shader code or operand-descriptor memory), the program must **reconfigure** `GPUREG_ATTRIBBUFFERS_FORMAT_LOW`, `GPUREG_VSH_INPUTBUFFER_CONFIG`, `GPUREG_VSH_NUM_ATTR` and `GPUREG_VSH_ATTRIBUTES_PERMUTATION_LOW`, or the GPU can crash.
- A draw-elements command sequence must end with **two dummy writes** to `GPUREG_PRIMITIVE_CONFIG`; without them the GPU can crash when the shader configuration changes next.
- Updating a LUT while its feature is switched off appears to fail (found while preloading procedural-texture LUTs with the unit disabled); how each LUT is affected is not known.
- Registers: LUT sizes in `citro3d`: light LUT 256 entries, fog LUT 128, procedural-texture LUT 128 (plus 256 colour and 256 delta entries).

### How the knowledge was gathered

The PICA200 was reverse-engineered from: two DMP papers (Ohbuchi & Unno on LUT-based shading; Kazakov & Ohbuchi on primitive processing), the gas patent, the CEDEC 2012 procedural-texture talk, and software — *Steel Diver: Sub Wars* (a free eShop download with symbols for DMP's OpenGL ES driver), *Brunswick Pro Bowling* (an ELF with DWARF2 debug information), and *CTRAging*, Nintendo's factory test program (recovered complete from some New 3DS units; it tests gas, subdivision and particles), plus the list of reserved `DMP`-suffixed GL enums.

## Homebrew notes

Use `citro3d`'s `C3D_LightLut`, `C3D_ProcTex` and `C3D_FogLut` APIs rather than writing registers by hand, and keep the quirks above in mind when writing a driver or emulator. Reverse engineers around the Citra emulator, notably fincs, are the main source of these findings.

## Open questions

- Gas rendering is undeciphered.
- Quaternion interpolation method is unknown.
- The 1D-mipmap mode of the procedural colour table has not been tested.
- 3dbrew documents the same procedural-texture pipeline from DMP's talk; fincs's RE notes add the PRNG and per-mode formulas.

## Sources

- [fincs, "Random PICA200 RE Notes" (gist `notes.md`)](https://gist.github.com/fincs/543f7e1375815f495f10020a053f14e9) — procedural-texture noise, shifting, clipping, combiner and mapper formulas; shadow mapping; random quirks; and the list of sources used for PICA200 reverse engineering (DMP papers, the CEDEC 2012 talk, *Steel Diver: Sub Wars* symbols, *Brunswick Pro Bowling* DWARF data, the CTRAging factory test program). Its Gas section stops after a short introduction (`...todo...`). The same notes were mirrored on the Citra project wiki, which is gone since the `citra-emu/citra` repository was removed.
- [wwylele, "GPU_LUTINPUT_CP" (gist)](https://gist.github.com/wwylele/f61d6653620bb763ab9dc7a60a9831ce) — the primary write-up: CP input, unnormalised H′, configuration 7 only, normal-mapping and tangent-mapping caveats; checked by fetching the gist. The same text was on the Citra project wiki, which is gone; Citra/Azahar's implementation file was not located or checked, so no implementation file is cited.
- [4Gamer.net: ［CEDEC 2012］3DSはまだその実力を100％発揮できていない!? 3DSが搭載するGPUコア「PICA200」の詳細](https://www.4gamer.net/games/017/G001762/20120822007/) — DMP's talk on procedural textures, secondary vertex cache and subdivision, fixed-function fragment stage, power saving (in Japanese).
- [3dbrew: GPU/Fragment Lighting](https://www.3dbrew.org/wiki/GPU/Fragment_Lighting) and [GPU/Procedural Texture Generation](https://www.3dbrew.org/wiki/GPU/Procedural_Texture_Generation) — lighting equation setups and the procedural pipeline.
- [citro3d source (devkitPro)](https://github.com/devkitPro/citro3d) — LUT sizes and module names (`lightlut.h`, `proctex.h`, `fog.h`, `gas.h`), table encodings.
- [3dbrew: GPU/Internal Registers](https://www.3dbrew.org/wiki/GPU/Internal_Registers) — lighting, proctex and shadow register fields (see [GPU internal registers](gpu-internal-registers.md)).
- [3dbrew: GPU/Primitive Engine](https://www.3dbrew.org/wiki/GPU/Primitive_Engine) — variable-size primitives.
