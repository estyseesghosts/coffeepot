# Global glossary

Terms shared across two or more consoles, or general computing terms. Per-console terms live in `ctr/glossary.md` and `nx/glossary.md`. Glossary entries are pointers — the full story lives on the linked wiki page.

### CPU (central processing unit) {#cpu}

The processor that runs program instructions. Each console has one or more CPUs with distinct roles (running games vs. running system software). See [CTR CPU](ctr/hardware/cpu.md) and [NX CPU](nx/hardware/cpu.md).

### GPU (graphics processing unit) {#gpu}

The hardware that draws 2D/3D images to the screen. Consoles pair fixed-function blocks with programmable stages in different mixes. See [CTR GPU](ctr/hardware/gpu.md) and [NX GPU](nx/hardware/gpu.md).

### System-on-a-chip (SoC) {#soc}

A single chip combining CPU, GPU, memory interfaces, and peripherals, rather than separate chips for each. Both consoles in this wiki are SoC-based designs.

### ARM {#arm}

The family of processor architectures used by both consoles in this wiki. Specific implementations (which ARM core, how many, at what clocks) differ per console — see the per-device CPU pages.

### Kernel {#kernel}

The privileged core of the [operating system](#os) that controls hardware access, memory, and scheduling. Exploiting or bypassing the kernel is central to most homebrew stories. See [CTR OS](ctr/software/os.md) and [NX OS](nx/software/os.md).

### Operating system (OS) {#os}

The system software that manages hardware and runs games and apps. Both consoles in this wiki run Nintendo software Nintendo calls Horizon, but they are distinct implementations — see the per-device OS pages.

### Firmware {#firmware}

Low-level software stored on the device that starts the system and loads the [operating system](#os): boot ROM, bootloaders, and versioned system partitions. See [CTR firmware](ctr/software/firmware.md) and [NX firmware](nx/software/firmware.md).

### Horizon {#horizon}

Nintendo's internal name for the [operating system](#os) on both the 3DS and the Switch — two distinct implementations sharing a name and some design ideas. This wiki always says which one it means and links to [CTR OS](ctr/software/os.md) or [NX OS](nx/software/os.md).

### Memory {#memory}

Temporary storage the [CPU](#cpu) and [GPU](#gpu) work in while running. How memory is split, shared, and protected differs per console — details live on the per-device pages.

### Storage media {#storage-media}

Where games and system software live when powered off: cartridges, internal flash, and SD cards. Per-console formats and filesystems are covered on the per-device firmware and OS pages.

### Homebrew {#homebrew}

Unofficial software written by hobbyists to run on a console: tools, emulators, games. See [CTR homebrew history](ctr/homebrew/history.md).

### Custom firmware (CFW) {#cfw}

Modified system software that enables [homebrew](#homebrew) and extra features. See [CTR custom firmware](ctr/homebrew/custom-firmware.md).

### Bootloader {#bootloader}

A small program that runs first at power-on and loads the next, larger piece of software. Consoles chain several of these, starting from an unchangeable [boot ROM](#boot-rom). See [CTR boot process](ctr/software/boot-process.md).

### Boot ROM {#boot-rom}

Code burned permanently into the processor chip at the factory. It is the first thing to run after power-on and cannot be updated, so flaws in it are permanent. See [CTR boot process](ctr/software/boot-process.md).

### NAND (flash memory) {#nand}

Non-volatile flash memory that keeps its contents with the power off; consoles use it for system software and settings. On the 3DS it is an eMMC chip (see [CTR NAND filesystem](ctr/software/nand-filesystem.md)).

### Cache {#cache}

A small, fast memory next to a CPU that holds recently used data so the CPU rarely waits on slower main memory. Caches can be per-core (level 1, "L1") or shared (level 2, "L2"). See [CTR CPU](ctr/hardware/cpu.md).

### Direct memory access (DMA) {#dma}

A hardware block that copies data between memories or devices without the CPU doing the copying. See [CTR memory](ctr/hardware/memory.md).

### Memory management unit (MMU) {#mmu}

CPU hardware that translates the addresses programs use (**virtual addresses**) into real memory locations (**physical addresses**) and enforces who may read, write, or run each page. See [CTR CPU](ctr/hardware/cpu.md).

### Memory protection unit (MPU) {#mpu}

A simpler cousin of the [MMU](#mmu): it enforces access rules on a few fixed address regions but does no address translation. See [CTR CPU](ctr/hardware/cpu.md).

### SIMD {#simd}

"Single instruction, multiple data": one instruction that works on several values packed into one register at once. See [CTR CPU](ctr/hardware/cpu.md).

### Floating-point unit (FPU) {#fpu}

Hardware for arithmetic on fractional numbers (`float`, `double`). On the 3DS this is the VFP coprocessor inside each core. See [CTR CPU](ctr/hardware/cpu.md).

### Thread {#thread}

One sequence of instructions the operating system schedules onto a CPU core. A [process](#process) contains one or more threads. See [CTR kernel](ctr/software/kernel.md).

### Process {#process}

A running program with its own private memory and handle table, isolated from other processes by the [kernel](#kernel). See [CTR kernel](ctr/software/kernel.md).

### Userland {#userland}

Everything that runs without full hardware privileges: games and most system services. Contrast with the [kernel](#kernel). A "userland exploit" is a bug that lets an attacker run their own code at this level. See [CTR exploits](ctr/homebrew/exploits.md).

### System call (syscall) {#syscall}

The controlled doorway a [userland](#userland) program uses to ask the [kernel](#kernel) to do something privileged, such as allocating memory or creating a thread. See [CTR kernel](ctr/software/kernel.md).

### Inter-process communication (IPC) {#ipc}

How [processes](#process) send requests and replies to each other, for example a game asking a system service to read a file. See [CTR services](ctr/software/services.md).

### Microkernel {#microkernel}

A kernel design that keeps only the essentials (memory, scheduling, messaging) in the privileged core and pushes everything else into ordinary [processes](#process). See [CTR OS](ctr/software/os.md).

### Framebuffer {#framebuffer}

A block of memory holding the pixels of one image, ready to be drawn by the GPU or sent to a screen. See [CTR GPU](ctr/hardware/gpu.md) and [CTR displays](ctr/hardware/display.md).

### Shader {#shader}

A small program that runs on the [GPU](#gpu) to process vertices (corners of shapes) or pixels. See [CTR GPU](ctr/hardware/gpu.md).

### GDB (GNU debugger) {#gdb}

A standard command-line debugger that can stop a program, show its memory and registers, and step through it. It can debug a program on another machine through a small "stub" that speaks GDB's remote protocol over a network or cable. See [CTR debugging](ctr/engineering/debugging.md#gdb-with-rosalina).

### Texture tiling (swizzling) {#tiling}

Storing an image in small square blocks (on the 3DS, 8×8 pixels, with pixels inside a block in Z-order) instead of row by row, so that neighbouring pixels sit close together in memory for the GPU's cache. Images must be converted into this layout before the GPU can use them as textures. See [CTR asset pipeline](ctr/engineering/asset-pipeline.md).

### Hardware performance counters {#performance-counters}

Counters built into a processor that count events such as clock cycles, cache misses or mispredicted branches while code runs. They show *why* code is slow rather than only *how long* it takes. See [CTR CPU optimisation](ctr/engineering/cpu-optimization.md#hardware-performance-counters).

### ADPCM {#adpcm}

Adaptive differential pulse-code modulation: a family of lossy audio formats that store about 4 bits per sample by encoding the difference from a predicted value. Nintendo's DSP-ADPCM variant, used since the GameCube, can be decoded by the 3DS's audio DSP. See [CTR audio](ctr/engineering/audio.md#formats).

### DSP (digital signal processor) {#dsp}

A processor specialised for streaming number crunching such as audio mixing. See [CTR audio](ctr/hardware/audio.md).

### Microcontroller (MCU) {#mcu}

A tiny, self-contained computer chip that handles low-level chores (power, LEDs, sensors) so the main processor doesn't have to. See [CTR I/O](ctr/hardware/io-and-peripherals.md).

### I²C and SPI {#i2c-spi}

Two common short-distance serial wires between chips. A processor ("master") talks to many small peripherals over them. See [CTR I/O](ctr/hardware/io-and-peripherals.md).

### eMMC {#emmc}

Flash storage soldered to the board that presents itself like an SD card. It is the 3DS's internal [NAND](#nand). See [CTR NAND filesystem](ctr/software/nand-filesystem.md).

### SD card {#sd-card}

Removable flash card. The 3DS uses it for downloaded games and user data. See [CTR SD filesystem](ctr/software/sd-filesystem.md).

### FAT filesystem {#fat}

The simple, widely supported file layout (FAT12/16/32) used on SD cards and inside 3DS internal storage. See [CTR NAND filesystem](ctr/software/nand-filesystem.md).

### One-time-programmable memory (OTP) {#otp}

Memory that can be written exactly once at the factory. Consoles use it for per-device secrets. See [CTR security hardware](ctr/hardware/security-hardware.md).

### AES, RSA, SHA, ECDSA {#crypto}

Standard cryptographic building blocks. **AES** encrypts data with a shared secret key; **RSA** and **ECDSA** are public-key signature schemes (anyone can check a signature, only the key holder can make one); **SHA** produces a fingerprint ("hash") of data. See [CTR security architecture](ctr/software/security-architecture.md).

### Digital signature and chain of trust {#chain-of-trust}

A signature proves data came from the key holder and wasn't altered. A **chain of trust** is a sequence in which each stage verifies the next, anchored in the [boot ROM](#boot-rom). See [CTR security architecture](ctr/software/security-architecture.md).

### Exploit {#exploit}

A flaw that can be abused to make a device do something its maker didn't intend, such as running unsigned code. See [CTR exploits](ctr/homebrew/exploits.md).

### Return-oriented programming (ROP) {#rop}

An exploitation technique that chains tiny existing snippets of a program's own code to do the attacker's work, because the attacker can't inject new executable code. See [CTR exploits](ctr/homebrew/exploits.md).

### Stereoscopic 3D {#stereoscopic-3d}

Showing each eye a slightly different image so the brain perceives depth. See [CTR displays](ctr/hardware/display.md).

### Vertical blank (V-blank) {#vblank}

The short pause between finishing one screen refresh and starting the next; the safe moment to swap which framebuffer is shown. See [CTR displays](ctr/hardware/display.md).

### Lookup table (LUT) {#lut}

A table of precomputed answers: instead of calculating a result, hardware or software uses the input as an index and reads the answer. The 3DS GPU uses LUTs for lighting, fog and procedural textures. See [CTR GPU shading features](ctr/hardware/gpu-shading-features.md).

### Thread-local storage (TLS) {#thread-local-storage}

A small memory page private to each thread. On the 3DS, offset `0x80` holds the IPC command buffer and `0x180` the static buffer table. See [IPC and services](ctr/engineering/ipc-and-services.md).

### Handle {#handle}

A number a process uses to refer to a kernel object it owns, such as a session or memory block. It is released with `svcCloseHandle`. See [IPC and services](ctr/engineering/ipc-and-services.md#session-lifetime-and-closing).

### Root certificate authority (root CA) {#root-ca}

The top of a certificate chain. A TLS client accepts a server only if the server's chain ends at a root the client trusts. See [networking](ctr/engineering/networking.md#certificate-validation).

### Transport Layer Security (TLS) {#tls-protocol}

The encryption layer under HTTPS. See [networking](ctr/engineering/networking.md).

### System tick {#system-tick}

A 64-bit counter of CPU ticks since power-on, read with `svcGetSystemTick()`. On the 3DS it runs at the fixed ARM11 base rate of 268,111,856 Hz on every model, regardless of the New 3DS clock mode. It is not the same as the CPU's own cycle counter. See [CPU optimisation](ctr/engineering/cpu-optimization.md#counters-versus-the-system-tick).

### Non-blocking I/O {#non-blocking}

A socket mode where a call returns at once with "would block" instead of waiting for data. Used with `poll` to implement timeouts. See [networking](ctr/engineering/networking.md).

### Translation lookaside buffer (TLB) {#tlb}

A small cache of recent virtual-to-physical address translations. See [ARM11 MPCore internals](ctr/hardware/arm11-mpcore.md#address-translation-mmu).

### MESI {#mesi}

A cache-coherency protocol where each cache line is Modified, Exclusive, Shared or Invalid. See [ARM11 MPCore internals](ctr/hardware/arm11-mpcore.md#snoop-control-unit-scu).

### Address space identifier (ASID) {#asid}

An 8-bit tag so translation entries from different processes can coexist in the TLB. See [ARM11 MPCore internals](ctr/hardware/arm11-mpcore.md#address-translation-mmu).
