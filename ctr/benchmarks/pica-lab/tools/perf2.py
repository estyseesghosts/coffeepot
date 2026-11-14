#!/usr/bin/env python3
"""perf2 analysis: fit p3d_us = c + a*tris + b*pixels over triangle-size variants.

Pixels per triangle come from the perf2_cov captures (measured marked pixels of one
triangle); if absent, the formula s*(s-1)/2 is used and flagged as ASSUMED. Full-target
quads are assumed to cover 65536 px each (ASSUMED; not captured).
Usage: python tools/perf2.py run_NNN.zip|run_NNN/|results.csv [--exclude name,name]
--exclude drops named variants from the fit (they stay in the table). The fit is still withheld
if any suspect variant (P3D >> drawing time) is not excluded; exclusions are printed.
"""
import csv, re, statistics, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))   # works under python -I too
from analyze import resolve_csv

def solve(A, y):
    """Least squares via normal equations (small, dependency-free). Returns coefficients."""
    k = len(A[0])
    M = [[sum(r[i]*r[j] for r in A) for j in range(k)] + [sum(r[i]*t for r, t in zip(A, y))] for i in range(k)]
    for i in range(k):
        piv = max(range(i, k), key=lambda r: abs(M[r][i]))
        if abs(M[piv][i]) < 1e-12: raise ValueError('singular: variants do not separate the terms')
        M[i], M[piv] = M[piv], M[i]
        for r in range(k):
            if r != i:
                f = M[r][i] / M[i][i]
                M[r] = [a - f*b for a, b in zip(M[r], M[i])]
    return [M[i][k] / M[i][i] for i in range(k)]

def run(path, exclude=()):
    rows = list(csv.DictReader(open(resolve_csv(path), newline='', encoding='utf-8')))
    cov = {int(r['parameter']): int(r['nonclear_pixels']) for r in rows if r['family'] == 'perf2_cov'}
    reps = {}
    drawing = {}
    for r in rows:
        if r['family'] == 'perf2' and r.get('p3d_us') not in (None, '', '-1'):
            reps.setdefault(r['variant'], []).append(float(r['p3d_us']))
            drawing.setdefault(r['variant'], []).append(float(r['gpu_queue_ms']) * 1000)
    if not reps: print('no perf2 rows'); return 1
    table = []   # name, tris, pixels, flag
    for name in reps:
        m = re.fullmatch(r'tri_s(\d+)_n(\d+)', name); q = re.fullmatch(r'quad_n(\d+)', name)
        if name == 'empty': table.append((name, 0, 0, '')); continue
        if m:
            s, n = int(m[1]), int(m[2])
            if s in cov: per, flag = cov[s], 'measured'
            else: per, flag = s*(s-1)//2, 'ASSUMED'
            table.append((name, n, n*per, flag))
        elif q:
            table.append((name, 2*int(q[1]), 65536*int(q[1]), 'ASSUMED'))
    emp = statistics.median(reps['empty']) if 'empty' in reps else 0.0
    print(f'{"variant":<14}{"tris":>7}{"pixels":>9}{"px src":>9}{"p3d med":>9}{"min":>6}{"max":>6}{"-empty":>8}{"us/tri":>8}{"ns/px":>8}')
    pts = []
    for name, t, px, flag in table:
        v = reps[name]; med = statistics.median(v)
        d = med - emp
        print(f'{name:<14}{t:>7}{px:>9}{flag:>9}{med:>9.0f}{min(v):>6.0f}{max(v):>6.0f}{d:>8.0f}'
              f'{(d/t if t else 0):>8.3f}{(1000*d/px if px else 0):>8.2f}')
        pts.append((name, t, px, med))
    suspect = [name for name in reps if statistics.median(reps[name]) > 5000 and
               statistics.median(reps[name]) > statistics.median(drawing[name]) + 5000]
    suspect_left = [n for n in suspect if n not in exclude]
    if suspect_left:
        suspect = suspect_left
        print('\nFIT WITHHELD: P3D and drawing timers disagree by over 5 ms for: ' + ', '.join(suspect))
        print('Investigate raw timing samples and synchronization; no samples were silently discarded.')
        return 1
    if exclude:
        print('\nEXCLUDED from fit (explicit --exclude): ' + ', '.join(exclude))
        pts = [p for p in pts if p[0] not in exclude]
    for label, sel in (('all variants', pts), ('triangle variants only', [p for p in pts if p[0] == 'empty' or p[0].startswith('tri_')])):
        if len(sel) < 4: continue
        try:
            c, a, b = solve([[1, t, px] for _, t, px, _ in sel], [m for *_, m in sel])
        except ValueError as e:
            print(f'fit ({label}): {e}'); continue
        res = [(n, m - (c + a*t + b*px)) for n, t, px, m in sel]
        rms = (sum(r*r for _, r in res)/len(res))**0.5
        print(f'\nfit ({label}, {len(sel)} points, 3 params): p3d_us = {c:.1f} + {a:.4f}*tris + {b*1000:.3f}ns*px; rms residual {rms:.1f} us')
        print('  residuals (us): ' + ', '.join(f'{n}:{r:+.1f}' for n, r in res))
    print('\nCaveat: medians of interleaved reps; p3d_us is integer microseconds. Terms are only separable '
          'to the extent the variants vary tris and pixels independently (see px src column).')
    return 0

if __name__ == '__main__':
    if len(sys.argv) < 2: print(__doc__); sys.exit(2)
    ex = ()
    if '--exclude' in sys.argv: ex = tuple(sys.argv[sys.argv.index('--exclude') + 1].split(','))
    sys.exit(run(sys.argv[1], ex))
