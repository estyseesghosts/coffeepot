# System software and security

Switch 2 runs a Nintendo operating system in the Horizon family. Applications run as separate processes and request system functionality from services. Knowing a service name does not give a process permission to use it; the entry point, process capabilities and firmware determine what is actually available.

## Behavior

The kernel manages processes, memory and threads; system processes provide services such as files, input and display. Applications communicate with them using inter-process communication. The broad model resembles [NX Horizon](../../nx/software/os.md), but interfaces and security rules must be verified for OZ. [Compatibility mode](compatibility.md) adds another translation layer for older games.

## Technical details

### New system calls

[Reverse-engineered] SwitchBrew lists Switch 2 system calls `0x80`–`0x82` for GPU memory attributes and locking, and `0x84`–`0x85` for remote memory mappings:

| SVC | Name | Purpose |
| --- | ---- | ------- |
| `0x80` | `SetGpuMemoryAttribute` | Sets the GPU-visible attributes of a memory block |
| `0x81` | `LockGpuSharableMemory` | Locks memory shared with GPU operations |
| `0x82` | `UnlockGpuSharableMemory` | Releases that lock |
| `0x84` | `MapRemoteMemory` | Maps memory belonging to another process |
| `0x85` | `UnmapRemoteMemory` | Removes that mapping |

[Reverse-engineered] A listed call is not proof that a given process can invoke it. Consult the kernel reference for signatures and check return values and permissions on the exact firmware.

For GPU memory especially, understanding why these interfaces exist matters as much as their signatures. The original Switch's model did not need an explicit attribute-setting step or a separate sharable-memory lock, and the difference is what a native port has to accommodate.

### Process metadata

[Reported] Process metadata describes permissions including allowed system calls, services, core affinity and priorities. Four executables examined in Eglantine's notes use a changed ACID layout; parsers written for NX need version-aware bounds and offset checks. The same sample does not establish limits for all titles.

[Reported] The same four executables use a 39-bit address space, a core affinity mask of `0x3F` covering cores 0–5, and thread priorities between 28 and 59. See [CPU and memory](../hardware/cpu-and-memory.md) for why these are samples rather than entitlements.

### Security protections

[Reverse-engineered] SwitchBrew documents execute-only code and pointer authentication among the mitigations used by Switch 2 software, with differences by firmware and process. These protections, signed metadata, access controls and the boot chain are separate boundaries. Compromising one process does not imply kernel or boot-chain control.

The protections themselves, the firmware version each arrived in, the memory encryption carveouts, the fuse block and the five levels of compromise are on the [security page](security.md).

### Services

The service landscape, including the graphics services that exist only on Switch 2, is on the [graphics and system services page](services.md).

### Boot and executables

[Reverse-engineered] The boot chain and its storage layout are on the [firmware page](firmware.md) and the [devices page](../hardware/devices.md).

[Reverse-engineered] The program loader, `ldr`, validates signed metadata before mapping an executable into memory. It is one of the two services at the centre of the first public Switch 2 exploit chain, and understanding the split between what `ldr` validates and what the filesystem service supplies is the clearest available explanation of Horizon's trust boundaries; see the [homebrew page](../homebrew/state-and-workflow.md).

## Homebrew notes

Log the firmware, process type, attempted service, command and returned result. Distinguish an unavailable service, denied capability, changed protocol and a bad argument before patching around a failure. Do not use emulator success stubs as evidence of real service behavior.

When porting a service call from NX, check the SVC number and the process capability list separately. The number may still exist while the permission does not, and a compatibility-mode process is deliberately granted more than its own NPDM asks for.

## Sources

- [SwitchBrew kernel](https://switchbrew.org/wiki/Kernel) and [SVC reference](https://switchbrew.org/wiki/SVC) — kernel and system calls.
- [SwitchBrew security mitigations](https://switchbrew.org/wiki/Security_Mitigations) — observed protections and version differences.
- [SwitchBrew system versions](https://switchbrew.org/wiki/Switch_2:_System_Versions) — the firmware a behaviour applies to.
- [Eglantine NX2 notes](https://github.com/Eglantine-Community/Eglantine/blob/nx2/NX2_NOTES.md) — sampled process metadata and emulator experiments.
- [ns2jb technical write-up](https://pepsipu.com/blog/2026-10-switch2-jailbreak-ns2jb/) — filesystem and loader trust boundary.
