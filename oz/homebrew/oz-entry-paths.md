# Switch 2 homebrew entry paths

A Switch 2 homebrew entry path is a way to start code or a program that Nintendo did not ship. What that program can do depends on *where it runs*: inside a game's scripting environment, as native code in that game's process, or in a separate system process. This guide compares demonstrated paths with research ideas, then explains how to turn an entry into a usable developer environment.

As of 12 November 2026, [CivHomebrew](https://github.com/XITRIX/CivHomebrew) provides game-hosted applications, and [ns2jb](https://pepsipu.com/blog/2026-10-switch2-jailbreak-ns2jb/) demonstrated native system-process execution on firmware 23.0.1. These outcomes are different; neither establishes a persistent, unrestricted custom firmware. See [homebrew environments](state-and-workflow.md) for current project prerequisites and [why NX entry paths differ](nx-entry-paths.md) for the original Switch comparison.

## What you need

Choose a path matching the console's **installed firmware**, available title and update, entry method and intended application. The game version can matter independently of the console's firmware. Check the upstream project's current instructions and supported configurations before acquiring software or changing the system.

For a research result, record console model, firmware, game and version, process context, entry point, launcher and library revisions, build options and whether it ran on physical hardware. Keep a known-good upstream example and an independent backup of any save used for testing. A game sandbox does not become a system-process session merely because both can run custom code.

## Choose a path

| Path | Evidence and prerequisites | Result and limitation |
| ---- | -------------------------- | --------------------- |
| Game-hosted app | [Documented] CivHomebrew requires an established CivSwitch setup using the physical Switch 1 edition of Civilization VII, version 1.0.0; check upstream for the console firmware and host requirements. | Runs CivSwitch JavaScript applications in the host environment. The project's `.civapp` format is not an NX `.nro`. |
| Game save and script runtime | [Documented] The [Balatro project](https://github.com/Luminger/balatro-switch-rce) describes a Lua environment entered through game save data; obtaining and installing the required save is a separate prerequisite. | Runs within the game process. Its permissions and library access are the host's, not a general system loader. |
| Privileged native session | [Reverse-engineered] ns2jb demonstrated a tethered SmileBASIC-to-system-process chain on Switch 2 firmware 23.0.1. | Native execution in the demonstrated process contexts, not a kernel or boot compromise. The filesystem escalation was patched in 23.5.0; other versions require separate verification. |
| Application-slot takeover | [Hypothesis] Investigate ns2jb loader substitution with a native application descriptor on a supported 23.0.1 session. | Hardware process creation, foreground status, resource budget and lifecycle are unverified; see [Path B1](#path-b1-investigate-an-nx-like-application-takeover). |
| Game plus a new service flaw | [Hypothesis] The demonstrated ns2jb chain makes this a credible *class* of research. No new post-patch escalation is established here. | A separate reachable flaw and a reliable way to use the resulting privileges would still be required. |
| Compatibility or hardware research | [Hypothesis] Compatibility layers and boot interfaces are distinct potential research surfaces. | Neither an NX compatibility feature nor NX hardware tooling establishes an OZ exploit. |

## Path A: make a game-hosted application

[Documented] CivHomebrew packages scripts, metadata and an optional icon into `.civapp` files. Its project provides a small sample and a host-side build and preview workflow. [Moonlight-CivSwitch](https://github.com/XITRIX/Moonlight-CivSwitch) is an example of a larger application adapted to the same environment. These are project-reported capabilities, not a guarantee that other runtimes or versions expose identical interfaces.

1. **Establish the control.** Use the upstream instructions to launch its smallest unmodified example. Record the firmware, game and update, launcher commit and build tools. A browser preview tests host-side presentation and logic, not console input, audio or timing.
2. **Inventory the host.** Add one independent check at a time for logging, input, drawing, audio, storage and networking. Log initialization, one operation, the result and cleanup. Mark each capability supported, denied, absent, failed or untested; do not conflate a missing wrapper with a missing system feature.
3. **Build a small complete application.** Separate update logic and data from host-specific input, rendering, audio and save adapters. Start with a deterministic scene and a versioned settings file. Use the host's documented package and save interfaces.
4. **Exercise lifecycle.** Test repeated launch, controller disconnect and reconnect, suspend and resume, normal exit, and recovery from an incomplete application-level save using disposable data. Record whether the host actually exposes each operation.
5. **Package and reproduce.** Build the same source twice using pinned dependencies; record package hashes and any generated assets. On a second supported setup, if available, test the package and record differences.

A host-based runtime can be useful even if no process boundary is crossed. [Hypothesis] A shared application interface for multiple suitable hosts could preserve portable game logic while providing separate adapters for each title. It would still need to negotiate features per host and cannot promise native GPU or filesystem access.

**Failure diagnosis:** An app that previews on a computer but fails on the console may depend on an unavailable host API, different timing or packaging. A successful launcher bootstrap only establishes that the bootstrap ran; test application imports and each subsystem separately. A save-triggered launch persists as game data, not as system-wide firmware modification.

## Path B: work within the demonstrated native session

[Reverse-engineered] The original ns2jb research describes a game-process entry, a filesystem-service flaw and a loader/filesystem trust-boundary attack, ending in native execution with system-process privileges on 23.0.1. The [maintained fork](https://gitlab.com/kurumuz/ns2jb/-/blob/main/README.md) documents SmileBASIC 4.4.9, a compatible host computer and a timing-sensitive setup; consult its README for the exact revision and other requirements. The fork describes its native probe as a debugging tool, not a stable general-purpose SDK.

1. **Reproduce upstream first.** Run the documented control unmodified and retain the run log, revision, process context and observed memory-layout information. Do not infer support for an untested firmware from a successful build.
2. **Classify the result.** Record the last completed stage, exact error, whether the remote session remains active and whether upstream cleanup completed. The fork warns that the filesystem stage is timing-sensitive and that interrupted cleanup may require a restart. A timed-out operation can have an unknown outcome; do not blindly replay it.
3. **Probe a narrow capability.** Using an interface explicitly exposed by the chosen session, test a simple return value or log, then allocation and orderly release. Add a documented read-only service operation only if the process and runtime permit it. Capture both service acquisition and command results.
4. **Verify the application interface.** If a launcher claims to support the same entry and firmware, first run its own sample. Record launcher and app commits, required imports, application format, service access, graphics initialization and shutdown. A native session is not by itself an `.nro` loader or a stable graphics interface.
5. **Turn a result into a developer contract.** Publish a small reproducible example, required toolchain versions, expected output, known failure modes, and separate build and physical-hardware verification. Keep native code, game-hosted code and host-PC helpers clearly identified.

[Reported] The fork's README describes a hardware test with failed acquisition attempts before a successful run; it also describes layout changes associated with a microSD Express support update. These observations are not a reliability estimate for every console. See the [project inventory](project-inventory.md) for other launchers and their stated scope.

## Path B1: investigate an NX-like application takeover

[Hypothesis] The most direct candidate for an NX-like experience on 23.0.1 is to investigate whether the demonstrated loader substitution can start a custom executable as a native OZ application using that application's existing signed metadata. This has **not** been demonstrated by Coffeepot or established by the sources reviewed. The ns2jb author describes substituting an executable while preserving signed metadata and demonstrates ordinary system-module execution; an application launch has additional process-manager, resource and lifecycle requirements. See [ns2jb](ns2jb.md) for the actual chain.

On NX, Atmosphère title override runs homebrew in place of a full application rather than an applet. That provides an application-class budget, not all physical system memory or unrestricted hardware control. A port reports roughly 3 GB of heap on NX, not all 4 GB; see [Atmosphère and running](../../nx/homebrew/atmosphere-and-running.md) and [NX memory](../../nx/hardware/memory.md). On OZ the target is a **measured native application budget**, not a promise of all 12 GB of physical memory.

### Gate 1: reproduce the existing helper

Use the exact supported firmware, entry, fork and instructions for [ns2jb](ns2jb.md). Reproduce the unmodified `ro`-policy helper and its cleanup on a research console. Record process identity, launch result, session state, failures and restoration. Keep an independent known-good control; do not alter the only working configuration while investigating a different target.

**Pass:** Repeated fresh launches demonstrate the documented helper and cleanup under recorded conditions. **Failure means:** Investigate acquisition, layout, connection or cleanup first; it says nothing about whether an application target can work.

### Gate 2: compare the application policy and launch context

With researcher-owned material and supported read access, compare one native application's signed program descriptor with the `ro` helper's descriptor: process type, service permissions, system calls, address-space type, core affinity and priorities. Inspect how an ordinary title reaches the application process manager, obtains its resource limit and becomes foreground. Distinguish metadata that describes allowed resources from allocations actually granted by the running system.

[Reported] The saved Eglantine-related Eden experiment examined four native executables and observed a 39-bit address-space type, core mask `0x3f` and priorities 28–59. The samples and local experiment do not establish a universal policy or an OZ hardware result; see [CPU and memory](../hardware/cpu-and-memory.md). A game's descriptor is not a general-purpose grant of every system capability.

**Pass:** A documented comparison and identified path for observing application creation and its resource limit. **Blocker:** If access to the target descriptor or the appropriate process-manager operation is denied, identify the missing privilege; a readable descriptor alone cannot create a process.

### Gate 3: test a minimal application-slot launch

[Hypothesis] If the existing authorized research session can supply a replacement executable for a valid native application descriptor and the process manager permits an application launch, test a minimal custom program for that *one* researcher-owned title. Reproduce the ordinary title launch as a control. Record the process-manager result, process identity, signed descriptor, foreground status, actual memory limit and allocation results, thread/core permissions, service access and orderly termination. Do not infer an application-class allocation merely from a 39-bit virtual address space.

**Pass:** A reproducible physical-console log showing that the custom executable actually occupies an application context, receives a measured application-class budget and exits so the normal title can launch again. Record all firmware and tool revisions, repetitions and cleanup. **If it starts under `ro` or a different context:** that is useful native execution, but it is not application takeover. **If creation or foreground activation fails:** preserve the exact stage and result; the selected session may lack permissions, or additional lifecycle and process-manager work may be needed. No reviewed source establishes that changing the target descriptor alone is sufficient.

### Gate 4: make the application usable

Only after measuring the application context, bring up one subsystem at a time: runtime entry and logging; heap and threads; application-manager lifecycle; controller input; a display clear and triangle; audio; application-owned save data; suspend, resume and clean exit. For each, compare against the same runtime's existing supported example and log the first failure. [OZ graphics services](../software/services.md) and [GPU](../hardware/gpu.md) describe why the graphics memory, synchronization and presentation stack requires separate work from NX.

**Pass:** A small complete program runs on hardware with the measured budget, accepts input, draws, plays audio, saves and exits, with suspend/resume behavior documented. A successful CPU-only launch does not prove the graphics services, application lifecycle or save system work. The [benchmark plan](../engineering/benchmark-plan.md) records how to measure without treating emulator timings as hardware performance.

### Gate 5: loader, applications and recovery

Once a repeatable application process exists, define a small homebrew loader and application ABI: executable format and relocation, imports, startup arguments, memory ownership, service and GPU resource ownership, error reporting and exit. First use a minimal test application; record the toolchain versions and build, emulator and hardware status separately. Compatibility with NX `.nro` and `libnx` is a later porting task, not a property of the executable file extension.

Add title selection, application launch interception and recovery only after the manual control is repeatable. Record what remains in memory, what is restored, and what requires re-acquiring ns2jb after a host disconnect or cold boot. The saved ns2jb demonstration uses a constrained helper and cleanup; it is not a general title-takeover loader. [Path B](#path-b-work-within-the-demonstrated-native-session) describes its control and failure boundaries.

### What an Atmosphère-like environment adds

[Documented] On NX, Atmosphère combines boot components, a secure monitor, system modules and application-level features; see [Atmosphère and running](../../nx/homebrew/atmosphere-and-running.md). An OZ project can pursue individual features without assuming that an NX component or its privileges port unchanged:

| Feature goal | Separate OZ question | Current evidence |
| ------------ | -------------------- | ---------------- |
| Application-slot loader | Can a supported session start a custom program as a foreground application with the target's real budget? | [Hypothesis] Gate 3; not demonstrated in this wiki. |
| Homebrew ABI and libraries | Can the actual process initialize input, audio, storage, display and GPU safely? | [Hypothesis] Needs per-interface hardware tests and a working loader. |
| Content overlays | Can a compatible held filesystem session redirect the desired title's content and cleanly restore state? | [Reported] [ns2jb-modloader](https://github.com/SebaaMG/ns2jb-modloader) describes a specific session-based implementation; see [project inventory](project-inventory.md). |
| More reliable or untethered startup | Can the required session be acquired and recovered without the host connection? | Unknown; the published ns2jb demonstration is tethered. A game-save trigger does not establish system-wide persistence. |
| Kernel or early-boot modifications | Is an independently demonstrated path available with the required privilege and startup point? | Not established by ns2jb; a new boundary would have to be demonstrated. Kernel access, secure-monitor access and persistent boot control are distinct. |

[Hypothesis] A useful application loader and some overlay features might be achievable without duplicating every NX boot component. Conversely, a feature that requires kernel or early-boot authority cannot be assumed available from a filesystem or ordinary system-module compromise. On 23.5.0 the documented filesystem escalation is patched; a new or different verified entry would be needed for this research on that firmware.

### Use emulation to prepare, hardware to decide

The supplied private `switch2/NX2_NOTES.md` describes a local Eden-derived Switch 2 experiment; it says that work was not upstreamed or pushed. The file is not part of the published wiki. It can guide a researcher toward metadata parsing, application startup and graphics initialization questions, but the recorded runs used automatic success stubs and include deliberately approximate behavior. Do not attribute all local changes to upstream Eglantine or treat a simulated process launch as a hardware authorization result.

Use the emulator to construct a minimal test and locate the first missing initialization step. Then run the equivalent supported probe on hardware and compare process identity, return codes, memory allocation, event order and output. Retain emulator commit, configuration, enabled stubs and firmware context. A matching result is evidence only for the tested operation; emulator frame times are never console measurements. See [differential research](../engineering/research-questions.md#9-which-emulator-assumptions-match-hardware).

## Path C: investigate a new service boundary

[Hypothesis] The strongest software research direction suggested by the sources is a repeatable game entry combined with a *different* reachable system-service vulnerability. The known 23.0.1 chain proves that one such composition existed; it does not establish that another is present on 23.5.0 or any other firmware.

### 1. Map the actual foothold

Start with a working entry point and determine whether it exposes scripts, native calls or a project-specific bridge. Identify the process and its documented permissions. For each candidate service, separately record whether the game can acquire a session and whether a harmless documented query succeeds. An absent wrapper, denied session, unknown command and failed initialization are different results.

**Deliverable:** A capability matrix tied to the game, firmware, entry and library revision. If the bridge cannot perform the necessary operation, that is a blocker for this route; it is not evidence that the service is globally inaccessible.

### 2. Select a boundary worth investigating

Prioritize a service only when the entry can reach it, its operation crosses into a separate process, and its documented behavior can be tested with a valid client. Shared buffers, asynchronous requests, resource ownership, parser inputs and cancellation are useful *questions* because they require agreement between components. Determine the service process's permissions before assuming a compromise there would help load homebrew.

The published ns2jb chain is a real example of a game-reachable service boundary and later loader/filesystem trust boundary. [ReSwitched's account](https://reswitched.github.io/ReswitchedWeekly15/) gives a separate historical NX example of filesystem and loader assumptions. Neither is evidence that their specific defects remain on OZ.

### 3. Establish and vary a control

Build the smallest legitimate client for one documented operation. Record the session setup, identity available to the caller, input and output lengths, result, events, timeout and teardown. Then vary one documented condition at a time: a valid boundary-sized buffer, a permitted cancellation, a reconnect, or an equivalent path within a disposable test filesystem. Compare with the unchanged control and retain exact inputs and raw results. The [research questions](../engineering/research-questions.md) specify concrete experiments and what their outcomes would establish.

**Deliverable:** A minimal reproducible behavior difference, including negative results and inaccessible cases. A crash or changed return code is not yet proof of memory corruption, a privilege escalation or a usable loader. If a result differs between firmware versions, keep process context, entry, input and tool revision constant where possible; otherwise name the confounder.

### 4. Demonstrate the claimed capability

For any candidate finding, distinguish control of the initiating game from control of the receiving process. Establish the affected component, target version, prerequisites, repeatability, effective permissions and cleanup behavior. A useful research report states what was observed and what remains unknown; a successful request or crash alone cannot establish code execution.

### 5. Engineer the missing pieces

Even a demonstrated service-process compromise leaves work: a repeatable acquisition path, process-safe execution, a launcher and application interface, memory and graphics access where permitted, and correct cleanup. Build each against a working control. An existing game sandbox can continue to host useful apps while this longer investigation proceeds.

## Path D: compare compatibility behavior

[Reverse-engineered] SwitchBrew describes Switch 1 compatibility modules and selective adaptations on Switch 2. [Hypothesis] Their boundary between an older game's assumptions and newer services is a useful place to compare behavior; no particular new vulnerability is established by that observation.

Use a researcher-owned NX test program or suitable host that can run an equivalent benign operation on both generations. Compare one documented result at a time—such as timer behavior, a supported thread-affinity request, or an input lifecycle event—and record game, firmware, controller and process context. Where a native OZ implementation of the same operation is available, use it as an additional control. Trace a difference to a wrapper, service or application only after eliminating version and test-input differences.

**Useful result:** A minimal test plus logs that show where application-visible behavior differs. A discrepancy can be an intentional adaptation and does not establish an escape. This route may instead produce valuable porting documentation and emulator tests; see [compatibility mode](../software/compatibility.md).

## Research leads: where to look next

These are hypotheses and research priorities, not claims of new or untried vulnerabilities. The detailed controls and reporting requirements are on the [research questions page](../engineering/research-questions.md). For each idea, establish the entry, reachable interface, receiving process, required privileges and useful result before investing in a chain.

| Lead | Evidence and first useful investigation | What remains unproved |
| ---- | -------------------------------------- | -------------------- |
| Capability-rich game host | [Documented] CivSwitch and Balatro provide different game-process entries. Test a host's input, rendering, audio, networking and save interfaces using a harmless sample; compare supported game and firmware versions. | A game entry is not a system escape. An old title or embedded runtime is not necessarily vulnerable or launchable on every firmware. |
| Alternate entry to a known escalation | [Documented] [IZCIVSwitch](https://github.com/byizmysh/IZCIVSwitch/blob/main/DISCLAIMER.md) describes combining Civ7 entry with the known ns2jb filesystem stage. Compare which requests each entry can actually make. | Reusing the same escalation does not make its patched `fs` flaw work on 23.5.0. Different entry paths may have different privileges. |
| Driver and buffer boundaries | [Reverse-engineered] Historical NX [nvhax research](https://hexkyz.blogspot.com/2018/12/the-switch-memoir.html) found a pointer contract problem in a driver service. Start with a reachable, documented OZ interface and map client, service and buffer ownership; compare normal completion and supported cleanup. | No analogous OZ defect is established. A crash or permission error is not proof of a controlled write. |
| Patch and neighboring contracts | [Reverse-engineered] SwitchBrew records the ns2jb filesystem fix in 23.5.0. Compare the documented old and new behavior, then examine related reachable operations using a disposable filesystem and bounded, valid controls. | A changed check does not establish a second vulnerability, affected version or usable escalation. |
| Object identity across components | [Reverse-engineered] The ns2jb author describes a loader/filesystem trust boundary after `fs` compromise. Trace who supplies a researcher's own test object's identity, permissions and content through each supported operation. | A rename or different result is not proof of an authorization bypass; a test requiring an already-privileged `fs` session is not an unprivileged entry. |
| Asynchronous ownership | [Hypothesis] For a documented operation reachable from the chosen host, compare completion, permitted cancellation and clean close; log event order and buffer ownership. | A timeout or delayed event may be expected. A usable escalation requires a separately demonstrated defect, control and privilege. |
| Independently reachable input | [Documented] Historical PS4 [PPPwn](https://github.com/TheOfficialFloW/PPPwn) and NX wireless research illustrate different input boundaries. Inventory only interfaces actually present and reachable on the researcher's OZ device; begin with a normal exchange and documented controls. | The PS4 protocol and vulnerability are not evidence of an OZ equivalent. An accessible parser is not an exploit or homebrew loader. |

The same host can be useful as a game-scoped development environment even if no escalation is found. A reusable capability probe, a minimal reproducer and negative results are valuable outcomes in their own right. [Hypothesis] For an NX-like environment, the application-slot experiment in [Path B1](#path-b1-investigate-an-nx-like-application-takeover) is the most direct next test on a supported ns2jb session.

## Path E: investigate hardware separately

[Hypothesis] A new physical fault or boot-interface research path is possible in principle, but the sources reviewed do not establish a generally available OZ equivalent of Fusée Gelée. A researcher would first need evidence for the target chip's actual boot stages, reachable interface and security state; an NX payload, fault timing or T210 memory map is not such evidence. Physical instrumentation, board access and repeatability are separate engineering problems. See [NX entry paths](nx-entry-paths.md) for the historical distinction.

This path is a long-term research question, not a prerequisite for developing a game-hosted app and not a promised consumer installation method.

## Technical details: a reusable probe

A useful common harness has a small portable core and one adapter per execution environment. Each probe should return a structured record containing a stable test name, input description, environment, setup result, operation result, observed output, cleanup result and verification level. Keep raw numeric results alongside human-readable decoding. Use timeouts for waits and label an interrupted operation **outcome unknown** rather than automatically retrying it.

| Layer | Responsibility |
| ----- | -------------- |
| Host adapter | Launch, logging, input and available timing for that game or native runtime. |
| Capability probe | Test one documented operation with a known-good control and bounded inputs. |
| Collector | Store environment and raw results; remove personal data and console-specific secrets before sharing. |
| Comparison | Compare equivalent inputs and contexts; flag unavailable or incomparable cases instead of reporting them as failures. |

No such harness is provided or verified by Coffeepot. A host-PC preview, emulator run, hardware run and measured performance result are different verification levels. Never use emulator timing as console timing.

## Limits and risks

Firmware, game versions and launcher projects change. An update may remove an entry or escalation; do not change a working research console's firmware just to compare versions. Exploit attempts can crash the console, and a failed cleanup or modified save can leave data in an unexpected state. Follow the original project's backup and recovery instructions and use disposable test data.

Do not infer that an application-process entry grants kernel privileges, that a system-process session gives boot control, or that any of these methods persists after a restart. Report unexpected security findings responsibly and exclude personal data, keys and proprietary content from published logs.

## Sources

- [CivSwitch](https://github.com/Gezine/CivSwitch) — Civilization VII game entry and host prerequisites.
- [CivHomebrew](https://github.com/XITRIX/CivHomebrew) — game-hosted applications, `.civapp` packaging and example build workflow.
- [Moonlight-CivSwitch](https://github.com/XITRIX/Moonlight-CivSwitch) — an application adapted to the game-hosted environment.
- [Balatro RCE](https://github.com/Luminger/balatro-switch-rce) — game-save entry and Lua environment.
- [ns2jb technical write-up](https://pepsipu.com/blog/2026-10-switch2-jailbreak-ns2jb/) — demonstrated 23.0.1 game-to-system-process chain and loader boundary.
- [kurumuz/ns2jb README](https://gitlab.com/kurumuz/ns2jb/-/blob/main/README.md) — version-specific prerequisites, debugging, observed layouts and cleanup.
- [SwitchBrew system flaws](https://switchbrew.org/wiki/Switch_System_Flaws) — documented filesystem flaw and 23.5.0 fix.
- [SwitchBrew compatibility mode](https://switchbrew.org/wiki/Switch_2:_Compatibility_Mode) — observed compatibility modules and adjustments.
- [ReSwitched Weekly 15](https://reswitched.github.io/ReswitchedWeekly15/) — historical NX service and loader-boundary research.
- [HBLauncher](https://github.com/ravensclawx/HBLauncher) — native launcher and project-specific application examples.
- [IZCIVSwitch disclaimer](https://github.com/byizmysh/IZCIVSwitch/blob/main/DISCLAIMER.md) — project-described Civ7 entry and reuse of the known filesystem stage.
- [The Switch: a memoir](https://hexkyz.blogspot.com/2018/12/the-switch-memoir.html) — historical NX driver process and pointer-boundary research.
- [PPPwn](https://github.com/TheOfficialFloW/PPPwn) — historical PS4 network entry methodology, not an OZ vulnerability.
