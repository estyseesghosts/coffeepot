# CTR firmware

Firmware is the software that wakes the 3DS up: it verifies each stage of startup and loads the operating system. Nintendo ships it as versioned partitions so system updates can replace pieces without touching user data.

On the 3DS these partitions are called [FIRM](../glossary.md#firm). The boot chain starts in on-chip boot ROM, loads successive signed stages, and ends by starting the [Horizon OS](os.md) ([Horizon](../../glossary.md#horizon)).

## Behavior

Turning the console on runs the chain automatically: each stage checks the next before running it. System updates deliver new FIRM versions; the console refuses to boot tampered stages. This is why firmware version numbers matter for both compatibility and homebrew.

## Technical details

> Scaffold — to be filled in the research pass.

| Item | Status |
| ---- | ------ |
| Boot stages and their order | TBD |
| FIRM partition layout and formats | TBD |
| Version history and what changed | TBD |
| Storage layout (internal flash, SD, gamecards) | TBD — summarized here, detailed on OS page if more apt |

## Homebrew notes

Firmware is the battleground for persistent homebrew: exploits target flaws in boot stages or downgrade paths. Specific exploit and custom firmware links will be added with the homebrew coverage.

## Sources

- To be added in the research pass.
