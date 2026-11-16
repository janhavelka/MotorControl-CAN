#!/usr/bin/env python3
"""Host-only hostile streams for per-axis console correlations; no CAN or serial I/O."""
from pathlib import Path
import json
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from bench_probe import BenchError, Console, campaign_form
from bench_console_adversarial_test import Clock, reply, record
from bench_campaign_adversarial_test import CampaignPort


class MultiAxisClient(unittest.TestCase):
    def client(self, capacity=4):
        clock, port = Clock(), CampaignPort()
        def handshake(identity, command):
            if command == "version":
                return reply(identity, "version", product="MotorControl-CAN", version="0.1.0",
                             operation_capacity=capacity)
            return port.handshake(identity, command)
        port.handler = handshake
        console = Console(port, clock=clock, sleeper=clock.sleep, first_id=1)
        console.identify(expected_version="0.1.0")
        self.assertEqual(console.operation_capacity, capacity)
        return console, port

    @staticmethod
    def drain(console, port):
        while port.buffer:
            console.pump()

    def test_advertised_capacity_allows_two_axes_and_separate_stop_reserves(self):
        console, port = self.client()
        operation = 0
        def admit(identity, command):
            nonlocal operation
            operation += 1
            axis = int(command.split()[1]) if command.startswith("stop ") else 100 + operation
            return reply(identity, command.split()[0], result="accepted", operation_id=operation, axis_id=axis)
        port.handler = admit
        for _ in range(4):
            console.begin("move absolute 16000 steps native 30 configured setup write")
            self.drain(console, port)
        writes = len(port.writes)
        with self.assertRaises(BenchError):
            console.begin("move absolute 16000 steps native 30 configured setup write")
        self.assertEqual(len(port.writes), writes)
        first = console.begin("stop 101")
        self.drain(console, port)
        second = console.begin("stop 102 fast")
        self.drain(console, port)
        self.assertEqual((first.axis_id, second.axis_id), (101, 102))
        writes = len(port.writes)
        with self.assertRaises(BenchError):
            console.begin("stop 101 fast")
        self.assertEqual(len(port.writes), writes)

    def test_explicit_stop_wrong_axis_receipt_or_terminal_poison_without_replay(self):
        for wrong_receipt in (True, False):
            console, port = self.client()
            def respond(identity, _command):
                accepted = reply(identity, "stop", result="accepted", operation_id=1,
                                 axis_id=102 if wrong_receipt else 101)
                terminal = record(identity, "terminal", "stop", ok=False, status="PROTOCOL_ERROR",
                                  command_id=identity, operation_id=1, completed_us="10", abort_code=None,
                                  outcome="DEADLINE", uncertain=True, axis_id=102)
                return accepted + terminal
            port.handler = respond
            with self.assertRaises(BenchError):
                console.command("stop 101")
            writes = list(port.writes)
            self.assertFalse(console.synchronized)
            with self.assertRaises(BenchError):
                console.command("stop 101")
            self.assertEqual(port.writes, writes)

    def group_rows(self, identity, *, duplicate=False, missing=False, refused=False, claim_success=False):
        rows = [reply(identity, "stop", result="accepted", operation_id=1, axis_count=2)]
        for axis in (101, 101 if duplicate else 102):
            if missing and axis == 102:
                continue
            accepted = not (refused and axis == 102)
            rows.append(record(identity, "group_stop_axis", "stop", operation_id=1, axis_id=axis,
                               accepted=accepted, status="OK" if accepted else "INVALID_CONFIG", detail=0,
                               outcome="OBSERVED" if accepted else "REFUSED", uncertain=not accepted,
                               native_operation=73 if accepted else 0))
        success = not refused or claim_success
        rows.append(record(identity, "terminal", "stop", ok=success,
                           status="OK" if success else "INVALID_CONFIG", detail=0,
                           command_id=identity, operation_id=1, completed_us="10", abort_code=None,
                           outcome="OBSERVED" if success else "UNCONFIRMED", uncertain=not success, axis_count=2))
        return b"".join(rows)

    def test_group_requires_every_exact_per_axis_result(self):
        for mode in ({"duplicate": True}, {"missing": True}, {"refused": True, "claim_success": True}):
            with self.subTest(mode=mode):
                console, port = self.client()
                port.handler = lambda identity, _command: self.group_rows(identity, **mode)
                with self.assertRaises(BenchError):
                    console.command("stop all")
                self.assertFalse(console.synchronized)
                self.assertEqual(sum(b"stop all" in data for data in port.writes), 1)

    def test_group_refusal_remains_retained_and_uncertain(self):
        console, port = self.client()
        port.handler = lambda identity, _command: self.group_rows(identity, refused=True)
        handle = console.begin("stop all fast")
        result = console.wait(handle)
        self.assertFalse(result["ok"])
        self.assertTrue(result["uncertain"])
        self.assertEqual([row["axis_id"] for row in handle.records if row["type"] == "group_stop_axis"], [101, 102])
        self.assertIs(console.operations[1], handle)

    def test_group_admission_status_native_identity_and_outcome_are_checked(self):
        for changes in ({"native_operation": 0}, {"status": "INVALID_CONFIG"},
                        {"outcome": "invented"}, {"outcome": "DEADLINE", "uncertain": False}):
            with self.subTest(changes=changes):
                console, port = self.client()
                def respond(identity, _command):
                    rows = [json.loads(line) for line in self.group_rows(identity).splitlines()]
                    rows[1].update(changes)
                    return b"".join((json.dumps(row) + "\n").encode() for row in rows)
                port.handler = respond
                with self.assertRaises(BenchError):
                    console.command("stop all")
                self.assertFalse(console.synchronized)
        console, port = self.client()
        port.handler = lambda identity, _command: self.group_rows(identity, refused=True).replace(
            b'"accepted":false,"status":"INVALID_CONFIG"', b'"accepted":false,"status":"OK"')
        with self.assertRaises(BenchError):
            console.command("stop all")

    def test_group_keeps_uncertain_observed_member_as_failure(self):
        console, port = self.client()
        def respond(identity, _command):
            rows = [json.loads(line) for line in self.group_rows(identity).splitlines()]
            rows[1]["uncertain"] = True
            rows[-1].update(ok=False, status="INVALID_CONFIG", outcome="UNCONFIRMED", uncertain=True)
            return b"".join((json.dumps(row) + "\n").encode() for row in rows)
        port.handler = respond
        result = console.command("stop all")
        self.assertFalse(result["ok"])
        self.assertTrue(console.synchronized)

    def test_group_native_ids_can_repeat_on_distinct_axes(self):
        console, port = self.client()
        port.handler = lambda identity, _command: self.group_rows(identity)
        self.assertTrue(console.command("stop all")["ok"])

    def test_full_adapter_capacity_group_fits_record_input_and_summary_bounds(self):
        console, port = self.client(508)
        def respond(identity, _command):
            rows = [reply(identity, "stop", result="accepted", operation_id=1, axis_count=254)]
            rows.extend(record(identity, "group_stop_axis", "stop", operation_id=1, axis_id=1000 + index,
                               accepted=True, status="OK", detail=0, outcome="OBSERVED", uncertain=False,
                               native_operation=4294967295) for index in range(254))
            rows.append(record(identity, "terminal", "stop", ok=True, status="OK", detail=0,
                               command_id=identity, operation_id=1, completed_us="18446744073709551615",
                               abort_code=None, outcome="OBSERVED", uncertain=False, axis_count=254))
            return b"".join(rows)
        port.handler = respond
        handle = console.begin("stop all")
        self.assertTrue(console.wait(handle)["ok"])
        self.assertEqual(len(handle.records), 256)
        self.assertEqual(len(console.summary(handle)["records"]), 256)
        writes = len(port.writes)
        with self.assertRaises(BenchError):
            console.begin("stop all")
        self.assertEqual(len(port.writes), writes)

    def test_grammar_is_bounded_and_legacy_compatible(self):
        for text in ("stop normal", "stop fast", "stop 101", "stop 102 normal", "stop 4294967295 fast"):
            self.assertEqual(campaign_form(text), "stop")
        for text in ("stop all", "stop all fast", "stop all normal"):
            self.assertEqual(campaign_form(text), "group_stop")
        for text in ("stop 0", "stop 4294967296", "stop all extra", "stop 101 fast extra"):
            self.assertIsNone(campaign_form(text))

    def test_adapter_capacity_bound_and_invalid_advertisements(self):
        for capacity in (2, 4, 16, 64, 508):
            self.client(capacity)
        for capacity in (0, 1, 3, 510, True):
            with self.subTest(capacity=capacity), self.assertRaises(BenchError):
                self.client(capacity)

    def test_handoff_cutoff_after_evidence_callback_is_local_without_write(self):
        console, port = self.client()
        clock = console.clock
        def advance(event, **_fields):
            if event == "intent":
                clock.sleep(0.03)
        console.emit = advance
        count = len(port.writes)
        with self.assertRaisesRegex(BenchError, "handoff cutoff"):
            console.begin("stop 101", timeout_s=0.1, handoff_cutoff_s=clock() + 0.02)
        self.assertEqual(len(port.writes), count)
        self.assertTrue(console.synchronized)
        self.assertFalse(console.pending)


if __name__ == "__main__":
    unittest.main()
