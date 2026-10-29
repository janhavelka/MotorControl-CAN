#!/usr/bin/env python3
"""Focused regression checks for prompt sequence and completion validation."""

import contextlib
import importlib.util
import io
import json
from pathlib import Path
import re
import sys
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
SEQUENCE = ROOT / "docs/prompts/SEQUENCE.json"
UPSTREAM = ROOT / "experiments/canopen_service_fit/UPSTREAM.json"
SPEC = importlib.util.spec_from_file_location("planning_checker", ROOT / "scripts/check_promptset.py")
CHECKER = importlib.util.module_from_spec(SPEC)
# Keep these read-only tests from creating a local __pycache__ artifact.
previous_bytecode = sys.dont_write_bytecode
try:
    sys.dont_write_bytecode = True
    SPEC.loader.exec_module(CHECKER)
finally:
    sys.dont_write_bytecode = previous_bytecode


class PromptSequenceTests(unittest.TestCase):
    def test_only_root_development_output_is_excluded_from_links(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for directory in (".build", "build", ".install", "dist", "docs/build", "docs/reports"):
                path = root / directory / "page.md"
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("[broken](missing.md)", encoding="utf-8")
            found = {path.relative_to(root).as_posix() for path in CHECKER.maintained_markdown(root)}
            self.assertEqual(found, {"docs/build/page.md", "docs/reports/page.md"})

    def check_with(self, replacements=None, missing_files=()):
        replacements = replacements or {}
        original_read = Path.read_text
        original_is_file = Path.is_file

        def read(path, *args, **kwargs):
            if path.resolve() in replacements:
                return replacements[path.resolve()]
            return original_read(path, *args, **kwargs)

        def is_file(path):
            return path.resolve() not in missing_files and original_is_file(path)

        output = io.StringIO()
        with patch.object(Path, "read_text", read), patch.object(Path, "is_file", is_file), \
                patch.object(sys, "argv", ["check_promptset.py"]):
            with contextlib.redirect_stdout(output):
                code = CHECKER.main()
        return code, json.loads(output.getvalue())

    def sequence_change(self, change):
        sequence = json.loads(SEQUENCE.read_text(encoding="utf-8"))
        change(sequence["prompts"])
        return {SEQUENCE: json.dumps(sequence)}

    def assert_rejected(self, changes, message):
        code, result = self.check_with(changes)
        self.assertEqual(code, 1)
        self.assertFalse(result["passed"])
        self.assertTrue(any(message in error for error in result["errors"]), result["errors"])

    def test_current_sequence_passes(self):
        code, result = self.check_with()
        self.assertEqual(code, 0, result["errors"])
        self.assertEqual(result["counts"]["stages"], 50)
        self.assertEqual(result["counts"]["hashed_artifacts"], 137)

    def upstream_change(self, change):
        upstream = json.loads(UPSTREAM.read_text(encoding="utf-8"))
        change(upstream)
        return {UPSTREAM: json.dumps(upstream)}

    def test_fit_missing_license_file_is_rejected(self):
        license_path = UPSTREAM.parent / "vendor/CANopenNode/LICENSE"
        code, result = self.check_with(missing_files={license_path.resolve()})
        self.assertEqual(code, 1)
        self.assertIn("CANopen fit upstream: missing file vendor/CANopenNode/LICENSE", result["errors"])

    def test_fit_license_cannot_be_removed_from_manifest(self):
        changes = self.upstream_change(lambda data: data.update(
            files=[row for row in data["files"] if row["upstream_path"] != "LICENSE"]))
        self.assert_rejected(changes, "complete 14-file inventory including LICENSE")

    def test_fit_altered_manifest_hash_is_rejected(self):
        changes = self.upstream_change(lambda data: data["files"][0].update(sha256="0" * 64))
        self.assert_rejected(changes, "CANopen fit upstream: SHA-256 mismatch")

    def test_fit_revision_must_match_immutable_public_pin(self):
        for revision in ("main", "0" * 40):
            with self.subTest(revision=revision):
                changes = self.upstream_change(lambda data: data.update(revision=revision))
                self.assert_rejected(changes, "immutable revision differs from public_code pin")

    def test_fit_source_mapping_cannot_name_another_upstream_file(self):
        changes = self.upstream_change(
            lambda data: data["files"][0].update(upstream_path="301/CO_config.h"))
        self.assert_rejected(changes, "invalid source mapping vendor/CANopenNode/301/CO_driver.h")

    def test_fit_manifest_path_cannot_escape_proof(self):
        changes = self.upstream_change(
            lambda data: data["files"][0].update(path="../../AGENTS.md"))
        self.assert_rejected(changes, "path escapes base: ../../AGENTS.md")

    def test_fit_unlisted_vendor_file_is_rejected(self):
        vendor = UPSTREAM.parent / "vendor"
        extra = vendor / "CANopenNode/unlisted.c"
        original_rglob = Path.rglob
        original_is_file = Path.is_file

        def rglob(path, pattern):
            yield from original_rglob(path, pattern)
            if path == vendor:
                yield extra

        def is_file(path):
            return path == extra or original_is_file(path)

        with patch.object(Path, "rglob", rglob), patch.object(Path, "is_file", is_file):
            self.assert_rejected({}, "missing/unexpected vendored files")

    def test_missing_prompt_cannot_silently_shrink_sequence(self):
        changes = self.sequence_change(lambda entries: entries.pop(16))
        self.assert_rejected(changes, "consecutive integers")

    def test_forward_dependency_is_rejected(self):
        changes = self.sequence_change(lambda entries: entries[0].update(depends_on=[50]))
        self.assert_rejected(changes, "distinct earlier prompts")

    def test_missing_declared_prerequisite_link_is_rejected(self):
        path = ROOT / "docs/prompts/17_pdo_observations.md"
        text = path.read_text(encoding="utf-8").replace(", [16](16_nmt_services.md)", "", 1)
        self.assert_rejected({path: text}, "prerequisite links differ")

    def test_missing_commit_and_sync_closure_is_rejected(self):
        path = ROOT / "docs/prompts/23_owner_dispatch.md"
        text = re.sub(r"\b(?:commit|sync)\b", "omitted", path.read_text(encoding="utf-8"),
                      flags=re.IGNORECASE)
        self.assert_rejected({path: text}, "missing commit/sync closure")

    def test_evidence_harness_cannot_depend_on_settings_or_remapping(self):
        # These remain earlier numeric IDs, so the forward-dependency check alone
        # would miss a documentary-evidence deadlock through the owner/adapter path.
        for gated_stage in (18, 20, 21, 22):
            with self.subTest(gated_stage=gated_stage):
                changes = self.sequence_change(
                    lambda entries: entries[22]["depends_on"].append(gated_stage))
                self.assert_rejected(changes, "Evidence harness 29 must not depend")

    def test_index_dependency_drift_is_rejected(self):
        path = ROOT / "docs/prompts/README.md"
        text = re.sub(r"^(\| 23 \| [^|]+ \| )[^|]+( \|)$", r"\g<1>6, 14, 18, 19\2",
                      path.read_text(encoding="utf-8"), flags=re.MULTILINE)
        self.assert_rejected({path: text}, "index prerequisites differ")

    def test_old_scope_cannot_disappear_from_mapping(self):
        def remove_origin(entries):
            for entry in entries:
                entry["source_stages"] = [value for value in entry["source_stages"] if value != "18"]

        self.assert_rejected(self.sequence_change(remove_origin), "retain ownership of all original stages")

    def test_changed_eds_default_is_rejected_against_original(self):
        path = ROOT / "docs/reference/extracted/LC_StepDriverCAN_V1.1.object_inventory.json"
        intake = json.loads(path.read_text(encoding="utf-8"))
        row = next(row for row in intake["scalar_entries"] if row["section"] == "2202")
        row["fields"]["DefaultValue"] = "0"  # The original says 10, despite the manual's 0/1 set.
        self.assert_rejected({path: json.dumps(intake)}, "EDS intake: field mismatch 2202")

    def test_missing_eds_scalar_is_rejected(self):
        path = ROOT / "docs/reference/extracted/LC_StepDriverCAN_V1.1.object_inventory.json"
        intake = json.loads(path.read_text(encoding="utf-8"))
        intake["scalar_entries"].pop()
        self.assert_rejected({path: json.dumps(intake)}, "EDS intake: incomplete/duplicate scalar_entries")

    def test_malformed_ledger_reports_error_instead_of_crashing(self):
        path = ROOT / "docs/reference/lichuan_objects.json"
        ledger = json.loads(path.read_text(encoding="utf-8"))
        ledger["objects"][0] = None
        self.assert_rejected({path: json.dumps(ledger)}, "Lichuan ledger: object row must be an object")


if __name__ == "__main__":
    unittest.main()
