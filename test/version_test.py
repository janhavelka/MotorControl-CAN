#!/usr/bin/env python3
"""Exercise version sync/check through temporary, isolated package copies."""

import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class VersionTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="motorcontrol-can-version-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        (self.root / "scripts").mkdir()
        self.script = self.root / "scripts/generate_version.py"
        shutil.copy2(ROOT / "scripts/generate_version.py", self.script)
        self.header = self.root / "include/MotorControlCAN/Version.h"
        self.metadata = self.root / "library.json"
        self.write_metadata({"version": "12.34.56"})

    def write_metadata(self, value):
        self.metadata.write_text(json.dumps(value), encoding="utf-8")

    def run_generator(self, command):
        return subprocess.run([sys.executable, str(self.script), command], cwd=self.root,
                              capture_output=True, text=True, timeout=10)

    def assert_success(self, command):
        result = self.run_generator(command)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result

    def test_sync_is_deterministic_and_check_accepts_current_header(self):
        self.assert_success("sync")
        original = self.header.read_bytes()
        for declaration in (b"uint16_t VERSION_MAJOR = 12;", b"uint16_t VERSION_MINOR = 34;",
                            b"uint16_t VERSION_PATCH = 56;", b"uint32_t VERSION_CODE = 123456;",
                            b'const char* VERSION = "12.34.56";'):
            self.assertIn(declaration, original)
        self.assertNotIn(b"\r", original)
        timestamp = self.header.stat().st_mtime_ns
        self.assert_success("check")
        self.assert_success("sync")
        self.assertEqual(self.header.read_bytes(), original)
        self.assertEqual(self.header.stat().st_mtime_ns, timestamp)

    def test_check_rejects_missing_header_without_creating_it(self):
        result = self.run_generator("check")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("missing or stale", result.stderr)
        self.assertFalse(self.header.exists())

    def test_check_rejects_stale_header_without_overwriting_it(self):
        self.assert_success("sync")
        original = self.header.read_bytes()
        self.write_metadata({"version": "12.34.57"})
        result = self.run_generator("check")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("missing or stale", result.stderr)
        self.assertEqual(self.header.read_bytes(), original)
        self.assert_success("sync")
        self.assertIn(b'VERSION = "12.34.57"', self.header.read_bytes())
        self.assert_success("check")

    def test_invalid_metadata_is_rejected_without_creating_a_header(self):
        for metadata in ({}, [], {"version": 1}, {"version": None}, {"version": True},
                         *({"version": value} for value in
                           ("1.2", "1.2.3.4", "01.2.3", "1.02.3", "1.2.03", "-1.2.3",
                            "1.2.3-dev", "1.2.3\n", "65536.0.0", "1.100.0", "1.0.100"))):
            with self.subTest(metadata=metadata):
                self.write_metadata(metadata)
                result = self.run_generator("sync")
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Version generation failed", result.stderr)
                self.assertNotIn("Traceback", result.stderr)
                self.assertFalse(self.header.exists())

    def test_numeric_encoding_boundary(self):
        self.write_metadata({"version": "65535.99.99"})
        self.assert_success("sync")
        self.assertIn(b"uint32_t VERSION_CODE = 655359999;", self.header.read_bytes())
        self.assert_success("check")

    def test_missing_or_malformed_json_reports_failure(self):
        self.metadata.unlink()
        result = self.run_generator("sync")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Version generation failed", result.stderr)
        self.metadata.write_text("{malformed", encoding="utf-8")
        result = self.run_generator("sync")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Version generation failed", result.stderr)
        self.assertFalse(self.header.exists())


if __name__ == "__main__":
    unittest.main()
