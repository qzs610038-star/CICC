from __future__ import annotations

import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


TOOL = Path(__file__).resolve().parents[1] / "verify_mycobot_g4_batch.py"


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


class G4BatchVerifierTests(unittest.TestCase):
    def make_batch(self, root: Path) -> Path:
        texts = {
            "project.xml": "project",
            "periphery.xml": "periphery",
            "constraints.sdc": "constraints",
            "top.v": "module top; endmodule",
            "soc.h": "#define UART0_BASE 0x40000000\n#define CLINT_BASE 0x02000000\n",
            "linker.ld": "ENTRY(_start)\n",
            "startup.S": "_start:\n",
            "firmware.elf": "ELF",
            "bitstream.bit": "BITSTREAM",
            "map.log": "MAP PASS\n",
            "pnr.log": "PNR PASS\n",
            "sta.log": "STA PASS\n",
            "hello1.log": "CPU HELLO build=g4-test-001\n",
            "hello2.log": "CPU HELLO build=g4-test-001\n",
            "hello3.log": "CPU HELLO build=g4-test-001\n",
        }
        for name, text in texts.items():
            (root / name).write_text(text, encoding="utf-8")
        asset_names = {
            "project_xml": "project.xml",
            "periphery_xml": "periphery.xml",
            "sdc": "constraints.sdc",
            "top": "top.v",
            "soc_h": "soc.h",
            "linker": "linker.ld",
            "startup": "startup.S",
            "elf": "firmware.elf",
            "bitstream": "bitstream.bit",
            "map_log": "map.log",
            "pnr_log": "pnr.log",
            "sta_log": "sta.log",
        }
        manifest = {
            "schema": "mycobot-g4-batch-v1",
            "build_id": "g4-test-001",
            "commit": "0123456789abcdef",
            "efinity_version": "2025.2.288.4.15",
            "profile": "arm_bringup",
            "backend": "disabled",
            "assets": asset_names,
            "sha256": {name: digest(root / filename) for name, filename in asset_names.items()},
            "log_assertions": {
                "map_log": ["MAP PASS"],
                "pnr_log": ["PNR PASS"],
                "sta_log": ["STA PASS"],
            },
            "cpu_hello_text": "CPU HELLO",
            "hello_runs": [
                {"log": f"hello{index}.log", "sha256": digest(root / f"hello{index}.log")}
                for index in range(1, 4)
            ],
        }
        path = root / "batch.json"
        path.write_text(json.dumps(manifest), encoding="utf-8")
        return path

    def run_tool(self, manifest: Path) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [sys.executable, str(TOOL), "--manifest", str(manifest)],
            text=True,
            capture_output=True,
            check=False,
        )

    def test_accepts_complete_consistent_batch(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            manifest = self.make_batch(Path(temp))
            result = self.run_tool(manifest)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(json.loads(result.stdout)["result"], "PASS")

    def test_rejects_mixed_batch_or_provisional_bsp(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            manifest = self.make_batch(root)
            (root / "soc.h").write_text("#define STANDALONE_TEST 1\n", encoding="utf-8")
            result = self.run_tool(manifest)
            self.assertEqual(result.returncode, 2)
            report = json.loads(result.stdout)
            self.assertEqual(report["result"], "FAIL")
            self.assertTrue(any("sha256 mismatch for soc_h" in item for item in report["errors"]))
            self.assertTrue(any("forbidden provisional token" in item for item in report["errors"]))


if __name__ == "__main__":
    unittest.main()
