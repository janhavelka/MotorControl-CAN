#!/usr/bin/env python3
"""Generate reviewed Lichuan declarations and coverage; --check never writes.

The single editable dictionary is docs/reference/lichuan_objects.json. Source
validation precedes every CLI render. No EDS flag authorizes a device operation.
Python and the ledger are developer dependencies, never consumer dependencies.
"""

import argparse
from collections import Counter
import configparser
import json
import os
from pathlib import Path
import re
import sys
import tempfile

from check_lichuan_ledger import (EXPRESSION, INTEGER, ROOT, TYPE_CODES,
                                  generation_disposition, load_json, pdo_observation_errors,
                                  read_coverage_errors, read_descriptor_representation,
                                  read_owner_errors, validate_ledger)


LEDGER_PATH = "docs/reference/lichuan_objects.json"
PUBLIC_PATH = "include/MotorControlCAN/profiles/lichuan_c"
OUTPUT_PATHS = (PUBLIC_PATH + "/Types.h", PUBLIC_PATH + "/Objects.h",
                PUBLIC_PATH + "/Catalogue.h", "docs/reference/lichuan_generated.md",
                PUBLIC_PATH + "/Pdo.h")
COMMAND = "python scripts/generate_lichuan_objects.py"
NAMESPACE = "namespace MotorControlCAN {\nnamespace LICHUAN_C {\n"
CLOSE_NAMESPACE = "} // namespace LICHUAN_C\n} // namespace MotorControlCAN\n"


def projected_rows(objects):
    """Validate declaration-relevant rows in a small projection of the ledger.

    This is also useful for synthetic unit fixtures. It cannot replace the full
    validate_ledger check performed by generate(), which verifies all originals,
    applicability, source references, semantic obligations and lossless counts.
    """
    rows, seen = [], set()
    if not isinstance(objects, list):
        raise ValueError("objects must be an array")
    for obj in objects:
        if not isinstance(obj, dict) or not isinstance(obj.get("entries"), list):
            raise ValueError("object entries must be an array")
        for entry in obj["entries"]:
            if not isinstance(entry, dict):
                raise ValueError("entry must be an object")
            key = entry.get("key")
            index, sub = entry.get("index"), entry.get("subindex")
            if (type(index) is not int or not 0 <= index <= 0xFFFF or
                    type(sub) is not int or not 0 <= sub <= 0xFF or
                    key != "0x{:04X}:{:02X}".format(index, sub)):
                raise ValueError("invalid entry address")
            scope = entry.get("scope_id")
            if scope != "eds_family_unbound":
                raise ValueError(key + ": missing or unsupported declaration applicability")
            identity = (scope, index, sub)
            if identity in seen:
                raise ValueError(key + ": duplicate entry address")
            seen.add(identity)
            native = entry.get("type")
            if not isinstance(native, dict) or native.get("eds_code") not in TYPE_CODES:
                raise ValueError(key + ": missing or unknown native type")
            code = native["eds_code"]
            name, width, signed = TYPE_CODES[code]
            if (type(native.get("width_bytes")) is not int or
                    type(native.get("signed")) is not bool or native != {
                        "eds_code": code, "name": name, "width_bytes": width, "signed": signed}):
                raise ValueError(key + ": datatype/width/signedness mismatch")
            numbers = {}
            for field in ("low_limit_eds", "high_limit_eds", "default_eds"):
                value = entry.get(field)
                if field not in entry or (value is not None and not isinstance(value, str)):
                    raise ValueError(key + ": missing or invalid numeric metadata")
                if value is None:
                    continue
                if field == "default_eds" and EXPRESSION.fullmatch(value):
                    continue
                if not INTEGER.fullmatch(value):
                    raise ValueError(key + ": invalid numeric metadata")
                number = int(value, 16) if value.startswith("0x") else int(value, 10)
                minimum = -(1 << (width * 8 - 1)) if signed else 0
                maximum = (1 << (width * 8 - int(signed))) - 1
                if not minimum <= number <= maximum:
                    raise ValueError(key + ": numeric metadata exceeds native type")
                numbers[field] = number
            if ("low_limit_eds" in numbers and "high_limit_eds" in numbers and
                    numbers["low_limit_eds"] > numbers["high_limit_eds"]):
                raise ValueError(key + ": reversed numeric limits")
            refs = entry.get("source_refs")
            if (not isinstance(refs, list) or not refs or any(not isinstance(ref, dict) or
                    not ref.get("id") or not (ref.get("section") or type(ref.get("page")) is int)
                    for ref in refs)):
                raise ValueError(key + ": source evidence missing")
            review = entry.get("review")
            if not isinstance(review, dict) or review.get("status") != "complete":
                raise ValueError(key + ": pending documentary review")
            if review.get("generation_type") != generation_disposition(entry):
                raise ValueError(key + ": missing or inconsistent generation type disposition")
            gates = review.get("gate_ids")
            if (not isinstance(gates, list) or not gates or
                    any(not isinstance(gate, str) or not re.fullmatch(r"Q(?:0[1-9]|1[0-9]|2[01])", gate)
                        for gate in gates) or len(gates) != len(set(gates))):
                raise ValueError(key + ": invalid qualification gates")
            if (review.get("read_effect") != "not_established" or
                    type(entry.get("owner_prompt")) is not int or
                    not 1 <= entry["owner_prompt"] <= 50):
                raise ValueError(key + ": unreviewed effect or owner")
            if entry.get("role") == "reserved":
                if (entry.get("disposition") != "excluded" or not entry.get("exclusion_reason") or
                        entry.get("access_reviewed") is not None or
                        entry.get("pdo_mapping_reviewed") is not False or
                        review.get("write_policy") != "reserved"):
                    raise ValueError(key + ": reserved entry grants access")
            else:
                access = entry.get("access_reviewed")
                allowed = {"RO": ("RO",), "WO": ("WO",), "RW": ("RO", "RW")}
                if access not in allowed.get(entry.get("access_eds"), ()):
                    raise ValueError(key + ": widened or missing SDO access")
                expected = "not_writable" if access == "RO" else "qualification_required"
                if review.get("write_policy") != expected:
                    raise ValueError(key + ": unqualified write policy")
                if entry.get("pdo_mapping_reviewed") is not False and not (
                        entry.get("pdo_mapping_reviewed") is None and entry.get("pdo_mapping_eds") == "1"):
                    raise ValueError(key + ": unqualified PDO policy")
            read_errors = read_owner_errors(entry) + read_coverage_errors(entry)
            if read_errors:
                raise ValueError("; ".join(read_errors))
            rows.append(entry)
    return sorted(rows, key=lambda row: (row["scope_id"], row["index"], row["subindex"]))


def notice(filename, purpose):
    return ("// SPDX-License-Identifier: MIT\n/**\n * @file " + filename + "\n * @brief " + purpose + "\n"
            " * Generated from " + LEDGER_PATH + ". Do not edit.\n"
            " * Regenerate with: " + COMMAND + "\n */\n#pragma once\n\n")


def types_header():
    return notice("Types.h", "Resolved documentary types and conservative Lichuan policies.") + r'''#include <stdint.h>

namespace MotorControlCAN {
/** @brief Lichuan C documentary scope; no live binding or device support is implied. */
namespace LICHUAN_C {

/** @brief Native scalar storage type; width and sign remain distinct from physical units. */
enum class ScalarType : uint8_t {
    U8,  ///< Unsigned eight-bit native integer.
    U16, ///< Unsigned sixteen-bit native integer.
    U32, ///< Unsigned thirty-two-bit native integer.
    I16, ///< Signed sixteen-bit native integer.
    I32  ///< Signed thirty-two-bit native integer.
};

/** @brief Conservative reviewed SDO access; these labels never authorize a transaction. */
enum class SdoAccess : uint8_t {
    READ_ONLY,  ///< Source permits upload only; read effects still need qualification.
    WRITE_ONLY, ///< Source permits download only; no implicit upload or action permission.
    READ_WRITE  ///< Source permits both services; independent effect gates remain open.
};

/** @brief Read-side effects are unresolved independently of the EDS access flag. */
enum class ReadEffect : uint8_t {
    UNQUALIFIED ///< Non-consuming or non-changing reads have not been established.
};

/** @brief Documentary write restriction; no member means an enabled raw write route. */
enum class WritePolicy : uint8_t {
    FORBIDDEN,             ///< Reviewed access prohibits writes.
    QUALIFICATION_REQUIRED ///< Future typed operation must satisfy its reviewed gates.
};

/** @brief PDO eligibility independent of SDO direction and action permission. */
enum class PdoPolicy : uint8_t {
    FORBIDDEN,  ///< Reviewed mapping policy excludes this entry.
    UNQUALIFIED ///< Source mapping claim exists; direction, map and effects are unqualified.
};

/** @brief Documentary applicability; no model or firmware identity is inferred. */
enum class SourceScope : uint8_t {
    EDS_FAMILY_UNBOUND ///< LC_StepDriverCAN_V1.1.eds family; model and firmware unverified.
};

/** @brief Native read representation; raw bytes do not settle disputed signedness. */
enum class ReadRepresentation : uint8_t {
    UNSIGNED, ///< Source-confirmed unsigned native integer, without physical units.
    SIGNED,   ///< Source-confirmed signed native integer, without physical units.
    RAW       ///< Exact bytes only; signed interpretation remains unresolved.
};

/** @brief Bounded read shape, independent of live binding, interpretation and effects.
 * Returned by readDescriptor for the reviewed Prompt 19/20 assignments only.
 * Does not establish a non-changing probe or grant polling qualification.
 */
struct ReadDescriptor {
    uint16_t index;                      ///< Native object index, 0 through 65535.
    uint8_t subindex;                    ///< Native subindex, 0 through 255.
    uint8_t widthBytes;                  ///< Exact scalar payload width, in bytes.
    ReadRepresentation representation; ///< Raw or source-confirmed native integer representation.
    ReadEffect readEffect;               ///< Read effects remain independent of access and width.
    SourceScope scope;                   ///< Documentary family scope, not a live firmware binding.
};

/**
 * @brief Fixed native declaration in the Lichuan profile, never a service permission.
 * No I/O, strings, default values or runtime catalogue are required. A consumer
 * selects a named constant; object numbers alone cannot dispatch vendor actions.
 */
struct ObjectDescriptor {
    uint16_t index;           ///< Native CANopen object index, 0 through 65535.
    uint8_t subindex;         ///< Native subindex, 0 through 255; zero may be a record count.
    ScalarType type;         ///< Resolved native scalar type, not a dimensional unit.
    uint8_t widthBytes;      ///< Exact native payload width in bytes; not record SubNumber.
    bool signedValue;        ///< True for the resolved signed native integer types.
    SdoAccess sdoAccess;      ///< Reviewed service direction; cannot bypass effect policy.
    ReadEffect readEffect;   ///< Upload effects remain unqualified even for read-only entries.
    WritePolicy writePolicy; ///< Forbidden or qualification-required; never write-enabled.
    PdoPolicy pdoPolicy;      ///< Separate mapping disposition; not an RPDO/TPDO template.
    uint8_t ownerPrompt;     ///< Numbered prompt owning the semantic implementation.
    uint32_t gateMask;       ///< Open evidence gates: Q01 is bit zero through Q21 at bit twenty.
    SourceScope scope;       ///< Unbound source scope; no live model/firmware applicability.
};

} // namespace LICHUAN_C
} // namespace MotorControlCAN
'''


def symbol(row, prefix="OBJECT"):
    return "{}_{:04X}_{:02X}".format(prefix, row["index"], row["subindex"])


def source_text(refs):
    result = []
    for ref in refs:
        if ref["id"] == "EDS":
            result.append("EDS [{}] lines {}-{}".format(
                ref["section"], ref["line_start"], ref["line_end"]))
        else:
            result.append("{} PDF page {}".format(ref["id"], ref["page"]))
    return "; ".join(result)


def objects_header(rows):
    lines = [notice("Objects.h", "Compact resolved native declarations; no action routes or catalogue."),
             '#include "MotorControlCAN/profiles/lichuan_c/Types.h"\n\n', NAMESPACE, "\n"]
    access = {"RO": "READ_ONLY", "WO": "WRITE_ONLY", "RW": "READ_WRITE"}
    for row in rows:
        if row["review"]["generation_type"] != "resolved":
            continue
        gates = row["review"]["gate_ids"]
        mask = sum(1 << (int(gate[1:]) - 1) for gate in gates)
        write = "FORBIDDEN" if row["review"]["write_policy"] == "not_writable" else "QUALIFICATION_REQUIRED"
        pdo = "FORBIDDEN" if row["pdo_mapping_reviewed"] is False else "UNQUALIFIED"
        native = row["type"]
        lines += ["/** @brief {} {} {} declaration; {}.\n".format(
                    row["key"], native["name"], row["role"], source_text(row["source_refs"])),
                  " * No operation is implemented or authorized; gates {} remain open. */\n".format(
                    ", ".join(gates)),
                  "constexpr ObjectDescriptor {} = {{\n".format(symbol(row)),
                  "    0x{:04X}, 0x{:02X}, ScalarType::{}, {}, {}, SdoAccess::{},\n".format(
                    row["index"], row["subindex"], native["name"], native["width_bytes"],
                    "true" if native["signed"] else "false", access[row["access_reviewed"]]),
                  "    ReadEffect::UNQUALIFIED, WritePolicy::{}, PdoPolicy::{},\n".format(write, pdo),
                  "    {}, 0x{:08X}u, SourceScope::EDS_FAMILY_UNBOUND\n}};\n\n".format(
                    row["owner_prompt"], mask)]
    lines.append(read_descriptor_function(rows))
    return "".join(lines) + CLOSE_NAMESPACE


def read_descriptor_function(rows):
    """Generate one bounded lookup from explicit read ownership, never EDS flags alone."""
    selected = [row for row in rows if row.get("read_owner_prompt") in (19, 20)]
    if len(selected) > 39:
        raise ValueError("Implemented read descriptor capacity exceeds 39")
    lines = [r'''/** @brief Copy the reviewed shape of one implemented native read.
 * Contains only explicitly assigned readable source rows. Raw mode bytes retain
 * disputed signedness; a match does not qualify read effects, polling, physical
 * units, interpretation, firmware identity or a non-changing probe.
 * Performs no I/O, time acquisition or allocation. Independent calls are reentrant;
 * callers serialize writes to a shared output. No input storage is retained.
 * @param[in] index Native object index, 0 through 65535.
 * @param[in] subindex Native subindex, 0 through 255.
 * @param[out] output Copied descriptor on success; unchanged when no row matches.
 * @return True only for a reviewed read shape; false for absent, reserved,
 * write-only or separately owned entries. Does not authorize a device effect.
 */
inline bool readDescriptor(uint16_t index, uint8_t subindex, ReadDescriptor& output) noexcept {
''']
    if selected:
        lines.append("    static const ReadDescriptor rows[] = {\n")
        for row in selected:
            lines.append("        {{0x{:04X}, 0x{:02X}, {}, ReadRepresentation::{}, "
                         "ReadEffect::UNQUALIFIED, SourceScope::EDS_FAMILY_UNBOUND}},\n".format(
                             row["index"], row["subindex"], row["type"]["width_bytes"],
                             read_descriptor_representation(row)))
        lines += ["    };\n", "    for (uint8_t i = 0; i < {}; ++i) {{\n".format(len(selected)),
                  "        if (rows[i].index == index && rows[i].subindex == subindex) {\n",
                  "            output = rows[i];\n            return true;\n        }\n    }\n"]
    else:
        lines.append("    (void)index;\n    (void)subindex;\n    (void)output;\n")
    lines.append("    return false;\n}\n\n")
    return "".join(lines)


def cpp_string(value):
    """Encode UTF-8 bytes as a C++11 literal, independent of compiler encoding.

    Fixed three-digit octal escapes cannot absorb following digits. Escaping
    question marks also prevents C++11 trigraph translation. UTF-8 encoding
    rejects isolated surrogates before any generated file is published.
    """
    if value is None:
        return "nullptr"
    escapes = {34: '\\"', 63: '\\?', 92: '\\\\'}
    return '"' + "".join(escapes.get(byte, chr(byte)) if 32 <= byte <= 126
                         else "\\{:03o}".format(byte) for byte in value.encode("utf-8")) + '"'


def pdo_header(objects, rows):
    """Generate only reviewed passive facts and original, unverified templates."""
    errors = pdo_observation_errors(objects)
    if errors:
        raise ValueError("; ".join(errors))
    lines = [notice("Pdo.h", "Reviewed passive layout facts and unverified EDS PDO templates."),
             '#include <MotorControlCAN/canopen/Pdo.h>\n\n',
             '/** Portable library value contracts; no device I/O. */\nnamespace MotorControlCAN {\n',
             '/** Lichuan documentary PDO layouts; model/firmware and live evidence remain unqualified. */\n',
             'namespace LICHUAN_C {\n\n', r'''/** Return reviewed raw-layout facts without authorizing any service or action.
 * The returned table has static lifetime and contains ten default-mapped objects.
 * Unresolved mode/velocity signedness remains raw-only. No time, I/O or allocation.
 * @param[in] profileId Caller-selected profile identity, nonzero for subsequent validation.
 * Zero remains invalid; this constructor does not establish a model binding.
 * @return Value containing four sets per direction and four slots per set, plus
 * borrowed immutable static facts. Validation and live qualification are separate.
 */
inline CANOPEN::PdoProfile pdoProfile(uint32_t profileId) noexcept {
    static const CANOPEN::PdoObject objects[] = {
''']
    for row in rows:
        fact = row["review"].get("pdo_observation")
        if fact is None:
            continue
        lines.append("        CANOPEN::PdoObject(0x{:04X}, 0x{:02X}, {}, CANOPEN::PdoSign::{}, "
                     "CANOPEN::PdoRole::{}, {}, {}),\n".format(
                         row["index"], row["subindex"], row["type"]["width_bytes"],
                         fact["representation"], fact["role"],
                         "true" if "RPDO" in fact["directions"] else "false",
                         "true" if "TPDO" in fact["directions"] else "false"))
    lines += [r'''    };
    CANOPEN::PdoProfile profile;
    profile.profile = profileId;
    profile.sets = 4;
    profile.slots = 4;
    profile.objects = objects;
    profile.objectCount = sizeof(objects) / sizeof(objects[0]);
    return profile;
}

/** Copy an original EDS layout template; never confirms live map contents.
 * Performs no device writes, reads, time acquisition or allocation. Independent
 * calls are reentrant; the caller serializes mutation of shared output.
 * @param[in] direction RPDO or TPDO, relative to the drive.
 * @param[in] number One-based set number, 1..4. Sets 3/4 have disabled count zero.
 * @param[out] output Owned complete candidate, unchanged on rejection. No input
 * pointer is retained. Inactive mapping words are always zero.
 * @return OK proves source-template construction only. INVALID_CONFIG rejects
 * invalid direction/number, detail zero; no live qualification follows.
 */
inline Status pdoTemplate(CANOPEN::PdoDirection direction, uint16_t number,
                          CANOPEN::PdoMap& output) noexcept {
    if ((direction != CANOPEN::PdoDirection::RPDO && direction != CANOPEN::PdoDirection::TPDO) ||
        number < 1 || number > 4) {
        return Status(Err::INVALID_CONFIG, 0, nullptr);
    }
    CANOPEN::PdoMap candidate;
    candidate.direction = direction;
    candidate.number = number;
''']
    by_address = {(row["index"], row["subindex"]): row for row in rows}
    for base, direction in ((0x1600, "RPDO"), (0x1A00, "TPDO")):
        lines.append("    if (direction == CANOPEN::PdoDirection::{}) {{\n".format(direction))
        lines.append("        switch (number) {\n")
        for number in range(4):
            count = int(by_address[(base + number, 0)]["default_eds"], 0)
            lines.append("        case {}:\n            candidate.count = {};\n".format(number + 1, count))
            for slot in range(1, count + 1):
                word = int(by_address[(base + number, slot)]["default_eds"], 0)
                lines.append("            candidate.words[{}] = 0x{:08X}u;\n".format(slot - 1, word))
            lines.append("            break;\n")
        lines.append("        }\n    }\n")
    return "".join(lines) + "    output = candidate;\n    return Ok();\n}\n\n" + CLOSE_NAMESPACE


def catalogue_header(rows, scopes):
    lines = [notice("Catalogue.h", "Optional static-lifetime descriptive metadata; no device support."),
             '#include "MotorControlCAN/profiles/lichuan_c/Objects.h"\n\n', NAMESPACE, r'''
/**
 * @brief Optional source descriptions; include only when these strings are needed.
 * All pointers refer to immutable static-lifetime literals, never live values.
 * A null limit/default means absent source metadata and is never a writable zero.
 * Descriptions carry vendor claims, not independently established device behavior.
 */
struct ObjectMetadata {
    ObjectDescriptor object;  ///< Same conservative declaration as the named compact constant.
    const char* name;         ///< Original EDS parameter name; no semantic API alias is implied.
    const char* sourceRefs;   ///< Source IDs, physical PDF pages or EDS sections and line spans.
    const char* units;        ///< Documentary native units; unknowns remain explicit.
    const char* scaling;      ///< Reviewed scale statement, including unresolved conversion facts.
    const char* effect;       ///< Documentary effect description, not an operation implementation.
    const char* prerequisites; ///< Documentary prerequisites; all gates remain independent.
    const char* edgeBehavior; ///< Source edge behavior, or an explicit unknown statement.
    const char* sourceAccess; ///< Original EDS access spelling, independent of PDO eligibility.
    const char* lowLimit;     ///< Original low-limit spelling, or null when absent.
    const char* highLimit;    ///< Original high-limit spelling, or null when absent.
    const char* sourceDefault; ///< Original default or unevaluated $NODEID expression, or null.
    const char* conflictIds;  ///< Retained unresolved conflict IDs; empty means no listed conflict.
    const char* applicability; ///< Source family/dialect/model/firmware scope; not a live binding.
};

''']
    scope_by_id = {scope["id"]: scope for scope in scopes}
    for row in rows:
        if row["review"]["generation_type"] != "resolved":
            continue
        values = [row["raw_fields"]["ParameterName"], source_text(row["source_refs"]),
                  row["units"], row["scaling"], row["effect"], "; ".join(row["prerequisites"]),
                  row["edge_behavior"], row["access_eds"], row["low_limit_eds"],
                  row["high_limit_eds"], row["default_eds"], ", ".join(row["conflict_ids"]),
                  json.dumps(scope_by_id[row["scope_id"]], ensure_ascii=True, sort_keys=True)]
        lines += ["/** @brief Optional descriptive metadata for {}. */\n".format(row["key"]),
                  "constexpr ObjectMetadata {} = {{\n    {},\n".format(symbol(row, "METADATA"), symbol(row)),
                  ",\n".join("    " + cpp_string(value) for value in values), "\n};\n\n"]
    return "".join(lines) + CLOSE_NAMESPACE


def text(value):
    """Escape source data for literal Markdown table cells without selecting facts."""
    if value is None:
        return "`null` (not stated)"
    if not isinstance(value, str):
        value = json.dumps(value, ensure_ascii=False, sort_keys=True)
    return (value.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
            .replace("|", "\\|").replace("\n", " ").replace("\r", " "))


def coverage_text(coverage):
    return "/".join("yes" if coverage[field] else "no" for field in
                    ("documented", "implemented", "host_tested", "hardware_qualified"))


def coverage_document(data, rows):
    counts = Counter(row["review"]["generation_type"] for row in rows)
    read_rows = [row for row in rows if row.get("read_owner_prompt") in (19, 20)]
    implemented_reads = sum(row.get("read_coverage", {}).get("implemented", False) for row in rows)
    lines = ["# Generated Lichuan native declarations and semantic coverage\n\n",
             "Generated from `" + LEDGER_PATH + "` by `" + COMMAND + "`. Do not edit.\n\n",
             "This is documentary metadata in `MotorControlCAN::LICHUAN_C`. The EDS family has no "
             "installed model/firmware binding. Generated descriptors implement no typed reads, typed "
             "settings, actions or workflows. No entry is hardware-qualified. Vendor source material "
             "retains its separate licensing and is excluded from the core distribution.\n\n",
             "`Objects.h` requires only `Types.h`. `Catalogue.h` is optional descriptive text; "
             "minimal consumers neither include it nor link a dictionary. SDO direction, read effect, "
             "write qualification and PDO eligibility remain separate. Lichuan 0x2300 save/reset "
             "must not be interpreted as Nanotec N5 0x2300.\n\n",
             "## Accounting and implementation states\n\n",
             "| Measure | Count | Meaning |\n| --- | ---: | --- |\n",
             "| EDS indexes | {} | Scalars, arrays and records, not implemented functions |\n".format(
                 len(data["objects"])),
             "| Explicitly typed EDS entries | {} | Separate from index count |\n".format(len(rows)),
             "| Resolved compact descriptors | {} | Native type and restrictive policy only |\n".format(counts["resolved"]),
             "| Withheld declarations | {} | Unresolved complete signed type, raw claims retained |\n".format(counts["withheld"]),
             "| Excluded reserved entries | {} | No descriptor or operation route |\n".format(counts["excluded"]),
             "| Manual-only semantic items | {} | Reviewed evidence, semantic functions pending |\n".format(len(data["manual_only"])),
             "| Read shape descriptors | {} | Prompt 19/20 source width/representation only; read effects remain unqualified |\n".format(len(read_rows)),
             "| Typed reads | {} | Implemented scalar read rows; see separate read coverage, not whole-object support |\n".format(implemented_reads),
             "| Typed settings | 0 | Assigned semantic prompts remain pending |\n",
             "| Actions | 0 | Assigned semantic prompts remain pending |\n",
             "| Workflows | 0 | Assigned semantic prompts remain pending |\n\n",
             "Coverage `D/I/H/Q` means documented / semantic implementation / host-tested semantic "
             "function / hardware-qualified. Generator and declaration tests do not change a "
             "semantic coverage field. `resolved` describes native declaration scope only; other "
             "conflicts and every listed qualification gate remain visible. `withheld` and "
             "`excluded` entries have no ObjectDescriptor or ObjectMetadata constant.\n\n",
             "## Original source identity and applicability\n\n",
             "| Source | Title and version | Acquired file / SHA-256 | URL | Applicability evidence |\n",
             "| --- | --- | --- | --- | --- |\n"]
    for source in data["sources"]:
        lines.append("| {} | {} | {} | {} | {} |\n".format(
            source["id"], text(source["manifest_title"] + "; " + source["revision"]),
            text(source["acquired_path"] + "; " + source["sha256"]), text(source["url"]),
            text({key: source[key] for key in ("models_named", "applicability_evidence")})))
    lines += ["\n## Source scopes\n\n", "| Scope | Manufacturer, model, dialect and firmware facts |\n| --- | --- |\n"]
    for scope in data["scopes"]:
        lines.append("| {} | {} |\n".format(scope["id"], text(scope)))
    lines += ["\n## Reviewed model binding and capabilities\n\n",
              text(data["semantic_review"]["binding_policy"]) + "\n\n",
              "| Model | Reviewed family facts and limitations |\n| --- | --- |\n"]
    for family in data["semantic_review"]["families"]:
        lines.append("| {} | {} |\n".format(text(family["model"]), text(family)))
    lines += ["\n## Qualification gates\n\n", "Gate bits in descriptors use Q01 at bit zero through Q21 at bit twenty. All are open.\n\n",
              "| Gate | Feature | Evidence required | Models | Owners |\n| --- | --- | --- | --- | --- |\n"]
    for gate in data["semantic_review"]["gates"]:
        lines.append("| {} | {} | {} | {} | {} |\n".format(gate["id"], text(gate["feature"]),
                     text(gate["required_evidence"]), text(gate["models"]), text(gate["owner_prompts"])))
    lines += ["\n## EDS structure: all indexes\n\n",
              "Counts and reserved subentries retain their own explicit native types. A record's "
              "SubNumber is structural metadata, not a synthesized child. In particular 0x1018 "
              "has only 00..03; no 04 is fabricated.\n\n",
              "| Index | Kind | EDS section | Original header fields |\n| --- | --- | --- | --- |\n"]
    for obj in sorted(data["objects"], key=lambda row: row["index"]):
        lines.append("| {} | {} | {} | {} |\n".format(obj["key"], obj["kind"],
                     text(source_text([obj["source_ref"]])), text(obj["raw_fields"])))
    rpdo1_words = [row["default_eds"] for row in rows if row["index"] == 0x1600 and
                   row["subindex"] in (1, 2, 3)]
    lines += ["\n## Scalar declaration coverage\n\n",
              "The independent native type/access/units columns describe source claims; only resolved "
              "rows generate declarations. Source PDOMapping=1 is never a write permission. The "
              "default RPDO1 source words " + ", ".join(text(value) for value in rpdo1_words) +
              " do not grant SDO reads of WO "
              "6040/6060 or any ungated action. `Pdo.h` generates ten explicitly reviewed passive "
              "layout facts and all eight source templates. They support raw-layout validation, "
              "not write permission, live configuration, sample provenance, physical units or motion "
              "completion. Mode bytes and target velocity retain raw representation while signedness "
              "conflicts remain unresolved; every live qualification gate stays open.\n\n",
              "Read ownership is separate from whole-object ownership. Only Prompt 19 read rows "
              "generate ReadDescriptor lookup entries; 6061 is a raw byte without an ObjectDescriptor. "
              "Read coverage does not qualify non-changing effects or close the pending Q20 probe.\n\n",
              "| Entry | Role | Native type / bytes / signed | EDS / reviewed SDO access | EDS / reviewed PDO | Declaration | Owner | Read owner | D/I/H/Q | Read D/I/H/Q |\n",
              "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |\n"]
    for row in rows:
        lines.append("| {} | {} | {} / {} / {} | {} / {} | {} / {} | {} | {} | {} | {} | {} |\n".format(
            row["key"], row["role"], row["type"]["name"], row["type"]["width_bytes"],
            str(row["type"]["signed"]).lower(), row["access_eds"], text(row["access_reviewed"]),
            row["pdo_mapping_eds"], text(row["pdo_mapping_reviewed"]),
            row["review"]["generation_type"], row["owner_prompt"], text(row.get("read_owner_prompt")),
            coverage_text(row["coverage"]),
            coverage_text(row["read_coverage"]) if "read_coverage" in row else "not reported"))
    lines += ["\n## Entry evidence and pending semantics\n\n"]
    for row in rows:
        lines += ["### {}\n\n".format(row["key"]),
                  "| Fact | Reviewed/source value |\n| --- | --- |\n"]
        fields = [("EDS name", row["raw_fields"]["ParameterName"]),
                  ("Sources", source_text(row["source_refs"])), ("Applicability", row["scope_id"]),
                  ("Declaration disposition", row["review"]["generation_type"]),
                  ("Documentary disposition", row["disposition"]),
                  ("Exclusion reason", row["exclusion_reason"]),
                  ("Units", row["units"]), ("Scaling", row["scaling"]),
                  ("Original low / high / default", [row["low_limit_eds"], row["high_limit_eds"], row["default_eds"]]),
                  ("Effect", row["effect"]), ("Prerequisites", row["prerequisites"]),
                  ("Edge behavior", row["edge_behavior"]), ("Documentary review", row["review"]),
                  ("Conflicts", row["conflict_ids"]), ("Owner prompt", row["owner_prompt"]),
                  ("Read owner prompt", row.get("read_owner_prompt")),
                  ("Read representation", read_descriptor_representation(row) if row.get("read_owner_prompt") in (19, 20) else None),
                  ("Read coverage D/I/H/Q", coverage_text(row["read_coverage"]) if "read_coverage" in row else "not reported"),
                  ("Semantic coverage D/I/H/Q", coverage_text(row["coverage"])),
                  ("Raw source claim", row["raw_fields"])]
        lines += ["| {} | {} |\n".format(label, text(value)) for label, value in fields]
        lines.append("\n")
    lines += ["## Manual-only inventory\n\n",
              "All source-reviewed manual items remain accounted for without declarations or "
              "implemented semantic functions. No second dictionary is synthesized.\n\n"]
    for item in data["manual_only"]:
        lines += ["### {}\n\n".format(item["key"]), "| Fact | Value |\n| --- | --- |\n"]
        lines += ["| {} | {} |\n".format(field, text(value)) for field, value in item.items()]
        lines.append("\n")
    lines += ["## Conflicting source claims\n\n",
              "Documentary policy does not select a firmware winner. Claims stay side by side.\n\n"]
    for conflict in data["conflicts"]:
        lines += ["### {}: {}\n\n".format(conflict["id"], text(conflict["subject"])),
                  "| Source | Claim |\n| --- | --- |\n"]
        for claim in conflict["claims"]:
            lines.append("| {} | {} |\n".format(text(claim["source_ref"]), text(claim["value"])))
        lines.append("\nDisposition: {}. Review: {}.\n\n".format(text(conflict["resolution"]), text(conflict["review"])))
    lines += ["## Gaps, anomalies and nonobject exclusions\n\n",
              "Address gaps are absent source entries, not reserved objects or implied support.\n\n",
              "| First absent index | Last absent index |\n| --- | --- |\n"]
    lines += ["| {} | {} |\n".format(row["first"], row["last"]) for row in data["index_gaps"]]
    for label, value in (("Source anomalies", data["source_anomalies"]),
                         ("Manual index inventory", data["manual_index_inventory"]),
                         ("Original accounting", data["accounting"])):
        lines.append("\n### {}\n\n{}\n".format(label, text(value)))
    return "".join(lines)


def render(data):
    """Render candidates in memory after projection checks; no filesystem mutation."""
    rows = projected_rows(data["objects"])
    products = (types_header(), objects_header(rows), catalogue_header(rows, data["scopes"]),
                coverage_document(data, rows), pdo_header(data["objects"], rows))
    return {path: product.encode("utf-8") for path, product in zip(OUTPUT_PATHS, products)}


def publish(outputs, root):
    """Stage every candidate first; roll back file contents if replacement fails."""
    root = Path(root)
    root.mkdir(parents=True, exist_ok=True)
    previous = {path: (root / path).read_bytes() if (root / path).exists() else None for path in outputs}
    changed = [path for path in outputs if previous[path] != outputs[path]]
    if not changed:
        return
    with tempfile.TemporaryDirectory(prefix=".lichuan-candidate-", dir=root) as temporary:
        candidate_root = Path(temporary)
        for number, path in enumerate(changed):
            (candidate_root / str(number)).write_bytes(outputs[path])
        committed = []
        try:
            for number, path in enumerate(changed):
                target = root / path
                target.parent.mkdir(parents=True, exist_ok=True)
                os.replace(candidate_root / str(number), target)
                committed.append(path)
        except OSError:
            for number, path in reversed(list(enumerate(committed))):
                target = root / path
                if previous[path] is None:
                    target.unlink()
                else:
                    restore = candidate_root / ("restore-" + str(number))
                    restore.write_bytes(previous[path])
                    os.replace(restore, target)
            raise


def generate(ledger, root=ROOT, source_root=ROOT, check=False):
    """Fully validate originals and ledger, then check or publish all candidates."""
    data = load_json(ledger)
    errors = validate_ledger(data, Path(source_root))
    if errors:
        raise ValueError("ledger validation failed:\n" + "\n".join(errors))
    outputs = render(data)
    if check:
        stale = [path for path, content in outputs.items() if not (Path(root) / path).is_file() or
                 (Path(root) / path).read_bytes() != content]
        if stale:
            raise ValueError("missing or stale generated output: " + ", ".join(stale) +
                             "; run " + COMMAND)
    else:
        publish(outputs, root)
    return outputs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="compare exact bytes without mutation")
    parser.add_argument("--ledger", type=Path, default=ROOT / LEDGER_PATH)
    parser.add_argument("--root", type=Path, default=ROOT, help="generated output checkout root")
    parser.add_argument("--source-root", type=Path, default=ROOT, help="original source checkout root")
    args = parser.parse_args()
    try:
        generate(args.ledger, args.root, args.source_root, args.check)
    except (OSError, ValueError, TypeError, KeyError, configparser.Error) as error:
        print("Lichuan object generation failed: " + str(error), file=sys.stderr)
        return 1
    print("PASS: reviewed Lichuan generated outputs " + ("are current" if args.check else "are synchronized"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
