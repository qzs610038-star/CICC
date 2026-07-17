import json
import shutil
import tempfile
import unittest
from pathlib import Path

import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import g2_run_bundle as bundle


ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / "fixtures" / "g2_negative"


def event(seq, kind, frame=0, round_id=0, arm=0):
    return (f"@E|v=1|seq={seq}|event={kind}|round={round_id}|frame={frame}|cfg=1|flags=71|"
            f"class=RED_CUBE|decision=SKIP|reason=NONE|ack=0|arm={arm}|source=fake_transport")


class G2RunBundleTests(unittest.TestCase):
    def make_bundle(self, lines):
        directory = Path(tempfile.mkdtemp(prefix="g2-bundle-test-"))
        raw = directory / "input.log"
        raw.write_text("\n".join(lines) + "\nTEST_SUMMARY total=1 passed=1 failures=0\n", encoding="utf-8")
        args = type("Args", (), {"run_dir": str(directory), "raw_log": str(raw),
                                   "repo_root": str(ROOT), "compiler": "fixture",
                                   "test_command": "fixture", "exit_code": 0})()
        bundle.create_bundle(args)
        return directory

    def assert_fixture_rejected(self, name):
        lines = (FIXTURES / name).read_text(encoding="utf-8").splitlines()
        directory = self.make_bundle(lines)
        self.assertTrue(bundle.validate_bundle(directory), name)
        shutil.rmtree(directory)

    def test_valid_bundle_passes(self):
        directory = self.make_bundle([event(1, "BOOT"), event(2, "SNAPSHOT_ACCEPT", 1, 1),
                                      event(3, "ACK", 1, 1), event(4, "ROUND_RESULT", 1, 1)])
        self.assertEqual(bundle.validate_bundle(directory), [])
        shutil.rmtree(directory)

    def test_negative_fixtures_are_rejected(self):
        for name in ("missing_boot.fixture", "missing_ack.fixture", "ack_mismatch.fixture",
                     "malformed_event.fixture", "event_order.fixture", "duplicate_result.fixture",
                     "arm_nonzero.fixture"):
            with self.subTest(name=name):
                self.assert_fixture_rejected(name)

    def test_mixed_hash_is_rejected(self):
        directory = self.make_bundle([event(1, "BOOT"), event(2, "SNAPSHOT_ACCEPT", 1, 1),
                                      event(3, "ACK", 1, 1)])
        manifest_path = directory / "manifest.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        manifest["input_sha256"]["raw.log"] = "0" * 64
        manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
        self.assertIn("mixed hash", bundle.validate_bundle(directory))
        shutil.rmtree(directory)


if __name__ == "__main__":
    unittest.main()
