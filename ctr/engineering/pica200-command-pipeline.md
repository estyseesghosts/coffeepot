# PICA200 command pipeline: from C3D_DrawElements to the GPU

When a homebrew program calls `C3D_DrawArrays` or `C3D_DrawElements`, nothing is drawn at that moment. The call only appends register writes to a buffer in memory; the buffer is handed to the graphics chip ([PICA200](../glossary.md#pica200)) later, when the frame ends. This page follows that path step by step: how vertex descriptions become registers, what a draw call writes, how a write is encoded in words, how the finished **command list** reaches the GPU through the [GSP](../glossary.md#gsp) service, and a checklist for when the result is a hang, garbage or a crash. It is for developers debugging GPU problems, or writing tools that inspect GPU traffic.

## Before you start

- Source read: citro3d `9f21cf7` (v1.7.1) and libctru `62b7c42` (v2.7.1), the commits pinned on [source versions](source-versions.md). Register names come from libctru `include/3ds/gpu/registers.h` and [3dbrew GPU/Internal Registers](https://www.3dbrew.org/wiki/GPU/Internal_Registers).
- Applies to both [Old and New 3DS](../glossary.md#old-new-3ds) as far as the sources show; nothing here was run on hardware or an emulator.
- Hardware background is on [CTR GPU](../hardware/gpu.md); the library workflow on [picasso and citro3d](../homebrew/picasso-and-citro3d.md) and [GPU rendering](gpu-rendering.md).

## The path in one picture

```text
C3D_AttrInfo / C3D_BufInfo / uniforms / texenv   (CPU-side state in C3D_Context, dirty flags)
        |  C3D_DrawArrays / C3D_DrawElements
        v
C3Di_UpdateContext(): emit register writes only for dirty state
        v
GPUCMD_Add*: append "parameter, header" word pairs to gpuCmdBuf (linear memory)
        v   C3D_FrameEnd -> C3Di_SplitFrame -> GPUCMD_Split (+FINALIZE, 16-byte align)
GX_ProcessCommandList(addr, size_bytes, flags)  -> gxCmdQueue (citro3d binds one)
        v   gspSubmitGxCommand: 32-byte GX command into a shared-memory slot
GSP module (separate process) -> GPU "P3D" command-list processor reads the buffer
        v   P3D interrupt when FINALIZE is reached
gxCmdQueueInterrupt -> next queued GX command (display transfer, ...)
```

## Describing vertices: AttrInfo and BufInfo

`C3D_AttrInfo` says what each vertex attribute is; `C3D_BufInfo` says where the arrays live. Neither writes registers until the next draw, when `C3Di_UpdateContext` (`source/base.c`) sees their dirty flags and calls `C3Di_AttrInfoBind` / `C3Di_BufInfoBind` [documented: citro3d `base.c`, `attribs.c`, `buffers.c`].

| Step | Register(s) written | Value built by citro3d |
| ---- | ------------------- | ---------------------- |
| `AttrInfo_AddLoader(info, reg, fmt, n)` | `GPUREG_ATTRIBBUFFERS_FORMAT_LOW/HIGH` (0x201-0x202, one incremental write) | per attribute id: `((n-1)<<2 \| fmt) << (id*4)`; HIGH also holds the fixed-attribute mask (bits 16-27, starts all ones, cleared per loader) and `attrCount-1` in bits 28-31 |
| attribute-to-shader-input map | `GPUREG_VSH_ATTRIBUTES_PERMUTATION_LOW/HIGH` (0x2BB-0x2BC) | `regId << (id*4)` |
| attribute count | `GPUREG_VSH_NUM_ATTR` (0x242), masked write of `GPUREG_VSH_INPUTBUFFER_CONFIG` (0x2B9) | `attrCount-1`; `0xA0000000 \| (attrCount-1)` with mask `0xB` |
| `BufInfo_Init` | (CPU only) | `base_paddr = 0x18000000` |
| `BufInfo_Add(info, data, stride, n, perm)` | (CPU only; max 12 buffers) | `offset = physical(data) - base_paddr`; flags0 = low 32 bits of permutation; flags1 = high bits \| `stride<<16` \| `n<<28` |
| bind | `GPUREG_ATTRIBBUFFERS_LOC` (0x200) = `base_paddr>>3`, then one incremental write of 36 words from `GPUREG_ATTRIBBUFFER0_OFFSET` (0x203) | 12 buffers x (`OFFSET`, `CONFIG1`, `CONFIG2`) |

`CONFIG1`/`CONFIG2` carry the component order (nibbles, with 12-15 meaning 4/8/12/16 bytes of padding), the bytes per vertex (bits 16-23 of `CONFIG2`) and the component count (bits 28-31) [reverse-engineered: 3dbrew GPU/Internal Registers]. `BufInfo_Add` returns -2 if the pointer's physical address is below the base, which is what a pointer into the normal heap looks like ([memory](../hardware/memory.md)).

## What a draw call writes

Both calls first run `C3Di_UpdateContext()` (flushing any dirty state as writes), then emit the draw sequence below [documented: citro3d `drawArrays.c`, `drawElements.c`]. Masked writes use `GPUCMD_AddMaskedWrite(reg, mask, value)`.

| # | `C3D_DrawArrays(prim, first, n)` | `C3D_DrawElements(prim, count, type, idx)` |
| - | -------------------------------- | ------------------------------------------ |
| 1 | `PRIMITIVE_CONFIG` mask 2 = prim | same, but `GPU_TRIANGLES` is written as `GPU_GEOMETRY_PRIM` |
| 2 | `RESTART_PRIMITIVE` = 1 | same |
| 3 | `INDEXBUFFER_CONFIG` = `0x80000000` (unused but required) | `(physical(idx) - base_paddr) \| (type << 31)` |
| 4 | `NUMVERTICES` = n | `NUMVERTICES` = count |
| 5 | `VERTEX_OFFSET` = first | `VERTEX_OFFSET` = 0 |
| 6 | `GEOSTAGE_CONFIG2` mask 1 = 1 (array mode) | triangles only: `GEOSTAGE_CONFIG` and `GEOSTAGE_CONFIG2` mask 2 = `0x100` |
| 7 | `START_DRAW_FUNC0` mask 1 = 0 (drawing mode) | same |
| 8 | **`DRAWARRAYS` = 1** | **`DRAWELEMENTS` = 1** |
| 9 | `START_DRAW_FUNC0` mask 1 = 1 (config mode) | same |
| 10 | `GEOSTAGE_CONFIG2` mask 1 = 0 | triangles only: both `GEOSTAGE_CONFIG*` cleared |
| 11 | `VTX_FUNC` = 1 (clear post-vertex cache) | same, then two masked writes of 0 to `PRIMITIVE_CONFIG` (mask 8) |

Both set `C3DiF_DrawUsed`, which makes the next framebuffer change or frame split add `FRAMEBUFFER_FLUSH` / `FRAMEBUFFER_INVALIDATE` / `EARLYDEPTH_CLEAR` writes (`base.c`, `C3Di_SplitFrame`). The two trailing `PRIMITIVE_CONFIG` writes are the "two dummy writes" the wiki already lists as required ([shading features](../hardware/gpu-shading-features.md#behaviours-and-quirks-found-while-reverse-engineering)). 3dbrew's [Programming Guide](https://www.3dbrew.org/wiki/GPU/Programming_Guide) lists a different order for the "standard GL" sequence; citro3d's order above is what this wiki's source pin actually emits.

`C3D_DrawElements` silently returns without drawing if `osConvertVirtToPhys(indices)` is below `bufInfo.base_paddr` (`drawElements.c`: `if (pa < base) return;`). Index data must therefore sit in linear memory, like the vertices.

## How a write is encoded

libctru's `GPUCMD_AddInternal` (`source/gpu/gpu.c`) stores each logical write as a **parameter word first, then a header word**, with any further parameters after the header [documented: libctru `gpu.c`, `gpu.h`].

| Header bits | Meaning |
| ----------- | ------- |
| 0-9 | register id (libctru masks with `0x3FF`; 3dbrew describes bits 0-15 as "command ID") |
| 16-19 | byte-lane write mask (`0xF` = whole word; lane k enabled by bit 16+k) |
| 20-27 | number of **extra** parameters (count - 1) |
| 28-30 | unused |
| 31 | consecutive-write mode: parameter k goes to register id + k |

- A command is `2 + extra` words; if that is odd, one zero pad word follows so every command is 8-byte aligned.
- `GPUCMD_Add` splits writes longer than `0x100` parameters; in incremental mode the register id advances by the chunk length.
- `GPUCMD_Split` appends `GPUREG_FINALIZE` (0x010) = `0x12345678`, and a second one if the word count is not a multiple of 4, so the list is 16-byte aligned. It then moves `gpuCmdBuf` past the finished list.
- 3dbrew: after the first `FINALIZE` the GPU ignores the rest of the buffer, and a buffer without one hangs the GPU [reverse-engineered: 3dbrew GPU/Internal Registers].

Illustrative words (derived by hand from the rules above, not captured from a console): `GPUCMD_AddWrite(GPUREG_NUMVERTICES, 3)` is `0x00000003, 0x000F0228`. `GPUCMD_AddMaskedWrite(GPUREG_PRIMITIVE_CONFIG, 2, 0x100)` is `0x00000100, 0x0002025E`.

## Frame end, GX and the GSP

1. `C3D_FrameBegin` points libctru at citro3d's command buffer (`GPUCMD_SetBuffer`, default `0x40000` bytes from `linearAlloc`, `C3D_Init`).
2. `C3D_FrameEnd(flags)` runs the end hook, then `C3D_FrameSplit(flags)` -> `C3Di_SplitFrame` -> `GPUCMD_Split`, then `GX_ProcessCommandList(buf, words*4, flags)` [documented: citro3d `renderqueue.c`, `base.c`].
3. citro3d binds a `gxCmdQueue` (`GX_BindQueue`), so that call only stores a 32-byte GX command (`0x01000101`, address, size, flag words) in the queue. After the display transfers are queued, `gxCmdQueueRun` submits up to three queued commands at a time with `gspSubmitGxCommand`; each P3D/PPF/PSC0/DMA completion interrupt (not PSC1 or V-blank) releases the next (libctru `gx.c`, `gxqueue.c`).
4. `gspSubmitGxCommand` copies the command into one of 15 slots of the thread's shared-memory queue (`gspSharedMem + 0x800 + thread*0x200`) and, if the queue was empty, triggers the GSP (`GSPGPU_TriggerCmdReqQueue`) (libctru `gspgpu.c`).
5. The flags: bit 0 of `flags` goes to GX word 3 (gas accumulation update); `GX_CMDLIST_FLUSH` (bit 1) goes to word 7, and libctru's comment says that makes the GSP flush the list from the CPU cache. Without it, `C3D_FrameEnd` flushes the whole linear heap itself ([memory and cache](memory-and-cache.md#the-c3d_frameend-flush)).

What the GSP module then does with the command is not in the sources read for this page. The wiki's [GPU register page](../hardware/gpu.md#registers) lists the P3D command-list processor at external register `0x18E0` (size >> 3, address >> 3, bit 0 start); that the GSP programs exactly this for a ProcessCommandList command is `[hypothesis]` here, confirmable from 3dbrew's GSP/GX pages or a GSP disassembly.

## Diagnosis checklist

Start from the symptom. "Check" columns are things to read or compute, not claims about a particular failure.

| Symptom | Likely cause | Check |
| ------- | ------------ | ----- |
| Program blocks in `C3D_FrameBegin`, CPU otherwise alive | GPU never finished the previous list (no P3D interrupt) | Use `C3D_FRAME_NONBLOCK` and log; bisect draws; run the checks below ([debugging](debugging.md#gpu-hangs)) |
| Hang after changing vertex format | stride != real vertex size, or padding missing | Compare `BufInfo_Add` stride with the attribute sizes and alignment; run the decoder's stride hint ([3dbrew Pitfalls](https://www.3dbrew.org/wiki/GPU/Pitfalls)) |
| Hang, shader recently edited | an output component written twice or not at all; two adjacent `mova` | Read the shader against [shader programming](pica200-shader-programming.md#debugging-decision-tree) |
| Hang after switching shader program | attribute/format registers not re-sent | whether citro3d re-sends these on a program change was not checked; custom code must resend `FORMAT_LOW`, `VSH_INPUTBUFFER_CONFIG`, `VSH_NUM_ATTR`, permutation ([shading features](../hardware/gpu-shading-features.md#behaviours-and-quirks-found-while-reverse-engineering)) |
| Draw silently missing (`DrawElements`) | index pointer not in linear memory (`pa < base`, early return) | Allocate with `linearAlloc`; check `osConvertVirtToPhys(idx) >= 0x18000000` |
| Crash (`svcBreak` panic) while recording | command buffer full: `GPUCMD_AddInternal` panics when `offset + n + 1 > size` | `C3D_GetCmdBufUsage()` after a frame; raise the size in `C3D_Init` |
| Memory corruption near the list | `GPUCMD_AddRawCommands` has **no** bounds check (`memcpy`) | Prefer `GPUCMD_Add*`; check sizes before raw adds |
| Stale or half-drawn data | CPU cache not flushed for vertex/texture data | With `GX_CMDLIST_FLUSH` you must flush your own buffers; otherwise rely on the full flush ([memory and cache](memory-and-cache.md)) |
| Hang on first frame | list lacks `FINALIZE` (hand-built lists) | Decode the list: the tool warns when none is present |
| Odd draw state after a hand-written write | wrong mask, wrong register id, written after `FINALIZE` | Decode the list and read each write against 3dbrew |

## Host-side command-list decoder

Design: take the raw words of one list (the exact buffer passed to `GX_ProcessCommandList`), walk `parameter, header, extra...` using the rules above, name each register from the libctru table, and apply a few cheap structural checks. Capturing the words needs a small hook; the fragment below dumps the list at the start of `C3D_FrameEnd`, before `FINALIZE` is added, so the decoder will report "no FINALIZE" for it, which is expected. Status: builds with devkitARM r68 / libctru 2.7.1 headers under `-Wall -Wextra` (compiled only; never run).

```c
#include <3ds.h>
#include <citro3d.h>
#include <stdio.h>

static void dumpCmdList(void* param)
{
	FILE* f = fopen((const char*)param, "wb");
	if (!f)
		return;
	fwrite(gpuCmdBuf, sizeof(u32), gpuCmdBufOffset, f);
	fclose(f);
}
/* in setup: C3D_FrameEndHook(dumpCmdList, (void*)"sdmc:/cmdlist.bin"); */
```

The decoder is [`tools/pica_cmdlist_decode.py`](../../tools/pica_cmdlist_decode.py) (`python3 -I tools/pica_cmdlist_decode.py LIST.bin`, `--hex`, `--selftest`). It prints each command, every register write, each draw trigger with `NUMVERTICES`, and warnings: truncated commands, non-zero pad words, header bits that should be zero, mask 0, registers outside libctru's table, commands after `FINALIZE`, missing `FINALIZE`, and a stride check against the attribute format.

> **Tested on synthetic data only.** Its 13 self-tests (`--selftest`, all pass) encode lists with a Python re-implementation of libctru's encoder and decode them back. The "draw" list is a subset of what `C3D_DrawArrays` writes, assembled by hand. It has never seen a real capture, so the rules it checks are only as good as the source reading above.

## Technical details

- Register ids and names: `registers.h`. Draw triggers `0x22E` / `0x22F`, `NUMVERTICES` `0x228`, `INDEXBUFFER_CONFIG` `0x227`, `FINALIZE` `0x010`.
- `GX_ProcessCommandList` command: word0 `0x01000100 | 1`, word1 address, word2 size in bytes, word3 `flags & 1`, word7 `(flags >> 1) & 1` (libctru `gx.c`). `GX_CMDLIST_FLUSH` is `BIT(1)`.
- GSP shared queue: 15 slots of 8 words; submit returns `-2` when 15 are pending (`gspgpu.c`).
- citro3d keeps `ctx->cmdBufUsage` (fraction of the buffer used) after each split (`base.c`).
- Format fields: libctru reads bits 0-1 as type (byte, ubyte, short, float) and bits 2-3 as component count - 1; 3dbrew labels the same two bits as a size of 8/16/24/32 bits. The decoder follows libctru.

## Pitfalls

- **Assuming a draw call talks to the GPU.** It only appends to memory; errors surface at the frame end or on the next `C3D_FrameBegin`.
- **Splitting a frame and forgetting the flush mode.** `C3D_FrameSplit` passes flags straight to `GX_ProcessCommandList`.
- **Trusting the 3dbrew draw order over the source.** Use citro3d's emitted order when comparing a capture.

## Open questions

- Real captures: dump a frame's list with the hook above, decode it, and compare against the tables here; this would also validate the decoder.
- What the GSP does with a ProcessCommandList command, and whether the P3D registers are written as listed in `gpu.md`.
- Whether an overfull stride always hangs or sometimes draws nothing; needs an Old/New 3DS experiment.

## Sources

- citro3d `source/base.c`, `attribs.c`, `buffers.c`, `drawArrays.c`, `drawElements.c`, `renderqueue.c` at `9f21cf7` — state binding, draw sequences, frame end.
- libctru `source/gpu/gpu.c`, `gx.c`, `gxqueue.c`, `source/services/gspgpu.c`, `include/3ds/gpu/gpu.h`, `registers.h` at `62b7c42` — encoding, split, GX queue, register names.
- [3dbrew: GPU/Internal Registers](https://www.3dbrew.org/wiki/GPU/Internal_Registers), [GPU/Programming Guide](https://www.3dbrew.org/wiki/GPU/Programming_Guide), [GPU/Pitfalls](https://www.3dbrew.org/wiki/GPU/Pitfalls) — header layout, register meanings, pitfalls.
