# CTR CPU

The 3DS runs games on a dual-core 32-bit processor and keeps system and security work on a separate older processor. Games see the fast cores; the system keeps its housekeeping isolated.

The application processor is a dual-core [ARM11](../glossary.md#arm11) ([ARM](../../glossary.md#arm) family), and the system processor is an [ARM9](../glossary.md#arm9) inherited from the DS era. Together with the [GPU](gpu.md) and memory they form the console's processing core.

## Where it fits

The ARM11 cores run game code and apps under the [Horizon OS](../software/os.md), while the ARM9 handles backwards compatibility and security-sensitive work. The [firmware](../software/firmware.md) boot chain brings both processors up before handing control to the OS.

## How it works

Software written for the 3DS targets the ARM11 cores. One core does most of the work while the second assists with system tasks, and the ARM9 stays largely invisible to game developers. New 3DS models add performance headroom that games can optionally use.

## Technical details

> Scaffold — to be filled in the research pass.

| Item | Status |
| ---- | ------ |
| Application processor model and core count | Dual-core ARM11 MPCore stated above; exact part, clocks, and cache TBD |
| System processor role and memory map | ARM9 stated above; addresses, privileges, IPC TBD |
| New 3DS differences (cores, clocks, cache) | TBD — needs table by model |
| Memory sizes and layout | TBD — belongs with OS memory management, linked from here |

## Homebrew notes

Homebrew developers target the ARM11 for userland and kernel work; deeper exploits involve the ARM9 and the boot chain. Details and links to `homebrew/` pages will be added with the homebrew coverage.

## Sources

- To be added in the research pass.
