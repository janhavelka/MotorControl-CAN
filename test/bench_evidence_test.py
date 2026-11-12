#!/usr/bin/env python3
"""Exercise pure console evidence interpretation without streams or hardware."""

import copy
import json
from pathlib import Path
import subprocess
import sys
import unittest


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import _bench_evidence as evidence


def receipt():
    """Synthetic accepted read; wire validation belongs to the Console tests."""
    return dict(type="reply", command="read", id=7, result="accepted",
                operation_id=4, ok=True, status="OK")


def terminal(**fields):
    result = dict(type="terminal", command="read", id=7, command_id=7,
                  operation_id=4, ok=True, status="OK", detail=0,
                  outcome="SUCCESS", uncertain=False, completed_us="18446744073709551615",
                  abort_code=None)
    result.update(fields)
    return result


def summary(records, **metadata):
    return evidence.summarize_operation(records, intent="read identity 20",
                                        evidence_kind=metadata.pop("evidence_kind", "host-tested"),
                                        **metadata)


class PureEvidence(unittest.TestCase):
    def test_import_and_summary_have_no_clock_serial_session_or_file_io(self):
        code = '''
import builtins
import importlib.abc
import sys
import time
sys.path.insert(0, sys.argv[1])
class ForbiddenSession(importlib.abc.MetaPathFinder):
    def find_spec(self, fullname, path=None, target=None):
        if fullname.split(".")[0] in {"bench_probe", "serial"}:
            raise AssertionError("pure evidence imported serial/session")
sys.meta_path.insert(0, ForbiddenSession())
def forbidden(*args, **kwargs):
    raise AssertionError("pure evidence performed file/clock I/O")
builtins.open = forbidden
time.monotonic = time.time = time.sleep = forbidden
import _bench_evidence as evidence
result = evidence.summarize_operation([], intent="probe 20", evidence_kind="planned")
assert result["classification"] == "planned"
assert evidence.summarize_execution([result])["operation_count"] == 1
assert "bench_probe" not in sys.modules and "serial" not in sys.modules
'''
        completed = subprocess.run([sys.executable, "-I", "-B", "-c", code,
                                    str(ROOT / "scripts")], capture_output=True, text=True, timeout=15)
        self.assertEqual(completed.returncode, 0, completed.stderr)

    def test_planned_skipped_host_and_live_evidence_stay_distinct(self):
        operations = [summary([], evidence_kind=kind) for kind in ("planned", "skipped")]
        operations += [summary([receipt(), terminal()], evidence_kind=kind)
                       for kind in ("host-tested", "live-observed")]
        result = evidence.summarize_execution(operations)
        self.assertEqual(result["evidence_counts"], dict.fromkeys(evidence.EVIDENCE_KINDS, 1))
        self.assertEqual(result["classification_counts"], {"planned": 1, "skipped": 1, "terminal": 2})
        self.assertNotIn("hardware_qualified", result)
        self.assertNotIn("ok", result)
        for kind in ("planned", "skipped"):
            for fields in ({"transmitted": True}, {"interruption": "disconnect"}, {"operation_id": 4}):
                with self.subTest(kind=kind, fields=fields), self.assertRaises(evidence.BenchError):
                    summary([], evidence_kind=kind, **fields)
            with self.assertRaises(evidence.BenchError):
                summary([receipt()], evidence_kind=kind)

    def test_exact_values_full_abort_and_original_failure_survive_summary_json(self):
        value = dict(type="value", raw="9007199254740993", native_signed="-9223372036854775808",
                     requested=["9007199254740993", "1", "3", False],
                     preview=1.25, exact=False, current=False, interpretation=None,
                     unrelated_exact_integer=9007199254740993)
        failure = terminal(ok=False, status="PROTOCOL_ERROR", detail=-2147483648,
                           outcome="REPLY_ERROR", abort_code=0xFFFFFFFF,
                           execution_code=2, completion_code=1, native_operation=4294967295)
        result = json.loads(json.dumps(summary([receipt(), value, failure]), allow_nan=False))
        self.assertEqual(result["classification"], "native_rejection")
        self.assertEqual(result["status"], "PROTOCOL_ERROR")
        self.assertEqual(result["detail"], -2147483648)
        self.assertEqual(result["abort_code"], 0xFFFFFFFF)
        self.assertTrue(result["abort_known"])
        self.assertEqual(result["records"][1], value)
        self.assertEqual(result["records"][2], failure)
        self.assertEqual(result["execution_code"], 2)
        self.assertEqual(result["completion_code"], 1)
        self.assertEqual(result["native_operation"], 4294967295)

    def test_local_refusal_is_different_from_lost_handoff(self):
        refusal = dict(type="reply", id=7, command="read", ok=False,
                       status="UNSUPPORTED", detail=0, reason="qualification_required")
        result = summary([refusal], transmitted=True)
        self.assertEqual(result["classification"], "local_refusal")
        self.assertFalse(result["admission"])
        self.assertIsNone(result["uncertain"])
        before_write = summary([], interruption="disconnect")
        self.assertEqual(before_write["classification"], "no_admission")
        after_write = summary([], transmitted=True, interruption="disconnect")
        self.assertEqual(after_write["classification"], "unknown_after_handoff")
        self.assertFalse(after_write["admission"])
        self.assertIsNone(after_write["uncertain"])
        self.assertEqual(summary([], transmitted=True)["classification"], "unknown_after_handoff")
        local_reply = dict(type="reply", id=7, command="host", ok=True, status="OK")
        known_local = summary([local_reply], transmitted=True, interruption="framing_loss")
        self.assertEqual(known_local["classification"], "local_reply")
        self.assertEqual(known_local["status"], "OK")

    def test_accepted_work_without_terminal_preserves_unknown_outcome(self):
        pending = summary([receipt()], transmitted=True)
        self.assertEqual(pending["classification"], "pending")
        self.assertTrue(pending["admission"])
        self.assertFalse(pending["terminal_known"])
        for interruption in ("framing_loss", "reboot", "disconnect", "deadline"):
            with self.subTest(interruption=interruption):
                result = summary([receipt()], transmitted=True, interruption=interruption)
                self.assertEqual(result["classification"], "unknown_after_admission")
                self.assertIsNone(result["outcome"])
                self.assertIsNone(result["uncertain"])
                self.assertIsNone(result["abort_code"])
                self.assertEqual(result["stream_integrity"], "intact" if interruption == "deadline" else "lost")
        confirmed_id = summary([], operation_id=4, interruption="deadline")
        self.assertTrue(confirmed_id["admission"])
        self.assertEqual(confirmed_id["classification"], "unknown_after_admission")

    def test_original_terminal_survives_later_framing_loss_or_reboot(self):
        original = terminal()
        for interruption in ("framing_loss", "reboot", "disconnect"):
            with self.subTest(interruption=interruption):
                result = summary([receipt(), original], interruption=interruption)
                self.assertEqual(result["classification"], "terminal")
                self.assertEqual(result["outcome"], "SUCCESS")
                self.assertIs(result["uncertain"], False)
                self.assertEqual(result["stream_integrity"], "lost")
                self.assertEqual(result["records"][-1], original)

    def test_uncertain_abort_does_not_become_a_known_refusal(self):
        result = summary([receipt(), terminal(ok=False, uncertain=True,
                         outcome="DEADLINE", abort_code=0x08000000)])
        self.assertEqual(result["classification"], "unknown_after_admission")
        self.assertTrue(result["uncertain"])
        self.assertTrue(result["abort_known"])
        self.assertEqual(result["outcome"], "DEADLINE")

    def test_lookup_identity_is_original_command_not_lookup_id(self):
        lookup = terminal(type="reply", command="result", id=99)
        result = summary([lookup])
        self.assertTrue(result["admission"])
        self.assertTrue(result["terminal_known"])
        self.assertEqual((result["request_id"], result["operation_id"]), (7, 4))
        explicit = summary([lookup], request_id=7, operation_id=4)
        self.assertEqual(explicit, result)

    def test_owned_values_and_provenance_do_not_alias_caller_or_execution(self):
        records = [receipt(), dict(type="value", raw=["1", {"source": "synthetic"}]), terminal()]
        provenance = dict(firmware=None, fixture={"nodes": [10, 20]}, source="fake-stream")
        original_records, original_provenance = copy.deepcopy(records), copy.deepcopy(provenance)
        operation = summary(records, provenance=provenance)
        execution = evidence.summarize_execution([operation], provenance=provenance)
        records[1]["raw"][1]["source"] = "changed"
        provenance["fixture"]["nodes"].append(30)
        self.assertEqual(operation["records"], original_records)
        self.assertEqual(operation["provenance"], original_provenance)
        operation["records"][1]["raw"].append("changed later")
        self.assertEqual(execution["operations"][0]["records"], original_records)
        self.assertEqual(execution["provenance"], original_provenance)

    def test_no_physical_state_or_sensor_retry_verdict_is_inferred(self):
        result = summary([receipt(), terminal(command="stop", outcome="STOP_COMPLETED")])
        for absent in ("stopped", "enabled", "healthy", "retry", "hardware_qualified", "drive_state"):
            self.assertNotIn(absent, result)
        self.assertEqual(result["outcome"], "STOP_COMPLETED")
        self.assertFalse(result["abort_known"])
        self.assertIsNone(result["execution_code"])

    def test_record_operation_byte_and_nesting_bounds_are_explicit(self):
        summary([{}] * evidence.MAX_SUMMARY_RECORDS)
        with self.assertRaises(evidence.BenchError):
            summary([{}] * (evidence.MAX_SUMMARY_RECORDS + 1))
        operation = summary([], evidence_kind="planned")
        execution = evidence.summarize_execution([operation] * evidence.MAX_OPERATIONS)
        self.assertEqual(execution["operation_count"], 16)
        with self.assertRaises(evidence.BenchError):
            evidence.summarize_execution([operation] * (evidence.MAX_OPERATIONS + 1))
        for value in ("x" * evidence.MAX_SUMMARY_BYTES, "\u00e9" * (evidence.MAX_SUMMARY_BYTES // 3)):
            with self.subTest(value_length=len(value)), self.assertRaises(evidence.BenchError):
                summary([], provenance={"large": value})
        nested = []
        for _ in range(evidence.MAX_SUMMARY_DEPTH):
            nested = [nested]
        with self.assertRaises(evidence.BenchError):
            summary([], provenance={"nested": nested})
        cycle = []
        cycle.append(cycle)
        with self.assertRaises(evidence.BenchError):
            summary([], provenance={"cycle": cycle})

    def test_execution_enforces_one_shared_byte_bound(self):
        operation = summary([], provenance={"padding": "x" * (evidence.MAX_SUMMARY_BYTES // 2)})
        with self.assertRaises(evidence.BenchError):
            evidence.summarize_execution([operation, operation])
        result = summary([], provenance={"padding": "x" * 2000})
        self.assertLessEqual(len(json.dumps(result, ensure_ascii=True, separators=(",", ":"))),
                             evidence.MAX_SUMMARY_BYTES)

    def test_invalid_json_values_or_metadata_fail_without_mutating_inputs(self):
        for value in (float("nan"), float("inf"), float("-inf"), b"bytes", (1, 2), {1: "key"}):
            with self.subTest(value=repr(value)), self.assertRaises(evidence.BenchError):
                summary([], provenance={"invalid": value})
        for metadata in ({"request_id": True}, {"request_id": 0}, {"operation_id": -1},
                         {"operation_id": 2**32}, {"transmitted": 1},
                         {"interruption": "retry"}, {"provenance": []},
                         {"evidence_kind": "passed"}):
            with self.subTest(metadata=metadata), self.assertRaises(evidence.BenchError):
                summary([], **metadata)
        records = [receipt(), terminal()]
        original = copy.deepcopy(records)
        with self.assertRaises(evidence.BenchError):
            summary(records, provenance={"invalid": b"bytes"})
        self.assertEqual(records, original)

    def test_json_hooks_reject_duplicates_and_nonfinite_constants(self):
        for text in ('{"id":7,"id":8}', '{"value":NaN}', '{"value":Infinity}'):
            with self.subTest(text=text), self.assertRaises(ValueError):
                json.loads(text, object_pairs_hook=evidence.unique_object,
                           parse_constant=evidence.invalid_constant)
        exact = json.loads('{"raw":"9007199254740993","native":9007199254740993}',
                           object_pairs_hook=evidence.unique_object,
                           parse_constant=evidence.invalid_constant)
        self.assertEqual(exact, {"raw": "9007199254740993", "native": 9007199254740993})
        for identity in (1, 0xFFFFFFFF):
            self.assertTrue(evidence.operation_id(identity))
        for identity in (None, True, False, 0, -1, 2**32, "1", 1.0):
            self.assertFalse(evidence.operation_id(identity))


if __name__ == "__main__":
    unittest.main()
