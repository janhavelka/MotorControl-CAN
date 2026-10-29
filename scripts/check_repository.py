#!/usr/bin/env python3
"""Check the implemented package and explicit test inventory without network I/O.

Reference integrity and maintained Markdown links belong to check_promptset.py.
Pass CTest's JSON listing after configuring/building to also check real commands.
"""

import argparse
import json
from pathlib import Path
import re
import sys

from generate_version import version_header


ROOT = Path(__file__).resolve().parents[1]
EXPORTS = {
    "include/", "src/", "cmake/", "CMakeLists.txt", "library.json", "LICENSE",
    "README.md", "CHANGELOG.md", "docs/getting_started.md", "docs/units_status.md", "docs/axis_contract.md", "docs/canopen_services.md",
}
REQUIRED = {
    "CMakeLists.txt", "cmake/MotorControlCANConfig.cmake.in", "library.json",
    "LICENSE", "README.md", "CHANGELOG.md", "CONTRIBUTING.md", "SECURITY.md",
    "Doxyfile", ".clang-format", ".github/workflows/verify.yml",
    "docs/README.md", "docs/getting_started.md", "docs/verification.md", "docs/units_status.md", "docs/axis_contract.md", "docs/canopen_services.md",
    "include/MotorControlCAN/MotorControlCAN.h", "include/MotorControlCAN/Status.h",
    "include/MotorControlCAN/Endpoint.h", "include/MotorControlCAN/Version.h",
    "include/MotorControlCAN/Units.h", "src/Units.cpp", "test/units_test.cpp",
    "include/MotorControlCAN/Axis.h", "src/Axis.cpp", "test/axis_test.cpp",
    "examples/units_preview/CMakeLists.txt", "examples/units_preview/main.cpp", "examples/units_preview/README.md",
    "include/MotorControlCAN/can/Frame.h", "src/Endpoint.cpp", "src/can/Frame.cpp",
    "include/MotorControlCAN/canopen/Sdo.h", "src/canopen/Sdo.cpp",
    "test/frame_test.cpp", "test/sdo_codec_test.cpp", "test/sdo_adapter_test.cpp",
    "scripts/verify.py", "scripts/check_repository.py", "scripts/generate_version.py",
    "scripts/export_package.py", "scripts/check_promptset.py",
    "scripts/check_lichuan_ledger.py", "scripts/doxygen_filter.py",
    "scripts/check_promptset_test.py", "scripts/check_lichuan_ledger_test.py",
    "test/CMakeLists.txt", "test/core_test.cpp", "test/api_ownership_test.cpp",
    "test/multidrive_contract_test.cpp", "test/version_test.py", "test/package_test.py",
    "test/repository_test.py", "test/verification_test.py", "test/documentation_test.py",
    "test/installed_consumer/CMakeLists.txt",
    "test/installed_consumer/main.cpp", "experiments/canopen_service_fit/CMakeLists.txt",
}
CHECKS = {
    "version_generated": ("scripts/generate_version.py", ["check"]),
    "repository_checks": ("scripts/check_repository.py", []),
    "planning_integrity": ("scripts/check_promptset.py", []),
}


def read(path, errors):
    try:
        return path.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as error:
        errors.append(f"{path}: {error}")
        return ""


def metadata_checks(root, errors):
    try:
        metadata = json.loads((root / "library.json").read_text(encoding="utf-8"))
        if not isinstance(metadata, dict):
            raise ValueError("must contain a JSON object")
        expected = version_header(metadata)
    except (OSError, UnicodeError, ValueError) as error:
        errors.append(f"library.json: {error}")
        return
    if read(root / "include/MotorControlCAN/Version.h", errors) != expected:
        errors.append("Version.h is missing or stale; run python scripts/generate_version.py sync")
    for field, expected_value in (("name", "MotorControl-CAN"), ("license", "MIT"),
                                  ("platforms", "*"), ("frameworks", "*")):
        if metadata.get(field) != expected_value:
            errors.append(f"library.json: {field} must be {expected_value!r}")
    for section, expected_value in (
            ("build", {"includeDir": "include", "srcDir": "src"}),
            ("repository", {"type": "git", "url": "https://github.com/janhavelka/MotorControl-CAN.git"})):
        if metadata.get(section) != expected_value:
            errors.append(f"library.json: invalid {section} identity/layout")
    if metadata.get("headers") != ["MotorControlCAN/MotorControlCAN.h"]:
        errors.append("library.json: headers must name the implemented umbrella header")
    authors = metadata.get("authors")
    if not isinstance(authors, list) or not any(
            isinstance(author, dict) and author.get("name") == "Jan Havelka" and
            author.get("maintainer") is True for author in authors):
        errors.append("library.json: Jan Havelka must be a maintainer")
    export = metadata.get("export")
    entries = export.get("include") if isinstance(export, dict) else None
    if (not isinstance(entries, list) or any(not isinstance(item, str) for item in entries)
            or len(entries) != len(EXPORTS) or set(entries) != EXPORTS
            or set(export) != {"include"}):
        errors.append("library.json: export.include must contain exactly the reviewed core allowlist")
    else:
        for entry in entries:
            if not (root / entry).exists():
                errors.append(f"library.json: missing exported package path {entry}")


def test_sources(root):
    return sorted((root / "test").glob("*_test.cpp")), sorted(
        [*(root / "test").glob("*_test.py"), *(root / "scripts").glob("*_test.py")])


def registration_checks(root, errors):
    # This is the repository's explicit NAME/COMMAND convention, not a CMake
    # interpreter. The generated CTest inventory is checked separately below.
    cmake = read(root / "test/CMakeLists.txt", errors)
    cmake = re.sub(r"#[^\n]*", "", cmake)
    registrations = re.findall(r"add_test\s*\(\s*NAME\s+(\w+)\s+COMMAND\s+([^)]*)\)", cmake)
    by_name = dict(registrations)
    if len(by_name) != len(registrations):
        errors.append("test/CMakeLists.txt: duplicate test names")
    native, python = test_sources(root)
    for source in native:
        target = source.stem
        name = target[:-5]
        if not re.search(r"add_executable\s*\(\s*" + re.escape(target) + r"\s+" +
                         re.escape(source.name) + r"\s*\)", cmake):
            errors.append(f"test/CMakeLists.txt: missing explicit executable for {source.name}")
        if by_name.get(name, "").strip(' \n\r\t"') not in (target, "$<TARGET_FILE:" + target + ">"):
            errors.append(f"test/CMakeLists.txt: missing or wrong native registration {name}")
    for source in python:
        if not any(source.name in command for _, command in registrations):
            errors.append(f"test/CMakeLists.txt: unregistered Python test {source.relative_to(root).as_posix()}")
    for name, (script, _) in CHECKS.items():
        if Path(script).name not in by_name.get(name, ""):
            errors.append(f"test/CMakeLists.txt: missing required registration {name}")


def inventory_checks(root, listing, build, errors):
    if not isinstance(listing, dict) or not isinstance(listing.get("tests"), list):
        errors.append("CTest inventory: expected a JSON object containing tests")
        return
    tests = listing["tests"]
    if not tests or any(not isinstance(test, dict) or not isinstance(test.get("name"), str)
                        or not isinstance(test.get("command"), list)
                        or not all(isinstance(arg, str) for arg in test["command"])
                        or not isinstance(test.get("properties", []), list) for test in tests):
        errors.append("CTest inventory: missing or malformed tests/commands/properties")
        return
    by_name = {test["name"]: test for test in tests}
    if len(by_name) != len(tests):
        errors.append("CTest inventory: duplicate test names")
    for test in tests:
        labels = []
        for prop in test.get("properties", []):
            if not isinstance(prop, dict) or "name" not in prop or "value" not in prop:
                errors.append(f"CTest inventory: malformed property on {test['name']}")
                continue
            if prop["name"] == "LABELS":
                if not isinstance(prop["value"], list) or not all(isinstance(label, str) for label in prop["value"]):
                    errors.append(f"CTest inventory: malformed LABELS on {test['name']}")
                else:
                    labels.extend(prop["value"])
            if ((prop["name"] in ("DISABLED", "WILL_FAIL", "PASS_REGULAR_EXPRESSION",
                                  "SKIP_REGULAR_EXPRESSION") and prop["value"])
                    or prop["name"] == "SKIP_RETURN_CODE"):
                errors.append(f"CTest inventory: {test['name']} bypasses failures with {prop['name']}")
            if prop["name"] in ("ENVIRONMENT", "ENVIRONMENT_MODIFICATION"):
                values = prop["value"] if isinstance(prop["value"], list) else str(prop["value"]).split(";")
                if any(str(value).partition("=")[0].upper() == "PYTHONOPTIMIZE" for value in values):
                    errors.append(f"CTest inventory: {test['name']} overrides PYTHONOPTIMIZE")
        requires_full = test["name"] in ("package_contract", "documentation_contract")
        if ("full" in labels) != requires_full:
            errors.append(f"CTest inventory: full label is required only on package_contract and "
                          f"documentation_contract; wrong labels on {test['name']}")
    native, python = test_sources(root)
    suffix = ".exe" if sys.platform == "win32" else ""
    binaries = {path.stem[:-5]: build / "test" / (path.stem + suffix) for path in native}
    binaries["canopen_service_fit"] = build / "test/fit_stack" / ("canopen_service_fit" + suffix)
    for name, path in binaries.items():
        command = by_name.get(name, {}).get("command", [])
        if len(command) != 1 or Path(command[0]).resolve() != path.resolve():
            errors.append(f"CTest inventory: missing or wrong native command for {name}: expected {path}")

    def python_command(test, script, arguments):
        command = test.get("command", [])
        correct = (len(command) >= 2 and Path(command[0]).resolve() == Path(sys.executable).resolve()
                   and Path(command[1]).resolve() == script.resolve())
        if not correct or (arguments is not None and command[2:] != arguments):
            errors.append(f"CTest inventory: missing or wrong Python command for {script.relative_to(root).as_posix()}")

    for source in python:
        matches = [test for test in tests if len(test["command"]) > 1 and
                   Path(test["command"][1]).resolve() == source.resolve()]
        if len(matches) != 1:
            errors.append(f"CTest inventory: expected exactly one registration for {source.relative_to(root).as_posix()}")
            continue
        if source.name == "package_test.py":
            python_command(matches[0], source, None)
            args = matches[0]["command"][2:]
            if len(args) != 4 or args[::2] != ["--cmake", "--compiler"]:
                errors.append("CTest inventory: package_test.py requires explicit --cmake and --compiler")
        else:
            python_command(matches[0], source, [])
    for name, (script, arguments) in CHECKS.items():
        python_command(by_name.get(name, {}), root / script, arguments)


def check(root, listing=None, build=None):
    """Return actionable diagnostics; never mutate source or generated output."""
    root = Path(root).resolve()
    errors = [f"missing required repository/package file: {relative}"
              for relative in sorted(REQUIRED) if not (root / relative).is_file()]
    metadata_checks(root, errors)
    registration_checks(root, errors)
    if listing is not None:
        if build is None:
            errors.append("CTest inventory: --build-dir is required with --ctest-json")
        else:
            inventory_checks(root, listing, Path(build).resolve(), errors)
    return errors


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--ctest-json", type=Path)
    parser.add_argument("--build-dir", type=Path)
    args = parser.parse_args(argv)
    try:
        listing = json.loads(args.ctest_json.read_text(encoding="utf-8")) if args.ctest_json else None
        if args.ctest_json and not isinstance(listing, dict):
            raise ValueError("CTest inventory must be a JSON object")
        errors = check(args.root, listing, args.build_dir)
    except (OSError, UnicodeError, ValueError) as error:
        errors = [f"repository check input: {error}"]
    for error in errors:
        print(error, file=sys.stderr)
    print(f"Repository checks: {'FAIL' if errors else 'PASS'}")
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
