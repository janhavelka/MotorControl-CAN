#!/usr/bin/env python3
"""Reject incomplete firmware build evidence without invoking an SDK or hardware."""

import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import check_firmware as checker


# Independent minimum: fixtures must not shrink when the production list changes.
SHARED_SOURCES = [
    "examples/common/ProbeApp.cpp",
    "examples/common/ProbePlatform.cpp",
    "examples/common/CanBusOwner.cpp",
    "examples/common/FirmwareProbe.cpp",
    "examples/common/EspProbePlatform.cpp",
    "examples/common/EspProbeMain.cpp",
    "adapters/twai/TwaiAdapter.cpp",
    "adapters/twai/IdfTwaiDriver.cpp",
]


class FirmwareBuildTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="can firmware evidence ")
        self.addCleanup(temporary.cleanup)
        self.base = Path(temporary.name)
        self.root = self.base / "source tree"
        self.inventory_path = self.root / "examples/common/firmware_sources.json"
        for name in SHARED_SOURCES:
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("// synthetic compile input\n", encoding="utf-8")
        self.inventory = {"sources": list(SHARED_SOURCES),
                          "include_dirs": ["examples/common", "adapters/twai"]}
        self.write_inventory()
        self.database = self.base / "build output/compile_commands.json"
        self.database.parent.mkdir()

    def write_inventory(self, value=None):
        self.inventory_path.write_text(json.dumps(self.inventory if value is None else value),
                                       encoding="utf-8")

    def write_database(self, entries):
        self.database.write_text(json.dumps(entries), encoding="utf-8")

    def entries(self):
        return [{"directory": str(self.root), "file": path,
                 "command": "compiler -c synthetic-input"} for path in SHARED_SOURCES]

    def expected(self):
        return [self.root / path for path in SHARED_SOURCES]

    def test_minimum_shared_inventory_accepts_paths_with_spaces(self):
        self.assertEqual(checker.firmware_inventory(self.root), self.inventory)

    def test_each_missing_shared_source_is_rejected_even_when_its_file_exists(self):
        for source in SHARED_SOURCES:
            with self.subTest(source=source):
                self.inventory["sources"] = [path for path in SHARED_SOURCES if path != source]
                self.write_inventory()
                with self.assertRaisesRegex(ValueError, "omits shared"):
                    checker.firmware_inventory(self.root)

    def test_deleted_source_is_rejected(self):
        (self.root / SHARED_SOURCES[0]).unlink()
        with self.assertRaisesRegex(ValueError, "Missing/outside"):
            checker.firmware_inventory(self.root)

    def test_parent_traversal_and_absolute_outside_paths_are_rejected(self):
        outside = self.base / "outside.cpp"
        outside.write_text("// must never enter the build\n", encoding="utf-8")
        for path in ("../outside.cpp", str(outside)):
            with self.subTest(path=path):
                self.inventory["sources"] = SHARED_SOURCES + [path]
                self.write_inventory()
                with self.assertRaisesRegex(ValueError, "Missing/outside"):
                    checker.firmware_inventory(self.root)

    def test_root_as_include_directory_is_rejected(self):
        self.inventory["include_dirs"].append(".")
        self.write_inventory()
        with self.assertRaisesRegex(ValueError, "Missing/outside"):
            checker.firmware_inventory(self.root)

    def test_duplicates_are_rejected_for_both_inventory_groups(self):
        for group in ("sources", "include_dirs"):
            with self.subTest(group=group):
                self.inventory[group].append(self.inventory[group][0])
                self.write_inventory()
                with self.assertRaisesRegex(ValueError, "Duplicate"):
                    checker.firmware_inventory(self.root)
                self.inventory[group].pop()

    def test_directory_cannot_be_a_source_and_file_cannot_be_an_include_directory(self):
        for group, path in (("sources", "examples/common"),
                            ("include_dirs", SHARED_SOURCES[0])):
            with self.subTest(group=group):
                self.inventory[group].append(path)
                self.write_inventory()
                with self.assertRaisesRegex(ValueError, "input kind"):
                    checker.firmware_inventory(self.root)
                self.inventory[group].pop()

    def test_malformed_inventory_is_a_validation_error(self):
        malformed = [None, [], {}, {"sources": SHARED_SOURCES},
                     {"sources": SHARED_SOURCES, "include_dirs": []},
                     {"sources": SHARED_SOURCES + [None], "include_dirs": ["examples/common"]},
                     {"sources": SHARED_SOURCES, "include_dirs": "examples/common"}]
        for value in malformed:
            with self.subTest(value=value):
                self.inventory_path.write_text(json.dumps(value), encoding="utf-8")
                with self.assertRaises(ValueError):
                    checker.firmware_inventory(self.root)

    def test_relative_source_paths_use_database_directory_instead_of_current_directory(self):
        self.write_database(self.entries())
        self.assertEqual(checker.check_inputs(self.database, self.expected()), len(SHARED_SOURCES))

    def test_absolute_source_paths_do_not_require_a_directory_field(self):
        self.write_database([{"file": str(path)} for path in self.expected()])
        self.assertEqual(checker.check_inputs(self.database, self.expected()), len(SHARED_SOURCES))

    def test_relative_database_filename_is_supported(self):
        self.write_database(self.entries())
        previous_directory = Path.cwd()
        try:
            # The temporary directory and checkout can be on different Windows
            # drives. A relative database belongs to its caller's working dir.
            os.chdir(self.database.parent)
            self.assertEqual(checker.check_inputs(Path(self.database.name), self.expected()),
                             len(SHARED_SOURCES))
        finally:
            os.chdir(previous_directory)

    def test_success_log_and_linked_artifacts_cannot_hide_an_omitted_source(self):
        (self.database.parent / "build.log").write_text("Build complete: SUCCESS\n", encoding="utf-8")
        (self.database.parent / "firmware.elf").write_bytes(b"synthetic ELF fixture")
        (self.database.parent / "firmware.bin").write_bytes(b"synthetic BIN fixture")
        entries = self.entries()
        entries.pop(3)  # FirmwareProbe is still required after an apparently successful build.
        self.write_database(entries)
        with self.assertRaisesRegex(ValueError, "Actual build omitted required inputs:.*FirmwareProbe"):
            checker.check_inputs(self.database, self.expected())

    def test_unrelated_same_basename_is_not_the_expected_shared_source(self):
        entries = self.entries()
        entries[0]["directory"] = str(self.base / "unrelated")
        self.write_database(entries)
        with self.assertRaisesRegex(ValueError, "omitted required inputs:.*ProbeApp"):
            checker.check_inputs(self.database, self.expected())

    def test_empty_and_nonlist_compilation_databases_are_rejected(self):
        for data in (None, [], {}, "Build complete: SUCCESS"):
            with self.subTest(data=data):
                self.write_database(data)
                with self.assertRaises(ValueError):
                    checker.check_inputs(self.database, self.expected())

    def test_malformed_compilation_rows_are_validation_errors(self):
        malformed = [None, "not a row", [], {}, {"file": None}, {"file": 7}, {"file": ""},
                     {"file": "ProbeApp.cpp"}, {"file": "ProbeApp.cpp", "directory": None},
                     {"file": "ProbeApp.cpp", "directory": 7},
                     {"file": "ProbeApp.cpp", "directory": ""},
                     {"file": "ProbeApp.cpp", "directory": "relative/build"}]
        for entry in malformed:
            with self.subTest(entry=entry):
                self.write_database([entry])
                with self.assertRaises(ValueError):
                    checker.check_inputs(self.database, self.expected())

    def test_truncated_json_is_rejected_for_inventory_and_database(self):
        for path in (self.inventory_path, self.database):
            with self.subTest(path=path):
                path.write_text('[{"file":', encoding="utf-8")
                with self.assertRaises(ValueError):
                    if path == self.inventory_path:
                        checker.firmware_inventory(self.root)
                    else:
                        checker.check_inputs(self.database, self.expected())

    def sdk_fixture(self):
        sdk = self.base / "synthetic SDK"
        manifests = self.root / "docs/adapters"
        manifests.mkdir(parents=True)
        for name, source in (("twai_sources.json", "transport/header.h"),
                             ("esp_platform_sources.json", "platform/header.h")):
            path = sdk / source
            path.parent.mkdir(parents=True, exist_ok=True)
            content = ("synthetic pinned " + source).encode("utf-8")
            path.write_bytes(content)
            manifest = {"sdk": {"tag": "v5.5.5", "commit": "synthetic fixture"},
                        "sources": [{"path": source,
                                     "sha256": hashlib.sha256(content).hexdigest()}]}
            (manifests / name).write_text(json.dumps(manifest), encoding="utf-8")
        interpreter = self.base / "selected venv/bin/python"
        args = SimpleNamespace(idf_path=sdk, idf_python=interpreter)
        runner = mock.Mock()
        runner.summary = {}
        runner.run.side_effect = lambda name, *args, **kwargs: (
            "ESP-IDF v5.5.5" if name == "idf_version" else "Python fixture")
        return args, runner

    def test_sdk_preserves_selected_virtualenv_interpreter_and_environment(self):
        args, runner = self.sdk_fixture()
        resolved_sdk = args.idf_path.resolve()
        sdk_alias = self.base / "SDK path alias"
        args.idf_path = sdk_alias
        lexical_interpreter = args.idf_python
        base_interpreter = self.base / "system Python/bin/python"
        original_resolve = Path.resolve

        def resolve(path, *args, **kwargs):
            # Model a Linux venv Python symlink without platform-dependent
            # symlink permissions. Resolving it would silently select the base.
            if path == lexical_interpreter:
                return base_interpreter
            # SDK roots are intentionally canonicalized, unlike venv Python.
            # Model Windows short paths or directory aliases on every OS.
            if path == sdk_alias:
                return resolved_sdk
            return original_resolve(path, *args, **kwargs)

        with mock.patch.object(checker, "ROOT", self.root), \
             mock.patch.dict(os.environ, {}, clear=True), \
             mock.patch.object(Path, "resolve", autospec=True, side_effect=resolve):
            command, env = checker.sdk_environment(args, runner)
        self.assertEqual(command, [lexical_interpreter, resolved_sdk / "tools/idf.py"])
        self.assertEqual(env["IDF_PYTHON_ENV_PATH"], str(lexical_interpreter.parent.parent))
        self.assertEqual(env["IDF_PATH"], str(resolved_sdk))
        self.assertEqual(runner.summary["sdk_python"], str(lexical_interpreter))
        version_call = runner.run.call_args_list[0]
        self.assertEqual(version_call.args[1][0], lexical_interpreter)
        self.assertEqual(version_call.kwargs["env"]["IDF_PYTHON_ENV_PATH"],
                         str(lexical_interpreter.parent.parent))
        self.assertEqual(runner.run.call_args_list[1].args[1][0], lexical_interpreter)
        runner.tool.assert_called_once_with("xtensa", "xtensa-esp32s3-elf-g++")

    def test_sdk_rejects_disabled_dependency_checks_before_any_tool_invocation(self):
        args, runner = self.sdk_fixture()
        for disabled in ("0", "n", "N", "no", "NO", "No", "nO"):
            with self.subTest(disabled=disabled), \
                 mock.patch.object(checker, "ROOT", self.root), \
                 mock.patch.dict(os.environ, {"IDF_PYTHON_CHECK_CONSTRAINTS": disabled}, clear=True):
                with self.assertRaisesRegex(checker.VerificationFailure,
                                            "dependency checks must remain enabled"):
                    checker.sdk_environment(args, runner)
                runner.run.assert_not_called()
                runner.tool.assert_not_called()

    def test_idf_products_and_diagnostics_survive_disposable_build_without_sdk_cache(self):
        runner = checker.Runner(self.base / "retained evidence", "firmware-idf")
        products = {
            "MotorControlCANProbe.elf": b"synthetic linked ELF\x00\xff",
            "MotorControlCANProbe.bin": b"synthetic firmware BIN\x00",
            "MotorControlCANProbe.map": b"synthetic link map\n",
            "compile_commands.json": b'[{"file":"/temporary/source.cpp","command":"compiler -c source.cpp"}]',
            "project_description.json": b'{"project_name":"MotorControlCANProbe"}',
            "sdkconfig": b"CONFIG_IDF_TARGET_ESP32S3=y\n",
            "bootloader/bootloader.bin": b"synthetic bootloader BIN",
            "partition_table/partition-table.bin": b"synthetic partition BIN",
            "log/idf_py_stdout_output_1": b"SDK build complete\n",
            "log/idf_py_stderr_output_1": b"retained SDK diagnostic\n",
        }
        with tempfile.TemporaryDirectory(prefix="mccan-idf-") as temporary:
            source = Path(temporary)
            disposable = {
                "CMakeCache.txt": b"machine-specific SDK paths",
                "CMakeFiles/main.dir/main.cpp.obj": b"disposable compiler object",
                "bootloader/CMakeFiles/bootloader.dir/startup.c.obj": b"bootloader object",
            }
            for name, content in {**products, **disposable}.items():
                path = source / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(content)
            runner.summary["firmware_artifacts"] = {
                "idf": str(source / "MotorControlCANProbe.elf")}
            checker.retain_idf(runner, "idf", source)
        self.assertFalse(source.exists())
        output = runner.directory / "idf"
        for name, content in products.items():
            with self.subTest(product=name):
                self.assertEqual((output / name).read_bytes(), content)
        copied = {path.relative_to(output).as_posix() for path in output.rglob("*") if path.is_file()}
        self.assertEqual(copied, set(products))
        summary = json.loads((runner.directory / "summary.json").read_text(encoding="utf-8"))
        retained_elf = output / "MotorControlCANProbe.elf"
        self.assertEqual(summary["firmware_artifacts"]["idf"], str(retained_elf))
        self.assertTrue(Path(summary["firmware_artifacts"]["idf"]).is_file())


if __name__ == "__main__":
    unittest.main()
