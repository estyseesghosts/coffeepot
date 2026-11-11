# NX operating system (Horizon)

The Switch runs Nintendo's own system software for everything from the home menu to games and background tasks. Players see the home menu, suspend and resume, and small overlays like the keyboard and login screens. Developers see a tiny privileged core plus a large set of ordinary background programs that provide files, graphics, audio, input, and networking.

Nintendo calls this system software [Horizon](../../glossary.md#horizon), a name it also uses for the different 3DS operating system — this page is only about the Switch one. The privileged core is the [kernel](../../glossary.md#kernel); almost everything else lives in background programs described below. Booting is covered on the [firmware page](firmware.md); talking to services is covered on the [services and IPC page](services-and-ipc.md).

## Behavior

Horizon starts system background programs first, then the home menu, then the game or app the player chose. Games run as isolated running programs (a **[process](../../glossary.md#process)**), each with its own private memory and numbered references to kernel objects. One sequence of instructions inside a process is a **[thread](../../glossary.md#thread)**; the kernel schedules threads onto CPU cores.

The kernel itself is deliberately small and runs with hardware privilege at EL1, with an even more privileged Secure Monitor at EL3 above it [reverse-engineered] (SwitchBrew Kernel page). Most of what an operating system normally does — files, launching titles, service registration, power state — runs instead as ordinary user-privileged background programs called sysmodules. Each sysmodule has a fixed title ID, for example `0100000000000000` (fs), `0001` (ldr), `0002` (ncm), `0003` (pm), and `0004` (sm) [reverse-engineered] (SwitchBrew title-list pages). Because they are separate processes, a crash or hang in one service looks like failed requests from the game rather than a kernel panic.

What the player thinks of as "overlays" are small apps called applets, managed through a lifecycle service. The home menu itself (qlaunch, title `0100000000001000`), the login screen (auth, `...1001`), and the on-screen keyboard (swkbd, `...1008`) are applets [reverse-engineered] (SwitchBrew Applets page). Games use the `appletOE` / `am` services to enter the foreground, yield for an applet overlay, suspend for sleep mode, and exit cleanly. Sleep mode freezes game threads and services; resume restores them rather than rebooting.

On CPU and memory, the practical model is simple: the game gets three of the four main cores and a bounded application memory pool, while the system reserves the remainder for its sysmodules and applets [reverse-engineered] (SwitchBrew memory-management notes). A game that spawns threads pins them to its available cores and allocates only from its own pool; asking for more fails instead of stealing system memory. What each title may do — which services, which memory sizes, which permissions — is declared up front in its NPDM capability descriptors (ACID/ACI), and the kernel enforces them [reverse-engineered] (SwitchBrew NPDM page).

## Technical details

| Area | Fact |
| ---- | ---- |
| Kernel privilege | Small kernel at EL1; Secure Monitor at EL3 owns keys and the highest-privilege transitions [reverse-engineered] (SwitchBrew Kernel, horizon notes). |
| Kernel objects | `KProcess`, `KThread`, `KSession`, `KEvent`, shared and transfer memory, and synchronization objects. User code never sees the objects directly, only numbers referring to them (a **[handle](../../glossary.md#handle)**) [reverse-engineered] (SwitchBrew Kernel). |
| How user code reaches the kernel | Controlled doorways (a **[system call](../../glossary.md#syscall)**) invoked through libnx wrappers; availability is gated by the title's NPDM capabilities [documented] (`libnx/source/kernel/svc.s`, `libnx/include/switch/kernel/svc.h`). Full number table lives on the [services and IPC page](services-and-ipc.md). |
| Process isolation | Each title gets its own address space and handle table. Handles cannot be used across processes except by explicit transfer or copy during a service call [reverse-engineered] (horizon notes). |
| Capability model | NPDM contains ACID (signed, from Nintendo) plus ACI (limits the title requests). The loader (`ldr`) checks them before the process starts [reverse-engineered] (SwitchBrew NPDM page). |
| Core split (retail behavior) | Games schedule on three CPU cores; one core stays largely with system activity [reverse-engineered] (SwitchBrew / horizon notes). Do not hardcode an affinity mask without reading it back; homebrew runtimes may differ. |
| Memory pools | Separate application, applet, and system pools. A title's maximum comes from its NPDM and the running applet state, not from total RAM size [reverse-engineered] (horizon notes). Exact pool sizes vary by firmware and applet mode; this wiki does not pin numbers here. |

Selected sysmodule title IDs (`01000000000000xx`):

| ID suffix | Short name | Role |
| --------- | ---------- | ---- |
| `0000` | fs | Filesystem service backend (`fsp-srv`) |
| `0001` | ldr | Loader: checks NPDM, loads code, starts processes |
| `0002` | ncm | Content management: installed titles and storage locations |
| `0003` | pm | Process manager: tracks processes and modes |
| `0004` | sm | Service manager (`sm:`): hands out named service sessions |

All values [reverse-engineered] (SwitchBrew title-list pages).

Selected applet title IDs (`0100000000001xxx`):

| ID suffix | Applet | When it appears |
| --------- | ------ | --------------- |
| `1000` | qlaunch | Home menu |
| `1001` | auth | User login / account selection |
| `1008` | swkbd | On-screen keyboard |

All values [reverse-engineered] (SwitchBrew Applets page). Full applet list lives on SwitchBrew; only game-relevant entries are pinned here.

> Unknown: exact per-firmware applet memory budgets. SwitchBrew documents the pool mechanism but this wiki has not pinned a version-by-version table; contributions welcome.

## Homebrew notes

Retail games ship as signed and encrypted packages; homebrew ships as NRO files run under a homebrew loader instead, so normal homebrew never needs to mint packages or forge NPDM signatures. Homebrew still lives under the same rules: it gets a process, a capability set from its loader / forwarder, and must look up services and manage handles like any title — see the [services and IPC page](services-and-ipc.md).

Custom firmware typically replaces or extends `ldr`, `fs`, and `sm` behavior rather than the kernel itself once kernel access is obtained; the exploit ladder itself belongs on [homebrew pages](../homebrew/toolchain-and-workflow.md), not here.

## Sources

- [SwitchBrew wiki](https://switchbrew.org/) — Kernel, System modules, Applets, NPDM, and title-list pages for kernel shape, sysmodule/applet IDs, and capability model.
- [libnx](https://github.com/switchbrew/libnx) — `source/kernel/svc.s` and `include/switch/kernel/svc.h` for syscall wrappers [documented]; `include/switch/services/` for how titles reach sysmodules.
- [NVIDIA Tegra X1 documentation](https://developer.nvidia.com/tegra-x1) — SoC context for EL1/EL3 privilege split.
