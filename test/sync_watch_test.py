#!/usr/bin/env python3
"""Compact native encoding and finite single-owner host capture; no hardware I/O."""
import binascii
import io
import json
from pathlib import Path
import subprocess
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import sync_watch as watch

BINARY = sys.argv.pop(1) if len(sys.argv) > 1 and not sys.argv[1].startswith("-") else None


def compact(body):
    return body + ",%04x" % binascii.crc_hqx(body.encode(), 0xffff)


def console(identity, kind, **values):
    return json.dumps(dict(schema="motorcontrolcan.console/1", protocol=3, profile="lichuan_c",
                          id=identity, type=kind, command="sync", **values), separators=(",", ":")) + "\n"


class Clock:
    value = 0.0
    def __call__(self): return self.value
    def sleep(self, seconds): self.value += seconds


class Port:
    def __init__(self, clock):
        self.clock, self.data, self.writes = clock, bytearray(), []
        self.sequence, self.generated, self.count = 0, 0, 0
        self.identity, self.origin, self.end, self.terminal_sent = 0, 0, 0, False
        self.bad_counter = False
        self.native_failure = False
    def add(self, line): self.data.extend(line.encode())
    def generate(self):
        while self.identity and self.generated < self.count and self.clock() >= self.origin / 1e6 + self.generated / 100:
            for index in range(2):
                self.add(compact("PC %x,%x,%x,64,1c4,50c00000000001" %
                    (self.sequence, 0x181 + index, self.generated * 10000 + 300 + index * 100)) + "\n")
                self.sequence += 1
            self.generated += 1
        if self.identity and self.clock() >= self.end and not self.terminal_sent:
            self.add(console(self.identity, "terminal", ok=not self.native_failure,
                status="INVALID_CONFIG" if self.native_failure else "OK", uncertain=True,
                outcome="MISSED_DEADLINE" if self.native_failure else "LOCAL_TX",
                operation_id=1, command_id=self.identity, abort_code=None,
                local_tx_count=self.count - int(self.native_failure), requested_count=self.count,
                transmission_possible=True, drive_effect_known=False))
            self.terminal_sent = True
    def read(self, capacity):
        self.generate()
        result = bytes(self.data[:capacity])
        del self.data[:capacity]
        return result
    def write(self, data):
        self.generate() # A terminal can arrive while the host waits for a cached STATS reply.
        self.writes.append(bytes(data))
        text = data.decode().strip()
        if text == "p":
            self.add("PONG elf=" + "a" * 64 + " pdo=7 active=1 running=1 soak=0 trial=0 watch=0 capture=0\n")
        elif text == "o": self.add("CONSOLE open ready\n")
        elif text == "s":
            self.add("STATS running=1 inhibited=0 dropped=0 errors=%d txRej=0 rxLost=0 ownerGap=2000 stackFree=8000\n" %
                     int(self.bad_counter and self.terminal_sent))
        elif text.startswith("P") and text != "P0":
            self.origin = int(self.clock() * 1e6)
            self.add("PDOW admitted seconds=11\nPDOW_BEGIN n=0 t=%d seconds=11 ring=32 ids=16 compact=1 origin=%d\n" % (self.origin, self.origin))
            self.sequence = 1
        elif text == "P0":
            self.add("PDOW_STOPPING; final summaries follow\n")
            for identifier in (0x181, 0x182):
                self.add("PDOW_ID n=%d t=1000000 id=%x ext=0 count=%d first=300 last=990300 minGap=10000 maxGap=10000 meanGap=10000 sdGap=0 changes=0 dlc=7 data=50c00000000001\n" %
                         (self.sequence, identifier, self.count))
                self.sequence += 1
            self.add("PDOW_END n=%d t=1000000 reason=stop frames=%d printed=%d unprinted=0 tapDropped=0 untimed=0 sdo=0 idOverflow=0\n" %
                     (self.sequence, self.count * 2, self.count * 2))
        else:
            prefix, command = text.split(" ", 1)
            identity = int(prefix[1:])
            fields = dict(ok=True, status="OK")
            if command == "sync inspect": fields.update(armed=False, producer_conflict=False, retained=False, active=False)
            if command.startswith("sync period"):
                self.identity, self.count = identity, int(command.split()[3])
                self.end = self.clock() + self.count / 100
                fields.update(result="accepted", operation_id=1)
            if command == "sync timing":
                fields.update(count=self.count, terminals=self.count, confirmed=self.count, handoffs=self.count,
                    origin_us=str(self.origin), period_us="10000", handoff_us=["0", "990000", "10000", "10000"],
                    delay_us=["100", "1000"], invalid=False)
            row = json.loads(console(identity, "reply", **fields))
            row["command"] = command.split()[0]
            self.add(json.dumps(row) + "\n")
        return len(data)


class CompactTests(unittest.TestCase):
    def test_independent_literal_crc_and_time_vectors(self):
        row = watch.decode_compact("PC 1,181,1f4,64,1c4,01020304050607,24e8", 1000000)
        self.assertEqual((row["lo"], row["hi"], row["data"]), (1000400, 1000500, "01020304050607"))
        row = watch.decode_compact("PC 2,701,1,0,5c,05,6809", 1000000)
        self.assertTrue(row["exact"])
        self.assertEqual(row["hi"], 999999)
        row = watch.decode_compact("PC 3,181,320,64,a4,0203,2285", 1000000)
        self.assertTrue(row["reversed_bounds"])
        self.assertEqual((row["lo"], row["hi"]), (1000900, 1000800))
        # Missing a previous line cannot shift a later absolute-from-origin record.
        row = watch.decode_compact(compact("PC 20,181,ffffffffffffffff,0,1cc,01020304050607"), 0)
        self.assertEqual(row["hi"], watch.U64)

    def test_bad_crc_flags_widths_payload_and_overflows_refuse(self):
        for body in ("PC 01,181,1,0,1cc,01020304050607", "PC 1,181,1,0,1000,-",
                     "PC 1,181,1,0,24c,010203040506070809", "PC 1,181,1,1,1cc,01020304050607",
                     "PC 1,181,1,0,1c0,01020304050607", "PC 1,181,2,0,1dc,01020304050607",
                     "PC 1,181,0,2,1c4,01020304050607", "PC 1,181,ffffffffffffffff,1,1e4,01020304050607",
                     "PC 100000000,181,0,0,1c4,01020304050607", "PC 1,800,0,0,1c4,01020304050607"):
            with self.subTest(body=body), self.assertRaises(ValueError):
                watch.decode_compact(compact(body), 1)
        with self.assertRaises(ValueError):
            watch.decode_compact("PC 1,181,1f4,64,1c4,01020304050607,24e9", 1000000)

    def test_actual_native_compact_producer(self):
        self.assertIsNotNone(BINARY, "CTest supplies the real pdo_watch_test producer")
        result = subprocess.run([BINARY, "--compact-transcript"], check=True, capture_output=True, timeout=10)
        analysis = watch.Analysis()
        for line in result.stdout.decode().splitlines(): analysis.accept(line)
        self.assertEqual(analysis.frames, 3)
        self.assertEqual(analysis.invalid_time, 1)
        self.assertTrue(analysis.summary()["raw_rx_complete"])
        self.assertFalse(analysis.summary()["timing_complete"])

    def test_sequence_loss_and_source_drop_cannot_qualify(self):
        analysis = watch.Analysis()
        analysis.accept("PDOW_BEGIN n=0 t=0 seconds=1800 ring=32 ids=16 compact=1 origin=0")
        with self.assertRaisesRegex(ValueError, "sequence"):
            analysis.accept(compact("PC 2,181,1,0,1cc,01020304050607"))
        analysis = watch.Analysis()
        analysis.accept("PDOW_BEGIN n=0 t=0 seconds=1800 ring=32 ids=16 compact=1 origin=0")
        analysis.accept("PDOW_END n=1 t=1800000000 reason=duration frames=2 printed=0 unprinted=2 tapDropped=1 untimed=0 sdo=0 idOverflow=0")
        self.assertFalse(analysis.summary()["raw_rx_complete"])
        self.assertFalse(analysis.summary()["timing_complete"])
        self.assertTrue(analysis.summary()["observed_bounds_valid"])

    def test_one_finite_schedule_streams_every_byte_and_keeps_terminal_during_poll(self):
        clock = Clock()
        port, raw, analysis = Port(clock), io.BytesIO(), watch.Analysis()
        stream = watch.Stream(port, raw, analysis, clock, clock.sleep)
        result = watch.run(stream, duration_s=1, period_ms=10, expected_elf="a" * 64, poll_s=1)
        self.assertTrue(result["ok"], result)
        self.assertEqual(analysis.frames, 200)
        self.assertEqual(sum(b"sync period " in command for command in port.writes), 1)
        self.assertIn(b"PC ", raw.getvalue())
        self.assertIn(b"P0\n", port.writes)
        self.assertFalse(result["watch"]["tx_raw_per_cycle"])
        self.assertTrue(result["local_tx_complete"])
        self.assertTrue(result["terminal"]["uncertain"])
        self.assertFalse(result["firmware_effect_qualified"])
        self.assertTrue(any(b"release 1" in command for command in port.writes))
        stats = result["watch"]["ids"][str(0x181) + "s"]
        self.assertEqual((stats["min_interval_lower"], stats["max_interval_upper"]), (9900, 10100))

    def test_controller_loss_is_reported_without_retry(self):
        clock = Clock()
        port = Port(clock)
        port.bad_counter = True
        result = watch.run(watch.Stream(port, io.BytesIO(), watch.Analysis(), clock, clock.sleep),
                           duration_s=1, period_ms=10, expected_elf="a" * 64, poll_s=1)
        self.assertFalse(result["ok"])
        self.assertTrue(result["counter_errors"])
        self.assertEqual(sum(b"sync period " in command for command in port.writes), 1)

    def test_target_identifier_requires_standard_data_and_the_fixture_tpdo_layout(self):
        for malformed in ("rtr", "dlc", "extended", "mixed"):
            class MalformedPort(Port):
                target_frames = 0
                def add(self, line):
                    if line.startswith("PC "):
                        fields = line.rsplit(",", 1)[0].split(",")
                        if fields[1] == "181":
                            self.target_frames += 1
                            if malformed != "mixed" or self.target_frames == 2:
                                fields[4], fields[5] = {
                                    "rtr": ("1c6", "-"), "mixed": ("1c6", "-"),
                                    "dlc": ("184", "50c000000000"),
                                    "extended": ("1c5", "50c00000000001"),
                                }[malformed]
                                line = compact(",".join(fields)) + "\n"
                    super().add(line)
            with self.subTest(malformed=malformed):
                clock = Clock()
                port, raw = MalformedPort(clock), io.BytesIO()
                result = watch.run(watch.Stream(port, raw, watch.Analysis(), clock, clock.sleep),
                                   duration_s=1, period_ms=10, expected_elf="a" * 64, poll_s=1)
                self.assertFalse(result["ok"])
                summary = result["watch"]
                self.assertTrue(summary["raw_rx_complete"])
                self.assertEqual(summary["received_frames"], 200)
                self.assertEqual(summary["rejected_tpdo_frames"], 1 if malformed == "mixed" else 100)
                self.assertTrue(summary["coverage_errors"])
                self.assertEqual(summary["tpdos"][str(0x182)]["count"], 100)
                if malformed == "mixed":
                    self.assertEqual(summary["ids"][str(0x181) + "s"]["max_hi_gap"], 10000)
                    self.assertEqual(summary["tpdos"][str(0x181)]["count"], 99)
                    self.assertEqual(summary["tpdos"][str(0x181)]["max_hi_gap"], 20000)
                else:
                    self.assertNotIn(str(0x181), summary["tpdos"])
                self.assertEqual(sum(b"sync period " in command for command in port.writes), 1)

    def test_genuine_p54_captures_keep_their_original_coverage_classification(self):
        root = Path(__file__).resolve().parents[1] / "docs/bench/20261116-p54"
        for name, count, complete in (("37-sync-10ms-drain-fixed", 500, True),
                                      ("38-sync-10ms-30min", 180000, False),
                                      ("40-sync-100ms-60s", 600, True)):
            with self.subTest(capture=name):
                request = json.loads((root / name / "request.json").read_text())
                analysis = watch.Analysis(request["period_ms"] * 1000)
                raw = io.BytesIO()
                stream = watch.Stream(io.BytesIO((root / name / "rx.bin").read_bytes()), raw, analysis)
                while stream.read():
                    stream.lines.clear()
                summary = analysis.summary(count)
                self.assertTrue(summary["raw_rx_complete"])
                self.assertTrue(summary["timing_complete"])
                self.assertEqual(summary["rejected_tpdo_frames"], 0)
                self.assertEqual(not summary["errors"], complete)
                for identifier in (0x181, 0x182):
                    self.assertEqual(summary["tpdos"][str(identifier)]["count"],
                                     summary["ids"][str(identifier) + "s"]["count"])

    def test_checked_native_failure_keeps_result_and_records_planned_local_cleanup(self):
        clock = Clock()
        port = Port(clock)
        port.native_failure = True
        result = watch.run(watch.Stream(port, io.BytesIO(), watch.Analysis(), clock, clock.sleep),
                           duration_s=1, period_ms=10, expected_elf="a" * 64, poll_s=1)
        self.assertFalse(result["ok"])
        self.assertEqual(result["terminal"]["outcome"], "MISSED_DEADLINE")
        self.assertFalse(result["local_tx_complete"])
        self.assertFalse(result["firmware_effect_qualified"])
        self.assertIn("sync_timing", result)
        self.assertFalse(any(b"release 1" in command for command in port.writes))
        self.assertTrue(any(b"sync disarm" in command for command in port.writes))
        self.assertIn(b"P0\n", port.writes)
        self.assertEqual(sum(b"sync period " in command for command in port.writes), 1)

    def test_retained_100ms_capture_local_tx_and_stopping_receipt(self):
        # This failed host capture stopped before END. Preserve that missing
        # evidence while recognizing its real successful, remote-unknown TX.
        path = Path(__file__).resolve().parents[1] / "docs/bench/20261116-p54/29-sync-100ms/rx.bin"
        data = path.read_bytes()
        raw, source = io.BytesIO(), io.BytesIO(data)
        analysis = watch.Analysis(100000)
        stream = watch.Stream(source, raw, analysis)
        while stream.read():
            stream.lines.clear()  # Consumer drains bounded lines; analysis is streaming.
        self.assertTrue(stream.healthy)
        self.assertEqual(raw.getvalue(), data)
        self.assertTrue(analysis.stopping)
        terminal = stream.console_records[(540004, "terminal")]
        self.assertTrue(watch.local_tx_complete(terminal, 50))
        self.assertTrue(terminal["uncertain"])
        self.assertFalse(terminal["drive_effect_known"])
        self.assertEqual(analysis.ids[str(0x181) + "s"]["count"], 50)
        self.assertEqual(analysis.ids[str(0x182) + "s"]["count"], 50)
        self.assertIsNone(analysis.end)
        self.assertFalse(analysis.summary(50)["raw_rx_complete"])
        self.assertFalse(analysis.summary(50)["timing_complete"])
        self.assertTrue(analysis.summary(50)["observed_bounds_valid"])
        for updates in ({"ok": False}, {"outcome": "MISSED_DEADLINE"},
                        {"local_tx_count": 49}, {"requested_count": 51},
                        {"drive_effect_known": True}, {"transmission_possible": False},
                        {"uncertain": False}, {"abort_code": 0x08000000},
                        {"status": "INVALID_CONFIG"}, {"local_tx_count": True}):
            with self.subTest(updates=updates):
                self.assertFalse(watch.local_tx_complete(dict(terminal, **updates), 50))

    def test_stopping_control_does_not_consume_sequence_or_complete_watch(self):
        analysis = watch.Analysis()
        stopping = "PDOW_STOPPING; final summaries follow"
        with self.assertRaises(ValueError):
            analysis.accept(stopping)
        analysis.accept("PDOW_BEGIN n=0 t=0 seconds=1 ring=32 ids=16 compact=1 origin=0")
        analysis.accept(stopping)
        self.assertEqual(analysis.next_sequence, 1)
        self.assertFalse(analysis.summary()["raw_rx_complete"])
        for line in (stopping, stopping + " n=1", "PDOW_STOPPING", "PDOW_UNKNOWN n=1 t=0"):
            with self.subTest(line=line), self.assertRaises(ValueError):
                analysis.accept(line)
        # Rejected malformed lines may consume a sequence; use a fresh state.
        analysis = watch.Analysis()
        analysis.accept("PDOW_BEGIN n=0 t=0 seconds=1 ring=32 ids=16 compact=1 origin=0")
        analysis.accept(stopping)
        analysis.accept("PDOW_END n=1 t=1 reason=stop frames=0 printed=0 unprinted=0 tapDropped=0 untimed=0 sdo=0 idOverflow=0")
        self.assertTrue(analysis.summary()["raw_rx_complete"])
        with self.assertRaises(ValueError):
            analysis.accept(stopping)

    def test_damaged_input_fences_all_later_handoffs(self):
        clock, raw = Clock(), io.BytesIO()
        port = Port(clock)
        port.add("PC invalid\n")
        stream = watch.Stream(port, raw, watch.Analysis(), clock, clock.sleep)
        with self.assertRaises(ValueError): stream.read()
        before = len(port.writes)
        with self.assertRaises(ValueError): stream.send(b"p")
        self.assertEqual(len(port.writes), before)
        self.assertEqual(raw.getvalue(), b"PC invalid\n")


if __name__ == "__main__":
    unittest.main()
