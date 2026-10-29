#!/usr/bin/env python3
"""Sync or check the public version header using library.json as the source."""

import argparse
import json
from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]


def version_header(metadata):
    """Validate the package version and render its deterministic C++11 header."""
    version = metadata.get("version") if isinstance(metadata, dict) else None
    if not isinstance(version, str) or not re.fullmatch(
            r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)", version):
        raise ValueError("library.json version must be MAJOR.MINOR.PATCH without leading zeros")
    major, minor, patch = map(int, version.split("."))
    if major > 65535 or minor > 99 or patch > 99:
        raise ValueError("version exceeds VERSION_CODE limits: major <= 65535, minor/patch <= 99")
    return (
        "/**\n"
        " * @file Version.h\n"
        " * @brief Package version generated from library.json. Do not edit.\n"
        " */\n"
        "#pragma once\n\n"
        "#include <stdint.h>\n\n"
        "namespace MotorControlCAN {\n\n"
        "/** @brief Package major version, in the range 0 through 65535. */\n"
        f"static constexpr uint16_t VERSION_MAJOR = {major};\n\n"
        "/** @brief Package minor version, in the range 0 through 99. */\n"
        f"static constexpr uint16_t VERSION_MINOR = {minor};\n\n"
        "/** @brief Package patch version, in the range 0 through 99. */\n"
        f"static constexpr uint16_t VERSION_PATCH = {patch};\n\n"
        "/** @brief Ordered numeric version: major * 10000 + minor * 100 + patch. */\n"
        f"static constexpr uint32_t VERSION_CODE = {major * 10000 + minor * 100 + patch};\n\n"
        "/** @brief Package MAJOR.MINOR.PATCH text with static storage duration. */\n"
        f'static constexpr const char* VERSION = "{version}";\n\n'
        "}  // namespace MotorControlCAN\n"
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("sync", "check"), nargs="?", default="sync")
    args = parser.parse_args()
    target = ROOT / "include/MotorControlCAN/Version.h"
    try:
        output = version_header(json.loads((ROOT / "library.json").read_text(encoding="utf-8")))
        current = target.read_text(encoding="utf-8") if target.exists() else None
        if current == output:
            print("Version.h is current")
        elif args.command == "check":
            print("Version.h is missing or stale; run python scripts/generate_version.py sync",
                  file=sys.stderr)
            return 1
        else:
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(output.encode("utf-8"))
            print("Updated Version.h")
    except (OSError, ValueError) as error:
        print(f"Version generation failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
