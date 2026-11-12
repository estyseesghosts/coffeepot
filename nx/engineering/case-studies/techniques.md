# Case studies by engineering question

This page is for a developer who has a question (should I replace OpenGL, why is my renderer CPU-bound, how do I test a change honestly) and wants the case studies that bear on it, with what each one does and does not establish. The case studies are project histories; this page is the index into them. It does not restate their numbers, which are all `[reported]` unless a page says otherwise.

## Questions and the studies that answer them

| Question | Read | The transferable part |
| -------- | ---- | --------------------- |
| When should I replace OpenGL or Vulkan with `deko3d`? | [Demanding ports (USF4, NFSMW)](case-studies-demanding-ports.md), [SA2](case-study-sa2.md), [open-engine ports](case-studies-open-engines.md) | A driver's per-bind and per-draw CPU cost was the measured limit in the recompiled ports; no tree gives a threshold at which that happens in a native game |
| What makes a CPU-bound renderer slow? | [SA2](case-study-sa2.md), [Demanding ports](case-studies-demanding-ports.md), [Serious Engine](case-study-serious-engine.md) | Repeated state copying, hashing and command encoding on the render thread; Serious Engine's entries are structural `[hypothesis]`, not measurements |
| How do I avoid costly GPU readbacks? | [Eidos](case-study-eidos-ouroboros.md), [melonDS](case-study-melonds.md) | Answer picture questions from scene state or a GPU summary; composite on the GPU |
| How should I prepare shaders? | [Wind Waker HD](case-study-wind-waker-hd.md), [Demanding ports](case-studies-demanding-ports.md) | `deko3d` never compiles at run time; ports that compile on the console ship a prebuilt pipeline list |
| How do I test a change without fooling myself? | [Demanding ports](case-studies-demanding-ports.md), [Wind Waker HD](case-study-wind-waker-hd.md), [Eidos](case-study-eidos-ouroboros.md) | Alternate baseline and candidate runs, include a no-change control, report percentiles, prove the work happened; see [profiling](../profiling-and-benchmarks.md) |
| How much Switch-specific code does a port need? | [nxquake2](case-study-nxquake2.md), [dhewm3](case-study-dhewm3.md), [Godot and SuperTuxKart](case-study-godot-stk-engines.md), [open-engine ports](case-studies-open-engines.md) | SDL and GL ports need little; the Switch-specific parts are launch mode, input, audio and clocks; see [porting](../porting-existing-software.md) |
| When did splitting work across cores help? | [SA2](case-study-sa2.md), [melonDS](case-study-melonds.md), [Godot and SuperTuxKart](case-study-godot-stk-engines.md) | Pinning followed a profile; SA2's first lock-free queue was a regression; see [lifecycle and threading](../lifecycle-and-threading.md) |

## What kind of evidence a claim is

Treat a claim according to its class. A source observation is not a performance measurement: finding hundreds of draw calls in an engine shows its submission structure, not that the draws dominate its time on a Switch; finding an optimisation plan in a repository does not show it was implemented or worked.

| Class | Meaning | Permitted conclusion | Label |
| ----- | ------- | -------------------- | ----- |
| API contract | A versioned header or implementation states it | Documented for that version | `[documented]` |
| Source observation | A source tree contains the mechanism | The project implements it | `[documented]` (name the file) |
| Project measurement | The developers recorded it with conditions | Observed in that project, on that setup | `[reported]` |
| Independent reproduction | Coffeepot reproduced it | Observed here under stated conditions | `[measured]` |
| Hypothesis | An inference or proposed improvement | Worth testing; not a performance claim | `[hypothesis]` |

No case study on this wiki has been independently reproduced. The Eidos measurements come from a private document and cannot be inspected; a reproducible test with an anonymised trace would strengthen that study.

## A standard shape for a case study

When a study is added or reworked, give it: the project and exact revision; the original design (threads, graphics, memory, audio, storage); the problem and its measured symptom; the intervention (files, mechanism); the measurement (scene, clocks, resolution, before and after, sample count, uncertainty); how correctness was checked; the trade-offs (complexity, memory, latency, compatibility); where the technique transfers and where it does not; and what remains unknown. Planned changes, estimated gains and recorded gains stay separate: an estimated millisecond saving is not a fact about Maxwell.

Two cautions from the sources: NFSMW's factor of 1.627 for NVK timestamps is specific to that stack, not a general `deko3d` correction; and SA2's techniques address a heavy translated draw stream, so a native game with a simple submission path may gain nothing from them.

## Sources

- The [case studies](case-study-sa2.md) linked above, pinned on [source versions](../source-versions.md).
- [sa2-nx](https://github.com/ChanseyIsTheBest/sa2-nx) at `baf523b`, [nfsmw-nx](https://github.com/StevensND/nfsmw-nx) at `20f8dc1`, [SwitchWakerHD](https://github.com/centollOS/SwitchWakerHD) at `9aec4e3` — the measured-claim cautions above.
