# Porting from NX: similarities and differences

Switch 2 shares enough concepts with the original Switch to reuse design knowledge, but not enough to assume that an NX binary, graphics backend or privilege model is a native OZ development environment. First identify whether the target is [compatibility mode](../software/compatibility.md), a game sandbox, or a native session. Then test each boundary independently.

## Before you start

This is a source-level porting checklist, not a tested build recipe. No example here has been built or run on OZ. Pin the host game, firmware, launcher and toolchain revisions and retain a working NX baseline. See [homebrew environments](../homebrew/state-and-workflow.md).

## Compare the platforms

| Area | NX | OZ | Porting consequence |
| ---- | -- | -- | ------------------- |
| CPU | Cortex-A57 cluster; see [NX CPU](../../nx/hardware/cpu.md). | Cortex-A78C; see [OZ CPU](../hardware/cpu-and-memory.md). | AArch64 concepts carry over; inspect the binary's instruction requirements and actual core permissions. Retune thread count rather than assuming all eight cores are usable. |
| Memory | 4 GB shared; budgets depend on launch mode ([NX memory](../../nx/hardware/memory.md)). | 12 GB shared physical memory; process budget is environment-dependent. | Query the process budget; do not treat physical RAM as heap or copy an NX budget. |
| GPU | Maxwell and [deko3d](../../nx/homebrew/deko3d-and-libnx.md). | Ampere and a different native graphics stack ([GPU](../hardware/gpu.md)). | Retain renderer architecture and assets where useful; replace or validate the backend, shaders, resource limits, fences and presentation. |
| OS | Horizon processes, kernel calls and services ([NX services](../../nx/software/services-and-ipc.md)). | Related concepts, changed calls, permissions and services ([system](../software/system.md)). | Recheck command IDs, signatures, permissions, errors and lifecycle; names alone are not compatibility. |
| Execution | NX custom firmware and homebrew loader provide established workflows. | Game sandboxes and version-specific native sessions have different capabilities. | Select the runtime first; do not assume NRO loading, title override or Atmosphère equivalence. |
| Display/input | NX Joy-Con and handheld/docked modes ([NX input](../../nx/hardware/input-and-display.md)). | New screen and Joy-Con 2 mouse mode ([devices](../hardware/devices.md)). | Query mode and input capabilities, scale UI, handle hotplug; do not force mouse support where unavailable. |
| Storage | NX internal flash and SD; system-managed saves. | UFS and microSD Express; system-managed access remains context-dependent. | Preserve the distinction between content, temporary files and saves; test commit and recovery in the selected host. |
| Compatibility | Original execution target. | Compatibility modules adapt many NX titles ([details](../software/compatibility.md)). | A working NX title under compatibility is valuable evidence for that path only, not proof of native OZ behavior. |

## What carries over

The following is a qualitative porting assessment, not a measurement of source compatibility:

| Area | Reuse | Explanation |
| ---- | ----- | ----------- |
| ARM64 programming | High | Same instruction-set family, different microarchitecture |
| Horizon concepts | High | Services, IPC, SVCs, processes and permissions remain recognizable |
| RomFS/ExeFS concepts | High | Existing filesystem and executable abstractions remain relevant |
| Switchbrew research | High | Directly useful for understanding new services and vulnerabilities |
| Existing game logic | High | Portable C and C++ code generally survives |
| `libnx` source | Partial | Active experimental ports exist, but not full compatibility |
| Atmosphère source | Partial | Individual libraries and modules can be adapted |
| deko3d and Maxwell GPU code | Low | Switch 2 uses Ampere and newer graphics interfaces |
| Existing `.nro` binaries | Not generally drop-in | No equivalent unrestricted native homebrew environment |
| Fusée Gelée and Hekate | Not transferable as a boot path | Fusée Gelée targets the T210 boot ROM; Hekate is a separate NX payload. See [entry paths](../homebrew/nx-entry-paths.md). |

The strongest evidence of practical reuse is [libnx-ounce](https://github.com/alula/libnx-ounce), a fork of the original Switch's `libnx`, and [Ozone](https://github.com/alula/Ozone), which attempts to port parts of Atmosphère. Ozone's README explicitly says it is not full Switch 2 custom firmware and very likely never will be.

## Port in small steps

1. Build portable logic separately and run its existing host tests. Preserve deterministic input and expected output.
2. Start the chosen runtime's minimal example and log its version and environment. Establish launch and clean exit before importing the application.
3. Add input and lifecycle handling, then storage. Check disconnect, suspend/resume and interrupted saves where the environment permits them.
4. Add graphics with one clear and one triangle using the runtime's documented sample. Confirm buffer ownership, synchronization and presentation before building a renderer.
5. Add audio and networking separately. Log exact errors and test recovery instead of assuming NX service privileges.
6. Measure the same workload and correctness on each target. Record firmware, mode, resolution, clocks if known, build, repetitions and raw results. Emulator timing is not console performance.

## Technical details

[Reported] Eglantine's sampled native binaries use a `0x3f` affinity mask, not a universal six-core promise. [Reverse-engineered] Compatibility hooks adapt timing and other behavior; use the runtime's time API rather than treating raw counter ticks as a portable time unit. The comparison uses the references linked from the table, not new Coffeepot measurements.

Two changes are worth calling out because they break tooling rather than game code:

- The original Switch's removable-storage content used NAX0; Switch 2 uses NAX1, and content directories use the newer `.nca.CONCAT` naming.
- The internal storage layout is a four-LUN UFS arrangement rather than the original Switch's eMMC layout.

Both are on the [devices page](../hardware/devices.md).

## Pitfalls

- **Build succeeds but launch fails:** check application format, ABI and entry point for the selected launcher, not just CPU architecture.
- **Service call fails:** record the result and check process permissions and interface version before assuming the service is absent.
- **GPU hangs or corrupts output:** check resource lifetime, visibility and synchronization in the actual backend; NX barriers are not automatically an OZ contract.
- **Different speed or timing:** check output mode, workload and timing source before changing simulation behavior.
- **Timing drifts:** under compatibility mode the system tick differs from the original Switch's; see [compatibility mode](../software/compatibility.md).

## Open questions

Per-runtime memory limits, supported graphics APIs, stable profiling tools and measured hardware costs remain to be established for each firmware and launcher combination.

## Sources

- [SwitchBrew compatibility mode](https://switchbrew.org/wiki/Switch_2:_Compatibility_Mode) — adaptation of NX software.
- [SwitchBrew T239](https://switchbrew.org/wiki/Switch_2:_Tegra_T239) — hardware comparison.
- [Eglantine NX2 notes](https://github.com/Eglantine-Community/Eglantine/blob/nx2/NX2_NOTES.md) — sampled native process capabilities.
- [ns2jb research](https://pepsipu.com/blog/2026-10-switch2-jailbreak-ns2jb/) — execution context.
- [libnx-ounce](https://github.com/alula/libnx-ounce) — a port of the original Switch's userland library.
- [Ozone](https://github.com/alula/Ozone) — Atmosphère component porting and its stated limits.
