"""Audit regressions using archived hardware CSVs and synthetic capture controls."""
import contextlib
import csv
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest

LAB = Path(__file__).resolve().parents[1] / 'ctr/benchmarks/pica-lab'
sys.path.insert(0, str(LAB / 'tools'))
import analyze
import interrogate
import timing_iso


class PicaLabAuditTests(unittest.TestCase):
    def output(self, fn, *args):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            status = fn(*args)
        return status, out.getvalue()

    def rows(self, run):
        with (LAB / 'run-logs' / run / 'results.csv').open(newline='') as stream:
            return list(csv.DictReader(stream))

    def write_rows(self, path, rows):
        with path.open('w', newline='') as stream:
            writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
            writer.writeheader()
            writer.writerows(rows)

    def test_run_010_repeated_controls(self):
        rows = self.rows('run_010')
        self.assertEqual(sum(r['variant'] == 'clear' for r in rows), 2)
        status, report = self.output(interrogate.run, LAB / 'run-logs/run_010/results.csv')
        self.assertEqual(status, 0)
        self.assertTrue(json.loads(report)['observations']['selftest']['pass'])
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / 'results.csv'
            # An incomplete repeated invocation must still fail.
            self.write_rows(path, rows + [next(r for r in rows if r['variant'] == 'clear')])
            self.assertEqual(self.output(interrogate.run, path)[0], 1)

    def test_run_014_timing_rows_have_no_images(self):
        rows = [r for r in self.rows('run_014') if r['family'] == 'timing_iso']
        self.assertEqual(len(rows), 320)
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / 'results.csv'
            self.write_rows(path, rows)
            status, report = self.output(analyze.summarize, path)
            self.assertEqual(status, 0)
            self.assertEqual(json.loads(report)['issues'], [])
            # Missing raster images must continue to be detected.
            self.write_rows(path, [next(r for r in self.rows('run_014') if r['family'] == 'raster')])
            self.assertEqual(self.output(analyze.summarize, path)[0], 1)

    def test_v06_vblank_column(self):
        # Actual v0.6 rows exercise the legacy timing_wall path.
        status, report = self.output(timing_iso.run, LAB / 'run-logs/run_015/results.csv')
        self.assertEqual(status, 0)
        self.assertIn('PRE-V0.7', report)
        self.assertIn('vblank', report)
        rows = self.rows('run_015')
        import statistics
        variant = next(r['variant'] for r in rows if r['family'] == 'timing_iso')
        expected = statistics.median(float(r['gpu_queue_ms']) * 1000 for r in rows
                                     if r['family'] == 'timing_wall' and r['variant'] == variant)
        line = next(line for line in report.splitlines() if line.startswith(variant))
        self.assertEqual(line.split()[6], f'{expected:.0f}')

    def test_synthetic_primary_capture_channel_order(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            path = root / 'results.csv'
            def write_probe(word):
                raw = word.to_bytes(4, 'little') * analyze.SIZE**2
                (root / 'selftest_0000.rgba').write_bytes(raw)
                self.write_rows(path, [dict(case_id='0', family='selftest', variant='red',
                    parameter='0', crc32=f'{analyze.crc32(raw):08X}',
                    nonclear_pixels=str(analyze.SIZE**2), center_abgr_u32=f'{word:08X}')])
            write_probe(0xFF0000FF)
            self.assertEqual(self.output(analyze.summarize, path)[0], 0)
            write_probe(0x0000FFFF)  # correct checksum, wrong channel
            status, report = self.output(analyze.summarize, path)
            self.assertEqual(status, 1)
            self.assertIn('channel order mismatch', report)

    def test_synthetic_v06_wall_rows(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / 'results.csv'
            common = dict(variant='s3_n1_blk', parameter='0', crc32='00000000',
                          nonclear_pixels='0', center_abgr_u32='00000000')
            self.write_rows(path, [dict(common, case_id='0', family='timing_iso',
                gpu_queue_ms='0.150', p3d_us='50000'), dict(common, case_id='1',
                family='timing_wall', gpu_queue_ms='16.789', p3d_us='-1')])
            self.assertEqual(self.output(analyze.summarize, path)[0], 0)
            status, report = self.output(timing_iso.run, path)
            self.assertEqual(status, 0)
            line = next(line for line in report.splitlines() if line.startswith('s3_n1_blk'))
            self.assertEqual(line.split()[6], '16789')


if __name__ == '__main__':
    unittest.main()
