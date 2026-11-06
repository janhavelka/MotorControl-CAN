#!/usr/bin/env python3
"""Build real Arduino/IDF examples and an isolated portable IDF consumer; never flash.

Targets: arduino (S3 compile fixture), idf (shared S3 compile fixture), idf-s2 (shared
S2 compile fixture), idf-s2-optak (shared S2 image with the OPTAK board configuration
and the test-only smoke hook), s2 (special S2 bench application), component (isolated
core consumer) and all (every gate above). arduino-s2 builds the S2 Arduino fixture,
which the pinned package blocks; it reports status "blocked" and is not part of all.

Every gate checks its linked ELF/BIN, the target in project_description.json (SDK
builds), the cross compiler of every compiled file, the reviewed resolved sdkconfig
values (SDK builds) and the exact set of checkout files it compiled; SDK, toolchain
and build-directory files are outside that set. The s2 gate leaves the bench
project's own source list to its CMakeLists and requires only its entry and raw
collector there.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import sys
import tempfile

from verify import ROOT, Runner, VerificationFailure


def firmware_inventory(root=ROOT):
    """Validate the finite shared build inputs before invoking either build system."""
    root = Path(root).resolve()
    data = json.loads((root / "examples/common/firmware_sources.json").read_text(encoding="utf-8"))
    if not isinstance(data, dict) or set(data) != {"sources", "include_dirs"}:
        raise ValueError("Firmware source inventory requires sources and include_dirs")
    for key in data:
        entries = data[key]
        if not isinstance(entries, list) or not entries or any(not isinstance(x, str) for x in entries):
            raise ValueError("Invalid firmware inventory " + key)
        if len(set(entries)) != len(entries):
            raise ValueError("Duplicate firmware " + key)
        for value in entries:
            path = (root / value).resolve()
            if path == root or root not in path.parents or not path.exists():
                raise ValueError("Missing/outside firmware input: " + value)
            if (key == "sources") != path.is_file():
                raise ValueError("Invalid firmware input kind: " + value)
    required = {"examples/common/ProbeApp.cpp", "examples/common/ProbePlatform.cpp",
                "examples/common/DeviceEvidence.cpp",
                "adapters/owner/CanBusOwner.cpp", "examples/common/FirmwareProbe.cpp",
                "examples/common/EspProbePlatform.cpp", "examples/common/EspProbeMain.cpp",
                "examples/common/EspTransceiverPin.cpp",
                "adapters/twai/TwaiAdapter.cpp", "adapters/twai/IdfTwaiDriver.cpp"}
    if not required.issubset(data["sources"]):
        raise ValueError("Firmware inventory omits shared application/owner/platform/adapter")
    return data


def compiled_inputs(database):
    """Rows of an actual compilation database as (resolved file, row)."""
    entries = json.loads(Path(database).read_text(encoding="utf-8"))
    if not isinstance(entries, list) or not entries:
        raise ValueError("Empty firmware compilation database")
    rows = []
    for item in entries:
        if not isinstance(item, dict) or not isinstance(item.get("file"), str) or not item["file"]:
            raise ValueError("Invalid compiler input row: file must be a nonempty string")
        path = Path(item["file"])
        if not path.is_absolute():
            directory = item.get("directory")
            if not isinstance(directory, str) or not Path(directory).is_absolute():
                raise ValueError("Relative compiler input requires an absolute directory")
            path = Path(directory) / path
        rows.append((path.resolve(), item))
    return rows


def within(path, directories):
    """True when path is one of directories or lies below one of them (all resolved)."""
    return any(path == item or item in path.parents for item in directories)


def sdk_roots(env):
    """The SDK and tool directories a build reads from, wherever they are installed. CI installs
    ESP-IDF inside the checkout (.build/esp-idf), so these must be excluded from the exact
    checkout-input check by location, not assumed to lie outside it."""
    roots = []
    for name in ("IDF_PATH", "IDF_TOOLS_PATH", "PLATFORMIO_CORE_DIR"):
        value = env.get(name)
        if value:
            roots.append(Path(value).resolve())
    return roots


def check_inputs(database, expected, forbidden=(), owned=None, outside=()):
    """Inspect actual compiler inputs, independently of build-list success messages.

    forbidden names files or directories that must not be compiled into this image,
    such as the special bench application or a test-only source. owned names the
    directory, normally the checkout, whose compiled files must equal expected exactly;
    SDK, toolchain and package files lie outside it and are not checked. outside lists
    directories within owned that the exactness check skips: the build directory, whose
    generated sources belong to the SDK, or a project whose own sources a gate names
    only in part. Without owned only the presence of expected inputs is checked."""
    rows = compiled_inputs(database)
    actual = {path for path, _ in rows}
    wanted = {Path(p).resolve() for p in expected}
    missing = wanted - actual
    if missing:
        raise ValueError("Actual build omitted required inputs: " + ", ".join(str(p) for p in sorted(missing)))
    blocked = [Path(p).resolve() for p in forbidden]
    extra = sorted(path for path in actual if within(path, blocked))
    if extra:
        raise ValueError("Actual build compiled forbidden inputs: " + ", ".join(str(p) for p in extra))
    if owned is not None:
        root = Path(owned).resolve()
        skipped = [Path(p).resolve() for p in outside]
        unexpected = sorted(path for path in actual - wanted if root in path.parents and not within(path, skipped))
        if unexpected:
            raise ValueError("Actual build compiled unexpected checkout inputs: "
                             + ", ".join(str(p) for p in unexpected))
    return len(rows)


def check_compiler(database, target):
    """Every compiled row must use the selected MCU's cross compiler, not a host or other target."""
    prefix = "xtensa-" + target + "-elf-"
    for path, item in compiled_inputs(database):
        arguments = item.get("arguments")
        if isinstance(arguments, list) and arguments and isinstance(arguments[0], str):
            compiler = arguments[0]
        elif isinstance(item.get("command"), str) and item["command"].strip():
            compiler = shlex.split(item["command"], posix=False)[0].strip('"')
        else:
            raise ValueError("Compiler input row has no command: " + str(path))
        if not Path(compiler.replace("\\", "/")).name.lower().startswith(prefix):
            raise ValueError("{} was compiled by {}, not {}*".format(path, compiler, prefix))


NOT_SET = None  # '# CONFIG_X is not set' must be present: the option exists and is disabled.
UNDECLARED = "<undeclared>"  # The key must be absent: no selected component declares it.


def sdkconfig_values(path):
    """CONFIG_ assignments of a resolved sdkconfig; '# CONFIG_X is not set' maps to NOT_SET."""
    values = {}
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line.startswith("# CONFIG_") and line.endswith(" is not set"):
            values[line[2:-len(" is not set")]] = NOT_SET
        elif line.startswith("CONFIG_") and "=" in line:
            key, value = line.split("=", 1)
            values[key] = value
    return values


def check_sdkconfig(path, required):
    """The resolved configuration must carry every reviewed value exactly.

    NOT_SET requires the '# CONFIG_X is not set' line, so an absent or misspelled key
    fails instead of passing as disabled. UNDECLARED requires the key to be absent, as
    for CONFIG_SPIRAM in S3 builds that select no esp_psram component; use it only for
    such a reviewed absence."""
    values = sdkconfig_values(path)

    def shown(value):
        return "not set" if value is NOT_SET else "absent" if value == UNDECLARED else repr(value)

    wrong = ["{}={} (requires {})".format(key, shown(values.get(key, UNDECLARED)), shown(value))
             for key, value in sorted(required.items()) if values.get(key, UNDECLARED) != value]
    if wrong:
        raise VerificationFailure("Resolved sdkconfig differs from the selected board: " + "; ".join(wrong))


def check_description(build, target):
    """project_description.json names the target the SDK actually configured."""
    description = json.loads((Path(build) / "project_description.json").read_text(encoding="utf-8"))
    if description.get("target") != target:
        raise VerificationFailure("Build linked target {!r}, not {}".format(description.get("target"), target))
    return description["target"]


# Checkout-relative, resolved against ROOT when used.
SHARED_IDF = "examples/probe_idf"
SMOKE_SOURCE = "examples/common/FirmwareSmoke.cpp"
BENCH_PROJECT = "examples/s2_bench"
ARDUINO_ENTRY = "examples/probe_cli/main.cpp"
COMMON_SDKCONFIG = {"CONFIG_ESP_CONSOLE_NONE": "y", "CONFIG_FREERTOS_SMP": NOT_SET, "CONFIG_PM_ENABLE": NOT_SET,
                    "CONFIG_TWAI_ISR_CACHE_SAFE": NOT_SET, "CONFIG_TWAI_ISR_IN_IRAM": NOT_SET}
FIXTURE_SDKCONFIG = {"CONFIG_MOTORCONTROLCAN_COMPILE_FIXTURE": "y"}
S2_FLASH = {"CONFIG_IDF_TARGET": '"esp32s2"', "CONFIG_ESPTOOLPY_FLASHSIZE": '"4MB"',
            "CONFIG_ESPTOOLPY_FLASHMODE_DIO": "y", "CONFIG_SPIRAM": NOT_SET}
# sdkconfig.defaults.esp32s3: the N8 no-PSRAM fixture. These S3 builds select no esp_psram
# component, so the pinned SDK writes no CONFIG_SPIRAM line at all.
S3_FLASH = {"CONFIG_IDF_TARGET": '"esp32s3"', "CONFIG_ESPTOOLPY_FLASHSIZE": '"8MB"', "CONFIG_SPIRAM": UNDECLARED}
# The isolated consumer's reviewed C++ settings (test/idf_consumer/sdkconfig.defaults).
COMPONENT_SDKCONFIG = {"CONFIG_IDF_TARGET": '"esp32s3"', "CONFIG_COMPILER_CXX_EXCEPTIONS": NOT_SET,
                       "CONFIG_COMPILER_CXX_RTTI": NOT_SET}
OPTAK_S2 = {"CONFIG_MOTORCONTROLCAN_COMPILE_FIXTURE": NOT_SET, "CONFIG_MOTORCONTROLCAN_CAN_TX_PIN": "17",
            "CONFIG_MOTORCONTROLCAN_CAN_RX_PIN": "16", "CONFIG_MOTORCONTROLCAN_CAN_BITRATE": "1000000",
            "CONFIG_MOTORCONTROLCAN_CAN_RX_DEPTH": "16", "CONFIG_MOTORCONTROLCAN_CAN_SILENT_PIN": "18",
            "CONFIG_MOTORCONTROLCAN_CAN_SILENT_LEVEL": "1", "CONFIG_MOTORCONTROLCAN_SMOKE_HOOK": "y",
            "CONFIG_MOTORCONTROLCAN_SMOKE_NODE_A": "1", "CONFIG_MOTORCONTROLCAN_SMOKE_NODE_B": "2",
            "CONFIG_MOTORCONTROLCAN_SMOKE_IDENTITY_U8": "y"}
# The shared application built per target and board configuration. The S2 OPTAK image
# adds only its application configuration and the test-only smoke source, never the
# special bench main; production fixtures must not compile the smoke source.
SHARED_IDF_BUILDS = {
    "idf": {"target": "esp32s3", "defaults": (), "extra": (), "forbidden": (SMOKE_SOURCE,),
            "sdkconfig": dict(COMMON_SDKCONFIG, **dict(S3_FLASH, **FIXTURE_SDKCONFIG))},
    "idf-s2": {"target": "esp32s2", "defaults": (), "extra": (), "forbidden": (SMOKE_SOURCE,),
               "sdkconfig": dict(COMMON_SDKCONFIG, **dict(S2_FLASH, **FIXTURE_SDKCONFIG))},
    "idf-s2-optak": {"target": "esp32s2",
                     "defaults": ("sdkconfig.defaults", "sdkconfig.optak_s2", "sdkconfig.optak_s2_smoke"),
                     "extra": (SMOKE_SOURCE,), "forbidden": (),
                     "sdkconfig": dict(COMMON_SDKCONFIG, **dict(S2_FLASH, **OPTAK_S2))},
}
# The special bench image's examples/s2_bench/sdkconfig.defaults, reviewed as for the shared S2 image.
BENCH_SDKCONFIG = dict(COMMON_SDKCONFIG, **S2_FLASH)
ARDUINO_BUILDS = {"arduino": ("probe_cli_compile", "esp32s3"), "arduino-s2": ("probe_cli_s2_compile", "esp32s2")}
# EspProbePlatform's guard text: the pinned Arduino S2 libraries enable the UART0 console.
ARDUINO_S2_GUARD = "S2 UART0 platform requires the SDK console disabled for exclusive ownership"
ALL_TARGETS = ("arduino", "idf", "idf-s2", "idf-s2-optak", "s2", "component")


def sdk_environment(args, runner, manifests=("twai_sources.json", "esp_platform_sources.json"),
                    target="esp32s3"):
    """Check the selected SDK scope without resolving away its Python virtualenv."""
    if not args.idf_path:
        raise VerificationFailure("Export IDF_PATH or supply --idf-path", "unavailable")
    sdk = args.idf_path.resolve()
    # Linux virtualenv executables are normally symlinks into the base Python.
    # Keep the selected environment's lexical path for IDF's dependency check.
    python = Path(os.path.abspath(args.idf_python))
    env = os.environ.copy()
    if env.get("IDF_PYTHON_CHECK_CONSTRAINTS", "yes").lower() in ("0", "n", "no"):
        raise VerificationFailure("SDK dependency checks must remain enabled")
    env["IDF_PATH"] = str(sdk)
    env["IDF_PYTHON_ENV_PATH"] = str(python.parent.parent)
    verified = 0
    for number, manifest in enumerate(manifests):
        pin = json.loads((ROOT / "docs/adapters" / manifest).read_text(encoding="utf-8"))
        if number == 0:
            runner.summary["sdk"] = pin["sdk"]
        for item in pin["sources"]:
            if hashlib.sha256((sdk / item["path"]).read_bytes()).hexdigest() != item["sha256"]:
                raise VerificationFailure("SDK differs from reviewed source: " + item["path"])
            verified += 1
    version = runner.run("idf_version", [python, sdk / "tools/idf.py", "--version"], timeout=60, env=env)
    if "ESP-IDF v5.5.5" not in version:
        raise VerificationFailure("Firmware requires ESP-IDF v5.5.5")
    runner.summary["source_hashes_verified"] = verified
    runner.summary["sdk_python"] = str(python)
    runner.run("sdk_python_version", [python, "--version"], timeout=20)
    runner.tool("xtensa", "xtensa-" + target + "-elf-g++")
    return [python, sdk / "tools/idf.py"], env


def idf_build(runner, command, env, name, project, build, options=(), target="esp32s3"):
    failure = None
    try:
        runner.run(name, [*command, "-C", project, "-B", build,
                         "-DSDKCONFIG=" + (build / "sdkconfig").as_posix(), "-DIDF_TARGET=" + target,
                         *options, "build"], timeout=600, env=env)
        images = list(build.glob("*.elf"))
        if len(images) != 1 or not (build / (images[0].stem + ".bin")).is_file():
            raise VerificationFailure(name + " did not link real ELF/BIN outputs")
    except Exception as error:
        failure = error
        raise
    finally:
        # Preserve resolved configuration and SDK diagnostics before the short
        # temporary build is removed, including when no image was produced.
        try:
            retain_idf(runner, name, build, failed=failure is not None)
        except OSError as error:
            if failure is None:
                raise
            runner.summary.setdefault("evidence_errors", {})[name] = str(error)


def retain_idf(runner, name, build, failed=False):
    """Retain linked products/evidence, without keeping a machine-local SDK cache."""
    output = runner.directory / name
    output.mkdir(exist_ok=True)
    for pattern in ("*.elf", "*.bin", "*.map", "compile_commands.json", "project_description.json", "sdkconfig"):
        for path in build.glob(pattern):
            shutil.copy2(path, output / path.name)
    for directory in ("bootloader", "partition_table"):
        for path in (build / directory).glob("*.bin"):
            target = output / directory / path.name
            target.parent.mkdir(exist_ok=True)
            shutil.copy2(path, target)
    if (build / "log").is_dir():
        shutil.copytree(build / "log", output / "log")
    if failed:
        for relative in ("CMakeCache.txt", "CMakeFiles/CMakeConfigureLog.yaml", "CMakeFiles/CMakeError.log"):
            path = build / relative
            if path.is_file():
                target = output / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, target)
    runner.summary.setdefault("sdk_build_evidence", {})[name] = str(output)
    images = list(output.glob("*.elf"))
    if len(images) == 1:
        runner.summary.setdefault("firmware_artifacts", {})[name] = str(images[0])
    runner.save()


def arduino_build(runner, pio, name, expected):
    """One PlatformIO environment: link, compiler inputs, selected MCU and the Arduino core."""
    environment, target = ARDUINO_BUILDS[name]
    env = os.environ.copy()
    pio_build = runner.directory / name
    env["PLATFORMIO_BUILD_DIR"] = str(pio_build)
    try:
        runner.run(name, [pio, "run", "-e", environment], timeout=600, env=env)
    except VerificationFailure as error:
        log = Path(runner.summary["steps"][-1]["log"])
        text = log.read_text(encoding="utf-8", errors="replace") if log.is_file() else ""
        if name == "arduino-s2" and ARDUINO_S2_GUARD in text:
            runner.summary["arduino_s2_blocker"] = {
                "guard": ARDUINO_S2_GUARD, "log": str(log),
                "cause": "pinned framework-arduinoespressif32-libs esp32s2 sdkconfig enables "
                         "CONFIG_ESP_CONSOLE_UART on UART0; see docs/firmware_examples.md"}
            raise VerificationFailure("Arduino S2 blocked by the pinned package's UART0 console; see "
                                      + str(log), "blocked") from error
        raise
    runner.run(name + "_inputs", [pio, "run", "-e", environment, "-t", "compiledb"], timeout=120, env=env)
    output = pio_build / environment
    database = output / "compile_commands.json"
    # Exact checkout inputs: the shared inventory, the core and the thin Arduino entry.
    runner.summary[name.replace("-", "_") + "_compile_entries"] = check_inputs(
        database, list(expected) + [ROOT / ARDUINO_ENTRY], [ROOT / BENCH_PROJECT, ROOT / SMOKE_SOURCE],
        owned=ROOT, outside=[pio_build] + sdk_roots(env))
    check_compiler(database, target)
    if not any(path.as_posix().endswith("cores/esp32/main.cpp") for path, _ in compiled_inputs(database)):
        raise VerificationFailure(name + " did not compile the Arduino core entry")
    if not (output / "firmware.elf").is_file() or not (output / "firmware.bin").is_file():
        raise VerificationFailure("Arduino did not link firmware ELF/BIN")
    runner.summary.setdefault("firmware_targets", {})[name] = target
    runner.summary.setdefault("firmware_artifacts", {})[name] = str(output / "firmware.elf")
    runner.save()


def shared_idf_build(runner, command, env, name, temporary_root, expected):
    """The shared probe_idf application for one target and board configuration."""
    spec = SHARED_IDF_BUILDS[name]
    project = ROOT / SHARED_IDF
    options = []
    if spec["defaults"]:
        options.append("-DSDKCONFIG_DEFAULTS=" + ";".join((project / item).as_posix() for item in spec["defaults"]))
    # Deep checkout + timestamped logs can exceed the Windows SDK compiler's
    # dependency-file path limit. Build under a short temporary root; preserve
    # products and logs separately.
    with tempfile.TemporaryDirectory(prefix="mccan-" + name + "-", dir=temporary_root) as temporary:
        build = Path(temporary) / "build"
        idf_build(runner, command, env, name, project, build, options, target=spec["target"])
        database = build / "compile_commands.json"
        runner.summary[name.replace("-", "_") + "_compile_entries"] = check_inputs(
            database, expected + [project / "main/main.cpp"] + [ROOT / item for item in spec["extra"]],
            [ROOT / BENCH_PROJECT] + [ROOT / item for item in spec["forbidden"]], owned=ROOT,
            outside=[build] + sdk_roots(env))
        check_compiler(database, spec["target"])
        check_sdkconfig(build / "sdkconfig", spec["sdkconfig"])
        runner.summary.setdefault("firmware_targets", {})[name] = check_description(build, spec["target"])
        runner.save()


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--target", choices=("arduino", "arduino-s2", "idf", "idf-s2", "idf-s2-optak", "s2",
                                             "component", "all"), default="all")
    parser.add_argument("--build-dir", type=Path, default=ROOT / ".build/firmware")
    parser.add_argument("--pio", default="pio")
    parser.add_argument("--idf-path", type=Path, default=os.environ.get("IDF_PATH"))
    parser.add_argument("--idf-python", type=Path, default=Path(sys.executable))
    parser.add_argument("--temporary-root", type=Path,
                        help="Existing short path for disposable IDF builds; products are retained")
    args = parser.parse_args()
    selected = ALL_TARGETS if args.target == "all" else (args.target,)
    runner = Runner(args.build_dir, "firmware-" + args.target)
    try:
        data = firmware_inventory()
        common = [ROOT / path for path in data["sources"]]
        core = list((ROOT / "src").rglob("*.cpp"))
        runner.summary["revision"] = runner.run("revision", ["git", "rev-parse", "HEAD"], timeout=10).strip()
        runner.run("integrity", [sys.executable, ROOT / "scripts/check_promptset.py"], timeout=120)
        # Every image consumes this one inventory; recorded for cross-build parity.
        runner.summary["shared_inputs"] = sorted(data["sources"])
        arduino = [name for name in selected if name in ARDUINO_BUILDS]
        if arduino:
            version = runner.run("platformio_version", [args.pio, "--version"], timeout=30)
            if "6.1.19" not in version:
                raise VerificationFailure("Firmware requires PlatformIO 6.1.19")
            for name in arduino:
                arduino_build(runner, args.pio, name, common + core)
        sdk_targets = []
        for name in selected:
            target = (SHARED_IDF_BUILDS[name]["target"] if name in SHARED_IDF_BUILDS
                      else "esp32s2" if name == "s2" else "esp32s3" if name == "component" else None)
            if target and target not in sdk_targets:
                sdk_targets.append(target)
        if sdk_targets:
            command, env = sdk_environment(args, runner, target=sdk_targets[0])
            for target in sdk_targets[1:]:
                runner.tool("xtensa_" + target[5:], "xtensa-" + target + "-elf-g++")
            for name in selected:
                if name in SHARED_IDF_BUILDS:
                    shared_idf_build(runner, command, env, name, args.temporary_root, common + core)
            if "s2" in selected:
                with tempfile.TemporaryDirectory(prefix="mccan-s2-", dir=args.temporary_root) as temporary:
                    build = Path(temporary) / "build"
                    project = ROOT / BENCH_PROJECT
                    idf_build(runner, command, env, "s2", project, build, target="esp32s2")
                    database = build / "compile_commands.json"
                    # Exact outside the bench project, whose own source list it owns.
                    runner.summary["s2_compile_entries"] = check_inputs(
                        database, common + core + [project / "main/main.cpp", project / "RawEvidence.cpp"],
                        owned=ROOT, outside=[build, project] + sdk_roots(env))
                    check_compiler(database, "esp32s2")
                    check_sdkconfig(build / "sdkconfig", BENCH_SDKCONFIG)
                    runner.summary["s2_target"] = check_description(build, "esp32s2")
                    runner.summary.setdefault("firmware_targets", {})["s2"] = runner.summary["s2_target"]
            if "component" in selected:
                # Source directories live outside the checkout. Only the staged
                # core and consumer are visible; SDK is the intentional dependency.
                with tempfile.TemporaryDirectory(prefix="mccan-core-", dir=args.temporary_root) as temporary:
                    isolated = Path(temporary)
                    component = isolated / "MotorControlCAN"
                    component.mkdir()
                    for file in ("CMakeLists.txt", "library.json"):
                        shutil.copy2(ROOT / file, component / file)
                    for directory in ("include", "src"):
                        shutil.copytree(ROOT / directory, component / directory)
                    consumer = isolated / "consumer"
                    shutil.copytree(ROOT / "test/idf_consumer", consumer)
                    build = isolated / "build"
                    idf_build(runner, command, env, "component", consumer, build,
                              ["-DMOTORCONTROLCAN_COMPONENT_PATH=" + component.as_posix()])
                    expected = list((component / "src").rglob("*.cpp")) + [consumer / "main/main.cpp"]
                    database = build / "compile_commands.json"
                    # Exactly the staged core and consumer, and nothing from the checkout
                    # (skipped only if the caller placed the temporary root inside it).
                    runner.summary["component_compile_entries"] = check_inputs(
                        database, expected, [] if ROOT in isolated.resolve().parents else [ROOT],
                        owned=isolated, outside=[build])
                    runner.summary["component_inputs"] = [str(path.relative_to(isolated)) for path in expected]
                    check_compiler(database, "esp32s3")
                    check_sdkconfig(build / "sdkconfig", COMPONENT_SDKCONFIG)
                    runner.summary.setdefault("firmware_targets", {})["component"] = check_description(
                        build, "esp32s3")
        runner.summary["status"] = "passed"
        print("Firmware compile/link gates passed; no flashing, ports or physical traffic.")
    except (OSError, ValueError, KeyError, VerificationFailure) as error:
        runner.summary["status"] = getattr(error, "status", "failed")
        runner.summary["error"] = str(error)
        print(str(error), file=sys.stderr)
    finally:
        runner.save()
    return 0 if runner.summary["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
