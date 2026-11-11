# citro3d fixed-function API: lighting, fog, procedural textures, gas and render state

The [PICA200](../glossary.md#pica200) has no pixel shader, so everything that happens per pixel is set up through fixed-function units: lighting, fog, procedural textures, gas, blending and depth/stencil tests. `citro3d` exposes each unit as a small group of functions. This page says, for each group, which call does what, which GPU registers it ends up writing, what its defaults are, and which traps the source contains. Read it when a lighting or fog setup draws nothing or the wrong colours, or when you need to know what a call costs in command-list writes.

Short answer to "which call sets what": the unit-by-unit tables below. For the register fields themselves see [GPU internal registers](../hardware/gpu-internal-registers.md); for how the units work and what is known about them see [GPU shading features](../hardware/gpu-shading-features.md); for frames, textures and combiners see [GPU rendering](PICA200/gpu-rendering.md).

## Before you start

- Checked by reading citro3d at `9f21cf7` (v1.7.1) and libctru at `62b7c42` ([source versions](source-versions.md)); everything marked `[documented]` is stated in those files. Nothing here was run on a console or emulator. All behaviour is the same on the Old 3DS and the New 3DS as far as the code is concerned: citro3d contains no model check in these paths.
- Verification status of the one code fragment: it builds with devkitARM/libctru/citro3d (`-Wall -Wextra`); not run on an Old 3DS or a New 3DS.
- citro3d is stateful. Every call below only changes a copy in the citro3d context and marks it dirty; the register writes are emitted at the next draw call (`C3Di_UpdateContext`) or frame end, so a mistake shows up as wrong output or a hang later, not at the call ([pipeline](PICA200/pica200-command-pipeline.md#what-a-draw-call-writes)).

## Lighting

### Setup order

1. `C3D_LightEnvInit(&env)` then `C3D_LightEnvBind(&env)` (only one environment is bound at a time).
2. Set a material with `C3D_LightEnvMaterial(&env, &mtl)` (`C3D_Material`: `ambient`, `diffuse`, `specular0`, `specular1`, `emission`, three floats each) and optionally `C3D_LightEnvAmbient(&env, r, g, b)` (scene ambient, default 1.0).
3. Attach look-up tables with `C3D_LightEnvLut(&env, lutId, input, negative, lut)`.
4. Create up to eight lights with `C3D_LightInit(&light, &env)` (returns the slot 0-7, or -1 when all eight are taken), then set colour, position and optionally spot and attenuation.
5. In the vertex shader, output the quaternion and view vector; in the colour combiner, use the lighting result as a source ([shader programming](PICA200/pica200-shader-programming.md#5-driving-fixed-function-lighting)).

```c
#include <3ds.h>
#include <citro3d.h>

static C3D_LightEnv lightEnv;
static C3D_Light light;
static C3D_LightLut lutPhong;
static C3D_LightLutDA lutAtten;

static void setupLighting(void)
{
	C3D_Material mtl = {
		{ 0.2f, 0.2f, 0.2f },  /* ambient   */
		{ 0.8f, 0.8f, 0.8f },  /* diffuse   */
		{ 0.5f, 0.5f, 0.5f },  /* specular0 */
		{ 0.0f, 0.0f, 0.0f },  /* specular1 */
		{ 0.0f, 0.0f, 0.0f },  /* emission  */
	};
	C3D_LightEnvInit(&lightEnv);
	C3D_LightEnvBind(&lightEnv);
	C3D_LightEnvMaterial(&lightEnv, &mtl);

	LightLut_Phong(&lutPhong, 30.0f);                       /* x^30 */
	C3D_LightEnvLut(&lightEnv, GPU_LUT_D0, GPU_LUTINPUT_NH, false, &lutPhong);

	C3D_LightInit(&light, &lightEnv);
	C3D_LightColor(&light, 1.0f, 1.0f, 1.0f);
	C3D_FVec pos = FVec4_New(0.0f, 0.0f, -1.0f, 1.0f);      /* w = 1: positional */
	C3D_LightPosition(&light, &pos);
	LightLutDA_Quadratic(&lutAtten, 0.0f, 10.0f, 0.05f, 0.01f);
	C3D_LightDistAttn(&light, &lutAtten);
}
```

### What each call does

| Call | Effect | Register(s) |
| ---- | ------ | ----------- |
| `C3D_LightEnvInit` | zeroes the environment; scene ambient 1.0; config0 = `(4<<8) \| BIT(27) \| BIT(31)` (highlight clamping on); config1 = all ones (every LUT, spot, shadow and attenuation switched off); SP LUT input = `GPU_LUTINPUT_SP`; abs flags `0x2222222` | `LIGHTING_CONFIG0/1` (`0x1C3`, `0x1C4`) |
| `C3D_LightEnvMaterial` / `C3D_LightEnvAmbient` | the scene colour register becomes `clamp(255 * (emission + ambient * sceneAmbient))` per channel, packed with 10 bits per channel (blue first) | `LIGHTING_AMBIENT` (`0x1C0`) |
| `C3D_LightEnvLut(env, id, input, negative, lut)` | stores the LUT (NULL switches it off), sets which dot product feeds it, and whether a negative input is allowed. `negative = false` sets the "take abs()" flag, so the table is indexed 0..1; `negative = true` clears it and the table covers -1..1. CP input forces light-environment configuration 7 | `LIGHTING_LUTINPUT_SELECT/ABS` (`0x1D1`, `0x1D0`), LUT data via `LIGHTING_LUT_INDEX/DATA` |
| `C3D_LightEnvFresnel(env, sel)` | Fresnel selector (`GPU_NO_FRESNEL`, primary, secondary, both alpha) | config0 bits 2-3 |
| `C3D_LightEnvBumpMode/BumpSel/BumpNormalZ` | bump or tangent mapping mode (`GPU_BUMP_NOT_USED`, `AS_BUMP`, `AS_TANG`), which texture unit holds the map, whether the Z of the normal is recalculated | config0 bits 28-29, 22-23, 30 |
| `C3D_LightEnvShadowMode/ShadowSel` | which outputs the shadow attenuation touches (`GPU_SHADOW_PRIMARY`, `SECONDARY`, `INVERT`, `ALPHA`), and which texture unit is the shadow map; setting any of primary/secondary/alpha also sets bit 0 | config0 bits 16-19, 0, 24-25 |
| `C3D_LightEnvClampHighlights` | clamp specular highlights | config0 bit 27 |
| `C3D_LightInit` | picks the first free of eight slots; light enabled; diffuse and both speculars default to white, ambient to zero | slot id selects `LIGHT0..7` block (`0x140 + 0x10*i`) |
| `C3D_LightEnable` | adds or removes the light from the enabled list | `LIGHTING_NUM_LIGHTS` (`0x1C2`), `LIGHTING_LIGHT_PERMUTATION` (`0x1D9`) |
| `C3D_LightColor` / `Diffuse` / `Specular0` / `Specular1` / `Ambient` | each channel is `(u32)(255 * material * light)`, packed 10 bits apart, blue first. The product is not clamped, so values above 1.0 spill into the neighbouring channel | `LIGHTi_DIFFUSE/SPECULAR0/SPECULAR1/AMBIENT` |
| `C3D_LightPosition(light, &pos)` | `pos.w == 0` makes it a directional light (the vector is a direction), otherwise positional; coordinates converted to 16-bit float | `LIGHTi_CONFIG` bit 0, `LIGHTi_XY/Z` |
| `C3D_LightTwoSideDiffuse`, `C3D_LightGeoFactor(light, id&1, on)` | two-sided diffuse; the two geometric factors | `LIGHTi_CONFIG` bits 1, 2-3 |
| `C3D_LightSpotDir(light, x, y, z)` | negates and normalises the direction, stores 2.11 fixed point, and switches the spot term on | `LIGHTi_SPOTDIR_XY/Z`, config1 bit 8+i |
| `C3D_LightSpotLut(light, lut)` / `C3D_LightSpotEnable` | the per-light spot table (NULL turns spot off) | SP LUT 8+i, config1 |
| `C3D_LightDistAttn(light, &lutDA)` / `C3D_LightDistAttnEnable` | per-light distance attenuation table and its bias/scale | `LIGHTi_ATTENUATION_BIAS/SCALE`, DA LUT 16+i |
| `C3D_LightShadowEnable` | per-light shadow switch | config1 bit i |

Only environment-wide LUTs (D0, D1, FR, RB, RG, RR) go through `C3D_LightEnvLut`. The spot (SP) and distance-attenuation (DA) tables are per light: calling `C3D_LightEnvLut(env, GPU_LUT_SP, ...)` only sets the input selector and stores no table, so use `C3D_LightSpotLut` and `C3D_LightDistAttn` `[documented: citro3d lightenv.c, the `ids[]` table maps SP and DA to -1]`.

### Table helpers

| Helper | Meaning |
| ------ | ------- |
| `LightLut_FromArray(lut, float[512])` | 256 values then 256 differences; converts to the 12+12-bit entry format (value x 0x1000 clamped to 0xFFF; difference x 0x800, sign in bit 11) |
| `LightLut_FromFunc(lut, func, param, negative)` | samples `func(x, param)`: non-negative tables use x = i/256 for i in 0..255, signed tables use x = i/128 for i in -128..127, indexed `i & 0xFF` |
| `LightLut_Phong(lut, shininess)` | `powf(x, shininess)`, non-negative table |
| `LightLut_Spotlight(lut, angle)` | step at `cos(angle)`, signed table |
| `LightLutDA_Create(...)`, `LightLutDA_Quadratic(lut, from, to, linear, quad)` | 256 samples of `1 / (1 + linear*d + quad*d²)` over `[from, to]`; stores `scale = 1/(to-from)`, `bias = -from*scale` which `C3D_LightDistAttn` writes to the light |

> Conflict: encoding of the lighting-LUT difference. See [the register page](../hardware/gpu-internal-registers.md#fragment-lighting-registers-0x140-0x1d9): 3dbrew says "absolute value", citro3d writes sign-magnitude. This page follows citro3d.

### Light environment configuration is chosen for you

At every update citro3d picks the lowest of the eight light-environment configurations 0-6 whose LUT set contains every LUT you enabled (SP counts as needed if any light has a spot, DA if any light has attenuation). If any LUT uses `GPU_LUTINPUT_CP`, configuration 7 is chosen `[documented: citro3d lightenv.c, C3Di_LightEnvSelectLayer]`. This is why CP only ever works "under configuration 7" ([quirk](../hardware/gpu-shading-features.md#the-gpu_lutinput_cp-quirk-anisotropic-reflection)). If you enable a LUT combination none of 0-6 provides, you land on 7 as well.

### Lighting gotchas

- With no enabled light, citro3d leaves the "number of lights" field at 0 (it subtracts 1 only when at least one light is enabled). What the hardware does with lighting enabled and 0 in that field is not documented here `[hypothesis]`: one light active.
- `C3D_LightEnvBind` does nothing outside an active citro3d context.
- A rebound or dirtied environment re-uploads every LUT (256 entries in 32 writes of 8 words each); avoid rebinding per draw.

## Fog

`C3D_FogGasMode(fogMode, gasMode, zFlip)` selects `GPU_NO_FOG`, `GPU_FOG` (LUT fog) or `GPU_GAS`, the gas density source and the Z flip, and writes them with `C3D_FogColor(0xAABBGGRR)` and `C3D_FogLutBind(&lut)` into `TEXENV_UPDATE_BUFFER` (masked write `0x7`), `FOG_COLOR` and the fog LUT (`FOG_LUT_INDEX` 0, then 128 words).

| Helper | Meaning |
| ------ | ------- |
| `FogLut_FromArray(lut, float[256])` | 128 values (clamped to [0,1), x 0x800) then 128 differences (13-bit two's complement, x 0x800) |
| `FogLut_Exp(lut, density, gradient, near, far)` | `exp(-(density * z)^gradient)` sampled at i/128, where `z = FogLut_CalcZ(depth, near, far) = far*near / (depth*(far-near) + near)` converts the depth value back to eye-space distance |

Gaps: citro3d at the pinned commit never writes `FOG_LINEAR_SLOPE/OFFSET` (`0x0E2`, `0x0E3`), so the "linear fog" mode of the hardware (mode 1) is not reachable through its API (`GPU_FOG` is mode 5). The same grep finds no write to `0x04C` (`DEPTHMAP_TRANSFORM_ENABLE`), `0x064` (`TEXUNIT_LOD_ENABLE`) or `0x06F` (`SH_OUTATTR_CLOCK`) in citro3d's sources; where those registers get set by a normal citro3d program is therefore not shown by these sources `[hypothesis]`: libctru's init or the GSP initial state.

## Procedural textures

| Call | Effect |
| ---- | ------ |
| `C3D_ProcTexInit(pt, offset, width)` | `offset`/`width` select the slice of the 256-entry colour table used as the 1D colour texture; sets the unknown fields to `0x60` and `0xE0C080` |
| `C3D_ProcTexNoiseCoefs(pt, C3D_ProcTex_U / _V / _UV, amplitude, frequency, phase)` | switches noise on; amplitude is stored x 0x1000 (fixed 1.3.12), frequency and phase as 16-bit floats |
| `C3D_ProcTexLodBias(pt, bias)` | texture bias split across `PROCTEX0` (low 8 bits) and `PROCTEX4` (high 8) |
| `C3D_ProcTexBind(texCoordId, pt)` | sets which texcoord (0-2) feeds unit 3, enables unit 3, uploads `PROCTEX0..5` (6 registers) |
| `C3D_ProcTexLutBind(GPU_LUT_NOISE / RGBMAP / ALPHAMAP, lut)` | binds one of the three 128-entry tables; `ProcTexLut_FromArray(lut, float[129])` builds it (value clamped to [0,1] x 0xFFF, difference = 12-bit two's complement of next - current) |
| `C3D_ProcTexColorLutBind(&colorLut)` | binds the 256-entry colour table; `ProcTexColorLut_Write(out, rgba, offset, width)` copies colours and builds the difference table (half of the signed per-channel difference; last entry 0) |

The pipeline (noise -> clamp -> combine -> LUT -> colour table) is described on [shading features](../hardware/gpu-shading-features.md#procedural-textures-texture-unit-3). The upload function `C3Di_ProcTexUpdate` returns immediately unless unit 3 is enabled (`texConfig` bit 10, set by `C3D_ProcTexBind`), so tables are uploaded only after a texture is bound. This matches fincs's observation that updating LUTs while a unit is off "appears to fail"; citro3d simply never tries `[documented: citro3d proctex.c]`.

## Gas

`C3D_GasBeginAcc`, `C3D_GasDeltaZ`, `C3D_GasAccMax`, `C3D_GasAttn`, `C3D_GasLightPlanar/View/Direction`, `C3D_GasLutInput` and `C3D_GasLutBind` (`GasLut_FromArray(lut, u32[9])`: 8 colours plus 8 sign-magnitude differences, first 8 words differences) set the gas registers `GAS_ACCMAX_FEEDBACK`, `GAS_DELTAZ_DEPTH`, `GAS_ACCMAX`, `GAS_ATTENUATION`, `GAS_LIGHT_XY/Z/Z_COLOR` and the 16-entry gas LUT at the next draw `[documented: citro3d gas.c]`. Gas is a two-stage effect: render density with `C3D_FragOpMode(GPU_FRAGOPMODE_GAS_ACC)`, then render the lit result with the fog unit in `GPU_GAS` mode. `C3D_GasAccMax(v)` writes `float16(1/v)`.

> Unknown: gas rendering is the least understood unit ([shading features](../hardware/gpu-shading-features.md#gas)). The register semantics in the table come from 3dbrew and citro3d's code; whether the density and light registers behave as the names suggest has not been checked.

A source-reading observation, not a run result: in `gas.c`, `conv_u8` computes `((u32)x * 255) << pos`. The cast binds before the multiplication, so any `x` below 1.0 truncates to 0 and only 1.0 gives 255. `C3D_GasLightPlanar`, `C3D_GasLightView` and `C3D_GasLightDirection` pass floats in [0,1] through it, so fractional intensities probably become 0. Pass 1.0 or 0.0 only, or write the registers yourself, until this is checked on hardware `[hypothesis]`.

## Render state (blend, depth, stencil, alpha, cull)

All of `effect.c` edits one `C3D_Effect` struct that is written out whenever it is dirty (`C3Di_EffectBind`): `DEPTHMAP_ENABLE`, `FACECULLING_CONFIG`, depth-map scale/offset, the alpha/stencil/depth test block, blend and logic-op registers, `COLOR_OPERATION`, `FRAGOP_SHADOW` and the early-depth registers.

| Call | Register fields built (all `[documented: citro3d effect.c]`) |
| ---- | -------------------------------------------------------------- |
| `C3D_DepthMap(isZBuffer, scale, offset)` | `DEPTHMAP_ENABLE` (`0x06D`) = 1 for Z-buffer, 0 for W-buffer; scale and offset as float24 into `0x04D/0x04E`. Common call: `C3D_DepthMap(true, -1.0f, 0.0f)` |
| `C3D_CullFace(GPU_CULL_NONE / FRONT_CCW / BACK_CCW)` | `FACECULLING_CONFIG` bits 0-1 |
| `C3D_DepthTest(enable, func, writeMask)` | enable, func << 4, writeMask << 8 (`GPU_WRITE_RED..ALPHA` bits 8-11, `GPU_WRITE_DEPTH` bit 12) into `DEPTH_COLOR_MASK`; also copies the depth function into the gas depth function field of `GAS_DELTAZ_DEPTH` via `GPU_MAKEGASDEPTHFUNC` |
| `C3D_AlphaTest(enable, func, ref)` | enable, func << 4, ref << 8 |
| `C3D_StencilTest(enable, func, ref, inputMask, writeMask)` | enable, func << 4, writeMask << 8, ref << 16, inputMask << 24; `C3D_StencilOp(sfail, dfail, pass)` fail, z-fail, z-pass at bits 0, 4, 8 |
| `C3D_AlphaBlend(colorEq, alphaEq, srcClr, dstClr, srcA, dstA)` | `BLEND_FUNC`; also selects blend mode in `COLOR_OPERATION` bit 8 |
| `C3D_ColorLogicOp(op)` | `LOGIC_OP`; selects logic-op mode instead of blending. The two are exclusive: calling one cancels the other |
| `C3D_BlendingColor(0xAABBGGRR)` | `BLEND_COLOR` |
| `C3D_FragOpMode(GPU_FRAGOPMODE_GL / GAS_ACC / SHADOW)` | `COLOR_OPERATION` mode bits 0-1 (and the `0xE40000` constant); `C3D_FragOpShadow(scale, bias)` writes `FRAGOP_SHADOW` = `f16(scale+bias) \| f16(-scale) << 16` |
| `C3D_EarlyDepthTest(enable, func, ref)` | `EARLYDEPTH_TEST1` and `TEST2` bit 0; `EARLYDEPTH_FUNC` written with mask 1, so only the low bit of the function reaches the register; `EARLYDEPTH_DATA` (mask 7) = ref |

## Render targets and immediate mode

- `C3D_CalcColorBufSize(w, h, fmt)` and `C3D_CalcDepthBufSize` multiply the pixel count by 2 plus `{2,1,0,0,0}[fmt]` (RGBA8 4 B, RGB8 3 B, others 2 B) and 2 plus `{0,0,1,2}[fmt]` (D16 2 B, D24 3 B, D24S8 4 B) `[documented: framebuffer.c]`; these feed the [VRAM budget](PICA200/gpu-rendering.md#render-target-memory-budget).
- `C3Di_FrameBufBind` invalidates the framebuffer cache, then writes depth/colour addresses (physical >> 3), `RENDERBUF_DIM` and `FRAMEBUFFER_DIM` (`0x01000000 | ((h-1) & 0xFFF) << 12 | (w & 0xFFF)`), buffer formats, `FRAMEBUFFER_BLOCK32` and the read/write enables (zero when a buffer is NULL). `C3D_FrameBufClear` uses the two memory-fill units (`GX_MemoryFill`) and `C3D_FrameBufTransfer` a `GX_DisplayTransfer` ([external registers](../hardware/gpu-external-registers.md)).
- Immediate mode: `C3D_ImmDrawBegin(prim)` writes `PRIMITIVE_CONFIG`, `RESTART_PRIMITIVE`, a dummy `INDEXBUFFER_CONFIG`, `GEOSTAGE_CONFIG2` bit 0 = 1, `START_DRAW_FUNC0` = drawing and `FIXEDATTRIB_INDEX` = `0xF`; each `C3D_ImmSendAttrib(x, y, z, w)` writes the four float24 values as three words; `C3D_ImmDrawEnd` restores configuration mode, clears bit 0 and clears the post-vertex cache. The attribute count is whatever the bound `C3D_AttrInfo` declares (3dbrew: set `VSH_NUM_ATTR`; all vertex-loader registers are ignored in this mode). Use it for a handful of vertices ([GPU rendering](PICA200/gpu-rendering.md#what-a-draw-call-costs)).
- Combiner buffer: `C3D_TexEnvBufUpdate(mode, mask)` picks which of stages 1-4 read the previous buffer (bits 8-11 RGB, 12-15 alpha of `TEXENV_UPDATE_BUFFER`) and `C3D_TexEnvBufColor` sets `TEXENV_BUFFER_COLOR`. Stages 4 and 5 are at register `0x0F0`/`0x0F8`, not at `0x0C0 + 8*n`: `C3Di_TexEnvBind` adds 2 to the stage index for them.

## Material state contracts

A material is the complete combination of vertex shader inputs/outputs, texture units, texture-environment (**[TEV](../glossary.md#tev)**) stages, lighting/fog, alpha/depth/stencil tests, blending, write masks and culling. It must establish its required state on entry; do not assume the previous material left the correct settings. This checklist describes intended behavior, not copy-ready or hardware-verified recipes:

| Material | State to decide together | Failure to check |
| -------- | ------------------------ | ---------------- |
| Opaque textured | Texture and UVs, TEV texture × vertex colour, depth test/write, culling | Wrong winding or stale tint; depth mismatch. |
| Cut-out sprite or foliage | Texture alpha, alpha comparison before depth write, depth and culling | Invisible texels occlude objects behind them. |
| Translucent water or glass | Blend factors, depth test, usually disabled depth writes, ordering | Incorrect layering when intersecting other transparent surfaces. |
| Additive glow | Source/destination factors, depth test/write and alpha policy | Glow leaks through walls or alters later composition. |
| Vertex-lit or lightmapped | Vertex colour or a second texture/UV set, corresponding TEV stages | Wrong modulation, UV misalignment or state leaking into the next draw. |
| Fragment-lit and fogged | Lighting environment, LUTs, normals and shader outputs; fog state and lookup table | Missing output attributes, incorrect highlights or fog depth. |

See [GPU rendering](PICA200/gpu-rendering.md#colour-combiners-tev-recipes), [lighting](#lighting) and [fog](#fog) for the API building blocks. Check each material over several backgrounds and after a different material; lightmap UVs, transparency ordering, alpha coverage and depth ties all need image comparisons. The wiki does not yet provide a complete validated material library or evidence that every desktop fragment effect can be represented in one pass.

## Pitfalls

- **Fractional gas light values** (see above) are probably truncated.
- **Material products over 1.0** spill into neighbouring colour channels because the colour registers are packed 10 bits apart without clamping.
- **Passing `negative` backwards.** `negative = true` means the LUT is indexed over -1..1 (spotlight); Phong-style tables use `false`.
- **Forgetting that SP/DA are per light**, not environment tables.
- **Expecting linear fog.** `FogLut_*` builds a LUT; the hardware's linear mode has no API.
- **Changing LUT contents without marking dirty.** Tables are copied to the GPU when the environment becomes dirty; changing a bound `C3D_LightLut` after the first draw has no effect until something dirties the environment (`C3D_LightEnvLut` again, or a re-bind) `[documented: citro3d lightenv.c flags]`.

## Open questions

- Run the lighting fragment on an Old 3DS and a New 3DS and record the result (nothing here has been run).
- Confirm the `conv_u8` truncation and the zero-light case on hardware.
- Which code sets `0x04C`, `0x064` and `0x06F` in a normal citro3d program.

## Sources

- citro3d `source/light.c`, `lightenv.c`, `lightlut.c`, `proctex.c`, `fog.c`, `gas.c`, `effect.c`, `framebuffer.c`, `immediate.c`, `texenv.c`, `base.c` and `include/c3d/*.h` at `9f21cf7` — every call/register mapping on this page.
- libctru `include/3ds/gpu/enums.h` at `62b7c42` — enum values (`GPU_LUT_*`, `GPU_PT_*`, `GPU_FRAGOPMODE_*`).
- [3dbrew: GPU/Internal Registers](https://www.3dbrew.org/wiki/GPU/Internal_Registers) — register field meanings.
- [3dbrew: GPU/Programming Guide](https://www.3dbrew.org/wiki/GPU/Programming_Guide) — immediate mode and fixed attributes.
- [fincs, "Random PICA200 RE Notes"](https://gist.github.com/fincs/543f7e1375815f495f10020a053f14e9) — the LUT-update-while-disabled quirk.
