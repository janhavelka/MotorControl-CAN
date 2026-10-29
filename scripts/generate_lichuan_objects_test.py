#!/usr/bin/env python3
"""Adversarial generation checks using small projections and full source validation."""

import copy
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from check_lichuan_ledger import ROOT, load_json, validate_ledger
from generate_lichuan_objects import (LEDGER_PATH, OUTPUT_PATHS, catalogue_header,
                                      coverage_document, cpp_string, generate, objects_header, projected_rows, publish)


def fixture_entry(index=0x1009, subindex=0):
    """A small documentary fixture, independent of the production dictionary."""
    return {
        "key": "0x{:04X}:{:02X}".format(index, subindex), "index": index, "subindex": subindex,
        "scope_id": "eds_family_unbound", "role": "value",
        "raw_fields": {"ParameterName": "Synthetic source version"},
        "type": {"eds_code": "0x0006", "name": "U16", "width_bytes": 2, "signed": False},
        "access_eds": "RO", "access_reviewed": "RO", "low_limit_eds": None,
        "high_limit_eds": None, "default_eds": None, "units": "raw code",
        "scaling": "unknown", "pdo_mapping_eds": "0", "pdo_mapping_reviewed": False,
        "effect": "unknown", "prerequisites": ["unbound fixture"], "edge_behavior": "unknown",
        "source_refs": [{"id": "EDS", "section": "1009", "line_start": 1, "line_end": 3}],
        "owner_prompt": 19, "disposition": "supported_evidence", "exclusion_reason": None,
        "conflict_ids": [], "review": {"status": "complete", "generation_type": "resolved",
            "read_effect": "not_established", "write_policy": "not_writable",
            "gate_ids": ["Q01", "Q20"]}}


def fixture_objects(*entries):
    return [{"entries": list(entries)}]


class ProjectionTests(unittest.TestCase):
    def reject(self, entry, fragment):
        with self.assertRaisesRegex(ValueError, fragment):
            projected_rows(fixture_objects(entry))

    def test_cpp_literal_uses_utf8_bytes_without_trigraphs_or_escape_ambiguity(self):
        self.assertEqual(cpp_string(None), "nullptr")
        self.assertEqual(cpp_string('quote " slash \\ ??/ \u00b0\U0001f50d\x017\n'),
                         r'"quote \" slash \\ \?\?/ \302\260\360\237\224\215\0017\012"')
        with self.assertRaises(UnicodeEncodeError):
            cpp_string("isolated \ud800 surrogate")

    def test_small_projection_is_ordered_and_preserves_native_version(self):
        rows = projected_rows(fixture_objects(fixture_entry(0x100A), fixture_entry()))
        self.assertEqual([row["index"] for row in rows], [0x1009, 0x100A])
        header = objects_header(rows)
        self.assertIn("ScalarType::U16, 2, false, SdoAccess::READ_ONLY", header)
        self.assertNotIn("Catalogue.h", header)
        self.assertNotIn("const char*", header)
        self.assertNotIn("canWrite", header)

    def test_duplicate_address_rejected(self):
        entry = fixture_entry()
        with self.assertRaisesRegex(ValueError, "duplicate entry address"):
            projected_rows(fixture_objects(entry, copy.deepcopy(entry)))

    def test_invalid_addresses_and_numeric_booleans_rejected(self):
        for field, value in (("index", True), ("index", -1), ("index", 0x10000),
                             ("subindex", False), ("subindex", -1), ("subindex", 256)):
            with self.subTest(field=field, value=value):
                entry = fixture_entry()
                entry[field] = value
                self.reject(entry, "invalid entry address")

    def test_invalid_type_width_and_sign_rejected(self):
        for field, value in (("width_bytes", True), ("width_bytes", 0), ("width_bytes", 4),
                             ("signed", 0), ("signed", True), ("name", "STRING")):
            with self.subTest(field=field, value=value):
                entry = fixture_entry()
                entry["type"][field] = value
                self.reject(entry, "datatype/width/signedness mismatch")

    def test_invalid_numeric_bounds_and_reversal_rejected(self):
        for field, value in (("low_limit_eds", "-1"), ("high_limit_eds", "65536"),
                             ("default_eds", True), ("default_eds", "1.5"),
                             ("low_limit_eds", "$NODEID+0x600")):
            with self.subTest(field=field, value=value):
                entry = fixture_entry()
                entry[field] = value
                self.reject(entry, "numeric")
        entry = fixture_entry()
        entry.update(low_limit_eds="10", high_limit_eds="9")
        self.reject(entry, "reversed numeric limits")

    def test_absent_limits_and_node_expression_are_preserved(self):
        entry = fixture_entry()
        entry["default_eds"] = "$NODEID+0x600"
        rows = projected_rows(fixture_objects(entry))
        header = catalogue_header(rows, [{"id": "eds_family_unbound", "firmware": None}])
        self.assertIn('"$NODEID+0x600"', header)
        self.assertIn("nullptr,\n    nullptr", header)
        self.assertNotIn("0x601", header)

    def test_missing_evidence_applicability_or_disposition_rejected(self):
        for field, value, message in (("source_refs", [], "source evidence"),
                                      ("scope_id", None, "applicability")):
            entry = fixture_entry()
            entry[field] = value
            self.reject(entry, message)
        entry = fixture_entry()
        del entry["review"]["generation_type"]
        self.reject(entry, "generation type disposition")

    def test_pending_review_cannot_generate_a_descriptor(self):
        entry = fixture_entry()
        entry["review"]["status"] = "pending"
        self.reject(entry, "pending documentary review")
        entry["review"].update(status="complete", generation_type="pending")
        self.reject(entry, "generation type disposition")

    def test_reserved_entry_excluded_from_both_headers(self):
        entry = fixture_entry(0x2400, 9)
        entry.update(role="reserved", disposition="excluded", exclusion_reason="Reserved source slot",
                     access_reviewed=None)
        entry["review"].update(generation_type="excluded", write_policy="reserved")
        rows = projected_rows(fixture_objects(entry))
        self.assertNotIn("OBJECT_2400_09", objects_header(rows))
        self.assertNotIn("METADATA_2400_09", catalogue_header(rows, []))
        entry["access_reviewed"] = "RW"
        self.reject(entry, "reserved entry grants access")

    def test_unresolved_signed_types_are_withheld(self):
        for index in (0x6060, 0x6061, 0x6098, 0x60FF):
            with self.subTest(index=index):
                entry = fixture_entry(index)
                self.reject(entry, "generation type disposition")
                entry["review"]["generation_type"] = "withheld"
                rows = projected_rows(fixture_objects(entry))
                self.assertNotIn("OBJECT_{:04X}_00".format(index), objects_header(rows))
                self.assertNotIn("METADATA_{:04X}_00".format(index), catalogue_header(rows, []))

    def test_sdo_access_write_policy_and_read_effect_cannot_widen(self):
        for field, value, message in (("access_reviewed", "RW", "SDO access"),
                                      ("pdo_mapping_reviewed", True, "PDO policy")):
            entry = fixture_entry()
            entry[field] = value
            self.reject(entry, message)
        entry = fixture_entry()
        entry["review"]["read_effect"] = "non_changing"
        self.reject(entry, "unreviewed effect")
        entry["review"].update(read_effect="not_established", write_policy="allowed")
        self.reject(entry, "unqualified write policy")

    def test_gate_shape_duplicates_and_owner_boolean_rejected(self):
        for gates in (["Q00"], ["Q22"], ["Q01", "Q01"], [None], [[]]):
            entry = fixture_entry()
            entry["review"]["gate_ids"] = gates
            self.reject(entry, "qualification gates")
        entry = fixture_entry()
        entry["owner_prompt"] = True
        self.reject(entry, "unreviewed effect or owner")

    def test_count_has_its_own_width_and_access(self):
        entry = fixture_entry(0x2400)
        entry["role"] = "count"
        rows = projected_rows(fixture_objects(entry))
        self.assertIn("ScalarType::U16, 2, false, SdoAccess::READ_ONLY", objects_header(rows))
        self.assertIn("WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN", objects_header(rows))


class GenerationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ledger = load_json(ROOT / LEDGER_PATH)

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="lichuan-generator-test-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def snapshot(self):
        return {path: (self.root / path).read_bytes() for path in OUTPUT_PATHS}

    def changed_ledger(self, mutate):
        data = copy.deepcopy(self.ledger)
        mutate(data)
        path = self.root / "ledger.json"
        path.write_text(json.dumps(data, ensure_ascii=True), encoding="utf-8")
        return path

    def test_repeated_generation_and_check_are_byte_identical(self):
        first = generate(ROOT / LEDGER_PATH, self.root)
        times = {path: (self.root / path).stat().st_mtime_ns for path in OUTPUT_PATHS}
        self.assertEqual(generate(ROOT / LEDGER_PATH, self.root), first)
        self.assertEqual(generate(ROOT / LEDGER_PATH, self.root, check=True), first)
        self.assertEqual(times, {path: (self.root / path).stat().st_mtime_ns for path in OUTPUT_PATHS})
        self.assertEqual(len(first), 4)
        self.assertEqual(len(self.ledger["objects"]), 99)
        rows = projected_rows(self.ledger["objects"])
        self.assertEqual(len(rows), 202)
        self.assertEqual(sum(row["review"]["generation_type"] == "resolved" for row in rows), 194)
        self.assertEqual(sum(row["review"]["generation_type"] == "withheld" for row in rows), 4)
        self.assertEqual(sum(row["review"]["generation_type"] == "excluded" for row in rows), 4)
        self.assertEqual(len(self.ledger["manual_only"]), 53)

    def test_stale_exact_bytes_fail_without_mutation(self):
        generate(ROOT / LEDGER_PATH, self.root)
        path = self.root / OUTPUT_PATHS[1]
        path.write_bytes(path.read_bytes().replace(b"ScalarType::U16", b"ScalarType::U32", 1))
        before = self.snapshot()
        with self.assertRaisesRegex(ValueError, "missing or stale.*Objects.h"):
            generate(ROOT / LEDGER_PATH, self.root, check=True)
        self.assertEqual(self.snapshot(), before)
        generate(ROOT / LEDGER_PATH, self.root)
        generate(ROOT / LEDGER_PATH, self.root, check=True)

    def test_line_ending_drift_is_stale(self):
        generate(ROOT / LEDGER_PATH, self.root)
        path = self.root / OUTPUT_PATHS[0]
        path.write_bytes(path.read_bytes().replace(b"\n", b"\r\n"))
        with self.assertRaisesRegex(ValueError, "missing or stale.*Types.h"):
            generate(ROOT / LEDGER_PATH, self.root, check=True)

    def test_missing_output_check_does_not_create_a_file(self):
        with self.assertRaisesRegex(ValueError, "missing or stale"):
            generate(ROOT / LEDGER_PATH, self.root / "absent", check=True)
        self.assertFalse((self.root / "absent").exists())

    def test_full_validation_failure_leaves_every_output_unchanged(self):
        generate(ROOT / LEDGER_PATH, self.root)
        before = self.snapshot()
        mutations = [
            lambda data: data["objects"][0]["entries"].append(copy.deepcopy(data["objects"][0]["entries"][0])),
            lambda data: data["objects"][0]["entries"][0].update(source_refs=[]),
            lambda data: data["objects"][0]["entries"][0]["type"].update(width_bytes=True),
            lambda data: data["objects"][0]["entries"][0]["review"].pop("generation_type"),
            lambda data: data["objects"][0]["entries"][0]["review"].update(generation_type="withheld"),
            lambda data: data["objects"][0]["entries"][0]["review"].update(gate_ids=["Q20"]),
            lambda data: data["conflicts"][0].update(resolution="resolved"),
        ]
        for mutate in mutations:
            with self.subTest(mutate=mutate):
                path = self.changed_ledger(mutate)
                with self.assertRaisesRegex(ValueError, "ledger validation failed"):
                    generate(path, self.root)
                self.assertEqual(self.snapshot(), before)

    def test_duplicate_json_member_fails_before_publication(self):
        generate(ROOT / LEDGER_PATH, self.root)
        before = self.snapshot()
        path = self.root / "duplicate.json"
        path.write_text('{"schema_version":2,"schema_version":2}', encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "duplicate JSON member"):
            generate(path, self.root)
        self.assertEqual(self.snapshot(), before)

    def test_render_failure_precedes_publication(self):
        generate(ROOT / LEDGER_PATH, self.root)
        before = self.snapshot()
        with patch("generate_lichuan_objects.catalogue_header", side_effect=ValueError("candidate failure")):
            with self.assertRaisesRegex(ValueError, "candidate failure"):
                generate(ROOT / LEDGER_PATH, self.root)
        self.assertEqual(self.snapshot(), before)

    def test_unencodable_source_text_does_not_publish_partial_outputs(self):
        generate(ROOT / LEDGER_PATH, self.root)
        before = self.snapshot()
        path = self.changed_ledger(lambda data: data["objects"][0]["entries"][0].update(
            units="unknown \ud800"))
        with self.assertRaises(UnicodeEncodeError):
            generate(path, self.root)
        self.assertEqual(self.snapshot(), before)

    def test_replacement_failure_rolls_back_existing_and_new_files(self):
        for existing in (False, True):
            with self.subTest(existing=existing):
                root = self.root / str(existing)
                outputs = {"a/header.h": b"first", "b/header.h": b"second", "report.md": b"third"}
                if existing:
                    publish({name: b"old" for name in outputs}, root)
                actual_replace = os.replace
                count = 0

                def fail_second(source, target):
                    nonlocal count
                    count += 1
                    if count == 2:
                        raise OSError("deliberate second replacement failure")
                    actual_replace(source, target)

                with patch("generate_lichuan_objects.os.replace", side_effect=fail_second):
                    with self.assertRaisesRegex(OSError, "deliberate second"):
                        publish(outputs, root)
                for name in outputs:
                    path = root / name
                    if existing:
                        self.assertEqual(path.read_bytes(), b"old")
                    else:
                        self.assertFalse(path.exists())
                self.assertEqual(list(root.glob(".lichuan-candidate-*")), [])

    def test_cli_source_validation_is_not_bypassed_by_external_output_root(self):
        result = subprocess.run([sys.executable, str(ROOT / "scripts/generate_lichuan_objects.py"),
                                 "--root", str(self.root / "output"),
                                 "--source-root", str(self.root / "no originals")],
                                capture_output=True, text=True, timeout=20, check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("generation failed", result.stderr)
        self.assertFalse((self.root / "output").exists())

    def test_full_validator_rejects_generation_disposition_forgery(self):
        for key, value in (("0x6060:00", "resolved"), ("0x60FF:00", "resolved"),
                           ("0x2400:09", "resolved"), ("0x1009:00", "excluded")):
            data = copy.deepcopy(self.ledger)
            entry = next(entry for obj in data["objects"] for entry in obj["entries"] if entry["key"] == key)
            entry["review"]["generation_type"] = value
            self.assertTrue(any("generation type disposition" in error for error in validate_ledger(data)))

    def test_coverage_preserves_model_specific_gates_and_family_binding(self):
        data = copy.deepcopy(self.ledger)
        data["semantic_review"]["binding_policy"] = "Unique explicit binding policy marker"
        data["semantic_review"]["families"][0]["limitations"] = "Unique family capability limit marker"
        data["semantic_review"]["gates"][0]["models"] = ["Unique gate model marker"]
        document = coverage_document(data, projected_rows(data["objects"]))
        self.assertIn("Unique explicit binding policy marker", document)
        self.assertIn("Unique family capability limit marker", document)
        self.assertIn("Unique gate model marker", document)
        self.assertIn("| Models | Owners |", document)
        self.assertIn("| Typed reads | 0 |", document)
        self.assertIn("| Typed settings | 0 |", document)
        self.assertIn("| Actions | 0 |", document)
        self.assertIn("| Workflows | 0 |", document)

    def test_coverage_lists_all_three_source_rpdo1_words(self):
        document = coverage_document(self.ledger, projected_rows(self.ledger["objects"]))
        self.assertIn("default RPDO1 source words 0x60400010, 0x60ff0020, 0x60600008", document)
        self.assertIn("do not grant SDO reads of WO 6040/6060 or any ungated action", document)

    def test_coverage_preserves_each_documentary_disposition(self):
        document = coverage_document(self.ledger, projected_rows(self.ledger["objects"]))
        for row in projected_rows(self.ledger["objects"]):
            section = document.split("### " + row["key"] + "\n", 1)[1].split("\n### ", 1)[0]
            self.assertIn("| Documentary disposition | " + row["disposition"] + " |", section)


if __name__ == "__main__":
    unittest.main()
