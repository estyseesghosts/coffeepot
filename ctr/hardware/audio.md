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
| Output | Stereo, 16-bit, **32,728.5 Hz** (`libctru`: `NDSP_SAMPLE_RATE = SYSCLOCK_SOC / 512`); 24 voices | — |
| Working RAM | 512 KB, double-buffered (2 × 256 KB) | — |
| DMA | Dedicated | — |
| Registers | `0x10203000` (DSP), `0x10103000` (CSND), `0x10145000` (I2S lines) | — |

> Conflict: DSP origin. Copetti says the same DSP was previously bundled with the Nintendo DSi as an optional accelerator; 3dbrew's Hardware page only describes the 3DS DSP (CEVA TeakLite, 134 MHz) and says nothing about the DSi, though its IO Registers page refers to a "DSi XpertTeak" section in the no$gba documentation for the DSP registers. This page does not assert the DSi origin; the XpertTeak reference is weak support for it. Unresolved.

Confirmed by 3dbrew IO Registers: the CSND register block is at `0x10103000` and the DSP register block at `0x10203000`.

### DSP firmware, shared memory and services

The firmware file format (`DSP1` header, up to 10 segments, signature), the two alternating 32 KB shared regions (24 input configurations, status, output and intermediate mix buffers, delay/reverb effects), the optional AAC decoder and the full `dsp::DSP` command list are on the [DSP firmware interface page](dsp.md). In short: the DSP firmware mixes 24 inputs per 160-sample frame and the ARM11 exchanges data with it through those regions, pipes and semaphores.

### Speaker and case

Copetti notes that the original Nintendo 3DS did not play well with its speakers: Nintendo published troubleshooting guidance for buzzing noises and fluctuations when the 3D slider moved, which Copetti attributes to the case design (Old 3DS; the source does not say whether later models were affected).

### Backwards-compatible CSND block

CSND is the DS-style sound block (32 channels, double the DS), with registers at `0x10103000`. DS, DSi and GBA software relies on it to reproduce their hardware, and 3DS software can use it as an extension of the DSP. The DSP and CSND work independently and both output to the speaker, and early homebrew used CSND while the DSP was being reverse engineered. `0x10103000` is also used by the Codec, CSND and DSP services per 3dbrew. The sound output lines (I2S) are at `0x10145000` and the codec/microphone chips sit on the SPI buses ([I/O](io-and-peripherals.md)).

> Unknown: the sources do not say whether the DSP or CSND hardware, or their firmware images, differ on the New 3DS.

## Homebrew notes

`libctru` ships `ndsp`, a userland driver for the DSP; its `NDSP_SAMPLE_RATE` and channel/filter functions (ADPCM, PCM8/PCM16 buffers, auxiliary output callbacks) mirror the voice model described above. [Userland homebrew](../homebrew/exploits.md) historically had no DSP access, and the hacking workflow even asks users to dump the Home Menu's DSP firmware so homebrew can load it.

## Open questions

- Sample rate: Copetti says "up to 32 kHz" and 3dbrew "32728 Hz" (Hardware page: 24 channels); the libctru definition (16,756,991 Hz ÷ 512 = 32,728.498 Hz) fixes the exact value used by the output.
- DSP architecture is only known through emulator work (the "Teakra" project); no official documentation is public.

## Sources

- [3dbrew: DSP Services](https://www.3dbrew.org/wiki/DSP_Services), [DSP Binary](https://www.3dbrew.org/wiki/DSP_Binary), [DSP Memory Region](https://www.3dbrew.org/wiki/DSP_Memory_Region), [DSP AAC Decoder](https://www.3dbrew.org/wiki/DSP_AAC_Decoder) — service commands, firmware header, shared-memory layout, AAC (documented in full on [DSP firmware interface](dsp.md)).
- [3dbrew: Hardware](https://www.3dbrew.org/wiki/Hardware) — DSP part and clock.
- [3dbrew: IO Registers](https://www.3dbrew.org/wiki/IO_Registers) — DSP/CSND register addresses.
- [libctru `ndsp.h`](https://github.com/devkitPro/libctru) — sample rate definition and the userland DSP driver API.
- [Rodrigo Copetti, Nintendo 3DS Architecture: Audio](https://www.copetti.org/writings/consoles/nintendo-3ds/) — DSP vs CSND, memory, channel counts.
