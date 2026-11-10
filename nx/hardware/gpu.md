# NX GPU

The Switch draws its images with NVIDIA graphics from the Maxwell generation, on the same chip as the CPU. The same game renders at modest settings on the handheld screen and at higher settings on a TV when docked.

The GPU is the [Maxwell](../glossary.md#maxwell)-based block of the [Tegra X1](../glossary.md#tegra-x1) [system-on-a-chip](../../glossary.md#soc). Unlike the 3DS fixed-function design, it is a fully programmable modern GPU. Display output goes to the built-in screen handheld or over HDMI through the dock.

## Where it fits

The [CPU](cpu.md) runs game logic and submits rendering work through graphics APIs under the [OS](../software/os.md). Docked vs. handheld mode changes available clocks and resolution, negotiated between the OS, the dock, and the game.

## How it works

Games use a Nintendo graphics API that compiles shaders and submits command buffers to the GPU. The OS manages display modes, vsync, and capture (screenshots, video), so games pick from supported configurations rather than driving the display directly.

## Technical details

> Scaffold — to be filled in the research pass.

| Item | Status |
| ---- | ------ |
| Clocks and resolutions per mode | TBD — needs handheld/docked table |
| API surface and shader model | TBD |
| Display pipeline and dock behavior | TBD |
| SoC revision differences | TBD |

## Homebrew notes

Homebrew graphics use community toolchains targeting the same API or lower-level access where available. Links will be added with the homebrew coverage.

## Sources

- To be added in the research pass.
