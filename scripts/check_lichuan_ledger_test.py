#!/usr/bin/env python3
"""Adversarial checks for reviewed policy and retained source accounting."""

import copy
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

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
        self.assertEqual(len(LEDGER["manual_only"]), 53)

    def test_read_ownership_does_not_change_object_ownership(self):
        data = self.changed()
        row = entry(data, "0x1005:00")
        self.assertEqual(row["owner_prompt"], 38)
        self.assertEqual(row["read_owner_prompt"], 19)
        self.assertEqual(validate_ledger(data), [])

    def test_missing_or_null_read_owner_cannot_drop_a_readable_obligation(self):
        # Cover each read owner, including history and deferred PDO records.
        for key in ("0x1003:01", "0x1005:00", "0x2201:00", "0x2400:01",
                    "0x2500:00", "0x1400:01"):
            for missing in (True, False):
                with self.subTest(key=key, missing=missing):
                    data = self.changed()
                    row = entry(data, key)
                    row.pop("read_coverage", None)
                    if missing:
                        row.pop("read_owner_prompt")
                    else:
                        row["read_owner_prompt"] = None
                    self.reject(data, "read owner required for a nonreserved readable")

    def test_only_nonreadable_or_reserved_rows_may_have_no_read_owner(self):
        for key in ("0x6040:00", "0x6060:00", "0x2400:09"):
            data = self.changed()
            row = entry(data, key)
            self.assertIsNone(row["read_owner_prompt"])
            row.pop("read_owner_prompt")
            self.assertEqual(validate_ledger(data), [])

    def test_invalid_read_owners_and_forbidden_reads_are_rejected(self):
        for value in (True, False, 0, 51, -1, "19", [], {}):
            data = self.changed()
            entry(data, "0x1000:00")["read_owner_prompt"] = value
            self.reject(data, "read owner must be an integer")
        for key in ("0x6040:00", "0x6060:00", "0x2400:09"):
            data = self.changed()
            entry(data, key)["read_owner_prompt"] = 19
            self.reject(data, "read owner requires a nonreserved readable")
        for key in ("0x2203:00", "0x6098:00", "0x60FF:00", "0x1600:00"):
            data = self.changed()
            entry(data, key)["read_owner_prompt"] = 19
            self.reject(data, "read owner outside the reviewed Prompt 19")

    def test_only_reviewed_raw_mode_byte_can_have_unresolved_read_type(self):
        data = self.changed()
        row = entry(data, "0x6061:00")
        self.assertEqual(row["read_owner_prompt"], 19)
        self.assertEqual(row["review"]["generation_type"], "withheld")
        row["type"]["width_bytes"] = 2
        self.reject(data, "read owner unresolved read type")

    def test_separate_read_coverage_can_record_software_without_effect_qualification(self):
        data = self.changed()
        for key in ("0x1005:00", "0x6061:00"):
            row = entry(data, key)
            row["read_coverage"] = dict(documented=True, implemented=True,
                                        host_tested=True, hardware_qualified=False)
            self.assertEqual(row["review"]["read_effect"], "not_established")
            self.assertFalse(row["coverage"]["implemented"])
        self.assertEqual(validate_ledger(data), [])

    def test_driver_read_scope_and_coverage_stay_separate_from_writes(self):
        data = self.changed()
        for key in ("0x2200:00", "0x2201:00", "0x2202:00", "0x607D:00",
                    "0x607D:01", "0x607D:02", "0x6081:00", "0x6083:00", "0x6084:00"):
            row = entry(data, key)
            self.assertEqual(row["read_owner_prompt"], 20)
            row["read_coverage"] = dict(documented=True, implemented=True,
                                        host_tested=True, hardware_qualified=False)
            self.assertFalse(row["coverage"]["implemented"])
            self.assertEqual(row["review"]["read_effect"], "not_established")
        self.assertEqual(validate_ledger(data), [])
        for key in ("0x2502:00", "0x1000:00", "0x60FF:00", "0x607D:03"):
            data = self.changed()
            row = entry(data, key) if key != "0x607D:03" else entry(data, "0x607D:02")
            row["read_owner_prompt"] = 20
            if key == "0x607D:03":
                row["subindex"] = 3
            self.reject(data, "read owner outside the reviewed Prompt 20")

    def test_read_coverage_cannot_widen_scope_or_claim_hardware(self):
        coverage = dict(documented=True, implemented=True, host_tested=True,
                        hardware_qualified=False)
        for key in ("0x6040:00", "0x2400:09", "0x1600:00", "0x2203:00"):
            data = self.changed()
            entry(data, key)["read_coverage"] = coverage.copy()
            self.reject(data, "read coverage requires a reviewed implemented-group")
        cases = [(None, "fields invalid"), ({}, "fields invalid"),
                 (dict(coverage, extra=False), "fields invalid"),
                 (dict(coverage, implemented=1), "must be boolean"),
                 (dict(coverage, documented=False), "must retain documented source"),
                 (dict(coverage, implemented=False), "host tests require"),
                 (dict(coverage, hardware_qualified=True), "cannot claim hardware")]
        for value, message in cases:
            data = self.changed()
            entry(data, "0x1000:00")["read_coverage"] = value
            self.reject(data, "read coverage " + message)

    def test_io_read_scope_retains_reserved_exclusions_and_write_gates(self):
        data = self.changed()
        reads = [e for o in data["objects"] for e in o["entries"]
                 if e.get("read_owner_prompt") == 21]
        self.assertEqual(len(reads), 23)
        for row in reads:
            row["read_coverage"] = dict(documented=True, implemented=True,
                                        host_tested=True, hardware_qualified=False)
            self.assertFalse(row["coverage"]["implemented"])
            self.assertEqual(row["review"]["read_effect"], "not_established")
            self.assertNotEqual(row["review"]["write_policy"], "approved")
        self.assertEqual(validate_ledger(data), [])
        for sub in (9, 10, 11, 16):
            data = self.changed()
            entry(data, "0x2400:{:02X}".format(sub))["read_owner_prompt"] = 21
            self.reject(data, "read owner requires a nonreserved readable")
        for key in ("0x2500:00", "0x2200:00", "0x1000:00", "0x60FE:03"):
            data = self.changed()
            row = entry(data, key) if key != "0x60FE:03" else entry(data, "0x60FE:02")
            row["read_owner_prompt"] = 21
            if key == "0x60FE:03": row["subindex"] = 3
            self.reject(data, "read owner outside the reviewed Prompt 21")

    def test_setting_subsets_keep_scope_policy_and_qualification_separate(self):
        data = self.changed()
        rows = [e for o in data["objects"] for e in o["entries"] if "setting_coverage" in e]
        self.assertEqual(len(rows), 12)
        self.assertEqual(sum(e["owner_prompt"] == 20 for e in rows), 3)
        self.assertEqual(sum(e["owner_prompt"] == 21 for e in rows), 8)
        self.assertEqual(sum(e["owner_prompt"] == 22 for e in rows), 1)
        for row in rows:
            self.assertEqual(row["review"]["write_policy"], "qualification_required")
            self.assertFalse(row["coverage"]["implemented"])
        for field, value in (("setting_scope", "all_firmware_writable"),
                             ("setting_coverage", dict(documented=True, implemented=False,
                                                      host_tested=True, hardware_qualified=False)),
                             ("setting_coverage", dict(documented=True, implemented=True,
                                                      host_tested=True, hardware_qualified=True))):
            data = self.changed()
            entry(data, "0x6081:00")[field] = value
            self.reject(data, "setting coverage")
        data = self.changed()
        e = entry(data, "0x60FE:01")
        e["setting_scope"] = "explicit_di_configuration_86c"
        e["setting_coverage"] = dict(documented=True, implemented=True,
                                      host_tested=True, hardware_qualified=False)
        self.reject(data, "setting coverage outside")

    def test_tuning_reads_cover_exact_scalars_without_qualifying_effects_or_writes(self):
        data = self.changed()
        rows = [e for o in data["objects"] for e in o["entries"]
                if e.get("read_owner_prompt") == 22]
        self.assertEqual({e["key"] for e in rows},
                         {"0x{:04X}:00".format(index) for index in range(0x2500, 0x2521)})
        for row in rows:
            row["read_coverage"] = dict(documented=True, implemented=True,
                                        host_tested=True, hardware_qualified=False)
            self.assertEqual(row["owner_prompt"], 22)
            self.assertEqual(row["type"], dict(eds_code="0x0006", name="U16",
                                              width_bytes=2, signed=False))
            self.assertEqual(row["review"]["read_effect"], "not_established")
            self.assertFalse(row["coverage"]["implemented"])
            self.assertNotEqual(row["review"]["write_policy"], "approved")
        self.assertEqual(validate_ledger(data), [])

    def test_tuning_read_coverage_rejects_reassigned_owner_keys_and_native_types(self):
        for key in ("0x1000:00", "0x2200:00", "0x2400:01", "0x607D:01"):
            data = self.changed()
            entry(data, key)["read_owner_prompt"] = 22
            self.reject(data, "Prompt 22 scalar read scope")
        for field, value in (("owner_prompt", 38), ("role", "count"), ("subindex", 1),
                             ("index", 0x2521), ("index", 0x24FF)):
            data = self.changed()
            entry(data, "0x2500:00")[field] = value
            self.reject(data, "Prompt 22 scalar read scope")
        for native in (dict(eds_code="0x0005", name="U8", width_bytes=1, signed=False),
                       dict(eds_code="0x0003", name="I16", width_bytes=2, signed=True),
                       dict(eds_code="0x0007", name="U32", width_bytes=4, signed=False)):
            data = self.changed()
            entry(data, "0x2500:00")["type"] = native
            self.reject(data, "Prompt 22 U16 native read type")
        for field, value, message in (("read_owner_prompt", 38, "requires a reviewed implemented-group read"),
                                     ("read_coverage", dict(documented=True, implemented=True,
                                                           host_tested=True, hardware_qualified=True),
                                      "cannot claim hardware qualification")):
            data = self.changed()
            row = entry(data, "0x2500:00")
            row["read_coverage"] = dict(documented=True, implemented=True,
                                        host_tested=True, hardware_qualified=False)
            row[field] = value
            self.reject(data, "read coverage " + message)

    def test_loop_mode_setting_coverage_cannot_escape_2500_or_qualification(self):
        coverage = dict(documented=True, implemented=True, host_tested=True,
                        hardware_qualified=False)
        for key in ("0x2501:00", "0x2502:00", "0x2520:00", "0x2400:01"):
            data = self.changed()
            row = entry(data, key)
            row.update(setting_scope="explicit_loop_mode_86c", setting_coverage=coverage.copy())
            self.reject(data, "setting coverage")
        for field, value in (("setting_scope", "all_tuning_86c"),
                             ("read_owner_prompt", 38), ("owner_prompt", 21),
                             ("access_reviewed", "RO")):
            data = self.changed()
            row = entry(data, "0x2500:00")
            row.update(setting_scope="explicit_loop_mode_86c", setting_coverage=coverage.copy())
            row[field] = value
            self.reject(data, "setting coverage")
        for policy in ("not_writable", "allowed"):
            data = self.changed()
            row = entry(data, "0x2500:00")
            row.update(setting_scope="explicit_loop_mode_86c", setting_coverage=coverage.copy())
            row["review"]["write_policy"] = policy
            self.reject(data, "setting coverage outside")
        for key in ("0x2500:00", "0x2501:00", "0x2520:00"):
            data = self.changed()
            row = entry(data, key)
            row["access_reviewed"] = "RW"
            row["review"]["write_policy"] = "qualification_required"
            row.pop("setting_coverage", None)
            self.reject(data, "conservative read-only policy required")

    def test_history_communication_and_pdo_read_owners_remain_finite_and_named(self):
        # Documentary/write ownership stays distinct from the implemented raw-read owner.
        for sub in range(5):
            row = entry(LEDGER, "0x1003:{:02X}".format(sub))
            self.assertEqual((row["owner_prompt"], row["read_owner_prompt"]), (19, 19))
        for key in ("0x1005:00", "0x1014:00", "0x1017:00", "0x1200:01", "0x1200:02"):
            row = entry(LEDGER, key)
            self.assertEqual((row["owner_prompt"], row["read_owner_prompt"]), (38, 19))
        row = entry(LEDGER, "0x1200:00")
        self.assertEqual((row["owner_prompt"], row["read_owner_prompt"]), (13, 19))
        pdo = [e for o in LEDGER["objects"] for e in o["entries"]
               if 0x1400 <= e["index"] <= 0x1A03]
        self.assertEqual(len(pdo), 88)
        for row in pdo:
            communication_count = (0x1400 <= row["index"] <= 0x1403 or
                                   0x1800 <= row["index"] <= 0x1803) and row["subindex"] == 0
            self.assertEqual((row["owner_prompt"], row["read_owner_prompt"]),
                             (17 if communication_count else 18, 38))
            self.assertNotIn("read_coverage", row)
        self.assertEqual(sum(e.get("read_owner_prompt") == 38
                             for o in LEDGER["objects"] for e in o["entries"]), 101)

    def test_pdo_observation_is_exactly_ten_passive_default_targets(self):
        facts = [value for obj in LEDGER["objects"] for value in obj["entries"]
                 if "pdo_observation" in value["review"]]
        self.assertEqual(len(facts), 10)
        for value in facts:
            fact = value["review"]["pdo_observation"]
            self.assertIs(fact["live_qualification_required"], True)
            self.assertIsNone(value["pdo_mapping_reviewed"])
            self.assertEqual(value["review"]["read_effect"], "not_established")
            self.assertIn(value["review"]["write_policy"], ("not_writable", "qualification_required"))
        self.assertEqual(entry(LEDGER, "0x6040:00")["access_reviewed"], "WO")
        self.assertEqual(entry(LEDGER, "0x6060:00")["access_reviewed"], "WO")

    def test_pdo_metadata_mutations_rejected(self):
        cases = [
            ("0x6064:00", "directions", ["RPDO"], "directions differ"),
            ("0x6064:00", "representation", "UNSIGNED", "representation conflicts"),
            ("0x6061:00", "representation", "SIGNED", "representation conflicts"),
            ("0x60FF:00", "representation", "SIGNED", "representation conflicts"),
            ("0x607A:00", "role", "ACTUAL", "role conflicts"),
            ("0x6064:00", "role", "COMMANDED", "role conflicts"),
            ("0x6041:00", "live_qualification_required", False, "cannot waive"),
            ("0x6041:00", "live_qualification_required", 1, "cannot waive"),
            ("0x6041:00", "source_refs", [], "source refs missing"),
            ("0x6041:00", "source_refs", [{"id": "EN86", "page": 999}], "source refs missing"),
        ]
        for key, field, value, fragment in cases:
            with self.subTest(key=key, field=field, value=value):
                data = self.changed()
                entry(data, key)["review"]["pdo_observation"][field] = value
                self.reject(data, "PDO observation " + fragment)

    def test_missing_extra_reserved_and_ineligible_pdo_facts_rejected(self):
        data = self.changed()
        fact = entry(data, "0x6041:00")["review"].pop("pdo_observation")
        self.reject(data, "PDO observation review missing")
        for key in ("0x2400:09", "0x1000:00", "0x2300:00"):
            data = self.changed()
            entry(data, key)["review"]["pdo_observation"] = copy.deepcopy(fact)
            self.reject(data, "PDO observation not supported by default mapping source")
        data = self.changed()
        entry(data, "0x6041:00")["pdo_mapping_reviewed"] = False
        self.reject(data, "PDO observation ineligible target")

    def test_pdo_width_and_mapping_source_forgery_rejected(self):
        data = self.changed()
        entry(data, "0x6041:00")["type"]["width_bytes"] = 4
        self.reject(data, "PDO observation ineligible target or source width mismatch")
        data = self.changed()
        entry(data, "0x1A00:01")["default_eds"] = "0x60410020"
        self.reject(data, "default_eds absent/value changed")
        self.reject(data, "PDO observation ineligible target or source width mismatch")

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
        self.reject(data, "manufacturer/family/dialect applicability invalid")

    def test_false_model_or_protocol_scope_rejected(self):
        for field, value in (("manufacturer", "Nanotec"), ("models", ["CL57-C"]),
                             ("dialect", "MODBUS RTU")):
            with self.subTest(field=field):
                data = self.changed()
                data["scopes"][1][field] = value
                self.assertTrue(validate_ledger(data))

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

    def test_missing_required_manifest_source_reports_error(self):
        manifest = load_json(ROOT / "docs/reference/SOURCES.json")
        manifest["sources"] = [row for row in manifest["sources"] if row["id"] != "EN86"]
        with patch("check_lichuan_ledger.load_json", return_value=manifest):
            self.reject(self.changed(), "source manifest has duplicate or missing required IDs")

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
        del data["objects"][0]["entries"][0]["access_reviewed"]
        del data["objects"][0]["entries"][0]["exclusion_reason"]
        del next(source for source in data["sources"] if source["id"] == "EDS")["pdf_pages"]
        del data["conflicts"][0]["decision"]
        errors = validate_ledger(data)
        self.assertTrue(any("firmware/binding must remain unverified" in value for value in errors))
        self.assertTrue(any("reviewed semantics missing" in value for value in errors))
        self.assertTrue(any("reviewed access cannot widen" in value for value in errors))
        self.assertTrue(any("reviewed entry disposition invalid" in value for value in errors))
        self.assertTrue(any("pdf_pages differs from manifest" in value for value in errors))
        self.assertTrue(any("unresolved conflict has selected decision" in value for value in errors))

    def test_manual_claim_needs_matching_model_pdf(self):
        data = self.changed()
        data["manual_only"][0]["source_refs"] = [{"id": "ZH57", "page": 1}]
        self.reject(data, "manual PDF refs do not match scope")

    def test_malformed_manual_scope_returns_error(self):
        data = self.changed()
        data["manual_only"][0]["scope_id"] = []
        self.reject(data, "missing applicability")

    def test_conflict_claim_needs_matching_model_pdf(self):
        data = self.changed()
        data["conflicts"][0]["claims"][-1]["source_ref"] = {"id": "ZH57", "page": 8}
        self.reject(data, "conflict PDF ref does not match scope")

    def test_malformed_conflict_scope_returns_error(self):
        data = self.changed()
        data["conflicts"][0]["scope_id"] = {}
        self.reject(data, "missing subject/applicability")

    def test_all_original_conflicts_must_remain(self):
        data = self.changed()
        data["conflicts"] = []
        for obj in data["objects"]:
            for value in obj["entries"]:
                value["conflict_ids"] = []
        for item in data["manual_only"]:
            item["conflict_ids"] = []
        self.reject(data, "source conflict inventory incomplete")

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
        self.assertIn({"value": "0B polarity; 0C..0E DO assignments",
                       "source_ref": {"id": "ZH86", "page": 8}}, conflict["claims"])
        function_claims = next(row["claims"] for row in LEDGER["conflicts"] if row["id"] == "C02")
        self.assertIn({"value": "2 running; 3 homing complete",
                       "source_ref": {"id": "ZH86", "page": 8}}, function_claims)
        self.assertIsNone(entry(LEDGER, "0x2400:0B")["access_reviewed"])

    def test_57c_status_is_separate_model_claim(self):
        row = next(item for item in LEDGER["manual_only"]
                   if item["key"] == "status_interpretation_57")
        self.assertEqual(row["scope_id"], "manual_57_contrast")
        self.assertEqual(row["source_refs"], [{"id": "ZH57", "page": 12}])
        self.assertEqual(row["related_entries"], ["0x6041:00"])
        self.assertFalse(row["coverage"]["implemented"])

    def test_inactive_mapping_slots_and_u16_count_are_preserved(self):
        self.assertEqual(entry(LEDGER, "0x1602:00")["default_eds"], "0")
        self.assertIsNone(entry(LEDGER, "0x1602:04")["default_eds"])
        self.assertEqual(entry(LEDGER, "0x2400:00")["type"]["name"], "U16")
        self.assertEqual(entry(LEDGER, "0x2400:00")["default_eds"], "16")
        self.assertEqual(entry(LEDGER, "0x1200:01")["default_eds"], "$NODEID+0x600")

    def test_every_entry_requires_completed_documentary_review(self):
        data = self.changed()
        entry(data, "0x251F:00")["review"]["status"] = "pending"
        self.reject(data, "documentary review incomplete")

    def test_required_feature_gates_cannot_be_removed(self):
        for key, gate in (("0x60FF:00", "Q03"), ("0x2300:00", "Q15"),
                          ("0x605D:00", "Q12"), ("0x2401:00", "Q08"),
                          ("0x251F:00", "Q09"), ("0x1600:01", "Q17"),
                          ("0x2301:00", "Q01")):
            with self.subTest(key=key, gate=gate):
                data = self.changed()
                entry(data, key)["review"]["gate_ids"].remove(gate)
                self.reject(data, "required feature or binding gate missing")

    def test_documentary_policy_cannot_authorize_writes(self):
        data = self.changed()
        entry(data, "0x2300:00")["review"]["write_policy"] = "approved"
        self.reject(data, "documentary review cannot authorize writes")

    def test_monitoring_and_opaque_settings_remain_read_only(self):
        for key in ("0x2100:00", "0x603F:00", "0x605D:00", "0x2400:0C",
                    "0x2401:00", "0x251F:00"):
            with self.subTest(key=key):
                data = self.changed()
                value = entry(data, key)
                value["access_reviewed"] = "RW"
                value["review"]["write_policy"] = "qualification_required"
                self.reject(data, "conservative read-only policy required")

    def test_unestablished_read_effect_cannot_be_qualified_by_prose(self):
        data = self.changed()
        entry(data, "0x6041:00")["review"].update(
            read_effect="non-consuming; verified", gate_ids=["Q01", "Q02"])
        self.reject(data, "read effect cannot claim qualification")

    def test_entry_owner_tracks_the_actual_feature(self):
        for key, wrong_owner in (("0x2301:00", 20), ("0x2300:00", 19),
                                  ("0x6098:00", 22)):
            with self.subTest(key=key):
                data = self.changed()
                entry(data, key)["owner_prompt"] = wrong_owner
                self.reject(data, "owner prompt invalid")

    def test_malformed_review_and_family_values_return_errors(self):
        for value in (None, {}, [None], [[]]):
            with self.subTest(value=value):
                data = self.changed()
                entry(data, "0x60FF:00")["review"]["gate_ids"] = value
                self.reject(data, "unknown or missing qualification gate")
        for value in ([], {}, None):
            with self.subTest(source_id=value):
                data = self.changed()
                data["semantic_review"]["families"][0]["source_refs"][0]["id"] = value
                self.reject(data, "source ID must be a string")

    def test_malformed_review_structures_fail_without_exceptions(self):
        paths = [
            ("semantic_review",), ("semantic_review", "gates"),
            ("semantic_review", "gates", 0), ("semantic_review", "gates", 0, "id"),
            ("semantic_review", "gates", 0, "models"),
            ("semantic_review", "families"), ("semantic_review", "families", 0),
            ("semantic_review", "families", 0, "model"),
            ("semantic_review", "families", 0, "source_refs"),
            ("semantic_review", "families", 0, "source_refs", 0, "id"),
            ("semantic_review", "eds_capabilities"),
            ("semantic_review", "eds_capabilities", "claims"),
            ("objects", 0, "entries", 0, "review"),
            ("objects", 0, "entries", 0, "review", "gate_ids"),
            ("objects", 0, "entries", 0, "prerequisites"),
        ]
        for path in paths:
            for value in (None, {}, [], [None], [[]]):
                with self.subTest(path=path, value=value):
                    data = self.changed()
                    parent = data
                    for key in path[:-1]:
                        parent = parent[key]
                    parent[path[-1]] = value
                    self.assertTrue(validate_ledger(data))

    def test_prerequisites_require_nonempty_text(self):
        for value in ([{}], [None], [""], [" "]):
            with self.subTest(value=value):
                data = self.changed()
                entry(data, "0x2300:00")["prerequisites"] = value
                self.reject(data, "reviewed semantics missing")

    def test_manual_action_obligation_cannot_be_dropped(self):
        data = self.changed()
        row = next(value for value in data["manual_only"] if value["key"] == "save_parameters")
        row["owner_prompt"] = 19
        row["review"].update(write_policy="not_writable", gate_ids=["Q19"])
        errors = validate_ledger(data)
        for fragment in ("manual owner obligation changed", "required manual feature or binding gate missing",
                         "manual action obligation changed"):
            self.assertTrue(any(fragment in value for value in errors), errors)

    def test_manual_inventory_cannot_shrink_or_replace_a_key(self):
        data = self.changed()
        data["manual_only"] = [value for value in data["manual_only"] if value["key"] != "tuning_settings"]
        data["accounting"]["manual_only_items"] = len(data["manual_only"])
        self.reject(data, "manual obligation inventory incomplete")
        data = self.changed()
        data["manual_only"][0]["key"] = "arbitrary_replacement"
        self.reject(data, "manual obligation inventory incomplete")

    def test_original_conflict_alternatives_and_refs_cannot_disappear(self):
        data = self.changed()
        data["conflicts"][0]["claims"] = data["conflicts"][0]["claims"][:2]
        self.reject(data, "original conflict claims or source references changed")
        data = self.changed()
        data["conflicts"][0]["claims"][-1]["source_ref"]["page"] = 9
        self.reject(data, "original conflict claims or source references changed")

    def test_gate_scope_cannot_merge_86c_and_57c_outputs(self):
        for gate_id in ("Q06", "Q07"):
            with self.subTest(gate=gate_id):
                data = self.changed()
                gate = next(value for value in data["semantic_review"]["gates"] if value["id"] == gate_id)
                gate["models"] = ["CL86-C", "OL86-C", "CL57-C", "OL57-C"]
                self.reject(data, "gate model applicability invalid")

    def test_encoder_absence_is_not_unknown_feedback(self):
        data = self.changed()
        family = next(value for value in data["semantic_review"]["families"] if value["model"] == "OL86-C")
        family["encoder_capability"] = "not_evidenced"
        self.reject(data, "family encoder capability invalid")

    def test_eds_capabilities_require_source_scope_and_exact_values(self):
        data = self.changed()
        data["semantic_review"]["eds_capabilities"]["scope_id"] = "manual_86"
        self.reject(data, "EDS capabilities must retain unbound source scope")
        data = self.changed()
        data["semantic_review"]["eds_capabilities"]["claims"]["LSS_Supported"] = "1"
        self.reject(data, "EDS capability claim differs from source")
        data = self.changed()
        del data["semantic_review"]["eds_capabilities"]["claims"]["SimpleBootUpSlave"]
        self.reject(data, "EDS capability claims incomplete")


if __name__ == "__main__":
    unittest.main()
