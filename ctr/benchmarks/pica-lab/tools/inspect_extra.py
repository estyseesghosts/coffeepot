#!/usr/bin/env python3
"""Dump the exact extra pixels found by rasterfine.py in run_007."""
import csv, zipfile, io

SIZE = 256
CLEAR = bytes((0xFF, 0, 0, 0))
GEOMS = {
    'A': [(80, 48), (176, 48), (80, 176)],
    'B': [(100, 40), (212, 61), (124, 181)],
}

def covered_exact(geom, ox256=0, oy256=0):
    v = [((x * 256 + ox256) * 2, (y * 256 + oy256) * 2) for x, y in GEOMS[geom]]
    xs = [p[0] for p in v]; ys = [p[1] for p in v]
    x0, x1 = max(0, min(xs) // 512 - 1), min(SIZE - 1, max(xs) // 512 + 1)
    y0, y1 = max(0, min(ys) // 512 - 1), min(SIZE - 1, max(ys) // 512 + 1)
    def e(a, b, p): return (b[0]-a[0])*(p[1]-a[1]) - (b[1]-a[1])*(p[0]-a[0])
    out = set()
    for py in range(y0, y1 + 1):
        for px in range(x0, x1 + 1):
            p = (px * 512 + 256, py * 512 + 256)
            s = (e(v[0], v[1], p), e(v[1], v[2], p), e(v[2], v[0], p))
            if all(t > 0 for t in s) or all(t < 0 for t in s):
                out.add((px, py))
    return out

def observed(raw):
    return {(i // SIZE, i % SIZE) for i in range(SIZE * SIZE) if raw[i*4:i*4+4] != CLEAR}

z = zipfile.ZipFile(r'pica-lab\run-logs\run_007.zip')
csvname = 'run_007/results.csv'
rows = list(csv.DictReader(io.TextIOWrapper(z.open(csvname), encoding='utf-8')))
base = csvname[:-len('results.csv')]

# The three exceptions:
# [A4] geom A y-offset 128: extra 96 px
# [B2] geom B x-offset 128: extra 7 px
# [B4] geom B y-offset 120: extra 7 px

cases = [
    ('A4', 'A', 'y', 128),
    ('B2', 'B', 'x', 128),
    ('B4', 'B', 'y', 120),
]

for win_name, geom, axis, step in cases:
    row = None
    for r in rows:
        if r['family'] == 'raster_fine' and win_name in r['variant'] and int(r['parameter']) == step:
            row = r
            break
    if not row:
        print(f"[{win_name}] step {step}: row not found")
        continue

    raw = z.read(f"{base}{row['family']}_{int(row['case_id']):04d}.rgba")
    obs = observed(raw)
    ox = step if axis == 'x' else 0
    oy = step if axis == 'y' else 0
    exp = covered_exact(geom, ox, oy)

    extra = obs - exp
    missing = exp - obs

    print(f"\n=== [{win_name}] geom {geom}, {axis}-offset {step}/256 ({step/256:.4f} px) ===")
    print(f"  Observed: {len(obs)} px | Expected: {len(exp)} px | Extra: {len(extra)} | Missing: {len(missing)}")

    if extra:
        extra_sorted = sorted(extra)
        y_min = min(p[1] for p in extra_sorted)
        y_max = max(p[1] for p in extra_sorted)
        x_min = min(p[0] for p in extra_sorted)
        x_max = max(p[0] for p in extra_sorted)

        print(f"  Extra pixels ({len(extra)}), bounding box x:{x_min}-{x_max}, y:{y_min}-{y_max}")
        for py in range(y_min, y_max + 1):
            line = ""
            for px in range(x_min, x_max + 1):
                line += "#" if (px, py) in extra else "."
            print(f"    y={py:3d}: {line}")

        # Check if the extra pixels form a pattern
        # Are they all in one row?
        by_row = {}
        for px, py in extra_sorted:
            by_row.setdefault(py, []).append(px)
        print(f"  By row: {len(by_row)} row(s) affected")
        for py in sorted(by_row):
            print(f"    y={py}: x={sorted(by_row[py])} ({len(by_row[py])} px)")

        # Check: are the extra pixels all on one side of the edge?
        # For geom A y-offset 128/256: triangle (80,48+0.5),(176,48+0.5),(80,176+0.5)
        # The hypotenuse goes from (176,48) to (80,176) with 3:4 slope
        # At y=128, the hypotenuse x-coordinate...
        if geom == 'A':
            # Hypotenuse: (176,48) -> (80,176), dx=-96, dy=128, slope = -3/4
            # x(y) = 176 + (y-48)*(-96/128) = 176 - (y-48)*0.75
            # With y-offset 128/256: vertices at y = 48+0.5000, 48+0.5000, 176+0.5000
            # Hypotenuse goes from (176.5, 48.5) to (80.5, 176.5)
            # At pixel row py, the edge x = 176.5 + (py + 0.5 - 48.5) * (-96/128)
            #                      = 176.5 + (py - 48) * (-0.75)
            # Pixel covered if its center (px+0.5, py+0.5) is strictly inside
            print(f"  Hypotenuse edge at each affected row:")
            for py in sorted(by_row):
                edge_x = 176.5 + (py + 0.5 - 48.5) * (-96/128)
                print(f"    y={py}: edge_x = {edge_x:.6f}")
                for px in sorted(by_row[py]):
                    center_x = px + 0.5
                    rel = center_x - edge_x
                    print(f"      px={px}, center_x={center_x}, edge_x={edge_x:.6f}, offset={rel:+.6f}")

    if missing:
        print(f"  Missing pixels ({len(missing)}): {sorted(missing)}")
