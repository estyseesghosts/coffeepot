# CTR glossary

Terms specific to the Nintendo 3DS family. General terms live in [the global glossary](../glossary.md). Each entry points at the canonical page for the full story.

### CTR {#ctr}

Nintendo's internal codename for the Nintendo 3DS family, used here as this platform's directory name and adjective (CTR CPU, CTR OS). See [CTR overview](index.md).

### Old 3DS and New 3DS {#old-new-3ds}

Community shorthand used throughout this wiki. **Old 3DS** means the Nintendo 3DS, 3DS XL and 2DS (2011–2013); **New 3DS** means the New Nintendo 3DS, New Nintendo 3DS XL and New Nintendo 2DS XL (2014–2017), which have more cores, more memory and a faster mode. Unmarked statements describe the Old 3DS; New 3DS facts appear as **New 3DS:** notes. See [CTR models](hardware/models.md).

### ARM11 {#arm11}

The 32-bit [ARM](../glossary.md#arm) processor architecture running 3DS games and apps (a dual-core MPCore; **New 3DS:** quad-core). See [CTR CPU](hardware/cpu.md).

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

"Fast Cycle RAM": the 3DS's main memory (128 MB; **New 3DS:** 256 MB). See [CTR memory](hardware/memory.md).

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

**New 3DS only:** the service that tracks the viewer's head and positions the 3D screen's parallax barrier. See [CTR displays](hardware/display.md).

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

The **Homebrew Menu** (`3ds-hbmenu`, file `boot.3dsx`) lists and starts `.3dsx` programs and includes the 3dslink netloader. **`hb:ldr`** is the Luma3DS loader service that actually loads a 3DSX into a taken-over title's process. See [development workflow](homebrew/development-workflow.md#5-run-it) and [Luma3DS](homebrew/luma3ds.md#homebrew-loading-hbldr).

### 3dslink {#3dslink}

A devkitPro protocol and PC tool that sends a `.3dsx` to the Homebrew Menu over Wi-Fi and can relay the program's text output back to the PC. See [debugging](engineering/debugging.md#logging-over-the-network-3dslink).


### Extended header (exheader) {#exheader}

The part of an executable title (NCCH) that declares what the program may do: its memory mode, CPU cores and clock, thread priority, the system services and filesystem areas it may use, memory mappings and kernel calls. The kernel and loader enforce it. Luma3DS generates one for `.3dsx` homebrew. See [file formats](software/file-formats.md) and [development workflow](homebrew/development-workflow.md#3dsx-versus-cia).

### Applet manager (APT) {#apt}

The system service that tells each running program whether it is in front, suspended, asleep or ordered to close, and that launches applets such as the HOME Menu and the software keyboard. libctru wraps it as `aptMainLoop` and the APT hooks. See [application lifecycle](engineering/application-lifecycle.md).

### FS archive {#fs-archive}

One storage area (SD card, save data, extdata, RomFS) opened through the file service, then mounted by libctru as a device name such as `sdmc:`. See [files and saves](engineering/filesystem-and-saves.md).

### RomFS {#romfs}

The read-only file tree bundled inside a program. A `.3dsx` can embed one, and `romfsMountSelf` mounts it. See [files and saves](engineering/filesystem-and-saves.md).

### Save data {#save-data}

Private per-title storage on an installed title. Changes must be committed explicitly (`ARCHIVE_ACTION_COMMIT_SAVE_DATA`). See [CTR savegames](software/savegames.md) and [files and saves](engineering/filesystem-and-saves.md).

### Extdata {#extdata}

Larger storage that a title creates on the SD card. libctru's stdio layer treats it as read-only. See [files and saves](engineering/filesystem-and-saves.md).

### Command buffer {#command-buffer}

The 64-word per-thread area at TLS offset `0x80` where an IPC request is written and the reply is read back. See [IPC and services](engineering/ipc-and-services.md).

### Session {#session}

A private client/server message channel to a service, referred to by a handle and opened through `srv:` `GetServiceHandle`. See [IPC and services](engineering/ipc-and-services.md).

### httpc (http:C) {#httpc}

The system HTTP/HTTPS service; libctru wraps it in `httpc.h`. See [networking](engineering/networking.md).

### Command list {#command-list}

A buffer of register writes that the GPU's command processor (P3D) executes to draw. citro3d builds one per frame. See [PICA200 command pipeline](engineering/pica200-command-pipeline.md).

### P3D {#p3d}

The part of the PICA200 that reads and runs a command list; it raises an interrupt when the list ends. See [PICA200 command pipeline](engineering/pica200-command-pipeline.md).

### GX command {#gx-command}

A request queued to the GSP service, such as processing a command list or copying a framebuffer. See [PICA200 command pipeline](engineering/pica200-command-pipeline.md).

### FINALIZE {#finalize}

The `GPUREG_FINALIZE` register write that ends a command list. See [PICA200 command pipeline](engineering/pica200-command-pipeline.md).

### Operand descriptor {#operand-descriptor}

A table entry in a shader binary giving the swizzle and negation for an instruction's operands. See [PICA200 shader programming](engineering/pica200-shader-programming.md).

### DVLB / DVLE / DVLP {#dvlb}

The container parts of a compiled shader (`.shbin`): the bundle, each entry point, and the shared program and descriptor data. See [PICA200 shader programming](engineering/pica200-shader-programming.md).

### Uniform {#uniform}

A shader constant set by the CPU before drawing. See [PICA200 shader programming](engineering/pica200-shader-programming.md).

### Address register {#address-register}

The shader registers `a0.x`, `a0.y` and `aL` used for indexed register access and loops. See [PICA200 shader programming](engineering/pica200-shader-programming.md).

### Geometry shader {#geometry-shader}

An optional shader stage after the vertex shader that can emit extra vertices. See [PICA200 shader programming](engineering/pica200-shader-programming.md).

### Outmap {#outmap}

The configuration that maps shader output registers to position, colour, texture coordinates and so on. See [PICA200 shader programming](engineering/pica200-shader-programming.md).

### MVD {#mvd}

**New 3DS only:** the hardware video decoder block (H.264 and MJPEG, plus YUV-to-RGB conversion), reached through the MVD services. See [I/O and peripherals](hardware/io-and-peripherals.md).

### Stubbed SVC {#stubbed-svc}

A system call that exists in the kernel's call table but only returns an error code (for example `0xF8C007F4`) instead of doing anything. Several ARM11 kernel calls are stubs, some since a given firmware version. See [kernel](software/kernel.md).

### Snoop Control Unit (SCU) {#scu}

The MPCore block that keeps the cores' data caches coherent. See [ARM11 MPCore internals](hardware/arm11-mpcore.md#snoop-control-unit-scu).

### BTAC {#btac}

Branch target address cache, the ARM11's 128-entry dynamic branch predictor. See [ARM11 MPCore internals](hardware/arm11-mpcore.md#branch-prediction).

### RunFast mode {#runfast}

The VFP11 mode with flush-to-zero, default NaN and all exception enables off, so every operation finishes in hardware. libctru sets it. See [ARM11 MPCore internals](hardware/arm11-mpcore.md#vfp11-floating-point-unit).

### Card cipher (SNOW 2.0 / RC4) {#card-cipher}

The stream ciphers used on 3DS game card command traffic. See [game cards](hardware/gamecard.md).

### Kernel object {#kernel-object}

A structure the kernel keeps for something it manages (process, thread, event, session). Programs hold handles that refer to it. See [kernel objects](software/kernel-objects.md).

### Class token {#class-token}

A one-byte code the kernel gives each kernel object class, marking for example whether the object can be waited on. See [kernel objects](software/kernel-objects.md).

### APPMEMTYPE {#appmemtype}

The number that picks how FCRAM is split between the application, system and base regions. See [CTR memory](hardware/memory.md).

### cetk {#cetk}

The common ticket shared by every console, for titles not sold in the eShop. See [title metadata and tickets](software/title-metadata-and-tickets.md).

### Gspwn {#gspwn}

Using the GPU's DMA to read and write physical memory from a userland exploit. See [system flaws](software/system-flaws.md).

### Seedminer {#seedminer}

Brute force of the `movable.sed` key Y. See [system flaws](software/system-flaws.md).

### NVer / CVer {#nver-cver}

System data archives that hold the installed system version. See [titles](software/titles.md).

### 3GX {#3gx}

Luma3DS's game-plugin file format (magic `3GX$0002`), loaded from `/luma/plugins`. See [Luma3DS](homebrew/luma3ds.md).

### *hax payload {#hax-payload}

The second-stage payload that most old userland exploits boot to start the Homebrew Launcher. No longer needed since Luma3DS 8.0. See [exploits](homebrew/exploits.md).

### InputRedirection {#inputredirection}

A Rosalina feature that receives controller input from a PC over UDP port 4950. See [Luma3DS](homebrew/luma3ds.md).

### Human interface device service (HID) {#hid}

The system service (`hid:USER`) that delivers buttons, Circle Pad, touch and motion-sensor data through shared memory. See [input and sensors](engineering/input-and-sensors.md).

### IRRST {#irrst}

`ir:rst`, the service for the C-Stick and ZL/ZR on a New 3DS; libctru starts it from HID. See [input and sensors](engineering/input-and-sensors.md).

### CAM / Y2R {#cam-y2r}

The camera service and the hardware YUV-to-RGB converter. See [cameras and Y2R](engineering/camera-and-y2r.md).

### MIC {#mic}

The microphone service. See [microphone](engineering/microphone.md).

### NFC {#nfc}

The contactless reader service used for amiibo. Built in on a New 3DS; an Old 3DS needs an accessory. See [NFC and infrared](engineering/nfc-and-infrared.md).

### UDS {#uds}

`nwm::UDS`, local-wireless communication between 3DS consoles. It is not DS Wireless. See [local wireless](engineering/local-wireless.md).

### NDM exclusive state {#ndm}

A mode of the network daemon manager that UDS enters. See [local wireless](engineering/local-wireless.md).

### CFG {#cfg}

The configuration service (model, language, region). See [system information and power](engineering/system-info-and-power.md).

### PTM {#ptm}

The power and play-time service (battery, lid, steps, play history). See [system information and power](engineering/system-info-and-power.md).

### Service group {#service-group}

libctru's one-header, one-init/exit-pair wrapper around a system service. See [libctru service groups](engineering/service-groups.md).

### Atlas (sprite sheet) {#atlas}

One texture that holds many small images (sprites or tiles), so they can be drawn together without switching textures. tex3ds builds one with `--atlas` and gives each image its own texture coordinates. See [2D rendering](engineering/rendering-2d.md).

### Draw call {#draw-call}

One request to the GPU to draw a list of triangles with the current state. On the 3DS each call also makes the CPU write changed state into the command buffer, so fewer, larger calls are cheaper. See [GPU rendering](engineering/gpu-rendering.md).

### Batch {#batch}

A run of quads that citro2d submits in one draw call; it ends when the texture, tint, fade, view or drawing mode changes. See [2D rendering](engineering/rendering-2d.md).

### Quad {#quad}

A rectangle made of two triangles with four corners; the unit of 2D drawing on the PICA200. See [2D rendering](engineering/rendering-2d.md).

### Render target {#render-target}

A buffer the GPU draws into: a screen's colour and depth buffers, or a texture used for render-to-texture. citro3d creates them in VRAM. See [GPU rendering](engineering/gpu-rendering.md).

### Morton order {#morton-order}

The interleaved-bits pixel order inside each 8x8 block of a 3DS texture (x bits at even positions, y bits at odd); the blocks themselves are row-major. See [2D rendering](engineering/rendering-2d.md).

### Wide mode {#wide-mode}

A top-screen mode (`gfxSetWide`) that shows 800x240 non-square pixels; exclusive with stereoscopic 3D and not available on every model. See [2D rendering](engineering/rendering-2d.md).
