# NX glossary

Terms specific to the Nintendo Switch family. General terms live in [the global glossary](../glossary.md). Each entry points at the canonical page for the full story.

<a name="nx"></a>

### NX
Nintendo's internal codename for the Nintendo Switch family, used here as this platform's directory name and adjective (NX CPU, NX OS). See [NX overview](index.md).

<a name="tegra-x1"></a>

### Tegra X1
The NVIDIA [system-on-a-chip](../glossary.md#soc) the Switch is built on, combining ARM CPU cores with a Maxwell-based GPU. See [NX CPU](hardware/cpu.md) and [NX GPU](hardware/gpu.md).

<a name="cortex-a57"></a>

### Cortex-A57
The 64-bit [ARM](../glossary.md#arm) processor core used for the Switch's main application processor (four cores). See [NX CPU](hardware/cpu.md).

<a name="maxwell"></a>

### Maxwell
NVIDIA's GPU architecture generation used in the [Tegra X1](#tegra-x1). See [NX GPU](hardware/gpu.md).

<a name="erista"></a>

### Erista
The original Tegra X1 revision in early Switch units (20 nm, ODNX02-A2). See [NX models](hardware/models.md).

<a name="mariko"></a>

### Mariko
The revised Tegra X1+ revision in later Switch, Lite, and OLED units (16 nm, ODNX10-A1). Same programming model, better power. See [NX models](hardware/models.md).

<a name="deko3d"></a>

### deko3d
The community homebrew GPU library for Switch, an explicit Vulkan-like alternative to Nintendo's NVN. See [libnx and deko3d](homebrew/deko3d-and-libnx.md) and [rendering](engineering/rendering.md).

<a name="libnx"></a>

### libnx
The community C library for Switch user programs: kernel wrappers plus clients for the system services (one header per service under `nx/include/switch/services/`). See [libnx and deko3d](homebrew/deko3d-and-libnx.md).

<a name="title-override"></a>

### Title override
Launching the homebrew menu in place of a full game (hold R on launch) to get the full application memory budget instead of the small applet budget. See [Atmosphere and running](homebrew/atmosphere-and-running.md).

<a name="atmosphere"></a>

### Atmosphere
The standard Switch custom firmware: loader and service patches that allow homebrew with full memory. See [Atmosphere and running](homebrew/atmosphere-and-running.md).

<a name="fence"></a>

### Fence
A marker in the GPU's work queue that the GPU signals when it reaches that point, so software can tell when earlier work has finished. In `deko3d`, the CPU waits on one with `dkFenceWait`; `dkQueueWaitFence` makes the GPU wait instead. See [rendering](engineering/rendering.md).

<a name="dksh"></a>

### DKSH
The file format `deko3d` uses for a precompiled shader module (native Maxwell GPU code plus metadata). It is produced on the PC by [uam](#uam); `deko3d` cannot compile shaders at run time. See [libnx and deko3d](homebrew/deko3d-and-libnx.md).

<a name="uam"></a>

### uam
The shader compiler for `deko3d`: a PC tool that turns GLSL into [DKSH](#dksh) files. It is based in part on Mesa's GLSL parser and Nouveau's code generator, but it is a separate project from `deko3d` itself. See [libnx and deko3d](homebrew/deko3d-and-libnx.md).

<a name="gob"></a>

### GOB and block-linear layout
The way the Switch GPU stores images in memory: in tiles called GOBs, 64 bytes wide by 8 rows, grouped into blocks, instead of one row after another (pitch-linear). It is the default image layout in `deko3d`. See [NX GPU](hardware/gpu.md) and [rendering](engineering/rendering.md).
