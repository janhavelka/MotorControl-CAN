#!/usr/bin/env python3
"""Compile the application TWAI shim against the reviewed SDK, without firmware or I/O.

Run in an exported ESP-IDF 5.5.5 environment. This explicit SDK gate is separate
from SDK-free quick/full host verification; it never downloads dependencies.
"""

import argparse
import os
import re
from pathlib import Path
import sys

from verify import ROOT, Runner, VerificationFailure
from check_firmware import sdk_environment


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--idf-path", type=Path, default=os.environ.get("IDF_PATH"))
    parser.add_argument("--idf-python", type=Path, default=Path(sys.executable))
    parser.add_argument("--build-dir", type=Path, default=ROOT / ".build/twai-sdk")
    args = parser.parse_args()
    runner = Runner(args.build_dir, "twai-sdk-compile")
    try:
        command, env = sdk_environment(args, runner, manifests=("twai_sources.json",))
        runner.tool("cmake", "cmake")
        build = runner.directory / "build"
        runner.run("configure", [*command, "-C", ROOT / "test/twai_sdk_compile",
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
