# Nintendo Switch 2 (OZ)

Nintendo Switch 2 is Nintendo's hybrid handheld and TV console, released in 2025. This wiki uses **[OZ](glossary.md#oz)** for Ounce, the platform codename. It inherits many ideas from the [original Switch (NX)](../nx/index.md), but has different hardware and a much less mature homebrew environment. Start by choosing an execution environment before choosing a library: an original Switch game, a game sandbox, and native code in a system process do not have the same interfaces or permissions.

## At a glance

| Area | Starting point |
| ---- | -------------- |
| CPU and memory | [T239, Cortex-A78C and shared memory](hardware/cpu-and-memory.md) |
| Graphics | [Ampere hardware and the software boundary](hardware/gpu.md) |
| Display, input and storage | [Screen, controllers, UFS and microSD Express](hardware/devices.md) |
| System software | [Horizon, processes, services and security](software/system.md) |
| Homebrew | [Execution environments and available projects](homebrew/state-and-workflow.md) |
| Porting | [Detailed NX comparison](engineering/nx-comparison.md) |

## Hardware

- [CPU and memory](hardware/cpu-and-memory.md) — core configuration, memory and limits of published figures.
- [GPU](hardware/gpu.md) — Ampere, graphics interfaces and what is not established for homebrew.
- [Devices](hardware/devices.md) — display, controllers and storage.

## Software

- [System and security](software/system.md) — process permissions, services, boot and executable boundaries.
- [Switch compatibility mode](software/compatibility.md) — how Switch games are adapted and what that does not imply for native software.

## Homebrew

- [State and workflow](homebrew/state-and-workflow.md) — choose a sandbox or native environment, find its actual prerequisites and test a small program.

## Engineering

- [NX comparison and porting](engineering/nx-comparison.md) — similarities, differences and a migration checklist.
- [Graphics and performance](engineering/graphics-and-performance.md) — how to investigate an Ampere workload without assuming desktop features or unsupported APIs.

## Notes and open questions

The execution paths described here are version-sensitive. No OZ example or benchmark in Coffeepot has been built or run on hardware. Published hardware capacities do not establish per-process budgets, sustained clocks, or access from a particular launcher. Record the console, firmware, entry point and project revision with every result.

## Sources

- [Nintendo technical specifications](https://www.nintendo.com/en-ca/gaming-systems/switch-2/tech-specs/) — official device and display specifications.
- [SwitchBrew T239](https://switchbrew.org/wiki/Switch_2:_Tegra_T239) — community hardware research.
- [ns2jb research](https://pepsipu.com/blog/2026-10-switch2-jailbreak-ns2jb/) — native execution and its scope.
