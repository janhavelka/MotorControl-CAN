#!/usr/bin/env python3
"""Adversarial checks for the source-intake validator."""

import copy
from pathlib import Path
import tempfile
import unittest

from check_lichuan_ledger import ROOT, load_json, validate_ledger


LEDGER = load_json(ROOT / "docs/reference/lichuan_objects.json")


def entry(data, key):
    return next(value for obj in data["objects"] for value in obj["entries"]
                if value["key"] == key)


class LedgerTests(unittest.TestCase):
    def changed(self):
        return copy.deepcopy(LEDGER)

    def reject(self, data, fragment):
        errors = validate_ledger(data)
        self.assertTrue(any(fragment in message for message in errors), errors)

    def test_original_ledger_passes(self):
        self.assertEqual(validate_ledger(LEDGER), [])
        self.assertEqual(len(LEDGER["objects"]), 99)
        self.assertEqual(sum(len(obj["entries"]) for obj in LEDGER["objects"]), 202)
        self.assertEqual(len(LEDGER["manual_only"]), 52)

    def test_duplicate_json_member_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "duplicate.json"
            path.write_text('{"schema_version":1,"schema_version":2}', encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "duplicate JSON member"):
                load_json(path)

    def test_duplicate_entry_address_rejected(self):
        data = self.changed()
        object_row = next(obj for obj in data["objects"] if obj["key"] == "0x2400")
        object_row["entries"].append(copy.deepcopy(object_row["entries"][10]))
        self.reject(data, "duplicate entry address")

    def test_duplicate_object_index_rejected(self):
        data = self.changed()
        data["objects"].append(copy.deepcopy(data["objects"][0]))
        self.reject(data, "duplicate object key in scope")

    def test_noncanonical_subindex_rejected(self):
        data = self.changed()
        entry(data, "0x2400:0A")["key"] = "0x2400:A"
        self.reject(data, "invalid entry key/address")

    def test_missing_applicability_rejected(self):
        data = self.changed()
        data["scopes"][0]["family"] = ""
        self.reject(data, "missing family/model applicability")

    def test_firmware_binding_cannot_be_invented(self):
        data = self.changed()
        data["scopes"][0]["firmware"] = "1.2.3"
        data["scopes"][0]["binding"] = "hardware qualified"
        self.reject(data, "firmware/binding must remain unverified")

    def test_wrong_width_rejected(self):
        data = self.changed()
        entry(data, "0x2400:00")["type"]["width_bytes"] = 1
        self.reject(data, "datatype/width/signedness mismatch")

    def test_invalid_numeric_format_rejected(self):
        data = self.changed()
        entry(data, "0x1003:00")["low_limit_eds"] = "1.0"
        self.reject(data, "invalid numeric/expression syntax")

    def test_numeric_value_outside_type_rejected(self):
        data = self.changed()
        entry(data, "0x1003:00")["high_limit_eds"] = "256"
        self.reject(data, "numeric value exceeds datatype")

    def test_changed_original_default_rejected(self):
        data = self.changed()
        entry(data, "0x2202:00")["default_eds"] = "0"
        self.reject(data, "default_eds absent/value changed")

    def test_missing_default_cannot_be_filled_with_zero(self):
        data = self.changed()
        entry(data, "0x607D:01")["default_eds"] = "0"
        self.reject(data, "default_eds absent/value changed")

    def test_nodeid_expression_cannot_be_evaluated(self):
        data = self.changed()
        entry(data, "0x1200:01")["default_eds"] = "0x601"
        self.reject(data, "default_eds absent/value changed")

    def test_invented_subindex_rejected(self):
        data = self.changed()
        object_row = next(obj for obj in data["objects"] if obj["key"] == "0x1018")
        fabricated = copy.deepcopy(object_row["entries"][3])
        fabricated["key"] = "0x1018:04"
        fabricated["subindex"] = 4
        object_row["entries"].append(fabricated)
        self.reject(data, "SubNumber does not match declared entries")

    def test_reserved_entry_cannot_be_writable_policy(self):
        data = self.changed()
        entry(data, "0x2400:10")["disposition"] = "pending_review"
        self.reject(data, "reserved entry must be excluded")

    def test_source_intake_cannot_claim_implementation(self):
        data = self.changed()
        entry(data, "0x6040:00")["coverage"]["implemented"] = True
        self.reject(data, "intake cannot claim implementation")

    def test_missing_reserved_entry_rejected(self):
        data = self.changed()
        object_row = next(obj for obj in data["objects"] if obj["key"] == "0x2400")
        object_row["entries"] = [value for value in object_row["entries"]
                                 if value["key"] != "0x2400:10"]
        self.reject(data, "SubNumber does not match declared entries")

    def test_missing_pdf_page_rejected(self):
        data = self.changed()
        data["manual_only"][0]["source_refs"][0]["page"] = 999
        self.reject(data, "PDF page invalid")

    def test_wrong_header_source_rejected(self):
        data = self.changed()
        data["objects"][0]["source_ref"].update(id="EN86", page=1)
        self.reject(data, "imported header must cite EDS")

    def test_wrong_source_hash_rejected(self):
        data = self.changed()
        next(source for source in data["sources"] if source["id"] == "EN86")["sha256"] = "0" * 64
        self.reject(data, "sha256 differs from manifest")

    def test_missing_manual_related_object_rejected(self):
        data = self.changed()
        data["manual_only"][0]["related_entries"] = ["0x1018:04"]
        self.reject(data, "related EDS address missing")

    def test_manual_object_address_unique_within_scope(self):
        data = self.changed()
        for item in data["manual_only"][:2]:
            item["kind"] = "object"
            item["address"] = "0x6502:00"
        data["manual_index_inventory"]["additional_canopen_addresses"] = [
            "0x6502:00", "0x6502:00"]
        self.reject(data, "duplicate manual object address in scope")

    def test_manual_owner_prompt_bounded(self):
        data = self.changed()
        data["manual_only"][0]["owner_prompt"] = -200
        self.reject(data, "missing claim/owner")

    def test_resolved_conflict_requires_evidence(self):
        data = self.changed()
        data["conflicts"][0]["resolution"] = "resolved"
        self.reject(data, "resolved without reviewed evidence")

    def test_fake_resolution_string_cannot_count_as_evidence(self):
        data = self.changed()
        data["conflicts"][0].update(resolution="resolved", decision="take EN86",
                                     resolution_evidence="verified")
        self.reject(data, "resolved without reviewed evidence")

    def test_malformed_source_and_claim_return_errors(self):
        data = self.changed()
        data["sources"][0]["id"] = []
        data["conflicts"][0]["claims"] = ["bad", "claim"]
        errors = validate_ledger(data)
        self.assertTrue(any("source ID must be a string" in value for value in errors))
        self.assertTrue(any("claim must be an object" in value for value in errors))

    def test_malformed_nested_rows_return_errors(self):
        for location in ("object", "entry", "manual", "conflict_ids"):
            with self.subTest(location=location):
                data = self.changed()
                if location == "object":
                    data["objects"][0] = None
                elif location == "entry":
                    data["objects"][0]["entries"][0] = None
                elif location == "manual":
                    data["manual_only"][0] = None
                else:
                    data["objects"][0]["entries"][0]["conflict_ids"] = None
                self.assertTrue(validate_ledger(data))

    def test_malformed_metadata_and_reference_return_errors(self):
        data = self.changed()
        data["eds_metadata"] = None
        data["objects"][0]["source_ref"]["section"] = []
        errors = validate_ledger(data)
        self.assertTrue(any("EDS metadata must be an object" in value for value in errors))
        self.assertTrue(any("EDS section must be a string" in value for value in errors))

    def test_explicit_unknown_fields_required(self):
        data = self.changed()
        del data["scopes"][0]["firmware"]
        del data["objects"][0]["entries"][0]["units"]
        errors = validate_ledger(data)
        self.assertTrue(any("firmware/binding must remain unverified" in value for value in errors))
        self.assertTrue(any("reviewed semantics are premature" in value for value in errors))

    def test_source_anomalies_need_unique_id_and_scope(self):
        data = self.changed()
        duplicate = copy.deepcopy(data["source_anomalies"][0])
        duplicate["scope_id"] = "unknown"
        data["source_anomalies"].append(duplicate)
        errors = validate_ledger(data)
        self.assertTrue(any("duplicate or invalid source anomaly ID" in value for value in errors))
        self.assertTrue(any("source anomaly applicability invalid" in value for value in errors))

    def test_contradiction_kept_side_by_side(self):
        conflict = next(row for row in LEDGER["conflicts"] if row["id"] == "C01")
        self.assertEqual(conflict["resolution"], "unresolved")
        self.assertEqual({row["source_ref"]["id"] for row in conflict["claims"]},
                         {"EDS", "EN86", "ZH86"})
        self.assertIsNone(entry(LEDGER, "0x2400:0B")["access_reviewed"])

    def test_inactive_mapping_slots_and_u16_count_are_preserved(self):
        self.assertEqual(entry(LEDGER, "0x1602:00")["default_eds"], "0")
        self.assertIsNone(entry(LEDGER, "0x1602:04")["default_eds"])
        self.assertEqual(entry(LEDGER, "0x2400:00")["type"]["name"], "U16")
        self.assertEqual(entry(LEDGER, "0x2400:00")["default_eds"], "16")
        self.assertEqual(entry(LEDGER, "0x1200:01")["default_eds"], "$NODEID+0x600")


if __name__ == "__main__":
    unittest.main()
