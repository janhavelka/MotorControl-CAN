#!/usr/bin/env python3
"""Record rules of the shared console's JSONL output (docs/cli_contract.md), for host tools.

test/console_protocol_test.py parses the native console's real transcripts with parse_record;
scripts/s2_console.py applies record_problems to every line the S2 image prints. Both read the
same rules: one object per line of at most 511 bytes including its newline, no CR or NUL,
no repeated key, no NaN or infinity, the schema identity, protocol 3, profile lichuan_c, a
uint32 integer id and a string command and type. Only an unsolicited debug trace carries id 0.
"""

import json
import math
import re

SCHEMA = "motorcontrolcan.console/1"
PROTOCOL = 3
PROFILE = "lichuan_c"
RECORD_LIMIT = 511  # Bytes including the newline: ProbeConsole::RECORD_CAPACITY minus its NUL.
ID_MAX = 4294967295


def unique_object(pairs):
    """Repeated keys make a record ambiguous even when json.loads accepts it."""
    result = {}
    for key, value in pairs:
        if key in result:
            raise AssertionError("duplicate JSON key: " + key)
        result[key] = value
    return result


def reject_constant(value):
    raise AssertionError("non-finite JSON constant: " + value)


def parse_record(line):
    """One record; AssertionError for a repeated key or a non-finite constant, ValueError otherwise."""
    def finite_float(text):
        value = float(text)
        if not math.isfinite(value):
            reject_constant(text)
        return value
    return json.loads(line, object_pairs_hook=unique_object, parse_constant=reject_constant,
                      parse_float=finite_float)


def is_uint32(value, low=0):
    return type(value) is int and low <= value <= ID_MAX


def motion_change_problem(record):
    """Validate additive change evidence; no report alone qualifies physical motion."""
    kind = record.get("type")
    if kind not in ("move_relation", "velocity_change"):
        return None
    if not is_uint32(record.get("operation_id"), 1):
        return "invalid motion change operation identity"
    if kind == "move_relation":
        if (record.get("command") != "move" or
                not is_uint32(record.get("original"), 1) or
                not is_uint32(record.get("replacement"), 1) or
                not record["original"] < record["replacement"] or
                record["operation_id"] not in (record["original"], record["replacement"]) or
                record.get("observation_superseded") is not True or
                record.get("physical_interruption_observed") is not False):
            return "invalid retained move relation"
        return None
    error = record.get("error")
    def decimal(value):
        return type(value) is str and re.fullmatch(r"0|[1-9][0-9]*", value) is not None
    if (record.get("command") != "velocity" or type(record.get("change_index")) is not int or
            record["change_index"] != 1 or not decimal(record.get("native_rpm")) or
            not 1 <= int(record["native_rpm"]) <= 3000 or
            not is_uint32(record.get("record"), 1) or
            type(record.get("gate_code")) is not int or record["gate_code"] not in (1, 2) or
            type(record.get("execution_code")) is not int or not 0 <= record["execution_code"] <= 3 or
            any(type(record.get(key)) is not bool for key in ("uncertain", "effect_possible", "report_present")) or
            not is_uint32(record.get("observed_cycle")) or
            type(record.get("first_report_rpm")) is not int or not -(2**31) <= record["first_report_rpm"] < 2**31 or
            "abort_code" not in record or
            record["abort_code"] is not None and not is_uint32(record["abort_code"]) or
            record.get("qualified") is not False or type(record.get("exact")) is not bool):
        return "invalid velocity change evidence"
    if record["exact"]:
        if ("error_bound" in record or type(error) is not list or len(error) != 3 or
                any(not decimal(part) or int(part) > 2**64 - 1 for part in error[:2]) or
                int(error[1]) == 0 or type(error[2]) is not bool):
            return "invalid exact velocity change quantization"
    else:
        bound = record.get("error_bound")
        if "error" in record or type(bound) is not str or len(bound) > 32:
            return "invalid approximate velocity change error bound"
        try:
            if not math.isfinite(float(bound)) or float(bound) < 0:
                return "invalid approximate velocity change error bound"
        except ValueError:
            return "invalid approximate velocity change error bound"
    if ((record["gate_code"] == 2 and int(record["native_rpm"]) > 60) or
            (record["observed_cycle"] and not record["report_present"]) or
            not record["report_present"] and record["first_report_rpm"] != 0 or
            record["execution_code"] == 3 and not record["uncertain"]):
        return "contradictory velocity change evidence"
    return None


def record_problems(line):
    """(record or None, problems) for one received line, bytes including its newline.

    record is the parsed object when it is one, even if other rules fail; problems is empty only
    for a record that satisfies every rule above."""
    problems = []
    if len(line) > RECORD_LIMIT:
        problems.append("oversize: %d bytes exceed %d" % (len(line), RECORD_LIMIT))
    if not line.endswith(b"\n"):
        problems.append("unterminated record")
    body = line[:-1] if line.endswith(b"\n") else line
    if b"\r" in body:
        problems.append("CR in record")
    if b"\x00" in body:
        problems.append("NUL in record")
    try:
        record = parse_record(body.decode("ascii"))
    except UnicodeDecodeError:
        return None, problems + ["non-ASCII byte in record"]
    except (AssertionError, ValueError) as error:
        return None, problems + ["unparseable: %s" % error]
    if not isinstance(record, dict):
        return None, problems + ["not a JSON object"]
    if record.get("schema") != SCHEMA:
        problems.append("schema %r" % (record.get("schema"),))
    if type(record.get("protocol")) is not int or record.get("protocol") != PROTOCOL:
        problems.append("protocol %r" % (record.get("protocol"),))
    if record.get("profile") != PROFILE:
        problems.append("profile %r" % (record.get("profile"),))
    if not isinstance(record.get("type"), str):
        problems.append("type %r" % (record.get("type"),))
    if not isinstance(record.get("command"), str):
        problems.append("command %r" % (record.get("command"),))
    identity = record.get("id")
    if not is_uint32(identity):
        problems.append("id %r is not a uint32 integer" % (identity,))
    elif identity == 0 and record.get("type") != "trace":
        problems.append("id 0 on a %r record; only unsolicited traces carry id 0" % (record.get("type"),))
    return record, problems
