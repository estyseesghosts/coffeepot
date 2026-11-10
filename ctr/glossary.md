# CTR glossary

Terms specific to the Nintendo 3DS family. General terms live in [the global glossary](../glossary.md). Each entry points at the canonical page for the full story.

### CTR {#ctr}

Nintendo's internal codename for the Nintendo 3DS family, used here as this platform's directory name and adjective (CTR CPU, CTR OS). See [CTR overview](index.md).

### Old 3DS and New 3DS {#old-new-3ds}

Community shorthand. **Old 3DS** means the 2011–2013 models (3DS, 3DS XL, 2DS); **New 3DS** means the 2014+ models (New 3DS, New 3DS XL, New 2DS XL), which have more cores, more memory and a faster mode. See [CTR models](hardware/models.md).

### ARM11 {#arm11}

The 32-bit [ARM](../glossary.md#arm) processor architecture running 3DS games and apps (a dual-core MPCore in the Old 3DS, quad-core in the New 3DS). See [CTR CPU](hardware/cpu.md).

### MPCore {#mpcore}

ARM's multi-core packaging of the ARM11: several "MP11" cores plus a shared cache-coherency unit, interrupt controller and per-core timers. See [CTR CPU](hardware/cpu.md).

### ARM9 {#arm9}

The older 32-bit [ARM](../glossary.md#arm) processor used in the 3DS as a separate system/security processor, also providing Nintendo DS backwards compatibility. See [CTR CPU](hardware/cpu.md).

### ARM7 {#arm7}

The Game Boy Advance–era ARM processor, kept for DS/DSi/GBA backwards compatibility and switched off in normal 3DS mode. See [CTR CPU](hardware/cpu.md).

### Appcore and syscore {#appcore-syscore}

The names for the first two ARM11 cores. The **appcore** runs the game; the **syscore** runs system services and can lend part of its time to the game. See [CTR OS](software/os.md).

### PICA200 {#pica200}

The graphics chip family used by the 3DS (a DMP design). Fixed-function hardware with some programmable stages, driving the stereoscopic display. See [CTR GPU](hardware/gpu.md).

### FCRAM {#fcram}

"Fast Cycle RAM": the 3DS's main memory (128 MB on Old 3DS, 256 MB on New 3DS). See [CTR memory](hardware/memory.md).

### VRAM {#vram}

The 6 MB of fast memory inside the [SoC](../glossary.md#soc), used mainly by the GPU. See [CTR memory](hardware/memory.md).

### AXI WRAM {#axi-wram}

512 KB of fast on-chip memory attached to the ARM11 over the AXI bus; the ARM11 kernel lives here. See [CTR memory](hardware/memory.md).

### CSND {#csnd}

The Nintendo DS–style sound hardware (32 channels) kept in the 3DS for backwards compatibility. See [CTR audio](hardware/audio.md).

### LgyFb {#lgyfb}

"Legacy framebuffer": the block that receives DS/GBA screen output and passes it on for display. See [CTR displays](hardware/display.md).

### PDC {#pdc}

The "primitive display controller" registers that time and feed each LCD. See [CTR displays](hardware/display.md).

### PXI {#pxi}

The FIFO message channel between the ARM11 and ARM9. See [CTR OS](software/os.md).

### FIRM {#firm}

The 3DS family name for versioned [firmware](../glossary.md#firmware) partitions loaded at boot. See [CTR firmware](software/firmware.md).

### NATIVE_FIRM, TWL_FIRM, AGB_FIRM, SAFE_MODE_FIRM {#firm-types}

The four firmware images: normal 3DS mode, DS/DSi mode, GBA mode and the maintenance/updater mode. See [CTR firmware](software/firmware.md).

### Boot9 and Boot11 {#boot9-boot11}

The unchangeable boot ROMs of the ARM9 and ARM11. See [CTR boot process](software/boot-process.md).

### Kernel9, Process9, Kernel11 {#kernel9-11}

The ARM9 kernel, the single ARM9 userland process that does storage and cryptography, and the ARM11 kernel. See [CTR OS](software/os.md).

### Horizon {#horizon}

Nintendo's OS name; for the 3DS see [CTR OS](software/os.md). The global glossary entry covers both consoles: [Horizon](../glossary.md#horizon).

### SVC {#svc}

"Supervisor call": the 3DS's [system call](../glossary.md#syscall) instruction. See [CTR kernel](software/kernel.md).

### Service {#service}

A system [process](../glossary.md#process) that answers requests from other processes by name (for example `fs:USER`). See [CTR services](software/services.md).

### Title {#title}

3DS unit of installable software (a game, system module, DLC or update) identified by a 64-bit title ID. See [CTR titles](software/titles.md).

### TMD and ticket {#tmd-ticket}

The **title metadata** lists a title's contents and hashes; the **ticket** carries the (encrypted) key and the right to use it. See [CTR titles](software/titles.md).

### NCSD, NCCH, CCI, CIA {#containers}

The container formats for firmware/NAND/cartridges (NCSD), executable or data packages (NCCH), cartridge images (CCI) and installable packages (CIA). See [CTR file formats](software/file-formats.md).

### 3DSX and SMDH {#3dsx}

The homebrew executable format and its icon/title metadata file. See [CTR development workflow](homebrew/development-workflow.md).

### Dev unit {#dev-unit}

Nintendo's non-retail development hardware and its extra software. See [CTR development units](software/dev-unit-software.md).

### emuNAND {#emunand}

A copy of the system's internal storage kept on the SD card so custom firmware can boot a separate system install. See [CTR custom firmware](homebrew/custom-firmware.md).

### Primary and secondary exploit {#primary-secondary}

A **primary** (entry-point) exploit works on an untouched console; a **secondary** exploit needs an earlier one to install it first. See [CTR exploits](homebrew/exploits.md).

### Sighax {#sighax}

A flaw in the boot ROM's signature check that lets anyone craft a firmware the console accepts as genuine. See [boot9strap](homebrew/boot9strap.md).

### boot9strap {#boot9strap}

The standard bootloader built on [sighax](#sighax) that runs custom code with full privileges at startup. See [boot9strap](homebrew/boot9strap.md).

### Luma3DS {#luma3ds}

The most widely used 3DS custom firmware. See [Luma3DS](homebrew/luma3ds.md) and [CTR custom firmware](homebrew/custom-firmware.md).

### Rosalina {#rosalina}

Luma3DS's overlay menu and system module: screenshots, screen filters, cheats, a debugger, and a loader for `.3dsx` apps. See [Luma3DS](homebrew/luma3ds.md).

### LayeredFS and 3GX {#layeredfs-3gx}

Luma3DS game-modding features: **LayeredFS** replaces a game's files by redirecting paths to the SD card; **3GX** is the game-plugin format. See [Luma3DS](homebrew/luma3ds.md).

### CRO, CRS, CRR {#cro}

Dynamic code modules (`.cro`), the static module's symbol file (`.crs`) and the module verification list (`.crr`), loaded by the RO service. See [CTR dynamic modules](software/cro-modules.md).

### CEC / StreetPass {#cec}

The system module that carries StreetPass messages between consoles through per-game mailboxes. See [StreetPass (CEC)](software/streetpass-cec.md).

### QTM {#qtm}

The New 3DS service that tracks the viewer's head and positions the 3D screen's parallax barrier. See [CTR displays](hardware/display.md).

### Linear heap {#linear-heap}

Physically contiguous process memory (the "LINEAR" region) used for buffers the GPU, DSP or DMA must read. See [CTR memory](hardware/memory.md) and [libctru](homebrew/libctru.md).

### SHBIN, picasso {#shbin}

`.shbin` is the compiled GPU shader container; **picasso** is the assembler that makes it. See [picasso and citro3d](homebrew/picasso-and-citro3d.md).

### libctru, citro3d {#libctru}

The standard homebrew user library and the GPU wrapper built on it. See [libctru](homebrew/libctru.md) and [picasso and citro3d](homebrew/picasso-and-citro3d.md).

### GSP {#gsp}

The system service (`gsp::Gpu`) through which applications use the GPU: it runs command lists, memory fills and display transfers, swaps the screens' framebuffers and reports completion events. Homebrew reaches it through libctru's `gspgpu` and `gx` functions. See [CTR GPU](hardware/gpu.md) and [performance](engineering/performance.md#per-operation-gpu-times-the-gsp-performance-log).

### TEV (texture combiner) {#tev}

The six fixed-function "texture environment" stages of the PICA200's fragment pipeline. Each stage combines up to three inputs (texture colours, vertex colour, a constant, the previous stage, lighting results) with one function such as multiply or interpolate; they replace programmable pixel shaders. See [GPU rendering](engineering/gpu-rendering.md#colour-combiners-tev-recipes).

### Homebrew Menu and hb:ldr {#hbmenu}

The **Homebrew Menu** (`3ds-hbmenu`, file `boot.3dsx`) lists and starts `.3dsx` programs and includes the 3dslink netloader. **`hb:ldr`** is the Luma3DS loader service that actually loads a 3DSX into a taken-over title's process. See [development workflow](homebrew/development-workflow.md#running-on-a-console) and [Luma3DS](homebrew/luma3ds.md#homebrew-loading-hbldr).

### 3dslink {#3dslink}

A devkitPro protocol and PC tool that sends a `.3dsx` to the Homebrew Menu over Wi-Fi and can relay the program's text output back to the PC. See [debugging](engineering/debugging.md#logging-over-the-network-3dslink).


### Extended header (exheader) {#exheader}

The part of an executable title (NCCH) that declares what the program may do: its memory mode, CPU cores and clock, thread priority, the system services and filesystem areas it may use, memory mappings and kernel calls. The kernel and loader enforce it. Luma3DS generates one for `.3dsx` homebrew. See [file formats](software/file-formats.md) and [development workflow](homebrew/development-workflow.md#packaging-3dsx-or-cia).
