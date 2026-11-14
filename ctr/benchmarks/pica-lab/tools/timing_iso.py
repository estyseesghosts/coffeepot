#!/usr/bin/env python3
"""timing_iso analysis: is the P3D anomaly tied to count, placement, or the preceding frame?

For each variant_mode prints P3D median/min/max, drawing-time median, the share of reps whose
P3D is anomalous, and how close those P3D values sit to multiples of one 60 Hz frame
(16666.7 us). Proximity to multiples supports (does not prove) VBlank quantisation.

v0.7+ runs (drain_us column): each frame's segments are printed (begin = FrameBegin with
SYNCDRAW, i.e. VBlank + previous-queue wait; end = C3D_FrameEnd call; drain = FrameEnd return
until the GX queue is idle; log = second GetPerfLog IPC). A rep is anomalous when P3D exceeds
drain by more than 5 ms. Rows with assoc=0 are counted.

v0.5/v0.6 runs: the harness read P3D and drawing time after C3D_FrameSync(), which waits for
VBlank and not for the GPU, so neither is tied to the submitted frame; v0.6 timing_wall rows
(FrameEnd..FrameSync) time a VBlank wait, not GPU work. Printed for the record, marked PRE-V0.7.
Usage: python tools/timing_iso.py run_NNN.zip|run_NNN/|results.csv
"""
import csv, statistics, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from analyze import resolve_csv

FRAME = 1_000_000 / 60

def med(v): return statistics.median(v) if v else float('nan')

def frames(an):
    return ','.join(f'{a/FRAME:.2f}' for a in sorted(set(round(x, -2) for x in an))[:4])

def run(path):
    allrows = list(csv.DictReader(open(resolve_csv(path), newline='', encoding='utf-8')))
    rows = [r for r in allrows if r['family'] == 'timing_iso']
    if not rows: print('no timing_iso rows (run v0.5+ on hardware)'); return 1
    v07 = 'drain_us' in rows[0]
    g = {}
    for r in rows: g.setdefault(r['variant'], []).append(r)
    if v07:
        print(f'{"variant":<16}{"n":>3}{"p3d med":>9}{"min":>8}{"max":>8}{"draw":>7}{"drain":>8}'
              f'{"begin":>8}{"end":>7}{"log":>7}{"assoc0":>7}{"anom":>7}{"frames(anom)":>16}')
        for name, rs in g.items():
            p = [float(r['p3d_us']) for r in rs]; dr = [float(r['drain_us']) for r in rs]
            an = [float(r['p3d_us']) for r in rs if float(r['p3d_us']) > float(r['drain_us']) + 5000]
            print(f'{name:<16}{len(rs):>3}{med(p):>9.0f}{min(p):>8.0f}{max(p):>8.0f}'
                  f'{med([float(r["gpu_queue_ms"]) * 1000 for r in rs]):>7.0f}{med(dr):>8.0f}'
                  f'{med([float(r["begin_us"]) for r in rs]):>8.0f}{med([float(r["frameend_us"]) for r in rs]):>7.0f}'
                  f'{med([float(r["perflog_us"]) for r in rs]):>7.0f}{sum(r["assoc"] == "0" for r in rs):>7}'
                  f'{len(an):>4}/{len(rs):<2}{frames(an):>16}')
        print('\nAll columns in us. draw = C3D_GetDrawingTime after the queue drained; drain includes '
              'GPU execution and completion latency; begin is VBlank + previous-queue wait (not GPU time).')
        return 0
    wall = {}   # v0.6: timing_wall rows carry FrameEnd..FrameSync time (a VBlank wait) in gpu_queue_ms
    for r in allrows:
        if r['family'] == 'timing_wall': wall.setdefault(r['variant'], []).append(float(r['gpu_queue_ms']) * 1000)
    print('PRE-V0.7 SYNCHRONISATION: P3D and drawing time were read after C3D_FrameSync (a VBlank wait),\n'
          'so they are not tied to the submitted frame; "vblank" is FrameEnd..FrameSync, not GPU time.\n')
    print(f'{"variant":<16}{"n":>3}{"p3d med":>9}{"min":>8}{"max":>8}{"draw med":>9}{"vblank":>8}{"anom":>7}{"frames(anom)":>16}')
    for name, rs in g.items():
        v = [(float(r['p3d_us']), float(r['gpu_queue_ms']) * 1000) for r in rs]
        p = [a for a, _ in v]; d = [b for _, b in v]
        an = [a for a, b in v if a > 5000 and a > b + 5000]
        w = f'{med(wall[name]):>8.0f}' if name in wall else f'{"-":>8}'
        print(f'{name:<16}{len(v):>3}{med(p):>9.0f}{min(p):>8.0f}{max(p):>8.0f}'
              f'{med(d):>9.0f}{w}{len(an):>4}/{len(v):<2}{frames(an):>16}')
    return 0

if __name__ == '__main__':
    if len(sys.argv) < 2: print(__doc__); sys.exit(2)
    sys.exit(run(sys.argv[1]))
