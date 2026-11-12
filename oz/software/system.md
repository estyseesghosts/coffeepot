# System software and security

Switch 2 runs a Nintendo operating system in the Horizon family. Applications run as separate processes and request system functionality from services. Knowing a service name does not give a process permission to use it; the entry point, process capabilities and firmware determine what is actually available.

## Behavior

The kernel manages processes, memory and threads; system processes provide services such as files, input and display. Applications communicate with them using inter-process communication. The broad model resembles [NX Horizon](../../nx/software/os.md), but interfaces and security rules must be verified for OZ. [Compatibility mode](compatibility.md) adds another translation layer for older games.

## Technical details

[Reverse-engineered] SwitchBrew lists Switch 2 system calls `0x80`–`0x82` for GPU memory attributes and locking, and `0x84`–`0x85` for remote memory mappings. A listed call is not proof that a given process can invoke it. Consult the kernel reference for signatures and check return values and permissions on the exact firmware.

Process metadata describes permissions including allowed system calls, services, core affinity and priorities. [Reported] Four executables examined in Eglantine's notes use a changed ACID layout; parsers written for NX need version-aware bounds and offset checks. The same sample does not establish limits for all titles.

[Reverse-engineered] SwitchBrew documents execute-only code and pointer authentication among the mitigations used by Switch 2 software, with differences by firmware and process. These protections, signed metadata, access controls and the boot chain are separate boundaries. Compromising one process does not imply kernel or boot-chain control.

## Homebrew notes

Log the firmware, process type, attempted service, command and returned result. Distinguish an unavailable service, denied capability, changed protocol and a bad argument before patching around a failure. Do not use emulator success stubs as evidence of real service behavior.

## Sources

- [SwitchBrew kernel](https://switchbrew.org/wiki/Kernel) and [SVC reference](https://switchbrew.org/wiki/SVC) — kernel and system calls.
- [SwitchBrew security mitigations](https://switchbrew.org/wiki/Security_Mitigations) — observed protections and version differences.
- [Eglantine NX2 notes](https://github.com/Eglantine-Community/Eglantine/blob/nx2/NX2_NOTES.md) — sampled process metadata and emulator experiments.
- [ns2jb technical write-up](https://pepsipu.com/blog/2026-10-switch2-jailbreak-ns2jb/) — filesystem and loader trust boundary.
