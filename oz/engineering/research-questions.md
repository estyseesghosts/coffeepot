# Switch 2 research questions

These experiments target questions that could improve native homebrew, compatibility research and emulation on an already-homebrewed Switch 2. They complement the practical [OZ entry paths guide](../homebrew/oz-entry-paths.md), the six boundary tests on [NX entry paths](../homebrew/nx-entry-paths.md) and the [benchmark plan](benchmark-plan.md). They are proposals, not established results or claims of novelty: check the linked projects and current research for the exact question and version before starting. Coffeepot has not run them.

## Before you start

An existing homebrew session is not a uniform privilege level. A game-sandbox program, a compatibility-mode process and the system-process execution described in [ns2jb](https://pepsipu.com/blog/2026-10-switch2-jailbreak-ns2jb/) have different capabilities. Each question states its minimum practical context; if the launcher does not expose the required API or the host forbids it, record that blocker instead of assuming a different privilege level.

Use a spare test title, disposable test data and an independent backup. Record console and firmware, title/update and process, entry point, launcher and library commits, build flags, mode, input and raw result. Keep a known-good control and change one variable at a time. Do not update, downgrade or modify system partitions to obtain a comparison; use separately available devices or test environments. Distinguish a documented interface, a hypothesis, an emulator workaround and a physical-console observation.

## Where to start

| Priority | Investigation | Why it is useful |
| -------- | ------------- | ---------------- |
| First | [7. Runtime capability manifest](#7-what-can-a-launcher-actually-provide), [8. Error and interface versioning](#8-which-interface-version-produced-an-error), [9. Hardware and emulator comparison](#9-which-emulator-assumptions-match-hardware) | Makes other experiments reproducible and prevents false conclusions. |
| Next | [10. Loader identity](#10-does-the-loader-bind-validation-to-the-same-content), [11. Shared-memory ownership](#11-who-owns-a-shared-buffer-and-for-how-long), [12. Asynchronous cancellation](#12-what-happens-when-asynchronous-work-is-cancelled) | Tests the kinds of cross-component assumptions highlighted by prior research. |
| Follow-on | [13–18](#13-do-buffer-lengths-and-reported-sizes-agree), [19–21](#19-which-clock-is-a-port-actually-using) | Improves compatibility, debugging and practical portability. |

Numbers continue the list on [NX entry paths](../homebrew/nx-entry-paths.md). Priorities are judgment calls about usefulness, not evidence of an undiscovered issue.

## Runtime and evidence

### 7. What can a launcher actually provide?

**Context:** Each available launcher or game-sandbox runtime; no additional privilege assumed.

**Experiment:** Build the launcher's smallest supported example and add one independent probe at a time for logging, allocation, input, timing, file access, audio and graphics. For each, record whether the interface is exposed, initializes, performs one documented operation and cleans up. Record missing imports and exact errors. Repeat after a clean restart; use the same source revision and build settings.

**Deliverable:** A versioned capability matrix and the smallest source/build instructions needed to reproduce it. Distinguish unsupported, untested, denied, initialization failure and success. A wrapper's absence does not prove the underlying system lacks a feature; a successful example does not establish support in other contexts. [HBLauncher](https://github.com/ravensclawx/HBLauncher) and [libnx-ounce](https://github.com/alula/libnx-ounce) provide starting points, subject to their own prerequisites.

### 8. Which interface version produced an error?

**Context:** A session with access to one documented service and its client library source.

**Experiment:** Pick a read-only operation that has a known valid request. Log the service name, session acquisition result, library revision, command selected by that library, request and response sizes, result code and any documented interface-version query. Run a positive control and one invalid parameter at a time; compare separately available firmware only when caller, input and library are otherwise equivalent.

**Deliverable:** A small interface-and-error matrix and a transcript showing where each failure occurs. This helps distinguish a denied session, mismatched protocol and invalid input. Do not publish private identifiers or raw buffers containing user data. A changed result is not proof of a changed service implementation until caller and protocol differences are excluded.

### 9. Which emulator assumptions match hardware?

**Context:** A small program runnable on the same supported interface on physical hardware and in an emulator.

**Experiment:** Select one operation for which an emulator explicitly uses a stub or provisional answer in [Eglantine's notes](https://github.com/Eglantine-Community/Eglantine/blob/nx2/NX2_NOTES.md). Construct a minimal client with a documented valid request and a bounded wait; collect return values, output lengths, event order and teardown behavior on each. Repeat after a fresh launch. Keep the emulator commit and test environment with the logs.

**Deliverable:** A differential test fixture containing expected observations per environment, with unknown fields marked unknown. A single matching result does not establish the full protocol; emulator success or timing is not a hardware result. If the hardware session cannot access the interface, report that as a limitation.

## Service and loader boundaries

### 10. Does the loader bind validation to the same content?

**Context:** A native or research session able to observe documented loader and filesystem operations, plus a researcher-owned test title or controlled test content. No expectation of system-wide loader access.

**Experiment:** Map the legitimate path from content identification to filesystem open and load, recording the object identity and documented errors visible at each boundary. With disposable test content, compare a stable control with benign replacement or rename between separate operations only where the API permits it. Stop at authorization failures. [ns2jb](https://pepsipu.com/blog/2026-10-switch2-jailbreak-ns2jb/) motivates the identity question, but its privileged manipulation is a different experiment.

**Deliverable:** A sequence diagram of observable decisions and the points at which object identity can or cannot be established. Inability to inspect a loader decision is an unknown, not evidence that it did not check. Do not interpret a failed load as proof of a particular signature check without a corresponding control.

### 11. Who owns a shared buffer, and for how long?

**Context:** A documented service and launcher that support a shared or transfer-memory buffer.

**Experiment:** Use one researcher-owned buffer containing a known pattern. Record initialization, registration, documented handoff, completion and release. Vary one legitimate lifecycle action per run: retain the buffer until completion, close the session after completion, or cancel using a documented API before releasing it. Check guard data in the client-owned allocation and log return values and event order.

**Deliverable:** An ownership and lifetime diagram, including the earliest confirmed safe reuse point and unanswered cases. A changed pattern may be expected output; establish the documented write range first. A crash alone does not identify the component responsible. This provides a basis for correct clients and for future object-lifetime audits.

### 12. What happens when asynchronous work is cancelled?

**Context:** A documented asynchronous operation with a completion event and supported cancellation or close behavior.

**Experiment:** Issue a bounded request against disposable data. Compare normal completion with cancellation before completion and session close after cancellation. Log a monotonic timestamp, call result and event state at every step; repeat enough times to expose nondeterministic ordering. Use a timeout, then perform a clean restart between runs where necessary.

**Deliverable:** A state machine showing which outcomes occur and which transitions are unresolved. Test event ordering independently from output correctness. The PS4 material motivates investigating ownership and races, but it does not establish that this operation has a race or that an observed timing difference is exploitable.

### 13. Do buffer lengths and reported sizes agree?

**Context:** A documented read-only query with caller-supplied output buffer and a documented size or count field.

**Experiment:** First establish a valid baseline. Then vary only the supplied buffer capacity around the documented boundary, including zero where supported, the exact reported size and a slightly smaller capacity. Initialize the client buffer with a pattern; record the returned result, reported length and bytes actually changed within the allocation. Use only the published request format and bounds of the client API.

**Deliverable:** A table of size contracts and a minimal failing input if one exists. A partial output on error may be permitted: check the documented contract before calling it a flaw. Do not read outside the allocation or treat a client-side guard check as proof of server memory safety.

### 14. Are authorization decisions stable across reconnects?

**Context:** Two available process contexts with different documented permissions, or one process able to test its own sessions before and after a supported lifecycle transition.

**Experiment:** On each independently acquired session, record identity and authorization for the same harmless read-only command. Close and reopen the session, then repeat. If the runtime supports it, repeat after suspend/resume. Do not transfer handles between processes unless the documented API expressly supports that operation.

**Deliverable:** A session-by-session table of results and process identity, distinguishing session acquisition from command authorization. This extends the initialization question to reconnects without assuming privileges are inherited or that identities can be spoofed. A difference can be intentional policy or a different process, so retain both controls.

## Content, compatibility and lifecycle

### 15. Are file identity and save commits consistent across reopen?

**Context:** An application-owned disposable save or filesystem and the runtime's documented file and commit APIs.

**Experiment:** Write a small versioned test record with a checksum, close it, perform the documented commit, then reopen and verify it. Compare with a control that closes without an explicit commit, if the API permits it. Repeat across clean app exit and a supported suspend/resume transition. Use separate disposable files for each run; never interrupt or modify system storage.

**Deliverable:** Observed visibility and persistence by operation and lifecycle, with errors and exact sequence. A successful reopen in one process is not proof of crash consistency or durability after power loss. This establishes what a developer can rely on before designing recovery experiments.

### 16. Which compatibility adaptations are title-specific?

**Context:** Two researcher-owned NX titles or test programs usable in compatibility mode, with an equivalent benign operation in each; optional native comparison only where feasible.

**Experiment:** Compare documented input, timing or save behavior under the same observable conditions, recording title version and the relevant available compatibility configuration. Use controls for controller mode and lifecycle. Where a function is documented as hooked, log the application-visible result rather than patching the system to disable the hook.

**Deliverable:** A per-title behavior matrix linked to the known [compatibility configuration](../software/compatibility.md). A difference does not prove a title-specific patch caused it: game code, build and runtime may differ. A specially constructed common test program is stronger evidence than unrelated games.

### 17. How do input capabilities change during a session?

**Context:** A runtime with supported input and at least one controller; test additional controller types only if available.

**Experiment:** Log the runtime-reported controller identity, available features, state and events while attaching, detaching, reconnecting and changing supported controller modes. Use a controlled sequence with a timestamped input log and repeat after suspend/resume if supported. Compare the application-facing API with the published [controller research](https://github.com/ndeadly/switch2_controller_research), without assuming raw report identifiers are visible to applications.

**Deliverable:** An event sequence and minimal reconnect test for each runtime. This helps a game rebuild its input state without stuck buttons. Absence from an API is not evidence the hardware lacks the feature.

### 18. What happens to GPU resources on suspend and resume?

**Context:** A launcher with a working graphics sample and a supported lifecycle callback.

**Experiment:** Display a deterministic image, record resource creation and completion, suspend and resume normally, then redraw and compare a checksum or captured image where supported. Run a clean non-suspend control. Log all callbacks, synchronization results and resource recreation; test one resource type at a time.

**Deliverable:** A versioned lifecycle recipe identifying resources that survived, needed recreation or could not be inspected. A blank frame can arise from lost focus, presentation state or memory visibility, so separate those checks before attributing it to a GPU fault.

## Timing, implementation and firmware

### 19. Which clock is a port actually using?

**Context:** Any runtime exposing a supported monotonic timer; a hardware reference clock is optional.

**Experiment:** Log successive elapsed-time readings during a fixed workload, an idle interval and a supported suspend/resume. Where available, compare a separate GPU timestamp or host-observed interval, clearly recording its units and frequency source. Run the identical test under compatibility and native paths only if both support it.

**Deliverable:** Timer units, monotonicity, observed suspend behavior and conversion procedure for each environment. Host network latency and presentation timing make poor ground truth for short intervals; record measurement uncertainty. Do not infer a CPU clock rate from a GPU timestamp.

### 20. Which import or ABI assumption prevents startup?

**Context:** A native launcher with a supported example and an application built against that launcher's documented interface.

**Experiment:** Pin launcher and application revisions; start with the working example, then add a single import or library initialization at a time. Capture the first unresolved symbol or error and the module and version expected by the build. Repeat with a clean launch and, if available, a second documented launcher revision without changing firmware.

**Deliverable:** A minimal reproducer and compatibility matrix of launcher, application and dependency revisions. A missing symbol may be a naming, export, version or access problem; a successful bootstrap does not prove the rest of the ABI works. This turns startup failures into actionable upstream reports.

### 21. Which firmware differences affect an existing client?

**Context:** Two independently available consoles or recorded test results with different firmware, each already able to run the same supported probe.

**Experiment:** Use the same source and comparable execution context to run a small suite of documented read-only queries, lifecycle checks and error cases from questions 7–9. Pin toolchain and test data. Compare results with published version notes and, where lawfully available, a source-level or binary comparison of the relevant component.

**Deliverable:** A firmware-difference table with exact environment and a minimal reproducer for each change. If access, title or launcher differs, record the confounder instead of assigning the change to firmware. A patch diff identifies a candidate cause; an independently reproduced behavior change supports a stronger conclusion.

## Reporting results

For every experiment publish the question, prerequisites, source revision, control, exact steps, raw outcome, repetitions and limitations. Record negative results and inaccessible interfaces as carefully as successes. For unexpected crashes, preserve the smallest reproducer and a sanitized log; report a suspected security issue to the affected project or vendor before publishing sensitive details. Do not publish console-unique secrets, personal saves or proprietary firmware contents.

## Technical details and open questions

The questions above deliberately specify observations rather than predicted outcomes. Availability of the required APIs, cross-process comparisons and reliable hardware timing remains unverified for each launcher and firmware. A useful next step is a small common test harness with machine-readable environment metadata and independently runnable probes; it does not yet exist in this wiki.

## Sources

- [ns2jb research](https://pepsipu.com/blog/2026-10-switch2-jailbreak-ns2jb/) — demonstrated execution context and filesystem/loader trust boundary.
- [SwitchBrew compatibility mode](https://switchbrew.org/wiki/Switch_2:_Compatibility_Mode) — observed compatibility modules and process changes.
- [SwitchBrew GSV services](https://switchbrew.org/wiki/Switch_2:_GSV_services) — documented graphics interface names and commands.
- [Eglantine NX2 notes](https://github.com/Eglantine-Community/Eglantine/blob/nx2/NX2_NOTES.md) — candidate emulator assumptions and their limitations.
- [HBLauncher](https://github.com/ravensclawx/HBLauncher) and [libnx-ounce](https://github.com/alula/libnx-ounce) — examples of distinct native homebrew projects to inspect and version.
- [Switch 2 controller research](https://github.com/ndeadly/switch2_controller_research) — controller protocol observations for comparison with runtime input.
