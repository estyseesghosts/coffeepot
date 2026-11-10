# NX OS (Horizon for Switch)

Horizon is the operating system of the Switch: it runs games, the home menu, and background applets while keeping them isolated. Players see the menu and quick functions; developers see a microkernel plus a large set of system services.

Nintendo calls this OS [Horizon](../../glossary.md#horizon), a name it reuses for the 3DS's distinct OS — this page is only about the Switch one. The [kernel](../../glossary.md#kernel) schedules processes across the [Cortex-A57 cores](../hardware/cpu.md); services mediate the [GPU](../hardware/gpu.md), filesystem, audio, network, and controller input.

## Behavior

The OS boots from [firmware](firmware.md), starts system services, then shows the home menu. Games run as application processes making service calls for files, display, and input. Applets (settings, eShop, capture, controller pairing) overlay or suspend the game under OS control, and sleep mode freezes the whole userland state.

## Technical details

> Scaffold — to be filled in the research pass.

| Item | Status |
| ---- | ------ |
| Kernel objects, syscalls, and privilege model | TBD |
| Service list and IPC conventions | TBD |
| Process and memory management | TBD |
| Title formats, NCA/NSP-era packaging, filesystem layout | TBD |

## Homebrew notes

Homebrew progresses from userland to kernel to warmboot/persistent boot control. The exploit ladder and custom firmware links will be added with the homebrew coverage.

## Sources

- To be added in the research pass.
