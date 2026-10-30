#!/usr/bin/env python3
"""Exercise repository-check failures in small disposable package fixtures."""

import contextlib
import copy
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import check_repository as checker


# Independent fixture inventory: deleting a checker rule must not weaken tests.
FILES = {
    "CMakeLists.txt", "cmake/MotorControlCANConfig.cmake.in", "library.json",
    "LICENSE", "README.md", "CHANGELOG.md", "CONTRIBUTING.md", "SECURITY.md",
    "Doxyfile", ".clang-format", ".github/workflows/verify.yml",
    "docs/README.md", "docs/getting_started.md", "docs/verification.md", "docs/units_status.md", "docs/axis_contract.md", "docs/canopen_services.md", "docs/reads.md", "docs/driver_settings.md", "docs/io_settings.md",
    "include/MotorControlCAN/MotorControlCAN.h", "include/MotorControlCAN/Status.h",
    "include/MotorControlCAN/Endpoint.h", "include/MotorControlCAN/Version.h",
    "include/MotorControlCAN/Units.h", "src/Units.cpp", "test/units_test.cpp",
    "include/MotorControlCAN/Axis.h", "src/Axis.cpp", "test/axis_test.cpp",
    "examples/units_preview/CMakeLists.txt", "examples/units_preview/main.cpp", "examples/units_preview/README.md",
    "include/MotorControlCAN/can/Frame.h", "src/Endpoint.cpp", "src/can/Frame.cpp",
    "include/MotorControlCAN/canopen/Sdo.h", "src/canopen/Sdo.cpp",
    "include/MotorControlCAN/canopen/SdoTransfer.h", "test/sdo_transfer_test.cpp",
    "test/sdo_reconciliation_test.cpp",
    "include/MotorControlCAN/canopen/Network.h", "src/canopen/Network.cpp", "test/network_codec_test.cpp", "test/network_events_test.cpp",
    "adapters/canopennode/NetworkState.h", "adapters/canopennode/Network.cpp",
    "adapters/canopennode/Nmt.h", "adapters/canopennode/Nmt.cpp", "test/nmt_codec_test.cpp", "test/nmt_services_test.cpp",
    "include/MotorControlCAN/canopen/Pdo.h", "src/canopen/Pdo.cpp",
    "include/MotorControlCAN/canopen/PdoState.h", "include/MotorControlCAN/ReadOperation.h",
    "include/MotorControlCAN/profiles/lichuan_c/Reads.h", "src/profiles/lichuan_c/Reads.cpp",
    "test/reads_test.cpp", "test/driver_settings_test.cpp", "test/io_settings_test.cpp", "test/settings_write_test.cpp",
    "include/MotorControlCAN/profiles/lichuan_c/IoSettings.h", "include/MotorControlCAN/profiles/lichuan_c/Settings.h", "src/profiles/lichuan_c/Settings.cpp",
    "examples/io_settings_preview/main.cpp", "examples/io_settings_preview/CMakeLists.txt", "examples/io_settings_preview/README.md",
    "include/MotorControlCAN/profiles/lichuan_c/DriverSettings.h",
    "examples/driver_settings_preview/main.cpp", "examples/driver_settings_preview/CMakeLists.txt", "examples/driver_settings_preview/README.md", "examples/reads_preview/main.cpp", "examples/reads_preview/CMakeLists.txt", "examples/reads_preview/README.md",
    "include/MotorControlCAN/profiles/lichuan_c/Pdo.h", "adapters/canopennode/PdoState.h",
    "adapters/canopennode/Pdo.cpp", "test/pdo_codec_test.cpp", "test/pdo_observations_test.cpp",
    "adapters/canopennode/CMakeLists.txt", "adapters/canopennode/SdoClient.h",
    "adapters/canopennode/SdoClient.cpp", "adapters/canopennode/CO_driver_target.h",
    "adapters/canopennode/Driver.cpp",
    "test/frame_test.cpp", "test/sdo_codec_test.cpp", "test/sdo_adapter_test.cpp",
    "scripts/verify.py", "scripts/check_repository.py", "scripts/generate_version.py",
    "scripts/export_package.py", "scripts/check_promptset.py",
    "scripts/check_lichuan_ledger.py", "scripts/doxygen_filter.py",
    "scripts/generate_lichuan_objects.py", "scripts/generate_lichuan_objects_test.py",
    "docs/reference/lichuan_objects.json", "docs/reference/lichuan_generated.md",
    "include/MotorControlCAN/profiles/lichuan_c/Types.h",
    "include/MotorControlCAN/profiles/lichuan_c/Objects.h",
    "include/MotorControlCAN/profiles/lichuan_c/Catalogue.h", "test/objects_test.cpp",
    "scripts/check_promptset_test.py", "scripts/check_lichuan_ledger_test.py",
    "test/CMakeLists.txt", "test/core_test.cpp", "test/api_ownership_test.cpp",
    "test/multidrive_contract_test.cpp", "test/version_test.py", "test/package_test.py",
    "test/repository_test.py", "test/verification_test.py", "test/documentation_test.py",
    "test/installed_consumer/CMakeLists.txt",
    "test/installed_consumer/main.cpp", "experiments/canopen_service_fit/CMakeLists.txt",
}
EXPORTS = ["include/", "src/", "cmake/", "CMakeLists.txt", "library.json", "LICENSE",
           "README.md", "CHANGELOG.md", "docs/getting_started.md", "docs/units_status.md", "docs/axis_contract.md", "docs/canopen_services.md", "docs/reads.md", "docs/driver_settings.md", "docs/io_settings.md"]


class RepositoryTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="MotorControlCAN checker ")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.build = self.root / "isolated build"
        for relative in FILES:
            self.write(relative, "fixture\n")
        self.metadata = json.loads((ROOT / "library.json").read_text(encoding="utf-8"))
        self.metadata["export"]["include"] = EXPORTS.copy()
        self.write_metadata()
        self.write("include/MotorControlCAN/Version.h",
                   (ROOT / "include/MotorControlCAN/Version.h").read_text(encoding="utf-8"))
        self.registrations = []
        self.tests = []
        suffix = ".exe" if sys.platform == "win32" else ""
        for name in ("core", "units", "axis", "frame", "sdo_codec", "sdo_adapter", "sdo_transfer", "sdo_reconciliation", "network_codec", "network_events", "nmt_codec", "nmt_services", "pdo_codec", "pdo_observations", "reads", "driver_settings", "io_settings", "settings_write", "io_settings_preview", "driver_settings_preview", "objects", "api_ownership", "multidrive_contract"):
            self.registrations.append(f"add_executable({name}_test {name}_test.cpp)\n"
                                      f"add_test(NAME {name} COMMAND {name}_test)\n")
            self.tests.append({"name": name, "command": [str(self.build / "test" / (name + "_test" + suffix))]})
        self.tests.append({"name": "canopen_service_fit", "command": [
            str(self.build / "test/fit_stack" / ("canopen_service_fit" + suffix))]})
        for name, relative, args in (
                ("version_generated", "scripts/generate_version.py", ["check"]),
                ("objects_generated", "scripts/generate_lichuan_objects.py", ["--check"]),
                ("objects_generator_contract", "scripts/generate_lichuan_objects_test.py", []),
                ("repository_checks", "scripts/check_repository.py", []),
                ("planning_integrity", "scripts/check_promptset.py", []),
                ("version_contract", "test/version_test.py", []),
                ("repository_contract", "test/repository_test.py", []),
                ("verification_contract", "test/verification_test.py", []),
                ("documentation_contract", "test/documentation_test.py", []),
                ("planning_validator_contract", "scripts/check_promptset_test.py", []),
                ("ledger_validator_contract", "scripts/check_lichuan_ledger_test.py", []),
                ("package_contract", "test/package_test.py", ["--cmake", "/tools/cmake", "--compiler", "/tools/c++"])):
            self.registrations.append('add_test(NAME ' + name + ' COMMAND "${Python3_EXECUTABLE}" '
                                      '"${PROJECT_SOURCE_DIR}/' + relative + '" ' + ' '.join(args) + ')\n')
            self.tests.append({"name": name, "command": [sys.executable, str(self.root / relative), *args]})
            if name in ("package_contract", "documentation_contract"):
                labels = ["full", "docs"] if name == "documentation_contract" else ["full"]
                self.tests[-1]["properties"] = [{"name": "LABELS", "value": labels}]
        self.write("test/CMakeLists.txt", "".join(self.registrations))

    def write(self, relative, content):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
        return path

    def write_metadata(self):
        self.write("library.json", json.dumps(self.metadata))

    def check(self, inventory=False):
        return checker.check(self.root, {"tests": self.tests} if inventory else None, self.build)

    def assert_failure(self, expected, inventory=False):
        findings = self.check(inventory)
        self.assertTrue(any(expected in finding for finding in findings), findings)

    def test_current_fixture_passes_source_and_actual_command_inventory(self):
        self.assertEqual(self.check(), [])
        self.assertEqual(self.check(True), [])

    def test_each_required_file_is_enforced_independently(self):
        for relative in sorted(FILES):
            with self.subTest(path=relative):
                path = self.root / relative
                original = path.read_bytes()
                path.unlink()
                self.assert_failure("missing required repository/package file: " + relative)
                path.write_bytes(original)

    def test_stale_version_fails_without_overwriting(self):
        self.metadata["version"] = "0.1.1"
        self.write_metadata()
        path = self.root / "include/MotorControlCAN/Version.h"
        original = path.read_bytes()
        self.assert_failure("Version.h is missing or stale")
        self.assertEqual(path.read_bytes(), original)

    def test_invalid_manifest_values_report_useful_errors(self):
        original = copy.deepcopy(self.metadata)
        cases = (("name", "Other", "name must"), ("version", "1.02.3", "version must"),
                 ("build", [], "invalid build"), ("repository", None, "invalid repository"),
                 ("authors", [None], "maintainer"), ("headers", [], "umbrella header"),
                 ("frameworks", ["arduino"], "frameworks must"),
                 ("platforms", ["espressif32"], "platforms must"),
                 ("export", [], "reviewed core allowlist"),
                 ("export", {"include": [None]}, "reviewed core allowlist"))
        for field, value, expected in cases:
            with self.subTest(field=field, value=value):
                self.metadata = copy.deepcopy(original)
                self.metadata[field] = value
                self.write_metadata()
                self.assert_failure(expected)
        for value in ("[]", "null", "{invalid"):
            self.write("library.json", value)
            self.assert_failure("library.json:")

    def test_export_cannot_omit_or_add_or_duplicate_paths(self):
        for entries in (EXPORTS[1:], EXPORTS + ["docs/reference/"],
                        EXPORTS + ["LICENSE"], ["../outside"]):
            with self.subTest(entries=entries):
                self.metadata["export"]["include"] = entries
                self.write_metadata()
                self.assert_failure("reviewed core allowlist")

    def test_unregistered_native_and_python_files_fail(self):
        self.write("test/new_test.cpp", "int main() {}\n")
        self.assert_failure("new_test.cpp")
        for relative in ("test/new_test.py", "scripts/new_test.py"):
            with self.subTest(relative=relative):
                self.write(relative, "print('test')\n")
                self.assert_failure("unregistered Python test " + relative)

    def test_commented_registration_does_not_count(self):
        self.write("test/CMakeLists.txt", "".join(
            "# " + entry.replace("\n", "\n# ") for entry in self.registrations))
        self.assert_failure("missing or wrong native registration core")
        self.assert_failure("unregistered Python test test/version_test.py")

    def test_actual_inventory_missing_or_misdirected_tests_fail(self):
        original = copy.deepcopy(self.tests)
        for name in ("core", "version_contract", "canopen_service_fit", "planning_integrity"):
            with self.subTest(name=name):
                self.tests = [test for test in original if test["name"] != name]
                self.assertTrue(self.check(True))
        self.tests = copy.deepcopy(original)
        self.tests[0]["command"] = [str(self.build / "core_test")]
        self.assert_failure("wrong native command for core", True)
        self.tests = copy.deepcopy(original)
        generated = next(test for test in self.tests if test["name"] == "version_generated")
        generated["command"][-1] = "sync"
        self.assert_failure("wrong Python command for scripts/generate_version.py", True)

    def test_python_command_must_use_current_interpreter_without_extra_args(self):
        original = copy.deepcopy(self.tests)
        target = next(test for test in self.tests if test["name"] == "version_contract")
        target["command"][0] = str(self.root / "fake-python")
        self.assert_failure("wrong Python command for test/version_test.py", True)
        self.tests = original
        target = next(test for test in self.tests if test["name"] == "version_contract")
        target["command"].append("--skip")
        self.assert_failure("wrong Python command for test/version_test.py", True)

    def test_duplicate_inventory_names_and_script_registrations_fail(self):
        self.tests.append(copy.deepcopy(self.tests[0]))
        self.assert_failure("duplicate test names", True)
        duplicate = copy.deepcopy(next(test for test in self.tests if test["name"] == "version_contract"))
        duplicate["name"] = "version_again"
        self.tests.append(duplicate)
        self.assert_failure("exactly one registration for test/version_test.py", True)

    def test_failure_and_skip_bypasses_are_rejected(self):
        for property_name, value in (("DISABLED", True), ("WILL_FAIL", True),
                                     ("PASS_REGULAR_EXPRESSION", ["PASS"]),
                                     ("SKIP_REGULAR_EXPRESSION", [".*"]), ("SKIP_RETURN_CODE", 77)):
            with self.subTest(property_name=property_name):
                self.tests[0]["properties"] = [{"name": property_name, "value": value}]
                self.assert_failure("bypasses failures with " + property_name, True)
        for property_name in ("ENVIRONMENT", "ENVIRONMENT_MODIFICATION"):
            self.tests[0]["properties"] = [{"name": property_name, "value": ["PYTHONOPTIMIZE=1"]}]
            self.assert_failure("overrides PYTHONOPTIMIZE", True)

    def test_full_label_cannot_hide_quick_contracts(self):
        for name in ("core", "canopen_service_fit", "repository_contract", "verification_contract",
                     "planning_validator_contract", "ledger_validator_contract"):
            with self.subTest(name=name):
                target = next(test for test in self.tests if test["name"] == name)
                target["properties"] = [{"name": "LABELS", "value": ["full"]}]
                self.assert_failure("wrong labels on " + name, True)
                target.pop("properties")

    def test_full_contracts_require_full_label_and_allow_docs_label(self):
        for name in ("package_contract", "documentation_contract"):
            with self.subTest(name=name):
                target = next(test for test in self.tests if test["name"] == name)
                original = target.pop("properties")
                self.assert_failure("wrong labels on " + name, True)
                target["properties"] = original
        self.assertEqual(self.check(True), [])

    def test_malformed_inventory_has_diagnostics(self):
        for listing in ([], {}, {"tests": []}, {"tests": [None]}, {"tests": [{"name": "missing-command"}]}):
            with self.subTest(listing=listing):
                self.assertTrue(checker.check(self.root, listing, self.build))

    def test_cli_returns_nonzero_and_reports_source_failure(self):
        (self.root / "cmake/MotorControlCANConfig.cmake.in").unlink()
        output = io.StringIO()
        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(output):
            status = checker.main(["--root", str(self.root)])
        self.assertEqual(status, 1)
        self.assertIn("MotorControlCANConfig.cmake.in", output.getvalue())
        self.assertIn("Repository checks: FAIL", output.getvalue())


if __name__ == "__main__":
    unittest.main()
