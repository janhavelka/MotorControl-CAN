#!/usr/bin/env python3
"""Check finite runner CLI boundaries with serial imports forbidden in every child."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts/bench_probe.py"
VERSION = json.loads((ROOT / "library.json").read_text(encoding="utf-8"))["version"]
SERIAL_GUARD = "CLI_TEST_SERIAL_IMPORT_FORBIDDEN"
CHILD = '''
import importlib.abc
from pathlib import Path
import runpy
import sys
class NoSerial(importlib.abc.MetaPathFinder):
    def find_spec(self, fullname, path=None, target=None):
        if fullname.split(".")[0] == "serial":
            raise AssertionError("CLI_TEST_SERIAL_IMPORT_FORBIDDEN")
sys.meta_path.insert(0, NoSerial())
script = Path(sys.argv[1])
sys.path.insert(0, str(script.parent))
sys.argv = [str(script), *sys.argv[2:]]
runpy.run_path(str(script), run_name="__main__")
'''


def strings(value):
    """Yield JSON string values without relying on a particular presentation layout."""
    if isinstance(value, str):
        yield value
    elif isinstance(value, list):
        for item in value:
            yield from strings(item)
    elif isinstance(value, dict):
        for item in value.values():
            yield from strings(item)


def evidence_kinds(value):
    if isinstance(value, dict):
        if "evidence_kind" in value:
            yield value["evidence_kind"]
        for item in value.values():
            yield from evidence_kinds(item)
    elif isinstance(value, list):
        for item in value:
            yield from evidence_kinds(item)


class RunnerCLI(unittest.TestCase):
    def invoke(self, arguments, *, expected=0):
        """Each command runs in an empty directory and must leave it empty."""
        with tempfile.TemporaryDirectory(prefix="CAN runner CLI ") as directory:
            command = [argument.replace("{log}", str(Path(directory) / "retained.jsonl"))
                       for argument in arguments]
            result = subprocess.run([sys.executable, "-I", "-B", "-c", CHILD,
                                     str(SCRIPT), *command], cwd=directory,
                                    capture_output=True, text=True, timeout=15)
            self.assertEqual(list(Path(directory).iterdir()), [], "CLI wrote an artifact before live execution")
        self.assertNotIn(SERIAL_GUARD, result.stderr + result.stdout)
        self.assertNotIn("Traceback (most recent call last)", result.stderr)
        if expected == 0:
            self.assertEqual(result.returncode, 0, result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertTrue(result.stderr.strip(), "invalid arguments need a readable diagnostic")
        return result

    def live_arguments(self):
        return ["--port", "COM_NOT_A_REAL_FIXTURE", "--baud", "115200",
                "--profile", "lichuan_c", "--node", "10", "--can-bitrate", "500000",
                "--expected-library-version", VERSION, "--log", "{log}"]

    def plan(self, *arguments):
        result = self.invoke(["--plan-only", *arguments])
        value = json.loads(result.stdout)
        kinds = list(evidence_kinds(value))
        self.assertIn("planned", kinds)
        self.assertTrue(all(kind == "planned" for kind in kinds), kinds)
        return value

    def test_help_imports_no_serial_and_documents_offline_and_explicit_live_flags(self):
        result = self.invoke(["--help"])
        for flag in ("--plan-only", "--self-test", "--port", "--baud", "--profile", "--node",
                     "--can-bitrate", "--expected-library-version", "--log", "--scenario",
                     "--state-block", "--timeout", "--startup", "--request-timeout-ms", "--release"):
            self.assertIn(flag, result.stdout)

    def test_default_plan_is_one_read_only_quick_probe_without_fixture_defaults(self):
        value = self.plan()
        text = list(strings(value))
        self.assertIn("quick", text)
        self.assertTrue(any(item == "probe" or item.startswith("probe ") for item in text), text)
        self.assertNotIn("COM13", text)
        for mutation in ("enable", "motor-release", "alarm-clear", "position-clear", "recover"):
            self.assertNotIn(mutation, text)

    def test_all_explicit_read_scenarios_make_plans_without_serial_or_files(self):
        for scenario, command in (("quick", "probe"), ("probe", "probe"),
                                  ("identity", "read identity"), ("config", "read config"),
                                  ("state", "read state")):
            with self.subTest(scenario=scenario):
                value = self.plan("--scenario", scenario, "--log", "{log}")
                text = list(strings(value))
                self.assertIn(scenario, text)
                self.assertTrue(any(item == command or item.startswith(command + " ") for item in text), text)

    def test_state_plan_retains_the_requested_reviewed_block(self):
        for block in ("status", "monitors", "feedback"):
            with self.subTest(block=block):
                value = self.plan("--scenario", "state", "--state-block", block)
                self.assertTrue(any(item.startswith("read state " + block) for item in strings(value)))

    def test_self_test_reports_host_evidence_with_bounded_fake_stream(self):
        result = self.invoke(["--self-test"])
        value = json.loads(result.stdout)
        kinds = list(evidence_kinds(value))
        self.assertIn("host-tested", kinds)
        self.assertNotIn("live-observed", kinds)
        self.assertLessEqual(len(result.stdout.encode("utf-8")), 262144)

    def test_live_mode_requires_every_explicit_fixture_and_retention_argument(self):
        self.invoke([], expected=1)
        arguments = self.live_arguments()
        for flag in ("--port", "--baud", "--profile", "--node", "--can-bitrate",
                     "--expected-library-version", "--log"):
            with self.subTest(missing=flag):
                shortened = arguments[:]
                index = shortened.index(flag)
                del shortened[index:index + 2]
                self.invoke(shortened, expected=1)

    def test_invalid_live_values_fail_before_serial_or_log_creation(self):
        invalid = {
            "--port": ("",),
            "--log": ("",),
            "--node": ("0", "128", "-1", "1.5", "nan"),
            "--baud": ("0", "-1", "115200.0", "fast"),
            "--can-bitrate": ("0", "-1", "500000.0"),
            "--profile": ("ess_rs", "nanotec_n5"),
            "--expected-library-version": ("v0.1.0", "0.1", "0.1.0-beta", "latest"),
            "--timeout": ("0", "-1", "60.01", "nan", "inf"),
            "--startup": ("0", "-1", "60.01", "nan", "inf"),
            "--request-timeout-ms": ("0", "-1", "60001", "1.5"),
        }
        for flag, values in invalid.items():
            for value in values:
                with self.subTest(flag=flag, value=value):
                    arguments = self.live_arguments()
                    if flag in arguments:
                        arguments[arguments.index(flag) + 1] = value
                    else:
                        arguments += [flag, value]
                    self.invoke(arguments, expected=1)

    def test_plan_checks_the_same_numeric_bounds_and_accepts_inclusive_maxima(self):
        for flag, value in (("--node", "0"), ("--baud", "0"), ("--timeout", "nan"),
                            ("--startup", "0"), ("--request-timeout-ms", "60001")):
            with self.subTest(flag=flag):
                self.invoke(["--plan-only", flag, value], expected=1)
        self.plan("--node", "127", "--timeout", "60", "--startup", "60",
                  "--request-timeout-ms", "60000", "--release")
        self.plan("--node", "1", "--timeout", "0.001", "--startup", "0.001",
                  "--request-timeout-ms", "1")

    def test_campaign_repetition_and_unreviewed_routes_are_rejected_offline(self):
        for arguments in (("--campaign", "quick"), ("--count", "2"), ("--repeat", "2"),
                          ("--scenario", "motion"), ("--state-block", "errors"),
                          ("--state-block", "history"), ("--state-block", "inputs")):
            with self.subTest(arguments=arguments):
                self.invoke(["--plan-only", *arguments], expected=1)
        self.invoke(["--plan-only", "--self-test"], expected=1)


if __name__ == "__main__":
    unittest.main()
