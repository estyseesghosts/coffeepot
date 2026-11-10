# Nintendo Switch (NX)

The Nintendo Switch is a hybrid console sold from 2017 that works both handheld and docked to a TV. It is built on NVIDIA's Tegra X1 chip and runs a Nintendo operating system the company also calls [Horizon](../glossary.md#horizon) — a distinct implementation from the 3DS one.

The family includes the original Switch, the Switch Lite (handheld-only), and the OLED model. This wiki uses **NX** ([glossary](glossary.md#nx)) as the platform name for the whole family.

## At a glance

| Area | Summary |
| ---- | ------- |
| CPU | [Four ARM Cortex-A57 cores on the Tegra X1](hardware/cpu.md) |
| GPU / video | [Maxwell-based NVIDIA graphics with handheld and docked modes](hardware/gpu.md) |
| Memory | [Shared system memory managed by the Horizon kernel](software/os.md) |
| Storage / media | [Cartridges, internal flash, and microSD under a versioned boot chain](software/firmware.md) |
| Operating software | [Nintendo's Horizon OS for Switch: kernel, services, and system applets](software/os.md) |
| Homebrew status | Homebrew coverage is planned, not yet written; entry point will live under `homebrew/` |

## Hardware

- [CPU](hardware/cpu.md) — the application processor and its place on the SoC.
- [GPU](hardware/gpu.md) — graphics in handheld vs. docked modes.

## Software

- [Firmware](software/firmware.md) — boot chain and system versioning.
- [OS](software/os.md) — the Horizon operating system: kernel, services, and applets.

## Homebrew

Not yet written. Planned: exploits, custom firmware, and developer workflows under `homebrew/`.

## Notes and open questions

- Scaffold only. Clock speeds per mode, memory sizes and layout, SoC revisions (e.g. Mariko), and firmware version history are all to be filled in from the research pass.
- Handheld vs. docked behavior differences need systematic coverage — open question whether per-page sections suffice.

## Sources

- To be added in the research pass. No technical claims above go beyond family-level public facts.
