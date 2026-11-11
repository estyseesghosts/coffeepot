# Audio programming with ndsp

Sound on the 3DS is produced by a separate audio processor (the **[DSP](../../glossary.md#dsp)**) running Nintendo's audio firmware. Homebrew talks to it through **ndsp**, libctru's driver: you queue buffers of samples on up to 24 channels, and the DSP resamples, mixes and plays them. The CPU's job is to keep those queues full. This page covers setting ndsp up (including the firmware file it needs), choosing formats and buffer sizes, streaming compressed music, the callback that runs every audio frame, and how to diagnose crackles and gaps.

Hardware background: [audio](../hardware/audio.md); shared-memory and firmware formats: [DSP firmware interface](../hardware/dsp.md). Buffer placement and flushing: [memory and cache](memory-and-cache.md#dsp-buffers).

## Before you start

- Checked against libctru 2.7.1 source (`ndsp/ndsp.h`, `ndsp/channel.h`, `source/ndsp/*`) and the `3ds-examples` audio projects. Fragments use real API names; nothing here has been measured on hardware.
- **ndsp needs the DSP firmware file.** `ndspInit()` loads `sdmc:/3ds/dspfirm.cdc` (or, failing that, a component handed over by the homebrew environment as `hb:ndsp`) and fails without it. Users create the file once with Luma3DS: Rosalina menu → *Miscellaneous options* → *Dump DSP firmware*, which writes `/3ds/dspfirm.cdc` [documented: libctru `source/ndsp/ndsp.c`, Luma3DS `menus/miscellaneous.c`]. Check `ndspInit`'s result and tell the user what to do if it fails.

## The model

- **Output**: about 32,728 Hz (`NDSP_SAMPLE_RATE` = `SYSCLOCK_SOC / 512`), produced in **sound frames of 160 samples**, roughly 4.89 ms each ([libctru](../homebrew/libctru.md#audio-sample-rate)).
- **Channels**: 24 (`0`–`23`). Each has its own sample format, sample rate, interpolation, volume mix and optional filters, and a queue of wave buffers.
- **Wave buffers** (`ndspWaveBuf`): a pointer to samples in **linear memory**, a sample count, an optional `looping` flag, and a `status` that moves `NDSP_WBUF_FREE` → `QUEUED` → `PLAYING` → `DONE`. When a channel's queue runs empty, it falls silent.
- **The ndsp thread**: libctru creates a detached thread at priority `0x18` (the highest userland priority) on the process's default core, with a **4 KiB stack**. It wakes every sound frame, calls your frame callback if you set one (`ndspSetCallback`), then updates the DSP [documented: `source/ndsp/ndsp.c`].

## Setting up a channel

```c
#include <3ds.h>
#include <string.h>

#define RATE        22050
#define SAMPLES     (RATE / 30)          // one buffer = 1/30 s
#define NBUF        3                    // buffers in rotation

static ndspWaveBuf wbuf[NBUF];
static s16 *pcm;                         // NBUF * SAMPLES stereo frames, linear memory

bool audio_init(void)
{
    if (R_FAILED(ndspInit())) return false;           // missing /3ds/dspfirm.cdc?
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);

    ndspChnReset(0);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, RATE);
    ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);
    float mix[12] = { 1.0f, 1.0f };                  // front left, front right
    ndspChnSetMix(0, mix);

    pcm = linearAlloc(NBUF * SAMPLES * 2 * sizeof(s16));
    if (!pcm) return false;
    memset(wbuf, 0, sizeof(wbuf));
    for (int i = 0; i < NBUF; i++) {
        wbuf[i].data_vaddr = &pcm[i * SAMPLES * 2];
        wbuf[i].nsamples = SAMPLES;                   // stereo frames, not bytes
    }
    return true;
}

/* Call often (every frame, or from a worker): refill finished buffers. */
void audio_pump(void (*generate)(s16 *dst, int frames))
{
    for (int i = 0; i < NBUF; i++) {
        if (wbuf[i].status != NDSP_WBUF_FREE && wbuf[i].status != NDSP_WBUF_DONE) continue;
        s16 *dst = (s16 *)wbuf[i].data_vaddr;
        generate(dst, SAMPLES);
        DSP_FlushDataCache(dst, SAMPLES * 2 * sizeof(s16));
        ndspChnWaveBufAdd(0, &wbuf[i]);
    }
}
```

This is the same structure as `3ds-examples/audio/streaming` with three buffers instead of two [documented]. On exit: `ndspChnWaveBufClear`, `ndspExit`, then free the buffers.

## Formats

| Format constant | Data | Notes |
| --------------- | ---- | ----- |
| `NDSP_FORMAT_MONO_PCM8`, `STEREO_PCM8` | signed 8-bit | Small, noisy |
| `NDSP_FORMAT_MONO_PCM16`, `STEREO_PCM16` | signed 16-bit, interleaved for stereo | The usual choice |
| `NDSP_FORMAT_MONO_ADPCM` | Nintendo DSP-**[ADPCM](../../glossary.md#adpcm)** (the GameCube format), 4 bits per sample | Decoded by the DSP, so no CPU cost; needs the 16 coefficients (`ndspChnSetAdpcmCoefs`) and the per-buffer predictor state (`ndspAdpcmData`) produced by the encoder. Mono only in the format list |

`nsamples` counts sample frames: bytes for PCM8, halfwords for PCM16, nibbles (excluding frame headers) for ADPCM. Set a channel's rate to the source's rate (`ndspChnSetRate`); the DSP resamples with the chosen interpolation (`NDSP_INTERP_POLYPHASE`, `LINEAR` or `NONE`).

## Choosing buffer sizes

The queue must hold enough audio to survive the longest time your program might go without refilling it — the **worst** frame, a file read, a level load, a GPU wait — plus the scheduling delay of whatever thread refills it.

| Refill from | Covers | Watch out for |
| ----------- | ------ | ------------- |
| The main loop, once per frame | Total queued ≥ worst frame time + one frame | Loading screens and hitches: keep pumping during loads, or queue more first |
| A worker thread woken by the ndsp frame callback (signal a `LightEvent` in the callback) | Short queues, low latency | The worker must not wait for the GPU, files or locks the main thread holds |
| The ndsp frame callback itself | Smallest latency | Runs on a 4 KiB stack at priority `0x18`: decoding there can overflow the stack and delays the ndsp thread; keep it to signalling |

Latency is the queued length: three 1/30 s buffers mean up to 100 ms between deciding to play a sound and hearing it. For sound effects, use separate channels with short one-shot buffers so they start without waiting behind music.

## Streaming compressed music

Decode Ogg Vorbis (`libvorbisidec`), Opus (`opusfile`) or tracker modules (`libmodplug`) into PCM16 buffers on a worker thread, a few buffers ahead (`3ds-examples/audio/ogg-vorbis-decoding`, `opus-decoding`, `modplug-decoding`). On an Old 3DS, decoding competes with the game for core 0 unless you move it to core 1 ([threading](threading.md#core-1-borrowing-the-system-core)); on a New 3DS, core 2 is the natural place. Read the file in large chunks; small SD reads in the decoder add latency spikes ([asset pipeline](asset-pipeline.md#audio)).

## Effects and mixing

- `ndspChnSetMix(id, mix[12])`: per-channel volumes — front left/right, back left/right for the main output, then the same four for each of the two auxiliary outputs (libctru labels this layout a "working hypothesis") [documented with caveat].
- Per-channel filters: one-pole (`ndspChnIirMono*`: low-pass, high-pass, custom) and biquad (`ndspChnIirBiquad*`: low-pass, high-pass, band-pass, notch, peaking EQ) run on the DSP.
- Auxiliary buses: `ndspAuxSetEnable`, `ndspAuxSetCallback` give you the bus's samples each frame to process on the CPU (e.g. reverb) — CPU cost on the ndsp thread.
- Master: `ndspSetMasterVol`, `ndspSetOutputMode` (mono, stereo, surround), `ndspSetClippingMode`.

## Diagnosing crackles and gaps

| Symptom | Likely cause | Check |
| ------- | ------------ | ----- |
| Short gaps when the game hitches | Queue shorter than the worst frame | Log queued time and the longest gap between refills |
| Constant crackle | Buffer reused while queued/playing, or not flushed | Only refill `FREE`/`DONE` buffers; flush after writing |
| Gaps only with a core-1 share | Slow `DSP_FlushDataCache` (third-party report of 1–5 ms at 80%) or refill thread starved | Time the flush; try `svcFlushProcessDataCache`; lower the share ([threading](threading.md)) |
| Gaps during loading | Refill thread waits on the file system or a lock | Decode ahead before loading; refill from an independent thread |
| Drift between music and gameplay | Game logic tied to the rendered frame rate | Fixed-step simulation ([frame pacing](frame-pacing.md#separating-simulation-from-rendering)) |

`ndspGetFrameCount()` (sound frames processed) and `ndspGetDroppedFrames()` (sound frames ndsp reports as dropped) are useful counters to log.

## What ndsp does not cover

- **AAC.** Some DSP firmware builds contain an AAC decoder reached through the DSP binary pipe 3 (3dbrew *DSP AAC Decoder*: modes 0 none, 1 decode, 2 encode; the page says parts still need testing). The 0x20-byte message format, decode request fields and sample-rate enum are on [DSP firmware interface](../hardware/dsp.md#aac-decoder). libctru 2.7.1 has no AAC API (no match in its headers), so homebrew decodes AAC, MP3 or Vorbis on the CPU. Whether a given dumped `dspfirm.cdc` includes the AAC component varies with the game it was dumped from per 3dbrew `[reverse-engineered]`.
- **CSND.** The older DS-style sound hardware path is a separate service; this page and libctru's current audio API use ndsp ([hardware audio](../hardware/audio.md)).
- **Microphone.** Capture is a different service (`audio/mic` example), not ndsp.

## Technical details

- Sleep and HOME: ndsp suspends and resumes the DSP itself, saving its state across sleep [documented: `source/ndsp/ndsp.c`].
- `ndspUseComponent(binary, size, progMask, dataMask)` lets a program supply its own DSP component instead of loading the file.
- The ndsp thread waits for the DSP interrupt with a timeout of about two sound frames (9.776 ms) and sleeps one sound frame (4.888 ms) while the DSP is not ready.
- Capture (`ndspSetCapture`) records the mixed output into a wave buffer, 160 samples per frame.

## Pitfalls

- **No `dspfirm.cdc`**: `ndspInit` fails and the program is silent. Detect and explain.
- **Sample buffers from `malloc`**: the DSP needs linear memory.
- **Heavy work in the frame callback**: 4 KiB stack, highest priority.
- **Counting bytes in `nsamples`**: it counts sample frames.
- **Stopping without clearing**: call `ndspChnWaveBufClear` before freeing queued buffers.

## Open questions

- Measured CPU cost of `NDSP_INTERP_POLYPHASE` vs `LINEAR` — the work is on the DSP, but the effect on the ndsp thread's update time is not documented.
- Exact behaviour of the auxiliary buses and surround mode beyond libctru's comments.

## Sources

- [libctru](https://github.com/devkitPro/libctru) — `ndsp/ndsp.h`, `ndsp/channel.h`, `source/ndsp/ndsp.c` (firmware loading, thread, frame size, sleep handling), `source/ndsp/ndsp-channel.c` (24 channels), `os.h` (`NDSP_SAMPLE_RATE`).
- [Luma3DS](https://github.com/LumaTeam/Luma3DS) — `sysmodules/rosalina/source/menus/miscellaneous.c` (*Dump DSP firmware*).
- [3ds-examples](https://github.com/devkitPro/3ds-examples) — `audio/streaming`, `audio/filters`, `audio/ogg-vorbis-decoding`, `audio/opus-decoding`, `audio/modplug-decoding`, `audio/mic`.
- [libctru issue #556](https://github.com/devkitPro/libctru/issues/556) — DSP flush latency under a core-1 share.
