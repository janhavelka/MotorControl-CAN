#!/usr/bin/env python3
"""Independent hostile-stream tests for the finite CAN console client; no serial I/O."""

import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from bench_probe import BenchError, Console  # noqa: E402


def record(request_id, record_type, command, **fields):
    """Use the real ProbeConsole envelope, preserving decimal strings."""
    body = dict(schema="motorcontrolcan.console/1", protocol=3, profile="lichuan_c",
                type=record_type, id=request_id, command=command)
    body.update(fields)
    return (json.dumps(body, separators=(",", ":")) + "\n").encode("ascii")


def reply(request_id, command, **fields):
    return record(request_id, "reply", command, **dict(ok=True, status="OK", **fields))


def admission(identity, operation=1):
    return reply(identity, "probe", result="accepted", operation_id=operation)


def failed_terminal(identity, operation=1, command_id=None):
    return record(identity, "terminal", "probe", ok=False, status="PROTOCOL_ERROR",
                  detail=0, outcome="DEADLINE", uncertain=True,
                  command_id=identity if command_id is None else command_id,
                  operation_id=operation, completed_us="9007199254740993", abort_code=None)


def successful_probe_result(identity, operation=1):
    """Checked value and terminal only; the caller supplies the admission receipt."""
    return (record(identity, "value", "probe", operation_id=operation, index=0x1000,
                   subindex=0, width=4, raw="42", native_signed=None, unit="raw-code",
                   current=True, interpretation=None, earliest_us="1", latest_us="2") +
            record(identity, "terminal", "probe", command_id=identity, operation_id=operation,
                   ok=True, status="OK", detail=0, outcome="SUCCESS", uncertain=False,
                   completed_us="3", abort_code=None))


class Clock:
    def __init__(self):
        self.now = 0.0

    def __call__(self):
        return self.now

    def sleep(self, seconds):
        self.now += seconds


class Port:
    """Replies become readable only after an actual complete host write."""
    def __init__(self):
        self.buffer = bytearray()
        self.writes = []
        self.handler = self.handshake
        self.read_limit = 256
        self.read_error = None
        self.short_write = False
        self.is_open = True

    def read(self, count):
        if self.read_error is not None:
            raise self.read_error
        count = min(count, self.read_limit)
        data = bytes(self.buffer[:count])
        del self.buffer[:count]
        return data

    def write(self, data):
        self.writes.append(bytes(data))
        if self.short_write:
            return len(data) - 1
        prefix, command = data.decode("ascii").rstrip("\n").split(" ", 1)
        response = self.handler(int(prefix[1:]), command)
        self.buffer.extend(response or b"")
        return len(data)

    @staticmethod
    def handshake(identity, command):
        if command == "version":
            return reply(identity, "version", product="MotorControl-CAN", version="0.1.0",
                         operation_capacity=2)
        if command == "profile list":
            return reply(identity, "profile", profiles=[dict(name="lichuan_c", protocol="canopen",
                         model_match=None, firmware_match=None, hardware_qualified=False)],
                         motor_kind=None, feedback=None, supported_modes=None, probe=True,
                         identity=True, selected_protocol=None)
        if command == "caps":
            rows = []
            for family in ("help", "version", "profile", "caps", "config", "status", "health",
                           "stats", "drv", "memory", "result", "release", "useaddr", "host",
                           "probe", "read"):
                asynchronous = family in ("probe", "read")
                effect = "device_read" if asynchronous else "host_configuration" if family in (
                    "release", "useaddr", "host", "stats") else "cache"
                rows.append(record(identity, "capability", family, implemented=True,
                                   owner=41 if asynchronous or family in ("useaddr", "host") else 40,
                                   effect=effect, completion="asynchronous" if asynchronous else "reply",
                                   reason="compiled test inventory"))
            rows.append(reply(identity, "caps", result="complete", probe=True, identity=True,
                              non_changing=False, can_match_exact_model=False, can_match_firmware=False,
                              model_match=None, firmware_match=None, endpoint_range=[1, 127],
                              motor_kind=None, feedback=None, hardware_qualified=False))
            return b"".join(rows)
        if command == "config":
            return reply(identity, "config", interface=0, bitrate_bits_per_second=500000,
                         selected_node=None, bus_traffic=False)
        raise AssertionError("unexpected handshake request: " + command)


class AdversarialConsoleTest(unittest.TestCase):
    def client(self):
        clock, port = Clock(), Port()
        console = Console(port, clock=clock, sleeper=clock.sleep, first_id=1,
                          host_epoch="independent-test")
        console.identify(expected_version="0.1.0")
        self.assertTrue(console.identified)
        self.assertIsNone(console.firmware_epoch)
        return console, port, clock

    def assert_poisoned_without_replay(self, console, port):
        self.assertFalse(console.synchronized)
        writes = list(port.writes)
        with self.assertRaises(BenchError):
            console.command("status", timeout_s=0.02)
        self.assertEqual(port.writes, writes)

    def test_terminal_without_admission_is_not_acceptance(self):
        console, port, _ = self.client()
        port.handler = lambda identity, _command: failed_terminal(identity)
        with self.assertRaises(BenchError):
            console.command("probe 10", timeout_s=0.02)
        self.assert_poisoned_without_replay(console, port)

    def test_wrong_original_request_or_operation_cannot_complete(self):
        for changes in (dict(operation=2), dict(command_id=1)):
            with self.subTest(changes=changes):
                console, port, _ = self.client()
                port.handler = lambda identity, _command: admission(identity)
                handle = console.begin("probe 10", timeout_s=0.02)
                port.buffer.extend(failed_terminal(handle.id, **changes))
                with self.assertRaises(BenchError):
                    console.wait(handle)
                self.assertIsNone(handle.terminal)
                self.assert_poisoned_without_replay(console, port)

    def test_lost_terminal_keeps_admission_and_never_replays(self):
        console, port, clock = self.client()
        port.handler = lambda identity, _command: admission(identity)
        handle = console.begin("probe 10", timeout_s=0.02)
        with self.assertRaises(BenchError):
            console.wait(handle)
        self.assertTrue(handle.accepted)
        self.assertIsNone(handle.terminal)
        self.assertIsNotNone(handle.interruption)
        self.assertLess(clock.now, 1.0)
        self.assertEqual(sum(b" probe " in write for write in port.writes), 1)
        self.assert_poisoned_without_replay(console, port)

    def test_lost_admission_is_an_unknown_handoff_not_a_refusal(self):
        console, port, _ = self.client()
        port.handler = lambda _identity, _command: b""
        with self.assertRaises(BenchError):
            console.command("probe 10", timeout_s=0.02)
        interrupted = [handle for handle in console.pending.values()
                       if handle.family == "probe"]
        self.assertEqual(len(interrupted), 1)
        handle = interrupted[0]
        self.assertTrue(handle.transmitted)
        self.assertFalse(handle.accepted)
        self.assertIsNone(handle.terminal)
        self.assertIsNotNone(handle.interruption)
        self.assert_poisoned_without_replay(console, port)

    def test_operation_id_cannot_be_reused_after_release(self):
        console, port, _ = self.client()
        port.handler = lambda identity, _command: admission(identity)
        handle = console.begin("probe 10", timeout_s=0.02)
        port.buffer.extend(failed_terminal(handle.id))
        terminal = console.wait(handle)
        self.assertEqual(terminal["completed_us"], "9007199254740993")
        port.handler = lambda identity, _command: reply(identity, "release")
        console.release(handle, timeout_s=0.02)
        self.assertTrue(handle.released)
        port.handler = lambda identity, _command: admission(identity)
        with self.assertRaises(BenchError):
            console.command("probe 10", timeout_s=0.02)
        self.assert_poisoned_without_replay(console, port)

    def test_duplicate_admission_is_framing_failure(self):
        console, port, _ = self.client()
        port.handler = lambda identity, _command: admission(identity) * 2
        with self.assertRaises(BenchError):
            console.command("probe 10", timeout_s=0.02)
        self.assert_poisoned_without_replay(console, port)

    def test_duplicate_or_incomplete_suffix_cannot_hide_after_terminal(self):
        for suffix in (b"duplicate", b"partial"):
            with self.subTest(suffix=suffix):
                console, port, _ = self.client()
                port.handler = lambda identity, _command: admission(identity)
                handle = console.begin("probe 10", timeout_s=0.02)
                terminal = failed_terminal(handle.id)
                port.buffer.extend(terminal + (terminal if suffix == b"duplicate" else b'{"schema":'))
                with self.assertRaises(BenchError):
                    console.wait(handle)
                self.assert_poisoned_without_replay(console, port)
                self.assertIsNotNone(handle.terminal)
                summary = console.summary(handle)
                self.assertTrue(summary["terminal_known"])
                self.assertEqual(summary["stream_integrity"], "lost")

    def test_terminal_at_deadline_is_too_late(self):
        console, port, clock = self.client()
        port.handler = lambda identity, _command: admission(identity)
        handle = console.begin("probe 10", timeout_s=0.02)
        port.buffer.extend(failed_terminal(handle.id))
        clock.now = handle.deadline
        with self.assertRaises(BenchError):
            console.wait(handle)
        self.assertIsNone(handle.terminal)
        self.assert_poisoned_without_replay(console, port)

    def test_disconnect_after_admission_keeps_unknown(self):
        console, port, _ = self.client()
        port.handler = lambda identity, _command: admission(identity)
        handle = console.begin("probe 10", timeout_s=0.02)
        console.pump()
        self.assertTrue(handle.accepted)
        port.read_error = OSError("USB disconnected")
        with self.assertRaises(BenchError):
            console.wait(handle)
        self.assertTrue(handle.accepted)
        self.assertIsNone(handle.terminal)
        self.assertIsNotNone(handle.interruption)
        self.assert_poisoned_without_replay(console, port)

    def test_short_write_cannot_be_retried_as_pretransmission_failure(self):
        console, port, _ = self.client()
        port.short_write = True
        with self.assertRaises(BenchError):
            console.command("probe 10", timeout_s=0.02)
        self.assertEqual(sum(b" probe " in write for write in port.writes), 1)
        self.assert_poisoned_without_replay(console, port)

    def test_any_post_handshake_banner_invalidates_connection(self):
        console, port, _ = self.client()
        port.buffer.extend(b"ESP-ROM:esp32s2-rc4-20191025\r\n")
        with self.assertRaises(BenchError):
            console.pump()
        self.assert_poisoned_without_replay(console, port)

    def test_split_utf8_startup_line_does_not_require_serial(self):
        clock, port = Clock(), Port()
        port.read_limit = 1
        port.buffer.extend("startup: \u017elu\u0165ou\u010dk\u00fd\n".encode("utf-8"))
        console = Console(port, clock=clock, sleeper=clock.sleep, first_id=1)
        console.drain_startup(0.2)
        self.assertTrue(console.synchronized)
        self.assertEqual(port.writes, [])

    def test_old_structured_startup_reply_is_not_a_handshake(self):
        clock, port = Clock(), Port()
        port.buffer.extend(Port.handshake(1, "version"))
        console = Console(port, clock=clock, sleeper=clock.sleep, first_id=1)
        with self.assertRaises(BenchError):
            console.drain_startup(0.02)
        self.assertEqual(port.writes, [])
        self.assertFalse(console.synchronized)

    def test_handshake_rejects_wrong_identity_and_boolean_protocol(self):
        for field, value in (("version", "0.0.1"), ("product", "MotorControl-RS"),
                             ("protocol", True), ("profile", "ess_rs"),
                             ("operation_capacity", True)):
            with self.subTest(field=field):
                clock, port = Clock(), Port()
                def altered(identity, command):
                    data = Port.handshake(identity, command)
                    if command == "version":
                        item = json.loads(data)
                        item[field] = value
                        return (json.dumps(item) + "\n").encode("ascii")
                    return data
                port.handler = altered
                console = Console(port, clock=clock, sleeper=clock.sleep, first_id=1)
                with self.assertRaises(BenchError):
                    console.identify(expected_version="0.1.0")
                self.assertFalse(console.identified)
                self.assertFalse(console.synchronized)
                self.assertTrue(all(b" version\n" in write for write in port.writes))

    def test_malformed_terminal_types_never_publish_success(self):
        for field, value in (("uncertain", 0), ("command_id", True),
                             ("operation_id", True), ("completed_us", 9007199254740993),
                             ("ok", 0), ("abort_code", -1)):
            with self.subTest(field=field):
                console, port, _ = self.client()
                port.handler = lambda identity, _command: admission(identity)
                handle = console.begin("probe 10", timeout_s=0.02)
                item = json.loads(failed_terminal(handle.id))
                item[field] = value
                port.buffer.extend((json.dumps(item) + "\n").encode("ascii"))
                with self.assertRaises(BenchError):
                    console.wait(handle)
                self.assertIsNone(handle.terminal)
                self.assert_poisoned_without_replay(console, port)

    def test_trace_flood_has_byte_budget_even_without_clock_progress(self):
        console, port, _ = self.client()
        port.handler = lambda identity, _command: admission(identity)
        handle = console.begin("probe 10", timeout_s=0.02)
        trace = record(0, "trace", "debug", kind=0, can_id=0, dlc=0, accepted=False,
                       processed_us="1", extended=False, rtr=False, payload="", decoded=False)
        original_read = port.read
        reads = 0
        def flood(count):
            nonlocal reads
            reads += 1
            # The fail-safe makes a missing client budget fail finitely in the test too.
            if reads > 10000:
                raise AssertionError("client has no finite trace byte/record budget")
            if not port.buffer:
                port.buffer.extend(trace)
            return original_read(count)
        port.read = flood
        with self.assertRaises(BenchError):
            console.wait(handle)
        self.assertLess(reads, 10000)
        self.assertIsNone(handle.terminal)
        self.assert_poisoned_without_replay(console, port)

    def test_async_read_cannot_succeed_without_admission_and_terminal(self):
        console, port, _ = self.client()
        port.handler = lambda identity, _command: reply(identity, "probe")
        with self.assertRaises(BenchError):
            console.command("probe 10", timeout_s=0.02)
        self.assert_poisoned_without_replay(console, port)

    def test_successful_probe_requires_its_object_value(self):
        for wrong_object in (False, True):
            with self.subTest(wrong_object=wrong_object):
                console, port, _ = self.client()
                def answer(identity, _command):
                    terminal = json.loads(failed_terminal(identity))
                    terminal.update(ok=True, status="OK", outcome="SUCCESS", uncertain=False)
                    value = record(identity, "value", "probe", operation_id=1, index=0x1001,
                                   subindex=0, width=4, raw="42", native_signed=None,
                                   unit="raw-code", current=True, interpretation=None,
                                   earliest_us="1", latest_us="2") if wrong_object else b""
                    return admission(identity) + value + (json.dumps(terminal) + "\n").encode("ascii")
                port.handler = answer
                with self.assertRaises(BenchError):
                    console.command("probe 10", timeout_s=0.02)
                self.assert_poisoned_without_replay(console, port)

    def test_cached_command_cannot_admit_native_work(self):
        console, port, _ = self.client()
        def answer(identity, _command):
            terminal = json.loads(failed_terminal(identity))
            terminal["command"] = "status"
            return (reply(identity, "status", result="accepted", operation_id=1) +
                    (json.dumps(terminal) + "\n").encode("ascii"))
        port.handler = answer
        with self.assertRaises(BenchError):
            console.command("status", timeout_s=0.02)
        self.assert_poisoned_without_replay(console, port)

    def test_result_lookup_must_match_retained_operation_identity_and_evidence(self):
        for changes in (dict(operation_id=2), dict(command_id=1),
                        dict(uncertain=False), dict(completed_us="1")):
            with self.subTest(changes=changes):
                console, port, _ = self.client()
                port.handler = lambda identity, _command: admission(identity)
                handle = console.begin("probe 10", timeout_s=0.02)
                port.buffer.extend(failed_terminal(handle.id))
                original = console.wait(handle)
                def lookup(identity, _command):
                    item = dict(original, id=identity, type="reply", command="result")
                    item.update(changes)
                    return (json.dumps(item) + "\n").encode("ascii")
                port.handler = lookup
                with self.assertRaises(BenchError):
                    console.command("result 1", timeout_s=0.02)
                self.assertEqual(handle.terminal, original)
                self.assert_poisoned_without_replay(console, port)

    def test_result_lookup_cannot_regress_completed_work_to_pending(self):
        console, port, _ = self.client()
        port.handler = lambda identity, _command: admission(identity)
        handle = console.begin("probe 10", timeout_s=0.02)
        port.buffer.extend(failed_terminal(handle.id))
        console.wait(handle)
        port.handler = lambda identity, _command: reply(identity, "result", result="pending",
                                                        operation_id=1, command_id=handle.id)
        with self.assertRaises(BenchError):
            console.command("result 1", timeout_s=0.02)
        self.assert_poisoned_without_replay(console, port)

    def test_bad_record_shapes_raise_protocol_error_and_poison(self):
        for field, value in (("status", []), ("status", {}),
                             ("extra", [[[[[[[[[[[[[[[[[[0]]]]]]]]]]]]]]]]]])):
            with self.subTest(field=field, value=value):
                console, port, _ = self.client()
                def malformed(identity, _command):
                    item = json.loads(reply(identity, "status"))
                    item[field] = value
                    return (json.dumps(item) + "\n").encode("ascii")
                port.handler = malformed
                with self.assertRaises(BenchError):
                    console.command("status", timeout_s=0.02)
                self.assert_poisoned_without_replay(console, port)

    def test_nonobject_profile_entry_poisoned_instead_of_attribute_error(self):
        clock, port = Clock(), Port()
        def malformed(identity, command):
            data = Port.handshake(identity, command)
            if command == "profile list":
                item = json.loads(data)
                item["profiles"] = [1]
                return (json.dumps(item) + "\n").encode("ascii")
            return data
        port.handler = malformed
        console = Console(port, clock=clock, sleeper=clock.sleep, first_id=1)
        with self.assertRaises(BenchError):
            console.identify(expected_version="0.1.0")
        self.assertFalse(console.synchronized)
        self.assertFalse(console.identified)

    def test_probe_rejects_boolean_operation_id_and_wrong_value_kind(self):
        for kind, operation in (("value", True), ("motion_value", 1)):
            with self.subTest(kind=kind, operation=operation):
                console, port, _ = self.client()
                def answer(identity, _command):
                    value = record(identity, kind, "probe", operation_id=operation, index=0x1000,
                                   subindex=0, width=4, raw="42", native_signed=None,
                                   unit="raw-code", current=True, interpretation=None,
                                   earliest_us="1", latest_us="2")
                    return admission(identity) + value + failed_terminal(identity)
                port.handler = answer
                with self.assertRaises(BenchError):
                    console.command("probe 10", timeout_s=0.02)
                self.assert_poisoned_without_replay(console, port)

    def test_capability_bad_container_and_boolean_endpoint_are_rejected(self):
        for field, value in (("effect", []), ("endpoint_range", [True, 127])):
            with self.subTest(field=field):
                clock, port = Clock(), Port()
                def malformed(identity, command):
                    data = Port.handshake(identity, command)
                    if command == "caps":
                        rows = [json.loads(line) for line in data.splitlines()]
                        rows[0 if field == "effect" else -1][field] = value
                        return b"".join((json.dumps(row) + "\n").encode("ascii") for row in rows)
                    return data
                port.handler = malformed
                console = Console(port, clock=clock, sleeper=clock.sleep, first_id=1)
                with self.assertRaises(BenchError):
                    console.identify(expected_version="0.1.0")
                self.assertFalse(console.synchronized)
                self.assertFalse(console.identified)

    def test_successful_probe_cannot_contradict_its_native_evidence(self):
        for changes in (dict(outcome="DEADLINE"), dict(abort_code=0x06020000),
                        dict(completed_us="0")):
            with self.subTest(changes=changes):
                console, port, _ = self.client()
                def answer(identity, _command):
                    terminal = json.loads(failed_terminal(identity))
                    terminal.update(ok=True, status="OK", outcome="SUCCESS", uncertain=False)
                    terminal.update(changes)
                    value = record(identity, "value", "probe", operation_id=1, index=0x1000,
                                   subindex=0, width=4, raw="42", native_signed=None,
                                   unit="raw-code", current=True, interpretation=None,
                                   earliest_us="1", latest_us="2")
                    return admission(identity) + value + (json.dumps(terminal) + "\n").encode("ascii")
                port.handler = answer
                with self.assertRaises(BenchError):
                    console.command("probe 10", timeout_s=0.02)
                self.assert_poisoned_without_replay(console, port)

    def test_already_buffered_future_id_cannot_become_a_new_admission(self):
        console, port, _ = self.client()
        port.buffer.extend(admission(console.next_id))
        writes = list(port.writes)
        port.handler = lambda _identity, _command: b""
        with self.assertRaises(BenchError):
            console.begin("probe 10", timeout_s=0.02)
        self.assertEqual(port.writes, writes)
        self.assert_poisoned_without_replay(console, port)

    def test_pending_result_summary_preserves_confirmed_admission(self):
        for known in (False, True):
            with self.subTest(original_retained_by_client=known):
                console, port, _ = self.client()
                original_id = 77
                if known:
                    port.handler = lambda identity, _: admission(identity)
                    original = console.begin("probe 10", timeout_s=0.02)
                    while not original.accepted:
                        console.pump()
                    original_id = original.id
                port.handler = lambda identity, _: reply(identity, "result", result="pending",
                    operation_id=1, command_id=original_id)
                lookup = console.begin("result 1", timeout_s=0.02)
                console.wait(lookup)
                summary = console.summary(lookup)
                self.assertTrue(summary["admission"])
                self.assertEqual(summary["operation_id"], 1)
                self.assertEqual(summary["classification"], "pending")
                self.assertFalse(summary["terminal_known"])
                self.assertIsNone(summary["uncertain"])
                self.assertEqual(summary["request_id"], lookup.id)
                self.assertEqual(summary["records"][0]["command_id"], original_id)

    def test_wait_with_explicit_release_surfaces_refusal_and_retains_terminal(self):
        console, port, _ = self.client()
        def answer(identity, command):
            if command == "probe 10":
                return admission(identity) + failed_terminal(identity)
            self.assertEqual(command, "release 1")
            return record(identity, "reply", "release", ok=False, status="ILLEGAL_VALUE",
                          error="result_not_releasable")
        port.handler = answer
        handle = console.begin("probe 10")
        with self.assertRaises(BenchError):
            console.wait(handle, release=True)
        self.assertFalse(handle.released)
        self.assertIs(console.operations[1], handle)
        self.assertEqual(handle.terminal["outcome"], "DEADLINE")
        self.assertTrue(handle.terminal["uncertain"])
        self.assertTrue(console.synchronized)
        self.assertEqual(sum(b" release " in item for item in port.writes), 1)

    def test_long_read_budget_accepts_a_terminal_after_eight_seconds(self):
        console, port, clock = self.client()
        port.handler = lambda identity, _: admission(identity)
        handle = console.begin("probe 60000", timeout_s=60)
        original_read = port.read
        sent = False

        def delayed_terminal(count):
            nonlocal sent
            if not sent and clock.now >= 10:
                sent = True
                port.buffer.extend(failed_terminal(handle.id))
            return original_read(count)

        port.read = delayed_terminal
        terminal = console.wait(handle)
        self.assertFalse(terminal["ok"])
        self.assertGreaterEqual(clock.now, 10)
        self.assertLess(clock.now, handle.deadline)
        self.assertIsNone(handle.interruption)
        self.assertTrue(console.synchronized)
        self.assertEqual(sum(b" probe " in item for item in port.writes), 1)

    def test_retained_terminal_does_not_spend_another_reads_service_budget(self):
        console, port, clock = self.client()
        port.handler = lambda identity, _: admission(identity) + successful_probe_result(identity)
        first = console.begin("probe 10", timeout_s=0.02)
        first_terminal = dict(console.wait(first))
        first_counters = (first.turns, first.input_bytes)
        port.handler = lambda identity, _: admission(identity, 2)
        second = console.begin("probe 60000", timeout_s=60)
        original_read = port.read
        sent = False

        def delayed_terminal(count):
            nonlocal sent
            if not sent and clock.now >= 10:
                sent = True
                port.buffer.extend(successful_probe_result(second.id, 2))
            return original_read(count)

        port.read = delayed_terminal
        self.assertTrue(console.wait(second)["ok"])
        self.assertGreaterEqual(clock.now, 10)
        self.assertLess(clock.now, second.deadline)
        self.assertLessEqual(second.turns, second.turn_limit)
        self.assertEqual(first.terminal, first_terminal)
        self.assertEqual((first.turns, first.input_bytes), first_counters)
        self.assertIs(console.operations[1], first)
        self.assertIs(console.operations[2], second)
        self.assertFalse(first.released)
        self.assertTrue(console.synchronized)
        self.assertEqual(sum(b" probe " in item for item in port.writes), 2)

    def test_terminal_with_partial_trailing_line_stays_bounded_with_frozen_clock(self):
        console, port, _ = self.client()
        console.sleep = lambda _: None
        partial = b'{"schema":'
        port.handler = lambda identity, _: admission(identity) + successful_probe_result(identity) + partial
        handle = console.begin("probe 10", timeout_s=0.02)
        with self.assertRaisesRegex(BenchError, "turn capacity"):
            console.wait(handle)
        self.assertEqual(handle.terminal["outcome"], "SUCCESS")
        self.assertEqual(bytes(console.buffer), partial)
        self.assertLessEqual(handle.turns, handle.turn_limit + 1)
        self.assert_poisoned_without_replay(console, port)

    def test_wait_on_already_retained_terminal_is_a_cached_read_without_io(self):
        console, port, _ = self.client()
        port.handler = lambda identity, _: admission(identity) + successful_probe_result(identity)
        handle = console.begin("probe 10", timeout_s=0.02)
        terminal = console.wait(handle)
        counters = (handle.turns, handle.input_bytes)
        port.buffer.extend(b'{"schema":')
        console.sleep = lambda _: None

        def forbidden_io(*_):
            raise AssertionError("cached terminal wait attempted stream I/O")

        port.read = port.write = forbidden_io
        self.assertIs(console.wait(handle), terminal)
        self.assertEqual((handle.turns, handle.input_bytes), counters)
        self.assertEqual(bytes(port.buffer), b'{"schema":')
        self.assertTrue(console.synchronized)

    def test_long_startup_window_is_not_cut_off_at_eight_seconds(self):
        for duration in (10, 60):
            with self.subTest(duration=duration):
                clock, port = Clock(), Port()
                console = Console(port, clock=clock, sleeper=clock.sleep, first_id=1)
                console.drain_startup(duration)
                self.assertGreaterEqual(clock.now, duration)
                self.assertTrue(console.synchronized)
                self.assertFalse(port.writes)

    def test_deadline_reached_during_preflight_never_hands_off_new_request(self):
        for seam in ("intent_callback", "empty_read"):
            with self.subTest(seam=seam):
                console, port, clock = self.client()
                writes = list(port.writes)
                port.handler = lambda *_: b""
                if seam == "intent_callback":
                    def emit(event, **fields):
                        if event == "intent":
                            clock.now = fields["deadline_s"]
                    console.emit = emit
                else:
                    def elapsed_empty_read(_count):
                        clock.now += 0.02
                        return b""
                    port.read = elapsed_empty_read
                with self.assertRaises(BenchError):
                    console.begin("probe 10", timeout_s=0.02)
                self.assertEqual(port.writes, writes)
                self.assert_poisoned_without_replay(console, port)

    def test_preflight_collects_split_reply_for_existing_request(self):
        console, port, clock = self.client()
        delayed = []
        def answer(identity, command):
            if command.startswith("probe "):
                response = admission(identity)
                delayed.append(response[60:])
                return response[:60]
            return reply(identity, "status")
        port.handler = answer
        original_read = port.read
        def fragmented(count):
            if not port.buffer and delayed and clock.now >= 0.002:
                port.buffer.extend(delayed.pop())
            return original_read(count)
        port.read = fragmented
        first = console.begin("probe 10", timeout_s=0.02)
        second = console.begin("status", timeout_s=0.02)
        self.assertTrue(first.accepted)
        self.assertTrue(console.wait(second)["ok"])
        port.buffer.extend(failed_terminal(first.id))
        self.assertFalse(console.wait(first)["ok"])
        self.assertTrue(console.synchronized)
        self.assertEqual(sum(b" probe " in write for write in port.writes), 1)


if __name__ == "__main__":
    unittest.main()
