#!/usr/bin/env python3
"""Reproducible build and upload of examples/s2_bench, with a retained record.

Exactly one mode is required, so nothing flashes by default:

  --dry-run           Print the plan from files only. Runs no process and neither
                      enumerates nor opens a port. It is not a build or an upload.
  --build-only        Build and validate; never touches a port.
  --flash             Build, validate, check USB and target identity, write once.
  --check-ping DIR    Offline: compare an s2_bench.py ping capture with --artifact.

The recorded fixture is checked, never assumed. A failed step is retained and is
not retried. This tool never erases all flash, changes eFuses, downloads a vendor
executable or upgrades a toolchain, and it does not make the running image a
qualified one: verify it afterwards with s2_bench.py ping and --check-ping.
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

from check_firmware import sdk_environment
from s2_evidence import check_image, replay_capture
from verify import Runner, VerificationFailure


ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "examples/s2_bench"
APP = "motorcontrol_can_s2_bench"
TARGET = "esp32s2"
ESP32S2_CHIP_ID = 2  # esptool/IDF image header chip id.
BAUD = 115200

# Recorded facts from docs/hardware_bench.md and the retained flash logs; a changed
# fixture needs a reviewed new record here, not a command-line override.
FIXTURE = {"port": "COM15", "usb_serial": "94A990398B40", "vid": 0x303A, "pid": 0x1002,
           "chip_prefix": "ESP32-S2", "mac": "dc:b4:d9:b5:a6:ce", "flash_size": "4MB"}
PINS = {"idf": "v5.5.5", "esptool": "4.12.0"}  # Exact installed versions; never upgraded here.

FLASH_LAYOUT = {0x1000: "bootloader/bootloader.bin",
                0x8000: "partition_table/partition-table.bin",
                0x10000: APP + ".bin"}
FLASH_OPTIONS = {"--flash_mode": {"dio"}, "--flash_freq": {"80m"}, "--flash_size": {"4MB"}}
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
def build_command(tools, build):
    return [str(tools["paths"]["idf_python"]), str(Path(tools["paths"]["idf"]) / "tools/idf.py"),
            "-C", str(PROJECT), "-B", str(build), "-DIDF_TARGET=" + TARGET,
            "-DSDKCONFIG=" + (Path(build) / "sdkconfig").as_posix(), *IDF_ACTIONS]


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
def parse_flash_args(text):
    """Accept exactly the reviewed options and three images; anything extra is refused."""
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
    if images != FLASH_LAYOUT:
        raise FlashError("flash layout %s differs from the reviewed %s" % (images, FLASH_LAYOUT))
    return options, images


def _image_chip(data, name):
    if len(data) < 16 or data[0] != 0xE9:
        raise FlashError("%s is not an ESP image" % name)
    return int.from_bytes(data[12:14], "little")


def check_sdkconfig(defaults_text, sdkconfig_text):
    """A reused build directory keeps its old sdkconfig; catch drift from the defaults."""
    actual = {}
    for line in sdkconfig_text.splitlines():
        match = re.fullmatch(r"(CONFIG_\w+)=(.*)", line.strip())
        if match:
            actual[match.group(1)] = match.group(2)
    problems = []
    for line in defaults_text.splitlines():
        line = line.strip()
        unset = re.fullmatch(r"# (CONFIG_\w+) is not set", line)
        wanted = re.fullmatch(r"(CONFIG_\w+)=(.*)", line)
        if unset and actual.get(unset.group(1), "n") not in ("n", ""):
            problems.append(unset.group(1) + " is set")
        elif wanted and actual.get(wanted.group(1)) != wanted.group(2):
            problems.append("%s=%s, defaults %s" % (wanted.group(1), actual.get(wanted.group(1)), wanted.group(2)))
    if actual.get("CONFIG_IDF_TARGET") != '"%s"' % TARGET:
        problems.append("CONFIG_IDF_TARGET=%s" % actual.get("CONFIG_IDF_TARGET"))
    return problems


def validate_build(build):
    """Check the generated outputs that define what would be written. Reads files only."""
    build = Path(build)
    try:
        description = json.loads((build / "project_description.json").read_text(encoding="utf-8"))
        flasher = json.loads((build / "flasher_args.json").read_text(encoding="utf-8"))
        flash_args = (build / "flash_args").read_text(encoding="utf-8")
        sdkconfig = (build / "sdkconfig").read_text(encoding="utf-8")
    except (OSError, ValueError) as error:
        raise FlashError("build outputs unavailable: %s" % error, "build_invalid") from error
    if description.get("target") != TARGET:
        raise FlashError("build target is %s" % description.get("target"), "build_invalid")
    extra = flasher.get("extra_esptool_args", {})
    if (extra.get("chip"), extra.get("before"), extra.get("after")) != (TARGET, "default_reset", "hard_reset"):
        raise FlashError("generated esptool arguments differ: %s" % extra, "build_invalid")
    for part in ("bootloader", "app", "partition-table"):
        if flasher.get(part, {}).get("encrypted") != "false":
            raise FlashError("generated %s requires encryption" % part, "build_invalid")
    options, images = parse_flash_args(flash_args)
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
        if name != FLASH_LAYOUT[0x8000] and _image_chip(data, name) != ESP32S2_CHIP_ID:
            raise FlashError("%s is not built for %s" % (name, TARGET), "build_invalid")
    elf = build / (APP + ".elf")
    if not elf.is_file():
        raise FlashError("linked ELF missing", "build_invalid")
    elf_hash = hashlib.sha256(elf.read_bytes()).hexdigest()
    hashes[elf.name] = elf_hash
    hashes["sdkconfig"] = _file_hash(build / "sdkconfig")
    application = (build / FLASH_LAYOUT[0x10000]).read_bytes()
    if application[ELF_HASH_OFFSET:ELF_HASH_OFFSET + 32].hex() != elf_hash:
        raise FlashError("application image does not embed this ELF's hash", "build_invalid")
    drift = check_sdkconfig((PROJECT / "sdkconfig.defaults").read_text(encoding="utf-8"), sdkconfig)
    if drift:
        raise FlashError("sdkconfig differs from sdkconfig.defaults: " + "; ".join(drift), "build_invalid")
    return {"target": description["target"], "app_elf": elf.name, "image_sha256": hashes,
            "flash_options": options, "flash_args": flash_args, "sdk_version": description.get("git_revision"),
            "project_version": description.get("project_version"),
            "c_compiler": description.get("c_compiler")}


def compiled_revision(build):
    """The Git revision compiled into main.cpp; stale if CMake did not reconfigure."""
    entries = json.loads((Path(build) / "compile_commands.json").read_text(encoding="utf-8"))
    for entry in entries:
        if Path(entry.get("file", "")).name == "main.cpp" and "s2_bench" in entry.get("file", ""):
            match = re.search(r'MOTORCONTROLCAN_SOURCE_REVISION=\\*"([0-9a-f]{40})', entry.get("command", ""))
            return match.group(1) if match else None
    return None


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

    def __init__(self, directory, mode):
        self.directory = Path(directory)
        self.directory.mkdir(parents=True, exist_ok=False)
        self.record = {"schema": 1, "mode": mode, "status": "running", "phases": [],
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


def default_artifact(mode):
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    if mode == "flash":
        return ROOT / "docs/bench" / (stamp + "-s2-upload")
    return ROOT / ".build/s2-flash-artifacts" / (stamp + "-s2-build")


def ping_instructions(port, serial_number, artifact):
    stamp = "NEW_PING_DIRECTORY"
    return ["python scripts/s2_bench.py --port %s --serial %s --command ping --seconds 2 --output %s" % (
        port, serial_number, stamp),
            "python scripts/s2_flash.py --check-ping %s --artifact %s" % (stamp, artifact)]


# ---------------------------------------------------------------- modes
def dry_run(args):
    """Files only: no process, no USB enumeration, no port, no artifact directory."""
    tools = discover_tools(args)
    build = Path(args.build_dir).resolve()
    port = args.port or "<VERIFIED_PORT>"
    print("DRY RUN: plan only; nothing is built, enumerated, opened or written.")
    print("Recorded fixture (checked at --flash, never assumed): %s" % json.dumps(FIXTURE))
    for name, value in tools["paths"].items():
        print("  tool %-15s %s" % (name, value if value else "MISSING"))
    print("Planned steps for --flash, stopping at the first failure:")
    print("  1 create a new artifact directory; retain revision, dirty hashes, tool versions")
    print("  2 require USB %(usb_serial)s %(vid)04X:%(pid)04X on %(port)s" % FIXTURE)
    if not tools["missing"]:
        print("  3 build: " + " ".join(build_command(tools, build)))
    print("  4 validate generated target, flash_args, images, ELF hash and sdkconfig")
    if not tools["missing"]:
        read_mac = esptool_command(tools, port, "read_mac")
        write = esptool_command(tools, port, "write_flash", "@flash_args")
        print("  5 target identity: " + " ".join(read_mac))
        print("  6 write once from %s: %s" % (build, " ".join(write)))
    print("  7 verify separately:")
    for line in ping_instructions(port, args.serial or FIXTURE["usb_serial"], "<ARTIFACT>"):
        print("      " + line)
    if (build / "flash_args").is_file():
        try:
            facts = validate_build(build)
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


def _build(artifact, tools, env, build):
    """Build once; refuse an image whose source moved or whose revision is stale."""
    before = source_identity()
    artifact.record.update(before)
    artifact.phase("source_identity", dirty_files=len(before["dirty_sha256"]))
    Path(build).mkdir(parents=True, exist_ok=True)
    command = build_command(tools, build)
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
    facts = validate_build(build)
    revision = compiled_revision(build)
    if revision != before["base_commit"]:
        raise FlashError("image embeds revision %s, HEAD is %s" % (revision, before["base_commit"]), "build_invalid")
    artifact.record.update(facts)
    artifact.record["compiled_revision"] = revision
    for name in ("sdkconfig", "flash_args", "flasher_args.json", "project_description.json"):
        artifact.retain(Path(build) / name)
    artifact.phase("build", image_sha256=facts["image_sha256"])
    return facts


def _flash(artifact, tools, env, build, port, serial_number):
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
    if verified != len(FLASH_LAYOUT) or "Hard resetting" not in output:
        raise FlashError("esptool exited 0 without %d verified images and a reset" % len(FLASH_LAYOUT),
                         "flash_unverified")
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
    if flash:
        # Check first so a wrong fixture does not cost a build or an artifact.
        try:
            check_usb(args.port, args.serial, comports())
        except FlashError as error:
            print("Refused: %s" % error, file=sys.stderr)
            return 2
    try:
        artifact = Artifact(args.output or default_artifact("flash" if flash else "build"),
                            "flash" if flash else "build-only")
    except FileExistsError as error:
        print("Refused: artifact directory already exists: %s" % error.filename, file=sys.stderr)
        return 2
    build = Path(args.build_dir).resolve()
    artifact.record["build_dir"] = str(build)
    try:
        env = _record_tools(artifact, args, tools)
        _build(artifact, tools, env, build)
        if flash:
            _flash(artifact, tools, env, build, args.port, args.serial)
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
        for line in ping_instructions(args.port, args.serial, artifact.directory):
            print("  " + line)
    return 0 if artifact.record["status"] in ("built", "flashed") else 1


def check_ping(args):
    """Offline image check: nothing here touches a port."""
    build = json.loads((Path(args.artifact) / "build.json").read_text(encoding="utf-8"))
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
    parser.add_argument("--artifact", type=Path, help="Retained build record for --check-ping")
    parser.add_argument("--port", help="Required with --flash; checked against the recorded USB identity")
    parser.add_argument("--serial", help="Required with --flash: the exact USB serial")
    parser.add_argument("--build-dir", type=Path, default=ROOT / ".build/s2-bench-live",
                        help="Short build path; a Windows dependency-file limit applies")
    parser.add_argument("--output", type=Path, help="New artifact directory; must not exist")
    parser.add_argument("--idf-path", type=Path)
    parser.add_argument("--idf-python", type=Path)
    parser.add_argument("--idf-tools-path", type=Path)
    args = parser.parse_args(argv)
    if args.flash and not (args.port and args.serial):
        parser.error("--flash requires --port and --serial")
    if args.check_ping and not args.artifact:
        parser.error("--check-ping requires --artifact")
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
