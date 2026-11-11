#!/usr/bin/env python3
"""Reproducible build and upload of one reviewed S2 application, with a retained record.

Exactly one mode is required, so nothing flashes by default:

  --dry-run           Print the plan from files only. Runs no process and neither
                      enumerates nor opens a port. It is not a build or an upload.
  --build-only        Build and validate; never touches a port.
  --flash             Build, validate, check USB and target identity, write once.
  --check-ping DIR    Offline: compare an s2_bench.py ping capture with --artifact
                      (s2-bench records only).

--app selects one reviewed application; nothing else can be built or written:

  s2-bench               examples/s2_bench, the special bench image. The default: the
                         build command, write and ping check it had before --app, plus
                         the identity checks below.
  probe-idf-optak-smoke  examples/probe_idf, the shared image with the OPTAK S2
                         board configuration and the test-only smoke hook, configured
                         exactly as check_firmware.py --target idf-s2-optak:
                         sdkconfig.defaults (and its .esp32s2 file), sdkconfig.optak_s2,
                         sdkconfig.optak_s2_smoke. Verify it afterwards with s2_smoke.py.
  probe-idf-optak-console
                         examples/probe_idf with the same OPTAK S2 board configuration
                         and no smoke hook, so UART0 carries the shared console. Configured
                         exactly as check_firmware.py --target idf-s2-optak-console:
                         sdkconfig.defaults (and its .esp32s2 file), sdkconfig.optak_s2.
                         Exercise it afterwards with s2_console.py.

Before any write the outputs must be the selected application: project name and path,
the defaults files IDF applied, target, application ELF/BIN names and the only ELF in
the build directory, the reviewed layout (bootloader 0x1000, partition table 0x8000,
application 0x10000, dio/80m/4MB) in both flash_args and flasher_args.json, chip IDs,
the ELF hash inside the image and sdkconfig without drift from those defaults. A shared
image must also carry every value its gate requires (the board values; the smoke values,
or for the console image the smoke hook not set and its options absent), have compiled
exactly the gate's inputs (FirmwareSmoke.cpp only for the smoke image) with the
xtensa-esp32s2 compiler and nothing from examples/s2_bench, and embed HEAD in its
project version.

The recorded fixture is checked, never assumed. A failed step is retained and is
not retried. This tool never erases all flash, changes eFuses, downloads a vendor
executable or upgrades a toolchain, and it does not make the running image a
qualified one: verify it afterwards with s2_bench.py ping and --check-ping, for the
smoke image with s2_smoke.py, and for the console image with s2_console.py.
"""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

from check_firmware import (BENCH_PROJECT, SHARED_IDF, SHARED_IDF_BUILDS, check_compiler, check_inputs,
                            firmware_inventory, sdk_environment)
from check_firmware import check_sdkconfig as check_resolved_sdkconfig
from s2_evidence import check_image, replay_capture
from verify import Runner, VerificationFailure


ROOT = Path(__file__).resolve().parents[1]
TARGET = "esp32s2"
# The reviewed applications. "defaults" None keeps IDF's own lookup of the project's
# sdkconfig.defaults and the bench build command exactly as it was; "gate" names the
# check_firmware.py build whose configuration, inputs and values an image must match;
# "verify" names the separate runner that checks the running image.
APPS = {
    "s2-bench": {"project": BENCH_PROJECT, "name": "motorcontrol_can_s2_bench", "defaults": None,
                 "gate": None, "build_dir": ".build/s2-bench-live", "label": "s2",
                 "verify": "s2_bench.py ping + s2_flash.py --check-ping"},
    "probe-idf-optak-smoke": {"project": SHARED_IDF, "name": "motorcontrol_can_probe",
                              "defaults": SHARED_IDF_BUILDS["idf-s2-optak"]["defaults"], "gate": "idf-s2-optak",
                              "build_dir": ".build/s2-smoke-live", "label": "s2-smoke", "verify": "s2_smoke.py"},
    "probe-idf-optak-console": {"project": SHARED_IDF, "name": "motorcontrol_can_probe",
                                "defaults": SHARED_IDF_BUILDS["idf-s2-optak-console"]["defaults"],
                                "gate": "idf-s2-optak-console", "build_dir": ".build/s2-console-live",
                                "label": "s2-console", "verify": "s2_console.py"},
}
DEFAULT_APP = "s2-bench"
PROJECT = ROOT / APPS[DEFAULT_APP]["project"]
APP = APPS[DEFAULT_APP]["name"]
ESP32S2_CHIP_ID = 2  # esptool/IDF image header chip id.
BAUD = 115200

# Recorded facts from docs/hardware_bench.md and the retained flash logs; a changed
# fixture needs a reviewed new record here, not a command-line override.
FIXTURE = {"port": "COM15", "usb_serial": "94A990398B40", "vid": 0x303A, "pid": 0x1002,
           "chip_prefix": "ESP32-S2", "mac": "dc:b4:d9:b5:a6:ce", "flash_size": "4MB"}
PINS = {"idf": "v5.5.5", "esptool": "4.12.0"}  # Exact installed versions; never upgraded here.

def flash_layout(app=DEFAULT_APP):
    """The reviewed offsets; flash_args and flasher_args.json must both state exactly these."""
    return {0x1000: "bootloader/bootloader.bin",
            0x8000: "partition_table/partition-table.bin",
            0x10000: APPS[app]["name"] + ".bin"}


FLASH_LAYOUT = flash_layout()
FLASH_OPTIONS = {"--flash_mode": {"dio"}, "--flash_freq": {"80m"}, "--flash_size": {"4MB"}}
# flasher_args.json names the same three images as parts with an offset and a file.
FLASHER_PARTS = {"bootloader": 0x1000, "partition-table": 0x8000, "app": 0x10000}
FLASH_BYTES = 4 * 1024 * 1024
ELF_HASH_OFFSET = 0xB0  # esp_app_desc_t.app_elf_sha256 inside the application image.

ESPTOOL_VERBS = {"read_mac", "write_flash"}  # Everything else is refused.
ESPTOOL_FORBIDDEN = ("erase", "efuse", "burn", "encrypt", "force", "merge", "--no-stub")
IDF_ACTIONS = ("reconfigure", "build")  # reconfigure refreshes the embedded Git revision.
SOURCE_DIRS = ("include", "src", "adapters", "examples", "experiments/canopen_service_fit")
SOURCE_SUFFIXES = {".h", ".hpp", ".c", ".cc", ".cpp", ".json", ".txt", ".cmake", ".defaults", ".in"}


class FlashError(RuntimeError):
    """A refused or failed step. status names it in the retained record."""

    def __init__(self, message, status="refused"):
        super().__init__(message)
        self.status = status


# ---------------------------------------------------------------- application selection
def project_dir(app):
    return ROOT / APPS[app]["project"]


def applied_defaults(app):
    """The defaults files IDF is given, in order, as project_description.json lists them."""
    return [project_dir(app) / name for name in (APPS[app]["defaults"] or ("sdkconfig.defaults",))]


def defaults_in_order(app):
    """Each listed file, then its .<target> file when present (IDF kconfig.cmake); later wins."""
    files = []
    for path in applied_defaults(app):
        files.append(path)
        variant = Path(str(path) + "." + TARGET)
        if variant.is_file():
            files.append(variant)
    return files


def _relative(path):
    path = Path(path)
    try:
        return path.resolve().relative_to(ROOT).as_posix()
    except ValueError:
        return path.as_posix()


def _same_path(text, path):
    if not isinstance(text, str) or not text:
        return False
    return os.path.normcase(str(Path(text).resolve())) == os.path.normcase(str(Path(path).resolve()))


def app_record(app):
    """The selection as the retained record states it."""
    spec = APPS[app]
    return {"name": app, "project": spec["project"], "project_name": spec["name"],
            "defaults": [_relative(path) for path in defaults_in_order(app)], "gate": spec["gate"],
            "flash_layout": {"0x%x" % offset: name for offset, name in sorted(flash_layout(app).items())},
            "verify": spec["verify"]}


def check_build_dir(build, app):
    """Refuse, before building, a directory that a different project configured."""
    description = Path(build) / "project_description.json"
    if not description.is_file():
        return
    try:
        facts = json.loads(description.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        raise FlashError("existing %s is unreadable: %s" % (description, error)) from error
    if facts.get("project_name") != APPS[app]["name"] or not _same_path(facts.get("project_path"), project_dir(app)):
        raise FlashError("build directory %s holds project %s from %s, not %s from %s; use another --build-dir" % (
            build, facts.get("project_name"), facts.get("project_path"), APPS[app]["name"], APPS[app]["project"]))
    # The smoke and console images share one project. A reused sdkconfig keeps the values of the
    # defaults it was first configured with, so another app's directory is refused before building.
    listed = facts.get("config_defaults")
    listed = listed.split(";") if isinstance(listed, str) and listed else []
    wanted = applied_defaults(app)
    if listed and (len(listed) != len(wanted) or not all(_same_path(a, b) for a, b in zip(listed, wanted))):
        raise FlashError("build directory %s was configured with defaults %s, not %s; use another --build-dir" % (
            build, ";".join(_relative(path) for path in listed), ";".join(_relative(path) for path in wanted)))


# ---------------------------------------------------------------- tool discovery
def _resolve(explicit, *installed):
    """A named path must exist: never quietly replace it with a guessed install."""
    if explicit:
        return Path(explicit) if Path(explicit).exists() else None
    for candidate in installed:
        if candidate and Path(candidate).exists():
            return Path(candidate)
    return None


def discover_tools(args, environ=os.environ):
    """Locate installed tools from arguments, environment, then known local installs.

    Reads the filesystem only. The MotorControl-RS paths are this machine's borrowed
    install, and every result is reported so a moved or missing tool is visible.
    """
    home = Path(environ.get("USERPROFILE", environ.get("HOME", "~")))
    core = environ.get("PLATFORMIO_CORE_DIR")
    sibling = ROOT.parent / "MotorControl-RS/build/p25"
    idf = _resolve(args.idf_path or environ.get("IDF_PATH"),
                   Path(core) / "packages/framework-espidf" if core else None,
                   "C:/pio/packages/framework-espidf", home / ".platformio/packages/framework-espidf")
    python = _resolve(args.idf_python or environ.get("IDF_PYTHON"),
                      sibling / "idf-python/Scripts/python.exe", sibling / "idf-python/bin/python")
    tools_path = _resolve(args.idf_tools_path or environ.get("IDF_TOOLS_PATH"), sibling / "idf-tools")
    search = []
    if idf:
        packages = idf.parent
        search = [str(p) for p in (packages / "toolchain-xtensa-esp-elf/bin", packages / "tool-cmake/bin",
                                   packages / "tool-ninja") if p.is_dir()]
    path = os.pathsep.join(search + [environ.get("PATH", "")])
    found = {"idf": idf, "idf_python": python, "idf_tools_path": tools_path}
    for name, executable in (("compiler", "xtensa-esp32s2-elf-gcc"), ("cmake", "cmake"),
                             ("ninja", "ninja")):
        location = shutil.which(executable, path=path)
        found[name] = Path(location) if location else None
    return {"paths": found, "path_prepend": search,
            "missing": sorted(name for name, value in found.items() if value is None)}


def tool_environment(tools):
    """Environment updates for child processes; IDF itself refuses an MSYS shell."""
    return {"IDF_PATH": str(tools["paths"]["idf"]),
            "IDF_TOOLS_PATH": str(tools["paths"]["idf_tools_path"]),
            "IDF_PYTHON_ENV_PATH": str(Path(os.path.abspath(tools["paths"]["idf_python"])).parent.parent),
            # No managed-component download can happen during a reproducible build.
            "IDF_COMPONENT_MANAGER": "0",
            "PATH": os.pathsep.join(tools["path_prepend"] + [os.environ.get("PATH", "")])}


def recommended_compiler(idf):
    """The toolchain version this SDK declares, from its own tools.json."""
    tools = json.loads((Path(idf) / "tools/tools.json").read_text(encoding="utf-8"))["tools"]
    for tool in tools:
        if tool["name"] == "xtensa-esp-elf":
            for version in tool["versions"]:
                if version.get("status") == "recommended":
                    return version["name"]
    raise FlashError("SDK tools.json names no recommended xtensa-esp-elf version", "unavailable")


# ---------------------------------------------------------------- command guards
def build_command(tools, build, app=DEFAULT_APP):
    command = [str(tools["paths"]["idf_python"]), str(Path(tools["paths"]["idf"]) / "tools/idf.py"),
               "-C", str(project_dir(app)), "-B", str(build), "-DIDF_TARGET=" + TARGET,
               "-DSDKCONFIG=" + (Path(build) / "sdkconfig").as_posix()]
    if APPS[app]["defaults"]:
        # The gate's own argument (check_firmware.shared_idf_build), so the image is that build.
        command.append("-DSDKCONFIG_DEFAULTS=" + ";".join(path.as_posix() for path in applied_defaults(app)))
    return command + list(IDF_ACTIONS)


def esptool_command(tools, port, verb, *arguments, after="hard_reset"):
    command = [str(tools["paths"]["idf_python"]), "-m", "esptool", "--chip", TARGET, "--port", port,
               "--baud", str(BAUD), "--before", "default_reset", "--after", after, verb, *arguments]
    assert_safe_esptool(command)
    return command


def assert_safe_esptool(command):
    """Last check before any esptool process: only reading the MAC or writing the image."""
    # Only arguments count: an install path may legitimately contain these words.
    text = [str(part).lower() for part in command]
    text = text[text.index("esptool"):] if "esptool" in text else text
    if not ESPTOOL_VERBS.intersection(text):
        raise FlashError("esptool command has no permitted verb: %s" % command)
    if any(word in part for part in text for word in ESPTOOL_FORBIDDEN):
        raise FlashError("forbidden esptool argument in: %s" % command)
    if len(ESPTOOL_VERBS.intersection(text)) != 1:
        raise FlashError("esptool command names several verbs: %s" % command)


def execute(command, cwd, log, env=None, timeout=600):
    """Run one process once, retaining combined output. Returns (returncode, output)."""
    try:
        result = subprocess.run([str(part) for part in command], cwd=cwd, env=env, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, errors="replace",
                                timeout=timeout, check=False)
        code, output = result.returncode, result.stdout
    except subprocess.TimeoutExpired as error:
        partial = error.stdout or ""
        partial = partial.decode("utf-8", errors="replace") if isinstance(partial, bytes) else partial
        code, output = None, partial + "\nCommand exceeded %d seconds\n" % timeout
    except OSError as error:
        code, output = None, str(error) + "\n"
    Path(log).write_text(output, encoding="utf-8", newline="\n")
    return code, output


# ---------------------------------------------------------------- USB identity
def comports():
    """Enumerate USB serial devices. Enumeration opens nothing; dry run never calls it."""
    from serial.tools import list_ports
    return list_ports.comports()


def check_usb(port, serial_number, ports):
    """Require the recorded probe, uniquely, on the requested port."""
    if serial_number != FIXTURE["usb_serial"]:
        raise FlashError("USB serial %s is not the recorded fixture %s" % (serial_number, FIXTURE["usb_serial"]))
    named = [p for p in ports if p.device.lower() == port.lower()]
    if len(named) != 1:
        raise FlashError("port %s is not uniquely present" % port)
    item = named[0]
    if item.serial_number != serial_number or (item.vid, item.pid) != (FIXTURE["vid"], FIXTURE["pid"]):
        raise FlashError("port %s is %s %s:%s, not the recorded ESP-Prog2" % (
            port, item.serial_number, item.vid, item.pid))
    same = [p.device for p in ports if p.serial_number == serial_number]
    if len(same) != 1:
        raise FlashError("USB serial %s appears on several ports: %s" % (serial_number, same))
    return {"port": item.device, "usb_serial": item.serial_number,
            "vid_pid": "%04X:%04X" % (item.vid, item.pid), "description": getattr(item, "description", "")}


def parse_target(output):
    """Target facts printed by esptool; absent facts stay absent."""
    chip = re.search(r"^Chip is (.+)$", output, re.M)
    mac = re.search(r"^MAC: ([0-9a-fA-F:]{17})\s*$", output, re.M)
    size = re.search(r"^Detected flash size: (\S+)", output, re.M)
    version = re.search(r"^esptool\.py v(\S+)", output, re.M)
    return {"chip": chip.group(1).strip() if chip else None, "mac": mac.group(1).lower() if mac else None,
            "flash_size": size.group(1) if size else None, "esptool": version.group(1) if version else None}


def check_target(facts):
    """The probe identity above says nothing about the board wired behind it."""
    if not facts["chip"] or not facts["chip"].startswith(FIXTURE["chip_prefix"]):
        raise FlashError("target chip is %s, not %s" % (facts["chip"], FIXTURE["chip_prefix"]))
    if facts["mac"] != FIXTURE["mac"]:
        raise FlashError("target MAC is %s, not the recorded %s" % (facts["mac"], FIXTURE["mac"]))
    if facts["flash_size"] not in (None, FIXTURE["flash_size"]):
        raise FlashError("target flash is %s, not %s" % (facts["flash_size"], FIXTURE["flash_size"]))


# ---------------------------------------------------------------- build validation
def parse_flash_args(text, layout=None):
    """Accept exactly the reviewed options and three images; anything extra is refused."""
    layout = FLASH_LAYOUT if layout is None else layout
    lines = [line.strip() for line in text.splitlines() if line.strip()]
    if not lines:
        raise FlashError("flash_args is empty")
    words = lines[0].split()
    if len(words) != len(FLASH_OPTIONS) * 2:
        raise FlashError("unexpected flash options: %s" % lines[0])
    options = dict(zip(words[0::2], words[1::2]))
    for name, allowed in FLASH_OPTIONS.items():
        if options.get(name) not in allowed:
            raise FlashError("flash option %s=%s is outside the reviewed set" % (name, options.get(name)))
    images = {}
    for line in lines[1:]:
        parts = line.split()
        if len(parts) != 2 or not re.fullmatch(r"0x[0-9a-fA-F]+", parts[0]):
            raise FlashError("unexpected flash_args line: %s" % line)
        offset = int(parts[0], 16)
        if offset in images:
            raise FlashError("repeated flash offset: %s" % parts[0])
        images[offset] = parts[1]
    if images != layout:
        raise FlashError("flash layout %s differs from the reviewed %s" % (images, layout))
    return options, images


def check_flasher_args(flasher, options, images):
    """flasher_args.json is the project's own statement of the write; it must equal flash_args."""
    def offsets(entries):
        try:
            return {int(offset, 16): name for offset, name in entries.items()}
        except (AttributeError, TypeError, ValueError):
            return None
    if offsets(flasher.get("flash_files", {})) != images:
        raise FlashError("flasher_args.json flash_files %s differ from flash_args %s" % (
            flasher.get("flash_files"), images), "build_invalid")
    settings = {"flash_mode": options["--flash_mode"], "flash_size": options["--flash_size"],
                "flash_freq": options["--flash_freq"]}
    if flasher.get("flash_settings") != settings:
        raise FlashError("flasher_args.json flash_settings %s differ from flash_args %s" % (
            flasher.get("flash_settings"), settings), "build_invalid")
    words = flasher.get("write_flash_args")
    if not isinstance(words, list) or len(words) % 2 or dict(zip(words[0::2], words[1::2])) != options:
        raise FlashError("flasher_args.json write_flash_args %s differ from flash_args" % words, "build_invalid")
    for part, offset in FLASHER_PARTS.items():
        entry = flasher.get(part, {})
        if offsets({entry.get("offset"): entry.get("file")}) != {offset: images.get(offset)}:
            raise FlashError("flasher_args.json %s is %s, not %s at 0x%x" % (
                part, entry, images.get(offset), offset), "build_invalid")


def _image_chip(data, name):
    if len(data) < 16 or data[0] != 0xE9:
        raise FlashError("%s is not an ESP image" % name)
    return int.from_bytes(data[12:14], "little")


def check_sdkconfig(defaults_text, sdkconfig_text):
    """A reused build directory keeps its old sdkconfig; catch drift from the defaults.

    defaults_text may join several defaults files in IDF's order: as for IDF, a later
    assignment of the same name wins."""
    actual = {}
    for line in sdkconfig_text.splitlines():
        match = re.fullmatch(r"(CONFIG_\w+)=(.*)", line.strip())
        if match:
            actual[match.group(1)] = match.group(2)
    wanted = {}
    for line in defaults_text.splitlines():
        line = line.strip()
        unset = re.fullmatch(r"# (CONFIG_\w+) is not set", line)
        assigned = re.fullmatch(r"(CONFIG_\w+)=(.*)", line)
        if unset:
            wanted[unset.group(1)] = None
        elif assigned:
            wanted[assigned.group(1)] = assigned.group(2)
    problems = []
    for name, value in wanted.items():
        if value is None and actual.get(name, "n") not in ("n", ""):
            problems.append(name + " is set")
        elif value is not None and actual.get(name) != value:
            problems.append("%s=%s, defaults %s" % (name, actual.get(name), value))
    if actual.get("CONFIG_IDF_TARGET") != '"%s"' % TARGET:
        problems.append("CONFIG_IDF_TARGET=%s" % actual.get("CONFIG_IDF_TARGET"))
    return problems


def check_project(description, app):
    """project_description.json must name the selected project, its images and its defaults."""
    spec = APPS[app]
    if description.get("project_name") != spec["name"]:
        raise FlashError("build is project %s, not the selected %s" % (description.get("project_name"), spec["name"]),
                         "build_invalid")
    if not _same_path(description.get("project_path"), project_dir(app)):
        raise FlashError("build project path %s is not %s" % (description.get("project_path"), project_dir(app)),
                         "build_invalid")
    if (description.get("app_elf"), description.get("app_bin")) != (spec["name"] + ".elf", spec["name"] + ".bin"):
        raise FlashError("build images are %s/%s, not %s.elf/.bin" % (
            description.get("app_elf"), description.get("app_bin"), spec["name"]), "build_invalid")
    listed = description.get("config_defaults")
    listed = listed.split(";") if isinstance(listed, str) and listed else []
    wanted = applied_defaults(app)
    if len(listed) != len(wanted) or not all(_same_path(a, b) for a, b in zip(listed, wanted)):
        raise FlashError("build applied defaults %s, not %s" % (
            description.get("config_defaults"), ";".join(_relative(path) for path in wanted)), "build_invalid")


def smoke_inputs(app):
    """(expected, forbidden) compiler inputs exactly as check_firmware.py's gate requires them."""
    gate = SHARED_IDF_BUILDS[APPS[app]["gate"]]
    inventory = firmware_inventory(ROOT)
    expected = [ROOT / path for path in inventory["sources"]] + sorted((ROOT / "src").rglob("*.cpp"))
    expected += [project_dir(app) / "main/main.cpp"] + [ROOT / item for item in gate["extra"]]
    return expected, [ROOT / BENCH_PROJECT] + [ROOT / item for item in gate["forbidden"]]


def check_gate(build, app):
    """The gate's resolved board/smoke values, compiler inputs and cross compiler."""
    gate = SHARED_IDF_BUILDS[APPS[app]["gate"]]
    database = Path(build) / "compile_commands.json"
    try:
        if gate["target"] != TARGET:
            raise ValueError("gate %s builds %s" % (APPS[app]["gate"], gate["target"]))
        check_resolved_sdkconfig(Path(build) / "sdkconfig", gate["sdkconfig"])
        expected, forbidden = smoke_inputs(app)
        entries = check_inputs(database, expected, forbidden)
        check_compiler(database, gate["target"])
    except (OSError, ValueError, KeyError, VerificationFailure) as error:
        raise FlashError("not the %s image: %s" % (app, error), "build_invalid") from error
    # The gate's required values, each verified in the resolved sdkconfig (None: not set).
    return {"compile_entries": entries, "compile_commands_sha256": _file_hash(database),
            "gate_sdkconfig": {name: value for name, value in sorted(gate["sdkconfig"].items())}}


def validate_build(build, app=DEFAULT_APP):
    """Check the generated outputs that define what would be written. Reads files only."""
    build = Path(build)
    spec = APPS[app]
    layout = flash_layout(app)
    try:
        description = json.loads((build / "project_description.json").read_text(encoding="utf-8"))
        flasher = json.loads((build / "flasher_args.json").read_text(encoding="utf-8"))
        flash_args = (build / "flash_args").read_text(encoding="utf-8")
        sdkconfig = (build / "sdkconfig").read_text(encoding="utf-8")
    except (OSError, ValueError) as error:
        raise FlashError("build outputs unavailable: %s" % error, "build_invalid") from error
    if description.get("target") != TARGET:
        raise FlashError("build target is %s" % description.get("target"), "build_invalid")
    check_project(description, app)
    checks = ["target", "project", "config_defaults"]
    extra = flasher.get("extra_esptool_args", {})
    if (extra.get("chip"), extra.get("before"), extra.get("after")) != (TARGET, "default_reset", "hard_reset"):
        raise FlashError("generated esptool arguments differ: %s" % extra, "build_invalid")
    for part in ("bootloader", "app", "partition-table"):
        if flasher.get(part, {}).get("encrypted") != "false":
            raise FlashError("generated %s requires encryption" % part, "build_invalid")
    options, images = parse_flash_args(flash_args, layout)
    check_flasher_args(flasher, options, images)
    checks += ["esptool_args", "unencrypted", "flash_args", "flasher_args"]
    hashes = {}
    end = 0
    for offset, name in sorted(images.items()):
        path = build / name
        if not path.is_file():
            raise FlashError("flash image missing: %s" % name, "build_invalid")
        data = path.read_bytes()
        if offset < end or offset + len(data) > FLASH_BYTES:
            raise FlashError("flash image %s overlaps or exceeds flash" % name, "build_invalid")
        end = offset + len(data)
        hashes[name] = hashlib.sha256(data).hexdigest()
        if name != layout[0x8000] and _image_chip(data, name) != ESP32S2_CHIP_ID:
            raise FlashError("%s is not built for %s" % (name, TARGET), "build_invalid")
    checks.append("images")
    elf = build / (spec["name"] + ".elf")
    if not elf.is_file():
        raise FlashError("linked ELF missing", "build_invalid")
    linked = sorted(path.name for path in build.glob("*.elf"))
    if linked != [elf.name]:
        raise FlashError("build directory holds ELF files %s, not only %s" % (linked, elf.name), "build_invalid")
    elf_hash = hashlib.sha256(elf.read_bytes()).hexdigest()
    hashes[elf.name] = elf_hash
    hashes["sdkconfig"] = _file_hash(build / "sdkconfig")
    application = (build / layout[0x10000]).read_bytes()
    if application[ELF_HASH_OFFSET:ELF_HASH_OFFSET + 32].hex() != elf_hash:
        raise FlashError("application image does not embed this ELF's hash", "build_invalid")
    checks += ["single_elf", "elf_hash_embedded"]
    files = defaults_in_order(app)
    drift = check_sdkconfig("\n".join(path.read_text(encoding="utf-8") for path in files), sdkconfig)
    if drift:
        raise FlashError("sdkconfig differs from %s: %s" % (", ".join(path.name for path in files), "; ".join(drift)),
                         "build_invalid")
    checks.append("sdkconfig_defaults")
    facts = {"target": description["target"], "app_elf": elf.name, "image_sha256": hashes,
             "flash_options": options, "flash_args": flash_args, "sdk_version": description.get("git_revision"),
             "project_version": description.get("project_version"),
             "c_compiler": description.get("c_compiler")}
    if spec["gate"]:
        facts.update(check_gate(build, app))
        checks += ["gate_sdkconfig", "compiler_inputs", "compiler"]
    facts.update(project_name=description["project_name"], app_bin=description["app_bin"],
                 project_path=_relative(description["project_path"]),
                 config_defaults=[_relative(path) for path in description["config_defaults"].split(";")],
                 app_checks=checks)
    return facts


def compiled_revision(build):
    """The Git revision compiled into main.cpp; stale if CMake did not reconfigure."""
    entries = json.loads((Path(build) / "compile_commands.json").read_text(encoding="utf-8"))
    for entry in entries:
        if Path(entry.get("file", "")).name == "main.cpp" and "s2_bench" in entry.get("file", ""):
            match = re.search(r'MOTORCONTROLCAN_SOURCE_REVISION=\\*"([0-9a-f]{40})', entry.get("command", ""))
            return match.group(1) if match else None
    return None


def version_revision(project_version):
    """The commit in IDF's PROJECT_VER, `git describe --always --tags --dirty` at configure
    time and embedded in esp_app_desc_t. A bare tag names no commit and is refused."""
    match = re.fullmatch(r"(?:.+-g)?([0-9a-f]{7,40})(?:-dirty)?", project_version or "")
    return match.group(1) if match else None


def check_revision(build, app, facts, head):
    """The revision the image embeds must be HEAD; returns the record's revision_check."""
    if APPS[app]["gate"]:
        embedded = version_revision(facts.get("project_version"))
        if not embedded or not head.startswith(embedded):
            raise FlashError("image project version %s is not HEAD %s" % (facts.get("project_version"), head),
                             "build_invalid")
        return {"source": "project_version (git describe at configure)", "embedded": facts["project_version"],
                "head": head}
    embedded = compiled_revision(build)
    if embedded != head:
        raise FlashError("image embeds revision %s, HEAD is %s" % (embedded, head), "build_invalid")
    return {"source": "MOTORCONTROLCAN_SOURCE_REVISION", "embedded": embedded, "head": head}


# ---------------------------------------------------------------- source identity
def _git(*arguments):
    code, output = _git_run(arguments)
    if code != 0:
        raise FlashError("git %s failed: %s" % (" ".join(arguments), output), "unavailable")
    return output


def _git_run(arguments):
    result = subprocess.run(["git", *arguments], cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            check=False)
    return result.returncode, result.stdout.decode("utf-8", errors="replace")


def _file_hash(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def source_identity():
    """Revision, dirty state and input hashes. Dirty bytes are hashed, never committed."""
    head = _git("rev-parse", "HEAD").strip()
    status = _git("status", "--porcelain=v1", "--untracked-files=all")
    dirty = {}
    for line in status.splitlines():
        name = line[3:].split(" -> ")[-1].strip().strip('"')
        path = ROOT / name
        dirty[name] = _file_hash(path) if path.is_file() else None
    sources = {}
    for directory in SOURCE_DIRS:
        for path in sorted((ROOT / directory).rglob("*")):
            if path.is_file() and path.suffix in SOURCE_SUFFIXES:
                sources[path.relative_to(ROOT).as_posix()] = _file_hash(path)
    for name in ("CMakeLists.txt", "library.json"):
        sources[name] = _file_hash(ROOT / name)
    return {"base_commit": head, "working_tree": status, "dirty_sha256": dirty, "source_sha256": sources}


def changed_sources(before, after):
    """Names that changed while building: such an image has no identifiable source."""
    names = sorted(name for name in set(before["source_sha256"]) | set(after["source_sha256"])
                   if before["source_sha256"].get(name) != after["source_sha256"].get(name))
    if before["base_commit"] != after["base_commit"]:
        names.append("HEAD")
    return names


# ---------------------------------------------------------------- retained record
class Artifact:
    """One new directory; the record is rewritten so an interruption keeps every phase."""

    def __init__(self, directory, mode, app=DEFAULT_APP):
        self.directory = Path(directory)
        self.directory.mkdir(parents=True, exist_ok=False)
        self.record = {"schema": 1, "mode": mode, "status": "running", "phases": [], "app": app_record(app),
                       "started_utc": datetime.now(timezone.utc).isoformat(), "fixture": dict(FIXTURE),
                       "baud": BAUD, "erase_all": False, "efuse": False, "retry": False}
        self.save()

    def save(self):
        (self.directory / "build.json").write_text(json.dumps(self.record, indent=2, sort_keys=True) + "\n",
                                                   encoding="utf-8", newline="\n")

    def phase(self, name, **details):
        self.record["phases"].append(dict(name=name, **details))
        self.save()

    def retain(self, source, name=None):
        source = Path(source)
        if source.is_file():
            shutil.copyfile(source, self.directory / (name or source.name))


def default_artifact(mode, app=DEFAULT_APP):
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    if mode == "flash":
        return ROOT / "docs/bench" / ("%s-%s-upload" % (stamp, APPS[app]["label"]))
    return ROOT / ".build/s2-flash-artifacts" / ("%s-%s-build" % (stamp, APPS[app]["label"]))


def ping_instructions(port, serial_number, artifact):
    stamp = "NEW_PING_DIRECTORY"
    return ["python scripts/s2_bench.py --port %s --serial %s --command ping --seconds 2 --output %s" % (
        port, serial_number, stamp),
            "python scripts/s2_flash.py --check-ping %s --artifact %s" % (stamp, artifact)]


def verify_instructions(app, port, serial_number, artifact):
    """The separate check of the running image: a ping for the bench, else the app's own runner."""
    if not APPS[app]["gate"]:
        return ping_instructions(port, serial_number, artifact)
    if APPS[app]["verify"] == "s2_console.py":
        return ["python scripts/s2_console.py --port %s --serial %s --artifact %s --reset-en "
                "--script scripts/console_scripts/local_routes.txt --output NEW_CONSOLE_DIRECTORY" % (
                    port, serial_number, artifact)]
    return ["python scripts/s2_smoke.py --port %s --serial %s --artifact %s --output NEW_SMOKE_DIRECTORY" % (
        port, serial_number, artifact)]


# ---------------------------------------------------------------- modes
def dry_run(args):
    """Files only: no process, no USB enumeration, no port, no artifact directory."""
    tools = discover_tools(args)
    app = args.app
    spec = APPS[app]
    build = Path(args.build_dir).resolve()
    port = args.port or "<VERIFIED_PORT>"
    print("DRY RUN: plan only; nothing is built, enumerated, opened or written.")
    print("Selected application %s: %s, project %s, images %s.elf/.bin" % (
        app, spec["project"], spec["name"], spec["name"]))
    print("  defaults in IDF order: " + ", ".join(_relative(path) for path in defaults_in_order(app)))
    if spec["gate"]:
        print("  configured exactly as check_firmware.py --target %s" % spec["gate"])
    print("Recorded fixture (checked at --flash, never assumed): %s" % json.dumps(FIXTURE))
    for name, value in tools["paths"].items():
        print("  tool %-15s %s" % (name, value if value else "MISSING"))
    print("Planned steps for --flash, stopping at the first failure:")
    print("  1 create a new artifact directory; retain the selection, revision, dirty hashes, tool versions")
    print("  2 require USB %(usb_serial)s %(vid)04X:%(pid)04X on %(port)s" % FIXTURE)
    if not tools["missing"]:
        print("  3 build: " + " ".join(build_command(tools, build, app)))
    print("  4 validate project %s at %s, applied defaults, target, the only ELF, images, ELF hash, sdkconfig" % (
        spec["name"], spec["project"]))
    print("    and the layout in flash_args and flasher_args.json: %s, %s" % (
        ", ".join("0x%x %s" % item for item in sorted(flash_layout(app).items())),
        "/".join("|".join(sorted(values)) for values in FLASH_OPTIONS.values())))
    if spec["gate"]:
        gate = SHARED_IDF_BUILDS[spec["gate"]]
        print("    plus the gate's sdkconfig values: " + " ".join(
            "%s=%s" % (name, "<not set>" if value is None else value) for name, value in sorted(gate["sdkconfig"].items())))
        print("    and compiler inputs: %s; nothing from %s; %s compiler" % (
            ", ".join(["shared inventory", "core", "main.cpp"] + list(gate["extra"])),
            ", ".join([BENCH_PROJECT] + list(gate["forbidden"])), "xtensa-" + gate["target"] + "-elf-"))
        print("    and a project version naming HEAD")
    else:
        print("    and the compiled MOTORCONTROLCAN_SOURCE_REVISION equal to HEAD")
    if not tools["missing"]:
        read_mac = esptool_command(tools, port, "read_mac")
        write = esptool_command(tools, port, "write_flash", "@flash_args")
        print("  5 target identity: " + " ".join(read_mac))
        print("  6 write once from %s: %s" % (build, " ".join(write)))
    print("  7 verify separately:")
    for line in verify_instructions(app, port, args.serial or FIXTURE["usb_serial"], "<ARTIFACT>"):
        print("      " + line)
    try:
        check_build_dir(build, app)
    except FlashError as error:
        print("Refused before building: %s" % error)
    if (build / "flash_args").is_file():
        try:
            facts = validate_build(build, app)
            print("Existing build at %s is valid for this plan: %s" % (build, json.dumps(facts["image_sha256"])))
        except FlashError as error:
            print("Existing build at %s would be refused: %s" % (build, error))
    else:
        print("No existing build at %s." % build)
    if tools["missing"]:
        print("VERDICT: UNAVAILABLE; missing " + ", ".join(tools["missing"]))
        return 2
    print("VERDICT: INCOMPLETE; a dry run is not a build or an upload.")
    return 0


def _record_tools(artifact, args, tools):
    """Run the shared SDK gate, then pin the versions it does not."""
    os.environ.update(tool_environment(tools))
    runner = Runner(artifact.directory / "tools", "s2-flash")
    gate = argparse.Namespace(idf_path=Path(tools["paths"]["idf"]), idf_python=Path(tools["paths"]["idf_python"]))
    _, env = sdk_environment(gate, runner, target=TARGET)
    env.update(tool_environment(tools))
    versions = {}
    for name, command in (("compiler", [tools["paths"]["compiler"], "--version"]),
                          ("cmake", [tools["paths"]["cmake"], "--version"]),
                          ("ninja", [tools["paths"]["ninja"], "--version"]),
                          ("python", [tools["paths"]["idf_python"], "--version"]),
                          ("esptool", [tools["paths"]["idf_python"], "-m", "esptool", "version"])):
        code, output = execute(command, ROOT, artifact.directory / ("version_%s.log" % name), env, 60)
        if code != 0:
            raise FlashError("%s version check failed" % name, "unavailable")
        versions[name] = output.strip().splitlines()[0] if output.strip() else ""
    wanted = recommended_compiler(tools["paths"]["idf"])
    if wanted not in versions["compiler"]:
        raise FlashError("compiler %s is not the SDK's %s" % (versions["compiler"], wanted), "unavailable")
    if PINS["esptool"] not in versions["esptool"]:
        raise FlashError("esptool %s is not the pinned %s" % (versions["esptool"], PINS["esptool"]), "unavailable")
    artifact.record.update(tools={k: str(v) for k, v in tools["paths"].items()}, tool_versions=versions,
                           idf_version=PINS["idf"], sdk_sources_verified=runner.summary.get("source_hashes_verified"))
    artifact.phase("tools")
    return env


def _build(artifact, tools, env, build, app=DEFAULT_APP):
    """Build once; refuse an image whose source moved or whose revision is stale."""
    before = source_identity()
    artifact.record.update(before)
    artifact.phase("source_identity", dirty_files=len(before["dirty_sha256"]))
    Path(build).mkdir(parents=True, exist_ok=True)
    command = build_command(tools, build, app)
    artifact.record["build_command"] = command
    code, _ = execute(command, ROOT, artifact.directory / "build.log", env, 900)
    if code != 0:
        raise FlashError("idf.py failed (%s); see %s" % (code, artifact.directory / "build.log"), "build_failed")
    after = source_identity()
    moved = changed_sources(before, after)
    artifact.record["source_stable"] = not moved
    if moved:
        artifact.record["source_changed_during_build"] = moved
        raise FlashError("sources changed during the build: %s" % ", ".join(moved[:8]), "source_unstable")
    facts = validate_build(build, app)
    revision = check_revision(build, app, facts, before["base_commit"])
    artifact.record.update(facts)
    artifact.record["revision_check"] = revision
    if not APPS[app]["gate"]:
        artifact.record["compiled_revision"] = revision["embedded"]
    for name in ("sdkconfig", "flash_args", "flasher_args.json", "project_description.json"):
        artifact.retain(Path(build) / name)
    artifact.phase("build", image_sha256=facts["image_sha256"], app_checks=facts["app_checks"])
    return facts


def _flash(artifact, tools, env, build, port, serial_number, app=DEFAULT_APP):
    """Identity immediately before the write, then exactly one write. No retry."""
    artifact.record["usb"] = check_usb(port, serial_number, comports())
    artifact.phase("usb_identity", **artifact.record["usb"])
    read_mac = esptool_command(tools, port, "read_mac")
    artifact.record["read_mac_command"] = read_mac
    code, output = execute(read_mac, build, artifact.directory / "read_mac.log", env, 120)
    facts = parse_target(output)
    artifact.record["target_identity"] = facts
    if code != 0:
        raise FlashError("target identity read failed (%s); nothing was written" % code, "identity_failed")
    check_target(facts)
    artifact.phase("target_identity", **facts)
    write = esptool_command(tools, port, "write_flash", "@flash_args")
    artifact.record["flash_command"] = write
    code, output = execute(write, build, artifact.directory / "flash.log", env, 300)
    result = parse_target(output)
    verified = output.count("Hash of data verified.")
    artifact.record["flash"] = dict(result, returncode=code, verified_images=verified)
    if code != 0:
        raise FlashError("esptool write failed (%s); the image on the target is unknown; no retry" % code,
                         "flash_failed")
    images = len(flash_layout(app))
    if verified != images or "Hard resetting" not in output:
        raise FlashError("esptool exited 0 without %d verified images and a reset" % images, "flash_unverified")
    artifact.phase("flash", verified_images=verified)


def build_or_flash(args, flash):
    if os.environ.get("MSYSTEM"):
        print("Refused: ESP-IDF rejects MSYS (MSYSTEM=%s); run from PowerShell or cmd." % os.environ["MSYSTEM"],
              file=sys.stderr)
        return 2
    tools = discover_tools(args)
    if tools["missing"]:
        print("Unavailable tools: %s; nothing was run." % ", ".join(tools["missing"]), file=sys.stderr)
        return 2
    app = args.app
    build = Path(args.build_dir).resolve()
    try:
        # Another project's directory would rebuild that project, never the selected one.
        check_build_dir(build, app)
        if flash:
            # Check first so a wrong fixture does not cost a build or an artifact.
            check_usb(args.port, args.serial, comports())
    except FlashError as error:
        print("Refused: %s" % error, file=sys.stderr)
        return 2
    try:
        artifact = Artifact(args.output or default_artifact("flash" if flash else "build", app),
                            "flash" if flash else "build-only", app)
    except FileExistsError as error:
        print("Refused: artifact directory already exists: %s" % error.filename, file=sys.stderr)
        return 2
    artifact.record["build_dir"] = str(build)
    try:
        env = _record_tools(artifact, args, tools)
        _build(artifact, tools, env, build, app)
        if flash:
            _flash(artifact, tools, env, build, args.port, args.serial, app)
        artifact.record["status"] = "flashed" if flash else "built"
    except (FlashError, VerificationFailure, OSError, ValueError, KeyError) as error:
        artifact.record["status"] = getattr(error, "status", "failed")
        artifact.record["error"] = str(error)
        print("FAILED (%s): %s" % (artifact.record["status"], error), file=sys.stderr)
    finally:
        artifact.record["finished_utc"] = datetime.now(timezone.utc).isoformat()
        artifact.save()
    print("Evidence: %s" % artifact.directory)
    print("Status: %s" % artifact.record["status"])
    if artifact.record["status"] == "flashed":
        print("Flashing restarted the MCU; that establishes no fresh SDO session and clears no uncertainty.")
        print("Verify the running image with these separate steps:")
        for line in verify_instructions(app, args.port, args.serial, artifact.directory):
            print("  " + line)
    return 0 if artifact.record["status"] in ("built", "flashed") else 1


def check_ping(args):
    """Offline image check: nothing here touches a port."""
    build = json.loads((Path(args.artifact) / "build.json").read_text(encoding="utf-8"))
    selected = build.get("app", {}).get("name", DEFAULT_APP) if isinstance(build.get("app"), dict) else DEFAULT_APP
    if selected != DEFAULT_APP:
        # Only the bench image answers a ping; a shared image is checked by its own runner.
        runner = APPS[selected]["verify"] if selected in APPS else "its own runner"
        print(json.dumps({"capture": str(args.check_ping), "artifact": str(args.artifact), "matches": False,
                          "problems": ["build record is the %s image; verify it with %s" % (selected, runner)]},
                         indent=2))
        return 1
    report = replay_capture(args.check_ping)
    problems = check_image(report, build)
    if report.verdict not in ("control",):
        problems.append("ping capture verdict is %s" % report.verdict)
    print(json.dumps({"capture": str(args.check_ping), "artifact": str(args.artifact),
                      "verdict": report.verdict, "matches": not problems, "problems": problems}, indent=2))
    return 0 if not problems else 1


def parse_args(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--dry-run", action="store_true")
    mode.add_argument("--build-only", action="store_true")
    mode.add_argument("--flash", action="store_true")
    mode.add_argument("--check-ping", type=Path, metavar="CAPTURE")
    parser.add_argument("--app", choices=sorted(APPS), default=DEFAULT_APP,
                        help="Reviewed application (default %s)" % DEFAULT_APP)
    parser.add_argument("--artifact", type=Path, help="Retained build record for --check-ping")
    parser.add_argument("--port", help="Required with --flash; checked against the recorded USB identity")
    parser.add_argument("--serial", help="Required with --flash: the exact USB serial")
    parser.add_argument("--build-dir", type=Path,
                        help="Short build path; a Windows dependency-file limit applies (default %s)" % ", ".join(
                            "%s for %s" % (spec["build_dir"], name) for name, spec in sorted(APPS.items())))
    parser.add_argument("--output", type=Path, help="New artifact directory; must not exist")
    parser.add_argument("--idf-path", type=Path)
    parser.add_argument("--idf-python", type=Path)
    parser.add_argument("--idf-tools-path", type=Path)
    args = parser.parse_args(argv)
    if args.flash and not (args.port and args.serial):
        parser.error("--flash requires --port and --serial")
    if args.check_ping and not args.artifact:
        parser.error("--check-ping requires --artifact")
    if args.check_ping and args.app != DEFAULT_APP:
        parser.error("--check-ping verifies the %s image only; verify %s with %s" % (
            DEFAULT_APP, args.app, APPS[args.app]["verify"]))
    if args.build_dir is None:
        args.build_dir = ROOT / APPS[args.app]["build_dir"]
    return args


def main(argv=None):
    args = parse_args(argv)
    if args.dry_run:
        return dry_run(args)
    if args.check_ping:
        return check_ping(args)
    return build_or_flash(args, args.flash)


if __name__ == "__main__":
    raise SystemExit(main())
