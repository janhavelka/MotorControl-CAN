"""Pack finished night-soak series directories losslessly: one tar.xz per series in 20261111-night/packed/, with
packed/MANIFEST.json listing every member file's SHA-256 and size and each archive's SHA-256. Raw directories are left
in place (the soak's state may reference the latest stop record). Usage: pack_night.py [--all]"""
import hashlib
import io
import json
import lzma
import pathlib
import sys
import tarfile

NIGHT = pathlib.Path(r"C:\Users\Honza\Documents\Projects\MotorControl-CAN\docs\bench\20261111-night")
PACKED = NIGHT / "packed"
PACKED.mkdir(exist_ok=True)
manifest_path = PACKED / "MANIFEST.json"
manifest = json.loads(manifest_path.read_text(encoding="utf-8")) if manifest_path.exists() else {"archives": {}}
state = json.loads((NIGHT / "state.json").read_text(encoding="utf-8"))
current = state["series"]
done = 0
for d in sorted(p for p in NIGHT.iterdir() if p.is_dir() and p.name.startswith("S")):
    number = int(d.name[1:5])
    if number >= current and "--all" not in sys.argv:
        continue  # The series still running, or the last one.
    archive = PACKED / (d.name + ".tar.xz")
    if d.name in manifest["archives"]:
        continue
    members = {}
    buffer = io.BytesIO()
    with tarfile.open(fileobj=buffer, mode="w") as tar:
        for f in sorted(x for x in d.rglob("*") if x.is_file()):
            data = f.read_bytes()
            rel = f.relative_to(NIGHT).as_posix()
            members[rel] = {"sha256": hashlib.sha256(data).hexdigest(), "bytes": len(data)}
            info = tarfile.TarInfo(rel)
            info.size = len(data)
            info.mtime = int(f.stat().st_mtime)
            tar.addfile(info, io.BytesIO(data))
    blob = lzma.compress(buffer.getvalue(), preset=9 | lzma.PRESET_EXTREME)
    # Round trip before recording: the archive must reproduce every member byte for byte.
    with tarfile.open(fileobj=io.BytesIO(lzma.decompress(blob)), mode="r") as tar:
        for m in tar.getmembers():
            if hashlib.sha256(tar.extractfile(m).read()).hexdigest() != members[m.name]["sha256"]:
                raise SystemExit("round trip mismatch in " + m.name)
    archive.write_bytes(blob)
    manifest["archives"][d.name] = {"archive": archive.name, "sha256": hashlib.sha256(blob).hexdigest(),
                                    "bytes": len(blob), "members": members}
    done += 1
manifest_path.write_bytes((json.dumps(manifest, indent=1, sort_keys=True) + "\n").encode("utf-8"))
total = sum(a["bytes"] for a in manifest["archives"].values())
raw = sum(m["bytes"] for a in manifest["archives"].values() for m in a["members"].values())
print("packed {} new series; {} archives, {:.1f} MB from {:.1f} MB raw".format(done, len(manifest["archives"]),
                                                                            total / 1e6, raw / 1e6))
