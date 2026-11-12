# OZ glossary

<a name="oz"></a>

### OZ / Ounce

Nintendo Switch 2's platform codename, used by this wiki to distinguish it from [NX](../nx/glossary.md#nx). See the [overview](index.md).

<a name="t239"></a>

### T239

The NVIDIA system-on-a-chip identified by community research in Switch 2. It combines processors and other controllers; see [CPU and memory](hardware/cpu-and-memory.md).

<a name="nvn2"></a>

### NVN2

The graphics interface used by native Switch 2 software, as identified by reverse engineering. It is distinct from the GPU hardware and from a public homebrew library; see [GPU](hardware/gpu.md).

<a name="compatibility-mode"></a>

### Compatibility mode

The software environment that adapts original Switch titles to Switch 2. It is not the same execution environment as a native Switch 2 title; see [compatibility mode](software/compatibility.md).

<a name="a78c"></a>

### Cortex-A78C

The Arm processor core used in the Switch 2's main chip, eight of which run at up to 1 GHz in the retail console. It is a successor to the Cortex-A57 of the original Switch; see [CPU and memory](hardware/cpu-and-memory.md).

<a name="ampere"></a>

### Ampere

The NVIDIA GPU architecture used in the Switch 2's main chip, a successor to the [Maxwell](../nx/glossary.md#maxwell) GPU of the original Switch. "Ampere" also names an unrelated datacenter GPU family, so desktop Ampere specifications do not automatically describe this console; see [GPU](hardware/gpu.md).

<a name="npdm"></a>

### NPDM

The Nintendo Program Descriptor Meta, the metadata blob that describes a program to Horizon: its address space, its core affinity mask, the services it may use, the system calls it may make and its resource limits. Reading a program's NPDM is how you learn what it is actually permitted to do; see [system and security](software/system.md).

<a name="acid"></a>

### ACID

Application Cache ID, the identifier Horizon uses to find an application's cached content and data. Its layout is version-dependent, so a parser written for one console generation may misread another's; see [system and security](software/system.md).

<a name="nro"></a>

### NRO

The Nintendo Relocatable Object, the executable container format used by homebrew on the original Switch. No unrestricted Switch 2 environment loads `.nro` files; see [homebrew environments and workflow](homebrew/state-and-workflow.md).

<a name="romfs"></a>

### RomFS

The read-only filesystem embedded in a packaged title's data container. Switch 2 tools read and write it through overlay and patch mechanisms; see [display, input and storage](hardware/devices.md) and [homebrew environments and workflow](homebrew/state-and-workflow.md).

<a name="exefs"></a>

### ExeFS

The executable filesystem inside a packaged title, holding the main program and its modules. It is the container that the loader ultimately maps into memory; see [system and security](software/system.md).

<a name="nax"></a>

### NAX

The encryption scheme used for content stored on removable storage. The original Switch used NAX0; the Switch 2 uses NAX1, so parsers and storage tooling must be version-aware; see [display, input and storage](hardware/devices.md).

<a name="ufs"></a>

### UFS and LUN

Universal Flash Storage, the internal storage standard used by the Switch 2, organised into logical units (LUNs). The boot chain lives in LUN0 and LUN1 holds a redundant copy; see [display, input and storage](hardware/devices.md).

<a name="game-key-card"></a>

### Game-Key Card

A physical cartridge that carries an authorization key rather than the full game data. The game itself must still be downloaded, but the card remains necessary to launch it; see [display, input and storage](hardware/devices.md).

<a name="joy-con-2"></a>

### Joy-Con 2

The Switch 2's detachable controllers, a revised version of the original Switch's Joy-Con with different internals and a new protocol; see [controllers](hardware/controllers.md).

<a name="nvn"></a>

### NVN

Nintendo's graphics interface for the original Switch, a predecessor of NVN2. Understanding NVN helps interpret NVN2 research, but the two differ in ways that matter to a port; see [GPU](hardware/gpu.md).

<a name="syncpoint"></a>

### Synchronization point (syncpoint)

A kernel object that lets one processing unit signal that it has finished work another is waiting on. Switch 2 graphics work uses these to order GPU queues against the display; see [graphics services](software/services.md).

<a name="gameshare"></a>

### GameShare

A Nintendo feature that streams gameplay from one console to nearby clients on both console generations rather than transferring the whole game; see [display, input and storage](hardware/devices.md).

<a name="ldr"></a>

### ldr

The Horizon program loader, the system service that maps signed executable content into memory. It is one of the two services at the centre of the first public Switch 2 exploit chain; see [homebrew environments and workflow](homebrew/state-and-workflow.md).

## Sources

- [SwitchBrew T239](https://switchbrew.org/wiki/Switch_2:_Tegra_T239) — chip identification.
- [SwitchBrew compatibility mode](https://switchbrew.org/wiki/Switch_2:_Compatibility_Mode) — compatibility components.
- [SwitchBrew flash filesystem](https://switchbrew.org/wiki/Switch_2:_Flash_Filesystem) — storage layout and LUN structure.
- [NVN object research](https://github.com/seedds/nxnative/blob/main/docs/reference/nvn-objects.md) — NVN and NVN2 research.
