# Investigating graphics and performance

This guide helps a developer decide what to measure when moving a renderer to Switch 2. Ampere documentation provides useful hypotheses, but a desktop GPU, a retail SDK and a homebrew runtime do not necessarily expose the same limits or features. No OZ performance measurements have been made by Coffeepot.

## Before you start

Use a launcher and graphics example that explicitly support the target firmware and environment. Record their revisions, console mode, display resolution, power conditions and build options. Confirm a correct image and clean shutdown before measuring. See [GPU](../hardware/gpu.md) and [homebrew environments](../homebrew/state-and-workflow.md).

## Establish a baseline

Capture a repeatable scene, warm up the application and record frame times, not just average frames per second. Separate CPU update, command recording, GPU execution and presentation only when the runtime exposes trustworthy timing for each. Record sample count, median and tail times, resolution and correctness. A timer around submission alone does not measure completed GPU work.

## Find the limiting work

Change one variable at a time: reduce render resolution to probe pixel and bandwidth pressure, simplify shaders to probe shading, reduce draw count to probe submission, or reduce geometry to probe geometry work. [Hypothesis] A resulting improvement identifies a useful direction, not a definitive hardware bottleneck; confirm with GPU timing or counters if available. Keep the same scene and compare image correctness.

## Manage memory and synchronization

Use the actual runtime's allocation, visibility, barrier and fence rules. Shared physical RAM does not make CPU writes automatically visible at every point or permit freeing a resource while queued GPU work uses it. Verify upload, consumption and reuse separately. The [NX synchronization guide](../../nx/engineering/gpu-memory-and-synchronization.md) explains the questions, but its API calls and guarantees apply to its NX backend.

Switch 2 adds explicit GPU memory attributes and a separate sharable-memory lock to the system call set, which is worth understanding before porting an NX memory model; see [system and security](../software/system.md).

## Technical details

NVIDIA's GA102 whitepaper describes Ampere scheduling, caches and specialized units. [Documented] It is a desktop implementation: cache sizes, unit counts, supported APIs and performance figures are not T239 measurements. Treat ray tracing, tensor operations, mesh shaders and variable-rate shading as unavailable until the chosen interface documents them and a test confirms the relevant path. NVIDIA confirms Switch 2 DLSS and hardware ray tracing at the platform level, not access from a particular homebrew launcher.

### Emulator behaviour is a hypothesis, not a result

[Reported] Eglantine is an experimental Switch 2 emulator derived from Eden and Yuzu. Its developers report running Switch 2 games, with compatibility, graphics accuracy and performance still evolving. Emulator implementations introduce shortcuts and provisional behaviour to get games running, so a successful emulator implementation does not prove that its model matches hardware.

The practical rule: a game that only runs after an emulator returns zero for an unknown IPC command has not established that the real service returns zero. The same applies to a homebrew program that only works under an emulator. Record which of the two produced a result.

### Distinguishing the sources of a limit

[Reverse-engineered] When a limit is reached, establish which of three things imposed it before optimizing for it: an API-advertised limit from the SDK, a driver-imposed limit from the Horizon graphics services, or a real hardware limit in the Ampere silicon. Only the last is a fixed property of the console. See [GPU](../hardware/gpu.md).

## Pitfalls

- A high output resolution does not mean the game rendered at that resolution; record render and presentation sizes separately.
- A faster single run can be noise, warmup or changed power conditions; repeat and retain raw data.
- An emulator's rendering or timing is not proof of hardware behavior. Compare images and logs on hardware when possible.
- A limit quoted from NVIDIA's desktop documentation is not a limit of this console.

## Open questions

Which GPU timers, counters, features and limits are exposed by each native homebrew runtime? What bandwidth, submission and synchronization costs occur under sustained handheld and docked workloads? Small controlled hardware benchmarks would answer these questions; the [benchmark plan](benchmark-plan.md) sets out what such a suite should measure.

## Sources

- [NVIDIA Ampere GA102 whitepaper](https://www.nvidia.com/content/dam/en-zz/Solutions/geforce/ampere/pdf/NVIDIA-ampere-GA102-GPU-Architecture-Whitepaper-V1.pdf) — architecture background and its scope.
- [NVIDIA Switch 2 announcement](https://blogs.nvidia.com/blog/nintendo-switch-2-leveled-up-with-nvidia-ai-powered-dlss-and-4k-gaming/) — platform graphics features.
- [NVN object research](https://github.com/seedds/nxnative/blob/main/docs/reference/nvn-objects.md) — observed SDK interfaces and limits.
- [Eglantine](https://github.com/Eglantine-Community/Eglantine) — the Switch 2 emulator and its development notes.
