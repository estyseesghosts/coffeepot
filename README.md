# coffeepot

an aggregated collection of research on nintendo consoles, what makes them tick, and how to write homebrew for them 

none of this research is my own and i do not assume credit for any of the work presented here. this is merely a collection to assist in development. 

To prepare a 3DS for homebrew, follow the [3DS Hacks Guide](https://3ds.hacks.guide/). Once your console is ready, start building with Coffeepot’s [3DS development workflow](ctr/homebrew/development-workflow.md). You can also use an emulator while setting up the toolchain.

## Platforms

- [Nintendo 3DS family (CTR)](ctr/index.md) — processors, GPU, memory, firmware, OS, security and the homebrew scene.
- [Nintendo Switch family (NX)](nx/index.md) — processors, GPU, memory, firmware, OS, Atmosphere and the homebrew toolchain, plus engineering guides on rendering, performance and porting. The engineering guides include a complete small game ([first game](nx/engineering/first-game.md)), a [native deko3d renderer](nx/engineering/deko3d-renderer.md), guides to [synchronization](nx/engineering/gpu-memory-and-synchronization.md), [audio](nx/engineering/audio.md), [saves](nx/engineering/storage-and-saves.md) and [debugging](nx/engineering/debugging.md). All code builds and the pure logic has host tests; the first game and two small programs ran in an emulator, and nothing has run on a console (see [NX source versions](nx/engineering/source-versions.md)).

- [Nintendo Switch 2 (OZ)](oz/index.md) — hardware, system software, homebrew environments and a practical comparison with NX.

## Reading guide

- Jargon is explained on first use and defined in the [global glossary](glossary.md) or the per-console glossary ([CTR](ctr/glossary.md), [NX](nx/glossary.md), [OZ](oz/glossary.md)).
- Each console has hardware, software and homebrew sections; the 3DS has engineering guides for debugging, performance, memory, rendering, assets and testing, and the Switch has guides for rendering and performance and porting. Start from the console's `index.md`.
- Every page ends with Sources. Contributors: read [AGENTS.md](AGENTS.md) first.
