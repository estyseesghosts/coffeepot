# NX models and revisions

The Switch family shares one software platform across four retail shapes: the original Switch, the Switch Lite (handheld-only), the OLED model, and the Mariko-revision original. Games see the same four processor cores and the same graphics family on all of them; what changes is screen, dock support, battery, storage size, and the chip revision inside.

The platform name **NX** covers the whole family (see [NX overview](../index.md)). The chip inside is the NVIDIA **[Tegra X1](../glossary.md#tegra-x1)** **[system-on-a-chip](../../glossary.md#soc)** in two silicon revisions: Erista (20 nm) and Mariko (16 nm).

## Where it fits

All models run the same [Horizon OS](../software/os.md) started by the same [firmware](../software/firmware.md) chain, and all expose the same [CPU](cpu.md) / [GPU](gpu.md) / [memory](memory.md) programming model. Model differences matter for [input and display](input-and-display.md) (which screen, docked or not), battery testing, and for homebrew entry points (see [Atmosphere and running](../homebrew/atmosphere-and-running.md)): early Erista units allow the RCM USB entry, later units do not.

## How it works

A game picks handheld vs. docked behavior at runtime from what the OS reports, not from which retail name is on the box. Handheld-only models do not report docked; dockable models switch when the dock is connected or removed. The OS also reports the display size so the game can reallocate its pictures (each a **[framebuffer](../../glossary.md#framebuffer)**) on mode change.

## Technical details

| Model | Retail board | SoC | Storage | Memory | Notes |
| ---- | ------------ | --- | ------- | ------ | ----- |
| Original Switch (2017) | HAC-CPU-01, HAC-CPU-10, HAC-CPU-20 | Erista ODNX02-A2 (Tegra X1, 20 nm) | 32 GB eMMC | 2 × 2 GB LPDDR4 | Dockable. Some early units can be entered through RCM without a modchip (see below). |
| Original Switch, Mariko revision (2019) | HAD-CPU-10 | Mariko ODNX10-A1 (Tegra X1+, 16 nm) | 32 GB eMMC | 2 × 2 GB LPDDR4X | Dockable. Same board-code family as the original (one letter different). |
| Switch Lite (2019) | HDH-CPU-02 | Mariko ODNX10-A1 | 32 GB eMMC | 2 × 2 GB LPDDR4X | Handheld-only. |
| OLED model (2021) | HEG-CPU-01 | Mariko ODNX10-A1 | 64 GB eMMC | 2 × 2 GB LPDDR4X | Different screen panel; other differences were not checked. |

All rows except the RCM note are [reverse-engineered] (SwitchBrew Hardware, "Motherboard" and "Specifications", oldid 15339, edited 2026-10-29). Pre-release board codes (ODIN-, MODIN-, VALI-, FRIC-CPU-X5/X9) also appear on that page; retail units carry the codes above. The RCM note is [reported] (old Homebrew Guide, `gettingstarted/checkingrcm.rst`, 2022). The retail model numbers (HAC-001 and so on), launch years, battery capacities and dock features that earlier text gave were not in the saved sources and are not repeated here.

Other hardware (Wi-Fi/Bluetooth Broadcom BCM4356, power ICs MAX77620 with MAX77621 on the original and MAX77812 on Mariko boards, audio Realtek ALC5639) is board detail, not game behavior, and lives on SwitchBrew [reverse-engineered] (SwitchBrew Hardware).

What differs for developers:

| Area | Effect |
| ---- | ------ |
| Clocks | The port documentation uses the same stock CPU and GPU clocks regardless of model [reported] (`nfsmw-nx`); whether Erista and Mariko sustain them equally under load is unmeasured. |
| Display | The Lite has no dock output; the dockable models switch between handheld and docked at run time. Query the mode with `appletGetOperationMode()`, do not assume. |
| Entry points | Early Erista units: USB RCM payload with no modification. Later Erista, Mariko, Lite and OLED: a modchip or another entry point. Which serial numbers are vulnerable is on the guide linked from [Atmosphere and running](../homebrew/atmosphere-and-running.md) and was not re-checked. |
| Fuses / downgrade | The anti-downgrade fuse mechanism is described on the [firmware](../software/firmware.md) page; whether it is identical on every model was not checked. |

> Unknown: per-model power/thermal throttling curves under sustained load. Until measured, test long sessions on both Erista and Mariko hardware.

## Homebrew notes

Test handheld and docked paths even if you own only one model: force the other layout in code (small framebuffer + integer upscale vs. full-HD target) so the game does not assume a fixed screen. For entry-point questions, match the serial/board against the guide linked from [Atmosphere and running](../homebrew/atmosphere-and-running.md) rather than guessing from the retail name.

## Sources

- SwitchBrew [Hardware](https://switchbrew.org/wiki/Hardware) (oldid 15339, edited 2026-10-29) — board codes, SoC part numbers, storage and memory per model.
- SwitchBrew [Tegra X1](https://switchbrew.org/wiki/Tegra_X1) (oldid 14745) — 20 nm Tegra X1 versus 16 nm X1+.
- [noahc3 Homebrew Guide](https://github.com/noahc3/Homebrew-Guide) at `6d7b7bc` (2022, archived) — RCM vulnerability check; current serial ranges are on [switch.homebrew.guide](https://switch.homebrew.guide/).
- [nfsmw-nx](https://github.com/StevensND/nfsmw-nx) at `20f8dc1` — stock clocks `[reported]`.
