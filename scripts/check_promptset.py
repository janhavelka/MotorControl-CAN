#!/usr/bin/env python3
"""Offline integrity checks for the planning package, not the future library."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
from urllib.parse import unquote


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check-live-baseline", action="store_true",
                        help="Also compare copied files with the recorded sibling checkout")
    parser.add_argument("--check-git-ref", choices=("HEAD", "INDEX"),
                        help="Also verify artifact bytes stored in Git HEAD or the staged index")
    parser.add_argument("--json-report", type=Path,
                        help="Write this planning-check result as JSON")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    errors = []
    counts = {"stages": 0, "requirements": 0, "multidrive_stage_gates": 0,
              "shared_contracts": 0, "concrete_stage_contracts": 0,
              "reference_packs": 0, "markdown_files": 0, "local_links": 0,
              "hashed_artifacts": 0, "live_baseline_files": 0, "git_artifact_files": 0}
    original_reference_paths = set()

    def require(condition, message):
        if not condition:
            errors.append(message)

    def read_json(path):
        try:
            return json.loads(path.read_text(encoding="utf-8"))
        except (OSError, ValueError) as exc:
            errors.append("{}: {}".format(path.relative_to(root), exc))
            return {}

    def check_records(base, records, label):
        seen = set()
        for record in records:
            rel = record.get("path", "")
            require(bool(rel) and rel not in seen, label + ": missing/duplicate path " + rel)
            seen.add(rel)
            path = (base / rel).resolve()
            require(base.resolve() in path.parents, label + ": path escapes base: " + rel)
            if not path.is_file():
                errors.append(label + ": missing file " + rel)
                continue
            content = path.read_bytes()
            require(len(content) == record.get("bytes"), label + ": size mismatch " + rel)
            require(hashlib.sha256(content).hexdigest() == record.get("sha256"),
                    label + ": SHA-256 mismatch " + rel)
            counts["hashed_artifacts"] += 1
            if args.check_git_ref:
                relative = path.relative_to(root).as_posix()
                prefix = "HEAD:" if args.check_git_ref == "HEAD" else ":"
                try:
                    stored = subprocess.run(
                        ["git", "cat-file", "blob", prefix + relative], cwd=root,
                        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                        timeout=10, check=False)
                    require(stored.returncode == 0,
                            "Git " + args.check_git_ref + ": missing artifact " + relative)
                    if stored.returncode == 0:
                        require(hashlib.sha256(stored.stdout).hexdigest() == record.get("sha256"),
                                "Git " + args.check_git_ref + ": SHA-256 mismatch " + relative)
                        counts["git_artifact_files"] += 1
                except (OSError, subprocess.TimeoutExpired) as exc:
                    errors.append("Git artifact check: " + str(exc))
            if path.suffix.lower() == ".pdf":
                require(content.startswith(b"%PDF-"), label + ": not a PDF " + rel)
            if path.suffix.lower() == ".eds":
                require(b"[FileInfo]" in content and b"[DeviceInfo]" in content,
                        label + ": not an expected EDS " + rel)

    refs = root / "docs/reference"
    reference_manifests = [refs / "SOURCES.json", refs / "multidrive/SOURCES.json",
                           refs / "other_can/SOURCES.json"]
    for manifest_path in reference_manifests:
        sources = read_json(manifest_path)
        originals = sources.get("sources", [])
        require(bool(originals), "Reference pack has no originals: " + str(manifest_path))
        for source in originals:
            require(bool(source.get("url")) and bool(source.get("retrieved_on")),
                    "Reference lacks provenance: " + source.get("id", "unknown"))
            if source.get("path"):
                original_reference_paths.add((manifest_path.parent / source["path"]).resolve())
        for remote in sources.get("remote_only", []):
            require(not remote.get("path") and not remote.get("sha256")
                    and remote.get("bytes") is None,
                    "Remote-only source must not claim a local file/hash: " + remote.get("id", "unknown"))
        check_records(manifest_path.parent, originals + sources.get("derived_artifacts", [])
                      + sources.get("auxiliary_artifacts", []), str(manifest_path.relative_to(root)))
        counts["reference_packs"] += 1

    baseline = root / "docs/baseline"
    snapshot = read_json(baseline / "SNAPSHOT.json")
    require(bool(snapshot.get("source_repository", {}).get("head")),
            "Baseline lacks source revision")
    check_records(baseline, snapshot.get("files", []), "baseline")
    expected_snapshot = {record["path"] for record in snapshot.get("files", [])}
    actual_snapshot = {path.relative_to(baseline).as_posix()
                       for path in (baseline / "MotorControl-RS").rglob("*") if path.is_file()}
    require(actual_snapshot == expected_snapshot, "Baseline has unexpected or unlisted files")
    require((baseline / "MotorControl-RS/LICENSE").is_file(), "Copied source lacks license")
    if args.check_live_baseline:
        source_root = Path(snapshot.get("source_repository", {}).get("local_path", ""))
        for record in snapshot.get("files", []):
            live = source_root / record["source"]
            if not live.is_file():
                errors.append("Live baseline file missing: " + str(live))
                continue
            require(hashlib.sha256(live.read_bytes()).hexdigest() == record["sha256"],
                    "Live source differs from snapshot: " + str(live))
            counts["live_baseline_files"] += 1

    evidence = root / "docs/audit/evidence/motorcontrol_rs_quick"
    evidence_manifest = read_json(evidence / "MANIFEST.json")
    check_records(evidence, evidence_manifest.get("files", []), "RS audit evidence")

    prompts = root / "docs/prompts"
    stages = sorted(prompts.glob("[0-9][0-9]_*.md"))
    counts["stages"] = len(stages)
    require([path.name[:2] for path in stages] == ["{:02d}".format(n) for n in range(20)],
            "Expected one implementation stage each from 00 through 19")
    index = (prompts / "README.md").read_text(encoding="utf-8")
    for stage in stages:
        content = stage.read_text(encoding="utf-8")
        require("[COMMON.md](COMMON.md)" in content, stage.name + ": missing recurring contract")
        require("## Audit" in content, stage.name + ": missing source audit")
        require("## Verify and exit" in content or "## Exit" in content,
                stage.name + ": missing acceptance gate")
        require("## Concrete implementation contract" in content,
                stage.name + ": missing concrete implementation contract")
        require("## Documentation and work split" in content,
                stage.name + ": missing documentation/work split")
        for contract in ("DOCUMENTATION.md", "MAINTAINABILITY.md", "DELEGATION.md"):
            require("(" + contract + ")" in content,
                    stage.name + ": missing shared contract link " + contract)
        counts["concrete_stage_contracts"] += int("## Concrete implementation contract" in content)
        require("(" + stage.name + ")" in index, stage.name + ": missing index link")
    matrix = (root / "docs/requirements_matrix.md").read_text(encoding="utf-8")
    requirement_ids = set(re.findall(r"\| (R\d\d) \|", matrix))
    counts["requirements"] = len(requirement_ids)
    require(requirement_ids == {"R{:02d}".format(n) for n in range(1, 36)},
            "Expected R01 through R35 with stage/evidence mapping")
    multidrive = (prompts / "MULTIDRIVE.md").read_text(encoding="utf-8")
    drive_stages = re.findall(r"^\| (\d\d) \|", multidrive, flags=re.MULTILINE)
    counts["multidrive_stage_gates"] = len(drive_stages)
    require(drive_stages == ["{:02d}".format(n) for n in range(20)],
            "Expected a multi-drive acceptance gate for each stage")
    require("[MULTIDRIVE.md](MULTIDRIVE.md)" in (prompts / "COMMON.md").read_text(encoding="utf-8"),
            "Common stage contract must incorporate multi-drive requirements")
    require((prompts / "ADD_DRIVE.md").is_file(), "Missing new-drive playbook")
    common = (prompts / "COMMON.md").read_text(encoding="utf-8")
    for name in ("CONVENTIONS.md", "DOCUMENTATION.md", "MAINTAINABILITY.md",
                 "DELEGATION.md", "MULTIDRIVE.md"):
        require((prompts / name).is_file(), "Missing shared contract " + name)
        require("(" + name + ")" in common, "COMMON must incorporate " + name)
        counts["shared_contracts"] += int((prompts / name).is_file())

    # Copied source docs intentionally retain original links to a partial snapshot.
    # Manufacturer HTML/PDF and full extracts are immutable third-party artifacts.
    excluded = baseline / "MotorControl-RS"
    for path in sorted(root.rglob("*.md")):
        if excluded in path.parents or path.resolve() in original_reference_paths or ".git" in path.parts:
            continue
        try:
            content = path.read_text(encoding="utf-8")
        except UnicodeError as exc:
            errors.append(str(path) + ": " + str(exc))
            continue
        counts["markdown_files"] += 1
        require("\ufffd" not in content, str(path.relative_to(root)) + ": replacement character")
        require(len(re.findall(r"^```", content, flags=re.MULTILINE)) % 2 == 0,
                str(path.relative_to(root)) + ": unmatched code fence")
        # Ignore example text inside fenced code blocks when checking links.
        prose = re.sub(r"^```[^\n]*\n.*?^```[^\n]*$", "", content,
                       flags=re.MULTILINE | re.DOTALL)
        table_columns = None
        for line_number, line in enumerate(prose.splitlines(), 1):
            if line.startswith("|"):
                columns = len(re.findall(r"(?<!\\)\|", line))
                if table_columns is not None:
                    require(columns == table_columns,
                            "{}: inconsistent table row near prose line {}".format(
                                path.relative_to(root), line_number))
                else:
                    table_columns = columns
            else:
                table_columns = None
        for target in re.findall(r"\[[^\]\n]*\]\(([^)\n]+)\)", prose):
            target = target.strip().strip("<>")
            if re.match(r"^[a-zA-Z][a-zA-Z0-9+.-]*:", target) or target.startswith("#"):
                continue
            target = unquote(target.split("#", 1)[0])
            if not target:
                continue
            counts["local_links"] += 1
            require((path.parent / target).exists(),
                    "{}: missing link {}".format(path.relative_to(root), target))

    result = {"scope": "Planning package integrity only; no CAN library/HIL validation",
              "passed": not errors, "counts": counts, "errors": errors}
    if args.json_report:
        args.json_report.parent.mkdir(parents=True, exist_ok=True)
        args.json_report.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))
    return 0 if not errors else 1


if __name__ == "__main__":
    sys.exit(main())
