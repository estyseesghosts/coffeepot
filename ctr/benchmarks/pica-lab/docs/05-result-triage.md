# Result triage: when does an experiment actually teach us something?

A measurement is useful only if it rejects at least one plausible explanation, or narrows a parameter range reproducibly. Document the negative and positive cases. Never infer undocumented architecture directly from a timing correlation.

## A five-level evidence ladder

1. **Captured:** complete hardware inputs, output, toolchain and console metadata; no model claims.
2. **Reproduced:** consistent across multiple samples, order reversals, and reboots; negative controls pass.
3. **Discriminated:** at least two numerical/pipeline hypotheses make *different predictions* for the exact input, and the capture rejects one.
4. **Generalized:** prediction holds for a second independently designed input family (different edge slope, framebuffer size, texture format, or opcode mix).
5. **Upstream-ready:** smallest reproducer, raw capture, expected bytes, alternative models, confidence bounds, code and documented caveats.

## Decision table

| Observed result | What can be said | What cannot yet be said | Next experiment |
|---|---|---|---|
| Raster masks change at some 1/64 pixel steps | A particular projected edge crossed an output threshold | PICA uses a 1/64 subpixel grid | Repeat with slopes, positions and 1/256 steps; fit competing lattice/rounding models |
| Two triangle splits disagree | This input is not invariant to the split with these states | Rasterizer has cracks or incorrect top-left convention | Disable blending, compare coverage-only masks, skew and rotate |
| TEV center output jumps by >1 | The final quantized result jumped | Intermediate TEV arithmetic has lower-than-8-bit precision | Design values where early-round and late-round models differ |
| 64 draws cost more than 1 draw | Additional draw calls increase total queue time under this case | A specific number of GPU cycles per register write | Sweep counts, preserve identical command bytes elsewhere, separate CPU preprocessing |
| Texture working-set growth slows rendering | This access pattern is sensitive to changing footprint | Texture cache capacity equals the threshold size | Hold unique sampled texels constant, vary stride/base address/format, check bank and transfer effects |
| Profiler counter follows vertex workload | Counter is correlated with vertex pressure | Counter equals vertex cycles | Test mixed overloads, idle, counter totals, and individual vertex op families |
| Added memory barrier changes output | Barrier affects results under this writer/reader sequence | Barrier is universally required | Test alternate writers, warm/cold reuse, independent staging and all synchronization orders |
| Emulator disagrees with hardware | Divergent behavior on this legal fixture | Emulator's rasterizer is at fault | Reduce commands, verify transfer, attribute input, shader output, TEV, blending and depth one by one |

## Confidence reporting

Every finding should include `claim`, `supported_observations`, `rejected_models`, `still_possible_explanations`, `versions`, `hardware_model`, `reproduction_rate`, `raw_data`, and `minimal_repro`. Use confidence labels: **observation** (pixels/timing only), **model supported** (predicts holdouts), or **mechanism unresolved** (multiple models survive). Never use "confirmed cache size" or "proven architecture" from a single microbenchmark.

## Suggested first ten-session campaign

Session 1: compile/boot, A self-test, verify raw capture semantics. Session 2: run contrasting-color and rotated geometry to establish channel/row order; add as a mandatory starter test. Sessions 3-4: R01/R02, including repeated boot, reverse order, and image diff. Sessions 5-6: C01/C02 with added adversarial 8-bit values; construct CPU references. Session 7: P03 warmup/ABBA and long/short batches. Session 8: read-only GPU register access check and GSP log feasibility. Sessions 9-10: render a minimum external test from yuriks/JayFoxRox locally and compare their documented expectations with actual Old 3DS captures.

Only after these ten sessions would it be responsible to interpret a texture-cache threshold or hotspot-counter distribution. The 'first ten sessions' are an experiment schedule, not a promise of results.

## A practical bug report that advances emulator accuracy

Include the full 256x256 hardware RGBA8 capture; a binary-diff image to the emulator result; shader assembly and compiled bytes; command-list bytes (once backend supports it); render-target format and clear state; input vertex/texture binaries; hardware identity and firmware; compiler and library versions; and the *smallest input where outputs differ*. Ensure the test is not secretly comparing different PPF or display-transfer configurations.

## Sources

- Project references and prior art: [sources and knowns](04-sources-and-knowns.md).
- Measured results so far: [state log](../STATE-LOG-2026-11-13.md).
