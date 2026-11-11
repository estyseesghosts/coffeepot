# CTR DSP firmware interface

The 3DS plays almost all of its sound through a separate audio processor, the **[DSP](../../glossary.md#dsp)**. It has no program of its own: every program that wants sound uploads a small firmware (a "component") to it, and from then on talks to that firmware through two shared memory windows, a few mailbox registers and byte pipes. This page documents those interfaces exactly: the firmware file, the shared-memory layout the mixer reads and writes, the optional AAC decoder, and the `dsp::DSP` service commands. The hardware overview (what the DSP is, how it relates to the old CSND block) is on the [audio page](audio.md); how to use it from a program is on [audio programming with ndsp](../engineering/audio.md).

Everything here is `[reverse-engineered]` from 3dbrew's pages (no official DSP documentation is public) and "may vary depending on the exact DSP firmware used". The sources do not say whether anything differs on a **New 3DS**.

> Unknown: whether the New 3DS runs the same DSP firmware images with the same layouts. The 3dbrew pages used here do not mention a model difference.

## Where it fits

The DSP is in the [SoC](../../glossary.md#soc), with registers at physical `0x10203000` and a 512 KB working RAM; the ARM11 sees DSP program RAM from `0x1FF00000` and data RAM from `0x1FF40000` (see [memory](memory.md)). An application never touches the registers: the OS service `dsp::DSP` ([services](../software/services.md)) loads the component, exposes the memory windows and relays messages. libctru's `ndsp` is the usual client.

## How it works

1. The program (or the system) calls `LoadComponent` with a DSP firmware image; the OS copies its segments into DSP program and data RAM and starts it.
2. The firmware starts writing audio frames. One **frame** is 160 stereo samples (about 4.9 ms). Each frame the DSP reads a set of input descriptions from one of two shared 32 KB regions and writes mixed output samples into the same region.
3. The program fills the *other* region for the next frame; the regions alternate like a double buffer. Which one is current is decided by their frame counters.
4. Extra services (AAC decoding, some state) travel over numbered **pipes**; the addresses of all shared structures are read from pipe channel 2 at start-up.

## Technical details

### DSP binary (`*.cdc`) layout

| Offset | Size | Field |
| ------ | ---- | ----- |
| `0x000` | `0x100` | RSA signature over bytes `0x100`-`0x2FF` |
| `0x100` | 4 | Magic `DSP1` |
| `0x104` | 4 | Binary size |
| `0x108` | 2 | Memory layout: bits 7:0 program RAM, bits 15:8 data RAM. Each bit stands for one 0x8000-byte region; the first is at `0x1FF00000`, the next at `0x1FF08000`, and so on. The hardware registers that configure DSP memory are `CFG11_SHAREDWRAM_32K_DATA` and `CFG11_SHAREDWRAM_32K_CODE` at physical `0x10140000` (virtual `0x1EC40000`) |
| `0x10C` | 1 | Unknown |
| `0x10D` | 1 | Special-segment memory type (`0` or `1` = program RAM at `0x1FF00000`, `2` = data RAM at `0x1FF40000`) |
| `0x10E` | 1 | Number of segments (must be 1 to 10) |
| `0x10F` | 1 | Flags: bit 0 = the DSP module calls `DSP:RecvData` on all three registers and expects each to reply `1`; bit 1 = load the special segment |
| `0x110` | 4 | Special segment start address, in 16-bit words |
| `0x114` | 4 | Special segment size in bytes |
| `0x118` | 8 | Zero |
| `0x120` | `0x30` x 10 | Segment records |

Each segment record:

| Offset | Size | Field |
| ------ | ---- | ----- |
| `0x00` | 4 | Offset of the segment data in the file |
| `0x04` | 4 | Start address in 16-bit words (must be below `0x20000` for type 0, below `0x10000` for types 1 and 2) |
| `0x08` | 4 | Size in bytes |
| `0x0F` | 1 | Memory type (`0` or `1` = program RAM at `0x1FF00000`, `2` = data RAM at `0x1FF40000`) |
| `0x10` | 32 | SHA-256 of the segment |

If the special-segment flag is set, `0x214` bytes are read from `CfgS:GetConfigInfoBlk8` block `0x70000` and copied into that segment (zeroes if the read fails). Its purpose is not known. Dumped images normally end in `.cdc`; the Luma3DS "Dump DSP firmware" menu writes `/3ds/dspfirm.cdc` (see [audio programming](../engineering/audio.md)).

### Shared memory regions

Two regions of `0x8000` bytes at `0x1FF50000` and `0x1FF70000`. Each holds 15 structures; their locations are obtained by reading channel 2 of the DSP pipe, in this order:

| # | Structure | Size / count | Written by |
| - | --------- | ------------ | ------ |
| 1 | Frame count | size not given | not stated |
| 2 | Input configurations | 24 x 192 bytes | program (implied) |
| 3 | Input status | 24 x 12 bytes | DSP |
| 4 | Input ADPCM coefficients | 24 x 32 bytes | program (implied) |
| 5 | DSP configuration | includes the delay and reverb effects | program (implied) |
| 6 | DSP status | 32 bytes | DSP |
| 7 | Output samples | 640 bytes | DSP |
| 8 | Intermediate mix samples | see conflict below | read/write |
| 9 | Compressor table | precomputed response curve | not stated |
| 10 | DSP debug statistics | size not given | not stated |
| 11-13 | Unknown coefficients (three blocks) | size not given | not stated |
| 14, 15 | Surround-sound biquad filters 1 and 2 | size not given | not stated |

**Frame count.** The first region's count must be even and the second's odd. The region with the higher count is the "current region". The DSP firmware only responds if the first frame count is 4 (as written on 3dbrew; the meaning of that start value is not explained).

The DSP has 24 inputs (voices), each individually configurable. Each input produces three sets of four audio channels (two left, two right) that feed three intermediate mixers; two of the mixers are used for effects and the auxiliary output. The DSP's internal format is four-channel (quadraphonic).

#### Input configuration (192 bytes)

| Offset | Type | Field |
| ------ | ---- | ----- |
| `0` | u32 | Dirty flags (tell the DSP which fields changed) |
| `4` | f32[3][2][2] | Input gain (12 channels: three mixers x two sets x left/right) |
| `52` | f32 | Rate multiplier (`1.0` = native DSP rate) |
| `56` | u8 | Interpolation mode |
| `57` | u8 | Polyphase filter select |
| `58` | u16 | bit 0 simple filter enabled; bit 1 biquad filter enabled |
| `60` | SimpleFilter (4 bytes) | One-pole filter |
| `64` | BiquadFilter (10 bytes) | Two-pole, two-zero filter |
| `74` | u16 | Bitmap of dirty buffers in the queue |
| `76` | Buffer[4] (4 x 20 bytes) | Buffer queue |
| `156` | u32 | Unknown |
| `160` | u16 | Is active |
| `162` | u16 | Sync count |
| `164` | u32 | Play position |
| `168` | 4 bytes | Unknown |
| `172` | u32 | Physical address of the embedded buffer |
| `176` | u32 | Sample count of the embedded buffer |
| `180` | u16 | Format (below) |
| `182` | AdpcmData (6 bytes) | ADPCM state for the embedded buffer |
| `188` | u16 | bit 0 ADPCM updated; bit 1 is looping |
| `190` | u16 | Buffer ID of the embedded buffer |

There is a two-sample delay in this preprocessing stage, likely from interpolation.

| Format bits | Meaning |
| ----------- | ------- |
| 1:0 | Channels: `0`, `1`, `3` = mono; `2` = stereo |
| 3:2 | Codec: `0` PCM8, `1` PCM16, `2` [ADPCM](../../glossary.md#adpcm) |
| 5 | Fade |

Buffer (20 bytes): `+0` u32 physical address, `+4` u32 sample count, `+8` AdpcmData (6 bytes), `+14` u8 ADPCM data dirty, `+15` u8 looping, `+16` u16 buffer ID, `+18` unused.

AdpcmData (6 bytes): `+0` u8 predictor/scale, `+1` unused, `+2` s16 y[n-1], `+4` s16 y[n-2].

SimpleFilter: `+0` s1.15 b0, `+2` s1.15 negated a1. BiquadFilter: `+0` s2.14 negated a2, `+2` negated a1, `+4` b2, `+6` b1, `+8` b0 (all s2.14).

#### Other structures

| Structure | Layout |
| --------- | ------ |
| Input status (12 bytes, read-only) | `0` u8 input enabled; `1` u8 buffer-ID dirty flag (set to 1 when a buffer after the first starts playing); `2` u16 sync count; `4` u32 position (samples into the current buffer); `8` u16 ID of the buffer that just started; `10` unused |
| Input ADPCM coefficients (32 bytes) | 16 x s5.11 coefficients |
| Delay effect | Delay with feedback; length counted in audio frames (160 samples each); the feedback arm only has a gain, with a one-pole filter under it |
| Reverb effect | Two comb filters and one all-pass filter in the standard arrangement |
| DSP status (32 bytes, read-only) | `0` u16 unknown; `2` u16 number of dropped frames; `4` 28 bytes unknown |
| Output samples (640 bytes, read-only) | `0` s16[160] left channel; `320` s16[160] right channel (separate arrays; two speakers) |
| Intermediate mix samples (read/write) | 160 quadraphonic samples, left/right interleaved; also the auxiliary path that lets the ARM11 apply custom effects. A sample is four s32: left A, right A, left B, right B |
| Compressor table | Precomputed response curve for the compressor |

> Conflict: the size of the intermediate mix structure. 3dbrew's *DSP Memory Region* page says "This structure is 5120 bytes long" but defines it as 160 samples of a 16-byte quadraphonic sample, which is 2560 bytes. This page does not choose; it records both. Possible reasons (for example, the 5120 bytes covering two of the three mixers) are not stated in the source. Unresolved: a read of the real structure size from the pipe's address table would settle it.

### AAC decoder

Some DSP firmware images contain an AAC codec (dumped from titles such as Pokemon X/Y or Rhythm Heaven Megamix); other images do not, and behaviour "may vary depending on the exact DSP firmware used". Requests and responses travel over the DSP binary pipe (pipe 3), using one 0x20-byte message in both directions.

| Offset | Type | Field |
| ------ | ---- | ----- |
| `0x00` | u16 | Mode: `0` none, `1` decode AAC, `2` encode AAC (3dbrew marks these as "seems to be") |
| `0x02` | u16 | Decoder command: `0` Initialize, `1` Decode/Encode, `2` Shutdown, `3` LoadState, `4` SaveState |
| `0x04` | u32 | Result code |
| `0x08` | 24 bytes | Request or response data |

Decode request data: `+0x00` physical address of the input AAC stream; `+0x04` its size in bytes; `+0x08` physical address for decoded PCM16 output (the left channel for stereo); `+0x0C` physical address of the right channel; `+0x10` and `+0x14` unknown.

Decode response data: `+0x00` sample-rate enum; `+0x04` channel count (?); `+0x08` size; `+0x0C`, `+0x10` unknown; `+0x14` number of decoded samples (?).

| Sample-rate enum | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
| ---------------- | - | - | - | - | - | - | - | - | - |
| Hz | 48000 | 44100 | 32000 | 24000 | 22050 | 16000 | 12000 | 11025 | 8000 |

3dbrew notes that parts of the AAC interface still need testing and reverse engineering; fields marked "?" are guesses there.

### `dsp::DSP` service commands

The main audio service. The CSND service can also play audio, and DSP and CSND hardware can run at the same time, but most processes use `dsp::DSP`.

| Command header | Name |
| -------------- | ---- |
| `0x00010040` | RecvData |
| `0x00020040` | RecvDataIsReady |
| `0x00030080` | SendData |
| `0x00040040` | SendDataIsEmpty |
| `0x000500C2` | SendFifoEx |
| `0x000600C0` | RecvFifoEx |
| `0x00070040` | SetSemaphore |
| `0x00080000` | GetSemaphore |
| `0x00090040` | ClearSemaphore |
| `0x000A0040` | MaskSemaphore |
| `0x000B0000` | CheckSemaphoreRequest |
| `0x000C0040` | ConvertProcessAddressFromDspDram |
| `0x000D0082` | WriteProcessPipe |
| `0x000E00C0` | ReadPipe |
| `0x000F0080` | GetPipeReadableSize |
| `0x001000C0` | ReadPipeIfPossible |
| `0x001100C2` | LoadComponent |
| `0x00120000` | UnloadComponent |
| `0x00130082` | FlushDataCache |
| `0x00140082` | InvalidateDCache |
| `0x00150082` | RegisterInterruptEvents |
| `0x00160000` | GetSemaphoreEventHandle |
| `0x00170040` | SetSemaphoreMask |
| `0x00180040` | GetPhysicalAddress |
| `0x00190040` | GetVirtualAddress |
| `0x001A0042` | SetIirFilterI2S1 (wrapper for `cdc:DSP` command 1) |
| `0x001B0042` | SetIirFilterI2S2 (wrapper for `cdc:DSP` command 2) |
| `0x001C0082` | SetIirFilterEQ (wrapper for `cdc:DSP` command 3) |
| `0x001D00C0` | ReadMultiEx_SPI2 (wrapper for `cdc:DSP` command 4) |
| `0x001E00C2` | WriteMultiEx_SPI2 (wrapper for `cdc:DSP` command 5) |
| `0x001F0000` | GetHeadphoneStatus |
| `0x00200040` | ForceHeadphoneOut |
| `0x00210000` | GetIsDspOccupied |

`SendFifoEx`/`RecvFifoEx` carry the 16-bit-word argument signatures `(unsigned short, unsigned short const*, unsigned int, unsigned int)` and `(unsigned short*, unsigned short, unsigned int, unsigned int)` on 3dbrew. For how commands are packed see [IPC and services](../engineering/ipc-and-services.md).

## Homebrew notes

Use `ndsp` from libctru rather than these commands directly; it already handles the alternating regions and the frame counters. Homebrew needs a dumped firmware (`/3ds/dspfirm.cdc`) because it cannot rely on the system's copy; details in [audio programming](../engineering/audio.md#before-you-start).

## Open questions

- Sizes and contents of structures 5, 9, 10 and 11 to 15; the unknown fields marked above.
- The meaning of the "first frame count must be 4" rule.
- The intermediate mix structure size (conflict above).
- Whether the New 3DS differs (see the Unknown above).

## Sources

- [3dbrew: DSP Binary](https://www.3dbrew.org/wiki/DSP_Binary) — firmware header, segment records, memory layout bits.
- [3dbrew: DSP Memory Region](https://www.3dbrew.org/wiki/DSP_Memory_Region) — shared structures, input configuration, filters, output and intermediate samples.
- [3dbrew: DSP AAC Decoder](https://www.3dbrew.org/wiki/DSP_AAC_Decoder) — AAC message format and sample-rate enum.
- [3dbrew: DSP Services](https://www.3dbrew.org/wiki/DSP_Services) — `dsp::DSP` command list.
