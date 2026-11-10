# CTR audio

The 3DS has two sound systems. The main one is a dedicated audio processor — a **[DSP](../../glossary.md#dsp)** — that games upload a small Nintendo-written program to and then control through system services. The second is a leftover from the Nintendo DS, called **[CSND](../glossary.md#csnd)**, which DS and GBA games use and which very early homebrew used before the DSP was understood.

Both feed the same speakers and headphone jack.

## Where it fits

The DSP lives inside the [SoC](../../glossary.md#soc) with its own 512 KB of [memory](memory.md). The [ARM11](cpu.md) exchanges data with it through shared memory windows, a small mailbox, and the `dsp::DSP` [service](../software/services.md). Nothing talks to the DSP directly in retail games; the OS and the uploaded firmware mediate everything.

## How it works

At startup a 3DS program loads a **DSP firmware** (the same one used by the Home Menu can be reused) into the DSP. The program then talks to the firmware with two shared 32 KB memory areas, alternating between them each audio frame, plus pipes and semaphores via the service.

The firmware does the mixing: up to 24 independent input voices that can play ADPCM or PCM samples, with filters, volume/panning, effects (delay, reverb), sequencing and a compressor. The output is stereo 16-bit audio at about 32 kHz. The DSP firmware can also include an AAC decoder.

CSND has 32 channels (double the DS) and is also used by 3DS software as an extension of the DSP.

## Technical details

| Item | DSP | CSND |
| ---- | --- | ---- |
| Hardware | CEVA TeakLite (II), ~134 MHz (134.06 MHz, the ARM9 clock) | DS audio block, 32 channels |
| Used by | 3DS games and apps | DS/DSi/GBA games; early homebrew; 3DS as extension |
| Output | Stereo, 16-bit, 32 kHz class (3dbrew lists 32728 Hz, 24 channels) | — |
| Working RAM | 512 KB, double-buffered (2 × 256 KB) | — |
| DMA | Dedicated | — |
| Registers | `0x10203000` (DSP), `0x10103000` (CSND) | — |

The DSP was first introduced as an optional accelerator in the Nintendo DSi; here it is the designated audio processor.

### DSP firmware (component binary)

A DSP binary starts with a 0x100-byte RSA signature over bytes `0x100–0x300`, then:

| Offset | Size | Field |
| ------ | ---- | ----- |
| `0x100` | 4 | Magic `DSP1` |
| `0x104` | 4 | Binary size |
| `0x108` | 2 | Memory layout (bits 0–7 program RAM, 8–15 data RAM; each bit selects a 0x8000-byte bank starting at `0x1FF00000`) |
| `0x10D` | 1 | Special-segment memory type |
| `0x10E` | 1 | Segment count (1–10) |

Hardware configuration registers for DSP memory are `CFG11_SHAREDWRAM_32K_DATA` and `…_CODE`.

### Shared data layout

The firmware alternates between two 0x8000-byte areas (at `0x1FF50000` and `0x1FF70000`). Each holds, in order: frame count, input configurations, input status, input ADPCM coefficients, DSP configuration, DSP status, output samples, intermediate mix samples, compressor table, debug statistics, three unknown coefficient blocks and two surround biquad filters. The 24 inputs each produce three sets of four audio channels (two left, two right) that feed three intermediate mixers; two mixers are used for effects (delay, reverb) and aux. The detailed field layouts are on 3dbrew's *DSP Memory Region* page.

### Services

`dsp::DSP` exposes `RecvData`/`SendData`, `SendFifoEx`/`RecvFifoEx`, a set of semaphore commands, `ConvertProcessAddressFromDspDram`, and pipe commands (`WriteProcessPipe`, `ReadPipe`, `GetPipeReadableSize`, `ReadPipeIfPossible`, …).

## Homebrew notes

[Userland homebrew](../homebrew/exploits.md) historically had no DSP access, and the hacking workflow even asks users to dump the Home Menu's DSP firmware so homebrew can load it. The original 3DS also had hardware speaker noise issues that Nintendo documented separately.

## Open questions

- Sample rate: 3dbrew says 32728 Hz, Copetti says "up to 32 kHz". The exact figure is not confirmed here.
- DSP architecture is only known through emulator work (the "Teakra" project); no official documentation is public.

## Sources

- [3dbrew: DSP Services](https://www.3dbrew.org/wiki/DSP_Services), [DSP Binary](https://www.3dbrew.org/wiki/DSP_Binary), [DSP Memory Region](https://www.3dbrew.org/wiki/DSP_Memory_Region), [DSP AAC Decoder](https://www.3dbrew.org/wiki/DSP_AAC_Decoder) — service commands, firmware header, shared-memory layout.
- [3dbrew: Hardware](https://www.3dbrew.org/wiki/Hardware) — DSP part and clock.
- [3dbrew: IO Registers](https://www.3dbrew.org/wiki/IO_Registers) — DSP/CSND register addresses.
- [Rodrigo Copetti, Nintendo 3DS Architecture: Audio](https://www.copetti.org/writings/consoles/nintendo-3ds/) — DSP vs CSND, memory, channel counts.
