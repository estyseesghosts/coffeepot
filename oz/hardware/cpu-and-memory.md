# CPU and memory

Switch 2's main processor runs game logic, input, audio work and commands for the graphics processor. CPU and GPU share system memory, so allocations and transfers compete for the same physical resource. The installed memory capacity is not a promise that any particular homebrew process can use all of it.

## Where it fits

The NVIDIA **[T239](../glossary.md#t239)** system-on-a-chip connects the CPU, [GPU](gpu.md), memory and device controllers. Software obtains memory through its process environment; see [system and security](../software/system.md).

## How it works

[Reverse-engineered] SwitchBrew identifies eight Arm Cortex-A78C cores and 12 GB of LPDDR5X memory. These are hardware figures, not the number of cores or amount of memory granted to an application. Thread affinity and process capabilities must be checked in the environment actually used.

## Technical details

| Item | Published information | Qualification |
| ---- | --------------------- | ------------- |
| CPU | 8 Cortex-A78C cores | [reverse-engineered] SwitchBrew; availability depends on process permissions |
| Cache | 256 KB L2 per core; 4 MB shared L3 | [reverse-engineered] SwitchBrew; do not substitute configurable Arm reference values |
| RAM | 12 GB LPDDR5X, 128-bit interface | [reverse-engineered] SwitchBrew; shared, not an application heap limit |
| Bandwidth | Approximately 102 GB/s theoretical peak | [reverse-engineered] SwitchBrew; not a measured application throughput |

The Arm manual describes possible A78C configurations, not all choices made in T239. [Reported] Eglantine's notes describe four examined native executables with 39-bit address spaces and an affinity mask of `0x3f` for cores 0–5. This is a sample, not a universal homebrew entitlement. The notes also identify newer atomic and authenticated-return instructions; a port or emulator must implement the instructions its actual binary uses.

## Homebrew notes

Query available memory and thread permissions through the chosen runtime; do not hard-code a retail title's budget. Measure thread scaling and contention on hardware, including handheld and docked conditions, before increasing worker counts.

## Sources

- [SwitchBrew T239](https://switchbrew.org/wiki/Switch_2:_Tegra_T239) — reverse-engineered chip configuration.
- [Arm Cortex-A78C technical reference](https://documentation-service.arm.com/static/6193c9bef45f0b1fbf3a85dd) — architecture and configurable implementation details.
- [Eglantine NX2 notes](https://github.com/Eglantine-Community/Eglantine/blob/nx2/NX2_NOTES.md) — sampled process metadata and instruction observations.
