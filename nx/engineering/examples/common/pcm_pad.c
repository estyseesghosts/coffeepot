#include "pcm_pad.h"
#include <string.h>

void pcm_fill(int16_t* out, size_t want, const int16_t* src, size_t have)
{
    if (have > want) { have = want; }
    if (have > 0) { memcpy(out, src, have * 2 * sizeof(int16_t)); }
    for (size_t i = have; i < want; i++) {
        out[2 * i]     = have > 0 ? out[2 * (have - 1)]     : 0;
        out[2 * i + 1] = have > 0 ? out[2 * (have - 1) + 1] : 0;
    }
}
