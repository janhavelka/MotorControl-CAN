#!/usr/bin/env python3
"""Check reviewed documentary policy and lossless original-source accounting.

This validates evidence structure, not firmware behavior. A complete review may
retain an open qualification gate; it must never silently grant writable policy.
"""

import argparse
from collections import Counter
import configparser
import hashlib
import json
from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
TYPE_CODES = {"0x0003": ("I16", 2, True), "0x0004": ("I32", 4, True),
              "0x0005": ("U8", 1, False), "0x0006": ("U16", 2, False),
              "0x0007": ("U32", 4, False)}
INDEX_KEY = re.compile(r"0x[0-9A-F]{4}\Z")
ENTRY_KEY = re.compile(r"0x[0-9A-F]{4}:[0-9A-F]{2}\Z")
INTEGER = re.compile(r"(?:0|[1-9][0-9]*|-?[1-9][0-9]*|0x[0-9A-Fa-f]+)\Z")
EXPRESSION = re.compile(r"\$NODEID\+0x[0-9A-Fa-f]+\Z")

# Prompt-2 intake at f25aa0b07b79ae617f2df139c23c459b7e6f8a50:
# SHA-256 of [{id, subject, scope_id, claims}, ...], serialized with sorted
# keys, compact separators and UTF-8 (ensure_ascii=False). Review decisions
# are excluded. Updating this preservation guard requires primary evidence
# and an explicit correction record, not merely a newly passing ledger.
ORIGINAL_CONFLICT_CLAIMS_SHA256 = "cf9fdece54e1cb61a542d3268ae8da8bc42c39c56411fe7ff45be1e69fa92537"
MODELS = {"CL86-C", "OL86-C", "CL57-C", "OL57-C"}
# These documentary conflicts do not establish a complete native signed type.
# Their raw EDS claims remain in the ledger and generated coverage, not in the
# resolved descriptor surface. This guard is independent of semantic gates.
WITHHELD_GENERATION_KEYS = {"0x6060:00", "0x6061:00", "0x6098:00", "0x60FF:00"}


def generation_disposition(entry):
    """Return the reviewed declaration disposition without granting operations."""
    if entry.get("role") == "reserved":
        return "excluded"
    if entry.get("key") in WITHHELD_GENERATION_KEYS:
        return "withheld"
    return "resolved"


def read_descriptor_representation(entry):
    """Return raw read representation without resolving interpretation or effects."""
    if generation_disposition(entry) == "withheld":
        return "RAW"
    return "SIGNED" if entry["type"]["signed"] else "UNSIGNED"


def read_owner_errors(entry):
    """Validate separate read ownership; only Prompt 19 produces a read table."""
    owner = entry.get("read_owner_prompt")
    label = str(entry.get("key")) + ": read owner "
    readable = (entry.get("role") != "reserved" and
                entry.get("access_eds") in ("RO", "RW") and
                entry.get("access_reviewed") in ("RO", "RW"))
    if owner is None:
        return [label + "required for a nonreserved readable source entry"] if readable else []
    if type(owner) is not int or not 1 <= owner <= 50:
        return [label + "must be an integer prompt in 1..50 or null"]
    if not readable:
        return [label + "requires a nonreserved readable source entry"]
    if owner != 19:
        return []
    index, subindex = entry.get("index"), entry.get("subindex")
    # Preserve the bounded reviewed assignment independently of later groups.
    scalar_indexes = {0x1000, 0x1001, 0x1005, 0x1009, 0x100A, 0x1014,
                      0x1017, 0x2000, 0x2001, 0x2100, 0x2101, 0x2102,
                      0x603F, 0x6041, 0x6061, 0x6064, 0x606C, 0x60FD}
    allowed = (type(index) is int and type(subindex) is int and
               ((index in scalar_indexes and subindex == 0) or
                (index == 0x1003 and 0 <= subindex <= 4) or
                (index == 0x1018 and 0 <= subindex <= 3) or
                (index == 0x1200 and 0 <= subindex <= 2)))
    if not allowed:
        return [label + "outside the reviewed Prompt 19 read scope"]
    disposition = generation_disposition(entry)
    if disposition != "resolved":
        if (entry.get("key") != "0x6061:00" or disposition != "withheld" or
                entry.get("type") != {"eds_code": "0x0005", "name": "U8",
                                      "width_bytes": 1, "signed": False}):
            return [label + "unresolved read type is not the approved raw mode byte"]
    return []


def read_coverage_errors(entry):
    """Check software read coverage separately from whole-object effect coverage."""
    if "read_coverage" not in entry:
        return []
    label = str(entry.get("key")) + ": read coverage "
    value = entry["read_coverage"]
    fields = {"documented", "implemented", "host_tested", "hardware_qualified"}
    if not isinstance(value, dict) or set(value) != fields:
        return [label + "fields invalid"]
    if any(type(v) is not bool for v in value.values()):
        return [label + "must be boolean"]
    errors = []
    if entry.get("read_owner_prompt") != 19 or read_owner_errors(entry):
        errors.append(label + "requires a reviewed Prompt 19 read")
    if value["documented"] is not True:
        errors.append(label + "must retain documented source evidence")
    if value["host_tested"] and not value["implemented"]:
        errors.append(label + "host tests require implemented read behavior")
    if value["hardware_qualified"]:
        errors.append(label + "cannot claim hardware qualification")
    return errors


def pdo_observation_errors(objects):
    """Check passive layout claims against the reviewed default mapping records.

    These facts permit raw layout validation only. They leave every effect,
    service-access and live-source qualification gate independent.
    """
    errors = []
    rows = {}
    for obj in objects:
        entries = obj.get("entries") if isinstance(obj, dict) else None
        if isinstance(entries, list):
            rows.update((entry["key"], entry) for entry in entries
                        if isinstance(entry, dict) and isinstance(entry.get("key"), str))
    claims = {}
    for base, direction in ((0x1600, "RPDO"), (0x1A00, "TPDO")):
        for number in range(4):
            index = base + number
            count = rows.get("0x{:04X}:00".format(index), {}).get("default_eds")
            if not isinstance(count, str) or not INTEGER.fullmatch(count):
                errors.append("PDO observation: missing source mapping count")
                continue
            active = int(count, 16) if count.startswith("0x") else int(count, 10)
            if not 0 <= active <= 4:
                errors.append("PDO observation: source mapping count outside four slots")
                continue
            for slot in range(1, active + 1):
                mapped = rows.get("0x{:04X}:{:02X}".format(index, slot), {})
                word = mapped.get("default_eds")
                if not isinstance(word, str) or not INTEGER.fullmatch(word):
                    errors.append("PDO observation: missing source mapping word")
                    continue
                value = int(word, 0)
                key = "0x{:04X}:{:02X}".format(value >> 16, (value >> 8) & 255)
                claim = claims.setdefault(key, {"directions": set(), "refs": [], "bits": []})
                claim["directions"].add(direction)
                refs = mapped.get("source_refs")
                if isinstance(refs, list):
                    claim["refs"].extend(refs)
                claim["bits"].append(value & 255)
    if len(claims) != 10:
        errors.append("PDO observation: expected ten original default-mapped targets")
    for key, row in rows.items():
        review = row.get("review", {})
        observation = review.get("pdo_observation") if isinstance(review, dict) else None
        claim = claims.get(key)
        if claim is None:
            if observation is not None:
                errors.append(str(key) + ": PDO observation not supported by default mapping source")
            continue
        label = str(key) + ": PDO observation "
        if not isinstance(observation, dict):
            errors.append(label + "review missing")
            continue
        if set(observation) != {"directions", "role", "representation", "source_refs",
                                "live_qualification_required"}:
            errors.append(label + "fields invalid")
        directions = observation.get("directions")
        if (not isinstance(directions, list) or directions != sorted(claim["directions"])):
            errors.append(label + "directions differ from source mapping words")
        native = row.get("type")
        native = native if isinstance(native, dict) else {}
        width = native.get("width_bytes")
        if (row.get("role") == "reserved" or row.get("pdo_mapping_eds") != "1" or
                row.get("pdo_mapping_reviewed") is not None or type(width) is not int or
                any(bits != width * 8 for bits in claim["bits"])):
            errors.append(label + "ineligible target or source width mismatch")
        representation = ("RAW" if generation_disposition(row) == "withheld" else
                          "SIGNED" if native.get("signed") is True else "UNSIGNED")
        if observation.get("representation") != representation:
            errors.append(label + "representation conflicts with reviewed native type")
        # These are semantic preservation guards, not a second object catalogue.
        expected_role = ("ACTUAL" if key in {"0x6064:00", "0x606C:00"} else
                         "COMMANDED" if key in {"0x607A:00", "0x6081:00", "0x60FF:00"} else "RAW")
        if observation.get("role") != expected_role:
            errors.append(label + "role conflicts with reviewed object meaning")
        refs = observation.get("source_refs")
        target_refs = row.get("source_refs")
        target_refs = target_refs if isinstance(target_refs, list) else []
        required = claim["refs"] + [ref for ref in target_refs
                                    if isinstance(ref, dict) and ref.get("id") == "EDS"]
        allowed = claim["refs"] + target_refs
        if (not isinstance(refs, list) or not refs or
                any(ref not in refs for ref in required) or any(ref not in allowed for ref in refs)):
            errors.append(label + "source refs missing or outside reviewed target/mapping evidence")
        if observation.get("live_qualification_required") is not True:
            errors.append(label + "cannot waive live qualification")
    for key in claims.keys() - rows.keys():
        errors.append(key + ": PDO observation target absent")
    return errors

# Stable prompt-3 obligations: owner, mandatory gates, documentary write policy.
# These guard loss of assigned scope; they are not another object dictionary.
MANUAL_REQUIREMENTS = {
    "mode_selection": (20, "Q01 Q10 Q11", "qualification_required"),
    "enable_transition": (30, "Q01 Q11", "qualification_required"),
    "position_parameters": (33, "Q01 Q04 Q11 Q02", "qualification_required"),
    "position_trigger": (33, "Q01 Q04 Q11 Q02", "qualification_required"),
    "position_update_policy": (33, "Q01 Q04 Q11 Q02", "qualification_required"),
    "position_coordinate_mode": (33, "Q01 Q04 Q11 Q02", "qualification_required"),
    "position_transition_examples": (33, "Q01 Q04 Q11 Q02", "qualification_required"),
    "normal_stop": (31, "Q01 Q12", "qualification_required"),
    "emergency_stop": (31, "Q01 Q12", "qualification_required"),
    "status_interpretation": (19, "Q01 Q02", "not_writable"),
    "velocity_parameters": (35, "Q01 Q03 Q11", "qualification_required"),
    "velocity_start_order": (35, "Q01 Q03", "qualification_required"),
    "homing_parameters": (36, "Q01 Q21 Q02 Q14", "qualification_required"),
    "homing_negative_limit_17": (36, "Q01 Q21 Q02 Q14", "qualification_required"),
    "homing_positive_limit_18": (36, "Q01 Q21 Q02 Q14", "qualification_required"),
    "homing_positive_origin_24": (36, "Q01 Q21 Q02 Q14", "qualification_required"),
    "homing_negative_origin_29": (36, "Q01 Q21 Q02 Q14", "qualification_required"),
    "homing_trigger_and_interrupt": (36, "Q01 Q21 Q02 Q14", "qualification_required"),
    "homing_status": (36, "Q01 Q21 Q02 Q14", "qualification_required"),
    "clear_current_position": (32, "Q01 Q14", "qualification_required"),
    "save_parameters": (39, "Q01 Q15", "qualification_required"),
    "restore_factory_parameters": (39, "Q01 Q15", "qualification_required"),
    "fault_reset_controlword": (32, "Q01 Q13", "qualification_required"),
    "fault_reset_vendor_object": (32, "Q01 Q13", "qualification_required"),
    "motion_monitor": (19, "Q01 Q20", "not_writable"),
    "digital_input_observation": (19, "Q01 Q20", "not_writable"),
    "digital_output_observation": (19, "Q01 Q20", "not_writable"),
    "default_direction": (20, "Q01 Q04", "qualification_required"),
    "subdivision_and_position_scale": (20, "Q01 Q04", "qualification_required"),
    "software_limit_enable": (20, "Q01 Q05", "qualification_required"),
    "software_node_setting": (38, "Q01 Q16", "qualification_required"),
    "input_polarity": (21, "Q01 Q19", "qualification_required"),
    "input_function_assignment": (21, "Q01 Q19", "qualification_required"),
    "input_filter": (21, "Q01 Q08", "qualification_required"),
    "output_polarity": (21, "Q01 Q06", "qualification_required"),
    "output_function_assignment": (21, "Q01 Q06", "qualification_required"),
    "user_output_command_and_mask": (21, "Q01 Q06", "qualification_required"),
    "brake_output_wiring": (21, "Q01 Q06 Q19", "qualification_required"),
    "drive_loop_mode": (22, "Q01 Q09", "qualification_required"),
    "encoder_resolution": (22, "Q01 Q09 Q04", "qualification_required"),
    "current_settings": (22, "Q01 Q09", "qualification_required"),
    "tuning_settings": (22, "Q01 Q09", "qualification_required"),
    "error_codes": (19, "Q01 Q20", "not_writable"),
    "led_alarm_and_reset_class": (19, "Q01 Q20", "not_writable"),
    "dip_node_selection": (38, "Q01 Q16", "qualification_required"),
    "dip_bitrate_selection": (38, "Q01 Q16", "qualification_required"),
    "termination_and_cabling": (47, "Q01 Q19", "not_writable"),
    "electrical_input_output_contract": (47, "Q01 Q19", "not_writable"),
    "product_protocol_and_geometry": (37, "Q01", "not_writable"),
    "supply_voltage_applicability": (47, "Q01 Q19", "not_writable"),
    "status_interpretation_57": (19, "Q01 Q02", "not_writable"),
    "output_terminal_labels_57": (21, "Q01 Q07", "qualification_required"),
    "supply_voltage_applicability_57": (47, "Q01 Q19", "not_writable"),
}


def entry_requirements(index, sub):
    """Return the assigned owner and feature gates; source facts stay in the ledger."""
    if 0x1400 <= index <= 0x1403 or 0x1800 <= index <= 0x1803:
        return (17 if sub == 0 else 18), "Q17"
    if 0x1600 <= index <= 0x1603 or 0x1A00 <= index <= 0x1A03:
        return 18, "Q17"
    if index == 0x1200:
        return (13, "") if sub == 0 else (38, "Q16")
    if index == 0x2400:
        if sub == 0:
            return 21, ""
        return 21, "Q19" if sub <= 8 else ("Q08" if sub == 10 else "Q06")
    if 0x2401 <= index <= 0x2407:
        return 21, "Q08"
    if 0x2500 <= index <= 0x2520:
        return 22, "Q09 Q04" if index in (0x2501, 0x250A, 0x250B) else "Q09"
    if index == 0x1018:
        return 19, ""
    if index in (0x607D, 0x6099, 0x60FE):
        owner, gates = {0x607D: (20, "Q05"), 0x6099: (36, "Q21"),
                        0x60FE: (21, "Q06 Q07")}[index]
        return owner, gates if sub else ""
    return {
        0x1000: (19, ""), 0x1001: (19, "Q18"), 0x1003: (19, "Q18"),
        0x1005: (38, "Q16"), 0x1009: (19, ""), 0x100A: (19, ""),
        0x1014: (38, "Q16"), 0x1017: (38, "Q18"),
        0x2000: (19, ""), 0x2001: (19, ""),
        0x2100: (19, "Q02"), 0x2101: (19, ""), 0x2102: (19, ""),
        0x2200: (20, "Q04"), 0x2201: (20, "Q04"), 0x2202: (20, "Q05"),
        0x2203: (38, "Q16"), 0x2300: (39, "Q15"),
        0x2301: (32, "Q13"), 0x2302: (32, "Q14"),
        0x603F: (19, ""), 0x6040: (30, "Q11 Q12 Q13"),
        0x6041: (19, "Q02"), 0x605D: (31, "Q12"),
        0x6060: (20, "Q10 Q11"), 0x6061: (19, "Q10"),
        0x6064: (19, "Q04"), 0x606C: (19, "Q04"),
        0x607A: (33, "Q04 Q11"), 0x607C: (32, "Q14 Q21"),
        0x6081: (20, "Q11"), 0x6083: (20, "Q11"), 0x6084: (20, "Q11"),
        0x6098: (36, "Q10 Q21"), 0x609A: (36, "Q11 Q21"),
        0x60FD: (19, "Q19"), 0x60FF: (35, "Q03"),
    }.get(index, (None, ""))


def load_json(path):
    """Reject duplicate JSON member names, including in nested raw fields."""
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError("duplicate JSON member: " + key)
            result[key] = value
        return result
    return json.loads(Path(path).read_text(encoding="utf-8"), object_pairs_hook=unique)


def eds_sections(path):
    content = path.read_text(encoding="utf-8-sig")
    eds = configparser.ConfigParser(interpolation=None)
    eds.optionxform = str
    eds.read_string(content)
    starts = [(m.group(1), line) for line, value in enumerate(content.splitlines(), 1)
              for m in [re.fullmatch(r"\[([^]]+)\]", value.strip())] if m]
    spans = {name: (start, starts[i + 1][1] - 1 if i + 1 < len(starts)
                    else len(content.splitlines()))
             for i, (name, start) in enumerate(starts)}
    return eds, spans


def validate_ledger(data, root=ROOT):
    errors = []

    def require(ok, message):
        if not ok:
            errors.append(message)

    if not isinstance(data, dict):
        return ["ledger root must be an object"]
    require(data.get("schema_version") == 2, "schema_version must be 2")
    require(data.get("source_manifest") == "docs/reference/SOURCES.json", "wrong source manifest")
    manifest = load_json(root / "docs/reference/SOURCES.json")
    required_sources = {"EDS", "EN86", "ZH86", "OP86", "ZH57", "OP57"}
    manifest_sources = manifest.get("sources") if isinstance(manifest, dict) else None
    if not isinstance(manifest_sources, list) or any(not isinstance(row, dict) or
            not isinstance(row.get("id"), str) for row in manifest_sources):
        return errors + ["source manifest has invalid rows"]
    manifest_ids = [row["id"] for row in manifest_sources]
    if len(manifest_ids) != len(set(manifest_ids)) or not required_sources.issubset(manifest_ids):
        return errors + ["source manifest has duplicate or missing required IDs"]
    originals = {row["id"]: row for row in manifest_sources}
    for sid in required_sources:
        original = originals[sid]
        if (not all(isinstance(original.get(field), str) for field in
                    ("title", "path", "url", "sha256", "revision")) or
                not isinstance(original.get("applicability"), list) or
                (sid != "EDS" and type(original.get("pdf_pages")) is not int)):
            return errors + ["{}: source manifest metadata invalid".format(sid)]
    sources = data.get("sources")
    if not isinstance(sources, list):
        return errors + ["sources must be an array"]
    seen_sources = set()
    for source in sources:
        if not isinstance(source, dict):
            errors.append("source must be an object")
            continue
        sid = source.get("id")
        if not isinstance(sid, str):
            errors.append("source ID must be a string")
            continue
        require(sid not in seen_sources, "duplicate source: {}".format(sid))
        seen_sources.add(sid)
        original = originals.get(sid)
        require(original is not None, "unknown source: {}".format(sid))
        if original is None:
            continue
        for field, expected in (("manifest_title", original["title"]),
                                ("acquired_path", "docs/reference/" + original["path"]),
                                ("url", original["url"]), ("sha256", original["sha256"]),
                                ("revision", original["revision"]),
                                ("models_named", original["applicability"]),
                                ("pdf_pages", original.get("pdf_pages"))):
            require(field in source and source.get(field) == expected,
                    "{}: {} differs from manifest".format(sid, field))
        require(bool(source.get("applicability_evidence")) and bool(source.get("printed_title")),
                "{}: missing cover/title or applicability evidence".format(sid))
        source_path = (root / "docs/reference" / original["path"]).resolve()
        require(source_path.is_file() and root.resolve() in source_path.parents,
                "{}: source file missing or outside checkout".format(sid))
        if source_path.is_file():
            require(hashlib.sha256(source_path.read_bytes()).hexdigest() == source.get("sha256"),
                    "{}: original SHA-256 mismatch".format(sid))
    require(seen_sources == required_sources, "source set differs from EDS and five PDFs")

    scopes = data.get("scopes")
    if not isinstance(scopes, list) or not scopes:
        return errors + ["scopes must be a nonempty array"]
    scope_ids = set()
    expected_scopes = {
        "eds_family_unbound": ([], "Lichuan CANopen stepper drive family",
                               "LC_StepDriverCAN_V1.1.eds", "unverified"),
        "manual_86": (originals["EN86"]["applicability"], "86C CANopen drives",
                      "EN86/ZH86/OP86 claims kept per source",
                      "documentary cover only; OP86 internal 57C headers conflict"),
        "manual_57_contrast": (originals["ZH57"]["applicability"], "57C CANopen drives",
                               "ZH57/OP57 contrast claims kept per source",
                               "documentary cover only; not an implemented profile"),
    }
    for scope in scopes:
        if not isinstance(scope, dict):
            errors.append("scope must be an object")
            continue
        sid = scope.get("id")
        if not isinstance(sid, str) or not sid:
            errors.append("scope ID must be a nonempty string")
            continue
        require(sid not in scope_ids, "duplicate scope id")
        scope_ids.add(sid)
        expected = expected_scopes.get(sid)
        if expected is None:
            errors.append("{}: unknown source scope".format(sid))
            continue
        models, family, dialect, binding = expected
        require(scope.get("manufacturer") == "Lichuan" and scope.get("family") == family and
                scope.get("dialect") == dialect,
                "{}: manufacturer/family/dialect applicability invalid".format(sid))
        require(scope.get("models") == models,
                "{}: models differ from source applicability".format(sid))
        require("firmware" in scope and scope.get("firmware") is None and
                scope.get("binding") == binding,
                "{}: prompt-2 firmware/binding must remain unverified".format(sid))
    require(scope_ids == set(expected_scopes), "scope set differs from source intake")

    eds_path = root / "docs/reference/vendor/LC_StepDriverCAN_V1.1.eds"
    eds, spans = eds_sections(eds_path)
    require(data.get("eds_source_sha256") == hashlib.sha256(eds_path.read_bytes()).hexdigest(),
            "EDS source SHA-256 mismatch")
    metadata = data.get("eds_metadata")
    require(isinstance(metadata, dict), "EDS metadata must be an object")
    if not isinstance(metadata, dict):
        metadata = {}
    for key, section in (("file_info", "FileInfo"), ("device_info", "DeviceInfo")):
        require(metadata.get(key) == dict(eds[section]), "EDS metadata mismatch: " + key)
    require(metadata.get("object_lists") == {key: dict(eds[key]) for key in
            ("MandatoryObjects", "ManufacturerObjects", "OptionalObjects")},
            "EDS object lists differ from original")
    require(data.get("eds_other_sections") == {key: dict(eds[key]) for key in
            ("Comments", "DummyUsage", "DynamicChannels")},
            "EDS nonobject sections differ from original")

    expected_headers = {name for name in eds.sections() if re.fullmatch(r"[0-9A-Fa-f]{4}", name)}
    expected_entries = {name for name in eds.sections() if "DataType" in eds[name]}
    listed = {}
    for list_name in ("MandatoryObjects", "ManufacturerObjects", "OptionalObjects"):
        for field, value in eds[list_name].items():
            if field != "SupportedObjects":
                index = int(value, 0)
                require(index not in listed, "duplicate EDS list index")
                listed[index] = list_name

    def check_ref(ref, label):
        if not isinstance(ref, dict):
            errors.append(label + ": source reference must be an object")
            return
        sid = ref.get("id")
        if not isinstance(sid, str):
            errors.append(label + ": source ID must be a string")
            return
        require(sid in seen_sources, label + ": unknown source ID")
        if sid == "EDS":
            section = ref.get("section")
            if not isinstance(section, str):
                errors.append(label + ": EDS section must be a string")
                return
            require(section in eds, label + ": missing EDS section")
            if section in eds:
                require((ref.get("line_start"), ref.get("line_end")) == spans[section],
                        label + ": EDS line span mismatch")
        elif sid in required_sources:
            pages = originals[sid].get("pdf_pages")
            require(type(ref.get("page")) is int and 1 <= ref["page"] <= pages,
                    label + ": PDF page invalid")

    def check_coverage(value, label):
        expected = {"documented", "implemented", "host_tested", "hardware_qualified"}
        require(isinstance(value, dict) and set(value) == expected, label + ": coverage fields invalid")
        if isinstance(value, dict):
            require(all(type(v) is bool for v in value.values()), label + ": coverage must be boolean")
            require(value.get("documented") is True and all(value.get(k) is False for k in expected - {"documented"}),
                    label + ": intake cannot claim implementation, test or qualification")

    review = data.get("semantic_review")
    gate_ids = set()
    require(isinstance(review, dict), "semantic review missing")
    if isinstance(review, dict):
        require(review.get("status") == "complete" and isinstance(review.get("binding_policy"), str)
                and bool(review["binding_policy"].strip()),
                "documentary review must be complete with explicit binding policy")
        gates = review.get("gates")
        require(isinstance(gates, list) and bool(gates), "qualification gates missing")
        for gate in gates if isinstance(gates, list) else []:
            if not isinstance(gate, dict) or not isinstance(gate.get("id"), str):
                errors.append("invalid qualification gate")
                continue
            require(gate["id"] not in gate_ids, "duplicate qualification gate")
            gate_ids.add(gate["id"])
            require(gate.get("status") == "open" and all(isinstance(gate.get(field), str)
                    and bool(gate[field].strip()) for field in ("feature", "required_evidence")),
                    "gate needs feature and required observation")
            owners = gate.get("owner_prompts")
            require(isinstance(owners, list) and bool(owners) and
                    all(type(n) is int and 12 <= n <= 48 for n in owners), "gate owners invalid")
            models = gate.get("models")
            expected_models = {"CL86-C", "OL86-C"} if gate["id"] == "Q06" else (
                {"CL57-C", "OL57-C"} if gate["id"] == "Q07" else MODELS)
            require(isinstance(models, list) and all(isinstance(m, str) for m in models)
                    and len(models) == len(expected_models) and set(models) == expected_models,
                    "gate model applicability invalid")
        require(gate_ids == {"Q{:02d}".format(n) for n in range(1, 22)},
                "qualification gate inventory incomplete")
        families = review.get("families")
        require(isinstance(families, list), "family matrix missing")
        models = []
        for family in families if isinstance(families, list) else []:
            if not isinstance(family, dict):
                errors.append("family must be an object")
                continue
            model = family.get("model")
            models.append(model)
            require(isinstance(model, str) and model in MODELS, "family model invalid")
            require(family.get("firmware") is None and "firmware" in family and
                    family.get("binding") == "user_selected_unqualified" and
                    family.get("implemented") is False and family.get("hardware_qualified") is False,
                    "family cannot claim a firmware binding or implementation")
            require(all(isinstance(family.get(field), str) and bool(family[field].strip())
                    for field in ("motor_feedback", "limitations")),
                    "family motor/feedback limitations missing")
            require(family.get("encoder_capability") == ("documented_present" if
                    isinstance(model, str) and model.startswith("CL") else "explicitly_absent"),
                    "family encoder capability invalid")
            caps = family.get("capabilities")
            require(caps == {"position": "documented_unqualified", "velocity": "documented_unqualified",
                            "homing": "documented_unqualified", "canopen": "documented_unqualified",
                            "torque": "not_evidenced", "cyclic_modes": "not_evidenced",
                            "servo_motor": "not_evidenced"}, "family capability evidence invalid")
            refs = family.get("source_refs")
            require(isinstance(refs, list) and bool(refs), "family sources missing")
            allowed = {"EN86", "ZH86", "OP86"} if model in ("CL86-C", "OL86-C") else {"ZH57", "OP57"}
            for ref in refs if isinstance(refs, list) else []:
                check_ref(ref, str(model))
                require(isinstance(ref, dict) and isinstance(ref.get("id"), str) and ref["id"] in allowed,
                        "family source model mismatch")
        require(sorted(str(m) for m in models) == ["CL57-C", "CL86-C", "OL57-C", "OL86-C"],
                "four separate model entries required")

        capabilities = review.get("eds_capabilities")
        require(isinstance(capabilities, dict), "explicit EDS capabilities missing")
        if isinstance(capabilities, dict):
            require(capabilities.get("scope_id") == "eds_family_unbound",
                    "EDS capabilities must retain unbound source scope")
            ref = capabilities.get("source_ref")
            check_ref(ref, "EDS capabilities")
            require(isinstance(ref, dict) and ref.get("id") == "EDS" and
                    ref.get("section") == "DeviceInfo", "EDS capabilities must cite DeviceInfo")
            claims = capabilities.get("claims")
            fields = {"SimpleBootUpSlave", "SimpleBootUpMaster", "DynamicChannelsSupported",
                      "GroupMessaging", "LSS_Supported"}
            require(isinstance(claims, dict) and set(claims) == fields,
                    "EDS capability claims incomplete")
            if isinstance(claims, dict):
                for field in fields:
                    value = claims.get(field)
                    original = eds["DeviceInfo"][field]
                    require(isinstance(value, str) and bool(INTEGER.fullmatch(value)) and
                            int(value, 0) == int(original, 0),
                            "EDS capability claim differs from source: " + field)
            require(isinstance(capabilities.get("interpretation"), str) and
                    bool(capabilities["interpretation"].strip()), "EDS capability interpretation missing")

    def check_review(value, label, fields):
        if not isinstance(value, dict):
            errors.append(label + ": documentary review missing")
            return set()
        require(value.get("status") == "complete", label + ": documentary review incomplete")
        for field in fields:
            require(isinstance(value.get(field), str) and bool(value[field].strip()),
                    label + ": review field missing: " + field)
        ids = value.get("gate_ids")
        require(isinstance(ids, list) and bool(ids) and
                all(isinstance(cid, str) and cid in gate_ids for cid in ids),
                label + ": unknown or missing qualification gate")
        if "write_policy" in fields:
            require(value.get("write_policy") in ("not_writable", "qualification_required", "reserved"),
                    label + ": documentary review cannot authorize writes")
        return {cid for cid in ids if isinstance(cid, str)} if isinstance(ids, list) else set()

    objects = data.get("objects")
    if not isinstance(objects, list):
        return errors + ["objects must be an array"]
    seen_headers, seen_entries, type_counts, access_counts, pdo_counts = set(), set(), Counter(), Counter(), Counter()
    reserved_count = 0
    for obj in objects:
        if not isinstance(obj, dict):
            errors.append("object row must be an object")
            continue
        label = str(obj.get("key"))
        index = obj.get("index")
        if type(index) is not int or not 0 <= index <= 0xFFFF:
            errors.append(label + ": index must be 0..65535")
            continue
        if not isinstance(obj.get("scope_id"), str):
            errors.append(label + ": scope ID must be a string")
            continue
        key = "0x{:04X}".format(index) if type(index) is int and 0 <= index <= 0xFFFF else None
        require(bool(INDEX_KEY.fullmatch(label)) and label == key, label + ": invalid index/key")
        require((obj.get("scope_id"), index) not in seen_headers, label + ": duplicate object key in scope")
        seen_headers.add((obj.get("scope_id"), index))
        require(obj.get("scope_id") == "eds_family_unbound", label + ": EDS object scope mismatch")
        require(obj.get("object_list") == listed.get(index), label + ": list membership mismatch")
        ref = obj.get("source_ref")
        check_ref(ref, label)
        require(isinstance(ref, dict) and ref.get("id") == "EDS",
                label + ": imported header must cite EDS")
        section = ref.get("section") if isinstance(ref, dict) else None
        require(isinstance(section, str) and section in expected_headers,
                label + ": header section missing")
        if not isinstance(section, str) or section not in expected_headers:
            continue
        fields = dict(eds[section])
        require(obj.get("raw_fields") == fields, label + ": header raw fields differ from EDS")
        require(index == int(section, 16), label + ": header index mismatch")
        object_type = fields.get("ObjectType")
        kind = {"7": "scalar", "8": "array", "9": "record"}.get(object_type)
        require(obj.get("kind") == kind, label + ": kind/ObjectType mismatch")
        entries = obj.get("entries")
        if not isinstance(entries, list):
            errors.append(label + ": entries must be an array")
            continue
        if kind in ("record", "array"):
            require("SubNumber" in fields and int(fields.get("SubNumber", "0"), 0) == len(entries),
                    label + ": SubNumber does not match declared entries")
        else:
            require(len(entries) == 1, label + ": scalar root must have one typed entry")
        for entry in entries:
            if not isinstance(entry, dict):
                errors.append(label + ": entry must be an object")
                continue
            sub = entry.get("subindex")
            if type(sub) is not int or not 0 <= sub <= 0xFF:
                errors.append(label + ": subindex must be 0..255")
                continue
            if not isinstance(entry.get("scope_id"), str):
                errors.append(label + ": entry scope ID must be a string")
                continue
            ekey = "0x{:04X}:{:02X}".format(index, sub) if type(sub) is int and 0 <= sub <= 0xFF else None
            elabel = str(entry.get("key"))
            require(bool(ENTRY_KEY.fullmatch(elabel)) and elabel == ekey and
                    type(entry.get("index")) is int and entry.get("index") == index,
                    elabel + ": invalid entry key/address")
            identity = (entry.get("scope_id"), index, sub)
            require(identity not in seen_entries, elabel + ": duplicate entry address in scope")
            seen_entries.add(identity)
            require(entry.get("scope_id") == obj.get("scope_id"), elabel + ": entry scope mismatch")
            refs = entry.get("source_refs")
            require(isinstance(refs, list) and bool(refs), elabel + ": source refs missing")
            if not isinstance(refs, list) or not refs:
                continue
            for item in refs:
                check_ref(item, elabel)
            eds_refs = [item for item in refs if isinstance(item, dict) and item.get("id") == "EDS"]
            require(len(eds_refs) == 1, elabel + ": exactly one EDS section required")
            if len(eds_refs) != 1 or not isinstance(eds_refs[0].get("section"), str) or eds_refs[0]["section"] not in eds:
                continue
            section_name = eds_refs[0]["section"]
            require(section_name in expected_entries, elabel + ": not a typed EDS section")
            expected_section = section if kind == "scalar" else "{}sub{:X}".format(section, sub)
            require(section_name.lower() == expected_section.lower(), elabel + ": EDS section/address mismatch")
            raw = dict(eds[section_name])
            require(entry.get("raw_fields") == raw, elabel + ": raw fields differ from original EDS")
            code = raw.get("DataType")
            require(code in TYPE_CODES, elabel + ": unknown EDS datatype")
            if code not in TYPE_CODES:
                continue
            name, width, signed = TYPE_CODES[code]
            native_type = entry.get("type")
            require(isinstance(native_type, dict) and
                    type(native_type.get("width_bytes")) is int and
                    type(native_type.get("signed")) is bool and
                    native_type == {"eds_code": code, "name": name,
                                    "width_bytes": width, "signed": signed},
                    elabel + ": datatype/width/signedness mismatch")
            require(entry.get("access_eds") == raw.get("AccessType"), elabel + ": access spelling mismatch")
            require(entry.get("pdo_mapping_eds") == raw.get("PDOMapping"), elabel + ": PDO flag mismatch")
            access = entry.get("access_reviewed")
            allowed_access = {"RO": ("RO",), "WO": ("WO",), "RW": ("RO", "RW")}
            require("access_reviewed" in entry and (access in allowed_access.get(raw.get("AccessType"), ())
                    or (entry.get("role") == "reserved" and access is None)),
                    elabel + ": reviewed access cannot widen source permission")
            require("pdo_mapping_reviewed" in entry and
                    (entry.get("pdo_mapping_reviewed") is False or
                     (entry.get("pdo_mapping_reviewed") is None and raw.get("PDOMapping") == "1")),
                    elabel + ": unqualified PDO mapping permission")
            for field, raw_name in (("low_limit_eds", "LowLimit"), ("high_limit_eds", "HighLimit"),
                                    ("default_eds", "DefaultValue")):
                value = entry.get(field)
                require(field in entry and value == raw.get(raw_name),
                        elabel + ": {} absent/value changed".format(field))
                if value is not None:
                    require(isinstance(value, str) and bool(INTEGER.fullmatch(value) or
                            (field == "default_eds" and EXPRESSION.fullmatch(value))),
                            elabel + ": invalid numeric/expression syntax")
                    if isinstance(value, str) and INTEGER.fullmatch(value):
                        number = int(value, 0) if value.startswith("0x") else int(value, 10)
                        minimum = -(1 << (8 * width - 1)) if signed else 0
                        maximum = (1 << (8 * width - (1 if signed else 0))) - 1
                        require(minimum <= number <= maximum, elabel + ": numeric value exceeds datatype")
            require(entry.get("role") == ("reserved" if (index, sub) in
                    {(0x2400, x) for x in (9, 10, 11, 16)} else
                    ("count" if kind in ("record", "array") and sub == 0 else "value")),
                    elabel + ": role mismatch")
            if entry.get("role") == "reserved":
                reserved_count += 1
                require(entry.get("disposition") == "excluded" and bool(entry.get("exclusion_reason")),
                        elabel + ": reserved entry must be excluded with reason")
            else:
                require(entry.get("disposition") in ("supported_evidence", "conservative_read_only", "pending_qualification") and
                        "exclusion_reason" in entry and entry.get("exclusion_reason") is None,
                        elabel + ": reviewed entry disposition invalid")
            require(entry.get("confidence") == "documented_metadata", elabel + ": confidence must be metadata only")
            expected_owner, feature_gates = entry_requirements(index, sub)
            require(type(entry.get("owner_prompt")) is int and expected_owner is not None and
                    entry["owner_prompt"] == expected_owner,
                    elabel + ": owner prompt invalid")
            errors.extend(read_owner_errors(entry))
            errors.extend(read_coverage_errors(entry))
            require(all(isinstance(entry.get(field), str) and bool(entry[field].strip()) for field in
                    ("units", "scaling", "effect", "edge_behavior")) and
                    isinstance(entry.get("prerequisites"), list) and bool(entry["prerequisites"]) and
                    all(isinstance(value, str) and bool(value.strip()) for value in entry["prerequisites"]),
                    elabel + ": reviewed semantics missing")
            selected_gates = check_review(entry.get("review"), elabel,
                                          ("type_policy", "value_policy", "read_effect", "write_policy"))
            required_gates = {"Q01"} | set(feature_gates.split())
            if raw.get("AccessType") != "WO":
                required_gates.add("Q20")
            require(required_gates <= selected_gates, elabel + ": required feature or binding gate missing")
            policy = entry.get("review", {})
            if isinstance(policy, dict):
                require(policy.get("generation_type") == generation_disposition(entry),
                        elabel + ": missing or inconsistent generation type disposition")
                require(policy.get("read_effect") == "not_established",
                        elabel + ": read effect cannot claim qualification")
                if (index in (0x2100, 0x2101, 0x2102, 0x603F, 0x605D) or
                        (index == 0x2400 and 12 <= sub <= 15) or
                        0x2401 <= index <= 0x2407 or 0x2500 <= index <= 0x2520):
                    require(access == "RO", elabel + ": conservative read-only policy required")
                if entry.get("role") == "reserved":
                    require(access is None and entry.get("pdo_mapping_reviewed") is False and
                            policy.get("write_policy") == "reserved", elabel + ": reserved policy cannot grant access")
                elif access == "RO":
                    require(policy.get("write_policy") == "not_writable", elabel + ": read-only policy cannot grant writes")
                else:
                    require(policy.get("write_policy") == "qualification_required",
                            elabel + ": command or setting needs qualification")
            require(isinstance(entry.get("conflict_ids"), list), elabel + ": conflict IDs must be an array")
            check_coverage(entry.get("coverage"), elabel)
            type_counts[name] += 1
            access_counts[raw.get("AccessType")] += 1
            pdo_counts[raw.get("PDOMapping")] += 1
    require({index for _, index in seen_headers} == {int(s, 16) for s in expected_headers}
            and len(seen_headers) == len(expected_headers), "EDS object headers not accounted exactly once")
    errors.extend(pdo_observation_errors(objects))
    require(len(seen_entries) == len(expected_entries), "EDS typed entries not accounted exactly once")
    gap_ranges = [{"first": "0x{:04X}".format(a + 1), "last": "0x{:04X}".format(b - 1)}
                  for a, b in zip(sorted(listed), sorted(listed)[1:]) if b > a + 1]
    require(data.get("index_gaps") == gap_ranges, "index gap ranges changed")

    manual = data.get("manual_only")
    if not isinstance(manual, list):
        errors.append("manual_only must be an array")
        manual = []
    seen_manual, seen_manual_addresses = set(), set()
    eds_addresses = {"0x{:04X}:{:02X}".format(index, sub) for _, index, sub in seen_entries}
    manual_sources = {"manual_86": {"EN86", "ZH86", "OP86"},
                      "manual_57_contrast": {"ZH57", "OP57"}}
    for item in manual:
        if not isinstance(item, dict):
            errors.append("manual item must be an object")
            continue
        key = item.get("key")
        if not isinstance(key, str) or not key:
            errors.append("manual key must be a nonempty string")
            continue
        require(key not in seen_manual, "duplicate manual key")
        seen_manual.add(key)
        require(item.get("kind") in ("object", "semantic_function"), "{}: invalid manual kind".format(key))
        require(isinstance(item.get("scope_id"), str) and item.get("scope_id") in scope_ids,
                "{}: missing applicability".format(key))
        require(item.get("disposition") == "reviewed", "{}: manual semantics must be reviewed".format(key))
        selected_gates = check_review(item.get("review"), str(key), ("decision", "write_policy"))
        obligation = MANUAL_REQUIREMENTS.get(key)
        require(obligation is not None, "{}: unknown manual obligation".format(key))
        if obligation is not None:
            owner, needed_gates, write_policy = obligation
            policy = item.get("review")
            require(item.get("owner_prompt") == owner, "{}: manual owner obligation changed".format(key))
            require(set(needed_gates.split()) <= selected_gates,
                    "{}: required manual feature or binding gate missing".format(key))
            require(isinstance(policy, dict) and policy.get("write_policy") == write_policy,
                    "{}: manual action obligation changed".format(key))
        require(isinstance(item.get("claim"), str) and bool(item["claim"].strip()) and
                type(item.get("owner_prompt")) is int and
                item["owner_prompt"] in set(range(12, 23)) | set(range(30, 40)) | {47},
                "{}: missing claim/owner".format(key))
        refs = item.get("source_refs")
        require(isinstance(refs, list) and bool(refs), "{}: missing manual source refs".format(key))
        if isinstance(refs, list):
            for ref in refs:
                check_ref(ref, str(key))
            scope_id = item.get("scope_id")
            allowed = manual_sources.get(scope_id, set()) if isinstance(scope_id, str) else set()
            pdf_ids = [ref.get("id") for ref in refs if isinstance(ref, dict) and ref.get("id") != "EDS"]
            require(bool(pdf_ids) and all(isinstance(sid, str) and sid in allowed for sid in pdf_ids),
                    "{}: manual PDF refs do not match scope".format(key))
        check_coverage(item.get("coverage"), str(key))
        require(item.get("confidence") in ("documented_manual", "contradicted"),
                "{}: manual confidence invalid".format(key))
        related = item.get("related_entries")
        require(isinstance(related, list) and all(isinstance(value, str) and
                value in eds_addresses for value in related),
                "{}: related EDS address missing".format(key))
        require(isinstance(item.get("conflict_ids"), list),
                "{}: conflict IDs must be an array".format(key))
        if item.get("kind") == "object":
            address = item.get("address")
            require(isinstance(address, str) and ENTRY_KEY.fullmatch(address),
                    "{}: manual object address invalid".format(key))
            if isinstance(address, str):
                require(address not in eds_addresses, "{}: object already in EDS".format(key))
            if isinstance(address, str) and isinstance(item.get("scope_id"), str):
                identity = (item.get("scope_id"), address)
                require(identity not in seen_manual_addresses,
                        "{}: duplicate manual object address in scope".format(key))
                seen_manual_addresses.add(identity)

    require(seen_manual == set(MANUAL_REQUIREMENTS), "manual obligation inventory incomplete")

    inventory = data.get("manual_index_inventory")
    require(isinstance(inventory, dict), "manual index inventory missing")
    if isinstance(inventory, dict):
        additional = inventory.get("additional_canopen_addresses")
        require(isinstance(additional, list) and additional ==
                [item.get("address") for item in manual if isinstance(item, dict) and
                 item.get("kind") == "object"],
                "manual-only CANopen addresses do not match object rows")
        require(bool(inventory.get("method")), "manual index comparison method missing")
        exclusions = inventory.get("excluded_nonobject_labels")
        require(isinstance(exclusions, list) and all(isinstance(row, dict) and
                row.get("label") and row.get("reason") for row in exclusions),
                "nonobject exclusions require labels and reasons")
        absent = inventory.get("not_declared_examples")
        require(isinstance(absent, list) and all(isinstance(key, str) and
                INDEX_KEY.fullmatch(key) and int(key, 16) not in listed for key in absent),
                "not-declared examples must be canonical absent indexes")

    anomalies = data.get("source_anomalies")
    require(isinstance(anomalies, list) and any(isinstance(row, dict) and
            row.get("id") == "A01" for row in anomalies),
            "1018 source anomaly must remain visible")
    if isinstance(anomalies, list):
        seen_anomalies = set()
        for anomaly in anomalies:
            if not isinstance(anomaly, dict):
                errors.append("source anomaly must be an object")
                continue
            anomaly_id = anomaly.get("id")
            require(isinstance(anomaly_id, str) and bool(anomaly_id) and anomaly_id not in seen_anomalies,
                    "duplicate or invalid source anomaly ID")
            if isinstance(anomaly_id, str):
                seen_anomalies.add(anomaly_id)
            require(anomaly.get("scope_id") == "eds_family_unbound",
                    "source anomaly applicability invalid")
            require(bool(anomaly.get("id")) and bool(anomaly.get("observed")) and
                    anomaly.get("disposition") == "reviewed" and
                    type(anomaly.get("owner_prompt")) is int,
                    "source anomaly lacks reviewed disposition")
            check_review(anomaly.get("review"), str(anomaly_id), ("decision",))
            refs = anomaly.get("source_refs")
            require(isinstance(refs, list) and bool(refs), "source anomaly lacks references")
            if isinstance(refs, list):
                for ref in refs:
                    check_ref(ref, str(anomaly.get("id")))

    conflicts = data.get("conflicts")
    if not isinstance(conflicts, list):
        errors.append("conflicts must be an array")
        conflicts = []
    conflict_ids = set()
    for conflict in conflicts:
        if not isinstance(conflict, dict):
            errors.append("conflict must be an object")
            continue
        cid = conflict.get("id")
        if not isinstance(cid, str) or not cid:
            errors.append("conflict ID must be a nonempty string")
            continue
        require(cid not in conflict_ids, "duplicate conflict ID")
        conflict_ids.add(cid)
        check_review(conflict.get("review"), str(cid), ("disposition", "consequence"))
        require(bool(conflict.get("subject")) and isinstance(conflict.get("scope_id"), str)
                and conflict.get("scope_id") in scope_ids,
                "{}: missing subject/applicability".format(cid))
        claims = conflict.get("claims")
        require(isinstance(claims, list) and len(claims) >= 2, "{}: at least two claims required".format(cid))
        if isinstance(claims, list):
            for claim in claims:
                if not isinstance(claim, dict):
                    errors.append("{}: claim must be an object".format(cid))
                    continue
                require(bool(claim.get("value")), "{}: claim value missing".format(cid))
                ref = claim.get("source_ref")
                check_ref(ref, str(cid))
                if isinstance(ref, dict) and ref.get("id") != "EDS":
                    scope_id = conflict.get("scope_id")
                    allowed = manual_sources.get(scope_id, set()) if isinstance(scope_id, str) else set()
                    require(isinstance(ref.get("id"), str) and ref.get("id") in allowed,
                            "{}: conflict PDF ref does not match scope".format(cid))
        resolution = conflict.get("resolution")
        require(resolution in ("unresolved", "resolved"), "{}: invalid resolution".format(cid))
        if resolution == "resolved":
            require(False, "{}: resolved without reviewed evidence; documentary policy is not a firmware resolution".format(cid))
            if isinstance(conflict.get("resolution_evidence"), dict):
                check_ref(conflict["resolution_evidence"], str(cid))
        else:
            require("decision" in conflict and "resolution_evidence" in conflict and
                    conflict.get("decision") is None and conflict.get("resolution_evidence") is None,
                    "{}: unresolved conflict has selected decision".format(cid))
    for obj in objects:
        if not isinstance(obj, dict) or not isinstance(obj.get("entries"), list):
            continue
        for entry in obj["entries"]:
            if not isinstance(entry, dict) or not isinstance(entry.get("conflict_ids"), list):
                continue
            for cid in entry["conflict_ids"]:
                require(isinstance(cid, str) and cid in conflict_ids,
                        "{}: dangling conflict ID {}".format(entry.get("key"), cid))
    for item in manual:
        if not isinstance(item, dict) or not isinstance(item.get("conflict_ids"), list):
            continue
        for cid in item["conflict_ids"]:
            require(isinstance(cid, str) and cid in conflict_ids,
                    "{}: dangling conflict ID {}".format(item.get("key"), cid))
    require(conflict_ids == {"C{:02d}".format(number) for number in range(1, 25)},
            "source conflict inventory incomplete")
    original_claims = [{field: row.get(field) for field in ("id", "subject", "scope_id", "claims")}
                       for row in conflicts if isinstance(row, dict)]
    claims_digest = hashlib.sha256(json.dumps(original_claims, sort_keys=True,
                                   separators=(",", ":"), ensure_ascii=False).encode("utf-8")).hexdigest()
    require(claims_digest == ORIGINAL_CONFLICT_CLAIMS_SHA256,
            "original conflict claims or source references changed")

    counts = data.get("accounting", {})
    expected_counts = {"object_indexes": len(expected_headers), "record_headers": sum(
        eds[s].get("ObjectType") == "9" for s in expected_headers),
        "array_headers": sum(eds[s].get("ObjectType") == "8" for s in expected_headers),
        "scalar_roots": sum(eds[s].get("ObjectType") == "7" for s in expected_headers),
        "child_entries": len(expected_entries) - sum("DataType" in eds[s] for s in expected_headers),
        "scalar_entries": len(expected_entries), "reserved_entries": reserved_count,
        "manual_only_items": len(manual), "index_gap_ranges": len(gap_ranges),
        "scalar_type_counts": dict(type_counts), "scalar_access_counts": dict(access_counts),
        "pdo_mapping_counts": dict(pdo_counts)}
    require(counts == expected_counts, "accounting summary differs from original EDS/ledger")
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ledger", type=Path, default=ROOT / "docs/reference/lichuan_objects.json")
    args = parser.parse_args()
    try:
        data = load_json(args.ledger)
        errors = validate_ledger(data)
    except (OSError, ValueError, configparser.Error) as exc:
        errors = [str(exc)]
    if errors:
        for error in errors:
            print("FAIL:", error, file=sys.stderr)
        return 1
    print("PASS: 99 EDS indexes, 202 typed entries, {} manual items; source and scope checked".format(
        len(data["manual_only"])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
