#!/usr/bin/env python3
"""Make conservative, falsifiable observations from a PICA Lab capture run.

Does not infer unobserved cache sizes, pipeline ordering, or internal precision.
"""
import csv
import json
import statistics
import sys
from collections import defaultdict
from pathlib import Path
from analyze import resolve_csv, image_id


def load_rows(path):
    with open(path, newline='', encoding='utf-8') as stream:
        return list(csv.DictReader(stream))


def case_image(row, base):
    name = f"{row['family']}_{int(row['case_id']):04d}.rgba"
    path = base / name
    return path.read_bytes() if path.is_file() else None


def run(path):
    rows = load_rows(path)
    root = path.parent
    ident = lambda r: image_id(r, root)   # SHA-256 when captured; CSV CRC-32 otherwise
    families = defaultdict(list)
    for row in rows:
        families[row['family']].append(row)
    report = {'observations': {}, 'caveats': []}
    controls = families['selftest']
    clears = [r for r in controls if r['variant'] == 'clear']
    rects = [r for r in controls if r['variant'] == 'rect']
    # Each invocation appends clear, rect, rect; RUN ALL may be repeated in one launch.
    baseline = [r for r in controls if r['variant'] in ('clear', 'rect')]
    valid = bool(clears) and len(rects) == 2 * len(clears)
    valid = valid and [r['variant'] for r in baseline] == ['clear', 'rect', 'rect'] * len(clears)
    if valid:
        valid = (all(int(r['nonclear_pixels']) == 0 for r in clears) and
                 all(ident(r) is not None for r in rects) and
                 len({ident(r) for r in rects}) == 1 and
                 all(100 < int(r['nonclear_pixels']) < 256 * 256 for r in rects))
    # The hardware and image channel order still need physical checks.
    report['observations']['selftest'] = {'completed': len(controls), 'pass': valid}
    primaries = {'red': 0xFF0000FF, 'green': 0x00FF00FF, 'blue': 0x0000FFFF}
    color_rows = [r for r in controls if r['variant'] in primaries]
    channels = len(color_rows) == 3 * len(clears) and bool(clears)
    channels = channels and all(sum(r['variant'] == name for r in color_rows) == len(clears)
                                for name in primaries)
    for r in color_rows:
        raw = case_image(r, root)
        offset = (100 * 256 + 100) * 4
        channels = channels and raw is not None and len(raw) == 256*256*4
        channels = channels and int.from_bytes(raw[offset:offset+4], 'little') == primaries[r['variant']]
    report['observations']['selftest']['channel_order_verified'] = bool(channels)
    if not channels:
        report['caveats'].append('Primary-color captures absent or invalid; channel order is unverified.')
    if not valid:
        report['caveats'].append('Baseline failed or absent; do not infer GPU behavior.')

    raster = families['raster']
    by_direction = {direction: {int(r['parameter']): r for r in raster if r['variant'] == direction}
                    for direction in ('forward', 'reverse')}
    shared = sorted(set(by_direction['forward']) & set(by_direction['reverse']))
    inconsistent = []
    if shared:
        for offset in shared:
            f = by_direction['forward'][offset]
            r = by_direction['reverse'][offset]
            if ident(f) != ident(r):
                inconsistent.append(offset)
    forward = by_direction['forward']
    steps = sorted(forward)
    transitions = [step for previous, step in zip(steps, steps[1:])
                   if step == previous + 1 and
                   ident(forward[step]) != ident(forward[previous])]
    report['observations']['raster'] = {
        'forward_samples': len(forward), 'reverse_samples': len(by_direction['reverse']),
        'matched_offsets': len(shared), 'direction_disagreements': inconsistent,
        'observed_transition_steps': transitions,
        'tested_step_pixels': '1/64',
        'interpretation': 'Transitions demonstrate output changes only; they do not establish a unique subpixel lattice.'
    }
    edges = families['edge']
    if len(edges) == 2:
        a, b = [case_image(r, root) for r in edges]
        if a is not None and b is not None and len(a) == len(b):
            changed = sum(a[i:i+4] != b[i:i+4] for i in range(0, len(a), 4))
            report['observations']['edge'] = {
                'pixel_mismatches': changed,
                'nonclear_pixels': [int(r['nonclear_pixels']) for r in edges],
                'note': 'A full 128x128 quad is 16384 px; about half means a triangle was lost (harness bug).'}
        else:
            report['caveats'].append('Edge images missing; hash equality is a weaker test than pixel comparison.')
    cov = []
    for r in families['edge_cov']:
        raw = case_image(r, root)
        if raw is None:
            continue
        gaps = overlaps = 0
        for y in range(256):
            for x in range(256):
                red = raw[(y*256+x)*4+3]   # word 0xRRGGBBAA, little-endian bytes
                if red >= 0x60: overlaps += 1
                elif red < 0x20 and 68 <= x < 188 and 68 <= y < 188: gaps += 1
        cov.append({'variant': r['variant'], 'gaps': gaps, 'overlaps': overlaps})
    if cov:
        bad = [c for c in cov if c['gaps'] or c['overlaps']]
        report['observations']['edge_cov'] = {
            'cases': len(cov), 'watertight_cases': len(cov) - len(bad), 'failing': bad,
            'note': 'gap = interior pixel with no coverage; overlap = pixel hit twice (additive).'}
    for family in ('tev', 'blend'):
        rs = sorted(families[family], key=lambda r: int(r['parameter']))
        if rs:
            report['observations'][family] = {
                'parameter_to_center_word': {
                    r['parameter']: r['center_abgr_u32'] for r in rs
                },
                'unique_center_outputs': len({r['center_abgr_u32'] for r in rs}),
                'note': 'These are observed 8-bit framebuffer words, not internal TEV/blend precision.'
            }
    if families['raster_fine']:
        report['observations']['raster_fine'] = {
            'cases': len(families['raster_fine']),
            'note': 'Run tools/rasterfine.py on this run for the model comparison.'}
    if families['perf2']:
        report['observations']['perf2'] = {
            'cases': len(families['perf2']),
            'note': 'Run tools/perf2.py on this run for the tris/pixels fit.'}
    perf = families['perf']
    if perf:
        by = defaultdict(lambda: ([], []))
        for r in perf:
            by[r['variant']][0].append(float(r['gpu_queue_ms']))
            if r.get('p3d_us') not in (None, '', '-1'):
                by[r['variant']][1].append(float(r['p3d_us']))
        base = by.get('empty')
        bg = statistics.median(base[0]) if base and base[0] else 0.0
        bp = statistics.median(base[1]) if base and base[1] else 0.0
        table = {}
        for name, (g, p) in by.items():
            row = {'n': len(g), 'gpu_ms_median': statistics.median(g), 'gpu_ms_min': min(g), 'gpu_ms_max': max(g),
                   'gpu_ms_over_empty': statistics.median(g) - bg}
            if p:
                row.update(p3d_us_median=statistics.median(p), p3d_us_over_empty=statistics.median(p) - bp)
            table[name] = row
        report['observations']['perf'] = {
            'variants': table,
            'note': 'Compare against the minimal-submission baseline, not a universal timing floor. Pre-v0.7 timing lacks guaranteed frame association; rerun before inferring GPU costs.'}
        if 'empty' not in by:
            report['caveats'].append('Perf run has no "empty" floor variant; deltas are not interpretable.')
    if raster and len(shared) != 64:
        report['caveats'].append('Raster sweep missing at least one forward/reverse pair.')
    if inconsistent:
        report['caveats'].append('Raster direction mismatch: state, synchronization, or nondeterminism not excluded.')
    print(json.dumps(report, indent=2))
    return 0 if valid else 1


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print('Usage: python3 tools/interrogate.py /path/to/results.csv|run_NNN.zip', file=sys.stderr)
        raise SystemExit(2)
    raise SystemExit(run(resolve_csv(sys.argv[1])))
