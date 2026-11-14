# Existing findings, confidence and source map

This page prevents wasting hardware time rediscovering established behavior. Sources are publicly available as of 2026-11-13. Citro3D, 3dbrew and emulator sources are documentation/evidence, not proof that undocumented behavior is identical on real console.

## Established or already investigated (verify only as controls)

- **Hardware topology:** PICA200 at ~268 MHz, three programmable vertex processors plus a primitive engine usable as fourth vertex processor in certain modes, four texture units including a dedicated procedural unit. 3dbrew Hardware: https://www.3dbrew.org/wiki/Hardware . Precise internal scheduling remains open.
- **Shader arithmetic:** uses float1.7.16 format (sign/exponent/mantissa), explicitly non-IEEE corner cases and flush-to-zero on some operations. MAD is not fused; intermediate result rounded. Details and output quirks are already tabulated: https://www.3dbrew.org/wiki/GPU/Shader_Instruction_Set . Don't call ordinary NaN tests novel.
- **Shader hazards:** each configured output component must be written exactly once; adjacent MOVAs freeze GPU; invalid vertex stride can hang the GPU. https://www.3dbrew.org/wiki/GPU/Pitfalls . Harness lint must reject these.
- **Hotspot profiler:** `0x1EF00064` enable, `0x1EF00068` interval control, `0x1EF0006C` FIFO; 4 words contain 8 x u16 counters. Interval length <3 appears unreliable. Each interval increments one counter; stage labels are educated guesses. https://www.3dbrew.org/wiki/GPU/External_Registers . The question is the *physical meaning*, sampling and restrictions, not discovering that there are eight counters.
- **Framebuffer cache:** explicit flush and invalidate commands exist; invalidate does not flush. https://www.3dbrew.org/wiki/GPU/Internal_Registers . Barrier semantics under particular operations remain worth testing.
- **PPF display transfer constraints:** tiled→linear transfer has width/height alignment and restricted format-conversion combinations. Unsupported configurations may lack completion interrupt or hang GPU. https://www.3dbrew.org/wiki/GPU/External_Registers . Keep a whitelist.
- **Citro3D render queue:** `C3D_GetDrawingTime()` records queued GPU time, including tasks chained in its queue; don't equate it to shader runtime. `C3D_SyncDisplayTransfer()` ensures queue ordering and a PPF completion wait when called outside a frame. See https://github.com/devkitPro/citro3d/blob/master/source/renderqueue.c .
- **Supported GSP performance log:** `GSPGPU_SetPerfLogMode` and `GSPGPU_GetPerfLog` exist; first frame may be invalid and cumulative durations must be differenced. See libctru header and https://rust3ds.github.io/ctru-rs/crates/ctru_sys/fn.GSPGPU_GetPerfLog.html . No published measurement is assumed.
- **GAS is not a blank page:** Citro3D already contains experimental GAS support, `source/gas.c`. https://github.com/devkitPro/citro3d/blob/master/source/gas.c . Its real-hardware semantics and emulator coverage remain research topics.
- **VRAM banks:** modern libctru exposes per-bank allocation. Citro3D's `C3D_RenderTargetCreate` explicitly attempts placing depth in the other bank from color. This is evidence that placement is part of the design, **not** proof of any particular throughput gain. https://github.com/devkitPro/citro3d/blob/master/source/renderqueue.c .

## Prior art to inspect before investing in each experiment

1. https://github.com/yuriks/3DS_gpu_tests — important existing test branches (vertex loading, color clamping, HILO8).
2. https://github.com/yuriks/tmp-3ds-depth-write-test — depth/stencil write behavior.
3. https://github.com/yuriks/3ds-gputest — GPU test code.
4. https://github.com/JayFoxRox/3ds-tests — numerous hardware experiments for Citra development.
5. https://github.com/MerryMage/3ds-tests — hardware testing reference.
6. https://github.com/wwylele/ctrhwtest — Citra-era system/hardware tests.
7. https://github-wiki-see.page/m/Zapeth/citra/wiki/Archive-of-3DS-Tests — index of old hardware tests; some original links may be stale.
8. https://github.com/devkitPro/3ds-examples/tree/master/graphics/gpu — known-good shaders, geometry examples, legal PICA state sequences.
9. https://github.com/azahar-emu/azahar — emulator interpreter/JIT/render paths; search comments with `TODO`, shader approximations, rasterization, GAS and command processor.
10. https://github.com/Tkibbs14/gc3ds — N3DS PICA measurements useful for generating questions but **not** interchangeable with Old 3DS data.
11. https://www.3dbrew.org/wiki/GPU/Programming_Guide — exact command-list semantics.
12. https://www.3dbrew.org/wiki/GPU/Primitive_Engine — underdocumented PE / variable-size primitive behavior.

## Avoid these accidental claims

- **'Profiler percentage = time stage X spent executing.'** The sampler may report *most active*, *stalling*, or *bottleneck* stage in a window, and samples may include idle or exclusion logic. Establish semantics first.
- **'Timing step proves N KiB cache.'** It could be format, texture layout, DMA, address-alignment, texture-coordinate cache or coverage effect. Only orthogonal scans support a cache hypothesis.
- **'Faster with different VRAM banks proves parallel banks.'** Allocation, hidden library state, transfer behavior and bank fallback all confound. Verify actual allocation and workload overlap.
- **'Bit-exact GPU floating-point output read from RGBA8.'** An RGBA8 output is rounded and clamped downstream. Recovering float24 requires an independent validated encoding/threshold mechanism.
- **'No discrepancy in emulator means emulator is accurate.'** A test may not distinguish alternative models. Build intentionally distinguishing cases.
- **'No visible change means a register is ignored.'** The feature may not be enabled or may require different state/inputs.
- **'New 3DS GPU benchmark equals Old 3DS.'** Different environment and clock modes may change behavior.
- **'State write without a crash is safe.'** PICA stalls and persistent corruption are documented for some texture combinations.

## Safety rules

**Green (default):** valid, supported Citro3D states; bounded vertices; standard pixel formats; no unknown register writes; predictable memory lifetimes.

**Amber (manual review):** raw command lists, documented external register profiling writes, shader precision near special values, controlled synchronization omissions, geometry shader edge cases.

**Red (not in unattended harness):** arbitrary register probing by write, unknown shader opcodes, intentionally incorrect strides/output mappings, nested shader stack overflow, invalid PPF dimensions, texture unit 1 with VRAM-backed L4 textures and other documented hang/corruption combinations.

Run only one amber experiment at a time with a deliberate reboot/recovery plan. Preserve SD data and close files before any risky work. Abort on first loss of deterministic control. If a physical unit shows persistent visual corruption, reboot and stop; don't run further tests to 'see if it clears.'

## What findings deserve upstream documentation?

- A reproducible mismatch in exact raster coverage across independently generated legal commands.
- A previously unknown barrier necessary for a deterministic read-after-write.
- A GL/PICA state edge case where console and a current emulator differ.
- A shader instruction input/output pair distinguishing two numerical models.
- A calibrated profiler counter interpretation supported by at least two different workload pairs.
- A legal, isolated state feature absent in the emulator (especially GAS or primitive engine output semantics).
- A reproducible performance ceiling with a useful rule for real game developers, plus all the controlled variables.

## Attribution convention

Each eventual finding should cite the exact public baseline that it extends, raw 3DS and Azahar results, tool versions, and a minimal test program. Put the new claim, previously known claim, and remaining uncertainty on separate lines. Never silently upgrade a documentation hypothesis into a fact.

## Sources

The numbered links and URLs above are the sources for this page. Measured results: [state log](../STATE-LOG-2026-11-13.md).
