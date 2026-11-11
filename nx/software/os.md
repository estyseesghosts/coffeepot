# NX operating system (Horizon)

The Switch runs Nintendo's own system software for everything from the home menu to games and background tasks. Players see the home menu, suspend and resume, and small overlays like the keyboard and login screens. Developers see a tiny privileged core plus a large set of ordinary background programs that provide files, graphics, audio, input, and networking.

Nintendo calls this system software [Horizon](../../glossary.md#horizon), a name it also uses for the different 3DS operating system — this page is only about the Switch one. The privileged core is the [kernel](../../glossary.md#kernel); almost everything else lives in background programs described below. Booting is covered on the [firmware page](firmware.md); talking to services is covered on the [services and IPC page](services-and-ipc.md).

## Behavior

Horizon starts system background programs first, then the home menu, then the game or app the player chose. Games run as isolated running programs (a **[process](../../glossary.md#process)**), each with its own private memory and numbered references to kernel objects. One sequence of instructions inside a process is a **[thread](../../glossary.md#thread)**; the kernel schedules threads onto CPU cores.

The kernel itself is deliberately small and runs with hardware privilege at EL1, with a more privileged secure monitor above it. SwitchBrew's Memory layout page describes the kernel's translation tables as the `TTBR0_EL1`/`TTBR1_EL1` ones [reverse-engineered] (SwitchBrew Memory layout, oldid 12866), and Atmosphere's README describes Exosphère as the customised secure monitor (TrustZone) and Thermosphère as an EL2 hypervisor [documented] (`Atmosphere` `README.md`). The "EL3" label for the secure monitor is standard ARM terminology and was not stated on the saved SwitchBrew Kernel page. Most of what an operating system normally does — files, launching titles, service registration, power state — runs instead as ordinary user-privileged background programs called sysmodules. Each sysmodule has a fixed title ID, for example `0100000000000000` (fs), `0100000000000001` (ldr), `0100000000000002` (ncm), `0100000000000003` (pm) and `0100000000000004` (sm), all "bundled with kernel" [reverse-engineered] (SwitchBrew Title list, oldid 15462). Because they are separate processes, a crash or hang in one service looks like failed requests from the game rather than a kernel panic.

What the player thinks of as "overlays" are small apps called applets, managed through a lifecycle service. The home menu itself (qlaunch, title `0100000000001000`), the login screen (auth, `...1001`), and the on-screen keyboard (swkbd, `...1008`) are applets [reverse-engineered] (SwitchBrew Title list, oldid 15462: `0100000000001000`, `...1001`, `...1008`). Games use the `appletOE` / `am` services to enter the foreground, yield for an applet overlay, suspend for sleep mode, and exit cleanly. Sleep mode freezes game threads and services; resume restores them rather than rebooting.

On CPU and memory, the practical model is simple: ports report that a game gets three of the four main cores and a bounded application memory pool, while the system keeps the remainder for its sysmodules and applets [reported] (`nfsmw-nx` `docs/platform-notes.md`, commit `20f8dc1`). A game that spawns threads pins them to its available cores and allocates only from its own pool; asking for more fails instead of stealing system memory. What each title may do — which services, which memory sizes, which permissions — is declared up front in its NPDM capability descriptors (ACID/ACI), and the kernel enforces them [reverse-engineered] (SwitchBrew NPDM page).

## Technical details

| Area | Fact |
| ---- | ---- |
| Kernel privilege | Small kernel at EL1; the secure monitor (TrustZone) holds key material, which "never leaves TrustZone" [reverse-engineered] (SwitchBrew Secure Monitor, edited 2026-11-06). |
| Kernel objects | `KProcess` (with capabilities and a handle table), `KThread`, `KSession`, events, shared and transfer memory, and synchronization objects. User code never sees the objects directly, only numbers referring to them (a **[handle](../../glossary.md#handle)**) [reverse-engineered] (SwitchBrew Kernel, oldid 12654). |
| How user code reaches the kernel | Controlled doorways (a **[system call](../../glossary.md#syscall)**) invoked through libnx wrappers; availability is gated by the title's NPDM capabilities [documented] (`libnx/nx/source/kernel/svc.s`, `libnx/nx/include/switch/kernel/svc.h`). Full number table lives on the [services and IPC page](services-and-ipc.md). |
| Process isolation | Each title gets its own address space and handle table. Handles cannot be used across processes except by explicit transfer or copy during a service call [reverse-engineered] (horizon notes). |
| Capability model | NPDM contains ACID (signed, from Nintendo) plus ACI (limits the title requests). The loader (`ldr`) checks them before the process starts [reverse-engineered] (SwitchBrew NPDM page). |
| Core split (retail behavior) | Games schedule on three CPU cores; one core stays with the system [reported] (`nfsmw-nx`, `sa2-nx`). Do not hardcode an affinity mask without reading it back; homebrew runtimes may differ. |
| Memory pools | Separate application, applet, and system pools; a program launched as an application sees a much larger heap than one launched as the Album applet [reported] (`nfsmw-nx`, `srb2-switch`; see [NX memory](../hardware/memory.md)). Exact pool sizes vary by firmware and applet mode and are not pinned. |

Selected sysmodule title IDs (`01000000000000xx`):

| ID suffix | Short name | Role |
| --------- | ---------- | ---- |
| `0000` | fs | Filesystem service backend (`fsp-srv`) |
| `0001` | ldr | Loader: checks NPDM, loads code, starts processes |
| `0002` | ncm | Content management: installed titles and storage locations |
| `0003` | pm | Process manager: tracks processes and modes |
| `0004` | sm | Service manager (`sm:`): hands out named service sessions |

All values [reverse-engineered] (SwitchBrew Title list, oldid 15462).

Selected applet title IDs (`0100000000001xxx`):

| ID suffix | Applet | When it appears |
| --------- | ------ | --------------- |
| `1000` | qlaunch | Home menu |
| `1001` | auth | User login / account selection |
| `1008` | swkbd | On-screen keyboard |

All values [reverse-engineered] (SwitchBrew Title list, oldid 15462: `0100000000001000`, `...1001`, `...1008`). Full applet list lives on SwitchBrew; only game-relevant entries are pinned here.

> Unknown: exact per-firmware applet memory budgets. SwitchBrew documents the pool mechanism but this wiki has not pinned a version-by-version table; contributions welcome.

## Homebrew notes

Retail games ship as signed and encrypted packages; homebrew ships as NRO files run under a homebrew loader instead, so normal homebrew never needs to mint packages or forge NPDM signatures. Homebrew still lives under the same rules: it gets a process, a capability set from its loader / forwarder, and must look up services and manage handles like any title — see the [services and IPC page](services-and-ipc.md).

Custom firmware typically replaces or extends `ldr`, `fs`, and `sm` behavior rather than the kernel itself once kernel access is obtained; the exploit ladder itself belongs on [homebrew pages](../homebrew/toolchain-and-workflow.md), not here.

## Sources

- SwitchBrew [Kernel](https://switchbrew.org/wiki/Kernel) (oldid 12654, edited 2024-03-06) — kernel object names, process capabilities, handle table.
- SwitchBrew [Memory layout](https://switchbrew.org/wiki/Memory_layout) (oldid 12866) — kernel translation tables and address-space layout.
- SwitchBrew [Secure Monitor](https://switchbrew.org/wiki/Secure_Monitor) (edited 2026-11-06) — keys stay inside TrustZone.
- SwitchBrew [Title list](https://switchbrew.org/wiki/Title_list) (oldid 15462) — sysmodule and applet title IDs.
- [Atmosphere](https://github.com/Atmosphere-NX/Atmosphere) at `4e3f37038d9e82dde7054897213ab32b2ecf651f` — `README.md` component descriptions.
- [libnx](https://github.com/switchbrew/libnx) at `b156b4774fb2df7b93402169509266da7547fc4c` — `nx/source/kernel/svc.s` and `nx/include/switch/kernel/svc.h` for syscall wrappers [documented]; `nx/include/switch/services/` for how titles reach sysmodules.
- SwitchBrew NPDM, Applets and horizon pages cited by earlier text were not saved with the research sources and were not re-verified.
