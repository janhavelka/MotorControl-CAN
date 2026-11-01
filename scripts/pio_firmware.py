"""PlatformIO source selection for the shared example; never opens hardware."""

import hashlib
import json
from pathlib import Path

Import("env")

root = Path(env.subst("$PROJECT_DIR"))
platform = env.PioPlatform()
platform_manifest = json.loads((Path(platform.get_dir()) / "platform.json").read_text(encoding="utf-8"))
if platform_manifest["version"] != "55.03.311":
    raise RuntimeError("Firmware requires pioarduino 55.03.311")
for package, expected in (("framework-arduinoespressif32", "3.3.11"),
                          ("framework-arduinoespressif32-libs", "5.5.5+sha.b774170ff46")):
    directory = platform.get_package_dir(package)
    if not directory:
        raise RuntimeError("Missing pinned framework package: " + package)
    manifest = json.loads((Path(directory) / "package.json").read_text(encoding="utf-8"))
    if manifest["version"] != expected:
        raise RuntimeError("Unexpected resolved firmware package: " + package)

# Match the existing optional CMake adapter's reviewed source pin and original
# byte inventory. No download or modification of the retained subset occurs.
fit = root / "experiments/canopen_service_fit"
upstream = json.loads((fit / "UPSTREAM.json").read_text(encoding="utf-8"))
if upstream["revision"] != "9b8beed8367241e96ac03f916cd5a500bcb2cf23":
    raise RuntimeError("Unexpected CANopenNode source pin")
for source in upstream["files"]:
    path = fit / source["path"]
    if hashlib.sha256(path.read_bytes()).hexdigest() != source["sha256"]:
        raise RuntimeError("CANopenNode SHA-256 mismatch: " + str(path))

inventory = json.loads((root / "examples/common/firmware_sources.json").read_text(encoding="utf-8"))
env.AppendUnique(CPPPATH=[str(root / path) for path in inventory["include_dirs"]])
env.AppendUnique(CPPPATH=[str(root / "include")])
env.Replace(COMPILATIONDB_PATH=str(Path(env.subst("$BUILD_DIR")) / "compile_commands.json"))
env.BuildSources("$BUILD_DIR/shared", str(root),
                 ["-<*>"] + ["+<" + path + ">" for path in inventory["sources"]])
