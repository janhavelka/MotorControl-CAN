#!/usr/bin/env python3
"""Export the library.json allowlist as a reproducible source ZIP.

This developer command copies committed/generated source as-is. It does not
generate files, fetch dependencies or include repository tests and evidence.
"""

import argparse
import json
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def export(output):
    metadata = json.loads((ROOT / "library.json").read_text(encoding="utf-8"))
    files = set()
    for item in metadata["export"]["include"]:
        path = ROOT / item
        if not path.exists():
            raise ValueError("Missing package input: " + item)
        if path.is_dir():
            files.update(p for p in path.rglob("*") if p.is_file())
        else:
            files.add(path)
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(files, key=lambda p: p.relative_to(ROOT).as_posix()):
            info = zipfile.ZipInfo(path.relative_to(ROOT).as_posix(), (1980, 1, 1, 0, 0, 0))
            info.create_system = 3
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, path.read_bytes())
    return len(files)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    print("Exported {} files to {}".format(export(args.output), args.output))


if __name__ == "__main__":
    main()
