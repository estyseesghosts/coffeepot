#!/usr/bin/env python3
"""Exact pixel-center model for the raster_fine family, and comparison to captures.

Model (hypothesis under test, not a hardware spec): a pixel is covered iff its center
(px+0.5, py+0.5) is inside the triangle. A center exactly on an edge (a "tie") is decided by
tie_rule(dx, dy) of that edge's direction in vertex order. v0.4 runs 009-011 (strict-inside
model, ties outside) showed ties on edges with dx>0 ARE covered (A4 step 128, B2 step 128,
B4 step 120) while ties on (-,+), (0,-), (-,-) edges are not. TIE_RULE below is the provisional
fit to that; the evidence table printed by analyze() shows which direction classes have data.
Arithmetic is exact (integers in 1/512-pixel units). Program x is the raw ROW index and
program y the raw COLUMN index (found by fitting run_003 raster step 0; the only one of the
8 orientations with zero mismatches).

Geometry tables MUST match source/main.c (fine_geoms / fine_wins).
Usage: python tools/rasterfine.py run_NNN.zip|run_NNN/|results.csv [--predict]
"""
import csv, io, sys, zipfile
from pathlib import Path

SIZE = 256
CLEAR = bytes((0xFF, 0, 0, 0))   # raw bytes of 0x000000FF little-endian
GEOMS = {   # vertices in program pixels (integer base; offset is added on top)
    'A': [(80, 48), (176, 48), (80, 176)],      # 3:4 hypotenuse (as v0.3 raster)
    'B': [(100, 40), (212, 61), (124, 181)],    # three slanted edges, no 3:4 slope
    'Ar': [(80, 48), (80, 176), (176, 48)],     # A with reversed vertex order (opposite winding)
    'Br': [(100, 40), (124, 181), (212, 61)],   # B with reversed vertex order
}
# (window name, geometry, axis, lo, hi) in 1/256-pixel offset units, inclusive; hi-lo+1 steps.
WINS = [
    ('A1', 'A', 'x', 26, 38), ('A2', 'A', 'x', 90, 102), ('A3', 'A', 'x', 122, 134),
    ('A4', 'A', 'y', 122, 134), ('A5', 'A', 'y', 36, 48),
    ('B1', 'B', 'x', 28, 40), ('B2', 'B', 'x', 118, 130), ('B3', 'B', 'x', 150, 162),
    ('B4', 'B', 'y', 112, 124),
    # v0.5 tie-rule windows: reversed winding, same offsets as the A/B windows with ties.
    ('R1', 'Ar', 'x', 26, 38), ('R2', 'Ar', 'x', 90, 102), ('R3', 'Ar', 'x', 122, 134),
    ('R4', 'Ar', 'y', 122, 134),
    ('R5', 'Br', 'x', 118, 130), ('R6', 'Br', 'y', 112, 124),
]
WINS_V04 = WINS[:9]   # windows present in v0.4 runs (009-011)

def tie_rule(dx, dy):
    """Provisional: a tie on an edge with direction (dx,dy) is covered iff dx>0 (or dx==0, dy>0
    -- no data yet for that case). Fit to 4 direction classes only; see evidence table."""
    return dx > 0 or (dx == 0 and dy > 0)

def covered(geom, ox256=0, oy256=0, ties=tie_rule):
    """Set of (px,py) program pixels covered, offsets in 1/256 px. Exact integers, unit 1/512 px.
    ties: callable(dx,dy)->bool for centers exactly on an edge; None = ties outside."""
    v = [((x * 256 + ox256) * 2, (y * 256 + oy256) * 2) for x, y in GEOMS[geom]]
    xs = [p[0] for p in v]; ys = [p[1] for p in v]
    x0, x1 = max(0, min(xs) // 512 - 1), min(SIZE - 1, max(xs) // 512 + 1)
    y0, y1 = max(0, min(ys) // 512 - 1), min(SIZE - 1, max(ys) // 512 + 1)
    def e(a, b, p): return (b[0]-a[0])*(p[1]-a[1]) - (b[1]-a[1])*(p[0]-a[0])
    area2 = e(v[0], v[1], v[2])
    edir = [(v[(i+1) % 3][0]-v[i][0], v[(i+1) % 3][1]-v[i][1]) for i in range(3)]
    out = set()
    for py in range(y0, y1 + 1):
        for px in range(x0, x1 + 1):
            p = (px * 512 + 256, py * 512 + 256)
            s = (e(v[0], v[1], p), e(v[1], v[2], p), e(v[2], v[0], p))
            w = 1 if area2 > 0 else -1
            ok = True
            for i in range(3):
                t = s[i] * w
                if t < 0 or (t == 0 and not (ties and ties(*edir[i]))): ok = False; break
            if ok: out.add((px, py))
    return out

def tie_pixels(geom, ox256, oy256):
    """[(pixel, edge_direction)] for centers exactly on an edge line within the bounding box."""
    v = [((x * 256 + ox256) * 2, (y * 256 + oy256) * 2) for x, y in GEOMS[geom]]
    out = []
    for py in range(min(p[1] for p in v) // 512 - 1, max(p[1] for p in v) // 512 + 2):
        for px in range(min(p[0] for p in v) // 512 - 1, max(p[0] for p in v) // 512 + 2):
            q = (px * 512 + 256, py * 512 + 256)
            for i in range(3):
                a, b = v[i], v[(i + 1) % 3]
                if (b[0]-a[0])*(q[1]-a[1]) - (b[1]-a[1])*(q[0]-a[0]) == 0:
                    out.append(((px, py), (b[0]-a[0], b[1]-a[1])))
    return out

def observed(raw):
    """Set of program pixels that differ from the clear word (x = raw row, y = raw column)."""
    return {(i // SIZE, i % SIZE) for i in range(SIZE * SIZE) if raw[i*4:i*4+4] != CLEAR}

def transitions(sets):
    """sets: {step: pixelset}, sorted by step. Returns [(step_before, step_after, +added, -removed)]."""
    ks = sorted(sets); out = []
    for a, b in zip(ks, ks[1:]):
        if sets[a] != sets[b]:
            out.append((a, b, len(sets[b] - sets[a]), len(sets[a] - sets[b])))
    return out

def predict_range(geom, axis, lo, hi):
    return {k: covered(geom, k if axis == 'x' else 0, k if axis == 'y' else 0) for k in range(lo, hi + 1)}

def find_model_transitions(geom, axis):
    """Offsets (1/256) in [0,256) where the exact model changes from k-1 to k."""
    prev = None; out = []
    for k in range(0, 257):
        s = covered(geom, k if axis == 'x' else 0, k if axis == 'y' else 0)
        if prev is not None and s != prev: out.append(k)
        prev = s
    return out

def read_images(path):
    """Yield (row, raw_bytes) for raster_fine rows from a zip, run dir or results.csv."""
    p = Path(path)
    if p.suffix == '.zip':
        z = zipfile.ZipFile(p)
        csvname = next(n for n in z.namelist() if n.endswith('results.csv'))
        base = csvname[:-len('results.csv')]
        rows = list(csv.DictReader(io.TextIOWrapper(z.open(csvname), encoding='utf-8')))
        get = lambda r: z.read(f"{base}{r['family']}_{int(r['case_id']):04d}.rgba")
    else:
        csvp = p / 'results.csv' if p.is_dir() else p
        rows = list(csv.DictReader(csvp.open(newline='', encoding='utf-8')))
        get = lambda r: (csvp.parent / f"{r['family']}_{int(r['case_id']):04d}.rgba").read_bytes()
    for r in rows:
        if r['family'] == 'raster_fine':
            yield r, get(r)

def analyze(path):
    data = {}   # (window, direction) -> {step: observed set}
    for r, raw in read_images(path):
        win, direction = r['variant'].rsplit('_', 1)
        data.setdefault((win, direction), {})[int(r['parameter'])] = observed(raw)
    if not data:
        print('no raster_fine rows'); return 1
    bad = 0
    print('raster_fine: observed vs exact pixel-center model (ties by edge direction, provisional rule)')
    evidence = {}   # edge-direction class -> [covered, not covered]
    for name, geom, axis, lo, hi in WINS:
        fwd = data.get((name, 'forward'), {}); rev = data.get((name, 'reverse'), {})
        if not fwd and not rev: continue
        expected = set(range(lo, hi + 1))
        for label, d in (('forward', fwd), ('reverse', rev)):
            if set(d) != expected:
                print(f'[{name}] {label}: incomplete or unexpected steps; missing {sorted(expected - set(d))}, extra {sorted(set(d) - expected)}')
                bad += 1
        if not fwd and not rev: continue
        pred = predict_range(geom, axis, lo, hi)
        for k, obs in fwd.items():
            ox, oy = (k, 0) if axis == 'x' else (0, k)
            allin = covered(geom, ox, oy, ties=lambda dx, dy: True)
            for pix, (dx, dy) in tie_pixels(geom, ox, oy):
                if pix in allin:
                    c = (('-', '0', '+')[(dx > 0) - (dx < 0) + 1], ('-', '0', '+')[(dy > 0) - (dy < 0) + 1])
                    ev = evidence.setdefault(c, [0, 0]); ev[0 if pix in obs else 1] += 1
        ptr = [(a, b) for a, b, *_ in transitions(pred)]
        line = f'[{name}] geom {geom} {axis}-offset {lo}..{hi} (/256 px = {lo/256:.4f}..{hi/256:.4f})'
        print(line)
        print(f'  predicted transitions (step_before->step_after): {ptr or "none in window"}')
        for label, d in (('forward', fwd), ('reverse', rev)):
            if not d: continue
            otr = [(a, b) for a, b, *_ in transitions(d)]
            mism = {k: (len(d[k] - pred[k]), len(pred[k] - d[k])) for k in d if k in pred and d[k] != pred[k]}
            print(f'  {label}: {len(d)} steps, observed transitions {otr or "none"}; '
                  f'steps differing from model: {len(mism)}')
            for k in sorted(mism)[:6]:
                print(f'    step {k}: extra {mism[k][0]} px, missing {mism[k][1]} px vs model')
            bad += len(mism)
        if fwd and rev:
            diff = [k for k in fwd if k in rev and fwd[k] != rev[k]]
            print(f'  forward vs reverse differing steps: {diff or "none"}')
            bad += len(diff)
        if len(ptr) == 0 and (fwd or rev):
            print('  NOTE: window contains no model transition; only "no change" is tested here.')
    print('Tie evidence by edge direction (dx,dy signs, vertex order): covered / not covered pixels')
    for dx in '-0+':
        for dy in '-0+':
            if (dx, dy) == ('0', '0'): continue
            ev = evidence.get((dx, dy))
            print(f'  ({dx},{dy}): ' + (f'{ev[0]} covered / {ev[1]} not' + ('  MIXED' if ev[0] and ev[1] else '') if ev else 'no data'))
    print('Resolution: steps are 1/256 px; displacement smaller than 1/256 px cannot be seen, and '
          'a transition is localised only to the step pair shown.')
    return 1 if bad else 0

if __name__ == '__main__':
    if '--predict' in sys.argv:
        for g in GEOMS:
            for ax in 'xy':
                print(g, ax, find_model_transitions(g, ax))
        sys.exit(0)
    if len(sys.argv) < 2:
        print(__doc__); sys.exit(2)
    sys.exit(analyze(sys.argv[1]))
