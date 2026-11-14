#!/usr/bin/env python3
"""timing_iso analysis: is the P3D anomaly tied to count, placement, or the preceding frame?

For each variant_mode prints P3D median/min/max, drawing-time median, the share of reps whose
P3D exceeds drawing time by >5 ms, and how close those P3D values sit to multiples of one 60 Hz
frame (16666.7 us). Proximity to multiples supports (does not prove) vblank quantization.
Usage: python tools/timing_iso.py run_NNN.zip|run_NNN/|results.csv
"""
import csv, statistics, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from analyze import resolve_csv

FRAME = 1_000_000 / 60

def run(path):
    rows = [r for r in csv.DictReader(open(resolve_csv(path), newline='', encoding='utf-8'))
            if r['family'] == 'timing_iso']
    if not rows: print('no timing_iso rows (run v0.5 on hardware)'); return 1
    g = {}
    for r in rows: g.setdefault(r['variant'], []).append((float(r['p3d_us']), float(r['gpu_queue_ms']) * 1000))
    print(f'{"variant":<16}{"n":>3}{"p3d med":>9}{"min":>8}{"max":>8}{"draw med":>9}{"anom":>6}{"frames(anom)":>16}')
    for name, v in g.items():
        p = [a for a, _ in v]; d = [b for _, b in v]
        an = [a for a, b in v if a > 5000 and a > b + 5000]
        fr = ','.join(f'{a/FRAME:.2f}' for a in sorted(set(round(x, -2) for x in an))[:4])
        print(f'{name:<16}{len(v):>3}{statistics.median(p):>9.0f}{min(p):>8.0f}{max(p):>8.0f}'
              f'{statistics.median(d):>9.0f}{len(an):>3}/{len(v):<2}{fr:>16}')
    return 0

if __name__ == '__main__':
    if len(sys.argv) < 2: print(__doc__); sys.exit(2)
    sys.exit(run(sys.argv[1]))
