#!/usr/bin/env python3
"""Offline integrity checks for the planning package, not the future library."""

import argparse
from collections import Counter
import configparser
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
                           refs / "other_can/SOURCES.json", refs / "public_code/SOURCES.json"]
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

    # Verify the research intake against original strings, not its own summary.
    # This checks transcription only; it cannot establish firmware semantics.
    intake = read_json(refs / "extracted/LC_StepDriverCAN_V1.1.object_inventory.json")
    eds_path = refs / "vendor/LC_StepDriverCAN_V1.1.eds"
    require(intake.get("source_path") == eds_path.relative_to(root).as_posix()
            and intake.get("source_sha256") == hashlib.sha256(eds_path.read_bytes()).hexdigest(),
            "EDS intake: source identity mismatch")
    eds_text = eds_path.read_text(encoding="utf-8-sig")
    eds = configparser.ConfigParser(interpolation=None)
    eds.optionxform = str
    eds.read_string(eds_text)
    section_starts = [(match.group(1), number) for number, line in enumerate(eds_text.splitlines(), 1)
                      for match in [re.fullmatch(r"\[([^\]]+)\]", line.strip())] if match]
    spans = {name: (start, section_starts[i + 1][1] - 1 if i + 1 < len(section_starts)
                     else len(eds_text.splitlines()))
             for i, (name, start) in enumerate(section_starts)}
    eds_types = {"0x0003": ("I16", 2), "0x0004": ("I32", 4),
                 "0x0005": ("U8", 1), "0x0006": ("U16", 2), "0x0007": ("U32", 4)}
    expected_headers = {name for name in eds.sections() if re.fullmatch(r"[0-9A-Fa-f]{4}", name)}
    expected_scalars = {name for name in eds.sections() if "DataType" in eds[name]}
    for key, expected in (("object_headers", expected_headers), ("scalar_entries", expected_scalars)):
        rows = intake.get(key, [])
        require(len(rows) == len(expected) and {row.get("section") for row in rows} == expected,
                "EDS intake: incomplete/duplicate " + key)
        for row in rows:
            section = row.get("section", "")
            if section not in expected:
                continue
            fields = dict(eds[section])
            address = re.fullmatch(r"([0-9A-Fa-f]{4})(?:sub([0-9A-Fa-f]+))?", section)
            index = int(address.group(1), 16)
            subindex = int(address.group(2), 16) if address.group(2) else (0 if "DataType" in fields else None)
            require(row.get("fields") == fields, "EDS intake: field mismatch " + section)
            require((row.get("index"), row.get("index_hex"), row.get("subindex"))
                    == (index, "0x{:04X}".format(index), subindex),
                    "EDS intake: address mismatch " + section)
            require((row.get("source_line_start"), row.get("source_line_end")) == spans[section],
                    "EDS intake: source lines mismatch " + section)
            if "DataType" in fields:
                require((row.get("type_name"), row.get("width_bytes")) == eds_types.get(fields["DataType"]),
                        "EDS intake: datatype mismatch " + section)
    for key, section in (("file_info", "FileInfo"), ("device_info", "DeviceInfo")):
        require(intake.get(key) == dict(eds[section]), "EDS intake: metadata mismatch " + key)
    require(intake.get("object_lists") == {key: dict(eds[key]) for key in
            ("MandatoryObjects", "ManufacturerObjects", "OptionalObjects")},
            "EDS intake: object list mismatch")
    scalar_fields = [dict(eds[name]) for name in expected_scalars]
    require(intake.get("counts") == {
        "object_indexes": len(expected_headers),
        "record_headers": sum("DataType" not in eds[name] for name in expected_headers),
        "scalar_entries": len(expected_scalars),
        "scalar_type_counts": dict(Counter(eds_types[row["DataType"]][0] for row in scalar_fields)),
        "scalar_access_counts": dict(Counter(row["AccessType"] for row in scalar_fields)),
        "scalar_pdo_mapping_counts": dict(Counter(row["PDOMapping"] for row in scalar_fields)),
        "scalars_without_default": sum("DefaultValue" not in row for row in scalar_fields)},
        "EDS intake: summary counts mismatch")

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
    sequence = read_json(prompts / "SEQUENCE.json")
    entries = sequence.get("prompts", [])
    require(sequence.get("version") == 2, "Expected active promptset version 2")
    ids = [entry.get("id") for entry in entries]
    require(bool(ids) and ids == list(range(1, len(ids) + 1)),
            "Prompt IDs must be consecutive integers starting at 1")
    names = [entry.get("file", "") for entry in entries]
    require(len(names) == len(set(names)), "Duplicate prompt filename in sequence")
    stages = [prompts / name for name in names]
    counts["stages"] = len(stages)
    actual_names = {path.name for path in prompts.glob("*.md")
                    if re.match(r"^\d+_", path.name)}
    require(actual_names == set(names),
            "Active numbered files must exactly match SEQUENCE.json (no retired stages)")
    source_stages = set()
    file_ids = dict(zip(names, ids))
    index = (prompts / "README.md").read_text(encoding="utf-8")
    progress = (prompts / "PROGRESS.md").read_text(encoding="utf-8")
    expected_rows = [str(value) for value in ids]
    index_dependencies = dict(re.findall(r"^\| (\d+) \| [^|]+ \| ([^|]+) \|$",
                                         index, flags=re.MULTILINE))
    for label, content in (("index", index), ("progress", progress)):
        rows = re.findall(r"^\| (\d+) \|", content, flags=re.MULTILINE)
        require(rows == expected_rows, label + ": prompt rows do not match numeric sequence")
    for entry, stage in zip(entries, stages):
        number = entry.get("id")
        require(bool(re.fullmatch(r"[1-9]\d*_[a-z0-9_]+\.md", stage.name))
                and stage.name.split("_", 1)[0] == str(number),
                "Invalid natural-number prompt filename: " + stage.name)
        dependencies = entry.get("depends_on", [])
        expected_dependencies = ", ".join(str(value) for value in dependencies) or "None"
        require(index_dependencies.get(str(number), "").strip() == expected_dependencies,
                stage.name + ": index prerequisites differ from sequence")
        require(len(dependencies) == len(set(dependencies))
                and all(isinstance(value, int) and value in ids and value < number
                        for value in dependencies),
                stage.name + ": dependencies must be distinct earlier prompts")
        origins = entry.get("source_stages", [])
        require(bool(origins) and all(value in {"{:02d}".format(n) for n in range(20)}
                                      for value in origins),
                stage.name + ": missing or invalid original-stage ownership")
        source_stages.update(origins)
        if not stage.is_file():
            errors.append("Missing numbered prompt: " + stage.name)
            continue
        content = stage.read_text(encoding="utf-8")
        require(content.startswith("# Prompt {} ".format(number)),
                stage.name + ": title must identify the current prompt number")
        require("[COMMON.md](COMMON.md)" in content, stage.name + ": missing recurring contract")
        for heading in ("Objective and boundaries", "Read and audit", "Implement",
                        "Verify and exit", "Documentation and delegation"):
            require("## " + heading in content, stage.name + ": missing " + heading)
        prerequisite = re.search(r"^.*Prerequisites:.*$", content, flags=re.MULTILINE)
        require(prerequisite is not None, stage.name + ": missing prerequisite line")
        if prerequisite:
            targets = re.findall(r"\]\(([^)]+)\)", prerequisite.group())
            linked_ids = [file_ids[target] for target in targets if target in file_ids]
            require(sorted(linked_ids) == sorted(dependencies),
                    stage.name + ": prerequisite links differ from sequence")
        for contract in ("CONVENTIONS.md", "DOCUMENTATION.md", "MAINTAINABILITY.md",
                         "DELEGATION.md", "MULTIDRIVE.md"):
            require(re.search(r"\]\(" + re.escape(contract) + r"(?:#[^)]*)?\)", content),
                    stage.name + ": missing shared contract link " + contract)
        require("docs/reports/prompt_{}.md".format(number) in content,
                stage.name + ": missing numbered completion report")
        require(re.search(r"commit\b", content, flags=re.IGNORECASE)
                and re.search(r"sync\b", content, flags=re.IGNORECASE),
                stage.name + ": missing commit/sync closure")
        counts["concrete_stage_contracts"] += int("## Implement" in content)
        require("(" + stage.name + ")" in index, stage.name + ": missing index link")
    require(source_stages == {"{:02d}".format(n) for n in range(20)},
            "The split must retain ownership of all original stages 00–19")

    # The harness acquires missing firmware evidence for these settings stages.
    # Requiring them first would deadlock evidence acquisition even in an acyclic graph.
    dependencies_by_id = {entry.get("id"): entry.get("depends_on", []) for entry in entries}
    evidence_ancestors = set()
    pending = list(dependencies_by_id.get(29, []))
    while pending:
        dependency = pending.pop()
        if dependency in evidence_ancestors:
            continue
        evidence_ancestors.add(dependency)
        pending.extend(dependencies_by_id.get(dependency, []))
    require(not evidence_ancestors.intersection({18, 20, 21, 22}),
            "Evidence harness 29 must not depend on firmware-gated remapping/settings 18/20-22")
    matrix = (root / "docs/requirements_matrix.md").read_text(encoding="utf-8")
    requirement_ids = set(re.findall(r"\| (R\d\d) \|", matrix))
    counts["requirements"] = len(requirement_ids)
    require(requirement_ids == {"R{:02d}".format(n) for n in range(1, 36)},
            "Expected R01 through R35 with stage/evidence mapping")
    multidrive = (prompts / "MULTIDRIVE.md").read_text(encoding="utf-8")
    drive_stages = re.findall(r"^\| (\d+) \|", multidrive, flags=re.MULTILINE)
    counts["multidrive_stage_gates"] = len(drive_stages)
    require(drive_stages == expected_rows,
            "Expected a multi-drive acceptance gate for each numbered prompt")
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
