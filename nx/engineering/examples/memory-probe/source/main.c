// Reports what the kernel says about this process's memory, then measures how much heap can actually be allocated.
// Used by nx/engineering/memory-and-storage.md. Run it under the Album applet and again under title override and
// compare: the two budgets differ, and the numbers on this wiki came from ports, not from this program.
//
// Writes report.csv to sdmc:/switch/memoryprobe/ and exits; there is no screen output.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <switch.h>

#define OUT_DIR "sdmc:/switch/memoryprobe"
#define CHUNK (16u * 1024u * 1024u)                    // allocate in 16 MiB steps
#define MAX_CHUNKS 512                                  // stop at 8 GiB however much the system claims

static FILE* g_out;

static void infoLine(const char* name, InfoType id)
{
    u64 v = 0;
    const Result rc = svcGetInfo(&v, id, CUR_PROCESS_HANDLE, 0);
    if (R_FAILED(rc)) {
        fprintf(g_out, "info,%s,unavailable,rc=0x%x module=%u description=%u\n", name, rc, R_MODULE(rc), R_DESCRIPTION(rc));
    } else {
        fprintf(g_out, "info,%s,%llu\n", name, (unsigned long long)v);
    }
}

int main(void)
{
    mkdir(OUT_DIR, 0777);
    g_out = fopen(OUT_DIR "/report.csv", "w");
    if (!g_out) { return 1; }

    const AppletType at = appletGetAppletType();
    fprintf(g_out, "info,applet_type,%d\n", (int)at);       // 0 = application (title override), 3 = overlay, 2 = library applet (Album)
    fprintf(g_out, "info,hos_version,%u.%u.%u\n", HOSVER_MAJOR(hosversionGet()), HOSVER_MINOR(hosversionGet()), HOSVER_MICRO(hosversionGet()));

    // Each call can fail on an older firmware; the report says which ones worked rather than assuming.
    infoLine("core_mask", InfoType_CoreMask);
    infoLine("priority_mask", InfoType_PriorityMask);
    infoLine("heap_region_size", InfoType_HeapRegionSize);
    infoLine("total_memory_size", InfoType_TotalMemorySize);
    infoLine("used_memory_size", InfoType_UsedMemorySize);
    infoLine("system_resource_size_total", InfoType_SystemResourceSizeTotal);
    infoLine("system_resource_size_used", InfoType_SystemResourceSizeUsed);
    infoLine("total_non_system_memory_size", InfoType_TotalNonSystemMemorySize);
    infoLine("used_non_system_memory_size", InfoType_UsedNonSystemMemorySize);

    extern char* fake_heap_start;                       // libnx newlib heap bounds, set once at startup (nx/source/runtime/init.c)
    extern char* fake_heap_end;
    fprintf(g_out, "info,newlib_heap_bytes,%llu\n", (unsigned long long)(fake_heap_end - fake_heap_start));

    // Allocate, touch every page, verify the pattern, stop at the first failure, then free everything.
    static uint8_t* chunks[MAX_CHUNKS];
    unsigned n = 0;
    bool corrupt = false;
    while (n < MAX_CHUNKS) {
        uint8_t* p = malloc(CHUNK);
        if (!p) { break; }
        for (size_t o = 0; o < CHUNK; o += 4096) { p[o] = (uint8_t)(n + (o >> 12)); }
        chunks[n++] = p;
    }
    for (unsigned i = 0; i < n; i++) {
        for (size_t o = 0; o < CHUNK; o += 4096) {
            if (chunks[i][o] != (uint8_t)(i + (o >> 12))) { corrupt = true; }
        }
    }
    fprintf(g_out, "info,malloc_chunk_bytes,%u\ninfo,malloc_chunks_obtained,%u\ninfo,malloc_bytes_obtained,%llu\n",
            CHUNK, n, (unsigned long long)n * CHUNK);
    infoLine("used_memory_size_at_peak", InfoType_UsedMemorySize);
    for (unsigned i = 0; i < n; i++) { free(chunks[i]); }
    infoLine("used_memory_size_after_free", InfoType_UsedMemorySize);

    fprintf(g_out, "pattern_verified,%s\n", corrupt ? "FAIL" : "pass");
    fprintf(g_out, "allocated_something,%s\n", n > 0 ? "pass" : "FAIL");
    fprintf(g_out, "note,done\n");
    fclose(g_out);
    return 0;
}
