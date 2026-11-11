# NX models and revisions

The Switch family shares one software platform across four retail shapes: the original Switch, the Switch Lite (handheld-only), the OLED model, and the revised-battery original. Games see the same four processor cores and the same graphics family on all of them; what changes is screen, dock support, battery, storage size, and the chip revision inside.

The platform name **NX** covers the whole family (see [NX overview](../index.md)). The chip inside is the NVIDIA **[Tegra X1](../glossary.md#tegra-x1)** **[system-on-a-chip](../../glossary.md#soc)** in two silicon revisions: Erista (20 nm) and Mariko (16 nm).

## Where it fits

All models run the same [Horizon OS](../software/os.md) started by the same [firmware](../software/firmware.md) chain, and all expose the same [CPU](cpu.md) / [GPU](gpu.md) / [memory](memory.md) programming model. Model differences matter for [input and display](input-and-display.md) (which screen, docked or not), battery testing, and for homebrew entry points (see [Atmosphere and running](../homebrew/atmosphere-and-running.md)): early Erista units allow the RCM USB entry, later units do not.

## How it works

A game picks handheld vs. docked behavior at runtime from what the OS reports, not from which retail name is on the box. Handheld-only models never report docked; dockable models switch when the dock negotiates. The OS also reports the display size so the game can reallocate its pictures (each a **[framebuffer](../../glossary.md#framebuffer)**) on mode change.

## Technical details

| Model | Board | SoC revision | Storage | Notes | Evidence |
| ---- | ----- | ------------ | ------- | ----- | -------- |
| Original Switch (2017, HAC-001) | ODIN / HAC-CPU-01/10/20 | Erista ODNX02-A2, 20 nm | 32 GB eMMC | Dockable, RCM-exploitable early serials | [reverse-engineered] (SwitchBrew hardware/board pages) |
| Switch refresh (2019, HAC-001-01) | MODIN / HAD-CPU-10 | Mariko ODNX10-A1, 16 nm | 32 GB eMMC | Dockable, better battery, RCM fixed | [reverse-engineered] (SwitchBrew board pages) |
| Switch Lite (2019, HDH-001) | VALI / HDH-CPU-02 | Mariko, 16 nm | 32 GB eMMC | Handheld-only, no dock, built-in controls | [reverse-engineered] (SwitchBrew hardware pages) |
| OLED model (2021, HEG-001) | FRIC / HEG-CPU-01 | Mariko, 16 nm | 64 GB eMMC | Larger OLED screen, dock with LAN, 64 GB | [reverse-engineered] (SwitchBrew hardware pages) |

Other hardware (Wi-Fi/BT Broadcom BCM4356, power MAX77620/77621, audio Realtek ALC5639) is board detail, not game behavior, and lives on SwitchBrew — not repeated here.

What differs for developers:

| Area | Effect |
| ---- | ------ |
| Clocks | Same [CPU](cpu.md) / [GPU](gpu.md) targets across models; battery life differs, performance targets do not. |
| Display | Lite: handheld only. Others: handheld + docked TV via HDMI. Query the mode, do not assume. |
| Entry points | Unpatched Erista: USB RCM payload with no mod. Patched / Mariko / Lite / OLED: modchip. See [Atmosphere and running](../homebrew/atmosphere-and-running.md). |
| Fuses / downgrade | Same fuse anti-downgrade mechanism on all models; see [firmware](../software/firmware.md). |

> Unknown: per-model power/thermal throttling curves under sustained load. Until measured, test long sessions on both Erista and Mariko hardware.

## Homebrew notes

Test handheld and docked paths even if you own only one model: force the other layout in code (small framebuffer + integer upscale vs. full-HD target) so the game does not assume a fixed screen. For entry-point questions, match the serial/board against the guide linked from [Atmosphere and running](../homebrew/atmosphere-and-running.md) rather than guessing from the retail name.

## Sources

- [SwitchBrew — Hardware](https://switchbrew.org/wiki/Hardware) — boards ODIN/MODIN/VALI/FRIC, Erista/Mariko revisions, storage sizes.
- [SwitchBrew — Tegra X1](https://switchbrew.org/wiki/Tegra_X1) — ODNX02-A2 vs ODNX10-A1, 20 nm vs 16 nm.
- [Hekate / Atmosphere docs](https://github.com/CTCaer/hekate) — which models allow RCM entry vs modchip.
