# CTR GPU

The 3DS draws its glasses-free 3D image with a DMP graphics chip from the PICA family. Games describe scenes with display lists and the chip renders them twice — once per eye — to produce depth.

The chip is the [PICA200](../glossary.md#pica200). It is largely fixed-function hardware with a small programmable stage, a style typical of handhelds of its era. Output goes to the stereoscopic upper screen and the touchscreen lower screen.

## Where it fits

The [CPU](cpu.md) prepares scenes and hands them to the GPU, which renders into framebuffers managed under the [OS](../software/os.md). Display configuration (3D depth, screen pairing) is system-controlled rather than raw hardware access.

## How it works

A game submits geometry, textures, and effects; the chip shades and rasterizes them for the left and right eye views. The famous 3D slider adjusts the stereo effect without the game changing its scene. 2D screens and DS backwards compatibility use simpler paths through the same display hardware.

## Technical details

> Scaffold — to be filled in the research pass.

| Item | Status |
| ---- | ------ |
| Exact part, clocks, and pipeline stages | TBD |
| Display resolutions and framebuffer formats | TBD |
| Texture formats, lighting, and shader limits | TBD |
| Register-level programming details | TBD |

## Homebrew notes

Homebrew graphics go through homebrew libraries that wrap GPU commands; low-level register details matter for driver and emulator work. Library and driver links will be added with the homebrew coverage.

## Sources

- To be added in the research pass.
