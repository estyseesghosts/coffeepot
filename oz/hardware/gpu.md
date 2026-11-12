# GPU

Switch 2 uses an NVIDIA graphics processor to draw and compute. Its Ampere architecture differs substantially from the original Switch's Maxwell GPU. A hardware feature, a retail graphics interface and an interface available to a particular homebrew program are three separate things.

## Where it fits

The GPU shares [memory](cpu-and-memory.md) with the CPU and presents images to the [display](devices.md). Retail software uses a graphics stack including **[NVN2](../glossary.md#nvn2)** and system services; homebrew must use the interface actually provided by its [execution environment](../homebrew/state-and-workflow.md).

## How it works

NVIDIA documents hardware ray tracing and DLSS on Switch 2. [Documented] That statement does not establish that a homebrew launcher exposes either feature, nor that a desktop NVIDIA API or tool is available on the console. DLSS requires software integration in addition to suitable hardware.

## Technical details

### Three layers, not one

[Reverse-engineered] It helps to separate the layers, because understanding one does not mean you can program the other two.

| Layer | What it is |
| ----- | ---------- |
| Game graphics API | NVN2: resource creation, pipelines, shader programs, command buffers and synchronization |
| Horizon graphics services and driver | GPU memory registration, queues, synchronization primitives, display surfaces and presentation |
| T239 Ampere hardware | Shader execution, caches, graphics and compute engines, memory access, RT and tensor units |

The services in the middle layer are on the [graphics and system services page](../software/services.md).

### The NVN2 observations

[Reverse-engineered] nxnative's reverse engineering of retail SDK binaries reports concrete NVN2 differences:

| Feature | Observed NVN2 behavior |
| ------- | ----------------------- |
| API version | 231.x, against 55.x for the older NVN |
| Vertex bindings and attributes | 32 reported |
| Compute workgroup | 1,024-thread maximum reported |
| Shader code padding | 2 KB |
| Scratch granularity | 16 KB |
| Separate sampler bindings | 32 |
| Device queries | New entries through `0xA9` |

These are values extracted from particular SDK builds, not a hardware specification. The same research notes that some resource limits are obtained dynamically from the GPU, so static analysis of an SDK build cannot establish everything.

[Reverse-engineered] The GPU itself is relatively well established: a 1,536-core Ampere GPU; NVIDIA separately confirms dedicated ray-tracing and tensor hardware for Switch 2. The 1,536 count refers to shader arithmetic cores, not the specialized units or independent CPU cores; counts alone do not predict frame rate. Clock, power, memory traffic and the work submitted all matter.

[Documented] NVIDIA's GA102 whitepaper is useful background, but it describes a different implementation. Its cache capacities, unit counts and desktop API support cannot be transplanted to T239. Ampere includes different gaming and datacenter implementations; an A100 optimization guide is not a T239 specification.

### Telling limits apart

[Reverse-engineered] Three different things can impose a limit, and they need different evidence:

- An **API-advertised limit** is what the SDK reports. It is a contract the library offers.
- A **driver-imposed limit** is what the Horizon graphics services enforce. It can be tighter than the API and can change between firmware versions.
- A **real hardware limit** is what the Ampere silicon can do. It is the upper bound and is rarely the first thing you hit.

Query limits through the interface you actually have. A number quoted from SDK analysis, from a desktop GPU or from an emulator tells you which of the three you are looking at at best.

## Homebrew notes

Do not assume [NX deko3d](../../nx/homebrew/deko3d-and-libnx.md) can submit native Ampere work. Establish supported formats, allocation, synchronization and presentation from the selected runtime's examples and source first.

Treat an emulator's behaviour as a hypothesis about the hardware rather than a result. A game that only runs after an emulator returns zero for an unknown command has not established that the real service returns zero; the [graphics investigation guide](../engineering/graphics-and-performance.md) separates what to test from what is known.

The [graphics investigation guide](../engineering/graphics-and-performance.md) also covers how to establish a baseline before attributing a slowdown to a particular part of the GPU.

## Sources

- [SwitchBrew T239](https://switchbrew.org/wiki/Switch_2:_Tegra_T239) — GPU configuration.
- [NVIDIA Switch 2 announcement](https://blogs.nvidia.com/blog/nintendo-switch-2-leveled-up-with-nvidia-ai-powered-dlss-and-4k-gaming/) — DLSS and ray tracing.
- [NVIDIA Ampere GA102 whitepaper](https://www.nvidia.com/content/dam/en-zz/Solutions/geforce/ampere/pdf/NVIDIA-ampere-GA102-GPU-Architecture-Whitepaper-V1.pdf) — architectural background, not T239 specifications.
- [NVN object research](https://github.com/seedds/nxnative/blob/main/docs/reference/nvn-objects.md) — SDK-specific reverse engineering.
- [SwitchBrew GSV services](https://switchbrew.org/wiki/Switch_2:_GSV_services) — the graphics service interface.
