# NX CPU

The Switch does all its thinking on one NVIDIA chip that holds both the main processor and the graphics. Games get three processor cores to themselves, while the system keeps one core for background work. The same game processor speed is used handheld and docked, so game logic timing stays consistent across modes. Two hardware revisions exist — the original chip and a smaller, cooler revision in later models — but games see the same four cores either way.

## Where it fits

The processor lives on the main chip that combines processor, graphics, memory interfaces and peripherals (a **system-on-a-chip**, or **[SoC](../../glossary.md#soc)**), the NVIDIA **[Tegra X1](../glossary.md#tegra-x1)**. It runs the **[kernel](../../glossary.md#kernel)** of Nintendo's **[Horizon](../../glossary.md#horizon)** **[operating system](../../glossary.md#os)** (see [NX OS](../software/os.md)), which starts via [firmware](../software/firmware.md) and schedules game and system code. It reads game data from shared [memory](memory.md) and hands drawing work to the [GPU](gpu.md).

## How it works

Game code runs as ordinary programs with their own private memory (each a **[process](../../glossary.md#process)**) split into sequences of instructions (each a **[thread](../../glossary.md#thread)**) spread over three cores. The programs are 64-bit **[ARM](../../glossary.md#arm)** code for four **[Cortex-A57](../glossary.md#cortex-a57)** cores, but ports report that a game gets three cores, with the fourth kept for the system [reported] (`nfsmw-nx` `docs/platform-notes.md`; `sa2-nx` `docs/porting-guide.md`: "three cores"). The stock CPU clock is the same handheld and docked in the port documentation, so a frame at 60 fps has the same per-core cycle budget in both modes [reported] (`nfsmw-nx` `README.md` stock-clock table). Overclocked runs of 1785 MHz appear in ports as a user-chosen setting, not as a stock mode.

## Technical details

| Item | Value | Evidence |
| ---- | ----- | -------- |
| **SoC**, or main combined chip | Tegra X1 (T210) | [documented] (NVIDIA Tegra X1 materials) |
| Revision, original models | Erista (Tegra X1), 20 nm, ODNX02-A2 | [documented] (SwitchBrew Tegra X1 and Hardware, oldid 14745 and 15339) |
| Revision, later models | Mariko (Tegra X1+), 16 nm, ODNX10-A1 | [documented] (SwitchBrew Tegra X1 and Hardware) |
| Model boards using Mariko | Retail board codes HDH-CPU-02 (Lite), HEG-CPU-01 (OLED), HAD-CPU-10 (Mariko refresh of the original); pre-release codes VALI-, FRIC- and MODIN-CPU-X5/X9 | [documented] (SwitchBrew Hardware, oldid 15339) |
| Main cores | 4x Cortex-A57 "Atlas" | [documented] (SwitchBrew Tegra X1) |
| Companion cores | 4x Cortex-A53 "Apollo" are listed in the SoC; 512 KB shared L2, 32 KB/32 KB L1 per core | [documented] (SwitchBrew Tegra X1) |

| **[Cache](../../glossary.md#cache)** level | Size / organization |
| ---- | ----- |
| L1 instruction, per core | 48 KB |
| L1 data, per core | 32 KB |
| L2, shared by all four A57 cores | 2 MB |

Cache sizes are [documented] (SwitchBrew Tegra X1). Associativity and line size were not in the saved sources.

> Unknown: whether Switch software uses the Cortex-A53 cluster. SwitchBrew lists the cluster as part of the SoC; no saved source says it is unused.

| Clock target | Frequency | Notes |
| ---- | ---- | ----- |
| Game **[CPU](../../glossary.md#cpu)** clock, handheld and docked | 1020 MHz | [reported] (`nfsmw-nx` `README.md` and `docs/platform-notes.md`, commit `20f8dc1`; the same stock value is used as the test clock in `sa2-nx`, `usf4-nx` and `SwitchWakerHD`) |
| Overclock used by ports | 1785 MHz | [reported] (`usf4-nx` `README.md`: fights hold 60 fps at CPU 1785 / GPU 768 MHz; set with a clock manager). A user setting, not a stock mode. |

> Unknown: the SwitchBrew Clocks page, which older text cited for these values and for a "load-only boost" mode, was not among the saved sources. No saved source documents a boost mode.

Core assignment:

| Core | Use |
| ---- | --- |
| 0–2 | Game threads |
| 3 | System services and kernel background work |

Assignment is [reported] (ports as above). Only thread priority `0x3B` is time-sliced on a core, with a 10 ms slice, per one port's notes [reported] (`sa2-nx` `docs/porting-guide.md`).

Frame budget: at the 1020 MHz game clock, one 60 fps frame is ~17M cycles per core (1020M ÷ 60), shared by all threads on that core. This is arithmetic from the reported clock above, not a new measurement — actual game throughput is lower because of memory waits and system activity.

What is not covered here: privilege levels and the secure monitor, and virtual-memory layout, belong with [NX OS](../software/os.md) and [NX memory](memory.md) and are only summarized here.

## Homebrew notes

Use libnx thread calls to create threads and set an explicit core mask for each; do not assume the fourth core is free. See [performance and porting](../engineering/performance-and-porting.md) for how ports laid threads out. Clocks are set by the system power service, not by writing chip registers directly.

## Sources

- SwitchBrew [Tegra X1](https://switchbrew.org/wiki/Tegra_X1) (oldid 14745) — SoC revisions, process nodes, A57 and A53 clusters and caches.
- SwitchBrew [Hardware](https://switchbrew.org/wiki/Hardware) (oldid 15339) — SoC part numbers and board codes per model.
- [nfsmw-nx](https://github.com/StevensND/nfsmw-nx) at `20f8dc1` — stock-clock table and three usable cores `[reported]`.
- [sa2-nx](https://github.com/ChanseyIsTheBest/sa2-nx) at `baf523b`, [usf4-nx](https://github.com/Thorhax/usf4-nx) at `30ffae0` — scheduling notes and overclock settings `[reported]`.
