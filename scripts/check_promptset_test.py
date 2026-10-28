#!/usr/bin/env python3
"""Focused regression checks for prompt sequence and completion validation."""

import contextlib
import importlib.util
import io
import json
from pathlib import Path
import re
import sys
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
SEQUENCE = ROOT / "docs/prompts/SEQUENCE.json"
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
    def check_with(self, replacements=None):
        replacements = replacements or {}
        original_read = Path.read_text

        def read(path, *args, **kwargs):
            if path.resolve() in replacements:
                return replacements[path.resolve()]
            return original_read(path, *args, **kwargs)

        output = io.StringIO()
        with patch.object(Path, "read_text", read), patch.object(sys, "argv", ["check_promptset.py"]):
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
