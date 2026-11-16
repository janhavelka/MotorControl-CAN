#!/usr/bin/env python3
"""Independent campaign failure vectors over the actual Console byte parser.

No fixture in this file opens serial or implements CAN/motion. The supplied
records follow ProbeConsoleMotion serialization; expected values are literal
vectors, independent of the client validation and summary helpers under test.
"""
from pathlib import Path
from contextlib import contextmanager
import json
import sys
import tempfile
import unittest
from unittest import mock
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
sys.path.insert(0, str(ROOT / "test"))

import bench_probe as bench
from bench_console_adversarial_test import Clock, Port, record, reply
from bench_console_test import value, terminal


MOVE = "move relative 1 steps native 120 configured setup write"


def velocity_change(identity, **changes):
    fields = dict(operation_id=1, change_index=1, native_rpm="20", record=52, gate_code=2,
                  execution_code=3, uncertain=True, effect_possible=True, observed_cycle=0,
                  first_report_rpm=0, abort_code=None, exact=True, error=["0", "1", False],
                  report_present=False, qualified=False)
    fields.update(changes)
    if fields.get("exact") is False:
        fields.pop("error", None)
    return record(identity, "velocity_change", "velocity", **fields)


def move_relation(identity, operation):
    return record(identity, "move_relation", "move", operation_id=operation, original=1,
                  replacement=2, observation_superseded=True, physical_interruption_observed=False)


def admitted(identity, family="move", operation=1):
    return reply(identity, family, result="accepted", operation_id=operation)


def motion_value(identity, *, operation=1, field="target", **changes):
    """Exact int64 endpoint just above binary64 precision, with a half-step error."""
    fields = dict(operation_id=operation, field=field, unit="steps", native="1",
                  exact=True, relative=True, endpoint_native="9007199254740993",
                  requested_endpoint=["9007199254740993", "1", "2", False],
                  error=["1", "2", True])
    if field == "speed":
        fields = dict(operation_id=operation, field="speed", unit="rpm", native="120",
                      exact=True, requested=["120", "0", "1", False],
                      error=["0", "1", False])
    fields.update(changes)
    return record(identity, "motion_value", "move", **fields)


def motion_terminal(identity, *, operation=1, family="move", ok=True,
                    uncertain=False, **changes):
    fields = dict(ok=ok, status="OK" if ok else "PROTOCOL_ERROR", detail=0,
                  outcome="OBSERVED" if ok else "DEADLINE", uncertain=uncertain,
                  command_id=identity, operation_id=operation,
                  completed_us="9007199254740995", abort_code=None,
                  execution_code=1 if ok else 3, completion_code=1 if ok else 0,
                  native_operation=73)
    if family == "stop":
        fields.update(stop_path_code=4, superseded=False)
    fields.update(changes)
    return record(identity, "terminal", family, **fields)


def stop_attempt(identity, *, operation=2, attempt="sequenced", **changes):
    fields = dict(operation_id=operation, attempt=attempt, native_operation=74,
                  outcome="OBSERVED", execution_code=1, completion_code=1,
                  uncertain=False, reply_qualifies=attempt == "sequenced", abort_code=None)
    fields.update(changes)
    return record(identity, "stop_attempt", "stop", **fields)


def successful_move(identity):
    return (admitted(identity) + motion_value(identity) +
            motion_value(identity, field="speed") + motion_terminal(identity))


class CampaignPort(Port):
    """Extend the independently defined handshake with the actual campaign families."""
    @staticmethod
    def handshake(identity, command):
        original = Port.handshake(identity, command)
        if command != "caps":
            return original
        extra = b"".join(record(identity, "capability", family, implemented=True,
                               owner=42, effect="device_action", completion="asynchronous",
                               reason="supplied host test records")
                         for family in ("move", "stop", "home", "velocity"))
        return extra + original


class CampaignAdversarial(unittest.TestCase):
    def test_change_receipt_belongs_to_original_run(self):
        console, port, _ = self.client()
        port.handler = lambda identity, _: admitted(identity, "velocity", 1)
        original = console.begin("velocity 10 rpm motor 12000 configured normal")
        while port.buffer:
            console.pump()
        port.handler = lambda identity, _: reply(identity, "velocity", result="accepted", operation_id=1, change_index=1)
        change = console.command("velocity change 1 20 rpm motor configured")
        self.assertEqual(change["operation_id"], 1)
        self.assertIsNone(original.terminal)
        self.assertEqual(set(console.operations), {1})
        port.buffer.extend(velocity_change(original.id) + motion_terminal(original.id, family="velocity", ok=False, uncertain=True))
        console.wait(original)
        self.assertEqual(original.records[-2]["type"], "velocity_change")

    def test_malformed_or_foreign_change_evidence_poison_stream(self):
        for changes in ({"operation_id": 99}, {"uncertain": "false"}, {"gate_code": True},
                        {"qualified": True}, {"error": ["1", "0", False]},
                        {"report_present": False, "observed_cycle": 1}, {"execution_code": 3, "uncertain": False}):
            with self.subTest(changes=changes):
                console, port, _ = self.client()
                port.handler = lambda identity, _: admitted(identity, "velocity", 1) + velocity_change(identity, **changes)
                with self.assertRaises(bench.BenchError):
                    console.command("velocity 10 rpm motor 12000 configured normal")
                self.assertFalse(console.synchronized)

    def test_change_receipt_rejects_foreign_or_boolean_operation(self):
        for operation in (99, True):
            console, port, _ = self.client()
            port.handler = lambda identity, _: admitted(identity, "velocity", 1)
            console.begin("velocity 10 rpm motor 12000 configured normal")
            while port.buffer:
                console.pump()
            port.handler = lambda identity, _: reply(identity, "velocity", result="accepted", operation_id=operation, change_index=1)
            with self.assertRaises(bench.BenchError):
                console.command("velocity change 1 20 rpm motor configured")

    def test_run_terminal_cannot_erase_change_uncertainty(self):
        console, port, _ = self.client()
        port.handler = lambda identity, _: (admitted(identity, "velocity", 1) + velocity_change(identity) +
                                           motion_terminal(identity, family="velocity", ok=False, uncertain=False))
        with self.assertRaises(bench.BenchError):
            console.command("velocity 10 rpm motor 12000 configured normal")

    def test_velocity_change_approximation_preserves_bound(self):
        console, port, _ = self.client()
        port.handler = lambda identity, _: (admitted(identity, "velocity", 1) +
            velocity_change(identity, exact=False, error_bound="0.000123") +
            motion_terminal(identity, family="velocity", ok=False, uncertain=True))
        handle = console.begin("velocity 10 rpm motor 12000 configured normal")
        console.wait(handle)
        self.assertEqual(handle.records[-2]["error_bound"], "0.000123")
        self.assertNotIn("error", handle.records[-2])
        for bound in ("nan", "inf", "-1", 0.01, "x"):
            console, port, _ = self.client()
            port.handler = lambda identity, _: (admitted(identity, "velocity", 1) +
                velocity_change(identity, exact=False, error_bound=bound))
            with self.assertRaises(bench.BenchError):
                console.command("velocity 10 rpm motor 12000 configured normal")

    def test_move_pair_survives_original_release(self):
        console, port, _ = self.client()
        port.handler = lambda identity, _: admitted(identity)
        original = console.begin(MOVE)
        while port.buffer:
            console.pump()
        port.handler = lambda identity, _: admitted(identity, operation=2)
        replacement = console.begin("move speed 1 20 immediate")
        while port.buffer:
            console.pump()
        port.buffer.extend(move_relation(original.id, 1) + motion_terminal(original.id, ok=False, uncertain=True))
        console.wait(original)
        port.handler = lambda identity, _: reply(identity, "release", result="released", operation_id=1)
        self.assertTrue(console.release(original)["ok"])
        port.buffer.extend(move_relation(replacement.id, 2) + motion_terminal(replacement.id, operation=2, ok=False, uncertain=True))
        console.wait(replacement)
        self.assertTrue(console.synchronized)
        self.assertEqual(replacement.records[-2]["original"], 1)

    def test_foreign_relation_and_unknown_record_are_rejected(self):
        for kind, fields in (("move_relation", dict(original=1, replacement=99, observation_superseded=True,
                                                  physical_interruption_observed=False)), ("future_change", {})):
            console, port, _ = self.client()
            port.handler = lambda identity, _: admitted(identity) + record(identity, kind, "move", operation_id=1, **fields)
            with self.assertRaises(bench.BenchError):
                console.command(MOVE)

    def test_reserved_stop_receipt_may_precede_earlier_move_receipt(self):
        console, port, _ = self.client()
        port.handler = lambda *_: b""
        move = console.begin(MOVE)
        stop = console.begin("stop normal")
        port.buffer.extend(admitted(stop.id, "stop", 2) + admitted(move.id, "move", 1))
        for _ in range(8):
            if not port.buffer:
                break
            console.pump()
        self.assertTrue(move.accepted)
        self.assertTrue(stop.accepted)
        self.assertEqual(set(console.operations), {1, 2})
        self.assertTrue(console.synchronized)

    def test_lower_operation_on_a_newer_request_still_indicates_epoch_loss(self):
        console, port, _ = self.client()
        port.handler = lambda identity, _: admitted(identity, "move", 7)
        first = console.begin(MOVE)
        console.pump()
        self.assertTrue(first.accepted)
        port.handler = lambda identity, _: admitted(identity, "move", 6)
        second = console.begin(MOVE)
        with self.assertRaises(bench.BenchError):
            console.wait(second)
        self.assertFalse(console.synchronized)

    def client(self, on_event=None):
        clock, port = Clock(), CampaignPort()
        console = bench.Console(port, clock=clock, sleeper=clock.sleep,
                                first_id=1, host_epoch="campaign-adversarial",
                                on_event=on_event)
        console.identify(expected_version="0.1.0")
        return console, port, clock

    def run_move(self, console, record):
        """A scenario owns one cleanup, preserving its original failure explicitly."""
        try:
            return bench.checked(console, MOVE, record=record, timeout_s=0.025)
        except (bench.BenchError, KeyboardInterrupt) as error:
            record.setdefault("primary_error", str(error) or "interrupted")
            raise
        finally:
            bench.cleanup_stop_attempts(console, record, stop="normal", timeout_s=0.025)

    def commands(self, port):
        return [wire.decode("ascii").rstrip("\n").split(" ", 1)[1]
                for wire in port.writes]

    def assert_no_cleanup_replay(self, console, port, report):
        before = list(port.writes)
        self.assertFalse(console.synchronized)
        bench.cleanup_stop_attempts(console, report, stop="normal", timeout_s=0.025)
        self.assertEqual(port.writes, before)
        self.assertEqual(self.commands(port).count(MOVE), 1)
        self.assertFalse(any(command.startswith("stop ") for command in self.commands(port)))
        self.assertFalse(any(command.startswith(("recover", "enable", "result "))
                             for command in self.commands(port)))

    def test_lost_receipt_preserves_unknown_handoff_and_forbids_cleanup(self):
        console, port, clock = self.client()
        port.handler = lambda *_: b""
        report = {}
        with self.assertRaises(bench.BenchError):
            self.run_move(console, report)
        self.assertLess(clock.now, 1.0)
        handles = [h for h in console.pending.values() if h.command == MOVE]
        self.assertEqual(len(handles), 1)
        summary = console.summary(handles[0])
        self.assertEqual(summary["classification"], "unknown_after_handoff")
        self.assertFalse(summary["admission"])
        self.assertTrue(summary["transmitted"])
        self.assert_no_cleanup_replay(console, port, report)

    def test_corrupted_receipt_and_stale_terminal_do_not_admit_success(self):
        vectors = (
            lambda identity: admitted(identity).replace(b'"operation_id":1', b'"operation_id":true'),
            lambda identity: admitted(identity) + motion_terminal(identity, command_id=identity - 1),
            lambda identity: admitted(identity) + motion_terminal(identity, operation=2),
        )
        for vector in vectors:
            with self.subTest(vector=vector):
                console, port, _ = self.client()
                port.handler = lambda identity, _: vector(identity)
                report = {}
                with self.assertRaises(bench.BenchError):
                    self.run_move(console, report)
                self.assert_no_cleanup_replay(console, port, report)

    def test_reboot_after_admission_never_reenables_or_resumes(self):
        console, port, _ = self.client()
        port.handler = lambda identity, _: admitted(identity) + b"ESP-ROM:esp32s2 reset\n"
        report = {}
        with self.assertRaises(bench.BenchError):
            self.run_move(console, report)
        handle = next(h for h in console.pending.values() if h.command == MOVE)
        summary = console.summary(handle)
        self.assertEqual(summary["interruption"], "reboot")
        self.assertEqual(summary["classification"], "unknown_after_admission")
        self.assert_no_cleanup_replay(console, port, report)

    def test_duplicate_terminal_retains_first_evidence_but_poisoned_stream(self):
        console, port, _ = self.client()
        port.handler = lambda identity, _: successful_move(identity) + motion_terminal(identity)
        report = {}
        with self.assertRaises(bench.BenchError):
            self.run_move(console, report)
        handle = next(h for h in console.pending.values() if h.command == MOVE)
        summary = console.summary(handle)
        self.assertTrue(summary["terminal_known"])
        self.assertEqual(summary["outcome"], "OBSERVED")
        self.assertEqual(summary["stream_integrity"], "lost")
        self.assert_no_cleanup_replay(console, port, report)

    def test_short_write_is_unknown_handoff_without_second_write(self):
        console, port, _ = self.client()
        port.short_write = True
        report = {}
        with self.assertRaises(bench.BenchError):
            self.run_move(console, report)
        handle = next(h for h in console.pending.values() if h.command == MOVE)
        self.assertEqual(console.summary(handle)["classification"], "unknown_after_handoff")
        self.assert_no_cleanup_replay(console, port, report)

    def test_disconnect_after_admission_preserves_unknown_without_cleanup(self):
        console, port, _ = self.client()
        port.handler = lambda identity, _: admitted(identity)
        def disconnect(event, **fields):
            if event == "line" and b'"result":"accepted"' in bytes.fromhex(fields["raw_hex"]):
                port.is_open = False
        console.emit = disconnect
        report = {}
        with self.assertRaises(bench.BenchError):
            self.run_move(console, report)
        handle = next(h for h in console.pending.values() if h.command == MOVE)
        self.assertTrue(handle.accepted)
        self.assertEqual(console.summary(handle)["classification"], "unknown_after_admission")
        self.assert_no_cleanup_replay(console, port, report)

    def test_interrupted_partial_write_cannot_trigger_cleanup_on_damaged_stream(self):
        console, port, _ = self.client()
        def interrupted_write(data):
            port.writes.append(bytes(data))
            raise KeyboardInterrupt("interrupted during serial handoff")
        port.write = interrupted_write
        report = {}
        with self.assertRaises((bench.BenchError, KeyboardInterrupt)):
            self.run_move(console, report)
        handle = next(h for h in console.pending.values() if h.command == MOVE)
        self.assertTrue(console.summary(handle)["transmitted"])
        self.assert_no_cleanup_replay(console, port, report)

    def test_primary_uncertainty_survives_successful_separate_cleanup(self):
        console, port, _ = self.client()
        def respond(identity, command):
            if command == MOVE:
                return admitted(identity) + motion_terminal(identity, ok=False, uncertain=True)
            if command == "stop normal":
                return (admitted(identity, "stop", 2) + stop_attempt(identity) +
                        motion_terminal(identity, operation=2, family="stop"))
            if command.startswith("release "):
                return reply(identity, "release")
            self.fail("unexpected campaign traffic: " + command)
        port.handler = respond
        report = {}
        with self.assertRaises(bench.BenchError):
            self.run_move(console, report)
        original = report["primary_error"]
        self.assertTrue(original)
        self.assertEqual(self.commands(port).count("stop normal"), 1)
        bench.cleanup_stop_attempts(console, report, stop="normal", timeout_s=0.025)
        self.assertEqual(report["primary_error"], original)
        self.assertEqual(self.commands(port).count("stop normal"), 1)
        self.assertEqual(self.commands(port).count(MOVE), 1)

    def test_lost_cleanup_terminal_never_replaces_primary_or_repeats_stop(self):
        console, port, clock = self.client()
        def respond(identity, command):
            if command == MOVE:
                return admitted(identity) + motion_terminal(identity, ok=False, uncertain=True)
            if command == "stop normal":
                return admitted(identity, "stop", 2)
            if command.startswith("release "):
                return reply(identity, "release")
            self.fail("unexpected campaign traffic: " + command)
        port.handler = respond
        report = {}
        with self.assertRaises(bench.BenchError):
            self.run_move(console, report)
        primary = report["primary_error"]
        self.assertLess(clock.now, 1.0)
        self.assertFalse(console.synchronized)
        self.assertEqual(self.commands(port).count("stop normal"), 1)
        bench.cleanup_stop_attempts(console, report, stop="normal", timeout_s=0.025)
        self.assertEqual(report["primary_error"], primary)
        self.assertEqual(self.commands(port).count("stop normal"), 1)

    def test_stop_terminal_at_deadline_is_late_and_keeps_original_failure(self):
        console, port, clock = self.client()
        port.handler = lambda identity, _: admitted(identity, "stop", 1)
        handle = console.begin("stop normal", timeout_s=0.025)
        console.pump()
        port.buffer.extend(stop_attempt(handle.id, operation=1) +
                           motion_terminal(handle.id, operation=1, family="stop"))
        clock.now = handle.deadline
        with self.assertRaises(bench.BenchError):
            console.wait(handle)
        self.assertIsNone(handle.terminal)
        self.assertEqual(console.summary(handle)["classification"], "unknown_after_admission")
        self.assertEqual(self.commands(port).count("stop normal"), 1)

    def test_finite_velocity_can_wait_past_eight_seconds_under_its_original_budget(self):
        console, port, clock = self.client()
        port.handler = lambda identity, _: admitted(identity, "velocity", 1)
        handle = console.begin("velocity 120 rpm motor 12000 configured normal", timeout_s=25)
        original_read, delivered = port.read, [False]
        def delayed_read(count):
            if clock.now >= 12.1 and not delivered[0]:
                delivered[0] = True
                port.buffer.extend(record(handle.id, "motion_value", "velocity",
                    operation_id=1, field="speed", unit="rpm", native="120", exact=True,
                    requested=["120", "0", "1", False], error=["0", "1", False]) +
                    motion_terminal(handle.id, family="velocity"))
            return original_read(count)
        port.read = delayed_read
        terminal_record = console.wait(handle)
        self.assertTrue(terminal_record["ok"])
        self.assertGreaterEqual(clock.now, 12.1)
        self.assertLess(clock.now, 25)
        self.assertEqual(sum(command.startswith("velocity ") for command in self.commands(port)), 1)

    def test_best_effort_reply_cannot_be_promoted_to_qualified_stop(self):
        console, port, _ = self.client()
        port.handler = lambda identity, _: (
            admitted(identity, "stop", 1) +
            stop_attempt(identity, operation=1, attempt="best_effort", reply_qualifies=True) +
            motion_terminal(identity, operation=1, family="stop", ok=False, uncertain=True,
                            outcome="STOP_SENT_UNCONFIRMED"))
        with self.assertRaises(bench.BenchError):
            console.command("stop normal", timeout_s=0.025)
        self.assertFalse(console.synchronized)

    def test_best_effort_cannot_claim_acknowledgement_or_completion(self):
        for changes in (dict(execution_code=1, completion_code=0),
                        dict(execution_code=3, completion_code=1)):
            with self.subTest(changes=changes):
                console, port, _ = self.client()
                port.handler = lambda identity, _: (
                    admitted(identity, "stop", 1) +
                    stop_attempt(identity, operation=1, attempt="best_effort",
                                 outcome="STOP_SENT_UNCONFIRMED", uncertain=True, **changes) +
                    motion_terminal(identity, operation=1, family="stop", ok=False, uncertain=True,
                                    outcome="STOP_SENT_UNCONFIRMED"))
                with self.assertRaises(bench.BenchError):
                    console.command("stop normal", timeout_s=0.025)
                self.assertFalse(console.synchronized)

    def test_valid_best_effort_keeps_unknown_execution_and_no_observed_completion(self):
        console, port, _ = self.client()
        port.handler = lambda identity, _: (
            admitted(identity, "stop", 1) +
            stop_attempt(identity, operation=1, attempt="best_effort",
                         outcome="STOP_SENT_UNCONFIRMED", execution_code=3,
                         completion_code=0, uncertain=True) +
            motion_terminal(identity, operation=1, family="stop", ok=False,
                            uncertain=True, outcome="STOP_SENT_UNCONFIRMED"))
        handle = console.begin("stop normal", timeout_s=0.025)
        terminal = console.wait(handle)
        self.assertFalse(terminal["ok"])
        self.assertTrue(terminal["uncertain"])
        self.assertEqual(terminal["completion_code"], 0)
        summary = console.summary(handle)
        self.assertEqual(summary["classification"], "unknown_after_admission")
        attempts = [row for row in summary["records"] if row["type"] == "stop_attempt"]
        self.assertEqual(len(attempts), 1)
        self.assertFalse(attempts[0]["reply_qualifies"])
        self.assertEqual(attempts[0]["execution_code"], 3)

    def test_successful_stop_without_its_component_evidence_is_rejected(self):
        console, port, _ = self.client()
        port.handler = lambda identity, _: (
            admitted(identity, "stop", 1) +
            motion_terminal(identity, operation=1, family="stop"))
        with self.assertRaises(bench.BenchError):
            console.command("stop normal", timeout_s=0.025)
        self.assertFalse(console.synchronized)

    def test_exact_motion_values_survive_without_binary64_rounding(self):
        console, port, _ = self.client()
        port.handler = lambda identity, _: successful_move(identity)
        handle = console.begin(MOVE, timeout_s=0.025)
        console.wait(handle)
        values = [row for row in console.summary(handle)["records"]
                  if row["type"] == "motion_value"]
        self.assertEqual(values[0]["endpoint_native"], "9007199254740993")
        self.assertEqual(values[0]["requested_endpoint"],
                         ["9007199254740993", "1", "2", False])
        self.assertEqual(values[0]["error"], ["1", "2", True])
        self.assertEqual(values[1]["requested"], ["120", "0", "1", False])

    def test_success_without_observed_completion_or_exact_values_is_rejected(self):
        vectors = (
            lambda identity: (motion_value(identity) + motion_value(identity, field="speed") +
                              motion_terminal(identity, completion_code=0)),
            lambda identity: (motion_value(identity) + motion_value(identity, field="speed") +
                              motion_terminal(identity, execution_code=3)),
            lambda identity: motion_terminal(identity),
            lambda identity: motion_value(identity, field="speed") + motion_terminal(identity),
        )
        for vector in vectors:
            with self.subTest(vector=vector):
                console, port, _ = self.client()
                port.handler = lambda identity, _: admitted(identity) + vector(identity)
                with self.assertRaises(bench.BenchError):
                    console.command(MOVE, timeout_s=0.025)
                self.assertFalse(console.synchronized)

    def test_invalid_motion_fraction_fails_before_terminal_publication(self):
        for changes in (dict(requested_endpoint=["1", "1", "0", False]),
                        dict(native=9007199254740993),
                        dict(error=["1", "2", "false"])):
            with self.subTest(changes=changes):
                console, port, _ = self.client()
                port.handler = lambda identity, _: (admitted(identity) +
                                                     motion_value(identity, **changes) +
                                                     motion_value(identity, field="speed") +
                                                     motion_terminal(identity))
                with self.assertRaises(bench.BenchError):
                    console.command(MOVE, timeout_s=0.025)
                self.assertFalse(console.synchronized)

    def test_evidence_failure_before_handoff_does_not_send_motion(self):
        console, port, _ = self.client()
        def fail(event, **_fields):
            if event == "intent":
                raise OSError("artifact device full")
        console.emit = fail
        report = {}
        with self.assertRaises(bench.BenchError):
            self.run_move(console, report)
        self.assertNotIn(MOVE, self.commands(port))
        self.assertFalse(any(command.startswith("stop ") for command in self.commands(port)))

    def test_evidence_failure_after_handoff_preserves_unknown_and_blocks_cleanup(self):
        console, port, _ = self.client()
        port.handler = lambda identity, _: admitted(identity)
        def fail(event, **_fields):
            if event == "line":
                raise OSError("artifact device full")
        console.emit = fail
        report = {}
        with self.assertRaises(bench.BenchError):
            self.run_move(console, report)
        handle = next(h for h in console.pending.values() if h.command == MOVE)
        self.assertTrue(console.summary(handle)["transmitted"])
        self.assert_no_cleanup_replay(console, port, report)

    def test_interrupted_received_evidence_fences_cleanup_and_preserves_unknown(self):
        for line_number, classification in ((1, "unknown_after_handoff"),
                                            (2, "unknown_after_admission")):
            with self.subTest(interrupted_line=line_number):
                console, port, _ = self.client()
                port.handler = lambda identity, _: admitted(identity) + motion_value(identity)
                lines = 0
                interruption = KeyboardInterrupt("received evidence interrupted")

                def interrupt_once(event, **_fields):
                    nonlocal lines
                    if event == "line":
                        lines += 1
                        if lines == line_number:
                            raise interruption

                console.emit = interrupt_once
                report = {}
                with self.assertRaises(KeyboardInterrupt) as caught:
                    self.run_move(console, report)
                self.assertIs(caught.exception, interruption)
                self.assertEqual(report["primary_failure"]["classification"], classification)
                self.assertTrue(report["primary_failure"]["transmitted"])
                self.assertFalse(report["primary_failure"]["terminal_known"])
                self.assertEqual(report["primary_error"], str(interruption))
                self.assert_no_cleanup_replay(console, port, report)

    def test_interrupted_intent_evidence_fences_connection_before_handoff(self):
        console, port, _ = self.client()
        writes = list(port.writes)
        interruption = KeyboardInterrupt("intent evidence interrupted")

        def interrupt_once(event, **_fields):
            if event == "intent":
                console.emit = lambda *_args, **_kwargs: None
                raise interruption

        console.emit = interrupt_once
        with self.assertRaises(KeyboardInterrupt) as caught:
            console.begin(MOVE, timeout_s=0.025)
        self.assertIs(caught.exception, interruption)
        self.assertEqual(port.writes, writes)
        self.assertEqual(console.pending, {})
        self.assertFalse(console.synchronized)
        with self.assertRaises(bench.BenchError):
            console.begin(MOVE, timeout_s=0.025)
        self.assertEqual(port.writes, writes)

    def test_interrupt_after_transport_read_fences_unparsed_bytes_and_cleanup(self):
        console, port, clock = self.client()
        port.handler = lambda identity, command: admitted(identity) if command == MOVE else b""
        received = console.received
        interruption = KeyboardInterrupt("interrupted after transport read")
        interrupted = False

        def interrupt_after_read():
            nonlocal interrupted
            if console.received > received and not interrupted:
                interrupted = True
                raise interruption
            return clock()

        console.clock = interrupt_after_read
        report = {}
        with self.assertRaises(KeyboardInterrupt) as caught:
            self.run_move(console, report)
        self.assertIs(caught.exception, interruption)
        self.assertTrue(interrupted)
        self.assertEqual(report["primary_failure"]["classification"], "unknown_after_handoff")
        self.assertFalse(report["primary_failure"]["admission"])
        self.assert_no_cleanup_replay(console, port, report)

    def test_interrupt_after_write_return_fences_unvalidated_handoff_and_cleanup(self):
        for short_write in (True, False):
            with self.subTest(short_write=short_write):
                console, port, _ = self.client()
                port.short_write = short_write
                port.handler = lambda *_args: b""
                interruption = KeyboardInterrupt("interrupted before write count validation")
                interrupted = False

                def interrupt_after_write(frame, event, _argument):
                    nonlocal interrupted
                    if (event == "line" and frame.f_code is bench.Console.begin.__code__
                            and "size" in frame.f_locals and not interrupted):
                        interrupted = True
                        raise interruption
                    return interrupt_after_write

                previous_trace = sys.gettrace()
                report = {}
                try:
                    sys.settrace(interrupt_after_write)
                    with self.assertRaises(KeyboardInterrupt) as caught:
                        self.run_move(console, report)
                finally:
                    sys.settrace(previous_trace)
                self.assertIs(caught.exception, interruption)
                self.assertTrue(interrupted)
                self.assertEqual(report["primary_failure"]["classification"], "unknown_after_handoff")
                self.assertFalse(report["primary_failure"]["admission"])
                self.assertTrue(report["primary_failure"]["transmitted"])
                self.assert_no_cleanup_replay(console, port, report)

    def test_interrupted_partial_startup_remains_fenced_without_a_request(self):
        clock, port = Clock(), CampaignPort()
        console = bench.Console(port, clock=clock, sleeper=clock.sleep, first_id=1)
        port.buffer.extend(b"ESP-ROM:")
        interruption = KeyboardInterrupt("startup interrupted")
        interrupted = False

        def interrupt_after_read():
            nonlocal interrupted
            if console.received and not interrupted:
                interrupted = True
                raise interruption
            return clock()

        console.clock = interrupt_after_read
        with self.assertRaises(KeyboardInterrupt) as caught:
            console.drain_startup(0.02)
        self.assertIs(caught.exception, interruption)
        self.assertEqual(bytes(console.buffer), b"ESP-ROM:")
        self.assertFalse(console.synchronized)
        with self.assertRaises(bench.BenchError):
            console.begin("version", timeout_s=0.02)
        self.assertEqual(port.writes, [])


class EvidenceSinkAdversarial(unittest.TestCase):
    """Retained evidence must survive the sink's write and flush contracts."""

    class Sink:
        def __init__(self, result=None, error=None):
            self.result, self.error = result, error
            self.writes = []
            self.flushes = 0

        def write(self, line):
            self.writes.append(line)
            return self.result(line) if self.result else len(line)

        def flush(self):
            self.flushes += 1
            if self.error is not None:
                raise self.error

    def assert_latched(self, log, sink):
        """A recovered medium cannot make this failed transcript usable again."""
        sink.result, sink.error = None, None
        before = (list(sink.writes), sink.flushes)
        with self.assertRaises(bench.BenchError):
            log("intent", command="probe 1000")
        self.assertEqual((sink.writes, sink.flushes), before)

    def test_short_or_invalid_evidence_write_counts_fail_and_remain_fenced(self):
        counts = (lambda line: 0, lambda line: len(line) - 1,
                  lambda line: len(line) + 1, lambda line: -1,
                  lambda line: None, lambda line: True,
                  lambda line: float(len(line)), lambda line: str(len(line)))
        for count in counts:
            with self.subTest(count=count):
                sink = self.Sink(result=count)
                log = bench.EvidenceLog(file=sink)
                with self.assertRaises(bench.BenchError):
                    log("intent", command="probe 1000")
                self.assert_latched(log, sink)

    def test_evidence_flush_failure_or_interruption_remains_fenced(self):
        for error in (OSError("evidence flush failed"), KeyboardInterrupt("flush interrupted")):
            with self.subTest(error=type(error).__name__):
                sink = self.Sink(error=error)
                log = bench.EvidenceLog(file=sink)
                with self.assertRaises(type(error)) as caught:
                    log("intent", command="probe 1000")
                self.assertIs(caught.exception, error)
                self.assert_latched(log, sink)

    def test_short_intent_evidence_prevents_console_handoff(self):
        clock, port = Clock(), CampaignPort()
        console = bench.Console(port, clock=clock, sleeper=clock.sleep,
                                first_id=1, host_epoch="short-intent-evidence")
        console.identify(expected_version="0.1.0")
        port.handler = lambda *_: b""
        sink = self.Sink(result=lambda line: len(line) - 1)
        log = bench.EvidenceLog(file=sink)
        console.emit = log
        before = list(port.writes)
        with self.assertRaises(bench.BenchError):
            console.begin(MOVE, timeout_s=0.025)
        self.assertEqual(port.writes, before)
        self.assertFalse(console.synchronized)
        self.assertEqual(console.pending, {})
        self.assert_latched(log, sink)
        with self.assertRaises(bench.BenchError):
            console.begin(MOVE, timeout_s=0.025)
        self.assertEqual(port.writes, before)

    def test_short_received_evidence_keeps_unknown_and_prevents_cleanup(self):
        clock, port = Clock(), CampaignPort()
        console = bench.Console(port, clock=clock, sleeper=clock.sleep,
                                first_id=1, host_epoch="short-received-evidence")
        console.identify(expected_version="0.1.0")
        port.handler = lambda identity, command: (reply(identity, "release")
            if command.startswith("release ") else successful_move(identity))
        sink = self.Sink(result=lambda line: 0 if '"event":"line"' in line else len(line))
        log = bench.EvidenceLog(file=sink)
        console.emit = log
        report = {}
        with self.assertRaises(bench.BenchError):
            bench.checked(console, MOVE, record=report, timeout_s=0.025)
        self.assertEqual(report["primary_failure"]["classification"], "unknown_after_handoff")
        self.assertTrue(report["primary_failure"]["transmitted"])
        self.assertFalse(console.synchronized)
        before = list(port.writes)
        self.assertFalse(bench.cleanup_stop_attempts(console, report, stop="normal", timeout_s=0.025))
        self.assertEqual(port.writes, before)
        self.assertEqual(sum(b" move " in wire for wire in port.writes), 1)
        self.assertFalse(any(b" stop " in wire or b" recover" in wire for wire in port.writes))
        self.assert_latched(log, sink)


class CampaignSessionAdversarial(unittest.TestCase):
    """Session ownership and durable failures use an injected in-memory port only."""
    def harness(self, prefix, run, *, opener_error=None, close_error=None):
        import bench_session
        clock, port = Clock(), CampaignPort()
        opened, closed = [], []
        selected = [None]
        def respond(identity, command):
            if command == "useaddr 7":
                selected[0] = 7
                return reply(identity, "useaddr")
            if command == "config":
                return reply(identity, "config", interface=0, bitrate_bits_per_second=500000,
                             selected_node=selected[0], bus_traffic=False)
            if command.startswith("probe"):
                return (admitted(identity, "probe", 1) + value(identity) + terminal(identity))
            if command.startswith("release "):
                return reply(identity, "release")
            if command in ("status", "health", "stats", "drv", "memory"):
                return reply(identity, command, bus_traffic=False)
            return CampaignPort.handshake(identity, command)
        port.handler = respond
        @contextmanager
        def opener(port_name, baud, timeout_s):
            opened.append((port_name, baud, timeout_s))
            if opener_error:
                raise opener_error
            try:
                yield port
            finally:
                closed.append(True)
                port.is_open = False
                if close_error:
                    raise close_error
        def factory(stream, **kwargs):
            kwargs.update(clock=clock, sleeper=clock.sleep, first_id=1,
                          host_epoch="session-adversarial")
            return bench.Console(stream, **kwargs)
        kwargs = dict(port="supplied-fake", baud=115200, out=prefix,
                      scenario="independent-failure-vector", inputs={"intent": "host test only"},
                      run=run, profile="lichuan_c", node=7, can_bitrate=500000,
                      expected_version="0.1.0", timeout_s=0.025, startup_s=0.002,
                      port_opener=opener, console_factory=factory, clock=clock,
                      evidence_kind="host-tested")
        return lambda: bench_session.run_recorded(**kwargs), port, opened, closed

    def test_serial_open_failure_is_retained_without_scenario_or_traffic(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / "open-failure"
            execute, port, opened, _ = self.harness(
                prefix, lambda *_: self.fail("scenario ran after serial-open failure"),
                opener_error=OSError("explicit fake open failure"))
            with self.assertRaises(OSError):
                execute()
            self.assertEqual(len(opened), 1)
            self.assertEqual(port.writes, [])
            summary = json.loads(prefix.with_suffix(".json").read_text(encoding="ascii"))
            self.assertFalse(summary["ok"])
            self.assertEqual(summary["verdict"], "failed")
            self.assertIn("explicit fake open failure", summary["primary_error"])
            self.assertTrue(prefix.with_suffix(".jsonl").is_file())
            self.assertTrue(prefix.with_suffix(".zip").is_file())

    def test_each_artifact_collision_preserves_prior_bytes_and_never_opens(self):
        for suffix in (".json", ".jsonl", ".zip"):
            with self.subTest(suffix=suffix), tempfile.TemporaryDirectory() as directory:
                prefix = Path(directory) / "retained"
                occupied = prefix.with_suffix(suffix)
                occupied.write_bytes(b"prior unsuccessful evidence\x00\xff")
                execute, port, opened, _ = self.harness(
                    prefix, lambda *_: self.fail("colliding session started"))
                with self.assertRaises((OSError, bench.BenchError, ValueError)):
                    execute()
                self.assertEqual(occupied.read_bytes(), b"prior unsuccessful evidence\x00\xff")
                self.assertEqual(opened, [])
                self.assertEqual(port.writes, [])

    def test_scenario_failure_is_original_exception_after_artifacts_and_close(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / "scenario-failure"
            original = bench.BenchError("original scenario failure")
            def fail(_console, report):
                report["primary_error"] = str(original)
                raise original
            execute, port, _, closed = self.harness(prefix, fail)
            with self.assertRaises(bench.BenchError) as caught:
                execute()
            self.assertIs(caught.exception, original)
            self.assertEqual(closed, [True])
            summary = json.loads(prefix.with_suffix(".json").read_text(encoding="ascii"))
            self.assertFalse(summary["ok"])
            self.assertEqual(summary["primary_error"], "original scenario failure")
            self.assertEqual(summary["verdict"], "failed")
            self.assertFalse(any(b" stop " in wire or b" recover" in wire for wire in port.writes))
            active = [wire for wire in port.writes if b" probe" in wire or b" read " in wire]
            self.assertEqual(len(active), 1)
            self.assertIn(b" probe", active[0])
            with zipfile.ZipFile(prefix.with_suffix(".zip")) as archive:
                self.assertEqual(sorted(archive.namelist()),
                                 ["manifest.json", "summary.json", "transcript.jsonl"])
                self.assertEqual(archive.read("summary.json"), prefix.with_suffix(".json").read_bytes())
                self.assertTrue(all(item.date_time == (1980, 1, 1, 0, 0, 0)
                                    for item in archive.infolist()))

    def test_interrupted_session_persists_nonpass_and_closes_without_session_cleanup(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / "interrupted"
            interruption = KeyboardInterrupt("operator interrupted test callback")
            def interrupt(_console, _report):
                raise interruption
            execute, port, _, closed = self.harness(prefix, interrupt)
            with self.assertRaises(KeyboardInterrupt) as caught:
                execute()
            self.assertIs(caught.exception, interruption)
            self.assertEqual(closed, [True])
            summary = json.loads(prefix.with_suffix(".json").read_text(encoding="ascii"))
            self.assertFalse(summary["ok"])
            self.assertEqual(summary["verdict"], "interrupted")
            self.assertTrue(summary["primary_error"])
            self.assertFalse(any(b" stop " in wire or b" recover" in wire for wire in port.writes))

    def test_port_close_failure_cannot_replace_original_scenario_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / "two-failures"
            original = bench.BenchError("original uncertain motion failure")
            def fail(_console, report):
                report["primary_error"] = str(original)
                raise original
            execute, _, _, closed = self.harness(
                prefix, fail, close_error=OSError("separate close failure"))
            with self.assertRaises(bench.BenchError) as caught:
                execute()
            self.assertIs(caught.exception, original)
            self.assertEqual(closed, [True])
            summary = json.loads(prefix.with_suffix(".json").read_text(encoding="ascii"))
            self.assertEqual(summary["primary_error"], str(original))
            self.assertIn("separate close failure", summary["owner_error"])

    def test_package_finalization_failure_cannot_leave_a_passing_archive(self):
        import bench_session
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / "package-finalization-failure"
            def complete(_console, report):
                report["workload_verified"] = True
            execute, _, _, closed = self.harness(prefix, complete)
            original_packager = bench_session.package_report
            def fail_after_writing(*streams):
                original_packager(*streams)
                raise OSError("package final flush failed")
            with mock.patch.object(bench_session, "package_report", side_effect=fail_after_writing):
                with self.assertRaisesRegex(OSError, "package final flush failed"):
                    execute()
            self.assertEqual(closed, [True])
            summary = json.loads(prefix.with_suffix(".json").read_text(encoding="ascii"))
            self.assertFalse(summary["ok"])
            self.assertEqual(summary["verdict"], "failed")
            self.assertIn("package final flush failed", summary["artifact_error"])
            # An unavailable/partial ZIP is honest; a complete report must never
            # advertise success after packaging failed and its summary changed.
            archive_path = prefix.with_suffix(".zip")
            if zipfile.is_zipfile(archive_path):
                with zipfile.ZipFile(archive_path) as archive:
                    packaged = json.loads(archive.read("summary.json"))
                    self.assertFalse(packaged["ok"])

    def test_artifact_close_failure_does_not_replace_original_scenario_exception(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / "artifact-close-failure"
            original = bench.BenchError("original admitted operation failure")
            def fail(_console, _report):
                raise original
            execute, _, _, _ = self.harness(prefix, fail)
            original_open = Path.open
            class CloseFailure:
                def __init__(self, stream):
                    self.stream = stream

                def __getattr__(self, name):
                    return getattr(self.stream, name)

                def __enter__(self):
                    return self

                def __exit__(self, *_exception):
                    self.close()

                def close(self):
                    self.stream.close()
                    raise OSError("summary owner close failed")
            def failing_open(path, mode="r", *args, **kwargs):
                stream = original_open(path, mode, *args, **kwargs)
                return (CloseFailure(stream) if path == prefix.with_suffix(".json") and mode == "x+b"
                        else stream)
            with mock.patch.object(Path, "open", new=failing_open):
                with self.assertRaises(bench.BenchError) as caught:
                    execute()
            self.assertIs(caught.exception, original)
            summary = json.loads(prefix.with_suffix(".json").read_text(encoding="ascii"))
            self.assertFalse(summary["ok"])
            self.assertEqual(summary["primary_error"], str(original))
            self.assertIn("summary owner close failed", summary["artifact_close_error"])


if __name__ == "__main__":
    unittest.main()
