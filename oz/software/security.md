# Security model and mitigations

Switch 2's protections are layered, and each layer has changed since the original Switch. This page lists the protections that exist, the firmware versions they arrived in, and why holding one boundary does not mean holding the next. It exists because the single most common mistake in Switch 2 homebrew writing is treating "I can run native code" as "I have custom firmware."

## Where it fits

The boot chain runs before Horizon starts and establishes what is trusted: the boot ROM, the fuse block, the first-stage bootloaders and the signed package chain. Horizon then applies per-process and per-module protections at runtime. The layers are described in the order they are crossed on the [firmware page](firmware.md), the boundary between them on the [system and security page](system.md), and what remains out of reach on the [homebrew page](../homebrew/state-and-workflow.md).

## How it works

Each protection below constrains a different actor.

| Protection | What it constrains |
| ---------- | ----------------- |
| Pointer authentication (PAC) | Code that tries to hijack a return address or a pointer that holds one |
| Execute-only memory (XOM) | Code that tries to read execute-only instructions as data |
| Control-flow integrity (CFI) | Code that tries to call through a corrupted function or vtable pointer |
| ASLR, KASLR, PASLR | An attacker who knows where something is loaded and wants it to stay there |
| RelRo | An attacker who wants to overwrite the relocation table |
| Memory encryption | An attacker who can read or write the external memory bus |
| Signed executable metadata | Unauthorized changes to signed metadata; executable content also depends on the loader and filesystem trust boundary |
| Service access control | A process that wants a service it was not granted |

[Reverse-engineered] Switchbrew's security mitigations page records the version at which each of these arrived. The sources reviewed do not establish a public Switch 2 boot-chain compromise comparable to Fusée Gelée. This does not prove the boot chain cannot be compromised; see [NX entry paths](../homebrew/nx-entry-paths.md).

## Technical details

### Memory layout randomization

[Reverse-engineered] Userspace ASLR is supported. Kernel ASLR (KASLR) was added in 5.0.0 and physical address randomization (PASLR) in 10.0.0.

### Read-only relocations

[Reverse-engineered] RelRo was added in 17.0.0. Binaries built for 17.0.0 and later use it.

### Execute-only memory

[Reverse-engineered] `--X` was added in 19.0.0 for both console generations, but is only used on Switch 2. On 19.0.0 it was controlled through the [NPDM](../glossary.md#npdm); from 20.0.0 it is toggled per executable module through a header field.

- All Switch 2 system modules have `--X` `.text`.
- On 19.0.0 through 20.5.0, all Switch 2 applets used readable-and-executable `.text`, because `nncfi` needs to read code to validate branch-target instructions.
- From 21.0.0, `nncfi` was replaced by `-fptrauth-type-info-vtable-pointer-discrimination` and `-fptrauth-vtable-pointer-type-discrimination`, and `.text` is `--X` for all applet code, including the `BrowserCoreDll` module.
- `offlineWebGen0` is the one exception: it did not get `--X` until 23.0.0.
- Games built with `nnSdk` 20.x or older use no `--X` module at all. Games built with `nnSdk` 21.x or newer may use `--X` for the main module only, and do not always.

### Pointer authentication

[Documented] The Cortex-A78C implements Armv8.3-A pointer authentication and Armv8.6-A enhanced pointer authentication instructions.

[Reverse-engineered] On Switch 2, PAC protects return addresses on the stack for games, applets and system modules. Switch 1 compatibility processes get PAC enabled on Switch 2 even though the Switch 1 binaries themselves are compiled without it. From 21.0.0, Switch 2 applets also get PAC on vtable pointers and function pointers.

### Control-flow integrity

[Reverse-engineered] Switch 2 uses different CFI mechanisms depending on whether dynamic linking is required and whether execute-only memory is supported.

| Mechanism | Where it is used and how it fails |
| --------- | --------------------------------- |
| `nncfi` | Software branch-target identification. The validation checks for a `bti c` or `bti j` instruction at the branch address; on failure it jumps to the undefined instruction `0x000080C0` plus the offending register. The check constrains eligible indirect-branch targets; consult the source for the branch-type distinctions. It reads from `.text`, so it can only be used when `.text` is readable and executable. |
| clang CFI | Switch 2 system modules are compiled with `--fsanitize=cfi-icall` and `--fsanitize=cfi-vcall`, giving runtime checks on function and vtable pointers. PAC is not used alongside this, and a failure raises an undefined-instruction exception. |

### Memory encryption

[Reverse-engineered] The T239 appears to support memory encryption, similar to the T234 used in NVIDIA's Orin products. Memory in certain carveout regions is encrypted: the memory controller encrypts on write and decrypts on read, so an attacker sniffing the external memory bus sees only ciphertext.

[Reverse-engineered] SwitchBrew reports that application memory is not encrypted; it suggests performance as a possible reason, not a verified design rationale.

SwitchBrew considers a per-physical-address tweak likely; if so, relocating ciphertext would not preserve the plaintext. The source reports no memory authentication: after external tampering the CPU reads back garbled plaintext, and earlier values could in principle be replayed to the same address.

### The boot chain and fuses

[Reverse-engineered] The T23x family has a 0x1000-byte (16,384-bit) fuse block. Switchbrew recovered its layout by reverse engineering `fskp_t234.bin` from the Linux4Tegra package. Fuses are laid out in 32-bit words, and the first word carries the security-relevant fields:

| Fuse word | Bits | Field |
| --------- | ---- | ----- |
| 0 | 5..8 | `OdmLock` |
| 0 | 9 | `Fa` |
| 0 | 10 | `SecurityMode` |
| 0 | 11 | `ArmJtagDisable` |
| 0 | 12 | `NvJtagProtectionEnable` |
| 0 | 16..20 | `DebugAuthentication` |
| 0 | 28 | `ZeroizeDis` |

The boot chain that consumes these fuses is described on the [firmware page](firmware.md).

### Five levels of compromise

[Reverse-engineered] It helps to separate the levels. They describe distinct capabilities: a result at one level does not automatically establish the next.

| Level | State as of November 12, 2026 |
| ----- | ---------------------------- |
| Game-process execution | Available. Several independent game vulnerabilities give code execution inside a title, confined to that title's privileges. |
| Privileged system-module execution | Demonstrated on 23.0.1 by the ns2jb chain, before it was patched in 23.5.0. It gives broader service access and filesystem operations. |
| Kernel control | Not publicly established through the released chain. |
| Boot-chain and secure-monitor control | No publicly established compromise comparable to the original Switch's earliest boot exploits. |
| Persistent unrestricted custom firmware | Not established by ns2jb; a persistent design would require an appropriate trusted or exploitable startup path and implementation. Kernel control and boot control are distinct and are not universally required by every possible CFW design. |

> Unknown: The boundaries between the last three levels are not publicly documented in enough detail to say precisely what a kernel-level compromise would yield.

## Homebrew notes

Distinguish the level you are working at before you draw conclusions from a failure. A service call that returns a permission error in a game process is expected to behave differently in a privileged system-module session, and neither result tells you whether the kernel would permit it.

When a chain stops working after a firmware update, check whether the failure is in acquisition, in the memory layout, or in the loaded code before assuming the technique is dead. The first public chain was acquired through a timing-sensitive heap overflow whose layout changed under a firmware update that did not otherwise alter the console's features; see [homebrew environments and workflow](../homebrew/state-and-workflow.md).

## Sources

- [SwitchBrew security mitigations](https://switchbrew.org/wiki/Security_Mitigations) — the protections above and the firmware version each arrived in.
- [SwitchBrew T239](https://switchbrew.org/wiki/Switch_2:_Tegra_T239) — pointer authentication and memory encryption on the chip.
- [SwitchBrew fuses](https://switchbrew.org/wiki/Switch_2:_Fuses) — the 16,384-bit fuse block and its layout.
- [Arm Cortex-A78C technical reference](https://documentation-service.arm.com/static/6193c9bef45f0b1fbf3a85dd) — the pointer authentication instruction sets.
- [ns2jb research](https://pepsipu.com/blog/2026-10-switch2-jailbreak-ns2jb/) — the privileged system-module level and the trust boundary it crosses.
