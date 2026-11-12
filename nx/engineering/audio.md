# Audio on Switch homebrew: audout, audren and a streaming loop

This page is for a developer who needs more than a beep: continuous music, sound effects, or an emulator's audio. The short answer: for one stream of PCM you generate yourself, use `audout` and keep several buffers in flight, refilling each when the system gives it back; for many sounds mixed by the system, use `audren` (through the `audrv` helper); and in either case count underruns instead of listening for them.

## Before you start

- Toolchain: devkitA64 r30-1, `libnx` 4.12.0-1 ([source versions](source-versions.md)). Header facts are `[documented]` from `libnx` at `b156b4774fb2df7b93402169509266da7547fc4c`.
- Verification: `nx/engineering/examples/audout-stream` builds with no warnings and its self-test **ran in Eden 0.2.1** (about 5 s: 254 blocks queued, 250 buffers returned, 0 underruns, short and empty source blocks padded). That shows the code path and ownership logic work in an emulator; it says nothing about real audio timing, latency or hardware behaviour. Not run on a console. The `audren` part of this page has no example here.
- Terms: audio is sent as **pulse-code modulation** (**[PCM](../../glossary.md#pcm)**), plain signed 16-bit sample values here. A *frame* below is one left and right sample pair.

## Choose an API

| API | Role | Use it when |
| --- | ---- | ----------- |
| `audout` | Submit PCM buffers to the system audio output | You produce the final mix yourself (emulators, your own mixer): one continuous stream |
| `audren` | The system audio renderer: voices, mixes, sinks, memory pools | You want the system to mix many voices; it runs on a 5 ms period (200 Hz, 240 frames at 48 kHz) |
| `audrv` | libnx's helper over `audren` | You want voices without filling every renderer parameter by hand |

`audout` supports one format on this wiki's pinned `libnx`: 48,000 Hz, 2 channels, signed 16-bit (`audoutGetSampleRate`, `audoutGetChannelCount`, `audoutGetPcmFormat`) `[documented]` (`audout.h`). Check them at startup and refuse to run otherwise, as the example does.

## A streaming loop with audout

```c
// NBUF buffers of 960 frames (20 ms), each 0x1000-aligned in address and size.
// state[i] is FREE (ours) or QUEUED (the audio service owns it).
for (;;) {
    AudioOutBuffer* rel; u32 n;
    audoutGetReleasedAudioOutBuffer(&rel, &n);        // drain the released list EVERY poll
    for (i in buffers) if (state[i] == QUEUED) {
        bool held;
        if (R_SUCCEEDED(audoutContainsAudioOutBuffer(&buf[i], &held)) && !held) state[i] = FREE;
    }
    for (i in buffers) if (state[i] == FREE) {
        have = source_read(buf[i].buffer, FRAMES);     // may be short or zero
        pcm_fill(buf[i].buffer, FRAMES, buf[i].buffer, have);   // repeat last frame, or silence
        if (R_SUCCEEDED(audoutAppendAudioOutBuffer(&buf[i]))) state[i] = QUEUED;
    }
    svcSleepThread(2000000);                           // poll every 2 ms
}
```

What to expect: about 50 blocks per second at 960 frames per block. The buffer is yours only while `state` is FREE; never write to a queued buffer, and free memory only after `audoutStopAudioOut` and after every buffer reports not held.

### Ownership: two signals, and a trap

- `audoutGetReleasedAudioOutBuffer(&buf, &count)` reports how many buffers were released, but `libnx` passes an output buffer of one pointer (`sizeof(*Buffer)`), so at most the **first** released buffer's address comes back `[documented]` (`nx/source/services/audout.c`); with several released at once the others' addresses are not returned. Poll often and track each buffer yourself.
- `audoutContainsAudioOutBuffer(buf, &held)` tells you whether the service still holds a particular buffer. In Eden it kept reporting "held" until `audoutGetReleasedAudioOutBuffer` had been called, so polling it alone stalled after the first NBUF blocks: call both, every poll (the example's counts agree: 250 and 250). Whether the same holds on hardware is untested.

### Sizes and latency

| Frames per buffer | Duration |
| ----------------- | -------- |
| 240 | 5 ms |
| 480 | 10 ms |
| 960 | 20 ms |
| 1,920 | 40 ms |

These are sample durations, not output latency. Queued audio is a trade: more of it tolerates a late frame, less of it makes a new sound start sooner. The first-game beep queues one 100 ms buffer; a mixer should keep a few short ones.

## The padding helper

`pcm_fill(out, want, src, have)` (`nx/engineering/examples/common/pcm_pad.c`) copies `min(have, want)` frames, then repeats the last frame, or writes silence when `have` is 0. It copies each frame as two `int16_t` values. Do not write this as a `uint32_t*` cast over `int16_t` samples: that violates C's effective-type rules and may misalign, and compiling cleanly does not make it safe. The helper is host-tested with zero, one, partial, full and oversized sources, and checks that nothing is written past `want` (`nx/tests/run.sh`).

## Sample rate, sync and the other policies

- Audio authored at another rate plays at the wrong speed and pitch if you label it 48 kHz: 44.1 kHz data played as 48 kHz runs about 9% fast (arithmetic). Resample, or let a mixer that resamples do it. max_nx sets its OpenAL context to 44,100 Hz and lets the Switch mixer resample `[reported]`.
- If video runs late there are several policies: keep audio and drop frames, slow the emulated clock, resample slightly, or accept more latency. None is universally right; an emulator, a rhythm game and a platformer differ. melonDS pads a short audio block with the last frame, so late video stretches audio `[documented]` (`melonDS` `src/frontend/switch/main.cpp`).
- Run the audio producer on its own thread at a deliberate priority only after measuring; godot_switch uses priority `0x2B` for `audren` and melonDS `0x20` for audio `[documented]`. See [lifecycle and threading](lifecycle-and-threading.md).

## audren in outline

`audren.h` defines renderer configuration, memory pools, voices, wave buffers, sinks and mixes, with a nominal 200 Hz period (`AUDREN_TIMER_FREQ_HZ`) and 240 frames per period at 48 kHz (`AUDREN_SAMPLES_PER_FRAME_48KHZ`). The upstream `audio/audren-simple` example allocates page-aligned memory, flushes the CPU data cache, creates the driver, attaches a memory pool, adds a device sink, starts a voice and calls `audrvUpdate` `[documented]` (`switch-examples`). A useful extension, not written here: two simultaneous effects plus streamed music with independent volumes.

## Tests worth running on a console

Source shorter than a block and empty source (host-tested); several effects triggered in one frame; output unavailable; suspend and resume; a CPU-heavy scene delaying the producer (count underruns); different source rates; stopping with buffers queued.

## Pitfalls

- **Crackle or silence.** Buffer address or size not 0x1000-aligned, a buffer rewritten while queued, or a late producer. Count underruns.
- **Stalls after the first few blocks.** Releases not drained; see the trap above.
- **Pitch or speed wrong.** Source rate not 48 kHz.
- **Freeing buffers while queued.** Stop, wait until none is held, then free.
- **Cast-based padding.** Use `pcm_fill`.

## Open questions

- Does `audoutContainsAudioOutBuffer` alone return the buffer on hardware, as it did not in Eden?
- What is the real output latency for 5, 10, 20 and 40 ms buffers? Needs a loopback or timestamp test on a console.
- Does audio survive suspend and resume without restarting the output?

## Sources

- [libnx](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/include/switch/services/audout.h) at `b156b4774fb2df7b93402169509266da7547fc4c` — `services/audout.h`, `services/audren.h`, `audio/driver.h`, `nx/source/services/audout.c`.
- [switch-examples](https://github.com/switchbrew/switch-examples) at `669786898205b7beb25ff1731e72982e6d0397d3` — `audio/audren-simple`, `audio/playtone`.
- [melonDS](https://github.com/RSDuck/melonDS) at `6c6f1e8`, [godot_switch](https://github.com/redthing1/godot_switch) at `0918643f6`, [max_nx](https://github.com/fgsfdsfgs/max_nx) at `264a0cc` — padding, priority and rate notes.
- `nx/engineering/examples/audout-stream`, `nx/tests/test_logic.c` — the loop and its host tests.
