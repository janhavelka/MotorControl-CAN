#!/usr/bin/env python3
"""Build real Arduino/IDF examples and an isolated portable IDF consumer; never flash."""

import argparse
import hashlib
import json
import os
from pathlib import Path
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
                "examples/common/CanBusOwner.cpp", "examples/common/FirmwareProbe.cpp",
                "examples/common/EspProbePlatform.cpp", "examples/common/EspProbeMain.cpp",
                "adapters/twai/TwaiAdapter.cpp", "adapters/twai/IdfTwaiDriver.cpp"}
    if not required.issubset(data["sources"]):
        raise ValueError("Firmware inventory omits shared application/owner/platform/adapter")
    return data


def check_inputs(database, expected):
    """Inspect actual compiler inputs, independently of build-list success messages."""
    entries = json.loads(Path(database).read_text(encoding="utf-8"))
    if not isinstance(entries, list) or not entries:
        raise ValueError("Empty firmware compilation database")
    actual = set()
    for item in entries:
        if not isinstance(item, dict) or not isinstance(item.get("file"), str) or not item["file"]:
            raise ValueError("Invalid compiler input row: file must be a nonempty string")
        path = Path(item["file"])
        if not path.is_absolute():
            directory = item.get("directory")
            if not isinstance(directory, str) or not Path(directory).is_absolute():
                raise ValueError("Relative compiler input requires an absolute directory")
            path = Path(directory) / path
        actual.add(path.resolve())
    missing = {Path(p).resolve() for p in expected} - actual
    if missing:
        raise ValueError("Actual build omitted required inputs: " + ", ".join(str(p) for p in sorted(missing)))
    return len(entries)


def sdk_environment(args, runner):
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
    for manifest in ("twai_sources.json", "esp_platform_sources.json"):
        pin = json.loads((ROOT / "docs/adapters" / manifest).read_text(encoding="utf-8"))
        for item in pin["sources"]:
            if hashlib.sha256((sdk / item["path"]).read_bytes()).hexdigest() != item["sha256"]:
                raise VerificationFailure("SDK differs from reviewed source: " + item["path"])
    version = runner.run("idf_version", [python, sdk / "tools/idf.py", "--version"], timeout=60, env=env)
    if "ESP-IDF v5.5.5" not in version:
        raise VerificationFailure("Firmware requires ESP-IDF v5.5.5")
    runner.summary["sdk"] = pin["sdk"]
    runner.summary["sdk_python"] = str(python)
    runner.run("sdk_python_version", [python, "--version"], timeout=20)
    runner.tool("xtensa", "xtensa-esp32s3-elf-g++")
    return [python, sdk / "tools/idf.py"], env


def idf_build(runner, command, env, name, project, build, options=()):
    runner.run(name, [*command, "-C", project, "-B", build,
                     "-DSDKCONFIG=" + (build / "sdkconfig").as_posix(), "-DIDF_TARGET=esp32s3",
                     *options, "build"], timeout=600, env=env)
    images = list(build.glob("*.elf"))
    if len(images) != 1 or not (build / (images[0].stem + ".bin")).is_file():
        raise VerificationFailure(name + " did not link real ELF/BIN outputs")
    runner.summary.setdefault("firmware_artifacts", {})[name] = str(images[0])
    runner.save()


def retain_idf(runner, name, build):
    """Retain linked products/evidence, without keeping a machine-local SDK cache."""
    output = runner.directory / name
    output.mkdir()
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
    runner.summary["firmware_artifacts"][name] = str(output / next(build.glob("*.elf")).name)
    runner.save()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", choices=("arduino", "idf", "component", "all"), default="all")
    parser.add_argument("--build-dir", type=Path, default=ROOT / ".build/firmware")
    parser.add_argument("--pio", default="pio")
    parser.add_argument("--idf-path", type=Path, default=os.environ.get("IDF_PATH"))
    parser.add_argument("--idf-python", type=Path, default=Path(sys.executable))
    args = parser.parse_args()
    runner = Runner(args.build_dir, "firmware-" + args.target)
    try:
        data = firmware_inventory()
        common = [ROOT / path for path in data["sources"]]
        core = list((ROOT / "src").rglob("*.cpp"))
        runner.summary["revision"] = runner.run("revision", ["git", "rev-parse", "HEAD"], timeout=10).strip()
        runner.run("integrity", [sys.executable, ROOT / "scripts/check_promptset.py"], timeout=120)
        if args.target in ("arduino", "all"):
            version = runner.run("platformio_version", [args.pio, "--version"], timeout=30)
            if "6.1.19" not in version:
                raise VerificationFailure("Firmware requires PlatformIO 6.1.19")
            env = os.environ.copy()
            pio_build = runner.directory / "arduino"
            env["PLATFORMIO_BUILD_DIR"] = str(pio_build)
            runner.run("arduino", [args.pio, "run", "-e", "probe_cli_compile"], timeout=600, env=env)
            runner.run("arduino_inputs", [args.pio, "run", "-e", "probe_cli_compile", "-t", "compiledb"],
                       timeout=120, env=env)
            output = pio_build / "probe_cli_compile"
            runner.summary["arduino_compile_entries"] = check_inputs(output / "compile_commands.json", common + core)
            if not (output / "firmware.elf").is_file() or not (output / "firmware.bin").is_file():
                raise VerificationFailure("Arduino did not link firmware ELF/BIN")
            runner.summary.setdefault("firmware_artifacts", {})["arduino"] = str(output / "firmware.elf")
            runner.save()
        if args.target in ("idf", "component", "all"):
            command, env = sdk_environment(args, runner)
            if args.target in ("idf", "all"):
                # Deep checkout + timestamped logs can exceed the Windows SDK
                # compiler's dependency-file path limit. Build under a short
                # temporary root; preserve products and logs separately.
                with tempfile.TemporaryDirectory(prefix="mccan-idf-") as temporary:
                    build = Path(temporary) / "build"
                    idf_build(runner, command, env, "idf", ROOT / "examples/probe_idf", build)
                    runner.summary["idf_compile_entries"] = check_inputs(build / "compile_commands.json", common + core)
                    retain_idf(runner, "idf", build)
            if args.target in ("component", "all"):
                # Source directories live outside the checkout. Only the staged
                # core and consumer are visible; SDK is the intentional dependency.
                with tempfile.TemporaryDirectory(prefix="mccan-core-") as temporary:
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
                    runner.summary["component_compile_entries"] = check_inputs(build / "compile_commands.json", expected)
                    runner.summary["component_inputs"] = [str(path.relative_to(isolated)) for path in expected]
                    retain_idf(runner, "component", build)
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
