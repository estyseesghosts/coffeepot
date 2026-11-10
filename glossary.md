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

Unofficial software written by hobbyists to run on a console: tools, emulators, games. Homebrew pages live under each console's `homebrew/` directory (planned — not yet scaffolded).

### Custom firmware (CFW) {#cfw}

Modified system software that enables [homebrew](#homebrew) and extra features. Per-console CFW coverage is planned under each console's `homebrew/` directory.
