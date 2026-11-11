# Recording from the microphone (MIC)

The 3DS has a built-in microphone. Homebrew reads it through the **MIC** service (`mic:u`): you give it a block of memory, tell it a sample format and rate, and the service writes audio into that block in a loop while you read it out. This page shows the setup and ownership rules, a polling read loop that avoids tearing, and what is and is not known about gain, latency and the lid.

Playing sound back is a different system: see [audio](audio.md). The microphone is available on every model.

## Before you start

- Checked against libctru 2.7.1 source (`services/mic.h`, `source/services/mic.c`) and the 3ds-examples `audio/mic` program ([source versions](source-versions.md)). The example below compiles; it has not been run on hardware.
- Nothing about recording quality, latency or noise is measured on this wiki.

## The model

1. You allocate a **page-aligned** buffer (`memalign(0x1000, size)`, size a multiple of 0x1000) in ordinary memory. The service shares it, so the buffer must stay valid until `micExit()`.
2. `micInit(buffer, size)` opens `mic:u`, creates a shared-memory block from your buffer, maps it into the service, and powers the microphone on (`MICU_SetPower(true)`) [documented: `mic.c`]. It is reference counted.
3. `MICU_StartSampling(encoding, rate, offset, size, loop)` starts recording into `buffer[offset .. offset+size)`. With `loop = true` the service wraps back to the start when it reaches the end. `size` must be at most `bufferSize - 4`; the last 4 bytes of the buffer hold the position of the most recent sample (`micGetSampleDataSize()` returns `bufferSize - 4`).
4. `micGetLastSampleOffset()` reads that 4-byte word: the offset of the last sample written. The data between your previous read position and it is new.
5. `MICU_StopSampling()` stops; `micExit()` unmaps the memory and powers the microphone off, and only then may you free the buffer.

| Parameter | Choices (`mic.h`) |
| --------- | ----------------- |
| Encoding | `MICU_ENCODING_PCM8` (unsigned 8-bit), `PCM16` (unsigned 16-bit), `PCM8_SIGNED`, `PCM16_SIGNED` |
| Sample rate | `MICU_SAMPLE_RATE_32730` (32 728.498 Hz), `_16360` (16 364.479 Hz), `_10910` (10 909.499 Hz), `_8180` (8 182.1245 Hz) |

The rates are approximately 32.7, 16.4, 10.9 and 8.2 kHz; the exact values are the ones in the comments of `mic.h` quoted above.

## Reading without losing data

The service writes in the background, so read like a ring buffer: keep your own `readPos`, and each frame consume `[readPos, writePos)` modulo the data size, where `writePos = micGetLastSampleOffset()`. If you read too rarely the writer laps you and you lose the oldest audio; the buffer must be large enough for the longest gap between your reads. At 16 364 Hz and 16-bit mono, the 0x3FFC byte data area of a 0x4000-byte buffer holds about 0.5 s (arithmetic from the table; no measurement).

Alternatively `MICU_GetEventHandle()` returns an event signalled "when the shared memory buffer is full" [documented: `mic.h`]. Whether that fires once per wrap in loop mode is not stated (> Unknown); the example polls instead.

Cache: the service (a separate process) writes the buffer by DMA or its own CPU stores, so the data you read can be stale in your data cache. The 3ds-examples read it directly. Treat the need to invalidate as `[hypothesis]` and test it ([memory and cache](memory-and-cache.md#keeping-caches-coherent)).

## Gain, clamping and the lid

- `MICU_SetGain(u8)` / `MICU_GetGain` set the amplifier gain; the valid range and its unit are not documented in the header (> Unknown).
- `MICU_SetClamp` / `MICU_GetClamp` "clamp microphone input"; what it clamps is not documented.
- `MICU_SetAllowShellClosed(true)` allows sampling while the lid is closed; without it sampling while closed is presumably refused (the header only says "whether to allow sampling when the shell is closed").
- The 3ds-examples feed recorded audio to CSND for playback; with [ndsp](audio.md) you would copy samples into an `ndspWaveBuf` instead.

## Complete example: input level meter

Samples 16-bit signed mono at 16 364 Hz into a 16 KiB ring and prints the peak of the newest samples each frame. START exits.

```c
#include <3ds.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>

#define MIC_BUF_SIZE 0x4000u   // multiple of 0x1000; the last 4 bytes belong to the service

int main(void)
{
	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);

	u8 *buf = memalign(0x1000, MIC_BUF_SIZE);   // must be page aligned (mic.h)
	if (!buf) { printf("out of memory\n"); gfxExit(); return 1; }

	Result rc = micInit(buf, MIC_BUF_SIZE);     // also powers the microphone on (mic.c)
	if (R_FAILED(rc)) {
		printf("micInit failed: %08lX\n", (unsigned long)rc);
	} else {
		const u32 dataSize = micGetSampleDataSize();   // MIC_BUF_SIZE - 4
		rc = MICU_StartSampling(MICU_ENCODING_PCM16_SIGNED, MICU_SAMPLE_RATE_16360, 0, dataSize, true);
		if (R_FAILED(rc)) printf("StartSampling failed: %08lX\n", (unsigned long)rc);

		u32 readPos = 0;
		while (R_SUCCEEDED(rc) && aptMainLoop()) {
			hidScanInput();
			if (hidKeysDown() & KEY_START) break;

			u32 writePos = micGetLastSampleOffset();   // the service writes here and wraps
			s32 peak = 0;
			while (readPos != writePos) {              // 2 bytes per PCM16 sample
				s16 s = *(volatile s16 *)(buf + readPos);
				s32 a = s < 0 ? -(s32)s : s;
				if (a > peak) peak = a;
				readPos = (readPos + 2) % dataSize;
			}
			printf("\x1b[1;1Hpeak %5ld / 32768   ", (long)peak);
			gfxFlushBuffers();
			gfxSwapBuffers();
			gspWaitForVBlank();
		}
		MICU_StopSampling();
	}

	micExit();            // unmaps the shared memory and powers the microphone off
	free(buf);            // only after micExit
	gfxExit();
	return 0;
}
```

Expected on an Old 3DS: `peak` stays small in a quiet room and rises when you speak or tap near the microphone; speaking loudly should approach but not necessarily reach 32768. This is the behaviour the API implies; it has not been observed.

## Technical details

| Item | Value | Source |
| ---- | ----- | ------ |
| Service | `mic:u` | `mic.c` |
| Shared memory | `svcCreateMemoryBlock(addr, size, RW, RW)` over the caller's buffer, then `MICU_MapSharedMem(size, handle)` (command `0x01`) | `mic.c` |
| Power | `micInit` calls `MICU_SetPower(true)`; `micExit` calls it with `false` | `mic.c` |
| Data area | `size - 4`; last-sample offset stored in the last 4 bytes | `mic.c` `micGetSampleDataSize`, `micGetLastSampleOffset` |
| Alignment | buffer aligned to 0x1000 | `mic.h` |
| Failure | `micInit` calls `micExit` on error and returns the error | `mic.c` |

## Pitfalls

- **Freeing or moving the buffer before `micExit()`.** The service still writes into it. Fix: free only after `micExit()`.
- **Unaligned buffer.** Symptom: `micInit` fails. Fix: `memalign(0x1000, n)`.
- **Using `size` equal to the whole buffer.** The last 4 bytes are the position word; pass `micGetSampleDataSize()`.
- **Reading too slowly.** The writer wraps and overwrites unread data. Fix: size the ring for your worst-case frame time and read every frame.
- **Treating unsigned PCM as signed.** The 8-bit and 16-bit unsigned encodings centre on the mid value, not 0.
- **Not powering down.** `micExit()` turns the microphone off; skipping it leaves the microphone powered (and its privacy implications) after your logic is done.

## Open questions

- Gain range and units, what clamping does, and the real noise floor.
- Recording latency (acoustic event to sample visible in the buffer) and whether it differs between Old 3DS and New 3DS.
- Whether the full-buffer event fires on each wrap in loop mode.
- Whether recording continues through HOME and sleep, and what the first buffer contents look like after a suspend ([application lifecycle](application-lifecycle.md)).
- Whether reading the buffer needs a cache invalidate.

## Verification status

| Item | Status |
| ---- | ------ |
| Example | Builds with devkitARM r68-1 (GCC 16.1.0), libctru 2.7.1-1, `-Wall -Wextra`, no warnings; linked with `3dsx.specs` |
| Old 3DS | not run |
| New 3DS | not run |
| Emulator | not run |

## Sources

- [libctru `mic.h`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/services/mic.h) and [`mic.c`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/source/services/mic.c) — encodings, rates, init/exit, sample-offset word.
- [3ds-examples `audio/mic`](https://github.com/devkitPro/3ds-examples/blob/be2001fee08cf8ec7d3366e095e83f6f75da8942/audio/mic/source/main.c) — record-and-play example.
