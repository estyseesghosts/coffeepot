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
The community C library for Switch user programs: kernel wrappers plus clients for ~100 system services. See [libnx and deko3d](homebrew/deko3d-and-libnx.md).

<a name="title-override"></a>

### Title override
Launching the homebrew menu in place of a full game (hold R on launch) to get the full application memory budget instead of the small applet budget. See [Atmosphere and running](homebrew/atmosphere-and-running.md).

<a name="atmosphere"></a>

### Atmosphere
The standard Switch custom firmware: loader and service patches that allow homebrew with full memory. See [Atmosphere and running](homebrew/atmosphere-and-running.md).
