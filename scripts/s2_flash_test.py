#!/usr/bin/env python3
"""Exercise s2_flash.py with fakes: no process, port, esptool or flash is ever used."""

import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
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


SMOKE = "probe-idf-optak-smoke"
CONSOLE = "probe-idf-optak-console"
SMOKE_NAME = "motorcontrol_can_probe"
GOOD_ARGS = ("--flash_mode dio --flash_freq 80m --flash_size 4MB\n0x1000 bootloader/bootloader.bin\n"
             "0x10000 %s.bin\n0x8000 partition_table/partition-table.bin\n")


def resolved_sdkconfig(app):
    """A resolved sdkconfig as kconfgen writes it from the app's defaults, later files winning."""
    values = {}
    for path in flash.defaults_in_order(app):
        for line in path.read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if line.startswith("# CONFIG_") and line.endswith(" is not set"):
                values[line[2:-len(" is not set")]] = None
            elif line.startswith("CONFIG_") and "=" in line:
                name, value = line.split("=", 1)
                values[name] = value
    values.update(CONFIG_IDF_TARGET='"esp32s2"', CONFIG_ESPTOOLPY_FLASHSIZE='"4MB"')
    # Kconfig writes the bool's default when no defaults file sets it; the options that depend
    # on the hook then get no line (the resolved sdkconfig of the idf-s2-optak-console gate).
    values.setdefault("CONFIG_MOTORCONTROLCAN_SMOKE_HOOK", None)
    return "".join("# %s is not set\n" % name if value is None else "%s=%s\n" % (name, value)
                   for name, value in values.items())


def flasher_args(app, target="esp32s2", encrypted="false", **changes):
    """flasher_args.json in IDF 5.5.5's shape (see the retained upload's copy)."""
    layout = flash.flash_layout(app)
    data = {"write_flash_args": ["--flash_mode", "dio", "--flash_size", "4MB", "--flash_freq", "80m"],
            "flash_settings": {"flash_mode": "dio", "flash_size": "4MB", "flash_freq": "80m"},
            "flash_files": {"0x%x" % offset: name for offset, name in layout.items()},
            "extra_esptool_args": {"after": "hard_reset", "before": "default_reset", "stub": True, "chip": target}}
    for part, offset in flash.FLASHER_PARTS.items():
        data[part] = {"offset": "0x%x" % offset, "file": layout[offset], "encrypted": encrypted}
    data.update(changes)
    return data


def write_build(build, *, app="s2-bench", target="esp32s2", chip=2, sdkconfig=None, flash_args=None,
                encrypted="false", revision=REVISION, description=None, flasher=None, inputs=None,
                compiler="C:/pio/xtensa-esp32s2-elf-g++.exe", version=None):
    """Generated outputs in the exact shape of a real build directory of the selected app."""
    build = Path(build)
    name = flash.APPS[app]["name"]
    (build / "bootloader").mkdir(parents=True, exist_ok=True)
    (build / "partition_table").mkdir(exist_ok=True)
    elf = build / (name + ".elf")
    elf.write_bytes(b"\x7fELF fake " + name.encode("ascii"))
    header = bytes([0xE9, 6, 2, 0x2F]) + bytes(8) + chip.to_bytes(2, "little") + bytes(2)
    application = bytearray(header + bytes(0x200))
    application[flash.ELF_HASH_OFFSET:flash.ELF_HASH_OFFSET + 32] = hashlib.sha256(elf.read_bytes()).digest()
    (build / (name + ".bin")).write_bytes(application)
    (build / "bootloader/bootloader.bin").write_bytes(header + bytes(32))
    (build / "partition_table/partition-table.bin").write_bytes(bytes.fromhex("aa500102") + bytes(3068))
    (build / "flash_args").write_text(flash_args if flash_args is not None else GOOD_ARGS % name, encoding="utf-8")
    (build / "flasher_args.json").write_text(json.dumps(flasher if flasher is not None else flasher_args(
        app, target, encrypted)), encoding="utf-8")
    project = flash.project_dir(app)
    smoke = bool(flash.APPS[app]["gate"])
    (build / "project_description.json").write_text(json.dumps(dict({
        "target": target, "git_revision": "5.5.5", "c_compiler": "fake",
        "project_version": version if version is not None else revision[:7] + "-dirty" if smoke else "fake",
        "project_name": name, "project_path": project.as_posix(), "app_elf": name + ".elf", "app_bin": name + ".bin",
        "config_defaults": ";".join(path.as_posix() for path in flash.applied_defaults(app))},
        **(description or {}))), encoding="utf-8")
    if sdkconfig is None and smoke:
        sdkconfig = resolved_sdkconfig(app)
    elif sdkconfig is None:
        defaults = (flash.PROJECT / "sdkconfig.defaults").read_text(encoding="utf-8")
        sdkconfig = "\n".join(line for line in defaults.splitlines() if line.startswith("CONFIG_")) + "\n"
    (build / "sdkconfig").write_text(sdkconfig, encoding="utf-8")
    if smoke:
        files = inputs if inputs is not None else flash.smoke_inputs(app)[0]
        rows = [{"directory": build.as_posix(), "file": Path(path).as_posix(),
                 "command": "%s -c %s" % (compiler, Path(path).as_posix())} for path in files]
    else:
        rows = [{"file": "C:/x/examples/s2_bench/main/main.cpp",
                 "command": 'g++ -DMOTORCONTROLCAN_SOURCE_REVISION=\\"%s\\" -c main.cpp' % revision}]
    (build / "compile_commands.json").write_text(json.dumps(rows), encoding="utf-8")


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


TOOLS = {"paths": {"idf_python": "python", "idf": "idf"}}


class Selection(unittest.TestCase):
    """--app names one reviewed application; the default is the bench exactly as before."""

    def test_default_is_the_bench_with_the_reviewed_command(self):
        for argv in (["--build-only"], ["--build-only", "--app", "s2-bench"]):
            args = flash.parse_args(argv)
            self.assertEqual(args.app, "s2-bench")
            self.assertEqual(args.build_dir, ROOT / ".build/s2-bench-live")
        # The command this tool ran before --app existed, word for word.
        self.assertEqual(flash.build_command(TOOLS, "b"), [
            "python", str(Path("idf") / "tools/idf.py"), "-C", str(ROOT / "examples/s2_bench"), "-B", "b",
            "-DIDF_TARGET=esp32s2", "-DSDKCONFIG=b/sdkconfig", "reconfigure", "build"])
        self.assertEqual(flash.build_command(TOOLS, "b", "s2-bench"), flash.build_command(TOOLS, "b"))
        self.assertEqual((flash.APP, flash.PROJECT), ("motorcontrol_can_s2_bench", ROOT / "examples/s2_bench"))
        self.assertEqual(flash.FLASH_LAYOUT, {0x1000: "bootloader/bootloader.bin",
                                              0x8000: "partition_table/partition-table.bin",
                                              0x10000: "motorcontrol_can_s2_bench.bin"})
        self.assertEqual(flash.default_artifact("flash").name[-10:], "-s2-upload")
        self.assertEqual(flash.default_artifact("build").name[-9:], "-s2-build")

    def test_smoke_command_is_the_gate_configuration(self):
        args = flash.parse_args(["--build-only", "--app", SMOKE])
        self.assertEqual(args.build_dir, ROOT / ".build/s2-smoke-live", "never the bench's build directory")
        command = flash.build_command(TOOLS, "b", SMOKE)
        project = ROOT / "examples/probe_idf"
        self.assertEqual(command[2:4], ["-C", str(project)])
        # The value check_firmware.shared_idf_build passes for idf-s2-optak.
        gate = flash.SHARED_IDF_BUILDS["idf-s2-optak"]
        self.assertIn("-DSDKCONFIG_DEFAULTS=" + ";".join((project / item).as_posix() for item in gate["defaults"]),
                      command)
        self.assertEqual(gate["defaults"], ("sdkconfig.defaults", "sdkconfig.optak_s2", "sdkconfig.optak_s2_smoke"))
        self.assertEqual(command[-2:], ["reconfigure", "build"])
        self.assertTrue(any(word == "-DIDF_TARGET=esp32s2" for word in command))
        self.assertNotIn("s2_bench", " ".join(command))
        self.assertEqual(flash.flash_layout(SMOKE), {0x1000: "bootloader/bootloader.bin",
                                                     0x8000: "partition_table/partition-table.bin",
                                                     0x10000: "motorcontrol_can_probe.bin"})
        self.assertEqual([path.name for path in flash.defaults_in_order(SMOKE)], [
            "sdkconfig.defaults", "sdkconfig.defaults.esp32s2", "sdkconfig.optak_s2", "sdkconfig.optak_s2_smoke"])
        self.assertEqual([path.name for path in flash.defaults_in_order("s2-bench")], ["sdkconfig.defaults"])
        self.assertTrue(flash.default_artifact("flash", SMOKE).name.endswith("-s2-smoke-upload"))

    def test_unknown_app_and_ping_of_the_smoke_image_are_refused(self):
        with mock.patch("sys.stderr", new_callable=io.StringIO):
            for argv in (["--build-only", "--app", "s2"], ["--build-only", "--app", "probe-idf"],
                         ["--check-ping", "x", "--artifact", "y", "--app", SMOKE]):
                with self.assertRaises(SystemExit, msg=str(argv)):
                    flash.parse_args(argv)

    def test_later_defaults_win_as_in_idf(self):
        sdkconfig = 'CONFIG_IDF_TARGET="esp32s2"\n'
        self.assertEqual(flash.check_sdkconfig("CONFIG_A=y\n# CONFIG_A is not set\n", sdkconfig), [])
        self.assertEqual(flash.check_sdkconfig("# CONFIG_A is not set\nCONFIG_A=y\n", sdkconfig), ["CONFIG_A=None, defaults y"])
        self.assertEqual(flash.check_sdkconfig("CONFIG_A=1\nCONFIG_A=2\n", sdkconfig + "CONFIG_A=2\n"), [])

    def test_version_revision(self):
        self.assertEqual(flash.version_revision("311b682-dirty"), "311b682")
        self.assertEqual(flash.version_revision("56e12ae"), "56e12ae")
        self.assertEqual(flash.version_revision("v1.0-3-g311b682-dirty"), "311b682")
        for text in ("1", "v1.0", "", None, "HEAD-HASH-NOTFOUND", "311b6"):
            self.assertIsNone(flash.version_revision(text), text)

    def test_real_retained_flasher_args_agree_with_flash_args(self):
        retained = ROOT / "docs/bench/20261106T1309Z-s2-upload"
        options, images = flash.parse_flash_args((retained / "flash_args").read_text(encoding="utf-8"))
        flash.check_flasher_args(json.loads((retained / "flasher_args.json").read_text(encoding="utf-8")),
                                 options, images)


class SmokeValidation(Fixture):
    """The selected smoke image, and refusal of every artifact that is not exactly it."""

    def refused(self, pattern=None, app=SMOKE, **options):
        if self.build.exists():
            shutil.rmtree(self.build)
        write_build(self.build, app=SMOKE, **options)
        with self.assertRaises(flash.FlashError, msg=str(options)) as caught:
            flash.validate_build(self.build, app)
        if pattern:
            self.assertRegex(str(caught.exception), pattern)
        # parse_flash_args keeps its original "refused" status; every other check is build_invalid.
        self.assertIn(caught.exception.status, ("build_invalid", "refused"))

    def test_valid_smoke_build(self):
        write_build(self.build, app=SMOKE)
        facts = flash.validate_build(self.build, SMOKE)
        self.assertEqual((facts["project_name"], facts["app_elf"], facts["app_bin"]),
                         (SMOKE_NAME, SMOKE_NAME + ".elf", SMOKE_NAME + ".bin"))
        self.assertEqual(facts["project_path"], "examples/probe_idf")
        self.assertEqual(facts["config_defaults"], ["examples/probe_idf/sdkconfig.defaults",
                                                    "examples/probe_idf/sdkconfig.optak_s2",
                                                    "examples/probe_idf/sdkconfig.optak_s2_smoke"])
        self.assertEqual(set(facts["image_sha256"]), {
            "bootloader/bootloader.bin", "partition_table/partition-table.bin", SMOKE_NAME + ".bin",
            SMOKE_NAME + ".elf", "sdkconfig"})
        for check in ("project", "config_defaults", "flash_args", "flasher_args", "single_elf", "elf_hash_embedded",
                      "sdkconfig_defaults", "gate_sdkconfig", "compiler_inputs", "compiler"):
            self.assertIn(check, facts["app_checks"])
        self.assertEqual(facts["compile_entries"], len(flash.smoke_inputs(SMOKE)[0]))
        values = facts["gate_sdkconfig"]
        for name, value in (("CONFIG_IDF_TARGET", '"esp32s2"'), ("CONFIG_MOTORCONTROLCAN_CAN_TX_PIN", "17"),
                            ("CONFIG_MOTORCONTROLCAN_CAN_RX_PIN", "16"), ("CONFIG_MOTORCONTROLCAN_CAN_SILENT_PIN", "18"),
                            ("CONFIG_MOTORCONTROLCAN_CAN_SILENT_LEVEL", "1"), ("CONFIG_MOTORCONTROLCAN_SMOKE_HOOK", "y"),
                            ("CONFIG_ESP_CONSOLE_NONE", "y"), ("CONFIG_MOTORCONTROLCAN_COMPILE_FIXTURE", None)):
            self.assertEqual(values[name], value, name)

    def test_wrong_project(self):
        self.refused("project", description={"project_name": flash.APP})
        self.refused("path", description={"project_path": (ROOT / "examples/s2_bench").as_posix()})
        self.refused("images", description={"app_bin": flash.APP + ".bin"})
        # The bench's real outputs are not this app, nor the smoke outputs the bench.
        shutil.rmtree(self.build)
        write_build(self.build)
        with self.assertRaisesRegex(flash.FlashError, "project"):
            flash.validate_build(self.build, SMOKE)
        shutil.rmtree(self.build)
        write_build(self.build, app=SMOKE)
        with self.assertRaisesRegex(flash.FlashError, "project"):
            flash.validate_build(self.build)

    def test_other_defaults_applied(self):
        project = ROOT / "examples/probe_idf"
        self.refused("defaults", description={"config_defaults": (project / "sdkconfig.defaults").as_posix()})
        self.refused("defaults", description={"config_defaults": ";".join(
            (project / name).as_posix() for name in ("sdkconfig.defaults", "sdkconfig.optak_s2"))})
        self.refused("defaults", description={"config_defaults": ""})

    def test_wrong_target(self):
        self.refused("target", target="esp32s3")
        self.refused(None, chip=9)
        self.refused("CONFIG_IDF_TARGET", sdkconfig=resolved_sdkconfig(SMOKE).replace('"esp32s2"', '"esp32s3"'))

    def test_missing_or_changed_board_and_smoke_values(self):
        good = resolved_sdkconfig(SMOKE)
        for old, new in (("CONFIG_MOTORCONTROLCAN_SMOKE_HOOK=y\n", ""),
                         ("CONFIG_MOTORCONTROLCAN_SMOKE_HOOK=y\n", "# CONFIG_MOTORCONTROLCAN_SMOKE_HOOK is not set\n"),
                         ("CONFIG_MOTORCONTROLCAN_SMOKE_NODE_B=2\n", "CONFIG_MOTORCONTROLCAN_SMOKE_NODE_B=3\n"),
                         ("CONFIG_MOTORCONTROLCAN_SMOKE_IDENTITY_U8=y\n", ""),
                         ("CONFIG_MOTORCONTROLCAN_CAN_TX_PIN=17\n", "CONFIG_MOTORCONTROLCAN_CAN_TX_PIN=16\n"),
                         ("CONFIG_MOTORCONTROLCAN_CAN_SILENT_LEVEL=1\n", "CONFIG_MOTORCONTROLCAN_CAN_SILENT_LEVEL=0\n"),
                         ("CONFIG_ESP_CONSOLE_NONE=y\n", "CONFIG_ESP_CONSOLE_UART_DEFAULT=y\n"),
                         ("# CONFIG_MOTORCONTROLCAN_COMPILE_FIXTURE is not set\n",
                          "CONFIG_MOTORCONTROLCAN_COMPILE_FIXTURE=y\n")):
            self.assertIn(old, good)
            self.refused("sdkconfig|smoke", sdkconfig=good.replace(old, new))
        # The gate's resolved values are required even where the defaults files say nothing.
        self.refused("CONFIG_ESPTOOLPY_FLASHSIZE", sdkconfig=good.replace('CONFIG_ESPTOOLPY_FLASHSIZE="4MB"\n', ""))

    def test_smoke_source_must_be_compiled_and_bench_sources_must_not(self):
        expected = flash.smoke_inputs(SMOKE)[0]
        smoke = ROOT / "examples/common/FirmwareSmoke.cpp"
        self.assertIn(smoke, expected)
        self.refused("omitted", inputs=[path for path in expected if path != smoke])
        self.refused("omitted", inputs=[path for path in expected if path.name != "main.cpp"])
        for bench in (ROOT / "examples/s2_bench/main/main.cpp", ROOT / "examples/s2_bench/RawEvidence.cpp"):
            self.refused("forbidden", inputs=expected + [bench])
        self.refused("compiled by", compiler="C:/pio/xtensa-esp32s3-elf-g++.exe")
        self.refused("compiled by", compiler="g++")

    def test_wrong_offsets_or_flash_settings(self):
        args = GOOD_ARGS % SMOKE_NAME
        self.refused("layout", flash_args=args.replace("0x10000", "0x20000"))
        self.refused("layout", flash_args=args.replace("0x8000", "0x9000"))
        self.refused("layout", flash_args=args.replace(SMOKE_NAME + ".bin", flash.APP + ".bin"))
        self.refused(None, flash_args=args.replace("dio", "qio"))
        # flasher_args.json is the project's own statement: it must agree with flash_args.
        files = {"0x1000": "bootloader/bootloader.bin", "0x8000": "partition_table/partition-table.bin",
                 "0x20000": SMOKE_NAME + ".bin"}
        self.refused("flash_files", flasher=flasher_args(SMOKE, flash_files=files))
        self.refused("flash_settings", flasher=flasher_args(
            SMOKE, flash_settings={"flash_mode": "qio", "flash_size": "4MB", "flash_freq": "80m"}))
        self.refused("write_flash_args", flasher=flasher_args(
            SMOKE, write_flash_args=["--flash_mode", "dio", "--flash_size", "8MB", "--flash_freq", "80m"]))
        self.refused("app is", flasher=dict(flasher_args(SMOKE), app={
            "offset": "0x20000", "file": SMOKE_NAME + ".bin", "encrypted": "false"}))
        self.refused("partition-table", flasher=dict(flasher_args(SMOKE), **{"partition-table": {"encrypted": "false"}}))
        self.refused("encryption", encrypted="true")

    def test_another_elf_in_the_build_directory(self):
        write_build(self.build, app=SMOKE)
        (self.build / (flash.APP + ".elf")).write_bytes(b"\x7fELF bench")
        with self.assertRaisesRegex(flash.FlashError, "ELF files"):
            flash.validate_build(self.build, SMOKE)

    def test_image_must_embed_this_elf(self):
        write_build(self.build, app=SMOKE)
        (self.build / (SMOKE_NAME + ".elf")).write_bytes(b"different")
        with self.assertRaisesRegex(flash.FlashError, "embed"):
            flash.validate_build(self.build, SMOKE)


class SmokeUpload(Fixture):
    def smoke(self, bench, *mode, **options):
        return self.run_mode(bench, *mode, "--app", SMOKE, **options)

    def test_full_sequence_and_retained_record(self):
        bench = Bench(self.build, app=SMOKE)
        code, output, _ = self.smoke(bench, "--flash", "--port", PORT, "--serial", SERIAL)
        self.assertEqual(code, 0, output)
        names = bench.names()
        self.assertEqual(names[names.index("build"):], ["build", "read_mac", "write_flash"])
        self.assertEqual(names.count("write_flash"), 1)
        build = dict(bench.commands)["build"]
        self.assertEqual(build[build.index("-C") + 1], str(ROOT / "examples/probe_idf"))
        self.assertTrue(any(word.startswith("-DSDKCONFIG_DEFAULTS=") for word in build))
        write = dict(bench.commands)["write_flash"]
        self.assertEqual(write[-2:], ["write_flash", "@flash_args"])
        self.assertEqual(write[write.index("--chip") + 1], "esp32s2")
        record = self.record()
        self.assertEqual(record["status"], "flashed")
        self.assertEqual(record["app"]["name"], SMOKE)
        self.assertEqual(record["app"]["gate"], "idf-s2-optak")
        self.assertEqual(record["app"]["flash_layout"], {"0x1000": "bootloader/bootloader.bin",
                                                         "0x8000": "partition_table/partition-table.bin",
                                                         "0x10000": SMOKE_NAME + ".bin"})
        self.assertEqual(record["app_elf"], SMOKE_NAME + ".elf")
        elf = hashlib.sha256((self.build / (SMOKE_NAME + ".elf")).read_bytes()).hexdigest()
        self.assertEqual(record["image_sha256"][SMOKE_NAME + ".elf"], elf)
        self.assertIn("compiler_inputs", record["app_checks"])
        self.assertEqual(record["revision_check"]["embedded"], REVISION[:7] + "-dirty")
        self.assertNotIn("compiled_revision", record, "no 40-digit revision is compiled into this image")
        self.assertEqual(record["flash"]["verified_images"], 3)
        self.assertEqual((record["erase_all"], record["efuse"], record["retry"]), (False, False, False))
        self.assertEqual(record["usb"]["usb_serial"], SERIAL)
        self.assertEqual(record["target_identity"]["mac"], flash.FIXTURE["mac"])
        for name in ("sdkconfig", "flash_args", "flasher_args.json", "project_description.json", "flash.log"):
            self.assertTrue((self.output / name).is_file(), name)
        self.assertIn("s2_smoke.py", output)
        self.assertNotIn("--check-ping", output)

    def test_stale_project_version_gets_no_write(self):
        for version in ("bbbbbbb-dirty", "1", "v1.0"):
            if self.output.exists():
                shutil.rmtree(self.output)
            bench = Bench(self.build, app=SMOKE, version=version)
            code, _, _ = self.smoke(bench, "--flash", "--port", PORT, "--serial", SERIAL)
            self.assertEqual(code, 1, version)
            self.assertNotIn("write_flash", bench.names())
            self.assertEqual(self.record()["status"], "build_invalid")

    def test_invalid_outputs_get_no_write(self):
        cases = ({"inputs": flash.smoke_inputs(SMOKE)[0] + [ROOT / "examples/s2_bench/main/main.cpp"]},
                 {"sdkconfig": resolved_sdkconfig(SMOKE).replace("CONFIG_MOTORCONTROLCAN_SMOKE_HOOK=y\n", "")},
                 {"description": {"project_name": flash.APP}})
        for options in cases:
            for path in (self.output, self.build):
                if path.exists():
                    shutil.rmtree(path)
            bench = Bench(self.build, app=SMOKE, **options)
            code, _, _ = self.smoke(bench, "--flash", "--port", PORT, "--serial", SERIAL)
            self.assertEqual(code, 1, options)
            self.assertNotIn("read_mac", bench.names())
            self.assertNotIn("write_flash", bench.names())
            self.assertEqual(self.record()["status"], "build_invalid")

    def test_bench_build_directory_is_refused_before_anything(self):
        write_build(self.build)  # A configured bench directory: reusing it would build the bench.
        bench = Bench(self.build, app=SMOKE)
        code, output, usb = self.smoke(bench, "--flash", "--port", PORT, "--serial", SERIAL)
        self.assertEqual(code, 2)
        self.assertEqual(bench.commands, [])
        self.assertFalse(self.output.exists())
        usb.assert_not_called()
        self.assertIn("another --build-dir", output)
        # And the reverse: the bench is never built in the smoke image's directory.
        shutil.rmtree(self.build)
        write_build(self.build, app=SMOKE)
        bench = Bench(self.build)
        code, _, _ = self.run_mode(bench, "--build-only")
        self.assertEqual(code, 2)
        self.assertEqual(bench.commands, [])

    def test_build_only_touches_no_port(self):
        bench = Bench(self.build, app=SMOKE)
        code, output, usb = self.smoke(bench, "--build-only")
        self.assertEqual(code, 0, output)
        usb.assert_not_called()
        self.assertEqual([name for name in bench.names() if name != "version"], ["build"])
        self.assertEqual(self.record()["status"], "built")
        self.assertEqual(self.record()["app"]["name"], SMOKE)

    def test_bench_record_names_its_selection(self):
        bench = Bench(self.build)
        code, output, _ = self.flash(bench)
        self.assertEqual(code, 0, output)
        record = self.record()
        self.assertEqual(record["app"]["name"], "s2-bench")
        self.assertEqual(record["compiled_revision"], REVISION)
        self.assertEqual(record["revision_check"]["source"], "MOTORCONTROLCAN_SOURCE_REVISION")
        build = dict(bench.commands)["build"]
        self.assertFalse(any(word.startswith("-DSDKCONFIG_DEFAULTS=") for word in build))
        self.assertNotIn("s2_smoke.py", output)

    def test_ping_check_refuses_a_smoke_record(self):
        self.output.mkdir()
        (self.output / "build.json").write_text(json.dumps({"app": {"name": SMOKE}}), encoding="utf-8")
        with mock.patch("sys.stdout", new_callable=io.StringIO) as out:
            code = flash.main(["--check-ping", str(self.base / "absent"), "--artifact", str(self.output)])
        self.assertEqual(code, 1)
        self.assertIn("s2_smoke.py", json.loads(out.getvalue())["problems"][0])


class SmokeDryRun(DryRun):
    def dry(self):
        blocked = mock.Mock(side_effect=AssertionError("dry run touched hardware or a process"))
        with mock.patch.object(subprocess, "run", blocked), mock.patch.object(subprocess, "Popen", blocked), \
                mock.patch.object(flash, "execute", blocked), mock.patch.object(flash, "comports", blocked), \
                mock.patch.object(flash, "sdk_environment", blocked), mock.patch.object(flash, "Runner", blocked), \
                mock.patch.object(flash, "source_identity", blocked), \
                mock.patch.dict(sys.modules, {"serial": None, "serial.tools": None}), \
                mock.patch("sys.stdout", new_callable=io.StringIO) as out:
            code = flash.main(self.arguments("--dry-run", "--app", SMOKE))
        return code, out.getvalue()

    def test_prints_the_plan_and_touches_nothing(self):
        code, output = self.dry()
        self.assertEqual(code, 0)
        self.assertFalse(self.output.exists())
        self.assertFalse(self.build.exists())
        for text in ("probe-idf-optak-smoke", "examples/probe_idf", "motorcontrol_can_probe", "idf-s2-optak",
                     "-DSDKCONFIG_DEFAULTS=", "sdkconfig.optak_s2_smoke", "sdkconfig.defaults.esp32s2",
                     "0x1000 bootloader/bootloader.bin", "0x8000 partition_table/partition-table.bin",
                     "0x10000 motorcontrol_can_probe.bin", "CONFIG_MOTORCONTROLCAN_SMOKE_HOOK=y",
                     "write_flash @flash_args", "read_mac", "s2_smoke.py", "VERDICT: INCOMPLETE"):
            self.assertIn(text, output)
        for text in ("erase_flash", "burn_efuse", "espefuse", "PASS", "--check-ping", "s2_bench.py"):
            self.assertNotIn(text, output)

    def test_validates_an_existing_build_without_touching_it(self):
        write_build(self.build, app=SMOKE)
        _, output = self.dry()
        self.assertIn("is valid for this plan", output)
        write_build(self.build, app=SMOKE, chip=9)
        _, output = self.dry()
        self.assertIn("would be refused", output)

    def test_reports_another_projects_directory(self):
        write_build(self.build)
        _, output = self.dry()
        self.assertIn("Refused before building", output)
        self.assertIn("would be refused", output)


class ConsoleImage(Fixture):
    """probe-idf-optak-console: the OPTAK board configuration without the smoke hook."""

    def console(self, bench, *mode, **options):
        return self.run_mode(bench, *mode, "--app", CONSOLE, **options)

    def refused(self, pattern, **options):
        if self.build.exists():
            shutil.rmtree(self.build)
        write_build(self.build, app=CONSOLE, **options)
        with self.assertRaisesRegex(flash.FlashError, pattern, msg=str(options)):
            flash.validate_build(self.build, CONSOLE)

    def test_command_and_selection_are_the_gate_configuration(self):
        args = flash.parse_args(["--build-only", "--app", CONSOLE])
        self.assertEqual(args.build_dir, ROOT / ".build/s2-console-live")
        command = flash.build_command(TOOLS, "b", CONSOLE)
        project = ROOT / "examples/probe_idf"
        gate = flash.SHARED_IDF_BUILDS["idf-s2-optak-console"]
        self.assertEqual(gate["defaults"], ("sdkconfig.defaults", "sdkconfig.optak_s2"))
        self.assertIn("-DSDKCONFIG_DEFAULTS=" + ";".join((project / item).as_posix() for item in gate["defaults"]),
                      command)
        self.assertNotIn("smoke", " ".join(command))
        self.assertEqual([path.name for path in flash.defaults_in_order(CONSOLE)], [
            "sdkconfig.defaults", "sdkconfig.defaults.esp32s2", "sdkconfig.optak_s2"])
        self.assertEqual(flash.flash_layout(CONSOLE), flash.flash_layout(SMOKE))
        self.assertTrue(flash.default_artifact("flash", CONSOLE).name.endswith("-s2-console-upload"))
        self.assertEqual(flash.app_record(CONSOLE)["verify"], "s2_console.py")
        line, = flash.verify_instructions(CONSOLE, PORT, SERIAL, "A")
        self.assertIn("s2_console.py", line)
        self.assertIn("--reset-en", line)
        self.assertIn("console_scripts/local_routes.txt", line)
        with mock.patch("sys.stderr", new_callable=io.StringIO) as err, self.assertRaises(SystemExit):
            flash.parse_args(["--check-ping", "x", "--artifact", "y", "--app", CONSOLE])
        self.assertIn("s2_console.py", err.getvalue())

    def test_valid_console_build(self):
        write_build(self.build, app=CONSOLE)
        facts = flash.validate_build(self.build, CONSOLE)
        self.assertEqual(facts["config_defaults"], ["examples/probe_idf/sdkconfig.defaults",
                                                    "examples/probe_idf/sdkconfig.optak_s2"])
        expected, forbidden = flash.smoke_inputs(CONSOLE)
        self.assertNotIn(ROOT / "examples/common/FirmwareSmoke.cpp", expected)
        self.assertIn(ROOT / "examples/common/FirmwareSmoke.cpp", forbidden)
        self.assertEqual(facts["compile_entries"], len(expected))
        values = facts["gate_sdkconfig"]
        self.assertIsNone(values["CONFIG_MOTORCONTROLCAN_SMOKE_HOOK"])
        for name in ("CONFIG_MOTORCONTROLCAN_SMOKE_NODE_A", "CONFIG_MOTORCONTROLCAN_SMOKE_NODE_B",
                     "CONFIG_MOTORCONTROLCAN_SMOKE_IDENTITY_U8"):
            self.assertEqual(values[name], "<undeclared>", name)
        for name, value in (("CONFIG_MOTORCONTROLCAN_CAN_TX_PIN", "17"), ("CONFIG_MOTORCONTROLCAN_CAN_RX_PIN", "16"),
                            ("CONFIG_MOTORCONTROLCAN_CAN_BITRATE", "1000000"),
                            ("CONFIG_MOTORCONTROLCAN_CAN_SILENT_PIN", "18"),
                            ("CONFIG_MOTORCONTROLCAN_CAN_SILENT_LEVEL", "1"),
                            ("CONFIG_MOTORCONTROLCAN_COMPILE_FIXTURE", None), ("CONFIG_ESP_CONSOLE_NONE", "y")):
            self.assertEqual(values[name], value, name)

    def test_smoke_values_or_source_and_board_changes_are_refused(self):
        good = resolved_sdkconfig(CONSOLE)
        hook = "# CONFIG_MOTORCONTROLCAN_SMOKE_HOOK is not set\n"
        self.assertIn(hook, good)
        for sdkconfig in (good.replace(hook, "CONFIG_MOTORCONTROLCAN_SMOKE_HOOK=y\n"), good.replace(hook, ""),
                          good + "CONFIG_MOTORCONTROLCAN_SMOKE_NODE_A=1\n",
                          good.replace("CONFIG_MOTORCONTROLCAN_CAN_TX_PIN=17\n", "CONFIG_MOTORCONTROLCAN_CAN_TX_PIN=16\n"),
                          good.replace("# CONFIG_MOTORCONTROLCAN_COMPILE_FIXTURE is not set\n",
                                       "CONFIG_MOTORCONTROLCAN_COMPILE_FIXTURE=y\n")):
            self.refused("sdkconfig", sdkconfig=sdkconfig)
        expected = flash.smoke_inputs(CONSOLE)[0]
        self.refused("forbidden", inputs=expected + [ROOT / "examples/common/FirmwareSmoke.cpp"])
        self.refused("forbidden", inputs=expected + [ROOT / "examples/s2_bench/main/main.cpp"])
        self.refused("omitted", inputs=[path for path in expected if path.name != "main.cpp"])
        # The smoke image's outputs are not this image: other defaults applied.
        self.refused("defaults", description={"config_defaults": ";".join(
            path.as_posix() for path in flash.applied_defaults(SMOKE))})

    def test_full_sequence_and_retained_record(self):
        bench = Bench(self.build, app=CONSOLE)
        code, output, _ = self.console(bench, "--flash", "--port", PORT, "--serial", SERIAL)
        self.assertEqual(code, 0, output)
        self.assertEqual(bench.names().count("write_flash"), 1)
        record = self.record()
        self.assertEqual((record["status"], record["app"]["name"], record["app"]["gate"]),
                         ("flashed", CONSOLE, "idf-s2-optak-console"))
        self.assertEqual(record["app"]["verify"], "s2_console.py")
        self.assertIn("s2_console.py", output)
        self.assertNotIn("s2_smoke.py", output)

    def test_a_smoke_build_directory_is_refused_before_anything(self):
        write_build(self.build, app=SMOKE)  # Same project; its sdkconfig would keep the smoke hook.
        bench = Bench(self.build, app=CONSOLE)
        code, output, usb = self.console(bench, "--flash", "--port", PORT, "--serial", SERIAL)
        self.assertEqual(code, 2)
        self.assertEqual(bench.commands, [])
        self.assertFalse(self.output.exists())
        usb.assert_not_called()
        self.assertIn("configured with defaults", output)
        # And the reverse.
        shutil.rmtree(self.build)
        write_build(self.build, app=CONSOLE)
        bench = Bench(self.build, app=SMOKE)
        code, output, _ = self.run_mode(bench, "--build-only", "--app", SMOKE)
        self.assertEqual(code, 2)
        self.assertEqual(bench.commands, [])

    def test_dry_run_plan(self):
        blocked = mock.Mock(side_effect=AssertionError("dry run touched hardware or a process"))
        with mock.patch.object(subprocess, "run", blocked), mock.patch.object(flash, "execute", blocked), \
                mock.patch.object(flash, "comports", blocked), mock.patch.object(flash, "Runner", blocked), \
                mock.patch("sys.stdout", new_callable=io.StringIO) as out:
            code = flash.main(self.arguments("--dry-run", "--app", CONSOLE))
        self.assertEqual(code, 0)
        output = out.getvalue()
        for text in (CONSOLE, "idf-s2-optak-console", "sdkconfig.optak_s2", "s2_console.py",
                     "CONFIG_MOTORCONTROLCAN_SMOKE_HOOK=<not set>", "CONFIG_MOTORCONTROLCAN_SMOKE_NODE_A=<undeclared>",
                     "nothing from examples/s2_bench, examples/common/FirmwareSmoke.cpp", "VERDICT: INCOMPLETE"):
            self.assertIn(text, output)
        self.assertNotIn("sdkconfig.optak_s2_smoke", output)
        self.assertNotIn("s2_smoke.py", output)

    def test_ping_check_names_the_console_runner(self):
        self.output.mkdir()
        (self.output / "build.json").write_text(json.dumps({"app": {"name": CONSOLE}}), encoding="utf-8")
        with mock.patch("sys.stdout", new_callable=io.StringIO) as out:
            code = flash.main(["--check-ping", str(self.base / "absent"), "--artifact", str(self.output)])
        self.assertEqual(code, 1)
        self.assertIn("s2_console.py", json.loads(out.getvalue())["problems"][0])


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
