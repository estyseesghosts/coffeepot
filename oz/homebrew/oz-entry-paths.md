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

The leads below are a prioritized research agenda, not known OZ vulnerabilities. They are grounded in published work, but Coffeepot cannot establish that other researchers have not already tested a particular idea. Start with an entry and a documented control that actually work on the target. For every candidate, write down **attacker-controlled input → first parser → process and permissions → next boundary → intended homebrew result**. Stop when any required step is unavailable; do not count an interface name as reachability.

### 1. Preserve an old application runtime and build inside it

**Why consider it:** [Documented] CivSwitch and Balatro both demonstrate game-process entry through different application surfaces. On PS4, [Vue After Free](https://github.com/Vuemony/vue-after-free) is a historical example of an old application retaining a vulnerable embedded runtime; that is not evidence that any particular OZ title has the same defect.

**First investigation:** Catalogue games already available to the researcher that contain an interpreter, user-content pipeline or embedded web interface. For each version, record how the input enters, whether a user can supply it without prior privileged access, what the runtime can express and whether the program can save its own state. Inspect upstream engine documentation and independently obtainable symbols or version information when available; do not equate an engine name with a vulnerable build.

**Experiment and decision:** Use a harmless marker program or a supported content example to map input, display, input devices, audio, networking and storage. Record what survives a game restart and what requires a companion computer. A host with a usable scripting interface may already support a game or utility; a parser that only crashes provides no such environment. A vulnerability in an installed game may survive a system update, but the platform may block the title or the game's own update may change it.

**Practical next step:** Extend an existing `.civapp` or Lua sample with a capability manifest and a deterministic save/reload test. This is a lower-cost route to game-scoped homebrew than assuming a native system escape. A successful test demonstrates only the tested host and version.

### 2. Separate entry delivery from escalation

**Why consider it:** [Documented] The [IZCIVSwitch project](https://github.com/byizmysh/IZCIVSwitch/blob/main/DISCLAIMER.md) describes using a Civ7 entry with the known ns2jb filesystem stage. That is an example of changing the initial host while reusing an existing escalation, not a new 23.5.0 escape. The [CivSwitch project](https://github.com/Gezine/CivSwitch) describes its own game entry.

**First investigation:** Compare a game-hosted script runtime, a game with a native-call bridge and a system-process session as distinct rows. For each, record whether it can express a service request, acquire the same service, construct the documented inputs and obtain the same response. Record save installation and computer or accessory requirements independently from the console-side capability.

**Experiment and decision:** Run the same harmless read-only operation through each supported interface; compare actual identity, request and result rather than assuming a shared service name implies identical behavior. If one host cannot express a request, a documented bridge or different host would be needed before testing that route. A surviving game entry on 23.5.0 does not restore the patched `fs` stage.

**Useful artifact:** A compatibility matrix that tells a future developer which entries can host an application, which can reach a candidate boundary and which merely improve delivery. Pin project commits and firmware; do not generalize from one fork's supported version range.

### 3. Check driver assumptions at the process boundary

**Why consider it:** [Reverse-engineered] On NX, [hexkyz's nvhax research](https://hexkyz.blogspot.com/2018/12/the-switch-memoir.html) describes a driver interface whose pointer assumptions changed when the implementation ran in a separate service process. It required several additional primitives to reach process takeover. This is a historical analogy, not a claim that the same command, bug or memory model exists on OZ.

**First investigation:** Starting from a documented graphics or other device interface reachable by the chosen entry, map the calling process, service, transport and allocation ownership. Use the selected runtime's existing sample to identify where data is copied, mapped or returned; distinguish a client address, a handle, a shared-buffer offset and a value interpreted by the service. The OZ [graphics services](../software/services.md) page and the upstream launcher are starting points, not proof of permission.

**Experiment and decision:** With a valid request and researcher-owned buffer, vary supported lengths and lifetimes within the documented client contract. Compare normal completion, documented cancellation and orderly teardown. If the game cannot reach the interface, or the only description is from an emulator stub, record that blocker and choose another candidate. An output difference or crash is not an arbitrary-write primitive.

**Useful artifact:** A diagram of buffer ownership and a minimal valid client with results per context and firmware. This is valuable to driver and homebrew authors even if no security defect is found.

### 4. Follow a patch to neighboring contracts

**Why consider it:** [Reverse-engineered] SwitchBrew documents the 23.5.0 fix for the ns2jb filesystem path-normalization issue. That is an OZ-specific starting point. The PS4 pOOBs4 history illustrates the method of comparing versions to understand a check; it is not evidence that the same filesystem defect exists on OZ. See [pOOBs4](https://github.com/ChendoChap/pOOBs4) and the [SwitchBrew flaw registry](https://switchbrew.org/wiki/Switch_System_Flaws).

**First investigation:** Use legally obtained, version-identified material and available public analyses to identify which input, length, allocation and consuming operation the known fix changes. Build an offline model of the documented path contract. List other operations that accept related path representations, then verify which are actually reachable from the chosen host and which filesystem types and flags apply.

**Experiment and decision:** On separately available supported setups, compare a bounded set of valid and rejected path forms in a disposable filesystem, using a known-good control. Check the exact error and resolved object where observable. Review neighboring length calculations and normalization stages offline; a firmware difference or patched function is a lead, not proof that an adjacent code path is faulty or reachable.

**Useful artifact:** A versioned path-handling table, minimized test corpus and explicit list of unchecked or inaccessible operations. Do not update a console merely to obtain the comparison.

### 5. Ask whether two components agree on the object

**Why consider it:** [Reverse-engineered] The [ns2jb author](https://pepsipu.com/blog/2026-10-switch2-jailbreak-ns2jb/) describes a loader and filesystem boundary where authenticated metadata and the executable filesystem were supplied through separate interactions after `fs` compromise. On PS4, [CTurt's emulator research](https://cturt.github.io/mast1c0re-2.html) shows another kind of cross-process shared-memory trust. Neither example establishes an unprivileged OZ bypass.

**First investigation:** Choose a researcher-owned test resource and diagram who supplies its name, handle, metadata, permissions and content. Identify the first validation and the later consumer, and whether they refer to the same object or separate representations. Determine whether the host can observe either boundary without already possessing the privilege the proposed chain is supposed to obtain.

**Experiment and decision:** With a supported API and disposable files, compare a stable control with a documented close/reopen or rename between *separate* operations. Record the object identity only where the API exposes it, authorization results and data read. Do not claim a check/use flaw merely because a name changes or a load fails. If testing requires privileged filesystem impersonation, label that as a prerequisite and do not sell it as a game-sandbox entry.

**Useful artifact:** A sequence diagram and a list of identities and permissions that can be verified on hardware. An unresolved identity link is a research question, not proof that a forged object will load.

### 6. Audit asynchronous resource ownership

**Why consider it:** [Documented] PS4 research includes distinct object-lifetime defects and network-parser exploits such as [PPPwn](https://github.com/TheOfficialFloW/PPPwn); their causes and affected interfaces are platform-specific. The general lesson is to ask who owns a request, handle or buffer across completion, cancellation and process exit.

**First investigation:** Select an OZ operation that the actual host can invoke and whose ordinary completion is understood. Diagram creation, submission, completion, cancellation and destruction, including event and buffer ownership. Prefer source or a published interface description and identify whether the worker is in the caller, a service or the kernel.

**Experiment and decision:** Compare a successful control with one documented cancellation or close transition per run. Log event order, request state and cleanup using bounded waits; repeat after clean restart. If a timeout makes the operation's outcome unknown, do not repeat it against the same resources. A failed handle lookup or delayed event may be intended behavior; demonstrate a contract violation before assigning cause.

**Useful artifact:** A lifecycle state machine and a reproducible test that improves clients and emulators. Finding a race would still leave reachability, control, privilege and reliability to establish separately.

### 7. Look for independently reachable input, but do not assume PS4 paths exist

**Why consider it:** [Documented] [PPPwn](https://github.com/TheOfficialFloW/PPPwn) reached a PS4 network parser without a browser entry. NX research also documented Bluetooth and local-wireless bugs in different services; consult the [SwitchBrew flaw registry](https://switchbrew.org/wiki/Switch_System_Flaws) for platform and version scope. These are examples of *how to select an input boundary*, not evidence that OZ has PPPoE, the same vulnerable code or a usable equivalent.

**First investigation:** Inventory input the OZ device deliberately accepts in the researcher's own controlled environment: a documented local connection, an owned controller, a game-hosted file or a supported accessory. Identify the first parsing component and whether the input needs pairing, user action, a specific title or a privileged client. Inspect public protocol documentation and the implementation's version provenance before choosing a target.

**Experiment and decision:** Capture a normal exchange with owned devices and compare it with its documented format. Reproduce benign, bounded variations in a local harness or test environment and log acceptance and rejection; do not test other people's devices or online services. If the relevant feature or parser cannot be reached, mark the route blocked. A reachable parser is not evidence of memory corruption or of a path to load homebrew.

**Useful artifact:** An annotated protocol/state diagram and a regression corpus that another researcher can reproduce. A meaningful route would still require demonstrating the affected process and a way to run useful code under its actual restrictions.

### 8. Rank the next step by the missing dependency

[Hypothesis] The practical route with the best near-term return is to combine a capability-rich existing game host with a reusable probe suite. It can support sandboxed applications now and expose measurable service boundaries for later research. A separate bug may never be found; the application and test results remain useful.

| Observation | Next engineering question | Do not conclude |
| ----------- | ------------------------- | --------------- |
| A host runs a script | Which input, rendering, audio and save APIs are actually exposed? | That the host loads arbitrary native programs. |
| A service session opens | Which documented commands are authorized and what process handles them? | That every command is allowed or that the service has useful extra privileges. |
| A malformed request crashes | Which component failed, and does a minimal input reproduce it after a clean restart? | That the researcher has a controllable memory primitive. |
| A service process is controlled | What permissions, recovery and loader interfaces are actually available? | That the kernel, secure monitor or boot chain is controlled. |
| An emulator gives a convenient result | Does equivalent hardware produce the same result? | That the emulator's stub is the console's contract. |

This ranking reflects usefulness and reachability, not a numerical probability. For an experiment's exact controls and expected artifacts, use the [research questions](../engineering/research-questions.md) rather than treating these hypotheses as completed findings.

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
- [CTurt: mast1c0re compiler attack](https://cturt.github.io/mast1c0re-2.html) — historical PS4 cross-process trust-boundary analysis, not an OZ result.
- [Vue After Free](https://github.com/Vuemony/vue-after-free) — historical PS4 example of a preserved application runtime, not an OZ result.
- [pOOBs4](https://github.com/ChendoChap/pOOBs4) and [PPPwn](https://github.com/TheOfficialFloW/PPPwn) — historical PS4 patch analysis and network entry methodology, not OZ vulnerabilities.
