#!/usr/bin/env python3
"""Compile the application TWAI shim against the reviewed SDK, without firmware or I/O.

Run in an exported ESP-IDF 5.5.5 environment. This explicit SDK gate is separate
from SDK-free quick/full host verification; it never downloads dependencies.
"""

import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import sys

from verify import ROOT, Runner, VerificationFailure


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--idf-path", type=Path, default=os.environ.get("IDF_PATH"))
    parser.add_argument("--idf-python", type=Path, default=Path(sys.executable))
    parser.add_argument("--build-dir", type=Path, default=ROOT / ".build/twai-sdk")
    args = parser.parse_args()
    runner = Runner(args.build_dir, "twai-sdk-compile")
    try:
        if not args.idf_path:
            raise VerificationFailure("Export IDF_PATH or supply --idf-path", "unavailable")
        sdk = args.idf_path.resolve()
        python = args.idf_python.resolve()
        env = os.environ.copy()
        if env.get("IDF_PYTHON_CHECK_CONSTRAINTS", "yes") == "no":
            raise VerificationFailure("SDK dependency constraint checking must remain enabled")
        env["IDF_PATH"] = str(sdk)
        env["IDF_PYTHON_ENV_PATH"] = str(python.parent.parent)
        manifest = json.loads((ROOT / "docs/adapters/twai_sources.json").read_text(encoding="utf-8"))
        for source in manifest["sources"]:
            path = sdk / source["path"]
            if hashlib.sha256(path.read_bytes()).hexdigest() != source["sha256"]:
                raise VerificationFailure("SDK source differs from reviewed pin: " + str(path))
        runner.summary["sdk"] = manifest["sdk"]
        runner.summary["source_hashes_verified"] = len(manifest["sources"])
        runner.save()
        runner.run("python_version", [python, "--version"], timeout=20, env=env)
        version = runner.run("idf_version", [python, sdk / "tools/idf.py", "--version"],
                             timeout=60, env=env)
        if "ESP-IDF v5.5.5" not in version:
            raise VerificationFailure("Expected ESP-IDF v5.5.5")
        runner.tool("cmake", "cmake")
        runner.tool("xtensa", "xtensa-esp32s3-elf-g++")
        build = runner.directory / "build"
        runner.run("configure", [python, sdk / "tools/idf.py", "-C", ROOT / "test/twai_sdk_compile",
                                 "-B", build, "-DSDKCONFIG=" + (build / "sdkconfig").as_posix(),
                                 "-DIDF_TARGET=esp32s3", "reconfigure"], timeout=180, env=env)
        runner.run("compile", ["cmake", "--build", build, "--target", "__idf_main", "--parallel", "2"],
                   timeout=300, env=env)
        sizes = runner.run("layout", ["xtensa-esp32s3-elf-nm", "-S", "--radix=d",
                                      build / "esp-idf/main/libmain.a"], timeout=20, env=env)
        measured = re.findall(r"\d+\s+(\d+)\s+B\s+motorcontrolcan_twai_(driver|adapter)_bytes", sizes)
        if {name for _, name in measured} != {"driver", "adapter"}:
            raise VerificationFailure("SDK object layout measurements missing")
        runner.summary["object_bytes"] = {name: int(size) for size, name in measured}
        runner.summary["status"] = "passed"
        print("TWAI SDK compile passed; no firmware linked, flashed or executed.")
    except (OSError, ValueError, VerificationFailure) as error:
        runner.summary["status"] = getattr(error, "status", "failed")
        runner.summary["error"] = str(error)
        print(str(error), file=sys.stderr)
        return 1
    finally:
        runner.save()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
