# Benchmark plan

This page specifies what a Switch 2 benchmark suite should measure and how. It exists so that results, when they arrive, can be compared and so that nobody has to guess what a number meant. Nothing in this plan has been built or run.

The reasoning behind the measurements is on the [graphics and performance page](graphics-and-performance.md); the CPU and memory constraints are on the [CPU and memory page](../hardware/cpu-and-memory.md).

## Before you start

A Switch 2 benchmark needs a suitable execution environment; a native benchmark in particular needs a native-code path. As of November 12, 2026 that means a game sandbox, or a privileged system-module session on firmware 23.0.1 that firmware 23.5.0 patched as a system escape. Any benchmark suite therefore inherits the limitations of its execution environment, and a result is only meaningful alongside the environment that produced it.

Record for every run:

| Field | Why it matters |
| ----- | -------------- |
| Console and board revision | Hardware varies between units |
| Firmware version | Service behaviour and protections change between versions |
| Execution context | Game sandbox, privileged sysmodule or emulator |
| Launcher and revision | The runtime's own overhead is part of the measurement |
| Build and compiler | Optimisation level and flags move numbers |
| Mode | Handheld or docked can change operating conditions and presentation cost |
| Repetitions, warmup, raw results | So the result can be re-derived |

Emulator timings are never results. If a suite runs in an emulator, it is a correctness check, and the README must say so.

## CPU

| Test | Question it answers |
| ---- | ------------------- |
| Cache-sensitive access | What does the L2/L3 hierarchy cost at each level, given 256 KB L2 per core and 4 MB shared L3? |
| Pointer chasing | What is the latency of a dependent load, and how does it compare with the original Switch's A57? |
| Atomics under contention | What does the LSE atomics path cost compared with a load-exclusive/store-exclusive loop, and where is the crossover? |
| Worker dispatch | What is the cost and scaling of dispatching more workers than available cores? |
| Memory bandwidth | How much of the published 102 GB/s peak is reachable from one process, and how does that split across cores? |

Record the process affinity and number of runnable workers. A request to use a forbidden core may fail; oversubscribing workers on permitted cores may instead cause contention.

## GPU

| Test | Question it answers |
| ---- | ------------------- |
| Upload path | What does it cost to get data into GPU-visible memory, including the attribute and lock steps? |
| Command submission | What does a draw call cost, and how does the cost change with draw count? |
| Synchronization | What does waiting on a [syncpoint](../glossary.md#syncpoint) cost, and how does it scale with queue depth? |
| Compute versus graphics contention | Do the compute and graphics paths interfere? |
| Render-target bandwidth | What does the GPU achieve against its own memory, separately from the display? |
| Presentation latency | How much time sits between a finished frame and the display? |

## Storage

| Test | Question it answers |
| ---- | ------------------- |
| Filesystem overhead | How much of a file operation is filesystem work rather than media work? |
| UFS throughput | What does the internal flash sustain, separately from the removable card? |
| microSD Express throughput | What does the removable card sustain? |

The distinction between filesystem overhead and media throughput matters because the two have different costs on every console generation and are easy to confuse.

## Method

Every benchmark needs a defined question, controlled variables, a warmup procedure, a number of repetitions, raw measurements and an explicit execution environment. Beyond that:

- Report median and tail times, not only averages. Averages hide the frame that hitched.
- Change one variable at a time when attributing a result to a cause.
- Compare images, not just frame counts, when the workload involves graphics.
- Retain the raw data. A result that cannot be re-derived is an anecdote.

## Open questions

- Which GPU timers and hardware performance counters, if any, are exposed to a native program on this console?
- Which access restrictions and synchronization overheads, if any, affect a workload?
- How do worker count, actual affinity and sustained power conditions affect scaling?

> Unknown: Coffeepot has not run this suite on Switch 2. Published third-party results must be evaluated against their own workloads and conditions.

## Sources

- [SwitchBrew T239](https://switchbrew.org/wiki/Switch_2:_Tegra_T239) — the cache and memory figures the CPU tests are built around.
- [SwitchBrew GSV services](https://switchbrew.org/wiki/Switch_2:_GSV_services) — the graphics service interface the GPU tests would use.
- [SwitchBrew T23X firewalls](https://switchbrew.org/wiki/Switch_2:_T23X_Firewalls) — the per-GPU rules that might affect GPU throughput.
- [Eglantine NX2 notes](https://github.com/Eglantine-Community/Eglantine/blob/nx2/NX2_NOTES.md) — the closest thing to observed GPU behaviour, from an emulator.
