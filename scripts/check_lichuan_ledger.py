#!/usr/bin/env python3
"""Check the source intake against original EDS bytes and retained provenance."""

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
    require(data.get("schema_version") == 1, "schema_version must be 1")
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
            require(bool(ENTRY_KEY.fullmatch(elabel)) and elabel == ekey and entry.get("index") == index,
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
            require(entry.get("type") == {"eds_code": code, "name": name,
                                          "width_bytes": width, "signed": signed},
                    elabel + ": datatype/width/signedness mismatch")
            require(entry.get("access_eds") == raw.get("AccessType"), elabel + ": access spelling mismatch")
            require(entry.get("pdo_mapping_eds") == raw.get("PDOMapping"), elabel + ": PDO flag mismatch")
            require("access_reviewed" in entry and "pdo_mapping_reviewed" in entry and
                    entry.get("access_reviewed") is None and entry.get("pdo_mapping_reviewed") is None,
                    elabel + ": reviewed permission is premature")
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
                require(entry.get("disposition") == "pending_review" and
                        "exclusion_reason" in entry and entry.get("exclusion_reason") is None,
                        elabel + ": unreviewed entry disposition invalid")
            require(entry.get("confidence") == "documented_metadata", elabel + ": confidence must be metadata only")
            require(type(entry.get("owner_prompt")) is int and 2 <= entry["owner_prompt"] <= 50,
                    elabel + ": owner prompt invalid")
            require(all(field in entry for field in ("units", "scaling", "effect", "edge_behavior")) and
                    entry.get("units") is None and entry.get("scaling") is None and
                    entry.get("effect") is None and entry.get("edge_behavior") is None and
                    entry.get("prerequisites") == [], elabel + ": reviewed semantics are premature")
            require(isinstance(entry.get("conflict_ids"), list), elabel + ": conflict IDs must be an array")
            check_coverage(entry.get("coverage"), elabel)
            type_counts[name] += 1
            access_counts[raw.get("AccessType")] += 1
            pdo_counts[raw.get("PDOMapping")] += 1
    require({index for _, index in seen_headers} == {int(s, 16) for s in expected_headers}
            and len(seen_headers) == len(expected_headers), "EDS object headers not accounted exactly once")
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
        require(item.get("disposition") == "pending_review", "{}: manual semantics must remain pending".format(key))
        require(bool(item.get("claim")) and type(item.get("owner_prompt")) is int and
                2 <= item["owner_prompt"] <= 50,
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
                    anomaly.get("disposition") == "pending_review" and
                    type(anomaly.get("owner_prompt")) is int,
                    "source anomaly lacks pending disposition")
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
            require(False, "{}: resolved without reviewed evidence is premature in prompt 2".format(cid))
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
