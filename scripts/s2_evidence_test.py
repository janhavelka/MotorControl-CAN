#!/usr/bin/env python3
"""Replay retained S2 captures and adversarial fixtures through the offline parser."""

import hashlib
import json
from pathlib import Path
import re
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
import s2_evidence as evidence  # noqa: E402


ROOT = evidence.ROOT
BENCH = ROOT / "docs/bench"
NODE1 = BENCH / "20261102-final-node1"
NODE2 = BENCH / "20261102-final-node2"
MOTION1 = "motion-readiness1"


def retained():
    return (NODE1 / "rx.bin").read_bytes()


def lines():
    return retained().decode("utf-8").splitlines()


def join(rows):
    return ("\n".join(rows) + "\n").encode("utf-8")


def replay(data, command=MOTION1, **meta):
    fields = {"command": command, "status": "captured", "started_utc": "fixture"}
    fields.update(meta)
    return evidence.replay_bytes(data, fields)


def codes(report):
    return {item["code"] for item in report.errors}


def warnings(report):
    return {item["code"] for item in report.warnings}


def result(step, index, outcome=0, ack=1, possible=1, uncertain=0, abort="00000000", size=2,
           data="37800000", admitted=None, completed=None):
    admitted = 1000 * step if admitted is None else admitted
    completed = admitted + 500 if completed is None else completed
    return ("RESULT step=%d object=%s outcome=%d ack=%d possible=%d uncertain=%d abort=%s cleanup=0 "
            "size=%d data=%s admitted=%d completed=%d" % (step, index, outcome, ack, possible, uncertain,
                                                          abort, size, data, admitted, completed))


def frame(kind, time, identifier, data, ok=1, bounds=None, dlc=8, label="TRAFFIC", extra=""):
    low, high = bounds or (0, 0)
    return ("%s k=%d p=%d id=%x ext=0 rtr=0 dlc=%d data=%s ok=%d exact=0 t=0 bounds=%d lo=%d hi=%d%s" % (
        label, kind, time, identifier, dlc, data, ok, 1 if bounds else 0, low, high, extra))


def request(time, index="4041600000000000"):
    return frame(evidence.TX, time, 0x601, index)


def report_frame(time, ok=1):
    return frame(evidence.TX_REPORT, time, 0, "0" * 16, ok=ok)


def reply(time, data, sent):
    return frame(evidence.RX, time, 0x581, data, bounds=(sent + 10, sent + 20))


def record(failure, terminals, traffic, tail=(), lost=0, tail_lost=0, node=1, end=True):
    """A synthetic record in the current firmware's own line format."""
    out = ["MOTION_READINESS admitted: thirty uploads only; no enable/target/stop/save/retry",
           "RAW id=9 node=%d failure=%d lost=%d terminals=%d traffic=%d tail=%d tailLost=%d gap=2000" % (
               node, failure, lost, len(terminals), len(traffic), len(tail), tail_lost)]
    out += terminals + traffic + list(tail)
    if end:
        out.append("END raw terminals=%d traffic=%d tail=%d tailLost=%d lost=%d; r=release" % (
            len(terminals), len(traffic), len(tail), tail_lost, lost))
    return join(out)


def first_step_ok():
    """Step one of the motion inventory, 1001:00, succeeding with one byte."""
    return (result(1, "1001:00", size=1, data="00000000", admitted=1000, completed=1500),
            [request(1000, "4001100000000000"), report_frame(1500),
             reply(1500, "4f01100000000000", 1000)])


class RetainedCaptures(unittest.TestCase):
    def test_final_nodes_replay_exactly(self):
        for directory, node, operation in ((NODE1, 1, 1), (NODE2, 2, 2)):
            report = evidence.replay_capture(directory)
            self.assertEqual(report.verdict, "complete", report.errors)
            self.assertEqual((report.terminals, report.traffic, report.tail), (30, 90, 0))
            self.assertEqual((report.node, report.operation, report.family), (node, operation, "RAW"))
            self.assertEqual(report.raw_uploads_successful, 30)
            self.assertEqual(report.typed_successes, 0)
            self.assertEqual(report.errors + report.warnings + report.unknown + report.malformed, [])
            self.assertEqual(report.manifest, "match")
            self.assertEqual(report.rx_txt, "identical")
            self.assertTrue(report.started and report.ended and report.inventory_checked)
            self.assertEqual(report.epochs, "not_emitted")

    def test_replay_leaves_retained_bytes_alone(self):
        manifest = json.loads((BENCH / "SHA256SUMS.json").read_text(encoding="utf-8"))
        evidence.replay_capture(NODE1)
        for name in ("rx.bin", "rx.txt", "request.json"):
            digest = hashlib.sha256((NODE1 / name).read_bytes()).hexdigest()
            self.assertEqual(digest, manifest["20261102-final-node1/" + name])

    def test_every_retained_capture_fits_the_grammar(self):
        directories = [p.parent for p in BENCH.glob("*/request.json")]
        self.assertGreater(len(directories), 60)
        for directory in directories:
            report = evidence.replay_capture(directory)
            self.assertEqual(report.unknown, [], directory.name)
            self.assertEqual(report.malformed, [], directory.name)
            self.assertEqual(report.typed_successes if report.family != "EVIDENCE" else 0, 0)
            request = json.loads((directory / "request.json").read_text(encoding="utf-8"))
            # A run the runner refused before its route line holds only its ping: there is no route
            # to complete, and the runner recorded why.
            refused_before_send = set(request.get("sent", ["?"])) <= {"70"} and bool(request.get("reason"))
            # 20261102-s2-identity1: the retained reset investigation. 20261103T1010Z-s2-ping: a ping
            # sent while the S2 was still booting after a reflash, with no PONG; 1012Z's was answered.
            retained_incomplete = ("20261102-s2-identity1", "20261103T1010Z-s2-ping")
            if directory.name not in retained_incomplete and not refused_before_send:
                self.assertNotIn(report.verdict, ("inconsistent", "incomplete"),
                                 (directory.name, report.errors))

    def test_reset_noise_before_the_boot_banner_is_kept_and_accepted(self):
        # The 2026-11-03 10:10Z capture began with one 0xB8 from the S2's UART at reset.
        banner = b"S2_BENCH ready; UART115200; CAN disabled"
        records, _ = evidence.parse_bytes(b"\xb8" + banner + b"\n")
        self.assertEqual((records[0].status, records[0].kind), ("parsed", "BOOT_READY"))
        self.assertEqual(records[0].raw, "\ufffd" + banner.decode())
        self.assertIn("reset noise", records[0].reason)
        # Never on a later line, never three bytes, never before anything but a boot line.
        for data in (b"PONG x\n\xb8" + banner + b"\n",
                     b"\xb8\xb8\xb8" + banner + b"\n",
                     b"\xb8TRIAL record released; sessions and uncertainty retained\n"):
            records, _ = evidence.parse_bytes(data)
            self.assertEqual(records[-1].status, "malformed", data)

    def test_earlier_one_byte_identity_discrepancy_is_preserved(self):
        for name in ("20261102-s2-raw-fixed-node1", "20261102-s2-raw-fixed-node2",
                     "20261102-nmt-pdo-node1", "20261102-nmt-pdo-reboot-node2"):
            report = evidence.replay_capture(BENCH / name)
            conflicts = {(w["object"], w["actual"], w["documented"], w["evidence"])
                         for w in report.width_discrepancies}
            self.assertEqual(conflicts, {("1009:00", 1, 2, "raw upload"), ("100a:00", 1, 2, "raw upload")}, name)
            self.assertEqual(report.typed_successes, 0, "raw bytes are never a typed success")

    def test_measured_width_identity_succeeds_and_keeps_its_discrepancy(self):
        # Outcome 1 is ReadOutcome::SUCCESS. Selecting MEASURED_U8 does not hide the EDS difference.
        for name, operation in (("20261103T0118Z-identity1", 1), ("20261103T0118Z-identity2-b", 2)):
            report = evidence.replay_capture(BENCH / name)
            self.assertEqual((report.verdict, report.family, report.operation),
                             ("complete", "EVIDENCE", operation))
            self.assertEqual(report.typed_successes, 9)
            self.assertEqual({(w["object"], w["actual"]) for w in report.width_discrepancies},
                             {("1009:00", 1), ("100a:00", 1)})

    def test_typed_width_rejection_stays_failed_evidence(self):
        for name, operation in (("20261102-s2-identity1-fixed", 1), ("20261102-s2-identity2", 2)):
            report = evidence.replay_capture(BENCH / name)
            self.assertEqual((report.verdict, report.family, report.operation),
                             ("failed-evidence", "EVIDENCE", operation))
            self.assertEqual(report.typed_successes, 1)  # 1000:00 only.
            self.assertEqual(report.failed[0]["object"], "1009:00")
            self.assertEqual(report.failed[0]["outcome_name"], "CLIENT_ABORT")
            self.assertEqual(report.uncertain[0]["step"], 2)
            self.assertEqual(report.typed_width_failures[0]["actual_wire_width"], 1)
            self.assertEqual(report.typed_width_failures[0]["abort"], "06070010")
            self.assertEqual(report.width_discrepancies, [])

    def test_older_record_with_tail_after_header(self):
        report = evidence.replay_capture(BENCH / "20261102-s2-final-node1")
        self.assertEqual(report.verdict, "complete", report.errors)
        self.assertEqual((report.terminals, report.traffic, report.tail), (20, 59, 1))
        self.assertIn("tail_grew_during_dump", warnings(report))
        self.assertEqual(report.rx_txt, "crlf_only")

    def test_multi_operation_pdo_record(self):
        report = evidence.replay_capture(BENCH / "20261102-nmt-pdo-tpdo1")
        self.assertEqual(report.verdict, "complete", report.errors)
        self.assertEqual((report.terminals, report.traffic, report.tail), (24, 73, 1))
        self.assertEqual(report.raw_uploads_successful, 18)
        self.assertFalse(report.inventory_checked)

    def test_network_listing_without_end_is_not_called_complete(self):
        report = evidence.replay_capture(BENCH / "20261102-nmt-pdo-network1")
        self.assertIn("network_end_absent", warnings(report))

    def test_inventories_match_firmware_source_and_eds(self):
        source = (ROOT / "examples/s2_bench/RawEvidence.cpp").read_text(encoding="utf-8")
        motion = re.search(r"motionReadiness\[\] = \{(.*?)\};", source, re.S).group(1)
        rows = [(int(a, 16), int(b), int(c)) for a, b, c in
                re.findall(r"\{0x([0-9a-f]+), (\d+), (\d+)\}", motion)]
        self.assertEqual(tuple(rows), evidence.INVENTORIES["motion_readiness"][0])
        fields = re.search(r"const Field fields\[\] = \{(.*?)\};", source, re.S).group(1)
        pairs = [(int(a, 16), int(b)) for a, b in re.findall(r"\{0x([0-9a-f]+), (\d+)\}", fields)]
        self.assertEqual(pairs, [row[:2] for row in evidence.INVENTORIES["identity_network"][0]])
        eds = (ROOT / "docs/reference/vendor/LC_StepDriverCAN_V1.1.eds").read_text(encoding="latin-1")
        widths = {"0x0002": 1, "0x0003": 2, "0x0004": 4, "0x0005": 1, "0x0006": 2, "0x0007": 4}
        for name in ("identity_network", "motion_readiness"):
            for index, subindex, width in evidence.INVENTORIES[name][0]:
                section = "%04Xsub%d" % (index, subindex)
                match = re.search(r"\[" + section + r"\]\r?\n(.*?)\r?\n\r?\n", eds, re.S | re.I)
                if match is None and subindex == 0:
                    section = "%04X" % index
                    match = re.search(r"\[" + section + r"\]\r?\n(.*?)\r?\n\r?\n", eds, re.S | re.I)
                self.assertIsNotNone(match, section)
                kind = re.search(r"DataType=(\S+)", match.group(1)).group(1).lower()
                self.assertEqual(widths[kind], width, section)


class DamagedOutput(unittest.TestCase):
    def test_truncated_mid_line(self):
        data = retained()
        report = replay(data[:int(len(data) * 0.6)])
        self.assertEqual(report.verdict, "incomplete")
        self.assertTrue({"truncated_line", "malformed_line", "count_short", "missing_end"} <= codes(report))

    def test_truncated_on_a_line_boundary_still_lacks_its_end(self):
        report = replay(join(lines()[:40]))
        self.assertEqual(report.verdict, "incomplete")
        self.assertTrue({"count_short", "missing_end"} <= codes(report))

    def test_partial_final_line_that_happens_to_parse_is_not_believed(self):
        report = replay(join(lines())[:-1])
        self.assertIn("truncated_line", codes(report))
        self.assertEqual(report.verdict, "incomplete")

    def test_duplicated_terminal(self):
        rows = lines()
        rows.insert(5, rows[4])
        report = replay(join(rows))
        self.assertEqual(report.verdict, "inconsistent")
        self.assertTrue({"duplicate_result", "count_excess"} <= codes(report))

    def test_duplicated_traffic(self):
        rows = lines()
        rows.insert(40, rows[39])
        report = replay(join(rows))
        self.assertEqual(report.verdict, "inconsistent")
        self.assertIn("duplicate_record", codes(report))

    def test_repeated_capture_of_a_whole_record(self):
        report = replay(retained() + retained())
        self.assertEqual(report.verdict, "inconsistent")
        self.assertTrue({"duplicate_start", "duplicate_header"} <= codes(report))

    def test_dropped_line_under_backpressure(self):
        rows = lines()
        del rows[9]
        report = replay(join(rows))
        self.assertEqual(report.verdict, "incomplete")
        self.assertIn("count_short", codes(report))

    def test_lost_bytes_merge_two_lines(self):
        rows = lines()
        rows[10:12] = [rows[10] + rows[11]]
        report = replay(join(rows))
        self.assertIn("malformed_line", codes(report))
        self.assertEqual(report.malformed[0]["line"], 11)
        self.assertIn(report.verdict, ("incomplete", "inconsistent"))
        self.assertIn(rows[10], [m["raw"] for m in report.malformed], "raw text survives")

    def test_undecodable_serial_bytes(self):
        data = bytearray(retained())
        data[300] = 0xFF
        report = replay(bytes(data))
        self.assertIn("malformed_line", codes(report))
        self.assertEqual(report.kinds["CORRUPT"], 1)

    def test_firmware_format_failure_line(self):
        rows = lines()
        rows[6] = "OUTPUT_FORMAT_ERROR; evidence incomplete"
        report = replay(join(rows))
        self.assertTrue({"format_error", "count_short"} <= codes(report))

    def test_dropped_admission_line(self):
        report = replay(join(lines()[1:]))
        self.assertEqual(report.verdict, "incomplete")
        self.assertEqual(codes(report), {"missing_start"})

    def test_empty_capture_and_failed_capture(self):
        self.assertEqual(codes(replay(b"", "ping")) & {"empty_capture", "missing_response"},
                         {"empty_capture", "missing_response"})
        report = replay(retained(), status="failed", error="bounded capture capacity exceeded")
        self.assertEqual(report.verdict, "incomplete")
        self.assertIn("capture_failed", codes(report))

    def test_duplicate_field_is_malformed_not_last_wins(self):
        record_line = evidence.parse_line(1, "STATS running=1 running=0 inhibited=0 rx=1 tx=1 dropped=0 "
                                             "errors=0 flags=00000000 stackFree=1")
        self.assertEqual(record_line.status, "malformed")
        self.assertIn("duplicate", record_line.reason)


class WrongRun(unittest.TestCase):
    def test_other_node_requested(self):
        report = replay(retained(), "motion-readiness2")
        self.assertEqual(report.verdict, "inconsistent")
        self.assertIn("wrong_node", codes(report))

    def test_other_operation_expected(self):
        report = evidence.replay_bytes(retained(), {"command": MOTION1}, expect_operation=7)
        self.assertIn("wrong_operation", codes(report))

    def test_other_procedure_requested(self):
        report = replay(retained(), "raw1")
        self.assertEqual(report.verdict, "inconsistent")
        self.assertTrue({"wrong_procedure", "wrong_object_for_step"} <= codes(report))

    def test_request_byte_differs_from_command(self):
        self.assertIn("request_byte_mismatch", codes(replay(retained(), tx_hex="62")))

    def test_record_in_a_control_response(self):
        report = replay(retained(), "ping")
        self.assertEqual(report.verdict, "inconsistent")
        self.assertIn("wrong_response", codes(report))

    def test_frames_from_the_other_node(self):
        rows = lines()
        for position, row in enumerate(rows):
            if row.startswith("TRAFFIC k=0"):
                rows[position] = row.replace("id=581", "id=582")
                break
        report = replay(join(rows))
        self.assertIn("wrong_channel", codes(report))
        request_rows = lines()
        for position, row in enumerate(request_rows):
            if row.startswith("TRAFFIC k=1"):
                request_rows[position] = row.replace("id=601", "id=602")
                break
        self.assertIn("wrong_channel", codes(replay(join(request_rows))))

    def test_address_mismatch_between_terminal_and_step(self):
        rows = lines()
        rows[4] = rows[4].replace("object=6041:00", "object=6040:00")
        report = replay(join(rows))
        self.assertTrue({"wrong_object_for_step", "wrong_request_object"} <= codes(report))

    def test_subindex_mismatch_on_the_wire(self):
        rows = lines()
        position = next(i for i, row in enumerate(rows) if "data=437d600100000080" in row)
        rows[position] = rows[position].replace("437d600100000080", "437d600200000080")
        self.assertIn("stale_reply", codes(replay(join(rows))))

    def test_step_sequence(self):
        rows = lines()
        rows[4], rows[5] = rows[5], rows[4]
        self.assertIn("step_sequence", codes(replay(join(rows))))

    def test_terminal_width_and_bytes_are_checked_against_the_wire(self):
        rows = lines()
        rows[4] = rows[4].replace("size=2", "size=4")
        self.assertIn("terminal_width_differs_from_wire", codes(replay(join(rows))))
        rows = lines()
        rows[4] = rows[4].replace("data=37800000", "data=37810000")
        self.assertIn("terminal_bytes_differ_from_wire", codes(replay(join(rows))))
        rows = lines()
        rows[4] = rows[4].replace("data=37800000", "data=37800001")
        self.assertIn("bytes_beyond_actual_width", codes(replay(join(rows))))

    def test_header_and_end_disagreement(self):
        rows = lines()
        rows[-1] = rows[-1].replace("traffic=90", "traffic=91")
        self.assertIn("end_differs_from_header", codes(replay(join(rows))))


class StaleFrames(unittest.TestCase):
    def test_reply_of_an_earlier_step_in_this_window(self):
        terminal, frames = first_step_ok()
        second = result(2, "603f:00", size=2, data="00000000", admitted=2000, completed=2500)
        data = record(0, [terminal, second], frames + [
            request(2000, "403f600000000000"), reply(2100, "4f01100000000000", 2000),
            report_frame(2500), reply(2500, "4b3f600000000000", 2000)])
        report = replay(data)
        self.assertEqual(report.verdict, "inconsistent")
        self.assertIn("stale_reply", codes(report))

    def test_reply_received_before_its_request_was_processed(self):
        terminal, frames = first_step_ok()
        frames[2] = frame(evidence.RX, 1500, 0x581, "4f01100000000000", bounds=(100, 200))
        report = replay(record(0, [terminal], frames))
        self.assertIn("stale_reply", codes(report))

    def test_reply_with_no_request_at_all(self):
        terminal, frames = first_step_ok()
        data = record(0, [terminal], [reply(900, "4f01100000000000", 800)] + frames)
        self.assertIn("stale_reply", codes(replay(data)))

    def test_second_reply_for_one_request(self):
        terminal, frames = first_step_ok()
        data = record(0, [terminal], frames + [reply(1500, "4f01100000000000", 1000)])
        self.assertIn("duplicate_reply", codes(replay(data)))

    def test_request_no_terminal_claims(self):
        terminal, frames = first_step_ok()
        data = record(0, [terminal], frames + [request(1700, "403f600000000000")])
        self.assertIn("unmatched_request", codes(replay(data)))

    def test_impossible_receive_bounds_and_time_reversal(self):
        terminal, frames = first_step_ok()
        frames[2] = frame(evidence.RX, 1500, 0x581, "4f01100000000000", bounds=(1400, 1300))
        self.assertIn("receive_bounds_invalid", codes(replay(record(0, [terminal], frames))))
        terminal, frames = first_step_ok()
        frames[1] = report_frame(900)
        self.assertIn("traffic_time_reversed", codes(replay(record(0, [terminal], frames))))


class FailedEvidence(unittest.TestCase):
    """Consistent failures stay separate from success and from damaged output."""

    def failing(self, failure, terminal, traffic):
        ok, frames = first_step_ok()
        return replay(record(failure, [ok, terminal], frames + traffic))

    def test_server_abort(self):
        report = self.failing(3, result(2, "603f:00", outcome=1, ack=0, uncertain=1, abort="06020000",
                                        size=0, data="00000000", admitted=2000, completed=2500),
                              [request(2000, "403f600000000000"), report_frame(2500),
                               reply(2500, "803f600000000206", 2000)])
        self.assertEqual(report.verdict, "failed-evidence", report.errors)
        self.assertEqual(report.failure_name, "SERVICE")
        self.assertEqual(report.failed[0]["outcome_name"], "SERVER_ABORT")
        self.assertEqual(report.failed[0]["abort"], "06020000")
        self.assertEqual(report.raw_uploads_successful, 1)
        self.assertEqual(report.typed_successes, 0)

    def test_abort_that_differs_from_the_wire(self):
        report = self.failing(3, result(2, "603f:00", outcome=1, ack=0, uncertain=1, abort="06090011",
                                        size=0, data="00000000", admitted=2000, completed=2500),
                              [request(2000, "403f600000000000"), report_frame(2500),
                               reply(2500, "803f600000000206", 2000)])
        self.assertIn("abort_differs_from_wire", codes(report))

    def test_timeout_keeps_uncertainty(self):
        report = self.failing(3, result(2, "603f:00", outcome=3, ack=0, uncertain=1, size=0,
                                        data="00000000", admitted=2000, completed=252000),
                              [request(2000, "403f600000000000"), report_frame(2500)])
        self.assertEqual(report.verdict, "failed-evidence", report.errors)
        self.assertEqual(report.failed[0]["outcome_name"], "TIMEOUT")
        self.assertEqual([item["step"] for item in report.uncertain], [2])

    def test_drive_reboot_during_a_transfer(self):
        boot = frame(evidence.RX, 2400, 0x701, "0000000000000000", dlc=1, bounds=(2300, 2350))
        report = self.failing(3, result(2, "603f:00", outcome=6, ack=0, uncertain=1, size=0,
                                        data="00000000", admitted=2000, completed=2500),
                              [request(2000, "403f600000000000"), report_frame(2100), boot])
        self.assertEqual(report.verdict, "failed-evidence", report.errors)
        self.assertEqual(report.failed[0]["outcome_name"], "REBOOT")
        log = evidence.Transitions()
        log.observe(report)
        boots = [e for e in log.entries if e["signal"] == "wire_state"]
        self.assertEqual((boots[0]["node"], boots[0]["to"], boots[0]["time_us"]), (1, "0x00", 2400))
        self.assertEqual(boots[0]["receive_bounds_us"], [2300, 2350])

    def test_controller_reset_during_a_record(self):
        rows = lines()[:20] + ["ESP-ROM:esp32s2-rc4-20191025", "rst:0xc (RTC_SW_CPU_RST),boot:0xa",
                               "S2_BENCH ready; UART115200; CAN disabled; p=ping c=start s=stats"]
        report = replay(join(rows))
        self.assertEqual(report.verdict, "incomplete")
        self.assertIn("target_boot_in_capture", warnings(report))
        log = evidence.Transitions()
        log.observe(report)
        self.assertTrue([e for e in log.entries if e["signal"] == "boot"])

    def test_uncertain_submission_with_no_final_report(self):
        report = self.failing(3, result(2, "6060:00", outcome=9, ack=0, uncertain=1, size=0,
                                        data="00000000", admitted=2000, completed=2500),
                              [request(2000, "4060600000000000")])
        self.assertEqual(report.verdict, "inconsistent")  # Step two of the inventory is 603f.
        self.assertIn("wrong_object_for_step", codes(report))
        report = self.failing(3, result(2, "603f:00", outcome=9, ack=0, uncertain=1, size=0,
                                        data="00000000", admitted=2000, completed=2500),
                              [request(2000, "403f600000000000")])
        self.assertEqual(report.verdict, "failed-evidence", report.errors)
        self.assertEqual(report.failed[0]["outcome_name"], "TRANSPORT_ERROR")
        self.assertIn("tx_report_not_copied", warnings(report))

    def test_failure_before_any_frame_left(self):
        ok, frames = first_step_ok()
        data = record(2, [ok, result(2, "603f:00", outcome=9, ack=0, possible=0, size=0, data="00000000",
                                     admitted=2000, completed=2100)], frames)
        report = replay(data)
        self.assertEqual(report.verdict, "failed-evidence", report.errors)
        self.assertEqual(report.uncertain, [])

    def test_incoherent_terminals(self):
        ok, frames = first_step_ok()
        cases = (result(1, "1001:00", size=1, data="00000000", uncertain=1, admitted=1000, completed=1500),
                 result(1, "1001:00", size=1, data="00000000", ack=0, admitted=1000, completed=1500))
        for terminal in cases:
            self.assertIn("terminal_incoherent", codes(replay(record(0, [terminal], frames))))
        publishing = result(1, "1001:00", outcome=3, ack=0, uncertain=1, size=1, data="05000000",
                            admitted=1000, completed=1500)
        self.assertIn("terminal_incoherent", codes(replay(record(3, [publishing], frames))))

    def test_header_cannot_hide_a_failed_terminal(self):
        ok, frames = first_step_ok()
        failed = result(2, "603f:00", outcome=3, ack=0, uncertain=1, size=0, data="00000000",
                        admitted=2000, completed=2500)
        report = replay(record(0, [ok, failed], frames + [request(2000, "403f600000000000"),
                                                          report_frame(2500)]))
        self.assertIn("failure_differs_from_terminal", codes(report))

    def test_no_result_may_follow_the_first_failure(self):
        ok, frames = first_step_ok()
        failed = result(2, "603f:00", outcome=3, ack=0, uncertain=1, size=0, data="00000000",
                        admitted=2000, completed=2500)
        later = result(3, "6041:00", admitted=3000, completed=3500)
        report = replay(record(3, [ok, failed, later], frames + [
            request(2000, "403f600000000000"), report_frame(2500), request(3000, "4041600000000000"),
            report_frame(3500), reply(3500, "4b41600037800000", 3000)]))
        self.assertIn("result_after_failure", codes(report))

    def test_short_success_is_not_a_complete_plan(self):
        ok, frames = first_step_ok()
        self.assertIn("plan_size_not_in_inventory", codes(replay(record(0, [ok], frames))))


class TraceLoss(unittest.TestCase):
    def test_trace_saturation_is_failed_evidence(self):
        ok, frames = first_step_ok()
        report = replay(record(5, [ok], frames, lost=1))
        self.assertEqual(report.verdict, "failed-evidence", report.errors)
        self.assertEqual(report.failure_name, "EVIDENCE_LOSS")
        self.assertTrue(report.trace_lost)
        self.assertIn("trace_saturated", warnings(report))

    def test_reported_success_with_lost_trace_is_contradictory(self):
        ok, frames = first_step_ok()
        report = replay(record(0, [ok], frames, lost=1))
        self.assertIn("failure_differs_from_terminal", codes(report))

    def test_lost_trace_cannot_be_accused_by_traffic_matching(self):
        # The request frame is missing from the copied traffic. With lost=1 the record said so itself.
        ok, frames = first_step_ok()
        report = replay(record(5, [ok], frames[1:], lost=1))
        self.assertEqual(report.verdict, "failed-evidence", report.errors)
        self.assertIn("success_without_request", warnings(report))
        self.assertEqual(codes(report), set())
        self.assertEqual(replay(record(5, [ok], frames[1:])).verdict, "inconsistent")
        self.assertEqual(replay(record(0, [ok], frames[1:], lost=1)).verdict, "inconsistent")

    def test_tail_overflow_is_visible(self):
        rows = lines()
        rows[1] = rows[1].replace("tailLost=0", "tailLost=1")
        rows[-1] = rows[-1].replace("tailLost=0", "tailLost=1")
        report = replay(join(rows))
        self.assertTrue(report.tail_lost)
        self.assertIn("tail_saturated", warnings(report))

    def test_end_may_extend_but_not_reduce_the_tail(self):
        rows = lines()
        rows[1] = rows[1].replace("tail=0", "tail=2")
        self.assertIn("count_short", codes(replay(join(rows))))


class Grammar(unittest.TestCase):
    def test_unknown_kind_is_preserved_and_counted(self):
        rows = lines()
        rows.insert(3, "TWAI_ISR depth=3 lost=0")
        report = replay(join(rows))
        self.assertEqual(report.kinds["UNKNOWN"], 1)
        self.assertEqual(report.unknown, [{"line": 4, "raw": "TWAI_ISR depth=3 lost=0"}])
        self.assertEqual(report.verdict, "complete", "a new diagnostic line must not fail old checks")

    def test_unknown_field_on_a_known_kind_is_preserved(self):
        rows = lines()
        rows[1] = rows[1] + " queueHigh=3"
        report = replay(join(rows))
        self.assertEqual(report.unknown_fields, [{"line": 2, "fields": {"queueHigh": "3"}}])
        self.assertEqual(report.verdict, "complete")

    def test_a_new_kind_needs_one_row(self):
        row = evidence.Row("QUEUE", "QUEUE ", "response", "high:u p:u", "p", (("health", "high"),))
        rows = lines()
        rows.insert(3, "QUEUE high=4 p=11")
        with mock.patch.object(evidence, "GRAMMAR", evidence.GRAMMAR + (row,)):
            report = evidence.replay_bytes(join(rows), {})
        self.assertEqual((report.kinds["QUEUE"], report.kinds["UNKNOWN"]), (1, 0))
        log = evidence.Transitions()
        log.observe(report)
        entry = [e for e in log.entries if e["signal"] == "high"][0]
        self.assertEqual((entry["to"], entry["time_us"]), (4, 11))

    def test_epochs_are_checked_only_where_emitted(self):
        rows = lines()
        rows = [r + " epoch=3" if r.startswith("TRAFFIC") else r for r in rows]
        report = replay(join(rows))
        self.assertEqual(report.epochs, [3])
        self.assertEqual(report.verdict, "complete")
        position = next(i for i, r in enumerate(rows) if r.startswith("TRAFFIC k=1")) + 30
        rows[position] = rows[position].replace("epoch=3", "epoch=4")
        report = replay(join(rows))
        self.assertIn("epoch_changed", codes(report))

    def test_reply_forms(self):
        self.assertEqual(evidence.sdo_reply(bytes.fromhex("4f01100000000000")), ("upload", 1))
        self.assertEqual(evidence.sdo_reply(bytes.fromhex("4b01100000000000")), ("upload", 2))
        self.assertEqual(evidence.sdo_reply(bytes.fromhex("4701100000000000")), ("upload", 3))
        self.assertEqual(evidence.sdo_reply(bytes.fromhex("4301100000000000")), ("upload", 4))
        self.assertEqual(evidence.sdo_reply(bytes.fromhex("4201100000000000")), ("upload_unsized", 4))
        self.assertEqual(evidence.sdo_reply(bytes.fromhex("4101100004000000")), ("upload_segmented", None))
        self.assertEqual(evidence.sdo_reply(bytes.fromhex("6001100000000000")), ("download_ack", 0))
        self.assertEqual(evidence.sdo_reply(bytes.fromhex("8001100000000206")), ("abort", None))


class TransitionLog(unittest.TestCase):
    def session(self, *names):
        return evidence.replay_session([BENCH / name for name in names])

    def test_retained_session_in_order(self):
        reports, entries, problems = self.session(
            "20261102-final-ping", "20261102-final-start", "20261102-final-node1",
            "20261102-final-release1", "20261102-final-node2", "20261102-final-stats")
        self.assertEqual(problems, [])
        self.assertEqual([e["seq"] for e in entries], list(range(1, len(entries) + 1)))
        status = [e for e in entries if e["signal"] == "statusword_6041"]
        self.assertEqual([(e["node"], e["to"], e["from"], e["time_us"]) for e in status],
                         [(1, "0x8037", None, 195982974), (2, "0x8037", None, 208348974)])
        self.assertIn("RESULT step=3 object=6041:00", status[0]["raw"])
        running = [(e["from"], e["to"]) for e in entries if e["signal"] == "running"]
        self.assertEqual(running, [(None, 0), (0, 1)], "controller started between ping and stats")
        self.assertEqual([e["to"] for e in entries if e["signal"] == "silent"], [1])

    def test_retained_drive_power_cycle_boundary(self):
        _, entries, _ = self.session("20261102-nmt-pdo-boot-watch")
        wire = [(e["node"], e["from"], e["to"], e["time_us"]) for e in entries if e["signal"] == "wire_state"]
        self.assertIn((1, "0x7f", "0x00", 509803962), wire)
        self.assertIn((2, None, "0x00", 509793962), wire)
        epoch = [(e["node"], e["from"], e["to"]) for e in entries if e["signal"] == "epoch"]
        self.assertEqual(epoch, [(1, None, 1), (2, None, 1), (1, 1, 2), (2, 1, 2)])
        producer = [(e["node"], e["to"]) for e in entries if e["signal"] == "producerKnown"]
        self.assertIn((1, 0), producer)

    def test_unchanged_values_add_nothing_and_changes_keep_raw_values(self):
        log = evidence.Transitions()
        rows = lines()
        log.observe(replay(join(rows)))
        count = len(log.entries)
        log.observe(replay(join(rows)))
        self.assertEqual(len(log.entries), count)
        changed = join([r.replace("data=37800000", "data=37020000") for r in rows])
        changed = changed.replace(b"data=4b41600037800000", b"data=4b41600037020000")
        log.observe(replay(changed))
        entry = log.entries[-1]
        self.assertEqual((entry["signal"], entry["from"], entry["to"]), ("statusword_6041", "0x8037", "0x0237"))
        self.assertIn("data=37020000", entry["raw"])

    def test_failure_controller_and_recovery_records_are_transitions(self):
        ok, frames = first_step_ok()
        failed = result(2, "603f:00", outcome=3, ack=0, uncertain=1, size=0, data="00000000",
                        admitted=2000, completed=2500)
        tail = [frame(evidence.CONTROLLER, 2600, 0, "0" * 16, label="TAIL", extra=" ctrl=BUS_OFF"),
                frame(evidence.RECOVERY, 2700, 0, "0" * 16, label="TAIL", extra=" rec=RETIRED"),
                frame(evidence.TX_REPORT, 2800, 0, "0" * 16, ok=0, label="TAIL")]
        report = replay(record(3, [ok, failed], frames + [request(2000, "403f600000000000")], tail=tail))
        self.assertEqual(report.verdict, "failed-evidence", report.errors)
        log = evidence.Transitions()
        log.observe(report)
        found = {(e["category"], e["signal"], e["to"]) for e in log.entries}
        self.assertTrue({("controller", "controller_state", "BUS_OFF"), ("controller", "recovery", "RETIRED"),
                         ("controller", "tx_report_ok", 0), ("health", "sdo_outcome", 3),
                         ("health", "effect_uncertain", 1), ("health", "failure", 3)} <= found)

    def test_move_and_its_stop_keep_separate_results(self):
        """A move record with its planned stop: each result has its own signal, never one key."""
        sample = ("TRIAL_MOVE_SAMPLE which=%d present=1 cycle=1 status=9037 monitor=0004 mode=01 feedback=1 "
                  "position=3 velocity=1 lo=10 hi=20")
        rows = ["TRIAL admitted route=move node=1 trial=7 motion=2 case=1; finite typed route, no retry",
                "TRIAL id=7 node=1 route=move failure=0 why=none lost=0 terminals=0 traffic=0 tail=0 tailLost=0 "
                "summaries=12 step=3 cycles=1 motion=2 case=1 unsettled=0 gap=1000 lossCancel=0",
                "TRIAL_PLAN route=move node=1 trial=7 attended=1 sp=0 target=16 rpm=1 up=1000 down=1000 "
                "deadline=10000 interval=1000 kind=1 after=1 sd=2000 be=0 si=500",
                "TRIAL_RES stack0=9300 stack1=9100 heap0=60000 heap1=59000 heapMin=58000 largest=40000 gap=1000 "
                "dropped=0 admission=0 started=10 completed=20",
                "TRIAL_EVIDENCE record=19 as=1 am=1000 av=1000 cs=1 cm=8000 cv=8000 rule=1 dt=00040192 hw=0012 "
                "sw=0034 vid=00000300 pc=00008605 rev=00010001 mc=0086 fc=0123",
                "TRIAL_MOVE own=1 state=3 phase=2 phases=07 outcome=4 int=2 setup=1 exec=1 completion=0 rearm=0 "
                "uncertain=1 readUnc=0 effects=1 cycles=1 op=9 target=16 mask=f ev=f trigger=1 rearmed=0 "
                "fenced=1 code=0 detail=0"]
        rows += [sample % which for which in range(5)]
        rows += ["TRIAL_STOP kind=1 path=1 final=4 own=1 state=2 outcome=1 exec=1 completion=1 uncertain=0 "
                 "effects=1 pred=001f documented=1 basis=1 observations=1 op=10",
                 "TRIAL_BEST_EFFORT which=0 own=0 state=0 outcome=0 exec=0 admitted=0 effects=0 op=0",
                 "TRIAL_BEST_EFFORT which=1 own=0 state=0 outcome=0 exec=0 admitted=0 effects=0 op=0",
                 "END trial terminals=0 traffic=0 tail=0 tailLost=0 lost=0; r=release"]
        report = evidence.replay_bytes(join(rows), {"command": "move", "node": 1, "trial": 7,
                                                    "line": "TG1,7", "status": "captured"})
        self.assertEqual(report.verdict, "complete", report.errors)
        self.assertEqual(report.unknown_fields, [])
        log = evidence.Transitions()
        log.observe(report)
        found = {(e["signal"], e["node"], e["from"], e["to"]) for e in log.entries if e["category"] == "health"}
        self.assertTrue({("moveOutcome", 1, None, 4), ("moveUncertain", 1, None, 1), ("moveFenced", 1, None, 1),
                         ("moveCompletion", 1, None, 0), ("stopOutcome", 1, None, 1),
                         ("stopPath", 1, None, 1)} <= found, found)
        self.assertFalse([e for e in log.entries if e["signal"] in ("outcome", "uncertain", "path", "fenced")])

    def test_boot_marks_following_observations(self):
        boot = join(["rst:0xc (RTC_SW_CPU_RST),boot:0xa (SPI_FAST_FLASH_BOOT)",
                     "S2_BENCH ready; UART115200; CAN disabled; p=ping c=start s=stats"])
        stats = b"STATS running=1 inhibited=0 rx=1 tx=1 dropped=0 errors=0 flags=00000000 stackFree=9\n"
        log = evidence.Transitions()
        log.observe(evidence.replay_bytes(stats, {}))
        log.observe(evidence.replay_bytes(boot, {}))
        log.observe(evidence.replay_bytes(stats, {}))
        last = log.entries[-1]
        self.assertTrue(last["first_observation"] and last["after_boot"])
        self.assertIsNone(last["from"], "a value from before the boot is not this boot's predecessor")

    def test_session_flags_a_repeated_operation_and_mixed_fixtures(self):
        _, _, problems = self.session("20261102-final-node1", "20261102-s2-final-node1")
        self.assertEqual(problems[0]["code"], "operation_repeated_without_observed_boot")
        with tempfile.TemporaryDirectory() as temporary:
            other = Path(temporary) / "other"
            other.mkdir()
            (other / "rx.bin").write_bytes(b"STATS running=0 inhibited=0 rx=0 tx=0 dropped=0 errors=0 "
                                           b"flags=00000000 stackFree=1\n")
            (other / "request.json").write_text(json.dumps(
                {"command": "stats", "serial": "OTHER", "status": "captured"}), encoding="utf-8")
            _, _, problems = evidence.replay_session([NODE1, other])
        self.assertEqual(problems[0]["code"], "mixed_fixture_serials")


class Image(unittest.TestCase):
    def setUp(self):
        self.build = json.loads((BENCH / "20261102T173312Z-s2-upload/build.json").read_text(encoding="utf-8"))
        self.report = evidence.replay_capture(BENCH / "20261102-final-ping")

    def test_retained_ping_matches_its_upload_record(self):
        self.assertEqual(evidence.check_image(self.report, self.build), [])

    def test_other_image_or_base_is_rejected(self):
        changed = json.loads(json.dumps(self.build))
        changed["image_sha256"]["motorcontrol_can_s2_bench.elf"] = "0" * 64
        self.assertIn("elf", evidence.check_image(self.report, changed)[0])
        changed = dict(self.build, base_commit="1" * 40)
        self.assertIn("base", evidence.check_image(self.report, changed)[0])
        self.assertIn("target", evidence.check_image(self.report, dict(self.build, target="esp32s3"))[0])
        self.assertIn("sdk", evidence.check_image(self.report, dict(self.build, sdk_version="5.4"))[0])

    def test_missing_or_repeated_pong_is_not_a_match(self):
        self.assertTrue(evidence.check_image(replay(b"", "ping"), self.build))
        pong = (BENCH / "20261102-final-ping/rx.bin").read_bytes()
        self.assertTrue(evidence.check_image(replay(pong + pong, "ping"), self.build))


class CommandLine(unittest.TestCase):
    def test_retained_nodes_pass_with_exact_counts(self):
        with mock.patch("builtins.print"):
            status = evidence.main([str(NODE1), str(NODE2), "--expect-terminals", "30",
                                    "--expect-traffic", "90", "--require-complete"])
        self.assertEqual(status, 0)

    def test_wrong_expected_count_fails(self):
        with mock.patch("builtins.print"):
            self.assertEqual(evidence.main([str(NODE1), "--expect-terminals", "26"]), 1)

    def test_failed_evidence_fails_only_when_completion_is_required(self):
        capture = str(BENCH / "20261102-s2-identity1-fixed")
        with mock.patch("builtins.print"):
            self.assertEqual(evidence.main([capture]), 0)
            self.assertEqual(evidence.main([capture, "--require-complete"]), 1)

    def test_reports_are_new_files_and_never_overwrite(self):
        with tempfile.TemporaryDirectory() as temporary:
            report, log = Path(temporary) / "report.json", Path(temporary) / "transitions.jsonl"
            with mock.patch("builtins.print"):
                self.assertEqual(evidence.main([str(NODE1), "--report", str(report),
                                                "--transitions", str(log)]), 0)
            self.assertEqual(json.loads(report.read_text(encoding="utf-8"))["captures"][0]["terminals"], 30)
            first = json.loads(log.read_text(encoding="utf-8").splitlines()[0])
            self.assertEqual(first["seq"], 1)
            with mock.patch("sys.stderr"), self.assertRaises(SystemExit):
                evidence.main([str(NODE1), "--report", str(report)])

    def test_replay_opens_no_serial_port(self):
        with mock.patch.dict(sys.modules, {"serial": None}):
            self.assertEqual(evidence.replay_capture(NODE1).verdict, "complete")


if __name__ == "__main__":
    unittest.main()
