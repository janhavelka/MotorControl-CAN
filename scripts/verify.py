#!/usr/bin/env python3
"""Run real repository, native and package/documentation gates with retained evidence."""

import argparse
import datetime
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import time
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
DOXYGEN_VERSION = "1.13.2"
DOXYGEN_REVISION = "26342b775ea25e6fefb53220926b20702c56fcb3"


def doxygen_matches(version):
    """Official source builds append a clean Git identity to --version output."""
    if version == DOXYGEN_VERSION:
        return True
    match = re.fullmatch(re.escape(DOXYGEN_VERSION) + r" \(([0-9a-f]{7,40})\)", version)
    return bool(match and DOXYGEN_REVISION.startswith(match.group(1)))


class VerificationFailure(RuntimeError):
    """A failed or unavailable required gate; both produce nonzero exit."""

    def __init__(self, message, status="failed"):
        super().__init__(message)
        self.status = status


class Runner:
    """One run's commands, outputs and summary; never deletes existing evidence."""

    def __init__(self, base, mode):
        self.base = Path(base).resolve()
        stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
        self.base.mkdir(parents=True, exist_ok=True)
        self.directory = Path(tempfile.mkdtemp(prefix=stamp + "-" + str(os.getpid()) + "-", dir=self.base))
        self.summary = {"mode": mode, "platform": platform.platform(),
                        "python": sys.version, "root": str(ROOT),
                        "artifacts": str(self.directory), "status": "running",
                        "tools": {}, "steps": []}
        self.save()

    def save(self):
        content = json.dumps(self.summary, indent=2) + "\n"
        (self.directory / "summary.json").write_text(content, encoding="utf-8")
        (self.base / "summary.json").write_text(content, encoding="utf-8")

    def run(self, name, command, timeout=120, cwd=ROOT, env=None):
        command = [str(part) for part in command]
        log = self.directory / ("{:02d}_{}.log".format(len(self.summary["steps"]), name))
        step = {"name": name, "command": command, "cwd": str(cwd), "log": str(log)}
        start = time.monotonic()
        output = ""
        try:
            result = subprocess.run(command, cwd=cwd, env=env, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, text=True, errors="replace",
                                    timeout=timeout, check=False)
            output = result.stdout
            step.update(returncode=result.returncode, status="passed" if result.returncode == 0 else "failed")
        except FileNotFoundError as error:
            output = str(error)
            step.update(returncode=None, status="unavailable")
        except subprocess.TimeoutExpired as error:
            output = error.stdout or ""
            if isinstance(output, bytes):
                output = output.decode("utf-8", errors="replace")
            output += "\nCommand exceeded {} seconds".format(timeout)
            step.update(returncode=None, status="failed")
        except OSError as error:
            output = str(error)
            step.update(returncode=None, status="failed")
        step["seconds"] = round(time.monotonic() - start, 3)
        log.write_text(json.dumps(step, indent=2) + "\n\n" + output, encoding="utf-8")
        self.summary["steps"].append(step)
        self.save()
        print("{}: {} ({})".format(name, step["status"], log), flush=True)
        if step["status"] != "passed":
            raise VerificationFailure("{}: see {}".format(name, log), step["status"])
        return output

    def tool(self, name, executable):
        path = shutil.which(executable)
        if not path:
            # Run the missing name through the same logging/error path.
            self.run(name + "_version", [executable, "--version"], timeout=20)
        output = self.run(name + "_version", [path, "--version"], timeout=20)
        self.summary["tools"][name] = {"path": path, "version": output.splitlines()[0]}
        self.save()
        return path


def check_test_output(output):
    """CTest may report success after a disabled/skipped test; reject that evidence."""
    if re.search(r"\bSkipped\b|\bDISABLED\b|\bDisabled\b|\*\*\*Not\s*Run|OK \(skipped=|Ran 0 tests|No tests were found", output):
        raise VerificationFailure("A required test was skipped, disabled or empty")


def check_documentation(root):
    """Check that strict Doxygen actually extracted every installed public header."""
    root = Path(root)
    xml = root / ".build/doxygen/xml"
    index = ET.parse(xml / "index.xml").getroot()
    expected = {p.relative_to(root).as_posix() for p in (root / "include/MotorControlCAN").rglob("*.h")}
    found = set()
    members = set()
    functions = set()
    expected_functions = set()
    for header in expected:
        expected_functions.update(re.findall(
            r"^(?:inline )?(?:constexpr )?(?:bool|Status|const char\*)\s+(\w+)\s*\(",
            (root / header).read_text(encoding="utf-8"), flags=re.MULTILINE))
    for entry in index.findall("compound"):
        compound = ET.parse(xml / (entry.get("refid") + ".xml")).getroot().find("compounddef")
        if compound is None:
            raise VerificationFailure("Doxygen compound has no definition: " + entry.get("refid"))
        if entry.get("kind") == "file":
            location = compound.find("location").get("file").replace("\\", "/")
            if location.startswith(root.as_posix() + "/"):
                location = location[len(root.as_posix()) + 1:]
            found.add(location)
        for member in compound.findall(".//memberdef"):
            members.add(member.get("id"))
            if member.get("kind") == "function":
                functions.add(member.findtext("name"))
    if not expected or not expected.issubset(found) or not members or not expected_functions.issubset(functions):
        raise VerificationFailure("Doxygen missing public extraction: headers={}, functions={}".format(
            sorted(expected - found), sorted(expected_functions - functions)))
    return {"headers": len(expected), "members": len(members), "free_functions": len(expected_functions)}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("quick", "full"), default="quick")
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--compiler", choices=("gcc", "clang"), default="gcc")
    parser.add_argument("--sanitize", action="store_true", help="Require a Linux ASan/UBSan full build")
    args = parser.parse_args(argv)
    runner = Runner(args.build_dir or ROOT / ".build/verification" / args.mode, args.mode)
    try:
        if sys.version_info < (3, 10) or sys.flags.optimize:
            raise VerificationFailure("Verification requires Python 3.10+ without -O/PYTHONOPTIMIZE")
        runner.summary["compiler"] = args.compiler
        runner.summary["sanitize_requested"] = args.sanitize
        runner.summary["capabilities"] = {
            "clang": "available" if shutil.which("clang++") or shutil.which("clang++-18") else "unavailable",
            "linux_sanitizers": "requires --sanitize" if sys.platform.startswith("linux") else "unavailable on this host"}
        runner.summary["revision"] = runner.run("revision", ["git", "rev-parse", "HEAD"], timeout=10).strip()
        runner.summary["working_tree"] = runner.run("working_tree", ["git", "status", "--porcelain"], timeout=10).splitlines()
        cmake = runner.tool("cmake", "cmake")
        ctest = runner.tool("ctest", "ctest")
        ninja = runner.tool("ninja", "ninja")
        def compiler_tool(variable, fallback):
            candidate = os.environ.get(variable, fallback)
            name = Path(candidate).name.lower()
            matches = "clang" in name if args.compiler == "clang" else "gcc" in name or "g++" in name
            return candidate if matches else fallback

        cc = runner.tool("cc", compiler_tool("CC", "gcc" if args.compiler == "gcc" else "clang"))
        cxx = runner.tool("cxx", compiler_tool("CXX", "g++" if args.compiler == "gcc" else "clang++"))
        actual_clang = "clang" in runner.summary["tools"]["cxx"]["version"].lower()
        if actual_clang != (args.compiler == "clang"):
            raise VerificationFailure("Requested compiler family differs from the executable's version")
        runner.summary["tools"]["python"] = {"path": sys.executable, "version": platform.python_version()}
        runner.run("repository", [sys.executable, ROOT / "scripts/check_repository.py"])
        runner.run("version", [sys.executable, ROOT / "scripts/generate_version.py", "check"])
        doxygen = None
        if args.mode == "full":
            doxygen = runner.tool("doxygen", "doxygen")
            version = runner.summary["tools"]["doxygen"]["version"]
            if not doxygen_matches(version):
                raise VerificationFailure("Doxygen {} required, found {}".format(DOXYGEN_VERSION, version), "unavailable")
        build = runner.directory / "native"
        flags = ""
        if args.sanitize:
            if args.mode != "full" or not sys.platform.startswith("linux"):
                raise VerificationFailure("--sanitize requires full mode and Linux ASan/UBSan runtimes", "unavailable")
            flags = "-fsanitize=address,undefined -fno-omit-frame-pointer"
        runner.run("configure", [cmake, "-S", ROOT, "-B", build, "-G", "Ninja",
                   "-DCMAKE_BUILD_TYPE=Release", "-DBUILD_TESTING=ON", "-DCMAKE_C_COMPILER=" + cc,
                   "-DCMAKE_CXX_COMPILER=" + cxx, "-DCMAKE_MAKE_PROGRAM=" + ninja,
                   "-DPython3_EXECUTABLE=" + sys.executable, "-DCMAKE_C_FLAGS=" + flags,
                   "-DCMAKE_CXX_FLAGS=" + (flags + " -Werror").strip()])
        runner.run("build", [cmake, "--build", build, "--parallel", "2"], timeout=240)
        inventory = runner.run("test_inventory", [ctest, "--test-dir", build, "--show-only=json-v1"])
        listing = runner.directory / "ctest_inventory.json"
        listing.write_text(inventory, encoding="utf-8")
        runner.run("registration", [sys.executable, ROOT / "scripts/check_repository.py",
                   "--ctest-json", listing, "--build-dir", build])
        # Package gets its own retained evidence directory in full mode.
        command = [ctest, "--test-dir", build, "--output-on-failure"]
        command += ["-LE", "^full$"] if args.mode == "quick" else ["-E", "^package_contract$"]
        test_env = os.environ.copy()
        test_env["ASAN_OPTIONS"] = "detect_leaks=1:halt_on_error=1"
        test_env["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
        check_test_output(runner.run("contracts", command, timeout=300, env=test_env))
        check_test_output((build / "Testing/Temporary/LastTest.log").read_text(encoding="utf-8", errors="replace"))
        if args.mode == "full":
            runner.run("package", [sys.executable, ROOT / "test/package_test.py", "--cmake", cmake,
                       "--compiler", cxx, "--work-dir", runner.directory / "package"], timeout=240)
            runner.run("documentation", [doxygen, ROOT / "Doxyfile"], timeout=120)
            runner.summary["documentation"] = check_documentation(ROOT)
            retained_docs = runner.directory / "documentation"
            shutil.copytree(ROOT / ".build/doxygen", retained_docs)
            runner.summary["documentation"]["artifacts"] = str(retained_docs)
        runner.summary["status"] = "passed"
    except (VerificationFailure, OSError, ValueError, ET.ParseError) as error:
        runner.summary["status"] = getattr(error, "status", "failed")
        runner.summary["error"] = str(error)
        print(str(error), file=sys.stderr)
    finally:
        runner.save()
        print("Summary: {}".format(runner.base / "summary.json"), flush=True)
    return 0 if runner.summary["status"] == "passed" else 2 if runner.summary["status"] == "unavailable" else 1


if __name__ == "__main__":
    raise SystemExit(main())
