import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

CHECKER = Path(__file__).resolve().parents[1] / "tools" / "check_docs.py"


class AnchorChecks(unittest.TestCase):
    def check_page(self, target, link):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "target.md").write_text(target, encoding="utf-8")
            (root / "index.md").write_text(link, encoding="utf-8")
            return subprocess.run([sys.executable, str(CHECKER), str(root)],
                                  text=True, capture_output=True, check=False)

    def test_named_anchor_and_heading(self):
        result = self.check_page('<a name="custom"></a>\n\n### ARM11\n',
                                 '[custom](target.md#custom) [heading](target.md#arm11)')
        self.assertEqual(result.returncode, 0, result.stdout)

    def test_attribute_heading_is_not_a_custom_anchor(self):
        result = self.check_page('### ARM11 {#arm11}\n', '[bad](target.md#arm11)')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('missing anchor', result.stdout)
        self.assertIn('unsupported heading anchor syntax', result.stdout)

    def test_fake_anchor_in_fenced_code_is_ignored(self):
        result = self.check_page('```html\n<a name="fake"></a>\n```\n',
                                 '[bad](target.md#fake)')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('missing anchor', result.stdout)


class StaleStatusChecks(unittest.TestCase):
    def run_checker(self, text, name="engineering/page.md"):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            page = root / "ctr" / name
            page.parent.mkdir(parents=True)
            page.write_text(text + "\n\n## Sources\n", encoding="utf-8")
            return subprocess.run([sys.executable, str(CHECKER), str(root)],
                                  text=True, capture_output=True, check=False)

    def test_stale_phrase_is_reported(self):
        result = self.run_checker("The engineering pages contain no measurements made for this wiki yet.")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("stale status", result.stdout)

    def test_stale_ok_marker_allows_the_line(self):
        result = self.run_checker("No measurements: contain no measurements made for this wiki. <!-- stale-ok -->")
        self.assertEqual(result.returncode, 0, result.stdout)

    def test_separate_chip_claim_is_reported(self):
        result = self.run_checker("The GPU is a separate chip, so a frame is limited by the slower side.")
        self.assertIn("same SoC", result.stdout)

    def test_stale_phrase_in_code_block_is_ignored(self):
        result = self.run_checker("```\nno results yet from the benchmark suite\n```")
        self.assertEqual(result.returncode, 0, result.stdout)

    def test_current_status_wording_passes(self):
        result = self.run_checker("The suite has Old 3DS results; the 804 MHz runs are not yet done.")
        self.assertEqual(result.returncode, 0, result.stdout)


if __name__ == "__main__":
    unittest.main()
