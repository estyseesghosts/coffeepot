# picasso and citro3d (homebrew GPU programming)

To draw 3D graphics on a 3DS from homebrew you use two tools that work together: **picasso**, an assembler that turns hand-written vertex and geometry shaders into the binary the GPU runs, and **citro3d**, a C library that wraps the [PICA200](../hardware/gpu.md) in a friendlier, state-tracking interface. Both come from the devkitPro project (picasso and citro3d are maintained by fincs and others), and both ship with the standard 3DS toolchain.

Required: a build environment with `devkitARM` and `libctru` ([libctru](libctru.md)). Outcome: a `.shbin` ([SHBIN](../glossary.md#shbin)) shader binary and a program that draws with it.

## What you need

- `devkitARM` + `libctru` + `citro3d` installed (the `3ds-dev` metapackage from `dkp-pacman`).
- `picasso`, invoked by the example projects' Makefile rules: vertex shaders are `*.v.pica` files, geometry shaders `*.g.pica`, and each pair is assembled into a `.shbin` that is linked into the program as data.
- For images: `tex3ds`, which the example Makefiles call to convert images into `.t3x` texture files (citro3d loads them through `tex3ds.h`).

## How it works

### picasso: shader assembler

picasso reads one or more source files and writes a single `.shbin` file, with one **DVLE** (a shader entry) per source file unless the file is marked `.nodvle` (procedures are shared across files). It can also emit a C header. Options: `-o` output file, `-h` header file, `-n`/`--no-nop` to disable the automatic padding no-ops, `-v` version.

A DVLE is a **vertex shader** by default; the `.gsh` directive makes it a **geometry shader** in one of three modes:

| Mode | Meaning |
| ---- | ------- |
| `point` | Called per input according to the configured stride and permutation; data starts at `v0`. Works with array drawing and element drawing |
| `variable` (a.k.a. subdivision) | Processes variable-size primitives: `vtxNum` vertices with full attributes plus one or more extra vertices with positions only. On entry `c0` holds the primitive's total vertex count in all components. Element drawing only; each primitive in the index array is prefixed with its vertex count |
| `fixed` (a.k.a. particle) | Processes fixed-size primitives with `vtxNum` vertices; data arrives in float uniform registers starting at `arrayStartReg`. Element drawing only |

Registers (all hold 4-component vectors of 24-bit floats, except integer and boolean uniforms):

| Registers | Use |
| --------- | --- |
| `o0`–`o15` | Outputs (`o7`–`o15` vertex shaders only) |
| `v0`–`v15` | Inputs |
| `r0`–`r15` | Scratch |
| `c0`–`c95` | Floating-point vector uniforms |
| `i0`–`i3` | Integer vector uniforms (8-bit integers) |
| `b0`–`b15` | Boolean uniforms (in geometry shaders, `b15` is set to true after each run) |
| `a0.x`, `a0.y`, `aL` | Indexing registers for relative addressing |

Components can be named `xyzw`, `rgba` or `stpq` and swizzled (`r0.wwxy`); outputs take a write mask. Directives include `.proc`/`.end` (procedures), `.alias`, `.fvec`/`.ivec`/`.bool` (uniforms), `.constf`/`.consti`/`.constfa` (preloaded constants), `.in` (inputs), `.out` (wires an output to a property), `.entry`, `.nodvle`, `.gsh`, `.setf`/`.seti`/`.setb`. Output properties: `position`, `normalquat` (the quaternion used in fragment lighting), `color`, `texcoord0`/`texcoord0w`, `texcoord1`, `texcoord2`, `view` (for lighting) and `dummy`. Uniform names starting with `_` are kept private (not exposed in the DVLE uniform table), and `$` in a name is translated to `.`. Vertex-shader uniforms with the same name are shared across source files in one `.shbin`; geometry shaders each get their own uniform map; constants are never shared. Relative addressing (`a0.x`, `a0.y`, `aL`, e.g. `arr[aL+2]`) works only on floating-point vector uniforms. `.gsh` forms: `.gsh point firstReg`, `.gsh variable firstReg vtxNum`, `.gsh fixed firstReg arrayStartReg vtxNum`. Older picasso names `a0`/`a1`/`a2`/`lcnt` for the index registers are still accepted [documented: picasso `Manual.md`].

**Caveats the assembler works around** (from the manual):

- Control-flow statements may fail at the end of another block; picasso inserts padding no-ops.
- Two consecutive `mova` instructions freeze the GPU.
- Only one *input* register can be referenced reliably in the source operands of an instruction (referring to the same register several times is fine).
- Uniforms (`c0`–`c95`) can only be used where the instruction encoding has a wide source slot: in an ordinary two-operand instruction such as `add`, the uniform must be the **first** source. `add r0.xyz, inpos.xyz, offset.xyz` fails with "invalid source2 register"; `add r0.xyz, offset.xyz, inpos.xyz` assembles [documented: picasso 2.7.2 error, found while writing the [drawbench](../benchmarks/README.md#drawbench) shader]. For `dph`, `sge`, `slt` and `mad`, picasso picks an alternative encoding automatically when the operands allow it (see Technical details).

### Related shader tools

`aemstro` (smealum, last commit January 2015) is the "original" PICA200 shader disassembler/assembler in Python, written while the instruction set was still being reverse engineered; its own README says it is experimental, its assembler is out of date and cannot reassemble its own disassembly, and it recommends neobrain's `nihstro` or picasso for real projects. Treat it as history ([history](history.md)), not as a reference for the instruction encoding; use 3dbrew's shader instruction set page for that.

### citro3d: GPU library

citro3d "exposes hardware functionality in the way that is most natural and convenient to the GPU", deviating from OpenGL where that serves the hardware. Its modules mirror GPU blocks:

| Module | What it manages |
| ------ | --------------- |
| `base` / `renderqueue` | Initialisation (default command buffer `0x40000` bytes), per-frame begin/draw/end, render targets bound to a screen and eye, frame-rate limiting, GPU timing queries |
| `framebuffer` | Colour/depth buffers, size calculation, clearing |
| `attribs`, `buffers`, `drawArrays`, `drawElements`, `immediate` | Vertex attribute formats, vertex buffers, draw calls, immediate mode |
| `uniforms`, `mtxstack`, `maths` | Shader uniforms, matrix stack, vector/matrix helpers |
| `texture`, `tex3ds` | Textures (sizes 8–1024 per side, cube maps with 6 faces, mipmaps), `tex3ds` atlas support |
| `texenv` | The **6** texture-environment (colour-combiner) stages |
| `light`, `lightenv`, `lightlut` | Up to **8** lights, per-light and environment LUTs, 256-entry light LUTs |
| `fog`, `proctex`, `gas`, `effect` | Fog (128-entry LUT), procedural textures (see [shading features](../hardware/gpu-shading-features.md)), gas, blend/depth/stencil state |

A typical frame: begin a frame, choose a render target (top-left, top-right, bottom), bind a shader program and uniforms, set the texture-environment stages, draw, end the frame; citro3d copies the render target to the screen's LCD framebuffer with the transfer flags stored on the target. This matches the pipeline in [GPU](../hardware/gpu.md) and [displays](../hardware/display.md). A practical guide to frames, draw-call costs, batching, textures and combiners is on [GPU rendering](../engineering/gpu-rendering.md).

Three behaviours of the frame API matter for performance and correctness [documented: `source/renderqueue.c`]:

- `C3D_FrameBegin` waits for the GPU to finish the previous frame (or, with `C3D_FRAME_NONBLOCK`, returns `false`); with `C3D_FRAME_SYNCDRAW` it first waits for V-blank on both screens. `C3D_FrameRate(fps)` lowers that V-blank pacing.
- `C3D_FrameEnd(flags)` flushes the CPU cache over the **entire linear heap** unless `GX_CMDLIST_FLUSH` is passed, in which case only the command list is flushed and the program must flush its own vertex and texture data ([memory and cache](../engineering/memory-and-cache.md#the-c3d_frameend-flush)).
- `C3D_GetProcessingTime()` and `C3D_GetDrawingTime()` report, in milliseconds, the CPU time spent recording the frame and the GPU time (including display transfers) of the last completed frame ([performance](../engineering/performance.md#citro3ds-two-timers)).

## Technical details

- Hardware limits visible in citro3d: 6 combiner stages, 8 lights, 6 light-LUT slots, 256-entry light LUTs, 128-entry fog and procedural-texture mapping LUTs, 256-entry procedural colour/delta tables.
- picasso's instruction set mirrors the [shader instruction set](https://www.3dbrew.org/wiki/GPU/Shader_Instruction_Set) documented on 3dbrew; `mad` has two operand forms (narrow-wide-narrow or narrow-narrow-wide) and `dph`/`sge`/`slt` accept a narrow-then-wide form, which picasso detects automatically.
- Comparisons set two flags (`cmp.x`, `cmp.y`) using `eq`, `ne`, `lt`, `le`, `gt`, `ge`; conditional execution uses `&&`/`||` on them.
- Instruction set accepted by picasso: `nop`, `end`, `emit`, `setemit`, `add`, `dp3`, `dp4`, `dph`, `dst`, `mul`, `sge`, `slt`, `max`, `min`, `ex2`, `lg2`, `litp`, `flr`, `rcp`, `rsq`, `mov`, `mova`, `cmp`, `mad`, and flow control (`call`, `callc`, `callu`, `for`, `break`, `breakc`, `ifc`, `ifu`, `jmpc`, `jmpu`).
- Geometry shader vertex emission: `setemit vtxId, emitFlags` configures a vertex (`vtxId` 0–2; flags `prim` to also emit a primitive, `inv` to invert vertex order) and `emit` sends it.

## Limits and risks

- GPU hangs are easy to cause: strides must match the real vertex size and some configuration orders must be respected ([GPU](../hardware/gpu.md#known-pitfalls), [shading features](../hardware/gpu-shading-features.md#behaviours-and-quirks-found-while-reverse-engineering)).
- citro3d is stateful; commands are batched in a command buffer, so errors may show up as a frozen GPU rather than an immediate error ([debugging GPU hangs](../engineering/debugging.md#gpu-hangs)).
- `C3D_TexBind` marks the texture unit dirty even when the same texture is already bound, and every rebind clears the GPU texture cache at the next draw; deduplicate binds in your own code ([GPU rendering](../engineering/gpu-rendering.md#what-a-draw-call-costs)).

## Sources

- [picasso (devkitPro)](https://github.com/devkitPro/picasso) — `Manual.md`: registers, directives, geometry-shader modes, caveats.
- [citro3d (devkitPro)](https://github.com/devkitPro/citro3d) — headers and sources: module list, limits, frame API.
- [3dbrew: Homebrew Libraries and Tools](https://www.3dbrew.org/wiki/Homebrew_Libraries_and_Tools) — `citro3d`, `citro2d`, `picasso`, `nihstro`, `aemstro` descriptions.
- [aemstro (smealum)](https://github.com/smealum/aemstro) — README: scope and status of the original shader (dis)assembler.
- [3dbrew: GPU/Shader Instruction Set](https://www.3dbrew.org/wiki/GPU/Shader_Instruction_Set) — instruction encoding.
