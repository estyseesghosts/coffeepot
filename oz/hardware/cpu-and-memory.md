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
| Cache | 64 KB L1 instruction and 64 KB L1 data per core; 256 KB L2 per core; 4 MB shared L3 | [reverse-engineered] SwitchBrew; do not substitute configurable Arm reference values |
| RAM | 12 GB (2 × 6 GB) LPDDR5X-3200, 128-bit interface | [reverse-engineered] SwitchBrew; shared, not an application heap limit |
| Bandwidth | Approximately 102 GB/s theoretical peak | [reverse-engineered] SwitchBrew; not a measured application throughput |

The Arm manual describes possible A78C configurations, not all choices made in T239.

### Reading the permissions rather than assuming them

[Reported] Eglantine's notes describe four examined native executables with 39-bit address spaces and an affinity mask of `0x3f` for cores 0–5, and thread priority values of 28–59. This is a sample, not a universal homebrew entitlement.

The right way to find out what a process may do is to read its [NPDM](../glossary.md#npdm): it records the core affinity mask, the permitted services, the permitted system calls and the resource limits. A program that assumes the retail defaults will be wrong whenever the title or environment differs, and the compatibility environment is one case where they deliberately differ; see [compatibility mode](../software/compatibility.md).

### Memory encryption

[Reverse-engineered] The T239 appears to support memory encryption, similar to the T234 used in NVIDIA's Orin products. Memory in certain carveout regions is encrypted by the memory controller, transparently on write and read. SwitchBrew reports application memory is not encrypted and suggests a possible performance reason; that rationale is not verified.

This is a security property rather than a performance one, and it is described in full on the [security page](../software/security.md).

### Instructions and atomics

[Reported] The same Eglantine notes identify newer atomic and authenticated-return instructions in the sampled executables. A port or emulator must implement the instructions its actual binary uses, not the instruction set the marketing name implies.

[Documented] The A78C implements Armv8.3-A pointer authentication and Armv8.6-A enhanced pointer authentication. An emulator or translator that does not correctly implement an instruction used by the binary can fail; ordinary AArch64 code need not contain authenticated returns.

### Researching the chip further

[Reverse-engineered] The T239 has many aspects in common with the T234 SoC found in NVIDIA's Orin products, and NVIDIA publishes Orin documentation that can inform reverse engineering. Its registers, firmware and memory configuration cannot be assumed identical to T239's, but it is the closest public relative.

### Moving code from the original Switch

The original Switch's processor is a Cortex-A57, so an existing NX port is already written for a different microarchitecture. When moving between the two CPUs, revisit these topics and measure the workload:

| Subject | What to revisit |
| ------- | --------------- |
| Thread scheduling | Eight physical cores against the six-core application affinity masks seen in Switch 2 software |
| Cache organization | The L1/L2/L3 hierarchy, cache-line behaviour and sharing between threads |
| Atomics | Large system extensions (LSE) instructions against older load-exclusive/store-exclusive loops |
| Branch prediction | Why code layout and unpredictable branches still matter |
| Memory | Cache misses, prefetching, alignment, contention and bandwidth |
| Power | Why peak frequency is not sustained performance |

## Homebrew notes

Query available memory and thread permissions through the chosen runtime; do not hard-code a retail title's budget. Measure thread scaling and contention on hardware, including handheld and docked conditions, before increasing worker counts.

Retune the worker count rather than assuming all eight cores are usable. Oversubscribing permitted cores can cause contention; an attempt to set an unauthorized affinity can instead fail.

## Sources

- [SwitchBrew T239](https://switchbrew.org/wiki/Switch_2:_Tegra_T239) — reverse-engineered chip configuration.
- [Arm Cortex-A78C technical reference](https://documentation-service.arm.com/static/6193c9bef45f0b1fbf3a85dd) — architecture and configurable implementation details.
- [Eglantine NX2 notes](https://github.com/Eglantine-Community/Eglantine/blob/nx2/NX2_NOTES.md) — sampled process metadata and instruction observations.
- [NVIDIA Jetson AGX Orin developer guide](https://docs.nvidia.com/jetson/archives/r35.2.1/DeveloperGuide/text/SO/JetsonAgxOrin.html) — the closest published relative of the T239.
