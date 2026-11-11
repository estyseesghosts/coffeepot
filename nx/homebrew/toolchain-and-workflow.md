# Switch homebrew toolchain and workflow

This page is for a game developer who wants to build and run homebrew on the Nintendo Switch family (**NX**). It works on any Switch that can already run unofficial hobbyist software (**[homebrew](../../glossary.md#homebrew)**) — in practice an unpatched 2017–2018 unit, or a patched / OLED unit with a modchip, on firmware supported by Atmosphere — and the outcome is a working build-run loop: compile a game on PC, copy one file to the SD card, and launch it from the homebrew menu.

If you only want to know which libraries to program against, read [libnx and deko3d](deko3d-and-libnx.md). If you have not set up the console side yet, read [Atmosphere and running](atmosphere-and-running.md).

## What you need

- A PC (Windows, Linux, or macOS) with devkitA64 installed via devkitPro pacman [documented] (devkitPro install docs and pacman package definitions).
- The `switch-dev` pacman group: `libnx`, `deko3d` + `dekotools`, and `switch-portlibs` for ports (SDL2, freetype, curl) [documented] (package list in devkitPro's `switch-dev` meta-package).
- A Switch able to launch the homebrew menu (hbmenu), plus an SD card. No Nintendo SDK, no devkit hardware.
- A text editor and `make`. The stock project template uses GNU Make; no IDE is required.

Install command (PC):

```sh
pacman -S switch-dev
```

This pulls the compiler, `libnx`, `deko3d`, tools, and example templates in one step.

## How it works

You write ordinary C / C++ against `libnx`, compile to an `.nro` file, and run that file from the SD card. There is no install step and no signing step for this path.

The high-level loop is:

1. Scaffold from the template (`templates/application` in the `switch-examples` repo) and edit code.
2. Run `make`. You get an `.nro` with its name and icon embedded.
3. Copy the `.nro` to `/switch/` on the SD card.
4. Launch hbmenu on the console and pick it. Print debug text back to the PC over the network with `nxlink`.

`.nro` (relocatable homebrew executable, run by hbmenu/hbloader with no install) is the normal dev format. `.nsp` (installable package handled by the `ns` system service, requiring signatures / modified system software (**[custom firmware](../../glossary.md#cfw)**)) is only needed when you want a home-screen icon or full product packaging; prefer `.nro` during development.

Two launch modes matter because they change how much memory your game gets:

- Album / applet mode: open hbmenu from the Album. Quick, but the system keeps most RAM for the suspended game. Your homebrew runs in a small applet budget.
- Title override: hold R while launching any installed title, so hbmenu takes over a full game slot instead. Your homebrew gets the full application memory budget (gigabyte-class). Use this as soon as your game loads real assets.

## Technical details

Build inputs and outputs [documented] (readable in `switch-examples/templates/application/Makefile` and `switch-tools` source):

| Item | What it is |
| ---- | ---------- |
| `devkitA64` | GCC/binutils targeting 64-bit **[ARM](../../glossary.md#arm)** (`aarch64-none-elf`). Cross-compiler that runs on PC and emits Switch code. |
| `libnx` | C library for Switch user programs. Wraps privileged kernel calls and system services. See [libnx and deko3d](deko3d-and-libnx.md). |
| `deko3d` + `dekotools` | Homebrew GPU API and its shader compiler. See [libnx and deko3d](deko3d-and-libnx.md). |
| `switch-portlibs` | Prebuilt ports (SDL2, zlib, freetype, curl, etc.) installed under `$DEVKITPRO/portlibs/switch`. |
| `templates/application` | Minimal project: `source/main.c`, `Makefile`, `icon.jpg`, metadata source. `make` embeds the name/icon into the `.nro`. |
| `.nro` | Nintendo relocatable object. Loadable anywhere in memory; contains code, data, embedded name/icon. Run by hbloader; never installed. |
| `.nsp` | Installable package. Installed and ticket-checked by system services; unsigned install requires patched install service under CFW. |

Dev-loop network logging [documented] (readable in `libnx` `nxlink` source and `switch-examples/network/nxlink`):

```sh
# PC side: build, then send and listen in one step
make && nxlink -s server.nro
```

`nxlink` serves the `.nro` over TCP and relays `printf` output back to the PC terminal. Useful when iterating without ejecting the SD card. The network setup on the console must already work; `nxlink` does not configure networking itself.

Memory-mode rule of thumb [documented] (readable in `hbmenu` / `hbloader` docs and Atmosphere `loader` source: applet memory budget vs. application memory budget):

- Applet (Album) hbmenu: small heap, large assets fail or fragment early. Good for menus and hello-world.
- Title override (hold-R game launch): full game-class heap and a 39-bit virtual address space for forwarders. Required before profiling asset budgets or GPU streaming.

## Limits and risks

- This page does not jailbreak the console. Without a working entry point (unpatched RCM unit, modchip, or software entry on exploitable firmware) there is nothing to run the `.nro` with. See [Atmosphere and running](atmosphere-and-running.md).
- Applet-mode out-of-memory crashes look like random aborts. If a demo works in the template but dies when art is added, retry under title override before debugging the renderer.
- `.nsp` distribution adds signing, install-service, and ban risk. Ship examples and test builds as `.nro`.
- Overclocking via CFW sysmodules does not change what `make` produces; it only changes console-side clocks. Do not bake overclock assumptions into asset budgets.

## Sources

- [devkitPro Getting Started](https://devkitpro.org/wiki/Getting_Started) — what `switch-dev` installs (`devkitA64`, `libnx`, `deko3d`).
- [switch-examples](https://github.com/switchbrew/switch-examples) — what `templates/application` covers (scaffold, `Makefile`, `.nro` with embedded name/icon) and the per-topic demo map.
- [libnx](https://github.com/switchbrew/libnx) — what the toolchain links against (startup, service wrappers, `nxlink`/`romfs` runtime support).
- [hbmenu](https://github.com/switchbrew/nx-hbmenu) — what Album vs. title-override launch covers (applet budget vs. full application memory).
- [switch-tools](https://github.com/switchbrew/switch-tools) — what network `printf` relay covers.
- [switch.homebrew.guide](https://switch.homebrew.guide/) — what console setup covers (linked, not duplicated here).
