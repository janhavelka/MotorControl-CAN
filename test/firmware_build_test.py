#!/usr/bin/env python3
"""Reject incomplete firmware build evidence without invoking an SDK or hardware."""

import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import check_firmware as checker
import check_twai_sdk as twai_checker


# Independent minimum: fixtures must not shrink when the production list changes.
SHARED_SOURCES = [
    "examples/common/ProbeApp.cpp", "examples/common/ProbeAppLocal.cpp", "examples/common/ProbeConsole.cpp",
    "examples/common/ProbePlatform.cpp",
    "examples/common/DeviceEvidence.cpp", "examples/common/DiscoveryScan.cpp",
    "adapters/owner/CanBusOwner.cpp",
    "examples/common/FirmwareProbe.cpp",
    "examples/common/EspProbePlatform.cpp",
    "examples/common/EspProbeMain.cpp",
    "examples/common/EspTransceiverPin.cpp",
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
        entries = [entry for entry in self.entries()
                   if entry["file"] != "examples/common/FirmwareProbe.cpp"]
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

    def test_s2_sdk_requires_the_s2_compiler_with_the_same_hash_and_version_gates(self):
        args, runner = self.sdk_fixture()
        with mock.patch.object(checker, "ROOT", self.root):
            checker.sdk_environment(args, runner, target="esp32s2")
        runner.tool.assert_called_once_with("xtensa", "xtensa-esp32s2-elf-g++")
        self.assertEqual(runner.summary["source_hashes_verified"], 2)

    def run_s2_cli(self, *, omit_main=False, omit_raw=False, linked_target="esp32s2",
                   compiler="xtensa-esp32s2-elf-g++", sdkconfig=None, extra=()):
        project = self.root / "examples/s2_bench"
        entry = project / "main/main.cpp"
        entry.parent.mkdir(parents=True)
        entry.write_text("// synthetic S2 entry\n", encoding="utf-8")
        raw = project / "RawEvidence.cpp"
        raw.write_text("// synthetic raw collector\n", encoding="utf-8")
        core = self.root / "src/core.cpp"
        core.parent.mkdir()
        core.write_text("// synthetic core\n", encoding="utf-8")
        runner = mock.Mock()
        runner.summary = {}
        runner.directory = self.base / "retained S2 output"
        runner.run.return_value = "synthetic revision"

        def build_inputs(*args, **kwargs):
            build = args[5]
            build.mkdir(parents=True)
            expected = (self.expected() + [core] + ([] if omit_main else [entry]) +
                        ([] if omit_raw else [raw]) + [self.root / item for item in extra])
            (build / "compile_commands.json").write_text(json.dumps(
                [{"file": str(path), "command": compiler + " -c " + str(path)} for path in expected]),
                encoding="utf-8")
            (build / "project_description.json").write_text(
                json.dumps({"target": linked_target}), encoding="utf-8")
            (build / "sdkconfig").write_text(
                self.sdkconfig_text(checker.BENCH_SDKCONFIG) if sdkconfig is None else sdkconfig,
                encoding="utf-8")

        argv = ["check_firmware.py", "--target", "s2", "--temporary-root", str(self.base)]
        with mock.patch.object(checker, "ROOT", self.root), \
             mock.patch.object(checker, "Runner", return_value=runner), \
             mock.patch.object(checker, "firmware_inventory", return_value=self.inventory), \
             mock.patch.object(checker, "sdk_environment", return_value=(["python", "idf.py"], {})) as sdk, \
             mock.patch.object(checker, "idf_build", side_effect=build_inputs) as build, \
             mock.patch.object(sys, "argv", argv), \
             mock.patch("sys.stdout", new_callable=io.StringIO), \
             mock.patch("sys.stderr", new_callable=io.StringIO):
            result = checker.main()
        self.assertEqual(sdk.call_args.kwargs["target"], "esp32s2")
        self.assertEqual(build.call_args.kwargs["target"], "esp32s2")
        self.assertEqual(build.call_args.args[3:5], ("s2", project))
        self.assertEqual(build.call_args.args[5].parent.parent, self.base)
        return result, runner

    def test_s2_cli_checks_shared_core_and_bench_entry_inputs(self):
        # The bench project names its own further sources; they are not a checkout extra.
        bench_source = self.root / "examples/s2_bench/PdoWatch.cpp"
        bench_source.parent.mkdir(parents=True)
        bench_source.write_text("// synthetic bench source\n", encoding="utf-8")
        result, runner = self.run_s2_cli(extra=("examples/s2_bench/PdoWatch.cpp",))
        self.assertEqual(result, 0, runner.summary.get("error"))
        self.assertEqual(runner.summary["status"], "passed")
        self.assertEqual(runner.summary["s2_target"], "esp32s2")
        self.assertEqual(runner.summary["firmware_targets"], {"s2": "esp32s2"})
        self.assertEqual(runner.summary["s2_compile_entries"], len(SHARED_SOURCES) + 4)

    def test_s2_cli_rejects_a_build_missing_the_bench_entry(self):
        result, runner = self.run_s2_cli(omit_main=True)
        self.assertEqual(result, 1)
        self.assertIn("main.cpp", runner.summary["error"])

    def test_s2_cli_rejects_a_build_missing_the_raw_collector(self):
        result, runner = self.run_s2_cli(omit_raw=True)
        self.assertEqual(result, 1)
        self.assertIn("RawEvidence.cpp", runner.summary["error"])

    def test_s2_cli_rejects_a_different_linked_target(self):
        result, runner = self.run_s2_cli(linked_target="esp32s3")
        self.assertEqual(result, 1)
        self.assertIn("not esp32s2", runner.summary["error"])

    def test_s2_cli_rejects_another_compiler_configuration_or_checkout_source(self):
        # Review B-D6: the bench gate checks its compiler and resolved configuration too.
        smoke = self.root / "examples/common/FirmwareSmoke.cpp"
        smoke.write_text("// test-only source outside the bench project\n", encoding="utf-8")
        good = self.sdkconfig_text(checker.BENCH_SDKCONFIG)
        cases = (({"compiler": "xtensa-esp32s3-elf-g++"}, "not xtensa-esp32s2"),
                 ({"compiler": "/usr/bin/g++"}, "not xtensa-esp32s2"),
                 ({"sdkconfig": good.replace("CONFIG_ESP_CONSOLE_NONE=y", "CONFIG_ESP_CONSOLE_UART=y")},
                  "CONFIG_ESP_CONSOLE_NONE"),
                 ({"sdkconfig": good.replace('CONFIG_ESPTOOLPY_FLASHSIZE="4MB"', 'CONFIG_ESPTOOLPY_FLASHSIZE="2MB"')},
                  "CONFIG_ESPTOOLPY_FLASHSIZE"),
                 ({"extra": ("examples/common/FirmwareSmoke.cpp",)}, "unexpected checkout inputs:.*FirmwareSmoke"))
        for options, message in cases:
            with self.subTest(message=message):
                for path in (self.root / "examples/s2_bench", self.root / "src"):
                    shutil.rmtree(path, ignore_errors=True)
                result, runner = self.run_s2_cli(**options)
                self.assertEqual(result, 1)
                self.assertRegex(runner.summary["error"].replace("\\", "/"), message)
                self.assertNotEqual(runner.summary["status"], "passed")

    def run_twai_cli(self, args, runner, environment=None):
        runner.directory = self.base / "TWAI build output"

        def tool_output(name, *args, **kwargs):
            if name == "idf_version":
                return "ESP-IDF v5.5.5"
            if name == "layout":
                return ("00000 256 B motorcontrolcan_twai_driver_bytes\n"
                        "00000 128 B motorcontrolcan_twai_adapter_bytes\n")
            return "synthetic tool success"

        runner.run.side_effect = tool_output
        argv = ["check_twai_sdk.py", "--idf-path", str(args.idf_path),
                "--idf-python", str(args.idf_python), "--build-dir", str(runner.directory)]
        with mock.patch.object(checker, "ROOT", self.root), \
             mock.patch.object(twai_checker, "ROOT", self.root), \
             mock.patch.object(twai_checker, "Runner", return_value=runner), \
             mock.patch.object(sys, "argv", argv), \
             mock.patch.dict(os.environ, environment or {}, clear=True), \
             mock.patch("sys.stdout", new_callable=io.StringIO), \
             mock.patch("sys.stderr", new_callable=io.StringIO):
            return twai_checker.main()

    def test_twai_cli_preserves_venv_interpreter_and_checks_only_its_sdk_sources(self):
        args, runner = self.sdk_fixture()
        resolved_sdk = args.idf_path.resolve()
        sdk_alias = self.base / "TWAI SDK path alias"
        args.idf_path = sdk_alias
        lexical_interpreter = args.idf_python
        base_interpreter = self.base / "system Python/bin/python"
        original_resolve = Path.resolve
        # The transport-only SDK gate does not consume the USB/time platform.
        (self.root / "docs/adapters/esp_platform_sources.json").unlink()

        def resolve(path, *args, **kwargs):
            if path == lexical_interpreter:
                return base_interpreter
            if path == sdk_alias:
                return resolved_sdk
            return original_resolve(path, *args, **kwargs)

        with mock.patch.object(Path, "resolve", autospec=True, side_effect=resolve):
            self.assertEqual(self.run_twai_cli(args, runner), 0)
        self.assertEqual(runner.summary["status"], "passed")
        invocations = {call.args[0]: call for call in runner.run.call_args_list}
        for name in ("idf_version", "configure"):
            with self.subTest(invocation=name):
                call = invocations[name]
                self.assertEqual(call.args[1][:2],
                                 [lexical_interpreter, resolved_sdk / "tools/idf.py"])
                self.assertEqual(call.kwargs["env"]["IDF_PATH"], str(resolved_sdk))
                self.assertEqual(call.kwargs["env"]["IDF_PYTHON_ENV_PATH"],
                                 str(lexical_interpreter.parent.parent))
        self.assertEqual(runner.summary["sdk_python"], str(lexical_interpreter))
        self.assertEqual(runner.summary["source_hashes_verified"], 1)
        self.assertEqual(runner.summary["sdk"],
                         {"tag": "v5.5.5", "commit": "synthetic fixture"})
        self.assertEqual(runner.summary["object_bytes"], {"driver": 256, "adapter": 128})
        self.assertEqual(invocations["sdk_python_version"].args[1][0], lexical_interpreter)
        self.assertEqual(runner.tool.call_args_list.count(
            mock.call("xtensa", "xtensa-esp32s3-elf-g++")), 1)

    def test_twai_cli_rejects_all_disabled_constraint_spellings_before_tools(self):
        args, runner = self.sdk_fixture()
        for disabled in ("0", "n", "N", "no", "NO", "No", "nO"):
            with self.subTest(disabled=disabled):
                runner.reset_mock()
                runner.summary = {}
                self.assertEqual(self.run_twai_cli(
                    args, runner, {"IDF_PYTHON_CHECK_CONSTRAINTS": disabled}), 1)
                self.assertEqual(runner.summary["status"], "failed")
                self.assertIn("dependency", runner.summary["error"])
                runner.run.assert_not_called()
                runner.tool.assert_not_called()

    def test_twai_cli_rejects_changed_sdk_source_before_tools(self):
        args, runner = self.sdk_fixture()
        (args.idf_path / "transport/header.h").write_text("changed SDK bytes", encoding="utf-8")
        self.assertEqual(self.run_twai_cli(args, runner), 1)
        self.assertEqual(runner.summary["status"], "failed")
        self.assertIn("transport/header.h", runner.summary["error"].replace("\\", "/"))
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

    def test_idf_build_uses_requested_s2_target_and_retains_its_images(self):
        runner = checker.Runner(self.base / "retained S2", "firmware-s2")
        with tempfile.TemporaryDirectory(prefix="mccan-s2-") as temporary:
            build = Path(temporary) / "build"

            def linked_build(*args, **kwargs):
                build.mkdir()
                (build / "S2Bench.elf").write_bytes(b"synthetic ELF")
                (build / "S2Bench.bin").write_bytes(b"synthetic BIN")

            with mock.patch.object(runner, "run", side_effect=linked_build) as run:
                checker.idf_build(runner, ["python", "idf.py"], {}, "s2", self.root,
                                  build, target="esp32s2")
            command = run.call_args.args[1]
            self.assertIn("-DIDF_TARGET=esp32s2", command)
            self.assertNotIn("-DIDF_TARGET=esp32s3", command)
            self.assertEqual(command[-1], "build")
            self.assertNotIn("flash", command)
        self.assertTrue(Path(runner.summary["firmware_artifacts"]["s2"]).is_file())
        self.assertEqual((runner.directory / "s2/S2Bench.bin").read_bytes(), b"synthetic BIN")

    def test_idf_failed_build_retains_partial_sdk_diagnostics_after_temporary_cleanup(self):
        runner = checker.Runner(self.base / "failed build evidence", "firmware-idf")
        failure = checker.VerificationFailure("SDK compilation failed at original source")
        products = {
            "sdkconfig": b"CONFIG_IDF_TARGET_ESP32S3=y\n",
            "log/idf_py_stderr_output_2": b"original SDK compiler error\n",
            "CMakeCache.txt": b"retained configuration after failure\n",
            "CMakeFiles/CMakeConfigureLog.yaml": b"configuration diagnosis\n",
            "CMakeFiles/CMakeError.log": b"configure compiler error\n",
        }
        with tempfile.TemporaryDirectory(prefix="mccan-idf-failure-") as temporary:
            build = Path(temporary) / "build"

            def fail_build(*args, **kwargs):
                for name, content in products.items():
                    path = build / name
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_bytes(content)
                raise failure

            with mock.patch.object(runner, "run", side_effect=fail_build):
                with self.assertRaises(checker.VerificationFailure) as caught:
                    checker.idf_build(runner, ["synthetic-python", "idf.py"], {},
                                      "idf", self.root, build)
            self.assertIs(caught.exception, failure)
        self.assertFalse(build.exists())
        output = runner.directory / "idf"
        for name, content in products.items():
            with self.subTest(product=name):
                self.assertEqual((output / name).read_bytes(), content)
        self.assertEqual(runner.summary["sdk_build_evidence"]["idf"], str(output))
        self.assertNotIn("idf", runner.summary.get("firmware_artifacts", {}))
        self.assertFalse(list(output.glob("*.elf")))

    def test_idf_missing_linked_image_retains_configuration_and_available_elf(self):
        runner = checker.Runner(self.base / "invalid image evidence", "firmware-component")
        with tempfile.TemporaryDirectory(prefix="mccan-idf-incomplete-") as temporary:
            build = Path(temporary) / "build"
            build.mkdir()
            (build / "MotorControlCANConsumer.elf").write_bytes(b"synthetic linked ELF")
            (build / "sdkconfig").write_text("CONFIG_IDF_TARGET_ESP32S3=y\n", encoding="utf-8")
            with mock.patch.object(runner, "run", return_value="SDK build reported success"):
                with self.assertRaisesRegex(checker.VerificationFailure, "ELF/BIN outputs"):
                    checker.idf_build(runner, ["synthetic-python", "idf.py"], {},
                                      "component", self.root, build)
        output = runner.directory / "component"
        self.assertEqual((output / "MotorControlCANConsumer.elf").read_bytes(), b"synthetic linked ELF")
        self.assertTrue((output / "sdkconfig").is_file())
        self.assertFalse((output / "MotorControlCANConsumer.bin").exists())
        self.assertEqual(runner.summary["sdk_build_evidence"]["component"], str(output))

    def test_idf_retention_failure_preserves_original_build_error(self):
        runner = checker.Runner(self.base / "retention failure evidence", "firmware-idf")
        failure = checker.VerificationFailure("original SDK build failure")
        with tempfile.TemporaryDirectory(prefix="mccan-idf-retention-") as temporary:
            build = Path(temporary) / "build"
            build.mkdir()
            (build / "sdkconfig").write_text("partial SDK configuration", encoding="utf-8")
            with mock.patch.object(runner, "run", side_effect=failure), \
                 mock.patch.object(checker.shutil, "copy2", side_effect=OSError("evidence disk full")):
                with self.assertRaises(checker.VerificationFailure) as caught:
                    checker.idf_build(runner, ["synthetic-python", "idf.py"], {},
                                      "idf", self.root, build)
            self.assertIs(caught.exception, failure)
        self.assertIn("evidence disk full", runner.summary["evidence_errors"]["idf"])

    def test_idf_retention_failure_after_success_is_a_failed_gate(self):
        runner = checker.Runner(self.base / "successful build lost evidence", "firmware-idf")
        failure = OSError("cannot retain firmware evidence")
        with tempfile.TemporaryDirectory(prefix="mccan-idf-success-") as temporary:
            build = Path(temporary) / "build"
            build.mkdir()
            (build / "Probe.elf").write_bytes(b"synthetic linked ELF")
            (build / "Probe.bin").write_bytes(b"synthetic firmware BIN")
            with mock.patch.object(runner, "run", return_value="SDK build success"), \
                 mock.patch.object(checker.shutil, "copy2", side_effect=failure):
                with self.assertRaises(OSError) as caught:
                    checker.idf_build(runner, ["synthetic-python", "idf.py"], {},
                                      "idf", self.root, build)
            self.assertIs(caught.exception, failure)


    # ---- Shared S2 firmware: selected MCU, configuration, inputs and the Arduino blocker.
    def compiler_rows(self, compiler, paths=None):
        return [{"directory": str(self.root), "file": str(path),
                 "command": '"' + compiler + '" -c ' + str(path)} for path in (paths or self.expected())]

    def test_forbidden_bench_and_smoke_inputs_are_rejected_even_with_every_required_input(self):
        bench = self.root / "examples/s2_bench/main/main.cpp"
        smoke = self.root / "examples/common/FirmwareSmoke.cpp"
        for extra, forbidden in ((bench, self.root / "examples/s2_bench"), (smoke, smoke)):
            with self.subTest(extra=extra.name):
                self.write_database([{"file": str(path)} for path in self.expected() + [extra]])
                with self.assertRaisesRegex(ValueError, "forbidden inputs:.*" + extra.name):
                    checker.check_inputs(self.database, self.expected(), [forbidden])
        self.write_database([{"file": str(path)} for path in self.expected()])
        self.assertEqual(checker.check_inputs(self.database, self.expected(),
                                              [self.root / "examples/s2_bench", smoke]), len(SHARED_SOURCES))

    def test_exact_checkout_inputs_refuse_an_extra_parser_or_renamed_bench_copy(self):
        # Review B-D2: required inputs alone let any additional checkout source pass.
        sdk = self.base / "synthetic SDK/components/driver/twai.c"
        generated = self.base / "build output/project_elf_src_esp32s2.c"
        for path in (sdk, generated):
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("// outside the checkout\n", encoding="utf-8")
        rows = self.expected() + [sdk, generated]
        self.write_database([{"file": str(path)} for path in rows])
        self.assertEqual(checker.check_inputs(self.database, self.expected(), owned=self.root), len(rows))
        for extra in ("examples/common/CommandParser.cpp", "examples/s2_bench_copy/main/main.cpp",
                      "src/renamed/Core.cpp"):
            with self.subTest(extra=extra):
                path = self.root / extra
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("// extra checkout source\n", encoding="utf-8")
                self.write_database([{"file": str(item)} for item in rows + [path]])
                with self.assertRaisesRegex(ValueError, "unexpected checkout inputs:.*" + path.name):
                    checker.check_inputs(self.database, self.expected(), owned=self.root)
                # Presence-only checking (no owned directory) is what let it through.
                checker.check_inputs(self.database, self.expected())
        # A build directory inside the checkout holds SDK-generated sources: skipped, not exact.
        inside = self.root / ".build/short/build/project_elf_src_esp32s2.c"
        inside.parent.mkdir(parents=True)
        inside.write_text("// generated\n", encoding="utf-8")
        self.write_database([{"file": str(path)} for path in self.expected() + [inside]])
        with self.assertRaisesRegex(ValueError, "unexpected checkout inputs"):
            checker.check_inputs(self.database, self.expected(), owned=self.root)
        checker.check_inputs(self.database, self.expected(), owned=self.root, outside=[inside.parent])

    def test_an_sdk_installed_inside_the_checkout_is_not_a_checkout_input(self):
        # CI installs ESP-IDF at .build/esp-idf; run 42249057257 counted its sources as unexpected
        # checkout inputs. The SDK roots come from the build environment, wherever they lie.
        sdk = self.root / ".build/esp-idf"
        tools = self.root / ".build/idf-tools"
        sources = [sdk / "components/driver/twai/twai.c", tools / "tools/xtensa/lib/crt.c"]
        for path in sources:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("// SDK\n", encoding="utf-8")
        self.write_database([{"file": str(path)} for path in self.expected() + sources])
        with self.assertRaisesRegex(ValueError, "unexpected checkout inputs:.*twai.c"):
            checker.check_inputs(self.database, self.expected(), owned=self.root)
        roots = checker.sdk_roots({"IDF_PATH": str(sdk), "IDF_TOOLS_PATH": str(tools), "PATH": "x"})
        self.assertEqual(roots, [sdk.resolve(), tools.resolve()])
        checker.check_inputs(self.database, self.expected(), owned=self.root, sdk=roots)
        # The component gate forbids the whole checkout; the SDK inside it is still not forbidden
        # (run 42252710418, 980982a), while a checkout source still is.
        self.write_database([{"file": str(path)} for path in sources])
        with self.assertRaisesRegex(ValueError, "forbidden inputs:.*twai.c"):
            checker.check_inputs(self.database, [], [self.root])
        checker.check_inputs(self.database, [], [self.root], sdk=roots)
        # A real extra checkout source still fails beside an SDK inside the checkout.
        extra = self.root / "examples/common/CommandParser.cpp"
        extra.parent.mkdir(parents=True, exist_ok=True)
        extra.write_text("// extra\n", encoding="utf-8")
        self.write_database([{"file": str(path)} for path in self.expected() + sources + [extra]])
        with self.assertRaisesRegex(ValueError, "unexpected checkout inputs:.*CommandParser.cpp"):
            checker.check_inputs(self.database, self.expected(), owned=self.root, sdk=roots)
        self.write_database([{"file": str(path)} for path in sources + [extra]])
        with self.assertRaisesRegex(ValueError, "forbidden inputs:.*CommandParser.cpp"):
            checker.check_inputs(self.database, [], [self.root], sdk=roots)

    def test_every_input_check_names_the_sdk_roots(self):
        text = (ROOT / "scripts/check_firmware.py").read_text(encoding="utf-8")
        calls = text.count("= check_inputs(")
        self.assertGreaterEqual(calls, 4)
        self.assertEqual(text.count("sdk=sdk_roots(env)"), calls)

    def test_compiler_must_be_the_selected_targets_cross_compiler(self):
        s2 = r"C:\pio\packages\toolchain-xtensa-esp-elf\bin\xtensa-esp32s2-elf-g++.exe"
        self.write_database(self.compiler_rows(s2))
        checker.check_compiler(self.database, "esp32s2")
        for target, compiler in (("esp32s3", s2), ("esp32s2", "/usr/bin/g++"),
                                 ("esp32s2", "/opt/xtensa-esp32s3-elf/bin/xtensa-esp32s3-elf-gcc")):
            with self.subTest(target=target, compiler=compiler):
                self.write_database(self.compiler_rows(compiler))
                with self.assertRaisesRegex(ValueError, "not xtensa-" + target):
                    checker.check_compiler(self.database, target)
        self.write_database([{"file": str(self.expected()[0]),
                              "arguments": ["/x/bin/xtensa-esp32s2-elf-gcc", "-c", "a.c"]}])
        checker.check_compiler(self.database, "esp32s2")
        self.write_database([{"file": str(self.expected()[0])}])
        with self.assertRaisesRegex(ValueError, "no command"):
            checker.check_compiler(self.database, "esp32s2")

    def test_sdkconfig_values_distinguish_unset_absent_and_wrong(self):
        path = self.base / "sdkconfig"
        path.write_text('CONFIG_IDF_TARGET="esp32s2"\n# CONFIG_SPIRAM is not set\n'
                        "CONFIG_MOTORCONTROLCAN_CAN_SILENT_LEVEL=0\n", encoding="utf-8")
        self.assertEqual(checker.sdkconfig_values(path),
                         {"CONFIG_IDF_TARGET": '"esp32s2"', "CONFIG_SPIRAM": None,
                          "CONFIG_MOTORCONTROLCAN_CAN_SILENT_LEVEL": "0"})
        checker.check_sdkconfig(path, {"CONFIG_IDF_TARGET": '"esp32s2"', "CONFIG_SPIRAM": checker.NOT_SET})
        # A reviewed absence must be stated as such: UNDECLARED passes only for an absent key.
        checker.check_sdkconfig(path, {"CONFIG_PM_ENABLE": checker.UNDECLARED})
        # Review B-D4: NOT_SET needs the "is not set" line, so an absent or misspelled key fails.
        for required in ({"CONFIG_MOTORCONTROLCAN_CAN_SILENT_LEVEL": "1"},
                         {"CONFIG_ESP_CONSOLE_NONE": "y"}, {"CONFIG_IDF_TARGET": checker.NOT_SET},
                         {"CONFIG_PM_ENABLE": checker.NOT_SET}, {"CONFIG_SPIRM": checker.NOT_SET},
                         {"CONFIG_SPIRAM": checker.UNDECLARED},
                         {"CONFIG_IDF_TARGET": checker.UNDECLARED}):
            with self.subTest(required=required):
                with self.assertRaisesRegex(checker.VerificationFailure, list(required)[0]):
                    checker.check_sdkconfig(path, required)
        with self.assertRaisesRegex(checker.VerificationFailure, r"CONFIG_PM_ENABLE=absent \(requires not set\)"):
            checker.check_sdkconfig(path, {"CONFIG_PM_ENABLE": checker.NOT_SET})

    def test_reviewed_optak_requirements_equal_the_application_configuration(self):
        # The gate and the configuration it checks must not drift apart.
        configured = {}
        for name in ("sdkconfig.defaults", "sdkconfig.optak_s2", "sdkconfig.optak_s2_smoke"):
            configured.update(checker.sdkconfig_values(ROOT / "examples/probe_idf" / name))
        for key, value in checker.OPTAK_S2.items():
            with self.subTest(key=key):
                self.assertEqual(configured.get(key, "absent"), value)
        s2 = checker.sdkconfig_values(ROOT / "examples/probe_idf/sdkconfig.defaults.esp32s2")
        self.assertEqual(s2, {"CONFIG_ESPTOOLPY_FLASHSIZE_4MB": "y", "CONFIG_ESPTOOLPY_FLASHMODE_DIO": "y"})
        # Review B-D3: the S3 flash size lives only in its target defaults; the gate checks it.
        s3 = checker.sdkconfig_values(ROOT / "examples/probe_idf/sdkconfig.defaults.esp32s3")
        self.assertEqual(s3, {"CONFIG_ESPTOOLPY_FLASHSIZE_8MB": "y"})
        self.assertEqual(checker.SHARED_IDF_BUILDS["idf"]["sdkconfig"]["CONFIG_ESPTOOLPY_FLASHSIZE"], '"8MB"')
        shared = checker.sdkconfig_values(ROOT / "examples/probe_idf/sdkconfig.defaults")
        self.assertNotIn("CONFIG_IDF_TARGET", shared)  # The build selects its target.
        self.assertEqual(shared["CONFIG_ESP_CONSOLE_NONE"], "y")
        # Every disabled option the shared gates require is disabled by the shared defaults.
        for key, value in checker.COMMON_SDKCONFIG.items():
            with self.subTest(key=key):
                self.assertEqual(shared.get(key, "absent"), value)
        # The bench gate checks the bench's own defaults, which state the same values.
        bench = checker.sdkconfig_values(ROOT / "examples/s2_bench/sdkconfig.defaults")
        for key, value in checker.BENCH_SDKCONFIG.items():
            if key not in ("CONFIG_ESPTOOLPY_FLASHSIZE", "CONFIG_IDF_TARGET"):
                with self.subTest(bench=key):
                    self.assertEqual(bench.get(key, "absent"), value)
        self.assertEqual(bench["CONFIG_IDF_TARGET"], '"esp32s2"')
        self.assertEqual(bench["CONFIG_ESPTOOLPY_FLASHSIZE_4MB"], "y")
        consumer = checker.sdkconfig_values(ROOT / "test/idf_consumer/sdkconfig.defaults")
        self.assertEqual(consumer, {"CONFIG_COMPILER_CXX_EXCEPTIONS": "n", "CONFIG_COMPILER_CXX_RTTI": "n"})

    def test_all_excludes_only_the_blocked_arduino_s2_gate(self):
        self.assertEqual(set(checker.ALL_TARGETS),
                         {"arduino", "idf", "idf-s2", "idf-s2-optak", "s2", "component"})
        self.assertEqual(checker.ARDUINO_BUILDS["arduino-s2"], ("probe_cli_s2_compile", "esp32s2"))

    def test_platformio_s2_environments_select_an_s2_board_without_usb_console(self):
        import configparser
        config = configparser.ConfigParser(interpolation=None)
        config.read(ROOT / "platformio.ini", encoding="utf-8")
        for name in ("env:probe_cli_s2_compile", "env:probe_cli_s2"):
            with self.subTest(env=name):
                self.assertEqual(config[name]["board"], "esp32-s2-saola-1")
                self.assertEqual(config[name]["board_build.flash_mode"], "dio")
                flags = config[name]["build_flags"].split()
                self.assertIn("-DARDUINO_USB_MODE=0", flags)
                self.assertIn("-DARDUINO_USB_CDC_ON_BOOT=0", flags)
        self.assertIn("-DMOTORCONTROLCAN_COMPILE_FIXTURE=1", config["env:probe_cli_s2_compile"]["build_flags"])
        self.assertEqual(config["env"]["board"], "esp32-s3-devkitc-1")  # S3 fixture unchanged.

    def run_shared_cli(self, target, sdkconfig, compiled=None, linked="esp32s2",
                       compiler="xtensa-esp32s2-elf-g++"):
        project = self.root / "examples/probe_idf"
        for path in (project / "main/main.cpp", self.root / "examples/common/FirmwareSmoke.cpp",
                     self.root / "src/core.cpp", self.root / "examples/s2_bench/main/main.cpp"):
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("// synthetic input\n", encoding="utf-8")
        runner = mock.Mock()
        runner.summary = {}
        runner.directory = self.base / "retained shared output"
        runner.run.return_value = "synthetic revision"
        calls = []

        def build_inputs(*args, **kwargs):
            calls.append((args, kwargs))
            build = args[5]
            build.mkdir(parents=True)
            files = compiled if compiled is not None else (
                self.expected() + [self.root / "src/core.cpp", project / "main/main.cpp"] +
                ([self.root / "examples/common/FirmwareSmoke.cpp"] if target == "idf-s2-optak" else []))
            (build / "compile_commands.json").write_text(json.dumps(
                [{"file": str(path), "command": compiler + " -c " + str(path)} for path in files]),
                encoding="utf-8")
            (build / "project_description.json").write_text(json.dumps({"target": linked}), encoding="utf-8")
            (build / "sdkconfig").write_text(sdkconfig, encoding="utf-8")

        argv = ["check_firmware.py", "--target", target, "--temporary-root", str(self.base)]
        with mock.patch.object(checker, "ROOT", self.root), \
             mock.patch.object(checker, "Runner", return_value=runner), \
             mock.patch.object(checker, "firmware_inventory", return_value=self.inventory), \
             mock.patch.object(checker, "sdk_environment", return_value=(["python", "idf.py"], {})) as sdk, \
             mock.patch.object(checker, "idf_build", side_effect=build_inputs), \
             mock.patch.object(sys, "argv", argv), \
             mock.patch("sys.stdout", new_callable=io.StringIO), \
             mock.patch("sys.stderr", new_callable=io.StringIO):
            result = checker.main()
        return result, runner, calls, sdk

    @staticmethod
    def sdkconfig_text(values):
        """A resolved sdkconfig that satisfies values: UNDECLARED keys are left out."""
        return "".join(("# {} is not set\n".format(key) if value is checker.NOT_SET else "{}={}\n".format(key, value))
                       for key, value in values.items() if value != checker.UNDECLARED)

    def test_shared_s2_fixture_builds_probe_idf_for_s2_without_bench_or_smoke(self):
        spec = checker.SHARED_IDF_BUILDS["idf-s2"]
        result, runner, calls, sdk = self.run_shared_cli("idf-s2", self.sdkconfig_text(spec["sdkconfig"]))
        self.assertEqual(result, 0, runner.summary.get("error"))
        self.assertEqual(sdk.call_args.kwargs["target"], "esp32s2")
        (args, kwargs), = calls
        self.assertEqual(args[3:5], ("idf-s2", self.root / "examples/probe_idf"))
        self.assertEqual(args[6], [])  # Default compile-fixture configuration.
        self.assertEqual(kwargs["target"], "esp32s2")
        self.assertEqual(runner.summary["firmware_targets"], {"idf-s2": "esp32s2"})
        self.assertEqual(runner.summary["idf_s2_compile_entries"], len(SHARED_SOURCES) + 2)
        self.assertEqual(runner.summary["shared_inputs"], sorted(SHARED_SOURCES))

    def test_shared_s2_fixture_rejects_wrong_target_compiler_config_or_extra_sources(self):
        spec = checker.SHARED_IDF_BUILDS["idf-s2"]
        good = self.sdkconfig_text(spec["sdkconfig"])
        project = self.root / "examples/probe_idf"
        base = self.expected() + [self.root / "src/core.cpp", project / "main/main.cpp"]
        cases = (
            ({"linked": "esp32s3"}, "not esp32s2"),
            ({"compiler": "xtensa-esp32s3-elf-g++"}, "not xtensa-esp32s2"),
            ({"sdkconfig": good.replace("CONFIG_ESP_CONSOLE_NONE=y", "CONFIG_ESP_CONSOLE_UART=y")},
             "CONFIG_ESP_CONSOLE_NONE"),
            ({"compiled": base + [self.root / "examples/common/FirmwareSmoke.cpp"]}, "forbidden.*FirmwareSmoke"),
            ({"compiled": base + [self.root / "examples/s2_bench/main/main.cpp"]}, "forbidden.*s2_bench"),
            ({"compiled": base + [self.root / "examples/common/CommandParser.cpp"]},
             "unexpected checkout inputs:.*CommandParser"),
            ({"compiled": base[1:]}, "omitted"))
        for options, message in cases:
            with self.subTest(message=message):
                options = dict(options)
                sdkconfig = options.pop("sdkconfig", good)
                result, runner, _, _ = self.run_shared_cli("idf-s2", sdkconfig, **options)
                self.assertEqual(result, 1)
                self.assertRegex(runner.summary["error"].replace("\\", "/"), message)

    def test_shared_s3_fixture_checks_8mb_flash_and_the_reviewed_psram_absence(self):
        spec = checker.SHARED_IDF_BUILDS["idf"]
        good = self.sdkconfig_text(spec["sdkconfig"])
        self.assertNotIn("CONFIG_SPIRAM", good)  # Undeclared: S3 builds no esp_psram component.
        options = {"linked": "esp32s3", "compiler": "xtensa-esp32s3-elf-g++"}
        result, runner, calls, sdk = self.run_shared_cli("idf", good, **options)
        self.assertEqual(result, 0, runner.summary.get("error"))
        self.assertEqual(sdk.call_args.kwargs["target"], "esp32s3")
        self.assertEqual(runner.summary["firmware_targets"], {"idf": "esp32s3"})
        for wrong, message in ((good.replace('CONFIG_ESPTOOLPY_FLASHSIZE="8MB"', 'CONFIG_ESPTOOLPY_FLASHSIZE="4MB"'),
                                "CONFIG_ESPTOOLPY_FLASHSIZE"),
                               (good.replace('CONFIG_ESPTOOLPY_FLASHSIZE="8MB"\n', ""), "CONFIG_ESPTOOLPY_FLASHSIZE"),
                               (good + "# CONFIG_SPIRAM is not set\n", "CONFIG_SPIRAM=not set"),
                               (good + "CONFIG_SPIRAM=y\n", "CONFIG_SPIRAM"),
                               (good.replace("# CONFIG_PM_ENABLE is not set\n", ""), "CONFIG_PM_ENABLE=absent")):
            with self.subTest(message=message):
                result, runner, _, _ = self.run_shared_cli("idf", wrong, **options)
                self.assertEqual(result, 1)
                self.assertIn(message, runner.summary["error"])

    def test_optak_s2_image_adds_board_configuration_and_smoke_source_only(self):
        spec = checker.SHARED_IDF_BUILDS["idf-s2-optak"]
        good = self.sdkconfig_text(spec["sdkconfig"])
        result, runner, calls, _ = self.run_shared_cli("idf-s2-optak", good)
        self.assertEqual(result, 0, runner.summary.get("error"))
        (args, kwargs), = calls
        project = (self.root / "examples/probe_idf").as_posix()
        self.assertEqual(args[6], ["-DSDKCONFIG_DEFAULTS=" + ";".join(
            project + "/" + name for name in ("sdkconfig.defaults", "sdkconfig.optak_s2",
                                              "sdkconfig.optak_s2_smoke"))])
        self.assertEqual(runner.summary["idf_s2_optak_compile_entries"], len(SHARED_SOURCES) + 3)
        for wrong in (good.replace("CONFIG_MOTORCONTROLCAN_CAN_SILENT_LEVEL=1",
                                   "CONFIG_MOTORCONTROLCAN_CAN_SILENT_LEVEL=0"),
                      good.replace("CONFIG_MOTORCONTROLCAN_CAN_TX_PIN=17", "CONFIG_MOTORCONTROLCAN_CAN_TX_PIN=19"),
                      good.replace("# CONFIG_MOTORCONTROLCAN_COMPILE_FIXTURE is not set",
                                   "CONFIG_MOTORCONTROLCAN_COMPILE_FIXTURE=y")):
            with self.subTest(wrong=wrong.splitlines()[0]):
                result, runner, _, _ = self.run_shared_cli("idf-s2-optak", wrong)
                self.assertEqual(result, 1)
                self.assertIn("differs from the selected board", runner.summary["error"])
        compiled = self.expected() + [self.root / "src/core.cpp", self.root / "examples/probe_idf/main/main.cpp"]
        result, runner, _, _ = self.run_shared_cli("idf-s2-optak", good, compiled=compiled)
        self.assertEqual(result, 1)
        self.assertIn("FirmwareSmoke", runner.summary["error"])

    def run_component_cli(self, compiler="xtensa-esp32s3-elf-g++", linked="esp32s3", sdkconfig=None,
                          extra=(), checkout=()):
        """The isolated core consumer through main(); extra are staged-relative, checkout
        checkout-relative compiled files."""
        for name in ("CMakeLists.txt", "library.json", "include/MotorControlCAN/Core.h", "src/core.cpp",
                     "test/idf_consumer/main/main.cpp"):
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("// synthetic core consumer input\n", encoding="utf-8")
        runner = mock.Mock()
        runner.summary = {}
        runner.directory = self.base / "retained component output"
        runner.run.return_value = "synthetic revision"
        calls = []

        def build_inputs(*args, **kwargs):
            calls.append((args, kwargs))
            build = args[5]
            isolated = build.parent
            build.mkdir(parents=True)
            files = ([isolated / "MotorControlCAN/src/core.cpp", isolated / "consumer/main/main.cpp",
                      build / "project_elf_src_esp32s3.c"] + [isolated / item for item in extra] +
                     [self.root / item for item in checkout])
            (build / "compile_commands.json").write_text(json.dumps(
                [{"file": str(path), "command": compiler + " -c " + str(path)} for path in files]),
                encoding="utf-8")
            (build / "project_description.json").write_text(json.dumps({"target": linked}), encoding="utf-8")
            (build / "sdkconfig").write_text(
                self.sdkconfig_text(checker.COMPONENT_SDKCONFIG) if sdkconfig is None else sdkconfig,
                encoding="utf-8")

        argv = ["check_firmware.py", "--target", "component", "--temporary-root", str(self.base)]
        with mock.patch.object(checker, "ROOT", self.root), \
             mock.patch.object(checker, "Runner", return_value=runner), \
             mock.patch.object(checker, "firmware_inventory", return_value=self.inventory), \
             mock.patch.object(checker, "sdk_environment", return_value=(["python", "idf.py"], {})), \
             mock.patch.object(checker, "idf_build", side_effect=build_inputs), \
             mock.patch.object(sys, "argv", argv), \
             mock.patch("sys.stdout", new_callable=io.StringIO), \
             mock.patch("sys.stderr", new_callable=io.StringIO):
            result = checker.main()
        return result, runner, calls

    def test_component_checks_target_compiler_configuration_and_exact_staged_inputs(self):
        # Review B-D6: the isolated consumer gate asserted no target or compiler before.
        result, runner, calls = self.run_component_cli()
        self.assertEqual(result, 0, runner.summary.get("error"))
        self.assertEqual(runner.summary["firmware_targets"], {"component": "esp32s3"})
        self.assertEqual(runner.summary["component_compile_entries"], 3)
        (args, kwargs), = calls
        self.assertEqual(args[3], "component")
        self.assertNotIn("target", kwargs)  # idf_build's default: esp32s3.
        good = self.sdkconfig_text(checker.COMPONENT_SDKCONFIG)
        cases = (({"linked": "esp32s2"}, "not esp32s3"),
                 ({"compiler": "xtensa-esp32s2-elf-g++"}, "not xtensa-esp32s3"),
                 ({"sdkconfig": good.replace("# CONFIG_COMPILER_CXX_RTTI is not set", "CONFIG_COMPILER_CXX_RTTI=y")},
                  "CONFIG_COMPILER_CXX_RTTI"),
                 ({"extra": ("MotorControlCAN/src/unlisted/Extra.cpp",)}, "unexpected checkout inputs:.*Extra"),
                 ({"checkout": ("src/core.cpp",)}, "forbidden inputs:.*core"))
        for options, message in cases:
            with self.subTest(message=message):
                result, runner, _ = self.run_component_cli(**options)
                self.assertEqual(result, 1)
                self.assertRegex(runner.summary["error"].replace("\\", "/"), message)

    def run_arduino(self, compiled):
        """The arduino gate's success path, with PlatformIO replaced by a fixed database."""
        runner = mock.Mock()
        runner.summary = {}
        # As in a real run the PlatformIO build directory lies inside the checkout.
        runner.directory = self.root / ".build/arduino evidence"

        def run(name, command, **kwargs):
            if name == "arduino_inputs":
                output = runner.directory / "arduino/probe_cli_compile"
                output.mkdir(parents=True, exist_ok=True)
                (output / "compile_commands.json").write_text(json.dumps(
                    [{"file": str(path), "command": "xtensa-esp32s3-elf-g++ -c " + str(path)}
                     for path in compiled]), encoding="utf-8")
                (output / "firmware.elf").write_bytes(b"synthetic ELF")
                (output / "firmware.bin").write_bytes(b"synthetic BIN")
            return "synthetic"

        runner.run.side_effect = run
        with mock.patch.object(checker, "ROOT", self.root):
            checker.arduino_build(runner, "pio", "arduino", self.expected())
        return runner

    def test_arduino_requires_its_entry_and_refuses_extra_checkout_sources(self):
        # Review B-D2 for the Arduino gate: its thin entry is required and nothing else of
        # the checkout may join; framework and build-directory files are not checkout inputs.
        framework = self.base / "platformio/packages/framework-arduinoespressif32/cores/esp32/main.cpp"
        entry = self.root / "examples/probe_cli/main.cpp"
        generated = self.root / ".build/arduino evidence/arduino/probe_cli_compile/generated.cpp"
        good = self.expected() + [entry, framework, generated]
        runner = self.run_arduino(good)
        self.assertEqual(runner.summary["arduino_compile_entries"], len(good))
        self.assertEqual(runner.summary["firmware_targets"], {"arduino": "esp32s3"})
        for compiled, message in (([path for path in good if path != entry], "omitted.*probe_cli"),
                                  (good + [self.root / "examples/common/CommandParser.cpp"],
                                   "unexpected checkout inputs:.*CommandParser"),
                                  (good + [self.root / "examples/common/FirmwareSmoke.cpp"],
                                   "forbidden inputs:.*FirmwareSmoke")):
            with self.subTest(message=message):
                with self.assertRaisesRegex(ValueError, message):
                    self.run_arduino(compiled)

    def run_arduino_s2(self, log_text):
        runner = checker.Runner(self.base / "arduino s2 evidence", "firmware-arduino-s2")
        original = runner.run

        def run(name, command, **kwargs):
            if name == "arduino-s2":
                log = runner.directory / "02_arduino-s2.log"
                log.write_text(log_text, encoding="utf-8")
                runner.summary["steps"].append({"name": name, "log": str(log), "status": "failed"})
                raise checker.VerificationFailure(name + ": see " + str(log))
            if name == "platformio_version":
                return "PlatformIO Core, version 6.1.19"
            return "synthetic"

        argv = ["check_firmware.py", "--target", "arduino-s2"]
        with mock.patch.object(checker, "Runner", return_value=runner), \
             mock.patch.object(runner, "run", side_effect=run), \
             mock.patch.object(checker, "firmware_inventory", return_value=self.inventory), \
             mock.patch.object(sys, "argv", argv), \
             mock.patch("sys.stdout", new_callable=io.StringIO), \
             mock.patch("sys.stderr", new_callable=io.StringIO):
            result = checker.main()
        del original
        return result, runner

    def test_arduino_s2_reports_the_console_blocker_and_never_passes(self):
        result, runner = self.run_arduino_s2(
            "EspProbePlatform.cpp:21:2: error: #error \"" + checker.ARDUINO_S2_GUARD + "\"\n")
        self.assertEqual(result, 1)
        self.assertEqual(runner.summary["status"], "blocked")
        self.assertEqual(runner.summary["arduino_s2_blocker"]["guard"], checker.ARDUINO_S2_GUARD)
        self.assertNotIn("firmware_artifacts", runner.summary)
        result, runner = self.run_arduino_s2("unrelated compiler failure\n")
        self.assertEqual(result, 1)
        self.assertEqual(runner.summary["status"], "failed")
        self.assertNotIn("arduino_s2_blocker", runner.summary)


if __name__ == "__main__":
    unittest.main()
