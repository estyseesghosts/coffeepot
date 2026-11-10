# NX CPU

The Switch runs everything — games and system software — on four 64-bit ARM cores on its NVIDIA chip. There is no separate legacy processor; isolation between games and the system is done by the operating system and hardware privilege levels.

The cores are [Cortex-A57](../glossary.md#cortex-a57) ([ARM](../../glossary.md#arm) family) on the [Tegra X1](../glossary.md#tegra-x1) [system-on-a-chip](../../glossary.md#soc). They share the chip with the [GPU](gpu.md), memory controller, and peripherals.

## Where it fits

The cores boot through the [firmware](../software/firmware.md) chain into the [Horizon OS](../software/os.md) [kernel](../../glossary.md#kernel), which schedules game and system processes with different privileges. Handheld and docked modes change GPU behavior primarily; CPU differences by mode are to be confirmed.

## How it works

Game code runs as user processes scheduled across the available cores, calling into system services for hardware access rather than touching hardware directly. The kernel reserves capacity for itself and background services, so games do not see the full theoretical throughput.

## Technical details

> Scaffold — to be filled in the research pass.

| Item | Status |
| ---- | ------ |
| Core count, clocks per mode, cache hierarchy | Four Cortex-A57 stated above; numbers TBD |
| SoC revisions and their differences | TBD |
| Privilege levels and secure monitor role | TBD |
| Memory sizes and layout | TBD — belongs with OS memory management, linked from here |

## Homebrew notes

Homebrew on the Switch runs as user processes first; kernel access is the escalation target. Toolchain and exploit links will be added with the homebrew coverage.

## Sources

- To be added in the research pass.
