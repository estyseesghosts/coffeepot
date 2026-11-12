// Fills an interleaved stereo s16 output block from a source that may be shorter than requested.
#pragma once
#include <stddef.h>
#include <stdint.h>

// Copies min(have, want) stereo frames, then repeats the last copied frame to fill the rest. With no source frames
// (have == 0) the whole block is silence. Never reads past `have` frames or writes past `want` frames.
// A frame is two int16_t (left, right); `out` and `src` need only int16_t alignment.
void pcm_fill(int16_t* out, size_t want, const int16_t* src, size_t have);
