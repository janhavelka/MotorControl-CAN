#!/usr/bin/env python3
"""Build exported and relocated installed packages outside the checkout.

Only this developer harness uses Python. The CMake subprocesses have no test,
SDK, reference or sibling files, no Python package discovery and no downloads.
"""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
HEADERS = {"MotorControlCAN/Status.h", "MotorControlCAN/Endpoint.h",
           "MotorControlCAN/can/Frame.h", "MotorControlCAN/Version.h",
           "MotorControlCAN/MotorControlCAN.h"}


def require(condition, evidence):
    if not condition:
        raise AssertionError(evidence)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--compiler", default="c++")
    args = parser.parse_args()
    cmake = str(Path(shutil.which(args.cmake) or args.cmake).resolve())
    compiler = str(Path(shutil.which(args.compiler) or args.compiler).resolve())
    ninja = shutil.which("ninja")
    if not ninja:
        raise SystemExit("Ninja is required for the package consumer check")
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
        result = subprocess.run(command, cwd=cwd, env=env, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
        if (result.returncode == 0) != success:
            raise AssertionError("Command {} returned {}:\n{}".format(command, result.returncode, result.stdout))
        return result.stdout

    with tempfile.TemporaryDirectory(prefix="MotorControlCAN package ") as directory:
        work = Path(directory)
        archive = work / "core.zip"
        # The exporter is a developer action; exported builds cannot access it.
        subprocess.run([sys.executable, str(ROOT / "scripts/export_package.py"), str(archive)], check=True)
        source = work / "exported source"
        expected = {"include/" + h for h in HEADERS} | {
            "src/Endpoint.cpp", "src/can/Frame.cpp", "CMakeLists.txt", "library.json",
            "cmake/MotorControlCANConfig.cmake.in", "LICENSE", "README.md", "CHANGELOG.md",
            "docs/getting_started.md"}
        with zipfile.ZipFile(archive) as package:
            require(set(package.namelist()) == expected, package.namelist())
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
                    "--test-dir", str(build), "--output-on-failure")
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
        # A real direct public dependency must fail in a fresh consumer build.
        (relocated / "sdk/include/MotorControlCAN/Status.h").unlink()
        failure = consumer(work / "missing header consumer", *options, success=False)
        require("Status.h" in failure, failure)
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


if __name__ == "__main__":
    main()
