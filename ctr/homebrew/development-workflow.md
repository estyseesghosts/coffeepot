# Nintendo 3DS homebrew: setup and development workflow

This is a start-to-finish guide to building a **`.3dsx` homebrew application**. It covers installing devkitPro on Windows, macOS and Linux, building with its official Docker image, testing in an emulator or on a real 3DS, and fixing common setup problems.

**Fast path:** install the **`3ds-dev`** package group, build an example with `make`, then launch the resulting `.3dsx` in the Homebrew Menu or an emulator. You do **not** need to build the compiler, libctru, or the 3DS SDK from source.

## 1. Pick your environment

| Your computer | Recommended setup | Where you run `make` |
| --- | --- | --- |
| Windows 10/11 | [devkitPro Windows installer](#windows-native-devkitpro-msys2) | **devkitPro MSYS2** terminal |
| macOS (Intel or Apple Silicon) | [devkitPro pacman `.pkg`](#macos) | Terminal |
| Ubuntu/Debian (including WSL2) | [devkitPro apt setup script](#ubuntu-debian-and-wsl2) | Linux shell |
| Other Linux distributions | [devkitPro pacman instructions](#other-linux-distributions) | Linux shell |
| Any host with Docker | [Official `devkitpro/devkitarm` image](#docker-windows-macos-or-linux) | Inside a container |

The examples below use C and GNU Make because they are the shortest supported way to get started. C++ works too. You may use an editor or IDE of your choice; it does not replace the cross-compiler.

### The tools, in one paragraph

**devkitPro** distributes the packages. **devkitARM** is the ARM cross-compiler; **libctru** provides 3DS operating-system services. **citro3d** and **citro2d** handle 3D and 2D rendering. **picasso** assembles PICA200 shaders. **3dsxtool** creates `.3dsx` executables, and **3dslink** sends them to a console over Wi-Fi. Installing **`3ds-dev`** supplies the standard development environment. You only need to install extra `3ds-*` libraries for projects that require them.

## 2. Install devkitPro

Choose **one** of the following paths. After installing, follow the [verification](#3-check-that-the-toolchain-works) and [first build](#4-build-your-first-3dsx) sections.

### Windows: native devkitPro (MSYS2)

1. Download the **current Windows installer** from [devkitPro/installer releases](https://github.com/devkitPro/installer/releases/latest).
2. Run it and select **3DS Development**. Leave **Minimal System/MSYS2** selected. The default installation location is `C:\devkitPro`.
3. Open **Start → devkitPro → MSYS2** (sometimes labelled **MSYS**). Use this shell for the commands in this guide, **not** a plain PowerShell or Command Prompt window.
4. Update the packages if needed:

   ```sh
   pacman -Syu
   ```

   If the update asks you to close the shell, reopen **devkitPro MSYS2** and run `pacman -Syu` again.

5. Ensure the complete 3DS package group is installed:

   ```sh
   pacman -S --needed 3ds-dev
   ```

**Check the environment** in that same MSYS2 shell:

```sh
echo "$DEVKITPRO"
echo "$DEVKITARM"
"$DEVKITARM/bin/arm-none-eabi-gcc" --version
```

The shell usually exposes devkitPro under `/opt/devkitpro`, even though the Windows installation is in `C:\devkitPro`. Those paths are mapped by MSYS2; do not change them to Windows-style paths inside a Makefile.

If you prefer WSL2, use the **Ubuntu/Debian** instructions instead. Do not mix the Windows devkitPro installation with WSL's Linux toolchain in one build.

### macOS

1. Install Apple's command-line tools if necessary (provides `make` and `git`):

   ```sh
   xcode-select --install
   ```

2. Download the **macOS `.pkg`** from [devkitPro/pacman releases](https://github.com/devkitPro/pacman/releases/latest). Open it and complete the installer. This installs devkitPro's package manager; it does not by itself install all the 3DS libraries.
3. Install the 3DS packages:

   ```sh
   sudo dkp-pacman -Syu
   sudo dkp-pacman -S --needed 3ds-dev
   ```

4. Make the compiler and helper tools visible to your shell. For the default **zsh**, add these lines to `~/.zshrc` (only once):

   ```sh
   export DEVKITPRO=/opt/devkitpro
   export DEVKITARM="$DEVKITPRO/devkitARM"
   export PATH="$DEVKITPRO/tools/bin:$DEVKITARM/bin:$PATH"
   ```

   Then reload it:

   ```sh
   source ~/.zshrc
   ```

   If you use bash instead, put the same exports in the startup file your bash sessions read (typically `~/.bashrc` or `~/.bash_profile`).

**Apple Silicon:** the current devkitPro pacman releases support macOS on Apple Silicon. Use the macOS installer; Docker is not required just because you have an ARM Mac.

### Ubuntu, Debian and WSL2

1. Install the host utilities:

   ```sh
   sudo apt update
   sudo apt install -y ca-certificates wget git make
   ```

2. Download devkitPro's **official apt-repository bootstrap script**:

   ```sh
   wget https://apt.devkitpro.org/install-devkitpro-pacman
   less install-devkitpro-pacman
   chmod +x install-devkitpro-pacman
   sudo ./install-devkitpro-pacman
   ```

   The `less` step lets you inspect the script before executing it as root. This script sets up devkitPro's package manager and apt repository; it is not the 3DS SDK itself.

3. Install the toolchain:

   ```sh
   sudo dkp-pacman -Syu
   sudo dkp-pacman -S --needed 3ds-dev
   ```

4. Open a new terminal. If `DEVKITPRO` or `DEVKITARM` is still empty, add these to your shell's startup file (for ordinary Ubuntu bash, `~/.bashrc`), then `source ~/.bashrc`:

   ```sh
   export DEVKITPRO=/opt/devkitpro
   export DEVKITARM="$DEVKITPRO/devkitARM"
   export PATH="$DEVKITPRO/tools/bin:$DEVKITARM/bin:$PATH"
   ```

**WSL2 note:** WSL uses the Linux toolchain, not the Windows installer. If the bootstrap script reports an `/etc/mtab` issue, consult the [devkitPro pacman release instructions](https://github.com/devkitPro/pacman/releases/latest) for the documented WSL-specific symlink fix. Keep the source tree in your Linux home directory for more predictable build performance.

### Other Linux distributions

Follow devkitPro's [pacman setup guide](https://devkitpro.org/wiki/devkitPro_pacman) for Arch, Fedora, Gentoo or your distribution. Configure **devkitPro's repositories**, not an unrelated compiler package of the same name. Then install `3ds-dev` using the configured package manager (`pacman` on distributions using native pacman; `dkp-pacman` where devkitPro provides its own).

If your distro's package-manager integration is unavailable, use the [official Docker image](#docker-windows-macos-or-linux) rather than manually copying toolchain folders.

### Docker: Windows, macOS or Linux

The official [**`devkitpro/devkitarm`** image](https://hub.docker.com/r/devkitpro/devkitarm) already includes `3ds-dev` and standard 3DS portlibs. You do **not** need a Dockerfile or to run `dkp-pacman` on every build.

1. Install Docker Desktop (Windows/macOS) or Docker Engine (Linux) and ensure it is running.
2. Download the image:

   ```sh
   docker pull devkitpro/devkitarm:latest
   ```

3. Go to your project's directory **on the host** (the folder containing `Makefile`). Run one of these commands:

   **macOS / Linux / WSL shell:**

   ```sh
   docker run --rm -v "$PWD:/work" -w /work devkitpro/devkitarm:latest make
   ```

   **Windows PowerShell:**

   ```powershell
   docker run --rm --mount "type=bind,source=$((Get-Location).Path),target=/work" -w /work devkitpro/devkitarm:latest make
   ```

The container compiles the project, then exits. **Build output stays in your project folder** on the host because the folder is mounted as `/work`. `--rm` removes only the stopped container, not your files. No compiler needs to be installed on the host.

Check the compiler inside the image with:

```sh
docker run --rm devkitpro/devkitarm:latest /opt/devkitpro/devkitARM/bin/arm-none-eabi-gcc --version
```

For **repeatable builds**, replace `latest` with a dated image tag you have tested (for example `20260610`), and record it in the project README or CI configuration. The official image is published for both `linux/amd64` and `linux/arm64` at that tag. On Linux, container-created files may be owned by root; you can add `--user "$(id -u):$(id -g)"` to the `docker run` command if necessary.

**Running a console application is separate from building it.** You can send the resulting `.3dsx` from your host, copy it to the SD card, or run `3dslink` from a container with appropriate networking. For the simplest first test, use the SD card or emulator. Docker Desktop's networking differs from native Linux networking, so do not assume automatic netloader discovery will work from inside Docker.

## 3. Check that the toolchain works

On **Windows MSYS2, macOS, or Linux**, run:

```sh
echo "DEVKITPRO=$DEVKITPRO"
echo "DEVKITARM=$DEVKITARM"
"$DEVKITARM/bin/arm-none-eabi-gcc" --version
command -v make
command -v 3dslink
```

You should see nonempty paths, a compiler version, and paths to `make` and `3dslink`. On the standard Unix installation, the variables should normally point to `/opt/devkitpro` and `/opt/devkitpro/devkitARM`. If this fails, fix the installation or shell environment **before** trying a project build. In Docker, use the compiler check from the previous section.

## 4. Build your first `.3dsx`

Use the maintained [devkitPro 3DS examples](https://github.com/devkitPro/3ds-examples), not a decade-old tutorial. The application template already has the right compiler flags, linker settings, `libctru` configuration, and output rules.

**Native installation** (in your devkitPro MSYS2 shell on Windows, or a terminal on macOS/Linux):

```sh
git clone https://github.com/devkitPro/3ds-examples.git
cp -R 3ds-examples/templates/application hello-3ds
cd hello-3ds
make
```

On **Docker**, run the `git clone`, `cp` and `cd` commands on your **host** first; then use the Docker `make` command above from inside `hello-3ds`.

A successful build creates files like:

```text
hello-3ds/
├── Makefile
├── source/main.c
├── hello-3ds.3dsx    ← run this
├── hello-3ds.elf     ← keep for debugging
├── hello-3ds.smdh    ← Homebrew Menu metadata
└── build/            ← intermediate files
```

The template shows **“Hello, world!”** on the top screen and exits when you press **START**. Edit `source/main.c`, rebuild with `make`, and run the new `.3dsx`. Use `make clean` if you change build rules or need a full rebuild.

**No Git installed?** Download the source ZIP from the [3ds-examples repository](https://github.com/devkitPro/3ds-examples), extract it, and copy `templates/application` as `hello-3ds` instead.

### What the template's Makefile controls

| Entry | Meaning |
| --- | --- |
| `TARGET` | Output name (`hello-3ds.3dsx`) |
| `SOURCES` | C, C++, assembly and shader source directories; usually `source` |
| `INCLUDES` | Headers, usually `include` |
| `DATA` | Files linked into the executable at build time |
| `GRAPHICS` | Assets for `tex3ds`, usually `gfx` |
| `ROMFS` | Optional directory bundled as a read-only filesystem |
| `LIBS` / `LIBDIRS` | Libraries to link and locations to search |
| `APP_TITLE`, `APP_AUTHOR`, `APP_DESCRIPTION`, `ICON` | Homebrew Menu name, text and icon |

The official template uses ARM11-specific flags, including **`-march=armv6k`** and **`-mfloat-abi=hard`**. Keep those unless you know why you are changing them. An ordinary desktop `gcc` or `clang` build cannot produce a runnable `.3dsx`.

## 5. Run it

### In an emulator

Open `hello-3ds.3dsx` with [Azahar](https://github.com/azahar-emu/azahar). An emulator is good for quick functional checks, but it does **not** prove a program is fast, correctly synchronized, or safe on real 3DS hardware.

### On a real 3DS: SD card

You need a console that already runs [Luma3DS](luma3ds.md) and the [Homebrew Menu](https://github.com/devkitPro/3ds-hbmenu). This page does not cover modifying a stock console; see [boot9strap](boot9strap.md) and [Luma3DS](luma3ds.md).

1. Turn off the console before removing its SD card.
2. Copy `hello-3ds.3dsx` to `sdmc:/3ds/hello-3ds/hello-3ds.3dsx` (that is `/3ds/hello-3ds/hello-3ds.3dsx` on the SD card).
3. Put the SD card back, open the Homebrew Menu, and launch **hello-3ds**.
4. Press **START** to exit the example.

The Homebrew Menu reads embedded SMDH metadata from `.3dsx` files; you normally do **not** need to copy the standalone `.smdh` for this template.

### On a real 3DS: wireless loading

Wireless loading is faster for edit/build/test cycles.

1. Connect the 3DS and development PC to the same network.
2. Open the **Homebrew Menu** on the 3DS and press **Y** to start its netloader. Note the IP shown on the console.
3. From the folder containing the built executable, run:

   ```sh
   3dslink hello-3ds.3dsx
   ```

   If broadcast discovery fails, supply the console's IP explicitly:

   ```sh
   3dslink -a 192.168.1.50 hello-3ds.3dsx
   ```

   Replace `192.168.1.50` with the actual IP. Use `-s` if the program supports sending debug output to the host and you want `3dslink` to start its output server after uploading:

   ```sh
   3dslink -s -a 192.168.1.50 hello-3ds.3dsx
   ```

`-s` starts a server; it does not automatically make every `printf` in an application appear on the PC. Applications must initialize/use the relevant logging or network output path. See [debugging](../engineering/debugging.md) for instrumentation and GDB.

## 6. Grow the project

### Add libraries

The `3ds-dev` group installs the core libraries and tools. Additional third-party libraries are packaged as `3ds-*` packages. Search the package index before compiling a dependency manually:

```sh
# Windows devkitPro MSYS2:
pacman -Ss 3ds-

# macOS / Debian-based Linux with devkitPro pacman:
dkp-pacman -Ss 3ds-
```

Install a package by its actual name (`pacman -S package-name` on Windows, `sudo dkp-pacman -S package-name` on macOS/Debian-based Linux). Add its header and linker settings according to that library's examples; installing a package does not automatically add it to every project.

### Graphics, shaders and assets

- **2D UI/sprites:** start with **citro2d**.
- **3D/PICA200:** start with **citro3d** and a GPU example from `3ds-examples/graphics/gpu`.
- **Shader assembly:** **picasso** turns PICA200 shader source into a binary consumed by the graphics library.
- **Textures:** **tex3ds** can convert sprites and atlases from `.t3s` descriptions into `.t3x` assets.
- **Files loaded at runtime:** use a project `romfs/` directory and the `ROMFS` Makefile setting; use `romfsInit()` in the app before opening RomFS paths.

Do not put every asset into a giant C array or draw all graphics pixel-by-pixel on the CPU. The detailed references are [GPU rendering](../engineering/gpu-rendering.md), [asset pipeline](../engineering/asset-pipeline.md), and [memory and cache](../engineering/memory-and-cache.md).

### `.3dsx` versus `.cia`

| | `.3dsx` | `.cia` |
| --- | --- | --- |
| What it is | Homebrew Menu executable | Installable 3DS title package |
| Best for | Development and ordinary homebrew releases | Apps that need a HOME Menu title, their own title/save configuration, or particular title settings |
| How you run it | Homebrew Menu / netloader | Install the package on a CFW console, then launch its HOME Menu icon |
| Build complexity | Included in the standard template | Extra metadata, banner, title configuration and packaging tools |

**Start with `.3dsx`.** A `.cia` is not simply a renamed `.3dsx`: it needs an appropriate executable, title metadata, exheader/configuration and packaging tooling such as `makerom`/`bannertool`. CIA generation is project-specific; do not copy an old `makerom` command without checking its RSF, memory mode, privileges and title ID.

### Debugging and release builds

- Keep the unstripped **`.elf`** with symbols. Match it to the exact build that crashed.
- Test on an **Old 3DS** if you support one; New 3DS and emulators can hide CPU or memory problems.
- Enable useful diagnostics in development builds, then compare performance with diagnostics disabled.
- Save a record of compiler/library versions and Docker image tags for reproducibility:

  ```sh
  # Windows: pacman; macOS / Debian Linux: dkp-pacman
  dkp-pacman -Q devkitARM libctru citro3d citro2d 3dslink
  ```

  On Windows replace `dkp-pacman` with `pacman`.

See [debugging](../engineering/debugging.md), [performance](../engineering/performance.md), and [testing on hardware](../engineering/testing-on-hardware.md).

## 7. Troubleshooting

| Problem | Check / fix |
| --- | --- |
| `Please set DEVKITARM in your environment` | Your shell has not loaded the devkitPro variables. Reopen the correct terminal or set `DEVKITPRO`, `DEVKITARM` and `PATH` as shown above. |
| `arm-none-eabi-gcc: command not found` | Check that `3ds-dev` is installed and `$DEVKITARM/bin` exists. Use `"$DEVKITARM/bin/arm-none-eabi-gcc" --version` to distinguish a PATH error from a missing compiler. |
| `3ds_rules: No such file or directory` | Wrong `DEVKITARM` value or incomplete installation. Confirm `$DEVKITARM/3ds_rules` exists. |
| `3ds.h: No such file or directory` / `cannot find -lctru` | `libctru` or the Makefile's include/library setup is missing. Start with the unmodified official template and confirm `3ds-dev` is installed. |
| `make` is not found on Windows | You are likely in PowerShell/CMD instead of **devkitPro MSYS2**, or your MSYS2 packages are incomplete. |
| `make` is not found on macOS / Linux | Install Xcode Command Line Tools (macOS) or the system `make` package (Linux). |
| `3dslink: command not found` | Check the `3ds-dev` installation and `$DEVKITPRO/tools/bin` in `PATH`. |
| `3dslink`: no response from 3DS | Enable netloader with **Y**, use `-a <3DS-IP>`, check both devices are on the same LAN, and check local firewall / guest Wi-Fi isolation. |
| Builds in Docker but not on host | The container's toolchain and environment differ from the host. Compare environment variables, package versions and project dependencies. |
| Docker output files belong to root on Linux | Use the host UID/GID with `docker run --user "$(id -u):$(id -g)" ...` on Linux. |
| `uses VFP register arguments` link error | Object files disagree on ARM float ABI. Rebuild everything with the template's hard-float settings. |
| Works in emulator but fails on hardware | Verify on the target Old/New 3DS, inspect errors and resources, and use [hardware testing](../engineering/testing-on-hardware.md). |

If a project still fails, first build **the untouched `templates/application` example**. If that works, your installation is probably fine and the failure is in the project's Makefile, dependencies or source. If it fails, fix the environment before editing application code.

## 8. Reference links

**Official tooling and examples**

- [devkitPro: Getting Started](https://devkitpro.org/wiki/Getting_Started) — overview and installation entry point.
- [devkitPro: pacman](https://devkitpro.org/wiki/devkitPro_pacman) — package-manager setup, especially for non-Debian Linux distributions.
- [Windows installer releases](https://github.com/devkitPro/installer/releases/latest) / [macOS and Linux pacman releases](https://github.com/devkitPro/pacman/releases/latest).
- [Official Docker image](https://hub.docker.com/r/devkitpro/devkitarm) / [Dockerfile](https://github.com/devkitPro/docker/blob/master/devkitarm/Dockerfile).
- [3DS examples](https://github.com/devkitPro/3ds-examples) / [application template Makefile](https://github.com/devkitPro/3ds-examples/blob/master/templates/application/Makefile).
- [libctru](https://github.com/devkitPro/libctru), [citro3d](https://github.com/devkitPro/citro3d), [citro2d](https://github.com/devkitPro/citro2d), [picasso](https://github.com/devkitPro/picasso), [3dslink](https://github.com/devkitPro/3dslink), [Homebrew Menu](https://github.com/devkitPro/3ds-hbmenu).

**More detailed pages in this wiki**

- [libctru](libctru.md) · [picasso and citro3d](picasso-and-citro3d.md) · [Luma3DS](luma3ds.md).
- [GPU rendering](../engineering/gpu-rendering.md) · [asset pipeline](../engineering/asset-pipeline.md) · [debugging](../engineering/debugging.md) · [performance](../engineering/performance.md) · [testing on hardware](../engineering/testing-on-hardware.md).

*Install and package instructions verified against the upstream devkitPro repositories and project examples in November 2026. Package versions and release links can change; avoid copying stale version numbers into new projects.*
