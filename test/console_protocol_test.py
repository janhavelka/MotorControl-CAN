#!/usr/bin/env python3
"""Parse the real shared console output; no copied JSON success records."""

import json
import subprocess
import sys
import tempfile
from pathlib import Path


def unique_object(pairs):
    """Repeated keys make a record ambiguous even when json.loads accepts it."""
    result = {}
    for key, value in pairs:
        assert key not in result, "duplicate JSON key: " + key
        result[key] = value
    return result


def reject_constant(value):
    raise AssertionError("non-finite JSON constant: " + value)


def parse_record(line):
    return json.loads(line, object_pairs_hook=unique_object, parse_constant=reject_constant)


def main():
    completed = subprocess.run([sys.argv[1], "--json-transcript"], check=True,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=15)
    data = completed.stdout
    assert data and data.endswith(b"\n"), "incomplete output record"
    lines = data.splitlines(keepends=True)
    records = []
    for line in lines:
        assert len(line) <= 511, "record exceeds the documented atomic bound"
        assert line.endswith(b"\n") and b"\r" not in line and b"\x00" not in line
        record = parse_record(line)
        assert isinstance(record, dict)
        assert record["schema"] == "motorcontrolcan.console/1"
        assert record["protocol"] == 3 and record["profile"] == "lichuan_c"
        assert type(record["id"]) is int and 1 <= record["id"] <= 4294967295
        assert isinstance(record["command"], str)
        records.append(record)
    by_id = {}
    for record in records:
        by_id.setdefault(record["id"], []).append(record)
    assert set(by_id) == set(range(1, 15))
    assert any(r.get("product") == "MotorControl-CAN" for r in by_id[1])
    status = by_id[2][0]
    assert status["selected"] is False and status["uncertain"] is False
    assert status["nmt"] is None and status["drive_state"] is None
    config = by_id[5][0]
    assert config["interface"] is None and config["bitrate_bits_per_second"] is None
    assert config["selected_node"] is None and config["bus_traffic"] is False
    assert by_id[6][0]["free_heap"] is None
    assert any(r.get("ok") is False for r in by_id[10])
    exact = [r for r in by_id[8] if r.get("ok") is True]
    assert exact, "exact preparation was not accepted"
    assert all(isinstance(r["error_numerator"], str) and
               isinstance(r["error_denominator"], str) for r in exact)

    def find_exact(value):
        if isinstance(value, dict):
            return any(find_exact(v) for v in value.values())
        if isinstance(value, list):
            return any(find_exact(v) for v in value)
        assert value != 9007199254740993 or isinstance(value, str), "wide integer lost its string type"
        return value == "9007199254740993"

    assert any(find_exact(r) for r in exact), "wide exact target was omitted or rounded"
    terminals = [r for r in by_id[11] if r["type"] == "terminal"]
    assert len(terminals) == 1
    terminal = terminals[0]
    assert terminal["operation_id"] == 1 and terminal["command_id"] == 11
    assert terminal["outcome"] == "RETIRED" and type(terminal["uncertain"]) is bool
    assert isinstance(terminal["completed_us"], str) and terminal["completed_us"].isdigit()
    lookup = [r for r in by_id[12] if r.get("operation_id") == 1]
    assert len(lookup) == 1 and lookup[0]["command_id"] == 11
    for key in ("outcome", "status", "uncertain"):
        assert lookup[0][key] == terminal[key], "lookup changed retained terminal evidence"
    axis = {r.get("part"): r for r in by_id[14] if r["type"] == "axis"}
    assert set(axis) == {"geometry", "feedback", "limits", "provenance"}
    assert axis["geometry"]["command_scale"] == ["1", "1", 1]
    assert any(r.get("result") == "complete" for r in by_id[14])
    print("console_protocol: parsed {} actual bounded JSONL records".format(len(records)))
    # Execute the separate real native entry too; linked test helpers cannot prove
    # its argument handling or startup wiring. No serial dependency or hardware.
    with tempfile.TemporaryDirectory(prefix="CAN console script ") as directory:
        script = Path(directory) / "commands.txt"
        script.write_bytes(b"@1 version\n@2 axis config set command 1\n"
                           b"@3 prepare absolute 9007199254740993 steps native\n"
                           b"@4 read identity\n")
        native = subprocess.run([sys.argv[2], "--console-fake", str(script)],
                                check=True, capture_output=True, timeout=15)
        actual = [parse_record(line) for line in native.stdout.splitlines()]
        assert actual[0]["product"] == "MotorControl-CAN"
        assert any(r.get("effective_native") == "9007199254740993" for r in actual)
        assert actual[-1]["id"] == 4 and actual[-1]["ok"] is False
        script.write_bytes(b"@1 wiring di0 connected\n" + b"x" * 256 + b"\n")
        refused = subprocess.run([sys.argv[2], "--console-fake", str(script)],
                                 capture_output=True, timeout=15)
        assert refused.returncode == 2 and not refused.stdout
        script.write_bytes(b"@1 wiring di0 connected")
        truncated = subprocess.run([sys.argv[2], "--console-fake", str(script)],
                                   capture_output=True, timeout=15)
        assert truncated.returncode == 2 and not truncated.stdout


if __name__ == "__main__":
    main()
