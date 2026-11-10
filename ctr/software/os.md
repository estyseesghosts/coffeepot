# CTR OS (Horizon for 3DS)

Horizon is the operating system of the 3DS: it runs games, the home menu, and background services, and it keeps them isolated from each other. Players see the menu and apps; developers see a kernel plus a set of system services.

Nintendo calls this OS [Horizon](../../glossary.md#horizon), a name it reuses for the Switch's distinct OS — this page is only about the 3DS one. The [kernel](../../glossary.md#kernel) runs on the [ARM11 CPU](../hardware/cpu.md); system services mediate access to the [GPU](../hardware/gpu.md), filesystem, network, and friends features.

## Behavior

The OS boots from [firmware](firmware.md), launches system processes, then shows the home menu. Games run as user processes requesting privileged work (files, display, network) through service calls. Background applets (browser, notifications, StreetPass/SpotPass-era features) coexist with the running game under OS scheduling.

## Technical details

> Scaffold — to be filled in the research pass.

| Item | Status |
| ---- | ------ |
| Kernel objects, syscalls, and privilege model | TBD |
| Service list and IPC conventions | TBD |
| Process and memory management | TBD |
| Title formats, tickets, and filesystem layout | TBD |

## Homebrew notes

Homebrew progresses from userland service abuse to kernel access to ARM9/firmware persistence. The exploit ladder and custom firmware links will be added with the homebrew coverage.

## Sources

- To be added in the research pass.
