# Switch homebrew toolchain and workflow

This page is for a game developer who wants to build and run homebrew on the Nintendo Switch family (**NX**). It assumes a console that can already launch unofficial hobbyist software (**[homebrew](../../glossary.md#homebrew)**) through the homebrew menu; getting there is covered by [Atmosphere and running](atmosphere-and-running.md) and the guide it links. The outcome is a working build-run loop: compile a program on a PC, send one file to the console, see its text output on the PC, and start over.

If you only want to know which libraries to program against, read [libnx and deko3d](deko3d-and-libnx.md).

## What you need

- A PC (Windows, Linux or macOS) with devkitA64 installed through devkitPro's `pacman` `[documented]` (SwitchBrew "Setting up Development Environment", oldid 8289: "run `sudo (dkp-)pacman -S switch-dev` to install the tools and libraries for switch development"; on Windows the graphical installer sets up an MSYS2 shell with `pacman`).
- `pacman -S switch-dev` installs a **package group**, not a single package. On the machine this page was checked on, the group contains `libnx`, `deko3d`, `uam`, `switch-tools` (which holds `nxlink`), `general-tools`, `catnip`, `switch-cmake`, `switch-pkg-config`, `switch-examples`, `devkita64-cmake`, `devkita64-binutils`, `devkitA64-gdb` and `devkit-env`; the compiler (`devkita64-gcc`) and C library (`devkita64-newlib`) come in as dependencies of `devkitA64` `[documented]` (`pacman -Sg switch-dev` and `pacman -Si devkitA64`, versions on [source versions](../engineering/source-versions.md)). Ports such as SDL2, freetype and curl are in separate groups, `switch-portlibs` and `switch-sdl2`; install them only when you need them.
- A console that can run the homebrew menu (hbmenu), an SD card, and for printf debugging a network connection that both the console and the PC share.
- A text editor and `make`. The stock template uses GNU Make; no IDE is required. Keep the project path free of spaces, and do not put it in a cloud-synced folder `[reported]` (old Homebrew Guide, `homebrew_dev/app_dev.rst`, 2022).

## Install by operating system

- **Windows:** use devkitPro's graphical installer and pick the Switch development option, then work in the devkitPro MSYS2 shell it installs. The installer's shell sets `DEVKITPRO`, `DEVKITA64` and `PATH` for you. In a plain PowerShell or Git Bash, `make` on a template Makefile stops with "Please set DEVKITPRO in your environment" until you set `DEVKITPRO` and `DEVKITA64` and put `devkitA64\bin`, `tools\bin` and the MSYS2 `usr\bin` on `PATH`; that is how the builds for this wiki were run [documented] for the installer (SwitchBrew "Setting up Development Environment"), observed here for the plain-shell failure.
- **Linux and macOS:** install devkitPro's `pacman` (the package is called `dkp-pacman` on some systems), then `sudo (dkp-)pacman -S switch-dev`, and log out and in again so the environment variables take effect [documented] (same SwitchBrew page). Linux x86_64 binaries also run under WSL.
- **Containers:** devkitPro also publishes Docker images with the toolchain and ports preinstalled [documented] (same page); this wiki has not used them.
- **Check the install** by building the template (above). If `make` fails before compiling, the environment variables are the usual cause; if it fails at link time with `undefined reference to sinf` or similar, add `-lm` to `LIBS`.

## Build the template

```sh
# in the devkitPro MSYS2 shell on Windows, or a shell with DEVKITPRO set on Linux/macOS
cp -r $DEVKITPRO/examples/switch/templates/application ~/projects/myswitchproject
cd ~/projects/myswitchproject
make
```

Result checked for this page: `make` compiled `source/main.c`, linked `myswitchproject.elf` and produced `myswitchproject.nacp` and `myswitchproject.nro` (about 166 KB) with no warnings, using devkitA64 r30-1 and `libnx` 4.12.0-1. Not run on a console.

The `.nro` is named after the folder because the template Makefile sets `TARGET := $(notdir $(CURDIR))` `[documented]` (`switch-examples` `templates/application/Makefile`). Rename the folder or edit that line to rename the output. The template Makefile uses `icon.jpg` or `<TARGET>.jpg` in the project folder if one exists and falls back to `libnx`'s default icon; it writes the program name, author and version from `APP_TITLE`, `APP_AUTHOR` and `APP_VERSION` into a `.nacp` metadata file and embeds it `[documented]` (same Makefile, comments at lines 20-30 and 129-132).

## Run it on the console

1. Copy the `.nro` to `/switch/` on the SD card (any folder works for hbmenu; `/switch/<name>/<name>.nro` is the usual layout).
2. Start the homebrew menu and pick it.
3. Press `+` to exit if your program handles it the way the template does (`if (kDown & HidNpadButton_Plus) break;`).

For a program that exercises input, drawing, audio, saves, logging and a crash dump, see [a first Switch game](../engineering/first-game.md); for what to do when it misbehaves, see [debugging](../engineering/debugging.md).

Two launch modes change how much memory the program gets: the Album applet versus title override. They are explained on [Atmosphere and running](atmosphere-and-running.md); start in title override as soon as your program loads real assets.

## Print debug text to the PC with nxlink

`nxlink` sends the `.nro` to the console over the network and then prints whatever the program writes to `stdout` and `stderr` on the PC. It needs three things: the console in NetLoader mode, a program that redirects its output, and the `.nro` file itself.

1. On the console, open the homebrew menu and press **Y** to enter NetLoader mode `[reported]` (old Homebrew Guide, `homebrew_dev/app_dev.rst` lines 180-181).
2. In the program, initialise sockets and redirect output once at startup:

```c
consoleInit(NULL);
const bool sockets = R_SUCCEEDED(socketInitializeDefault());   // sockets first,
const int nxlinkFd = sockets ? nxlinkStdio() : -1;             // then redirect stdout and stderr; negative means no host
printf("appears in the PC terminal when started with nxlink -s\n");
/* ... main loop; keep working when nxlinkFd < 0 ... */
if (nxlinkFd >= 0) close(nxlinkFd);                            // libnx: close the socket during cleanup
if (sockets) socketExit();
consoleExit(NULL);
```

3. On the PC, from the project folder after `make`:

```sh
nxlink -a <console-ip> -s myswitchproject.nro
```

Verification of the program fragment: compiles in `api-check`, `-Wall`, no warnings; not run on a console. `nxlinkStdio()` returns the socket descriptor on success and a negative number on failure, and the socket should be closed with `close()` during cleanup `[documented]` (`libnx` `nx/include/switch/runtime/nxlink.h`). The command line options come from `nxlink --help` of `switch-tools` 1.13.1: `-a`/`--address` is the console's hostname or IPv4 address, `-s`/`--server` starts the output server after the upload, `-p`/`--path` sets the upload path, `--args` sends arguments to the program, `-r` sets retries `[documented]`. If you omit `-a`, `nxlink` tries to find the console itself; the old guide says to pass `-a` when it cannot `[reported]`.

- The file you pass must be the `.nro`, not the `.elf`. The old guide reports a fatal error screen (2347-0005, 0xb5b) when something else is sent `[reported]`.
- The upstream demonstration is `network/nxlink_stdio` in `switch-examples`: it calls `socketInitializeDefault()` then `nxlinkStdio()`, and its header comment gives the command `nxlink <-a switch_ip> nxlink_stdio.nro -s <arguments>` `[documented]` (`switch-examples` `network/nxlink_stdio/source/main.c`). The old wiki text `nxlink -s server.nro` used a file name your project will not produce.
- `nxlinkStdio()` sets its socket to non-blocking mode, so a stalled PC does not by itself freeze your frame `[documented]` (`libnx` `nx/source/runtime/nxlink_stdio.c`). The default client port is 28771 and the server port 28280 (`libnx` `nx/include/switch/runtime/nxlink.h`), which matters if a firewall blocks them.
- How it fails: no output appears if `socketInitializeDefault()` was not called first, if the console is not in NetLoader mode, or if the PC firewall blocks the port.

> Unknown: how `nxlink` finds the console without `-a`. The `switch-tools` source was not among the saved sources and no discovery test was run.

## Technical details

| Item | What it is | Source |
| ---- | ---------- | ------ |
| `devkitA64` | GCC and binutils targeting 64-bit **[ARM](../../glossary.md#arm)** (`aarch64-none-elf`), cross-compiler that runs on the PC | installed `devkita64-gcc` 16.1.0 |
| `libnx` | C library for Switch user programs: startup code, kernel wrappers, service clients. See [libnx and deko3d](deko3d-and-libnx.md) | `libnx` source |
| `deko3d`, `uam` | GPU API and its offline shader compiler | [libnx and deko3d](deko3d-and-libnx.md) |
| `switch-tools` | `elf2nro`, `nacptool`, `build_romfs`, `nxlink` and related packaging tools | installed `switch-tools` 1.13.1 |
| `switch-portlibs`, `switch-sdl2` | Ports such as SDL2, installed under `$DEVKITPRO/portlibs/switch` | `pacman -Sg` |
| `.nro` | Nintendo relocatable object, the homebrew executable format: loadable at any address, with optional embedded name, author and icon; run by the homebrew loader and never installed | `libnx` `nx/include/switch/nro.h`, `switch-examples` Makefile |
| `.nsp` | Installable package handled by system services; installing an unsigned one needs patched system software | [Atmosphere and running](atmosphere-and-running.md) |

The Makefile flags used for the build checks on this wiki are the template's (`-g -Wall -O2 -ffunction-sections`, linked with `-lnx`) `[documented]` (`deko_basic` Makefile in `switch-examples`; the `deko_basic` build links `-ldeko3dd -lnx`).

For virtual-address-space notes (including why one page says 38-bit and another 39-bit) see [NX memory](../hardware/memory.md#address-space-widths).

## Limits and risks

- This page does not get a console into a state where it can run homebrew. See [Atmosphere and running](atmosphere-and-running.md).
- Applet-mode out-of-memory crashes can look like random aborts. If a demo works in the template but dies when assets are added, retry under title override before debugging the renderer.
- Distributing a `.nsp` adds signing, installation and ban risk (see [Atmosphere and running](atmosphere-and-running.md)). Ship examples and test builds as `.nro`.
- An overclocking sysmodule does not change what `make` produces; it changes console clocks. Do not bake overclock assumptions into asset budgets.

## Sources

- SwitchBrew [Setting up Development Environment](https://switchbrew.org/wiki/Setting_up_Development_Environment) (oldid 8289, edited 2019-11-10) — `pacman -S switch-dev`, template location, `TARGET` naming; older than the other sources, so installer details may have changed.
- [switch-examples](https://github.com/switchbrew/switch-examples) at `669786898205b7beb25ff1731e72982e6d0397d3` — `templates/application/Makefile` and `source/main.c`, `network/nxlink_stdio/source/main.c`.
- [libnx](https://github.com/switchbrew/libnx) at `b156b4774fb2df7b93402169509266da7547fc4c` — `nx/include/switch/runtime/nxlink.h`, `nx/source/runtime/nxlink_stdio.c`, `nx/include/switch/nro.h`.
- [switch-tools](https://github.com/switchbrew/switch-tools) — `nxlink`, `nacptool`, `elf2nro`; behaviour read from the installed `switch-tools` 1.13.1 `--help` output (source not read).
- [noahc3 Homebrew Guide](https://github.com/noahc3/Homebrew-Guide) at `6d7b7bc` (2022-08-02, archived guide) — `homebrew_dev/app_dev.rst`: NetLoader (Y button), `-a` option, fatal error when the wrong file type is sent. Old and unmaintained; confirm on current hbmenu.
- [switch.homebrew.guide](https://switch.homebrew.guide/) — current console setup (linked, not duplicated here).
