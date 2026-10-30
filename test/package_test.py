#!/usr/bin/env python3
"""Build exported and relocated installed packages outside the checkout.

Only this developer harness uses Python. The CMake subprocesses have no test,
SDK, reference or sibling files, no Python package discovery and no downloads.
"""

import argparse
import contextlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import zipfile

ROOT = Path(__file__).resolve().parents[1]
HEADERS = {"MotorControlCAN/Status.h", "MotorControlCAN/Axis.h", "MotorControlCAN/Units.h", "MotorControlCAN/Endpoint.h",
           "MotorControlCAN/can/Frame.h", "MotorControlCAN/canopen/Sdo.h",
           "MotorControlCAN/canopen/SdoTransfer.h", "MotorControlCAN/canopen/Network.h", "MotorControlCAN/Version.h",
           "MotorControlCAN/MotorControlCAN.h",
           "MotorControlCAN/profiles/lichuan_c/Types.h",
           "MotorControlCAN/profiles/lichuan_c/Objects.h",
           "MotorControlCAN/profiles/lichuan_c/Catalogue.h", "MotorControlCAN/profiles/lichuan_c/Pdo.h",
           "MotorControlCAN/canopen/Pdo.h", "MotorControlCAN/canopen/PdoState.h",
           "MotorControlCAN/ReadOperation.h", "MotorControlCAN/profiles/lichuan_c/Reads.h", "MotorControlCAN/profiles/lichuan_c/DriverSettings.h", "MotorControlCAN/profiles/lichuan_c/IoSettings.h", "MotorControlCAN/profiles/lichuan_c/Settings.h"}


def require(condition, evidence):
    if not condition:
        raise AssertionError(evidence)


def check_package(args, work, summary):
    cmake = str(Path(shutil.which(args.cmake) or args.cmake).resolve())
    # Preserve clang++ driver identity when it is a symlink to clang.
    compiler = str(Path(shutil.which(args.compiler) or args.compiler).absolute())
    ninja = shutil.which("ninja")
    require(ninja, "Ninja is required for the package consumer check")
    env = os.environ.copy()
    for name in ("CPATH", "CPLUS_INCLUDE_PATH", "CMAKE_PREFIX_PATH", "CMAKE_TOOLCHAIN_FILE",
                 "CMAKE_PROJECT_TOP_LEVEL_INCLUDES", "PYTHONPATH", "IDF_PATH"):
        env.pop(name, None)
    # Keep only compiler/runtime tool directories; CMake also disables Python lookup.
    tool_dirs = [str(Path(p).parent) for p in (cmake, compiler, ninja)]
    if os.name == "nt":
        tool_dirs.append(str(Path(os.environ["SystemRoot"]) / "System32"))
    env["PATH"] = os.pathsep.join(dict.fromkeys(tool_dirs))
    if os.name == "nt":
        require(shutil.which("python", path=env["PATH"]) is None, "Python leaked into consumer PATH")

    def run(*command, cwd=None, success=True):
        record = {"command": list(command), "cwd": str(cwd or work),
                  "expected_success": success, "status": "FAIL"}
        log = work / "logs" / "{:02d}.log".format(len(summary["commands"]) + 1)
        record["log"] = str(log.relative_to(work))
        summary["commands"].append(record)
        started = time.monotonic()
        output = ""
        try:
            result = subprocess.run(command, cwd=cwd or work, env=env, text=True,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
            record["exit"] = result.returncode
            output = result.stdout
            require((result.returncode == 0) == success,
                    "Command {} returned {}:\n{}".format(command, result.returncode, output))
            record["status"] = "PASS"
            return output
        except subprocess.TimeoutExpired as error:
            record["status"] = "TIMEOUT"
            output = error.stdout or ""
            if isinstance(output, bytes):
                output = output.decode("utf-8", errors="replace")
            raise
        except OSError as error:
            record["status"] = "UNAVAILABLE"
            output = str(error)
            raise
        finally:
            record["seconds"] = round(time.monotonic() - started, 3)
            log.write_text(json.dumps(record) + "\n" + output, encoding="utf-8")

    summary["tools"] = {name: run(path, "--version").splitlines()[0]
                        for name, path in (("cmake", cmake), ("compiler", compiler), ("ninja", ninja))}

    archive = work / "core.zip"
    # The exporter is a developer action; exported builds cannot access it.
    run(sys.executable, str(ROOT / "scripts/export_package.py"), str(archive))
    source = work / "exported source"
    expected = {"include/" + h for h in HEADERS} | {
        "src/Endpoint.cpp", "src/can/Frame.cpp", "src/Units.cpp", "src/Axis.cpp", "src/canopen/Sdo.cpp", "src/canopen/Network.cpp", "src/canopen/Pdo.cpp", "src/profiles/lichuan_c/Reads.cpp", "src/profiles/lichuan_c/Settings.cpp", "CMakeLists.txt", "library.json",
        "cmake/MotorControlCANConfig.cmake.in", "LICENSE", "README.md", "CHANGELOG.md",
        "docs/getting_started.md", "docs/units_status.md", "docs/axis_contract.md", "docs/canopen_services.md", "docs/reads.md", "docs/driver_settings.md", "docs/io_settings.md"}
    with zipfile.ZipFile(archive) as package:
        require(len(package.namelist()) == len(expected) and set(package.namelist()) == expected,
                package.namelist())
        for name in expected:
            require(package.read(name) == (ROOT / name).read_bytes(), name)
        package.extractall(source)
    fixture = work / "consumer source"
    shutil.copytree(ROOT / "test/installed_consumer", fixture)
    version = json.loads((source / "library.json").read_text(encoding="utf-8"))["version"]
    base = [cmake, "--no-warn-unused-cli", "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
            "-DCMAKE_CXX_COMPILER=" + compiler, "-DCMAKE_MAKE_PROGRAM=" + ninja,
            "-DCMAKE_DISABLE_FIND_PACKAGE_Python3=TRUE", "-DFETCHCONTENT_FULLY_DISCONNECTED=ON",
            "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF", "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF"]

    def consumer(build, *options, success=True):
        run(*base, "-S", str(fixture), "-B", str(build), *options)
        output = run(cmake, "--build", str(build), success=success)
        if success:
            run(str(Path(cmake).with_name("ctest.exe" if os.name == "nt" else "ctest")),
                "--test-dir", str(build), "--output-on-failure", "--no-tests=error")
        return output

    consumer(work / "source consumer", "-DMOTORCONTROLCAN_SOURCE_DIR=" + str(source),
             "-DMOTORCONTROLCAN_HEADERS_DIR=" + str(source / "include"))
    prefix = work / "install prefix"
    build = work / "library build"
    strict = "/WX" if Path(compiler).name.lower() == "cl.exe" else "-Werror -pedantic-errors"
    run(*base, "-S", str(source), "-B", str(build), "-DBUILD_TESTING=OFF",
        "-DCMAKE_CXX_FLAGS=" + strict, "-DCMAKE_INSTALL_PREFIX=" + str(prefix),
        "-DCMAKE_INSTALL_INCLUDEDIR=sdk/include", "-DCMAKE_INSTALL_LIBDIR=lib",
        "-DCMAKE_INSTALL_DATADIR=share")
    run(cmake, "--build", str(build))
    run(cmake, "--install", str(build))
    relocated = work / "relocated prefix"
    prefix.rename(relocated)
    installed = {p.relative_to(relocated).as_posix() for p in relocated.rglob("*") if p.is_file()}
    installed_headers = {"sdk/include/" + h for h in HEADERS}
    cmake_files = {"lib/cmake/MotorControlCAN/" + name for name in (
        "MotorControlCANConfig.cmake", "MotorControlCANConfigVersion.cmake",
        "MotorControlCANTargets.cmake", "MotorControlCANTargets-release.cmake")}
    library = "lib/MotorControlCAN.lib" if Path(compiler).name.lower() == "cl.exe" else "lib/libMotorControlCAN.a"
    expected_install = installed_headers | cmake_files | {library, "share/MotorControl-CAN/LICENSE"}
    require(installed == expected_install, installed)
    for path in relocated.rglob("*.cmake"):
        require(str(ROOT).replace("\\", "/") not in path.read_text(encoding="utf-8"), path)
    options = ("-DCMAKE_PREFIX_PATH=" + str(relocated),
               "-DMOTORCONTROLCAN_EXPECTED_VERSION=" + version,
               "-DMOTORCONTROLCAN_HEADERS_DIR=" + str(relocated / "sdk/include"))
    consumer(work / "installed consumer", *options)
    # Profile numeric metadata has no required descriptive catalogue dependency.
    catalogue = relocated / "sdk/include/MotorControlCAN/profiles/lichuan_c/Catalogue.h"
    original_catalogue = catalogue.read_bytes()
    catalogue.unlink()
    try:
        consumer(work / "no catalogue consumer", *options)
    finally:
        catalogue.write_bytes(original_catalogue)
    # A real direct public dependency must fail in a fresh consumer build.
    required_header = relocated / "sdk/include/MotorControlCAN/Status.h"
    original_header = required_header.read_bytes()
    required_header.unlink()
    try:
        failure = consumer(work / "missing header consumer", *options, success=False)
        require("Status.h" in failure, failure)
    finally:
        required_header.write_bytes(original_header)
    # Exercise the optional component CMake branch without pretending to build firmware.
    component = work / "idf boundary"
    component.mkdir()
    (component / "CMakeLists.txt").write_text(
        'cmake_minimum_required(VERSION 3.19)\nproject(ComponentBoundary LANGUAGES CXX)\n'
        'function(idf_component_register)\n'
        'cmake_parse_arguments(IDF "" "" "SRCS;INCLUDE_DIRS" ${ARGN})\n'
        'add_library(component STATIC ${IDF_SRCS})\n'
        'target_include_directories(component PUBLIC ${IDF_INCLUDE_DIRS})\n'
        'set(COMPONENT_LIB component PARENT_SCOPE)\nendfunction()\n'
        'add_subdirectory("' + source.as_posix() + '" core)\n', encoding="utf-8")
    run(*base, "-S", str(component), "-B", str(work / "idf build"))
    run(cmake, "--build", str(work / "idf build"))
    print("PASS: exported source, independent headers, C++11/C++17/no-RTTI, relocated install, missing dependency, IDF CMake boundary")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--compiler", default="c++")
    parser.add_argument("--work-dir", type=Path,
                        help="retain commands, logs, archive and builds in a new or empty directory")
    args = parser.parse_args()
    if args.work_dir:
        work = args.work_dir.resolve()
        if work.exists() and (not work.is_dir() or any(work.iterdir())):
            parser.error("--work-dir must be a new or empty directory: " + str(work))
        work.mkdir(parents=True, exist_ok=True)
        context = contextlib.nullcontext(work)
    else:
        context = tempfile.TemporaryDirectory(prefix="MotorControlCAN package ")
    with context as directory:
        work = Path(directory)
        (work / "logs").mkdir()
        summary = {"status": "FAIL", "python": sys.version, "artifact": str(work), "commands": []}
        try:
            check_package(args, work, summary)
            summary["status"] = "PASS"
        except (AssertionError, OSError, ValueError, subprocess.SubprocessError) as error:
            summary["error"] = str(error)
            print("Package check failed: " + str(error), file=sys.stderr)
        finally:
            (work / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        return 0 if summary["status"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
