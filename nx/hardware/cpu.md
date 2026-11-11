# NX CPU

The Switch does all its thinking on one NVIDIA chip that holds both the main processor and the graphics. Games get three processor cores to themselves, while the system keeps one core for background work. The same game processor speed is used handheld and docked, so game logic timing stays consistent across modes. Two hardware revisions exist — the original chip and a smaller, cooler revision in later models — but games see the same four cores either way.

## Where it fits

The processor lives on the main chip that combines processor, graphics, memory interfaces and peripherals (a **system-on-a-chip**, or **[SoC](../../glossary.md#soc)**), the NVIDIA **[Tegra X1](../glossary.md#tegra-x1)**. It runs the **[kernel](../../glossary.md#kernel)** of Nintendo's **[Horizon](../../glossary.md#horizon)** **[operating system](../../glossary.md#os)** (see [NX OS](../software/os.md)), which starts via [firmware](../software/firmware.md) and schedules game and system code. It reads game data from shared [memory](memory.md) and hands drawing work to the [GPU](gpu.md).

## How it works

Game code runs as ordinary programs with their own private memory (each a **[process](../../glossary.md#process)**) split into sequences of instructions (each a **[thread](../../glossary.md#thread)**) spread over three cores. The programs are 64-bit **[ARM](../../glossary.md#arm)** code for four **[Cortex-A57](../glossary.md#cortex-a57)** cores, but only cores 0–2 run game threads while core 3 is reserved for the system [reverse-engineered] (SwitchBrew kernel and libnx core-mask behavior). Clocks do not change between handheld and docked for game code, so a frame at 60 fps always has the same per-core cycle budget. A short high-speed boost mode exists for loading, not for sustained gameplay.

## Technical details

| Item | Value | Evidence |
| ---- | ----- | -------- |
| **SoC**, or main combined chip | Tegra X1 (T210) | [documented] (NVIDIA Tegra X1 materials) |
| Revision, original models | Erista, 20 nm, ODNX02-A2 | [documented] (NVIDIA; SwitchBrew board data) |
| Revision, later models | Mariko, 16 nm, ODNX10-A1 | [documented] (NVIDIA; SwitchBrew board data) |
| Model boards using Mariko | Lite VALI / HDH-CPU-02; OLED FRIC / HEG-CPU-01; Mariko refresh MODIN / HAD-CPU-10 | [reverse-engineered] (SwitchBrew board revisions) |
| Main cores used | 4x Cortex-A57 "Atlas" | [documented] (NVIDIA Tegra X1 TRM; ARM Cortex-A57 TRM) |
| Companion cores, unused | 4x Cortex-A53 "Apollo" present on silicon, not used by Switch software | [reverse-engineered] (SwitchBrew Tegra X1 notes) |

| **[Cache](../../glossary.md#cache)** level | Size / organization |
| ---- | ----- |
| L1 instruction, per core | 48 KB |
| L1 data, per core | 32 KB |
| L2, shared by all four A57 cores | 2 MB, 16-way, 64-byte lines |

Cache sizes are [documented] (ARM Cortex-A57 TRM).

| Clock target | Frequency | Notes |
| ---- | ----- | ----- |
| Game **[CPU](../../glossary.md#cpu)** clock, handheld and docked | 1020 MHz | [reported] (SwitchBrew Clocks page; homebrew clock dumps; same value both modes as stated) |
| Boost clock | 1785 MHz | [reported] (SwitchBrew Clocks page; load-only boost, not sustained gameplay) |

Core assignment:

| Core | Use |
| ---- | --- |
| 0–2 | Game threads |
| 3 | System services and kernel background work |

Assignment is [reverse-engineered] (SwitchBrew kernel scheduling notes; libnx thread core-mask defaults in `thread.c`).

Frame budget: at the 1020 MHz game clock, one 60 fps frame is ~17M cycles per core (1020M ÷ 60), shared by all threads on that core. This is arithmetic from the [reported] clock above, not a new measurement — actual game throughput is lower because of memory waits and system activity.

What is not covered here: privilege levels and the secure monitor, and virtual-memory layout, belong with [NX OS](../software/os.md) and [NX memory](memory.md) and are only summarized here.

## Homebrew notes

Use libnx thread calls to create threads and pin time-critical threads to cores 0–2; do not assume core 3 is free, and do not assume the boost clock is available during gameplay. Clocks are set by the system power service, not by writing chip registers directly.

## Sources

- [SwitchBrew — Tegra X1](https://switchbrew.org/wiki/Tegra_X1) — SoC revisions, Erista/Mariko part numbers, unused A53 cluster.
- [SwitchBrew — Clocks](https://switchbrew.org/wiki/Clocks) — 1020 MHz game CPU clock handheld+docked, 1785 MHz load-only boost.
- [NVIDIA Tegra X1 Technical Reference / datasheet](https://developer.nvidia.com/tegra-x1) — T210 SoC composition, A57+A53 clusters, process node.
- [ARM Cortex-A57 Technical Reference Manual](https://developer.arm.com/Processors/Cortex-A57) — 48 KB I / 32 KB D L1, 2 MB 16-way L2 with 64-byte lines.
- [libnx — thread and core-mask API](https://github.com/switchbrew/libnx) — how homebrew creates threads and which cores games may use (`thread.c`, core-mask behavior).
