#!/usr/bin/env python3
"""Validates host tools only; NOT a 3DS GPU test."""
import tempfile
from pathlib import Path
from analyze import BYTES,SIZE,crc32,load,counts,ppm,diff
raw=bytes((0xff,0,0,0))*(SIZE*SIZE)
assert len(raw)==BYTES and counts(raw)==(0,0xff)
import zlib
assert crc32(raw)==zlib.crc32(raw)&0xffffffff
with tempfile.TemporaryDirectory() as d:
    path=Path(d)/'test.rgba';path.write_bytes(raw)
    assert load(path)==raw and diff(path,path)==0
    dest=Path(d)/'test.ppm';ppm(raw,dest)
    assert dest.read_bytes().startswith(b'P6\n256 256\n255\n')
print('Host analyzer self-test: PASS (hardware untested)')

# --- raster_fine / perf2 tooling, synthetic data (tests plumbing, NOT hardware) ---
import csv, io, zipfile, rasterfine, perf2, analyze
def synth_zip(path, perturb=False):
    rows = []; z = zipfile.ZipFile(path, 'w'); cid = 0
    for name, geom, axis, lo, hi in rasterfine.WINS:
        for direction in ('forward', 'reverse'):
            for k in range(lo, hi + 1):
                s = rasterfine.covered(geom, k if axis == 'x' else 0, k if axis == 'y' else 0)
                img = bytearray(bytes((0xFF, 0, 0, 0)) * (SIZE*SIZE))
                for px, py in s:
                    i = (px*SIZE + py)*4; img[i:i+4] = bytes((0xFF, 0xFF, 0xFF, 0xFF))
                if perturb and name == 'A1' and k == 30 and direction == 'forward':
                    img[:4] = bytes((0xFF,) * 4)
                z.writestr(f'run_999/raster_fine_{cid:04d}.rgba', bytes(img))
                rows.append([cid, 'raster_fine', f'{name}_{direction}', k, f'{analyze.crc32(bytes(img)):08X}', len(s), '0', '0', -1]); cid += 1
    buf = io.StringIO(); w = csv.writer(buf)
    w.writerow('case_id,family,variant,parameter,crc32,nonclear_pixels,center_abgr_u32,gpu_queue_ms,p3d_us'.split(','))
    w.writerows(rows); z.writestr('run_999/results.csv', buf.getvalue()); z.close()
import contextlib
with tempfile.TemporaryDirectory() as d:
    synth_zip(Path(d)/'ok.zip')
    with contextlib.redirect_stdout(io.StringIO()): assert rasterfine.analyze(Path(d)/'ok.zip') == 0
    synth_zip(Path(d)/'bad.zip', perturb=True)
    with contextlib.redirect_stdout(io.StringIO()): assert rasterfine.analyze(Path(d)/'bad.zip') == 1
# perf2 fit recovers known coefficients
a, b, c = solve = 0.6, 0.005, 235.0
pts = [(t, px) for t, px in [(0,0),(8000,24000),(6000,24000),(3000,24000),(1000,24000),(256,768),(256,4096),(256,30000),(256,500000),(2,131072)]]
sol = perf2.solve([[1, t, px] for t, px in pts], [c + a*t + b*px for t, px in pts])
assert all(abs(x - y) < 1e-6 for x, y in zip(sol, (c, a, b))), sol
# the exact model reproduces the v0.3 hardware raster transitions (run_003) when sampled at 1/64 px
rl = Path(__file__).parent.parent/'run-logs'/'run_003.zip'
if rl.exists():
    sets = {s: rasterfine.covered('A', s*4, 0) for s in range(64)}
    assert [b for _, b, *_ in rasterfine.transitions(sets)] == [9, 25, 32, 41, 57]
print('raster_fine/perf2 tool self-test: PASS (synthetic; hardware untested)')

# --- v0.7 timing columns, compare and sha256 (synthetic) ---
import timing_iso
V07 = 'case_id,family,variant,parameter,crc32,nonclear_pixels,center_abgr_u32,gpu_queue_ms,p3d_us,begin_us,frameend_us,drain_us,perflog_us,assoc'
with tempfile.TemporaryDirectory() as d:
    p = Path(d)/'results.csv'
    lines = [V07]
    for i, (v, p3d, drain) in enumerate([('s3_n1_blk', 30, 180), ('s3_n1_blk', 50000, 180), ('s8_n256_blk', 62, 210)]):
        lines.append(f'{i},timing_iso,{v},{i},00000000,0,00000000,0.150000,{p3d},12000.0,95.0,{drain}.0,40.0,1')
    p.write_text('\n'.join(lines) + '\n')
    out = io.StringIO()
    with contextlib.redirect_stdout(out): assert timing_iso.run(p) == 0
    assert 'drain' in out.getvalue() and '1/2' in out.getvalue(), out.getvalue()   # one anomalous rep of two
    # compare: identical runs give 0 differences, a one-byte change gives 1
    synth_zip(Path(d)/'a.zip'); synth_zip(Path(d)/'b.zip'); synth_zip(Path(d)/'c.zip', perturb=True)
    with contextlib.redirect_stdout(io.StringIO()):
        assert analyze.compare([Path(d)/'a.zip', Path(d)/'b.zip']) == 0
        assert analyze.compare([Path(d)/'a.zip', Path(d)/'c.zip']) == 1
assert analyze.sha256(b'') == 'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855'
assert not analyze.has_image({'family': 'timing_iso'}) and analyze.has_image({'family': 'raster_fine'})
print('timing/compare tool self-test: PASS (synthetic; hardware untested)')
