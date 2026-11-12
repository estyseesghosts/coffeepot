# GPU

Switch 2 uses an NVIDIA graphics processor to draw and compute. Its Ampere architecture differs substantially from the original Switch's Maxwell GPU. A hardware feature, a retail graphics interface and an interface available to a particular homebrew program are three separate things.

## Where it fits

The GPU shares [memory](cpu-and-memory.md) with the CPU and presents images to the [display](devices.md). Retail software uses a graphics stack including **[NVN2](../glossary.md#nvn2)** and system services; homebrew must use the interface actually provided by its [execution environment](../homebrew/state-and-workflow.md).

## How it works

NVIDIA documents hardware ray tracing and DLSS on Switch 2. [Documented] That statement does not establish that a homebrew launcher exposes either feature, nor that a desktop NVIDIA API or tool is available on the console. DLSS requires software integration in addition to suitable hardware.

## Technical details

[Reverse-engineered] SwitchBrew identifies an Ampere GPU with 1,536 CUDA cores. These are arithmetic lanes, not independent CPU cores; counts alone do not predict frame rate. Clock, power, memory traffic and the work submitted all matter. The GA102 Ampere whitepaper is useful background, but describes a different implementation: its cache capacities, unit counts and desktop API support cannot be transplanted to T239.

[Reverse-engineered] The nxnative NVN object notes compare particular SDK binaries and report 32 vertex bindings and attributes and a 1,024-thread compute workgroup limit. Those are observations of those builds, not a complete hardware or homebrew API contract. Query limits through the actual available interface where possible.

## Homebrew notes

Do not assume [NX deko3d](../../nx/homebrew/deko3d-and-libnx.md) can submit native Ampere work. Establish supported formats, allocation, synchronization and presentation from the selected runtime's examples and source first. The [graphics investigation guide](../engineering/graphics-and-performance.md) separates what to test from what is known.

## Sources

- [SwitchBrew T239](https://switchbrew.org/wiki/Switch_2:_Tegra_T239) — GPU configuration.
- [NVIDIA Switch 2 announcement](https://blogs.nvidia.com/blog/nintendo-switch-2-ai-powered-dlss-gaming/) — DLSS and ray tracing.
- [NVIDIA Ampere GA102 whitepaper](https://www.nvidia.com/content/dam/en-zz/Solutions/geforce/ampere/pdf/NVIDIA-ampere-GA102-GPU-Architecture-Whitepaper-V1.pdf) — architectural background, not T239 specifications.
- [NVN object research](https://github.com/seedds/nxnative/blob/main/docs/reference/nvn-objects.md) — SDK-specific reverse engineering.
