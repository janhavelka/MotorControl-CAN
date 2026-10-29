#!/usr/bin/env python3
"""Adversarial checks for verifier command execution and retained failures."""
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("verify", ROOT / "scripts/verify.py")
verify = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verify)


class VerificationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="can verification ")
        self.addCleanup(self.temporary.cleanup)
        self.base = Path(self.temporary.name)
        self.runner = verify.Runner(self.base, "quick")

    def last(self):
        summary = json.loads((self.base / "summary.json").read_text(encoding="utf-8"))
        step = summary["steps"][-1]
        self.assertTrue(Path(step["log"]).is_file())
        self.assertIn("command", Path(step["log"]).read_text(encoding="utf-8"))
        return step

    def test_literal_arguments_and_success_are_retained(self):
        value = "space ; $(do-not-execute) & literal"
        output = self.runner.run("literal", [sys.executable, "-c", "import sys; print(sys.argv[1])", value])
        self.assertEqual(output.strip(), value)
        self.assertEqual(self.last()["status"], "passed")

    def test_failure_is_not_converted_to_success(self):
        with self.assertRaises(verify.VerificationFailure):
            self.runner.run("failure", [sys.executable, "-c", "raise SystemExit(7)"])
        self.assertEqual(self.last()["returncode"], 7)

    def test_unavailable_tool_is_nonzero_evidence(self):
        with self.assertRaises(verify.VerificationFailure) as caught:
            self.runner.run("missing", [str(self.base / "missing-tool")])
        self.assertEqual(caught.exception.status, "unavailable")
        self.assertEqual(self.last()["status"], "unavailable")

    def test_timeout_retains_failure_and_command(self):
        with self.assertRaises(verify.VerificationFailure):
            self.runner.run("timeout", [sys.executable, "-c", "import time; time.sleep(10)"], timeout=0.1)
        self.assertEqual(self.last()["status"], "failed")
        self.assertIn("exceeded", Path(self.last()["log"]).read_text(encoding="utf-8"))

    def test_skips_and_empty_test_runs_are_not_passing(self):
        for output in ("No tests were found!!!", "Ran 0 tests", "***Not Run", "DISABLED", "Skipped", "OK (skipped=1)"):
            with self.subTest(output=output), self.assertRaises(verify.VerificationFailure):
                verify.check_test_output(output)
        verify.check_test_output("100% tests passed, 0 tests failed out of 3")

    def test_new_run_keeps_previous_evidence(self):
        previous = self.runner.directory
        verify.Runner(self.base, "full")
        self.assertTrue((previous / "summary.json").is_file())

    def test_empty_documentation_extraction_fails(self):
        header = self.base / "include/MotorControlCAN/Public.h"
        header.parent.mkdir(parents=True)
        header.write_text("/** @file Public.h */", encoding="utf-8")
        index = self.base / ".build/doxygen/xml/index.xml"
        index.parent.mkdir(parents=True)
        index.write_text("<doxygenindex/>", encoding="utf-8")
        with self.assertRaises(verify.VerificationFailure):
            verify.check_documentation(self.base)

    def test_exact_doxygen_version_accepts_only_clean_pinned_source_suffix(self):
        self.assertTrue(verify.doxygen_matches("1.13.2"))
        self.assertTrue(verify.doxygen_matches("1.13.2 (" + verify.DOXYGEN_REVISION + ")"))
        for invalid in ("1.13.20", "1.13.1", "1.13.2 dirty", "1.13.2 (abcdef123)",
                        "1.13.2 (" + verify.DOXYGEN_REVISION + "*)"):
            self.assertFalse(verify.doxygen_matches(invalid), invalid)

    def test_optimized_python_is_rejected(self):
        import subprocess
        result = subprocess.run([sys.executable, "-O", str(ROOT / "scripts/verify.py"),
                                 "--build-dir", str(self.base / "optimized")],
                                capture_output=True, text=True, timeout=10)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("without -O", result.stderr)


if __name__ == "__main__":
    unittest.main()
