# NX firmware

Firmware is the software that wakes the Switch up: it verifies each stage of startup and loads the operating system. Nintendo ships versioned boot components so system updates can evolve the chain while keeping user data intact.

The chain starts in on-chip boot ROM on the [Tegra X1](../hardware/cpu.md), passes through signed bootloaders, and ends by starting the [Horizon OS](os.md) ([Horizon](../../glossary.md#horizon)).

## Behavior

Powering on runs the chain automatically, each stage checking the next. System updates deliver new boot and system components together; mismatched or tampered stages refuse to boot. Firmware version numbers therefore gate both features and homebrew entry points.

## Technical details

> Scaffold — to be filled in the research pass.

| Item | Status |
| ---- | ------ |
| Boot stages and their order | TBD |
| Partition/package layout and formats | TBD |
| Version history and what changed | TBD |
| Storage layout (internal flash, microSD, gamecards) | TBD — summarized here, detailed on OS page if more apt |
| SoC revision effects on the boot chain | TBD |

## Homebrew notes

Early boot flaws are the most valuable homebrew targets because they run before the OS hardens the system. Specific exploit and custom firmware links will be added with the homebrew coverage.

## Sources

- To be added in the research pass.
