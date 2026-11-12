// Continuous PCM streaming through audout, used by nx/engineering/audio.md.
//
// Keeps NBUF buffers of 20 ms in flight, refills each one when the audio service gives it back, fills short or
// missing source data with pcm_fill (repeat the last frame, or silence), and counts what went wrong:
//   underruns     polls that found every buffer already released, so output may have run dry
//   short_blocks  blocks the source could only partly fill
//   silent_blocks blocks the source could not fill at all
// Build with `make CFLAGS_EXTRA=-DAS_SELFTEST` for a 5 s scripted run that writes report.csv (see the page).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <malloc.h>
#include <math.h>
#include <unistd.h>
#include <sys/stat.h>
#include <switch.h>
#include "pcm_pad.h"

#define NBUF 4
#define BLOCK_FRAMES 960                                // 20 ms at 48 kHz
#define BLOCK_BYTES (BLOCK_FRAMES * 2 * sizeof(int16_t))
#define BUF_BYTES ((BLOCK_BYTES + 0xFFF) & ~(size_t)0xFFF)   // audout wants 0x1000-aligned size and address
#define OUT_DIR "sdmc:/switch/audoutstream"

typedef enum { BUF_FREE, BUF_QUEUED } BufState;
static AudioOutBuffer g_buf[NBUF];
static BufState g_state[NBUF];

// A source that can run short: a 440 Hz tone that lasts 3 s, and every 50th block delivers only 100 frames.
static uint64_t g_srcPos;                               // frames produced so far
static size_t sourceRead(int16_t* out, size_t want, unsigned blockNo)
{
    const uint64_t total = 3u * 48000u;
    size_t have = want;
    if (blockNo % 50 == 49) { have = 100; }
    if (g_srcPos >= total) { return 0; }
    if (g_srcPos + have > total) { have = (size_t)(total - g_srcPos); }
    for (size_t i = 0; i < have; i++) {
        const int16_t v = (int16_t)(0.2f * 32767.0f * sinf(2.0f * (float)M_PI * 440.0f * (float)(g_srcPos + i) / 48000.0f));
        out[2 * i] = v;
        out[2 * i + 1] = v;
    }
    g_srcPos += have;
    return have;
}

int main(void)
{
    int rc = 0;
    unsigned filled = 0, released = 0, underruns = 0, shortBlocks = 0, silentBlocks = 0, polls = 0, releasedByGet = 0;

    if (R_FAILED(audoutInitialize())) { printf("audoutInitialize failed\n"); return 1; }
    if (audoutGetSampleRate() != 48000 || audoutGetChannelCount() != 2 || audoutGetPcmFormat() != PcmFormat_Int16) {
        printf("unexpected output format: %u Hz, %u ch\n", audoutGetSampleRate(), audoutGetChannelCount());
        audoutExit();
        return 1;
    }
    for (int i = 0; i < NBUF; i++) {
        void* mem = memalign(0x1000, BUF_BYTES);
        if (!mem) { rc = 1; break; }
        memset(mem, 0, BUF_BYTES);
        g_buf[i] = (AudioOutBuffer){ NULL, mem, BUF_BYTES, BLOCK_BYTES, 0 };
    }
    if (rc == 0 && R_FAILED(audoutStartAudioOut())) { rc = 1; }

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
#ifdef AS_SELFTEST
    const u64 start = armGetSystemTick();
#endif

    while (rc == 0 && appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) { break; }
#ifdef AS_SELFTEST
        if (armTicksToNs(armGetSystemTick() - start) > 5000000000ULL) { break; }
#endif
        polls++;
        {   // The second ownership signal: the service's own released-buffer count (it returns at most one pointer per call).
            AudioOutBuffer* rel = NULL;
            u32 cnt = 0;
            if (R_SUCCEEDED(audoutGetReleasedAudioOutBuffer(&rel, &cnt))) { releasedByGet += cnt; }
        }

        // A buffer is ours again when the service no longer holds it.
        unsigned queued = 0;
        for (int i = 0; i < NBUF; i++) {
            if (g_state[i] != BUF_QUEUED) { continue; }
            bool held = true;
            if (R_SUCCEEDED(audoutContainsAudioOutBuffer(&g_buf[i], &held)) && !held) {
                g_state[i] = BUF_FREE;
                released++;
            } else {
                queued++;
            }
        }
        if (queued == 0 && filled >= NBUF) { underruns++; }     // everything we queued has already been played

        for (int i = 0; i < NBUF; i++) {
            if (g_state[i] != BUF_FREE) { continue; }
            int16_t* block = (int16_t*)g_buf[i].buffer;
            const size_t have = sourceRead(block, BLOCK_FRAMES, filled);
            if (have == 0) { silentBlocks++; } else if (have < BLOCK_FRAMES) { shortBlocks++; }
            pcm_fill(block, BLOCK_FRAMES, block, have);         // in place: frames [0, have) are already there
            if (R_SUCCEEDED(audoutAppendAudioOutBuffer(&g_buf[i]))) {
                g_state[i] = BUF_QUEUED;
                filled++;
            }
        }
        svcSleepThread(2000000ULL);                            // poll every 2 ms; a 20 ms block gives plenty of slack
    }

    // Shutdown: stop output, then wait until the service has given every buffer back before freeing any memory.
    audoutStopAudioOut();
    for (int tries = 0; tries < 100; tries++) {
        bool any = false;
        for (int i = 0; i < NBUF; i++) {
            bool held = false;
            if (g_state[i] == BUF_QUEUED && R_SUCCEEDED(audoutContainsAudioOutBuffer(&g_buf[i], &held)) && held) { any = true; }
        }
        if (!any) { break; }
        svcSleepThread(5000000ULL);
    }

#ifdef AS_SELFTEST
    mkdir(OUT_DIR, 0777);
    FILE* f = fopen(OUT_DIR "/report.csv", "w");
    if (f) {
        fprintf(f, "info,rate_hz,%u\ninfo,channels,%u\ninfo,polls,%u\ninfo,blocks_filled,%u\ninfo,buffers_released,%u\n",
                audoutGetSampleRate(), audoutGetChannelCount(), polls, filled, released);
        fprintf(f, "info,underruns,%u\ninfo,short_blocks,%u\ninfo,silent_blocks,%u\ninfo,released_by_get_count,%u\n",
                underruns, shortBlocks, silentBlocks, releasedByGet);
        fprintf(f, "blocks_flowed,%s\n", (filled > 100 && released > 100) ? "pass" : "FAIL");
        fprintf(f, "short_blocks_seen,%s\n", shortBlocks >= 2 ? "pass" : "FAIL");
        fprintf(f, "silence_after_source_ends,%s\n", silentBlocks > 50 ? "pass" : "FAIL");
        fprintf(f, "note,done\n");
        fclose(f);
    }
#else
    printf("polls %u blocks %u released %u (service count %u) underruns %u short %u silent %u\n",
           polls, filled, released, releasedByGet, underruns, shortBlocks, silentBlocks);
#endif
    for (int i = 0; i < NBUF; i++) { free(g_buf[i].buffer); }
    audoutExit();
    return rc;
}
