# Global glossary

Terms shared across two or more consoles, or general computing terms. Per-console terms live in `ctr/glossary.md` and `nx/glossary.md`. Glossary entries are pointers — the full story lives on the linked wiki page.

<a name="cpu"></a>

### CPU (central processing unit)
The processor that runs program instructions. Each console has one or more CPUs with distinct roles (running games vs. running system software). See [CTR CPU](ctr/hardware/cpu.md) and [NX CPU](nx/hardware/cpu.md).

<a name="gpu"></a>

### GPU (graphics processing unit)
The hardware that draws 2D/3D images to the screen. Consoles pair fixed-function blocks with programmable stages in different mixes. See [CTR GPU](ctr/hardware/gpu.md) and [NX GPU](nx/hardware/gpu.md).

<a name="soc"></a>

### System-on-a-chip (SoC)
A single chip combining CPU, GPU, memory interfaces, and peripherals, rather than separate chips for each. Both consoles in this wiki are SoC-based designs.

<a name="arm"></a>

### ARM
The family of processor architectures used by both consoles in this wiki. Specific implementations (which ARM core, how many, at what clocks) differ per console — see the per-device CPU pages.

<a name="kernel"></a>

### Kernel
The privileged core of the [operating system](#os) that controls hardware access, memory, and scheduling. Exploiting or bypassing the kernel is central to most homebrew stories. See [CTR OS](ctr/software/os.md) and [NX OS](nx/software/os.md).

<a name="os"></a>

### Operating system (OS)
The system software that manages hardware and runs games and apps. Both consoles in this wiki run Nintendo software Nintendo calls Horizon, but they are distinct implementations — see the per-device OS pages.

<a name="firmware"></a>

### Firmware
Low-level software stored on the device that starts the system and loads the [operating system](#os): boot ROM, bootloaders, and versioned system partitions. See [CTR firmware](ctr/software/firmware.md) and [NX firmware](nx/software/firmware.md).

<a name="horizon"></a>

### Horizon
Nintendo's internal name for the [operating system](#os) on both the 3DS and the Switch — two distinct implementations sharing a name and some design ideas. This wiki always says which one it means and links to [CTR OS](ctr/software/os.md) or [NX OS](nx/software/os.md).

<a name="memory"></a>

### Memory
Temporary storage the [CPU](#cpu) and [GPU](#gpu) work in while running. How memory is split, shared, and protected differs per console — details live on the per-device pages.

<a name="storage-media"></a>

### Storage media
Where games and system software live when powered off: cartridges, internal flash, and SD cards. Per-console formats and filesystems are covered on the per-device firmware and OS pages.

<a name="homebrew"></a>

### Homebrew
Unofficial software written by hobbyists to run on a console: tools, emulators, games. See [CTR homebrew history](ctr/homebrew/history.md).

<a name="cfw"></a>

### Custom firmware (CFW)
Modified system software that enables [homebrew](#homebrew) and extra features. See [CTR custom firmware](ctr/homebrew/custom-firmware.md).

<a name="bootloader"></a>

### Bootloader
A small program that runs first at power-on and loads the next, larger piece of software. Consoles chain several of these, starting from an unchangeable [boot ROM](#boot-rom). See [CTR boot process](ctr/software/boot-process.md).

<a name="boot-rom"></a>

### Boot ROM
Code burned permanently into the processor chip at the factory. It is the first thing to run after power-on and cannot be updated, so flaws in it are permanent. See [CTR boot process](ctr/software/boot-process.md).

<a name="nand"></a>

### NAND (flash memory)
Non-volatile flash memory that keeps its contents with the power off; consoles use it for system software and settings. On the 3DS it is an eMMC chip (see [CTR NAND filesystem](ctr/software/nand-filesystem.md)).

<a name="cache"></a>

### Cache
A small, fast memory next to a CPU that holds recently used data so the CPU rarely waits on slower main memory. Caches can be per-core (level 1, "L1") or shared (level 2, "L2"). See [CTR CPU](ctr/hardware/cpu.md).

<a name="dma"></a>

### Direct memory access (DMA)
A hardware block that copies data between memories or devices without the CPU doing the copying. See [CTR memory](ctr/hardware/memory.md).

<a name="mmu"></a>

### Memory management unit (MMU)
CPU hardware that translates the addresses programs use (**virtual addresses**) into real memory locations (**physical addresses**) and enforces who may read, write, or run each page. See [CTR CPU](ctr/hardware/cpu.md).

<a name="mpu"></a>

### Memory protection unit (MPU)
A simpler cousin of the [MMU](#mmu): it enforces access rules on a few fixed address regions but does no address translation. See [CTR CPU](ctr/hardware/cpu.md).

<a name="simd"></a>

### SIMD
"Single instruction, multiple data": one instruction that works on several values packed into one register at once. See [CTR CPU](ctr/hardware/cpu.md).

<a name="fpu"></a>

### Floating-point unit (FPU)
Hardware for arithmetic on fractional numbers (`float`, `double`). On the 3DS this is the VFP coprocessor inside each core. See [CTR CPU](ctr/hardware/cpu.md).

<a name="thread"></a>

### Thread
One sequence of instructions the operating system schedules onto a CPU core. A [process](#process) contains one or more threads. See [CTR kernel](ctr/software/kernel.md).

<a name="process"></a>

### Process
A running program with its own private memory and handle table, isolated from other processes by the [kernel](#kernel). See [CTR kernel](ctr/software/kernel.md).

<a name="userland"></a>

### Userland
Everything that runs without full hardware privileges: games and most system services. Contrast with the [kernel](#kernel). A "userland exploit" is a bug that lets an attacker run their own code at this level. See [CTR exploits](ctr/homebrew/exploits.md).

<a name="syscall"></a>

### System call (syscall)
The controlled doorway a [userland](#userland) program uses to ask the [kernel](#kernel) to do something privileged, such as allocating memory or creating a thread. See [CTR kernel](ctr/software/kernel.md).

<a name="ipc"></a>

### Inter-process communication (IPC)
How [processes](#process) send requests and replies to each other, for example a game asking a system service to read a file. See [CTR services](ctr/software/services.md).

<a name="microkernel"></a>

### Microkernel
A kernel design that keeps only the essentials (memory, scheduling, messaging) in the privileged core and pushes everything else into ordinary [processes](#process). See [CTR OS](ctr/software/os.md).

<a name="framebuffer"></a>

### Framebuffer
A block of memory holding the pixels of one image, ready to be drawn by the GPU or sent to a screen. See [CTR GPU](ctr/hardware/gpu.md) and [CTR displays](ctr/hardware/display.md).

<a name="shader"></a>

### Shader
A small program that runs on the [GPU](#gpu) to process vertices (corners of shapes) or pixels. See [CTR GPU](ctr/hardware/gpu.md).

<a name="gdb"></a>

### GDB (GNU debugger)
A standard command-line debugger that can stop a program, show its memory and registers, and step through it. It can debug a program on another machine through a small "stub" that speaks GDB's remote protocol over a network or cable. See [CTR debugging](ctr/engineering/debugging.md#gdb-with-rosalina).

<a name="tiling"></a>

### Texture tiling (swizzling)
Storing an image in small square blocks (on the 3DS, 8×8 pixels, with pixels inside a block in Z-order) instead of row by row, so that neighbouring pixels sit close together in memory for the GPU's cache. Images must be converted into this layout before the GPU can use them as textures. See [CTR asset pipeline](ctr/engineering/PICA200/asset-pipeline.md).

<a name="performance-counters"></a>

### Hardware performance counters
Counters built into a processor that count events such as clock cycles, cache misses or mispredicted branches while code runs. They show *why* code is slow rather than only *how long* it takes. See [CTR CPU optimisation](ctr/engineering/cpu-optimization.md#hardware-performance-counters).

<a name="pcm"></a>

### PCM (pulse-code modulation)
Uncompressed digital audio: a stream of sample values, one per channel per time step. On the Switch's `audout` it is signed 16-bit, 48,000 samples per second, two channels. See [audio on Switch homebrew](nx/engineering/audio.md).

<a name="adpcm"></a>

### ADPCM
Adaptive differential pulse-code modulation: a family of lossy audio formats that store about 4 bits per sample by encoding the difference from a predicted value. Nintendo's DSP-ADPCM variant, used since the GameCube, can be decoded by the 3DS's audio DSP. See [CTR audio](ctr/engineering/audio.md#formats).

<a name="dsp"></a>

### DSP (digital signal processor)
A processor specialised for streaming number crunching such as audio mixing. See [CTR audio](ctr/hardware/audio.md).

<a name="mcu"></a>

### Microcontroller (MCU)
A tiny, self-contained computer chip that handles low-level chores (power, LEDs, sensors) so the main processor doesn't have to. See [CTR I/O](ctr/hardware/io-and-peripherals.md).

<a name="i2c-spi"></a>

### I²C and SPI
Two common short-distance serial wires between chips. A processor ("master") talks to many small peripherals over them. See [CTR I/O](ctr/hardware/io-and-peripherals.md).

<a name="emmc"></a>

### eMMC
Flash storage soldered to the board that presents itself like an SD card. It is the 3DS's internal [NAND](#nand). See [CTR NAND filesystem](ctr/software/nand-filesystem.md).

<a name="sd-card"></a>

### SD card
Removable flash card. The 3DS uses it for downloaded games and user data. See [CTR SD filesystem](ctr/software/sd-filesystem.md).

<a name="fat"></a>

### FAT filesystem
The simple, widely supported file layout (FAT12/16/32) used on SD cards and inside 3DS internal storage. See [CTR NAND filesystem](ctr/software/nand-filesystem.md).

<a name="otp"></a>

### One-time-programmable memory (OTP)
Memory that can be written exactly once at the factory. Consoles use it for per-device secrets. See [CTR security hardware](ctr/hardware/security-hardware.md).

<a name="crypto"></a>

### AES, RSA, SHA, ECDSA
Standard cryptographic building blocks. **AES** encrypts data with a shared secret key; **RSA** and **ECDSA** are public-key signature schemes (anyone can check a signature, only the key holder can make one); **SHA** produces a fingerprint ("hash") of data. See [CTR security architecture](ctr/software/security-architecture.md).

<a name="chain-of-trust"></a>

### Digital signature and chain of trust
A signature proves data came from the key holder and wasn't altered. A **chain of trust** is a sequence in which each stage verifies the next, anchored in the [boot ROM](#boot-rom). See [CTR security architecture](ctr/software/security-architecture.md).

<a name="exploit"></a>

### Exploit
A flaw that can be abused to make a device do something its maker didn't intend, such as running unsigned code. See [CTR exploits](ctr/homebrew/exploits.md).

<a name="rop"></a>

### Return-oriented programming (ROP)
An exploitation technique that chains tiny existing snippets of a program's own code to do the attacker's work, because the attacker can't inject new executable code. See [CTR exploits](ctr/homebrew/exploits.md).

<a name="stereoscopic-3d"></a>

### Stereoscopic 3D
Showing each eye a slightly different image so the brain perceives depth. See [CTR displays](ctr/hardware/display.md).

<a name="vblank"></a>

### Vertical blank (V-blank)
The short pause between finishing one screen refresh and starting the next; the safe moment to swap which framebuffer is shown. See [CTR displays](ctr/hardware/display.md).

<a name="lut"></a>

### Lookup table (LUT)
A table of precomputed answers: instead of calculating a result, hardware or software uses the input as an index and reads the answer. The 3DS GPU uses LUTs for lighting, fog and procedural textures. See [CTR GPU shading features](ctr/hardware/gpu-shading-features.md).

<a name="thread-local-storage"></a>

### Thread-local storage (TLS)
A small memory page private to each thread. On the 3DS, offset `0x80` holds the IPC command buffer and `0x180` the static buffer table. See [IPC and services](ctr/engineering/ipc-and-services.md).

<a name="handle"></a>

### Handle
A number a process uses to refer to a kernel object it owns, such as a session or memory block. It is released with `svcCloseHandle`. See [IPC and services](ctr/engineering/ipc-and-services.md#session-lifetime-and-closing).

<a name="root-ca"></a>

### Root certificate authority (root CA)
The top of a certificate chain. A TLS client accepts a server only if the server's chain ends at a root the client trusts. See [networking](ctr/engineering/networking.md#certificate-validation).

<a name="tls-protocol"></a>

### Transport Layer Security (TLS)
The encryption layer under HTTPS. See [networking](ctr/engineering/networking.md).

<a name="system-tick"></a>

### System tick
A 64-bit counter of CPU ticks since power-on, read with `svcGetSystemTick()`. On the 3DS it runs at the fixed ARM11 base rate of 268,111,856 Hz on every model, regardless of the New 3DS clock mode. It is not the same as the CPU's own cycle counter. See [CPU optimisation](ctr/engineering/cpu-optimization.md#counters-versus-the-system-tick).

<a name="non-blocking"></a>

### Non-blocking I/O
A socket mode where a call returns at once with "would block" instead of waiting for data. Used with `poll` to implement timeouts. See [networking](ctr/engineering/networking.md).

<a name="tlb"></a>

### Translation lookaside buffer (TLB)
A small cache of recent virtual-to-physical address translations. See [ARM11 MPCore internals](ctr/hardware/arm11-mpcore.md#address-translation-mmu).

<a name="mesi"></a>

### MESI
A cache-coherency protocol where each cache line is Modified, Exclusive, Shared or Invalid. See [ARM11 MPCore internals](ctr/hardware/arm11-mpcore.md#snoop-control-unit-scu).

<a name="asid"></a>

### Address space identifier (ASID)
An 8-bit tag so translation entries from different processes can coexist in the TLB. See [ARM11 MPCore internals](ctr/hardware/arm11-mpcore.md#address-translation-mmu).
