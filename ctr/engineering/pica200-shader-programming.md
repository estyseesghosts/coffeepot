# PICA200 shader programming: from one triangle to lighting

The PICA200 runs small programs (**[shaders](../../glossary.md#shader)**) on each vertex, and optionally on each triangle. There is no programmable pixel stage; the pixel side is fixed-function ([GPU](../hardware/gpu.md)). This page teaches vertex and geometry shaders in the order a developer meets them: a single triangle, matrices in uniforms, branches and loops with the address registers, a geometry shader, and finally the outputs that drive fixed-function lighting. It ends with the instruction encoding, descriptor limits, float behaviour and a debugging decision tree. The tool reference is [picasso and citro3d](../homebrew/picasso-and-citro3d.md); hardware facts are on [GPU](../hardware/gpu.md) and [shading features](../hardware/gpu-shading-features.md). Neither is repeated here.

## Before you start

- Every shader below was **assembled** with picasso 2.7.2 (`picasso -o x.shbin file.pica`) from the pinned manual `d522455` ([source versions](source-versions.md)). Assembly proves the syntax and operand rules only. **No shader here has been run on hardware or an emulator**, so what each draws is expected, not observed.
- The C fragment in "Loading a shader" compiled under `-Wall -Wextra` with devkitARM r68 and libctru/citro3d 2.7.1/1.7.1 headers (compile only).
- Registers, in brief: inputs `v0-v15`, outputs `o0-o15`, scratch `r0-r15`, float uniforms `c0-c95`, integer uniforms `i0-i3`, booleans `b0-b15` (picasso) and address registers `a0.x`, `a0.y`, `aL`. Details: [picasso and citro3d](../homebrew/picasso-and-citro3d.md#how-it-works).

## 1. A single triangle

Position and colour pass straight through; the vertex data must already be clip-space coordinates. `.out` ties an output register to a meaning; picasso allocates the registers. The vertex shader must write all four components of position.

```text
; s1_tri.v.pica  (assembled with picasso 2.7.2)
.constf consts(1.0, 0.0, 0.0, 0.0)
.alias  one consts.xxxx

.out outpos position
.out outclr color

.alias inpos v0
.alias inclr v1

.proc main
	mov outpos.xyz, inpos
	mov outpos.w,   one
	mov outclr,     inclr
	end
.end
```

Result of assembly: 4 instruction words, 3 operand descriptors, outputs position (register 0) and colour (register 1) (parsed from the `.shbin` with the layout libctru's `shbin.c` reads). Expected on screen: a coloured triangle, if attributes are `v0` = 3 floats, `v1` = colour and the colour combiner passes the vertex colour ([combiner recipes](gpu-rendering.md#colour-combiners-tev-recipes)). `[hypothesis]` until run.

## 2. Scale, rotate, translate with uniforms

A **uniform** is a register the CPU fills before drawing. Matrices go in four consecutive float uniforms, and `dp4` against each row yields one output component. The `shaderInstanceGetUniformLocation` call finds the register by name.

```text
; s2_srt.v.pica  (assembled)
.fvec projection[4], modelView[4]

.constf consts(1.0, 0.0, 0.0, 0.0)
.alias  one consts.xxxx

.out outpos position
.out outtc0 texcoord0
.out outclr color

.alias inpos v0
.alias intex v1

.proc main
	mov r0.xyz, inpos
	mov r0.w,   one

	dp4 r1.x, modelView[0], r0
	dp4 r1.y, modelView[1], r0
	dp4 r1.z, modelView[2], r0
	dp4 r1.w, modelView[3], r0

	dp4 outpos.x, projection[0], r1
	dp4 outpos.y, projection[1], r1
	dp4 outpos.z, projection[2], r1
	dp4 outpos.w, projection[3], r1

	mov outtc0, intex
	mov outclr, one
	end
.end
```

Assembled: 13 words, 8 descriptors. Loading and drawing (compiled, not run; `vshader_shbin` comes from the template Makefile's `bin2s` rule; frame and render-target setup are on [GPU rendering](gpu-rendering.md#anatomy-of-a-frame)):

```c
#include <3ds.h>
#include <citro3d.h>

extern const u8  vshader_shbin[];
extern const u32 vshader_shbin_size;

static DVLB_s*         vshaderDvlb;
static shaderProgram_s program;
static int             uLocProjection, uLocModelView;
typedef struct { float x, y, z; float u, v; } Vertex;

static void sceneInit(void)
{
	vshaderDvlb = DVLB_ParseFile((u32*)vshader_shbin, vshader_shbin_size);
	shaderProgramInit(&program);
	shaderProgramSetVsh(&program, &vshaderDvlb->DVLE[0]);
	C3D_BindProgram(&program);
	uLocProjection = shaderInstanceGetUniformLocation(program.vertexShader, "projection");
	uLocModelView  = shaderInstanceGetUniformLocation(program.vertexShader, "modelView");

	C3D_AttrInfo* attr = C3D_GetAttrInfo();
	AttrInfo_Init(attr);
	AttrInfo_AddLoader(attr, 0, GPU_FLOAT, 3); /* v0 = position */
	AttrInfo_AddLoader(attr, 1, GPU_FLOAT, 2); /* v1 = texcoord */
	/* ... linearAlloc a Vertex array, BufInfo_Add(buf, vbo, sizeof(Vertex), 2, 0x10) ... */
}

static void sceneDraw(void)
{
	C3D_Mtx projection, modelView;
	Mtx_OrthoTilt(&projection, 0.0f, 400.0f, 0.0f, 240.0f, 0.0f, 1.0f, true);
	Mtx_Identity(&modelView);
	Mtx_Translate(&modelView, 10.0f, 0.0f, 0.0f, true);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocModelView, &modelView);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocProjection, &projection);
	C3D_DrawArrays(GPU_TRIANGLES, 0, 3);
}
```

The uniforms and attributes become register writes as described on [the command pipeline](pica200-command-pipeline.md#describing-vertices-attrinfo-and-bufinfo). picasso's `-h` option also writes `VSH_FVEC_<name>` register numbers into a header.

**The wide-source rule.** An instruction's second source can only be an input or scratch register; only the first (wide) source may be a uniform. picasso rejects `add r0.xyz, v0.xyz, offset.xyz` with `invalid source2 register: offset` and `add r0, v0, v1` with `source operands must be different input registers (v0..v15)` (both reproduced with picasso 2.7.2). Swap the operands, or `mov` one into a scratch register first.

## 3. Branches, loops and address registers

`ifu`/`callu`/`jmpu` test a boolean uniform. `for` runs a loop controlled by an integer uniform `iN` = (iterations-1, start of `aL`, step). `mova` loads `a0.x`/`a0.y` from a float, truncating to integer; `aL` is the loop counter. Only float uniforms can be indexed.

```text
; s3_loop.v.pica  (assembled)
.fvec projection[4]
.fvec bones[8]
.ivec loopParams
.bool useBone

.constf consts(1.0, 0.0, 0.0, 0.0)
.alias  one consts.xxxx
.alias  zero consts.yyyy

.out outpos position
.out outclr color

.alias inpos  v0
.alias boneId v1

.proc main
	mov r0.xyz, inpos
	mov r0.w, one

	ifu useBone
		mova a0.x, boneId.x
		add r0.xyz, bones[a0.x].xyz, r0.xyz
	.end

	mov r2, zero
	for loopParams
		add r2.xyz, bones[aL].xyz, r2.xyz
	.end

	dp4 outpos.x, projection[0], r0
	dp4 outpos.y, projection[1], r0
	dp4 outpos.z, projection[2], r0
	dp4 outpos.w, projection[3], r0
	mov outclr, r2
	end
.end
```

Assembled: 14 words, 8 descriptors, four uniforms in the table. It is a contrived example, written to exercise syntax. Rules to keep (3dbrew Shader Instruction Set, wiki-confirmed in picasso's manual):

- Out-of-range relative addressing: an offset outside -128..127 is ignored; the index is masked with `0x7F`; if the result is above 95 the read gives (1, 1, 1, 1).
- Never put two `mova` back to back, and avoid `break` inside a nested if/call inside a loop (3dbrew). picasso inserts padding `nop`s after block ends unless told `-n`.
- Control-flow stacks are small: 3dbrew lists a 4-deep call stack, an 8-deep if stack and a 4-deep loop stack (it says "four stacks" but names three; see the conflict in Technical details), and says an overflow silently discards the oldest entry. `break` on an empty loop stack hangs the GPU `[reverse-engineered: 3dbrew, authorship not stated on the page]`.

## 4. A geometry shader

A geometry shader sees a whole primitive and emits vertices of its own. With `.gsh point c0` it receives the vertex-shader outputs of each vertex in order from `v0`; here two outputs per vertex x three vertices, so the stride is 6 and the draw uses `GPU_GEOMETRY_PRIM` ([`3ds-examples` geoshader](https://github.com/devkitPro/3ds-examples/tree/be2001fee08cf8ec7d3366e095e83f6f75da8942/graphics/gpu/geoshader) does the same). `setemit` picks the output vertex slot (0-2), `prim` also emits the triangle, and `emit` sends the vertex.

```text
; s4_vs.v.pica  (assembled together with s4_gs.g.pica into one .shbin, two DVLEs)
.constf consts(1.0, 0.0, 0.0, 0.0)
.alias  one consts.xxxx
.out outpos position
.out outclr color
.proc main
	mov outpos.xyz, v0
	mov outpos.w,   one
	mov outclr,     v1
	end
.end
```

```text
; s4_gs.g.pica  (assembled)
.gsh point c0
.fvec scaleU
.constf consts(1.0, 0.0, 0.0, 0.0)
.out outpos position
.out outclr color

.entry gmain
.proc gmain
	setemit 0
	mul outpos, scaleU, v0
	mov outclr, v1
	emit

	setemit 1
	mul outpos, scaleU, v2
	mov outclr, v3
	emit

	setemit 2, prim
	mul outpos, scaleU, v4
	mov outclr, v5
	emit
	end
.end
```

Together: 17 words, 3 descriptors, DVLE 0 is a vertex shader and DVLE 1 a geometry shader. C side: `shaderProgramSetVsh(&p, &dvlb->DVLE[0]); shaderProgramSetGsh(&p, &dvlb->DVLE[1], 6);` (from the example; not compiled here). Geometry-shader uniforms are allocated separately from vertex-shader ones, so look uniforms up on `program.geometryShader`. `b15` is set after each geometry-shader run, which a shader can use to detect first-run state (picasso manual). Other modes (`variable`, `fixed`) and their register layouts: [picasso and citro3d](../homebrew/picasso-and-citro3d.md#how-it-works).

## 5. Driving fixed-function lighting

Per-pixel lighting needs two extra outputs from the vertex shader: `normalquat` (a quaternion describing the surface frame) and `view` (the vector from the surface to the eye, here `-modelView * position`). Everything else (lights, [LUTs](../../glossary.md#lut), materials) is set up on the C side with citro3d's `light`/`lightenv` modules ([shading features](../hardware/gpu-shading-features.md#fragment-lighting)). The shader below is the one from `3ds-examples/graphics/gpu/fragment_light`, reassembled here with picasso 2.7.2 (432 bytes, 30 words, 12 descriptors, five outputs).

```text
; s5_light.v.pica  (assembled; same logic as the fragment_light example's shader)
.fvec projection[4], modelView[4]
.constf myconst(0.0, 1.0, -1.0, 0.5)
.alias  zeros myconst.xxxx
.alias  ones  myconst.yyyy
.alias  half  myconst.wwww

.out outpos position
.out outtc0 texcoord0
.out outclr color
.out outview view
.out outnq normalquat

.alias inpos v0
.alias intex v1
.alias innrm v2

.proc main
	mov r0.xyz, inpos
	mov r0.w,   ones

	dp4 r1.x, modelView[0], r0
	dp4 r1.y, modelView[1], r0
	dp4 r1.z, modelView[2], r0
	dp4 r1.w, modelView[3], r0

	mov outview, -r1

	dp4 outpos.x, projection[0], r1
	dp4 outpos.y, projection[1], r1
	dp4 outpos.z, projection[2], r1
	dp4 outpos.w, projection[3], r1

	mov outtc0, intex

	dp3 r14.x, modelView[0], innrm
	dp3 r14.y, modelView[1], innrm
	dp3 r14.z, modelView[2], innrm
	dp3 r6.x, r14, r14
	rsq r6.x, r6.x
	mul r14.xyz, r14.xyz, r6.x

	mov r0, myconst.yxxx
	add r4, ones, r14.z
	mul r4, half, r4
	cmp zeros, ge, ge, r4.x
	rsq r4, r4.x
	mul r5, half, r14
	jmpc cmp.x, degenerate

	rcp r0.z, r4.x
	mul r0.xy, r5, r4

degenerate:
	mov outnq, r0
	mov outclr, ones
	end
.end
```

The quaternion construction (a rotation taking +Z to the normal, with a special case for normals pointing at -Z) is the example author's code; the example's own comment says a separate normal matrix would be more correct. The wiki has not derived or checked the maths.

## Output mapping

picasso turns `.out` lines into an output table; libctru's `DVLE_GenerateOutmap` (`source/gpu/shbin.c`) converts it to the semantic bytes written to `GPUREG_SH_OUTMAP_O*` [documented: libctru `shbin.c`]:

| Output | Semantic base | Components | Extra |
| ------ | ------------- | ---------- | ----- |
| `position` | `0x00` | 4 | |
| `normalquat` | `0x04` | 4 | clock bit 24 |
| `color` | `0x08` | 4 | clock bit 1 |
| `texcoord0` | `0x0C` | 2 | clock bit 8, outmap mode 1 |
| `texcoord0w` | `0x10` | 1 | clock bit 16, mode 1 |
| `texcoord1` | `0x0E` | 2 | clock bit 9, mode 1 |
| `texcoord2` | `0x16` | 2 | clock bit 10, mode 1 |
| `view` | `0x12` | 3 | clock bit 24 |

Components get consecutive semantic numbers in write-mask order. Only outputs present in the table are enabled, and disabled ones take no slot ([3dbrew Pitfalls](https://www.3dbrew.org/wiki/GPU/Pitfalls): `OUTMAP_MASK` `0x11` -> `O0 = 0x03020100`, `O1 = 0x0B0A0908`). Each enabled component must be written exactly once per run.

## Technical details

### Binary format

`.shbin` = a `DVLB` header (magic, DVLE count, offsets), one `DVLP` (code words, operand descriptors) and one `DVLE` per entry with its main/end offsets, constants, outputs, uniforms and symbol names; libctru's `DVLB_ParseFile` is the reference parser (`shbin.c`). Sizes measured on the assembled files above (parsed with that layout):

| File | Bytes | Code words | Descriptors | DVLEs |
| ---- | ----- | ---------- | ----------- | ----- |
| s1_tri | 192 | 4 | 3 | 1 |
| s2_srt | 316 | 13 | 8 | 1 |
| s3_loop | 340 | 14 | 8 | 1 |
| s4_vs + s4_gs | 364 | 17 | 3 | 2 |
| s5_light | 432 | 30 | 12 | 1 |

### Instruction encoding and limits (3dbrew GPU/Shader Instruction Set)

| Format | Used by | Fields (bit offset, width) |
| ------ | ------- | -------------------------- |
| 1 | two-source ALU | desc 0:7, src2 7:5, src1 12:7, idx1 19:2, dst 21:5, opcode 26:6 |
| 1i | `*I` forms (uniform as source 2) | desc 0:7, src2 7:7, src1 14:5, idx2 19:2, dst 21:5, opcode 26:6 |
| 1u | one-source (`mov`, `rcp`...) | desc 0:7, src1 12:7, idx1 19:2, dst 21:5, opcode 26:6 |
| 1c | `cmp` | desc, src2, src1, idx1, cmpY 21:3, cmpX 24:3, opcode 27:5 |
| 2 | conditional flow | num 0:8, dst 10:12, condop 22:2, refy 24, refx 25, opcode 26:6 |
| 3 | `*U`, `loop` | num 0:8, dst 10:12, constant id 22:4, opcode 26:6 |
| 4 | `setemit` | winding 22, primemit 23, vtx id 24:2, opcode 26:6 |
| 5 / 5i | `mad` | desc 0:5, src3, src2, src1, idx, dst 24:5, opcode 29:3 |

Opcodes: `add 0x00`, `dp3 01`, `dp4 02`, `dph 03`, `dst 04`, `ex2 05`, `lg2 06`, `litp 07`, `mul 08`, `sge 09`, `slt 0A`, `flr 0B`, `max 0C`, `min 0D`, `rcp 0E`, `rsq 0F`, `mova 12`, `mov 13`, `dphi 18`, `dsti 19`, `sgei 1A`, `slti 1B`, `break 20`, `nop 21`, `end 22`, `breakc 23`, `call 24`, `callc 25`, `callu 26`, `ifu 27`, `ifc 28`, `loop 29`, `emit 2A`, `setemit 2B`, `jmpc 2C`, `jmpu 2D`, `cmp 2E-2F`, `madi 30-37`, `mad 38-3F`. Operand descriptor (a word per distinct operand pattern): destination mask bits 0-3 (x = bit 3), then per source a negate bit and an 8-bit swizzle (x=0..w=3, component 0 in the top two bits; `0x1B` = `.xyzw`). Source numbers 0x00-0x0F inputs, 0x10-0x1F scratch, 0x20-0x7F float uniforms. Destinations 0x00-0x0F outputs, 0x10-0x1F scratch. Comparison operators: eq, ne, lt, le, gt, ge (6 and 7 appear to be "true"). Conditions: `condop` 0 or, 1 and, 2 x only, 3 y only.

Limits: "not more than 512 instructions and 128 operand descriptors" comes from debug text in the *Ironfall: Invasion* binary quoted by 3dbrew and matches picasso's constants (see the conflict below); it is not a documented hardware figure. Operand descriptors are referenced by 7 bits (5 for `mad`), so `mad` can only use the first 32 descriptors; picasso handles descriptor allocation `[hypothesis: not checked in picasso source]`.

> Conflict: boolean uniform count (b0-b7 vs b0-b15). 3dbrew contradicts itself: the register table on its Shader Instruction Set page lists `b0-b15`, the prose a few paragraphs later says "eight boolean constant registers (b0-b7)", and its Internal Registers page documents `GPUREG_SH_BOOLUNIFORM` bits for `b0` through `b15`. picasso's manual says `b0` through `b15`; libctru and citro3d write `0x7FFF0000 | bits` to `GPUREG_VSH_BOOLUNIFORM` / `GSH_BOOLUNIFORM` (`shaderProgram.c`, `uniforms.c`), and citro3d clears `b15` for the geometry-shader run flag. Resolution: the register definition and both toolchains say 16; the "eight" sentence looks like stale prose. Follow 16 for register layout `[documented: libctru, citro3d, picasso Manual.md]`. Whether `b8-b15` work as branch conditions in every shader unit is untested here `[hypothesis]`; a one-branch test shader per bit would settle it.

> Conflict: number of control-flow stacks. 3dbrew's Control Flow section says "four independent stacks" and then lists only CALL (4-deep), IF (8-deep) and LOOP (4-deep); its priority list (LOOP, IF, CALL, JMP, original PC) also names only those three plus the jump. Resolution: treat it as three stacks; "four" may count the PC path or be a leftover, and the page does not say. The depths, the oldest-entry-discarded overflow rule, the hang on `BREAK` with an empty loop stack and the note that the fourth consecutive CALL pop is missed ("probably a hardware bug") are 3dbrew `[reverse-engineered]`, with no author or hardware revision stated. picasso and libctru contain no stack-depth limits to cross-check. Unresolved: whether a fourth stack exists, and the exact depths.

> Conflict: the 512-instruction limit. 3dbrew says debug text in the *Ironfall: Invasion* `code.bin` "suggests" at most 512 instructions and 128 operand descriptors. The local sources split the claim: picasso defines `MAX_VSH_SIZE 512` and rejects a *vertex* shader above it ("instruction outside vertex shader code memory"; geometry shaders skip that check), and `MAX_OPDESC 128` for every shader ("too many operand descriptors") (`picasso.h`, `picasso_assembler.cpp`). Against that, `GPUREG_SH_CODETRANSFER_INDEX` and `OPDESCS_INDEX` are 12-bit fields (bits 0-11, 3dbrew Internal Registers), which could address 4096 words. Resolution: 512 / 128 are real toolchain limits and match the game's own assertion, so stay inside them; that they are hardware limits is shown by no local source, and the wider index field only shows the register is wider than the limit. Unresolved without a hardware test of a 513-word shader.

### Float24 behaviour

Shader numbers are 24-bit floats: 1 sign, 7 exponent, 16 mantissa bits (3dbrew; libctru `f32tof24` in `gpu.c` builds the same layout, truncating the mantissa, flushing exponent underflow to zero and saturating overflow to infinity [documented: libctru `gpu.c`]). 3dbrew's results "from some tests done on hardware" (the page text does not name who ran them):

| Computation | Result |
| ----------- | ------ |
| `inf * 0` (also inside `mad`, `dp4`) | 0 |
| `NaN * 0` | NaN |
| `+inf - +inf` | NaN |
| `rcp(0)`, `rcp(-0)` | +inf (there is no negative zero) |
| `rcp(+inf)` | 0 |
| `rsq(-2)` | NaN |
| `max(0, NaN)` / `max(NaN, 0)` | NaN / 0 (also `min`) |
| `cmp(s, 0)` for subnormal `s` | false: `cmp` does not flush inputs |
| `mul(s, 2)`, `mul(n, 0.5)` | 0: arithmetic flushes input and output subnormals (`s` largest subnormal, `n` smallest normal) |

Consequence: 1.0 can be halved 63 times before reaching zero. `mad` is not a fused multiply-add (the product is rounded) per 3dbrew. Treat this table as `[reverse-engineered]`, source 3dbrew, not reproduced by this wiki.

## Debugging decision tree

1. **Does it assemble?** Read the picasso error: `invalid source2 register` -> swap operands; `source operands must be different input registers` -> `mov` one input to a scratch first.
2. **GPU freezes on the first draw with this shader?**
   - Every enabled output component written exactly once on every path (including `ifu`/`jmpc` branches)? If a branch skips a write, the component is not written.
   - Any two `mova` adjacent? Insert a `nop` or reorder.
   - `break` outside a loop, or inside a nested block in a loop?
   - Attribute count, formats and stride consistent with the shader's inputs ([pipeline checklist](pica200-command-pipeline.md#diagnosis-checklist))?
3. **Nothing drawn, no freeze?** Is `outpos.w` non-zero (a missing `mov outpos.w, one` is common)? Is culling facing the right way? Is the matrix uniform actually uploaded (`shaderInstanceGetUniformLocation` returns a negative value for a name picasso did not expose, e.g. one that starts with `_`)?
4. **Wrong colours or lighting?** Output present in the `.out` list? `normalquat` and `view` both written for lighting? Colour combiner set to use the vertex colour or lighting result ([TEV recipes](gpu-rendering.md#colour-combiners-tev-recipes))?
5. **Geometry shader draws nothing or hangs?** Stride matches the vertices per primitive x outputs; `GPU_GEOMETRY_PRIM` used for the draw; `setemit` index 0-2 and one `prim` per triangle; vertex-shader outputs line up with `v0...` in order.
6. **Numeric oddities?** Check the float24 table: flushed subnormals, no negative zero, `inf*0 = 0`.
7. **Still unclear?** Decode the command list with the [decoder tool](pica200-command-pipeline.md#host-side-command-list-decoder) and read the shader registers written, or reduce the shader to stage 1 and add instructions back.

## Pitfalls

- **Uniform as second source.** Rejected by picasso; swap operands.
- **Reading a different input register in one instruction.** Rejected by picasso; the hardware gives wrong results otherwise.
- **Counting on `b8-b15`.** See the boolean-count conflict above.
- **Copying shader snippets between vertex and geometry shaders.** `o7-o15` exist in vertex shaders only.

## Open questions

- Run each shader here on an Old and New 3DS and record what appears; until then every "expected" statement is a `[hypothesis]`.
- Who ran the float tests on 3dbrew and on which hardware revision.
- Boolean uniform count (8 vs 16) and the real number of control-flow stacks.
- Whether 512 instructions and 128 descriptors are hardware limits; test by assembling and running a 513-instruction shader.

## Sources

- [3dbrew: GPU/Shader Instruction Set](https://www.3dbrew.org/wiki/GPU/Shader_Instruction_Set) — formats, opcodes, descriptors, addressing, float tests, control flow.
- [3dbrew: GPU/Pitfalls](https://www.3dbrew.org/wiki/GPU/Pitfalls) — output mapping, `mova`, output-write rule.
- [picasso](https://github.com/devkitPro/picasso/blob/d522455ea59cd5cb9219c699b93eb7750039233c/Manual.md) at `d522455`, and the installed picasso 2.7.2 — syntax, directives, error messages, assembly of every example.
- libctru `source/gpu/shbin.c`, `shaderProgram.c`, `gpu.c` at `62b7c42` — shbin parsing, outmap, float24 conversion.
- [3ds-examples](https://github.com/devkitPro/3ds-examples/tree/be2001fee08cf8ec7d3366e095e83f6f75da8942/graphics/gpu) at `be2001f` — `geoshader`, `fragment_light`, `simple_tri`.
