#!/usr/bin/env python3
"""Exercise s2_flash.py with fakes: no process, port, esptool or flash is ever used."""

import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
import s2_flash as flash  # noqa: E402


ROOT = flash.ROOT
RETAINED_UPLOAD = ROOT / "docs/bench/20261102T173312Z-s2-upload"
REVISION = "a" * 40
SERIAL, PORT = flash.FIXTURE["usb_serial"], flash.FIXTURE["port"]
MAC_OUTPUT = ("esptool.py v4.12.0\nChip is ESP32-S2FH4 (revision v1.0)\nMAC: %s\nHard resetting via RTS pin...\n"
              % flash.FIXTURE["mac"])
WRITE_OUTPUT = MAC_OUTPUT + "Hash of data verified.\n" * 3


def port_info(device=PORT, serial=SERIAL, vid=0x303A, pid=0x1002):
    return SimpleNamespace(device=device, serial_number=serial, vid=vid, pid=pid, description="fake")


def executable(path):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("not run\n", encoding="utf-8")
    path.chmod(path.stat().st_mode | stat.S_IEXEC)
    return path


def make_tools(base):
    """An installed-tool layout with inert files; nothing here can execute usefully."""
    suffix = ".exe" if os.name == "nt" else ""
    packages = base / "packages"
    idf = packages / "framework-espidf"
    (idf / "tools").mkdir(parents=True)
    (idf / "tools/idf.py").write_text("not run\n", encoding="utf-8")
    (idf / "tools/tools.json").write_text(json.dumps({"tools": [
        {"name": "xtensa-esp-elf", "versions": [{"name": "esp-fake", "status": "recommended"}]}]}),
        encoding="utf-8")
    executable(packages / "toolchain-xtensa-esp-elf/bin" / ("xtensa-esp32s2-elf-gcc" + suffix))
    executable(packages / "tool-cmake/bin" / ("cmake" + suffix))
    executable(packages / "tool-ninja" / ("ninja" + suffix))
    python = executable(base / "python" / ("python" + suffix))
    tools_path = base / "idf-tools"
    tools_path.mkdir()
    return idf, python, tools_path


def write_build(build, *, target="esp32s2", chip=2, sdkconfig=None, flash_args=None, encrypted="false",
                revision=REVISION):
    """Generated outputs in the exact shape of a real build directory."""
    build = Path(build)
    (build / "bootloader").mkdir(parents=True, exist_ok=True)
    (build / "partition_table").mkdir(exist_ok=True)
    elf = build / (flash.APP + ".elf")
    elf.write_bytes(b"\x7fELF fake")
    header = bytes([0xE9, 6, 2, 0x2F]) + bytes(8) + chip.to_bytes(2, "little") + bytes(2)
    application = bytearray(header + bytes(0x200))
    application[flash.ELF_HASH_OFFSET:flash.ELF_HASH_OFFSET + 32] = hashlib.sha256(elf.read_bytes()).digest()
    (build / (flash.APP + ".bin")).write_bytes(application)
    (build / "bootloader/bootloader.bin").write_bytes(header + bytes(32))
    (build / "partition_table/partition-table.bin").write_bytes(bytes.fromhex("aa500102") + bytes(3068))
    (build / "flash_args").write_text(flash_args if flash_args is not None else (
        "--flash_mode dio --flash_freq 80m --flash_size 4MB\n0x1000 bootloader/bootloader.bin\n"
        "0x10000 %s.bin\n0x8000 partition_table/partition-table.bin\n" % flash.APP), encoding="utf-8")
    (build / "flasher_args.json").write_text(json.dumps({
        "extra_esptool_args": {"after": "hard_reset", "before": "default_reset", "stub": True, "chip": target},
        "bootloader": {"encrypted": encrypted}, "app": {"encrypted": encrypted},
        "partition-table": {"encrypted": encrypted}}), encoding="utf-8")
    (build / "project_description.json").write_text(json.dumps({
        "target": target, "git_revision": "5.5.5", "project_version": "fake", "c_compiler": "fake"}), encoding="utf-8")
    defaults = (flash.PROJECT / "sdkconfig.defaults").read_text(encoding="utf-8")
    text = sdkconfig if sdkconfig is not None else (
        "\n".join(line for line in defaults.splitlines() if line.startswith("CONFIG_")) + "\n")
    (build / "sdkconfig").write_text(text, encoding="utf-8")
    (build / "compile_commands.json").write_text(json.dumps([{
        "file": "C:/x/examples/s2_bench/main/main.cpp",
        "command": 'g++ -DMOTORCONTROLCAN_SOURCE_REVISION=\\"%s\\" -c main.cpp' % revision}]), encoding="utf-8")


IDENTITY = {"base_commit": REVISION, "working_tree": " M src/x.cpp\n", "dirty_sha256": {"src/x.cpp": "1" * 64},
            "source_sha256": {"src/x.cpp": "1" * 64}}


class Bench:
    """Fake process runner that records every command and refuses unknown ones."""

    def __init__(self, build, read_mac=MAC_OUTPUT, write=(0, WRITE_OUTPUT), build_code=0, **build_options):
        self.build, self.read_mac, self.write = build, read_mac, write
        self.build_code, self.build_options = build_code, build_options
        self.commands = []

    def names(self):
        return [name for name, _ in self.commands]

    def execute(self, command, cwd, log, env=None, timeout=600):
        words = [str(part) for part in command]
        if "--version" in words or "version" in words:
            name, output = "version", "tool esp-fake 4.12.0\n"
            code = 0
        elif "idf.py" in " ".join(words):
            name, code, output = "build", self.build_code, "built\n"
            if self.build_code == 0:
                write_build(self.build, **self.build_options)
        elif "read_mac" in words:
            name, code, output = "read_mac", 0, self.read_mac
        elif "write_flash" in words:
            name, (code, output) = "write_flash", self.write
        else:
            raise AssertionError("unexpected command: %s" % words)
        self.commands.append((name, words))
        Path(log).write_text(output, encoding="utf-8")
        return code, output


class Fixture(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.base = Path(temporary.name)
        self.idf, self.python, self.tools_path = make_tools(self.base)
        self.build = self.base / "build"
        self.output = self.base / "artifact"
        self.environment = mock.patch.dict(os.environ, {"PATH": ""}, clear=False)
        self.environment.start()
        self.addCleanup(self.environment.stop)
        for name in ("MSYSTEM", "IDF_PATH", "IDF_TOOLS_PATH", "IDF_PYTHON"):
            os.environ.pop(name, None)

    def arguments(self, *mode):
        return [*mode, "--idf-path", str(self.idf), "--idf-python", str(self.python),
                "--idf-tools-path", str(self.tools_path), "--build-dir", str(self.build),
                "--output", str(self.output)]

    def run_mode(self, bench, *mode, ports=None, identity=None):
        """Run one mode against the fake bench; nothing reaches a real process or port."""
        gate = mock.Mock(return_value=(["python", "idf.py"], dict(os.environ)))
        runner = mock.Mock()
        runner.summary = {"source_hashes_verified": 41}
        with mock.patch.object(flash, "execute", bench.execute), \
                mock.patch.object(flash, "sdk_environment", gate), \
                mock.patch.object(flash, "Runner", return_value=runner), \
                mock.patch.object(flash, "comports", return_value=[port_info()] if ports is None else ports) as usb, \
                mock.patch.object(flash, "source_identity", side_effect=identity or (lambda: dict(IDENTITY))), \
                mock.patch.object(subprocess, "run", side_effect=AssertionError("real process")), \
                mock.patch("sys.stdout", new_callable=io.StringIO) as out, \
                mock.patch("sys.stderr", new_callable=io.StringIO) as err:
            code = flash.main(self.arguments(*mode))
        return code, out.getvalue() + err.getvalue(), usb

    def record(self):
        return json.loads((self.output / "build.json").read_text(encoding="utf-8"))

    def flash(self, bench, **options):
        return self.run_mode(bench, "--flash", "--port", PORT, "--serial", SERIAL, **options)


class Modes(Fixture):
    def test_a_mode_is_required_and_modes_are_exclusive(self):
        with mock.patch("sys.stderr", new_callable=io.StringIO):
            for argv in ([], ["--dry-run", "--flash"], ["--build-only", "--flash"]):
                with self.assertRaises(SystemExit):
                    flash.parse_args(argv)

    def test_flash_names_port_and_serial_explicitly(self):
        with mock.patch("sys.stderr", new_callable=io.StringIO):
            for argv in (["--flash"], ["--flash", "--port", PORT], ["--flash", "--serial", SERIAL],
                         ["--check-ping", "x"]):
                with self.assertRaises(SystemExit):
                    flash.parse_args(argv)

    def test_no_ignored_helper_is_referenced(self):
        source = Path(flash.__file__).read_text(encoding="utf-8")
        self.assertNotIn("flash_s2_bench", source)
        self.assertNotIn("s2_bench_build", source)


class DryRun(Fixture):
    def dry(self):
        blocked = mock.Mock(side_effect=AssertionError("dry run touched hardware or a process"))
        with mock.patch.object(subprocess, "run", blocked), mock.patch.object(subprocess, "Popen", blocked), \
                mock.patch.object(flash, "execute", blocked), mock.patch.object(flash, "comports", blocked), \
                mock.patch.object(flash, "sdk_environment", blocked), mock.patch.object(flash, "Runner", blocked), \
                mock.patch.object(flash, "source_identity", blocked), \
                mock.patch.dict(sys.modules, {"serial": None, "serial.tools": None}), \
                mock.patch("sys.stdout", new_callable=io.StringIO) as out:
            code = flash.main(self.arguments("--dry-run"))
        return code, out.getvalue()

    def test_prints_the_plan_and_touches_nothing(self):
        code, output = self.dry()
        self.assertEqual(code, 0)
        self.assertFalse(self.output.exists(), "a dry run creates no artifact")
        self.assertFalse(self.build.exists(), "a dry run creates no build")
        for text in ("write_flash @flash_args", "read_mac", "--chip esp32s2", "--baud 115200", SERIAL,
                     "idf.py", "reconfigure build", "s2_bench.py", "--command ping", "--check-ping",
                     "VERDICT: INCOMPLETE"):
            self.assertIn(text, output)
        for text in ("erase_flash", "erase-flash", "burn_efuse", "espefuse", "PASS"):
            self.assertNotIn(text, output)

    def test_never_claims_a_build_or_upload(self):
        _, output = self.dry()
        self.assertIn("not a build or an upload", output)

    def test_missing_tool_is_reported(self):
        (self.idf.parent / "tool-cmake/bin").rename(self.base / "moved")
        code, output = self.dry()
        self.assertEqual(code, 2)
        self.assertIn("VERDICT: UNAVAILABLE", output)
        self.assertIn("cmake", output)

    def test_validates_an_existing_build_without_touching_it(self):
        write_build(self.build)
        _, output = self.dry()
        self.assertIn("is valid for this plan", output)
        write_build(self.build, chip=9)
        _, output = self.dry()
        self.assertIn("would be refused", output)


class Guards(unittest.TestCase):
    def test_only_mac_read_and_image_write_are_permitted(self):
        base = ["python", "-m", "esptool", "--chip", "esp32s2", "--port", PORT]
        flash.assert_safe_esptool(base + ["read_mac"])
        flash.assert_safe_esptool(base + ["write_flash", "@flash_args"])
        for extra in (["erase_flash"], ["erase_region", "0", "4096"], ["write_flash", "--erase-all", "@f"],
                      ["write_flash", "--encrypt", "@f"], ["write_flash", "--force", "@f"],
                      ["burn_efuse", "X"], ["merge_bin"], ["read_flash", "0", "1", "f"], ["load_ram", "f"],
                      ["read_mac", "write_flash", "@f"], []):
            with self.assertRaises(flash.FlashError, msg=str(extra)):
                flash.assert_safe_esptool(base + extra)

    def test_install_path_words_do_not_count_as_arguments(self):
        flash.assert_safe_esptool(["C:/force/erase/python", "-m", "esptool", "--chip", "esp32s2", "read_mac"])

    def test_build_uses_only_reconfigure_and_build(self):
        tools = {"paths": {"idf_python": "python", "idf": "idf"}}
        command = flash.build_command(tools, "b")
        self.assertEqual(command[-2:], list(flash.IDF_ACTIONS))
        self.assertEqual(flash.IDF_ACTIONS, ("reconfigure", "build"))
        self.assertTrue(any(word.startswith("-DIDF_TARGET=esp32s2") for word in command))


class Usb(unittest.TestCase):
    def test_recorded_probe_is_accepted(self):
        result = flash.check_usb(PORT, SERIAL, [port_info(), port_info("COM3", "OTHER", 1, 2)])
        self.assertEqual((result["port"], result["vid_pid"]), (PORT, "303A:1002"))

    def test_identity_is_checked_not_assumed(self):
        cases = {
            "another serial argument": ("COM15", "OTHER", [port_info(serial="OTHER")]),
            "port absent": (PORT, SERIAL, [port_info("COM7")]),
            "no ports": (PORT, SERIAL, []),
            "other serial on port": (PORT, SERIAL, [port_info(serial="OTHER")]),
            "other vid": (PORT, SERIAL, [port_info(vid=0x1234)]),
            "other pid": (PORT, SERIAL, [port_info(pid=0x1001)]),
            "serial on two ports": (PORT, SERIAL, [port_info(), port_info("COM16")]),
            "duplicate name": (PORT, SERIAL, [port_info(), port_info()]),
        }
        for name, (port, serial, ports) in cases.items():
            with self.assertRaises(flash.FlashError, msg=name):
                flash.check_usb(port, serial, ports)

    def test_target_identity_uses_the_real_retained_log_format(self):
        log = (RETAINED_UPLOAD / "flash.log").read_text(encoding="utf-8")
        facts = flash.parse_target(log)
        self.assertEqual(facts["chip"], "ESP32-S2FH4 (revision v1.0)")
        self.assertEqual(facts["esptool"], flash.PINS["esptool"])
        self.assertEqual(facts["mac"], flash.FIXTURE["mac"])
        flash.check_target(facts)
        self.assertEqual(log.count("Hash of data verified."), len(flash.FLASH_LAYOUT))
        self.assertIn("Hard resetting", log)

    def test_wrong_target_is_refused(self):
        good = dict(chip="ESP32-S2FH4", mac=flash.FIXTURE["mac"], flash_size=None)
        flash.check_target(good)
        for change in ({"chip": "ESP32-S3"}, {"chip": None}, {"mac": "00:00:00:00:00:00"}, {"mac": None},
                       {"flash_size": "2MB"}):
            with self.assertRaises(flash.FlashError, msg=str(change)):
                flash.check_target(dict(good, **change))


class Validation(Fixture):
    def test_valid_build(self):
        write_build(self.build)
        facts = flash.validate_build(self.build)
        self.assertEqual(facts["target"], "esp32s2")
        self.assertEqual(set(facts["image_sha256"]), {
            "bootloader/bootloader.bin", "partition_table/partition-table.bin", flash.APP + ".bin",
            flash.APP + ".elf", "sdkconfig"})

    def test_generated_args_outside_the_reviewed_set(self):
        good = ("--flash_mode dio --flash_freq 80m --flash_size 4MB\n0x1000 bootloader/bootloader.bin\n"
                "0x10000 %s.bin\n0x8000 partition_table/partition-table.bin\n" % flash.APP)
        flash.parse_flash_args(good)
        for bad in ("",
                    good.replace("--flash_size 4MB", "--flash_size 8MB"),
                    good.replace("--flash_mode dio", "--flash_mode qio"),
                    good.replace("--flash_size 4MB", "--flash_size 4MB --encrypt"),
                    good.replace("--flash_size 4MB", "--flash_size 4MB --erase-all"),
                    good + "0x200000 extra.bin\n",
                    good.replace("0x10000", "0x20000"),
                    good.replace("bootloader/bootloader.bin", "other.bin"),
                    good + "0x1000 bootloader/bootloader.bin\n",
                    good.replace("0x1000 ", "4096 ")):
            with self.assertRaises(flash.FlashError, msg=bad):
                flash.parse_flash_args(bad)

    def test_build_outputs_for_another_board_or_config(self):
        cases = ({"target": "esp32s3"}, {"chip": 9}, {"encrypted": "true"},
                 {"sdkconfig": 'CONFIG_IDF_TARGET="esp32s3"\n'},
                 {"flash_args": "--flash_mode dio --flash_freq 80m --flash_size 4MB\n"})
        for options in cases:
            write_build(self.build, **options)
            with self.assertRaises(flash.FlashError, msg=str(options)):
                flash.validate_build(self.build)

    def test_image_must_embed_this_elf(self):
        write_build(self.build)
        (self.build / (flash.APP + ".elf")).write_bytes(b"different")
        with self.assertRaisesRegex(flash.FlashError, "embed"):
            flash.validate_build(self.build)

    def test_missing_outputs_are_refused(self):
        write_build(self.build)
        (self.build / "bootloader/bootloader.bin").unlink()
        with self.assertRaises(flash.FlashError):
            flash.validate_build(self.build)
        with self.assertRaises(flash.FlashError):
            flash.validate_build(self.base / "nothing")

    def test_sdkconfig_drift_from_tracked_defaults(self):
        defaults = (flash.PROJECT / "sdkconfig.defaults").read_text(encoding="utf-8")
        configured = "\n".join(l for l in defaults.splitlines() if l.startswith("CONFIG_"))
        self.assertEqual(flash.check_sdkconfig(defaults, configured), [])
        self.assertTrue(flash.check_sdkconfig(defaults, configured.replace("CONFIG_FREERTOS_HZ=1000", "CONFIG_FREERTOS_HZ=100")))
        self.assertTrue(flash.check_sdkconfig(defaults, configured + "\nCONFIG_SPIRAM=y"))
        self.assertTrue(flash.check_sdkconfig(defaults, ""))

    def test_compiled_revision_is_read_from_the_compile_command(self):
        write_build(self.build)
        self.assertEqual(flash.compiled_revision(self.build), REVISION)

    def test_source_change_detection(self):
        after = json.loads(json.dumps(IDENTITY))
        self.assertEqual(flash.changed_sources(IDENTITY, after), [])
        after["source_sha256"]["src/x.cpp"] = "2" * 64
        after["source_sha256"]["src/new.cpp"] = "3" * 64
        self.assertEqual(flash.changed_sources(IDENTITY, after), ["src/new.cpp", "src/x.cpp"])
        self.assertIn("HEAD", flash.changed_sources(IDENTITY, dict(IDENTITY, base_commit="b" * 40)))


class BuildOnly(Fixture):
    def test_builds_without_any_port_activity(self):
        bench = Bench(self.build)
        code, output, usb = self.run_mode(bench, "--build-only")
        self.assertEqual(code, 0, output)
        usb.assert_not_called()
        self.assertNotIn("read_mac", bench.names())
        self.assertNotIn("write_flash", bench.names())
        record = self.record()
        self.assertEqual(record["status"], "built")
        self.assertEqual(record["base_commit"], REVISION)
        self.assertEqual(record["dirty_sha256"], {"src/x.cpp": "1" * 64})
        self.assertTrue(record["source_stable"])
        self.assertIn(flash.APP + ".elf", record["image_sha256"])
        self.assertIn("compiler", record["tool_versions"])
        for name in ("build.log", "sdkconfig", "flash_args", "flasher_args.json", "project_description.json"):
            self.assertTrue((self.output / name).is_file(), name)
        self.assertEqual(record["compiled_revision"], REVISION)

    def test_failed_build_is_retained(self):
        bench = Bench(self.build, build_code=1)
        code, output, _ = self.run_mode(bench, "--build-only")
        self.assertEqual(code, 1)
        self.assertEqual(self.record()["status"], "build_failed")
        self.assertTrue((self.output / "build.log").is_file())
        self.assertIn("build_failed", output)

    def test_stale_embedded_revision_is_refused(self):
        bench = Bench(self.build, revision="b" * 40)
        code, _, _ = self.run_mode(bench, "--build-only")
        self.assertEqual(code, 1)
        self.assertEqual(self.record()["status"], "build_invalid")

    def test_wrong_compiler_is_refused_not_upgraded(self):
        bench = Bench(self.build)
        (self.idf / "tools/tools.json").write_text(json.dumps({"tools": [
            {"name": "xtensa-esp-elf", "versions": [{"name": "esp-other", "status": "recommended"}]}]}), encoding="utf-8")
        code, _, _ = self.run_mode(bench, "--build-only")
        self.assertEqual(code, 1)
        self.assertEqual(self.record()["status"], "unavailable")
        self.assertNotIn("build", bench.names())

    def test_existing_artifact_directory_is_refused(self):
        self.output.mkdir()
        code, output, _ = self.run_mode(Bench(self.build), "--build-only")
        self.assertEqual(code, 2)
        self.assertIn("already exists", output)

    def test_msys_is_refused_before_anything_runs(self):
        bench = Bench(self.build)
        with mock.patch.dict(os.environ, {"MSYSTEM": "MINGW64"}):
            code, output, _ = self.run_mode(bench, "--build-only")
        self.assertEqual(code, 2)
        self.assertEqual(bench.commands, [])
        self.assertIn("PowerShell", output)

    def test_missing_tools_run_nothing(self):
        (self.idf.parent / "tool-ninja").rename(self.base / "moved")
        bench = Bench(self.build)
        code, output, _ = self.run_mode(bench, "--build-only")
        self.assertEqual(code, 2)
        self.assertEqual(bench.commands, [])
        self.assertFalse(self.output.exists())
        self.assertIn("ninja", output)


class Upload(Fixture):
    def test_full_sequence_and_retained_record(self):
        bench = Bench(self.build)
        code, output, _ = self.flash(bench)
        self.assertEqual(code, 0, output)
        names = bench.names()
        self.assertEqual(names[names.index("build"):], ["build", "read_mac", "write_flash"])
        self.assertEqual(names.count("write_flash"), 1)
        write = dict(bench.commands)["write_flash"]
        self.assertEqual(write[write.index("--chip") + 1], "esp32s2")
        self.assertEqual(write[write.index("--port") + 1], PORT)
        self.assertEqual(write[write.index("--baud") + 1], "115200")
        self.assertEqual(write[-2:], ["write_flash", "@flash_args"])
        record = self.record()
        self.assertEqual(record["status"], "flashed")
        self.assertEqual(record["usb"]["usb_serial"], SERIAL)
        self.assertEqual(record["target_identity"]["mac"], flash.FIXTURE["mac"])
        self.assertEqual(record["flash"]["verified_images"], 3)
        self.assertEqual((record["erase_all"], record["efuse"], record["retry"]), (False, False, False))
        self.assertTrue((self.output / "flash.log").is_file())
        self.assertIn("s2_bench.py", output)
        self.assertIn("--check-ping", output)

    def test_wrong_probe_stops_before_build_and_artifact(self):
        bench = Bench(self.build)
        code, output, _ = self.flash(bench, ports=[port_info(serial="OTHER")])
        self.assertEqual(code, 2)
        self.assertEqual(bench.commands, [])
        self.assertFalse(self.output.exists())
        self.assertIn("Refused", output)

    def test_probe_removed_during_the_build_stops_the_write(self):
        bench = Bench(self.build)
        sequence = [[port_info()], []]
        with mock.patch.object(flash, "comports", side_effect=sequence):
            gate = mock.Mock(return_value=(["python", "idf.py"], dict(os.environ)))
            runner = mock.Mock(summary={})
            with mock.patch.object(flash, "execute", bench.execute), \
                    mock.patch.object(flash, "sdk_environment", gate), \
                    mock.patch.object(flash, "Runner", return_value=runner), \
                    mock.patch.object(flash, "source_identity", side_effect=lambda: dict(IDENTITY)), \
                    mock.patch("sys.stdout", new_callable=io.StringIO), \
                    mock.patch("sys.stderr", new_callable=io.StringIO):
                code = flash.main(self.arguments("--flash", "--port", PORT, "--serial", SERIAL))
        self.assertEqual(code, 1)
        self.assertNotIn("read_mac", bench.names())
        self.assertNotIn("write_flash", bench.names())

    def test_other_target_behind_the_probe_gets_no_write(self):
        bench = Bench(self.build, read_mac=MAC_OUTPUT.replace(flash.FIXTURE["mac"], "00:11:22:33:44:55"))
        code, _, _ = self.flash(bench)
        self.assertEqual(code, 1)
        self.assertNotIn("write_flash", bench.names())
        self.assertEqual(self.record()["status"], "refused")
        self.assertTrue((self.output / "read_mac.log").is_file())

    def test_failed_write_is_retained_and_not_retried(self):
        bench = Bench(self.build, write=(2, "A fatal error occurred\n"))
        code, output, _ = self.flash(bench)
        self.assertEqual(code, 1)
        self.assertEqual(bench.names().count("write_flash"), 1)
        record = self.record()
        self.assertEqual(record["status"], "flash_failed")
        self.assertIn("unknown", record["error"])
        self.assertEqual((self.output / "flash.log").read_text(encoding="utf-8"), "A fatal error occurred\n")
        self.assertNotIn("--check-ping", output)

    def test_success_exit_without_verified_images_is_not_success(self):
        bench = Bench(self.build, write=(0, MAC_OUTPUT))
        code, _, _ = self.flash(bench)
        self.assertEqual(code, 1)
        self.assertEqual(self.record()["status"], "flash_unverified")

    def test_source_moved_during_build_gets_no_write(self):
        states = [dict(IDENTITY), dict(IDENTITY, source_sha256={"src/x.cpp": "9" * 64})]
        bench = Bench(self.build)
        code, _, _ = self.flash(bench, identity=lambda: states.pop(0))
        self.assertEqual(code, 1)
        self.assertNotIn("read_mac", bench.names())
        self.assertNotIn("write_flash", bench.names())
        record = self.record()
        self.assertEqual(record["status"], "source_unstable")
        self.assertEqual(record["source_changed_during_build"], ["src/x.cpp"])


class PingCheck(Fixture):
    def capture(self, line):
        directory = self.base / "ping"
        directory.mkdir()
        (directory / "rx.bin").write_text(line, encoding="utf-8", newline="\n")
        (directory / "rx.txt").write_text(line, encoding="utf-8", newline="\n")
        (directory / "request.json").write_text(json.dumps(
            {"command": "ping", "status": "captured", "tx_hex": "70"}), encoding="utf-8")
        return directory

    def artifact(self, **fields):
        self.output.mkdir()
        (self.output / "build.json").write_text(json.dumps(dict({
            "base_commit": REVISION, "app_elf": flash.APP + ".elf", "target": "esp32s2", "sdk_version": "5.5.5",
            "image_sha256": {flash.APP + ".elf": "c" * 64}}, **fields)), encoding="utf-8")

    def pong(self, base=REVISION, elf="c" * 64, target="esp32s2"):
        return ("PONG target=%s base=%s elf=%s sdk=5.5.5 running=0 active=0 silent=1 pinsReady=1\n"
                % (target, base, elf))

    def check(self, directory):
        with mock.patch("sys.stdout", new_callable=io.StringIO) as out:
            code = flash.main(["--check-ping", str(directory), "--artifact", str(self.output)])
        return code, json.loads(out.getvalue())

    def test_running_image_matches(self):
        self.artifact()
        code, result = self.check(self.capture(self.pong()))
        self.assertEqual(code, 0, result)
        self.assertTrue(result["matches"])

    def test_retained_ping_against_its_retained_upload(self):
        self.output = RETAINED_UPLOAD
        code, result = self.check(ROOT / "docs/bench/20261102-final-ping")
        self.assertEqual(code, 0, result)

    def test_old_image_still_running_is_detected(self):
        self.artifact()
        code, result = self.check(self.capture(self.pong(elf="d" * 64)))
        self.assertEqual(code, 1)
        self.assertIn("elf", result["problems"][0])

    def test_no_reply_is_not_verification(self):
        self.artifact()
        code, result = self.check(self.capture(""))
        self.assertEqual(code, 1)
        self.assertFalse(result["matches"])


class ToolDiscovery(Fixture):
    def test_arguments_win_and_everything_is_named(self):
        args = argparse.Namespace(idf_path=self.idf, idf_python=self.python, idf_tools_path=self.tools_path)
        tools = flash.discover_tools(args, {"PATH": ""})
        self.assertEqual(tools["missing"], [])
        self.assertEqual(tools["paths"]["idf"], self.idf)
        self.assertEqual(tools["paths"]["compiler"].parent, self.idf.parent / "toolchain-xtensa-esp-elf/bin")
        environment = flash.tool_environment(tools)
        self.assertEqual(environment["IDF_COMPONENT_MANAGER"], "0", "a reproducible build downloads nothing")
        self.assertEqual(environment["IDF_TOOLS_PATH"], str(self.tools_path))

    def test_a_wrong_path_is_not_silently_replaced_by_a_guess(self):
        args = argparse.Namespace(idf_path=self.base / "absent", idf_python=self.python,
                                  idf_tools_path=self.tools_path)
        self.assertIn("idf", flash.discover_tools(args, {"PATH": ""})["missing"])
        args = argparse.Namespace(idf_path=None, idf_python=self.python, idf_tools_path=self.tools_path)
        self.assertIn("idf", flash.discover_tools(args, {"PATH": "", "IDF_PATH": str(self.base / "absent")})["missing"])

    def test_recommended_compiler_comes_from_the_sdk(self):
        self.assertEqual(flash.recommended_compiler(self.idf), "esp-fake")
        (self.idf / "tools/tools.json").write_text(json.dumps({"tools": []}), encoding="utf-8")
        with self.assertRaises(flash.FlashError):
            flash.recommended_compiler(self.idf)


if __name__ == "__main__":
    unittest.main()
