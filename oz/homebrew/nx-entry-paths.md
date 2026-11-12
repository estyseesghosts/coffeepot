# Why NX homebrew entry paths do not transfer automatically

An original Switch homebrew setup combines an entry point, one or more privilege escalations, a loader and libraries. Switch 2 retains some Horizon concepts, but the old chains depended on specific bugs, hardware and firmware. A game running in compatibility mode may still contain its own bug; that does not recreate the rest of an NX exploit chain or give native Switch 2 privileges. For choosing an environment today, see [homebrew environments](state-and-workflow.md).

## What to compare

[Reverse-engineered] Separate the input that reaches a bug from the privilege obtained and the component that loads a program:

| Boundary | NX example | What must be established on OZ |
| -------- | ---------- | ------------------------------ |
| Application | PegaSwitch used an old WebKit flaw to control the browser process. | The exact vulnerable component and version must be present and reachable; a game bug may survive in compatibility mode, with the game's permissions. |
| Service authorization | `smhax` exploited an uninitialized service-manager session to obtain otherwise restricted services on early NX firmware. | Service access and initialization checks must be checked on the target version. Similar service names do not establish the same bug. |
| Loader | `nspwn` combined service access and path redirection with an unsigned container accepted as executable content. | A loader, its filesystem provider and authorization checks must all permit the required transition. The NX fix and a different OZ loader environment prevent assuming this historical chain works. |
| Kernel and secure world | NX research chained graphics-driver and memory access bugs with further kernel or warmboot weaknesses. | An application or service compromise does not itself grant kernel, secure-monitor or boot control. Confirm each additional boundary. |
| Boot ROM | Fusée Gelée exploited a T210 recovery USB handler before normal boot verification. | T239 is different silicon; the published T210 exploit and its memory-layout assumptions are not an established OZ entry point. |

The NX examples and their patches are described in [SwitchBrew's system flaws](https://switchbrew.org/wiki/Switch_System_Flaws), [ReSwitched's loader account](https://reswitched.github.io/ReswitchedWeekly15/) and [hexkyz's driver research](https://hexkyz.blogspot.com/2018/12/the-switch-memoir.html). They demonstrate why a chain fails when any necessary link is absent; they do not prove that OZ has no other vulnerabilities.

## Userland: an entry point is not a loader

[Reverse-engineered] On early NX, a vulnerable WebKit JavaScript engine provided browser-process memory access and native control. That process was sandboxed. `smhax` then bypassed service access checks because a session could request services before its process identity was initialized. In a separate loader chain, `expLDR` and service-manager access helped reach `fsp-ldr`; `nspwn` redirected a program path to an unsigned NSP container accepted by the executable filesystem path. ReSwitched's account describes the dependencies. NX firmware updates fixed individual links; this is not a claim that every NX software exploit had the same firmware range.

On OZ, [compatibility mode](../software/compatibility.md) adapts an NX game, not the old console's entire firmware image. The host game's vulnerability can remain, as the SmileBASIC entry in ns2jb shows, while the service and loader vulnerabilities it needs must be separately present on the newer system. The first published OZ chain instead used a Switch 2 `fs` path-normalization overflow and a loader/filesystem trust boundary. It demonstrated native system-process execution on 23.0.1. SwitchBrew records the `fs` flaw as fixed in 23.5.0. This comparison does **not** establish that every historic NX flaw was individually tested on every OZ version.

A `.nro` file is a format, not an exploit. The NX [homebrew launcher and runtime](../../nx/homebrew/toolchain-and-workflow.md) depend on a suitable launch environment and interfaces. A game-sandbox script host, a compatibility-mode game, and an OZ system-process launcher have different entry conventions, capabilities and graphics interfaces. Check the selected project rather than assuming a file extension gives it authority to launch.

## Recovery-mode hardware: why Fusée Gelée was different

[Reverse-engineered] On vulnerable early NX Erista/T210 units, the USB recovery handler in the boot ROM copied an attacker-controlled request length into an internal RAM buffer. The resulting overflow could overwrite boot-time control data before normal authentication completed. That bypassed the boot chain rather than breaking the cryptography. Recovery mode was a legitimate feature; the unsafe handler and its memory layout made the attack possible. [fail0verflow's ShofEL2 account](https://fail0verflow.com/blog/2018/shofel2/) independently documents the T210 bug.

The defect in already-secured early units could not be repaired by an ordinary Horizon update: execution had already reached ROM before that software ran. Later Erista production units used hardware-configured ROM patches, and Mariko units changed the boot implementation. Firmware downgrading does not change the chip's ROM or manufacturing fuse configuration. Even on affected hardware the vulnerability is permanent, but a USB-injected boot payload is not automatically persistent custom firmware.

On later NX hardware, some modchips use a physical fault-injection approach to disrupt a boot-time check and run a payload. That is a separate hardware attack, with its own target, timing and installation requirements; it is not Fusée Gelée and does not establish an OZ-compatible modchip. The sources reviewed do not establish a publicly demonstrated general OZ equivalent.

OZ uses T239, not T210. The T210 request handler, patch configuration and internal layout cannot simply be presumed to exist there. There is no publicly established Fusée Gelée-equivalent OZ boot-ROM chain in the sources reviewed as of 12 November 2026; this is a statement about published evidence, not proof that recovery or hardware attacks are impossible. Hekate is a bootloader/payload, not the vulnerability that gives it control, and an NX Hekate binary also expects NX hardware and boot interfaces.

## A research method, not an assumed exploit

The supplied PS4 post-mortem illustrates a useful cross-platform method: distinguish the first controlled process from later privileges, identify whether an interface is reachable from that process, and inspect object ownership, state transitions and the exact patch. Its browser, emulator, kernel and network examples are historical PS4 examples, not evidence that the same bugs exist on OZ. On OZ, compare the game-to-`fs` and `fs`-to-`ldr` boundaries in ns2jb: signed metadata alone did not guarantee that the code object supplied later was the intended one.

For a reproducible investigation, record the exact hardware, firmware, title and version, process, entry point, source revision and observed result. For each proposed step state the input, reachable interface, primitive, resulting permissions and next dependency. Compare patched and unpatched code where available; an exploit failing after an update does not by itself identify the fixed root cause. Distinguish a researcher report, an emulator workaround and a physical-console observation. Do not infer privilege, persistence or a working loader from a crash or from native instruction execution alone.

## Technical details and open questions

The published ns2jb chain demonstrates system-process execution, not kernel or boot-ROM control. SwitchBrew's 23.5.0 fix blocks its documented `fs` escalation; the continued availability of a particular game entry depends on its title, version and setup. Other access paths may exist or be discovered. A rigorous claim that a specific NX vulnerability also affects OZ would require identifying the corresponding OZ implementation, reachability, firmware and a reproducible observation.

## Open questions for research on an existing homebrew session

These are proposed experiments, not discoveries or claims that nobody has tried them. The sources reviewed for this page do not establish the answers for the exact configurations below; check current project issues and published research before starting. The aim is to identify observable boundaries and produce useful developer documentation, not to assume that a difference is exploitable. The [expanded research questions](../engineering/research-questions.md) continue this list with loader, shared-memory, compatibility, debugging and differential tests. The [benchmark plan](../engineering/benchmark-plan.md) covers performance questions separately.

**Common setup.** Record console model, firmware, title and update if applicable, entry point, process identity, launcher and library revisions, build flags, and whether the test runs in a game process or a system module. Keep raw logs and a minimal test case. A privileged ns2jb session on 23.0.1 and a game-sandbox session on 23.5.0 are different environments: availability in one is not evidence for the other. Compare firmware only on independently available consoles or supported test setups; do not change a working console's firmware just to complete a comparison. These proposals have not been run by Coffeepot.

### 1. Which service permissions actually follow the process?

- **Question:** How do service access and permitted system calls differ among a compatibility-mode host game, a native homebrew session and the system process used by ns2jb?
- **Experiment:** In each environment, run the smallest supported probe that records process identity and attempts to open a short, predefined list of documented services. For each, record the exact result of session acquisition and, only if allowed, a documented read-only query. Close every handle. Compare with the process metadata and [compatibility adjustments](../software/compatibility.md), where available.
- **Useful result:** A versioned matrix of allowed, denied and unavailable operations, with raw result codes and process context. A denied open does not reveal whether the service itself would accept a command; a successful open does not imply permission to invoke every command. Different environments may be impossible to compare with the same binary.

### 2. Does initialization order affect authorization?

- **Question:** Are documented service sessions consistently rejected until their required initialization completes, including after a reconnect or a failed initialization?
- **Experiment:** Select one service and one harmless, documented read-only command reachable in the existing process. Compare its result on a fresh session before initialization, after valid initialization, and on a newly opened session after closing the first. Repeat across the two process contexts only if the same service and command are available in both. Use the published interface definitions; do not infer request layouts from another firmware.
- **Useful result:** A state-transition table with exact return codes and whether a session remains usable after failure. NX `smhax` motivates examining initialization, but a different result alone is not proof of an authorization bypass: establish the service's intended contract and effective privileges first. See [Switch Security](https://switchbrew.github.io/34c3-slides/) for the NX background.

### 3. Where does a compatibility wrapper end and kernel enforcement begin?

- **Question:** For the documented core-mask query, does the wrapper's reported mask match what a controlled thread can actually request, and does the result vary by context?
- **Experiment:** In an existing compatibility-mode host that permits native test code, log the runtime's available-core query, the process metadata where accessible, and the result of requesting each core through the supported thread API. Run the same probe in a native session only if that runtime provides the equivalent API. Restore thread settings and check all return values.
- **Useful result:** Query result, requested mask and success or failure for each process, with software versions. This distinguishes a library-visible restriction from a kernel-enforced one; it does not imply a game can safely use a core merely because one request succeeds. SwitchBrew documents the compatibility mask; the exact behavior of this particular probe remains to be measured.

### 4. What is the contract at the filesystem path boundary?

- **Question:** Do path normalization and error handling differ between documented filesystem operations or firmware versions for equivalent paths?
- **Experiment:** Use an isolated disposable save or test directory, create a small set of known files, and issue documented read-only open or query operations on a bounded corpus of equivalent and invalid paths: repeated separators, dot components, parent components and backslashes where the API permits them. Record the input bytes, operation, returned result and resolved object where the API exposes it. Compare only equivalent filesystems and permissions; use a separate test console for another firmware.
- **Useful result:** A reproducible table of accepted forms, rejection codes and unexpected differences, plus a minimal case. A crash or changed error is a finding to investigate, not evidence of arbitrary memory access. The [ns2jb write-up](https://pepsipu.com/blog/2026-10-switch2-jailbreak-ns2jb/) and [SwitchBrew system flaws](https://switchbrew.org/wiki/Switch_System_Flaws) establish why path handling is worth testing, not that this corpus will find another flaw.

### 5. Which graphics and synchronization calls are truly available?

- **Question:** Which graphics services, memory operations and synchronization primitives can a specific homebrew runtime use, and what happens across resource creation and teardown?
- **Experiment:** Starting from a working sample for the selected launcher, log service opens and results for its normal initialization, one small buffer allocation, one submission, a completion wait and orderly destruction. Repeat with a fresh session and a controlled early exit. Compare the observations with the [GSV documentation](https://switchbrew.org/wiki/Switch_2:_GSV_services) and with emulator logs, recording where an emulator supplies a stub.
- **Useful result:** A minimal sequence of successful operations and exact failure points for one versioned runtime. A timeout can reflect missing synchronization, unsupported permissions or an emulator implementation; it is not by itself a hardware defect or a new privilege boundary. Do not infer retail API coverage from a launcher example.

### 6. Can a handle outlive the object or process state it represents?

- **Question:** What do documented handles and events do when their owner closes a session, the app suspends and resumes, or a client exits normally?
- **Experiment:** With a service and event the runtime explicitly supports, build a two-phase test: create and use the object, perform one lifecycle transition, then query or wait using the documented API and record the result. Compare a control run without the transition. Repeat separately for clean close and suspend/resume; avoid unbounded waits and retain a way to terminate the test.
- **Useful result:** A lifecycle table identifying signaled events, invalid handles, recoverable errors and required reinitialization. This can guide robust homebrew and emulator behavior. PS4 object-lifetime research motivates the question, but these observations alone cannot establish a use-after-free or an OZ vulnerability.

## Limits and risks

Historical descriptions here are for understanding trust boundaries, not installation instructions. Exploit attempts can crash the system or damage data, and an update may close a path. Consult the original project for the exact supported environment and keep independent backups.

## Sources

- [Switch Security, 34C3](https://switchbrew.github.io/34c3-slides/) — NX sandbox, service-manager and early exploit research.
- [ReSwitched Weekly 15](https://reswitched.github.io/ReswitchedWeekly15/) — expLDR, nspwn and their dependencies.
- [The Switch: a memoir](https://hexkyz.blogspot.com/2018/12/the-switch-memoir.html) — browser-to-driver escalation and its limitations.
- [ShofEL2](https://fail0verflow.com/blog/2018/shofel2/) — original T210 recovery-mode research.
- [Switch Homebrew Guide: checking RCM](https://switch.hacks.guide/user_guide/getting_started/what_you_need/) — NX model and entry-point distinctions.
- [SwitchBrew fuses](https://switchbrew.org/wiki/Fuses) and [hardware](https://switchbrew.org/wiki/Hardware) — NX hardware revisions and boot-ROM patching.
- [ns2jb technical write-up](https://pepsipu.com/blog/2026-10-switch2-jailbreak-ns2jb/) — OZ game, filesystem and loader boundaries.
- [SwitchBrew system flaws](https://switchbrew.org/wiki/Switch_System_Flaws) — documented vulnerability and firmware fixes.
- [mast1c0re research](https://github.com/CTurt/PS4-Mast1c0re) — PS4 emulator-boundary methodology; not evidence of an OZ flaw.
