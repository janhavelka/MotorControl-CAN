#!/usr/bin/env python3
"""s2_smoke.py against a fake board, clock and USB list: no port, no pyserial, no process."""

import contextlib
import hashlib
import io
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
import s2_flash as flash  # noqa: E402
import s2_flash_test as flash_test  # noqa: E402
import s2_smoke as smoke  # noqa: E402

ROOT = flash.ROOT
SERIAL, PORT = flash.FIXTURE["usb_serial"], flash.FIXTURE["port"]
ELF = "e" * 64
# The real S2 ROM banner as retained before the bench image's first line (2026-11-02).
RETAINED_BOOT = (ROOT / "docs/bench/20261102-s2-identity1/rx.bin").read_bytes()
ROM = RETAINED_BOOT[:RETAINED_BOOT.index(b"S2_BENCH ready")]
PASSIVE_LINE = smoke.PASSIVE.encode("ascii") + b"\n"


def ready_line(elf=ELF, idf="5.5.5", project="motorcontrol_can_probe"):
    """FirmwareSmoke's ready line with probe_idf main.cpp's identity text; idf as app->idf_ver
    reported 5.5.5 in the retained bench pings."""
    return ("SMOKE ready elf=%s idf=%s project=%s\n" % (elf, idf, project)).encode("ascii")


def boot(ready=None):
    """ROM banner, the passive line, then the ready line (the recorded image by default)."""
    return ROM + PASSIVE_LINE + (ready_line() if ready is None else ready)


PASSIVE_BOOT = boot()
IDENTITY = ("identity=ok device=00040192 hw=0012/1 sw=0034/1 entries=4 vendor=00000300 product=00008605 "
            "revision=00010001 model=0086 firmware=0123 skew=1800")


def image_line(elf=ELF, start="ok", running=1, transceiver="normal-requested", project="motorcontrol_can_probe"):
    """FirmwareSmoke.cpp's format; the identity text is main.cpp's smokeConfig."""
    return ("SMOKE image elf=%s idf=5.5.5 project=%s start=%s running=%d transceiver=%s check=0\n"
            % (elf, project, start, running, transceiver))


def reply(elf=ELF, node1=IDENTITY, node2=IDENTITY, ignored=0):
    return (image_line(elf) + "SMOKE node=1 %s\n" % node1 + "SMOKE node=2 %s\n" % node2 +
            "SMOKE done; no further requests; ignored=%d\n" % ignored).encode("ascii")


class Clock:
    def __init__(self):
        self.seconds = 0.0

    def utc(self):
        return "2026-11-06T16:00:%06.3fZ" % self.seconds

    def monotonic(self):
        self.seconds += 0.01
        return self.seconds


class Board:
    """The smoke image's console as the runner sees it: bytes become readable at clock times."""

    def __init__(self, clock, boot=PASSIVE_BOOT, answer=None, later=(), boots_on_reset=False):
        self.clock = clock
        # A board that already booted keeps its boot lines only if they arrive after a reset.
        self.pending = bytearray(boot) if boots_on_reset else bytearray()
        self.queue = bytearray() if boots_on_reset else bytearray(boot)
        self.signals = []
        self._rts = False
        self.answer = reply() if answer is None else answer
        self.later = sorted(later)  # (seconds, bytes) that arrive on their own, without a write.
        self.writes = []
        self.closed = False

    def _release(self):
        while self.later and self.later[0][0] <= self.clock.seconds:
            self.queue.extend(self.later.pop(0)[1])

    @property
    def in_waiting(self):
        self._release()
        return len(self.queue)

    def read(self, count):
        self._release()
        out = bytes(self.queue[:count])
        del self.queue[:count]
        return out

    def write(self, data):
        self.writes.append(bytes(data))
        if data == smoke.TRIGGER and len(self.writes) == 1:
            self.queue.extend(self.answer)  # A second G would do nothing, as in FirmwareSmoke.
        return len(data)

    def close(self):
        self.closed = True

    @property
    def rts(self):
        return self._rts

    @rts.setter
    def rts(self, value):
        self.signals.append(("rts", value))
        if self._rts and not value:  # EN released: the chip boots and prints its boot lines.
            self.queue.extend(self.pending)
            del self.pending[:]
        self._rts = value

    @property
    def dtr(self):
        return False

    @dtr.setter
    def dtr(self, value):
        self.signals.append(("dtr", value))


class Item:
    def __init__(self, device=PORT, serial=SERIAL, vid=0x303A, pid=0x1002):
        self.device, self.serial_number, self.vid, self.pid = device, serial, vid, pid


class Backend:
    def __init__(self, board, ports=None):
        self.board = board
        self.ports = [Item()] if ports is None else ports
        self.opened = []

    def comports(self):
        return self.ports

    def open(self, device):
        self.opened.append(device)
        if isinstance(self.board, Exception):
            raise self.board
        return self.board


class Untouchable:
    def comports(self):
        raise AssertionError("enumerated ports")

    def open(self, device):
        raise AssertionError("opened %s" % device)


def write_record(directory, **changes):
    """A retained smoke upload record in s2_flash.py's shape."""
    directory.mkdir(parents=True)
    sdkconfig = changes.pop("sdkconfig", flash_test.resolved_sdkconfig(flash_test.SMOKE)).encode("utf-8")
    (directory / "sdkconfig").write_bytes(sdkconfig)
    record = {"status": "flashed", "app": flash.app_record(flash_test.SMOKE), "target": "esp32s2",
              "app_elf": "motorcontrol_can_probe.elf", "project_name": "motorcontrol_can_probe",
              "sdk_version": "5.5.5", "base_commit": "a" * 40, "usb": {"usb_serial": SERIAL},
              "image_sha256": {"motorcontrol_can_probe.elf": ELF, "sdkconfig": hashlib.sha256(sdkconfig).hexdigest()}}
    record.update(changes)
    (directory / "build.json").write_text(json.dumps(record), encoding="utf-8")
    return directory


class SmokeTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.base = Path(temporary.name)
        self.output = self.base / "smoke"
        self.artifact = self.base / "upload"
        self.clock = Clock()
        state = mock.patch.object(smoke, "repository_state", return_value={"host_revision": "a" * 40,
                                                                              "host_dirty": True})
        state.start()
        self.addCleanup(state.stop)

    def run_main(self, backend, *extra, artifact=None):
        out, err = io.StringIO(), io.StringIO()
        argv = ["--port", PORT, "--serial", SERIAL, "--artifact", str(artifact or self.artifact),
                "--output", str(self.output), *extra]
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err), \
                mock.patch.object(subprocess, "run", side_effect=AssertionError("process")):
            code = smoke.main(argv, backend, self.clock)
        return code, out.getvalue() + err.getvalue()

    def meta(self):
        return json.loads((self.output / "request.json").read_text(encoding="utf-8"))

    def board(self, **options):
        return Board(self.clock, **options)

    def test_passes_with_one_g_and_retains_everything(self):
        write_record(self.artifact)
        board = self.board()
        code, output = self.run_main(Backend(board))
        self.assertEqual(code, 0, output)
        self.assertEqual(board.writes, [b"G"])
        self.assertTrue(board.closed)
        meta = self.meta()
        self.assertEqual(meta["result"]["verdict"], "passed")
        self.assertEqual(meta["sent"], ["47"])
        self.assertEqual(meta["boot_line"], smoke.PASSIVE)
        self.assertEqual(meta["ready_line"], ready_line().decode("ascii").strip())
        self.assertEqual(meta["ready"], {"elf": ELF, "idf": "5.5.5", "project": "motorcontrol_can_probe"})
        self.assertEqual(meta["ready_problems"], [])
        self.assertLess(meta["times"]["ready_line_s"], meta["times"]["trigger_s"])
        self.assertEqual(meta["reset_reason"], "rst:0xc (RTC_SW_CPU_RST),boot:0xa (SPI_FAST_FLASH_BOOT)")
        self.assertIn("ESP-ROM:esp32s2-rc4-20191025", meta["boot_lines"])
        self.assertEqual((meta["reset"], meta["retry"], meta["dtr_rts"]), (False, False, "deasserted before open"))
        self.assertEqual(meta["result"]["nodes"]["1"]["vendor"], "00000300")
        self.assertEqual(meta["result"]["nodes"]["2"]["result"], "ok")
        self.assertEqual(meta["result"]["image"]["elf"], ELF)
        self.assertEqual(meta["result"]["ignored"], 0)
        self.assertLess(meta["times"]["boot_line_s"], meta["times"]["trigger_s"])
        raw = (self.output / "rx.bin").read_bytes()
        self.assertEqual(raw, PASSIVE_BOOT + reply(), "every byte, in order, nothing flushed")
        self.assertEqual(meta["trigger_offset"], len(PASSIVE_BOOT))
        self.assertTrue((self.output / "rx.txt").is_file())
        self.assertIn("node 1: SMOKE node=1 identity=ok", output)
        self.assertIn("ready line: SMOKE ready elf=%s" % ELF, output)
        self.assertIn("verdict: passed", output)

    def test_record_written_by_s2_flash_is_accepted(self):
        """End to end offline: s2_flash.py --app probe-idf-optak-smoke --flash against its fakes."""
        upload = self.base / "real-upload"
        idf, python, tools_path = flash_test.make_tools(self.base / "tools")
        build = self.base / "build"
        bench = flash_test.Bench(build, app=flash_test.SMOKE)
        runner = mock.Mock(summary={})
        environment = {key: value for key, value in os.environ.items()
                       if key not in ("MSYSTEM", "IDF_PATH", "IDF_TOOLS_PATH", "IDF_PYTHON")}
        environment["PATH"] = ""
        with mock.patch.dict(os.environ, environment, clear=True), \
                mock.patch.object(flash, "execute", bench.execute), \
                mock.patch.object(flash, "sdk_environment", return_value=(["python"], {})), \
                mock.patch.object(flash, "Runner", return_value=runner), \
                mock.patch.object(flash, "comports", return_value=[flash_test.port_info()]), \
                mock.patch.object(flash, "source_identity", side_effect=lambda: dict(flash_test.IDENTITY)), \
                mock.patch("sys.stdout", new_callable=io.StringIO), mock.patch("sys.stderr", new_callable=io.StringIO):
            self.assertEqual(flash.main(["--flash", "--app", flash_test.SMOKE, "--port", PORT, "--serial", SERIAL,
                                         "--idf-path", str(idf), "--idf-python", str(python),
                                         "--idf-tools-path", str(tools_path), "--build-dir", str(build),
                                         "--output", str(upload)]), 0)
        elf = json.loads((upload / "build.json").read_text(encoding="utf-8"))["image_sha256"][
            "motorcontrol_can_probe.elf"]
        board = self.board(boot=boot(ready_line(elf=elf)), answer=reply(elf=elf))
        code, output = self.run_main(Backend(board), artifact=upload)
        self.assertEqual(code, 0, output)
        self.assertEqual(self.meta()["expected"]["nodes"], [1, 2])
        # The same record against an image still running from before: refused by its ready line, unsent.
        self.output = self.base / "smoke-old"
        board = self.board()
        code, output = self.run_main(Backend(board), artifact=upload)
        self.assertEqual(code, 2)
        self.assertEqual(board.writes, [])
        self.assertIn("elf: image reports %s" % ELF, output)

    def test_post_start_elf_mismatch_fails(self):
        """The image line is still compared after the start, even after a matching ready line."""
        write_record(self.artifact)
        board = self.board(answer=reply(elf="d" * 64))
        code, output = self.run_main(Backend(board))
        self.assertEqual(code, 1)
        self.assertEqual(board.writes, [b"G"])
        problems = self.meta()["result"]["problems"]
        self.assertEqual(problems, ["elf: image reports %s, build record has %s" % ("d" * 64, ELF)])
        self.assertEqual(self.meta()["result"]["verdict"], "failed")

    def refused_unsent(self, reason, **board):
        """Runs once against a fake board; nothing may be written and the exit status is 2."""
        self.output = self.base / ("unsent-%d" % len(list(self.base.glob("unsent-*"))))
        fake = self.board(**board)
        code, output = self.run_main(Backend(fake))
        self.assertEqual(code, 2, output)
        self.assertEqual(fake.writes, [], reason)
        meta = self.meta()
        self.assertEqual((meta["reason"], meta["sent"], meta["result"]["verdict"]), (reason, [], "not_sent"))
        self.assertNotIn("trigger_offset", meta)
        return meta, output

    def test_ready_line_with_another_identity_is_refused_unsent(self):
        write_record(self.artifact)
        for changes, text in (({"elf": "d" * 64}, "elf: image reports " + "d" * 64),
                              ({"idf": "5.4.1"}, "idf: image reports 5.4.1, build record has 5.5.5"),
                              ({"project": "motorcontrol_can_s2_bench"}, "project: image reports motorcontrol_can_s2")):
            meta, output = self.refused_unsent("identity_mismatch", boot=boot(ready_line(**changes)))
            self.assertTrue(any(text in problem for problem in meta["ready_problems"]), meta["ready_problems"])
            self.assertIn(text, output)
            self.assertIn(text, " ".join(meta["result"]["problems"]))
        # Only a leading v differs: the same SDK version, as the image-line check treats it.
        self.output = self.base / "v-prefix"
        board = self.board(boot=boot(ready_line(idf="v5.5.5")))
        self.assertEqual(self.run_main(Backend(board))[0], 0)

    def test_unconfigured_or_malformed_ready_line_is_refused_unsent(self):
        write_record(self.artifact)
        self.refused_unsent("identity_unconfigured", boot=boot(b"SMOKE ready identity=unconfigured\n"))
        for line in (b"SMOKE ready elf=%s idf=5.5.5\n" % (b"e" * 64), b"SMOKE ready elf=EEEE idf=5.5.5 project=x\n",
                     b"SMOKE ready elf=%s idf=5.5.5 project=motorcontrol_can_probe extra\n" % (b"e" * 64),
                     b"SMOKE ready\n"):
            self.refused_unsent("ready_line_malformed", boot=boot(line))

    def test_missing_ready_line_is_refused_unsent(self):
        write_record(self.artifact)
        meta, _ = self.refused_unsent("ready_line_missing", boot=ROM + PASSIVE_LINE)
        self.assertEqual(meta["after_passive"], [])
        # Any other line between the passive and ready lines also stops it.
        meta, _ = self.refused_unsent("ready_line_missing", boot=boot(
            b"MotorControl-CAN: clock fault; controller inhibited.\n" + ready_line()))
        self.assertEqual(meta["after_passive"][0], "MotorControl-CAN: clock fault; controller inhibited.")
        # A ready line that never ends in a newline is not a ready line.
        self.refused_unsent("ready_line_missing", boot=ROM + PASSIVE_LINE + ready_line().rstrip(b"\n"))

    def test_ready_line_before_the_passive_line_is_refused_unsent(self):
        write_record(self.artifact)
        self.refused_unsent("ready_before_passive", boot=ROM + ready_line() + PASSIVE_LINE)
        self.refused_unsent("ready_before_passive", boot=ROM + ready_line())

    def test_ready_line_in_a_later_read_passes(self):
        write_record(self.artifact)
        board = self.board(boot=ROM + PASSIVE_LINE, later=[(0.3, ready_line())])
        code, output = self.run_main(Backend(board))
        self.assertEqual(code, 0, output)
        self.assertEqual(board.writes, [b"G"])

    def test_output_after_the_ready_line_in_the_same_read_is_refused_unsent(self):
        """Lines that arrive in the ready line's own chunk count, not only those read later."""
        write_record(self.artifact)
        meta, _ = self.refused_unsent("output_after_ready", boot=PASSIVE_BOOT + b"SMOKE node=1 identity=ok\n")
        self.assertEqual(meta["after_ready"], ["SMOKE node=1 identity=ok"])
        meta, _ = self.refused_unsent("output_after_ready", boot=PASSIVE_BOOT + b"partial")
        self.assertEqual(meta["after_ready"], ["partial"])

    def test_project_idf_and_start_must_match(self):
        cases = {"project": image_line(project="motorcontrol_can_s2_bench"),
                 "idf": image_line().replace("idf=5.5.5", "idf=v5.4.1"),
                 "start=failed": image_line(start="failed", running=0, transceiver="failed"),
                 "transceiver=silent-requested": image_line(transceiver="silent-requested")}
        write_record(self.artifact)
        for number, (text, line) in enumerate(cases.items()):
            self.output = self.base / ("smoke%d" % number)
            body = line.encode("ascii") + (b"" if "failed" in text else reply()[len(image_line()):])
            board = self.board(answer=body)
            code, _ = self.run_main(Backend(board))
            self.assertEqual(code, 1, text)
            self.assertEqual(board.writes, [b"G"], text)
            self.assertTrue(any(text.split("=")[0] in problem for problem in self.meta()["result"]["problems"]), text)

    def test_failed_start_is_terminal_and_not_waited_out(self):
        write_record(self.artifact)
        board = self.board(answer=image_line(start="failed", running=0).encode("ascii"))
        code, _ = self.run_main(Backend(board), "--seconds", "100")
        self.assertEqual(code, 1)
        self.assertEqual(self.meta()["reason"], "terminal")
        self.assertLess(self.clock.seconds, 5, "the capture stops at the terminal line")

    def test_each_node_result_is_reported(self):
        write_record(self.artifact)
        board = self.board(answer=reply(node1="identity=failed state=3 outcome=4 uncertain=1",
                                        node2="identity=refused; no request"))
        code, output = self.run_main(Backend(board))
        self.assertEqual(code, 1)
        result = self.meta()["result"]
        self.assertEqual(result["nodes"]["1"]["result"], "failed")
        self.assertEqual(result["nodes"]["1"]["uncertain"], "1")
        self.assertEqual(result["nodes"]["2"]["result"], "refused")
        self.assertIn("node 1: SMOKE node=1 identity=failed state=3 outcome=4 uncertain=1", output)
        self.assertIn("problem: node 2: node=2 identity=refused; no request", output)

    def test_stopped_controller_ends_the_capture(self):
        write_record(self.artifact)
        answer = (image_line() + "MotorControl-CAN: clock fault; controller inhibited.\n"
                  "SMOKE node=1 stopped: controller not running failure=1\n").encode("ascii")
        code, _ = self.run_main(Backend(self.board(answer=answer)))
        self.assertEqual(code, 1)
        meta = self.meta()
        self.assertEqual(meta["reason"], "terminal")
        problems = " ".join(meta["result"]["problems"])
        for text in ("node 1: node=1 stopped", "node 2: no result line", "unexpected output: MotorControl-CAN: "
                     "clock fault", "no SMOKE done line"):
            self.assertIn(text, problems)

    def test_timeout_after_g_is_incomplete_and_sends_nothing_more(self):
        write_record(self.artifact)
        board = self.board(answer=(image_line() + "SMOKE node=1 %s\n" % IDENTITY).encode("ascii"))
        code, output = self.run_main(Backend(board), "--seconds", "2")
        self.assertEqual(code, 1)
        self.assertEqual(board.writes, [b"G"], "no second G, no other byte, no retry")
        meta = self.meta()
        self.assertEqual((meta["reason"], meta["result"]["verdict"]), ("no_terminal", "incomplete"))
        self.assertIn("node 2: no result line", meta["result"]["problems"])
        self.assertIn("verdict: incomplete", output)

    def test_other_bytes_counted_by_the_image_fail(self):
        write_record(self.artifact)
        code, _ = self.run_main(Backend(self.board(answer=reply(ignored=2))))
        self.assertEqual(code, 1)
        self.assertIn("the image counted 2 bytes other than the one G", self.meta()["result"]["problems"])

    def test_refused_fixture_configuration_is_terminal(self):
        write_record(self.artifact)
        code, _ = self.run_main(Backend(self.board(answer=(smoke.REFUSED + "\n").encode("ascii"))))
        self.assertEqual(code, 1)
        self.assertEqual(self.meta()["reason"], "terminal")

    def test_passive_line_missed_sends_nothing(self):
        write_record(self.artifact)
        board = self.board(boot=b"")
        code, output = self.run_main(Backend(board), "--boot-seconds", "1")
        self.assertEqual(code, 2)
        self.assertEqual(board.writes, [])
        meta = self.meta()
        self.assertEqual((meta["reason"], meta["result"]["verdict"], meta["sent"]),
                         ("passive_state_missed", "not_sent", []))
        self.assertIn("reset the board by hand", output)
        # A ROM banner alone (the line never came, or came at another baud) is no passive state either.
        self.output = self.base / "rom-only"
        board = self.board(boot=ROM)
        self.assertEqual(self.run_main(Backend(board), "--boot-seconds", "1")[0], 2)
        self.assertEqual(board.writes, [])
        self.assertIn("ESP-ROM:esp32s2-rc4-20191025", self.meta()["boot_lines"])

    def test_bench_image_or_other_state_gets_no_g(self):
        write_record(self.artifact)
        # The ready line follows any boot banner, so a realistic case still holds it; it is never reached.
        cases = {"bench_image": RETAINED_BOOT,
                 "not_passive": ROM + b"MotorControl-CAN: compile fixture; CAN disabled, configuration required.\n" +
                 ready_line(),
                 "smoke_output_before_trigger": PASSIVE_BOOT.replace(smoke.PASSIVE.encode("ascii"),
                                                                     image_line().strip().encode("ascii"))}
        for number, (reason, data) in enumerate(cases.items()):
            self.output = self.base / ("state%d" % number)
            board = self.board(boot=data)
            code, _ = self.run_main(Backend(board))
            self.assertEqual(code, 2, reason)
            self.assertEqual(board.writes, [], reason)
            self.assertEqual(self.meta()["reason"], reason)

    def test_output_during_the_quiet_interval_gets_no_g(self):
        write_record(self.artifact)
        meta, _ = self.refused_unsent("output_after_ready", later=[
            (0.2, b"MotorControl-CAN: clock fault; controller inhibited.\n")])
        self.assertEqual(meta["after_ready"], ["MotorControl-CAN: clock fault; controller inhibited."])

    def test_usb_identity_is_checked_before_anything_is_created(self):
        write_record(self.artifact)
        for ports in ([Item(serial="OTHER")], [Item(device="COM7")], [], [Item(vid=0x1234)], [Item(pid=0x1001)],
                      [Item(), Item()]):
            backend = Backend(self.board(), ports=ports)
            code, output = self.run_main(backend)
            self.assertEqual(code, 2, ports)
            self.assertEqual(backend.opened, [])
            self.assertFalse(self.output.exists())
            self.assertIn("REFUSED", output)

    def test_only_the_recorded_fixture_serial(self):
        write_record(self.artifact, usb={"usb_serial": "OTHER"})
        backend = Backend(self.board(), ports=[Item(serial="OTHER")])
        out = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
            code = smoke.main(["--port", PORT, "--serial", "OTHER", "--artifact", str(self.artifact),
                               "--output", str(self.output)], backend, self.clock)
        self.assertEqual(code, 2)
        self.assertEqual(backend.opened, [])
        self.assertIn("recorded fixture", out.getvalue())

    def test_build_record_is_checked_before_enumeration(self):
        sdkconfig = flash_test.resolved_sdkconfig(flash_test.SMOKE)
        cases = {"s2-bench": {"app": flash.app_record("s2-bench")},
                 "not flashed": {"status": "built"},
                 "target": {"target": "esp32s3"},
                 "ELF SHA-256": {"image_sha256": {"sdkconfig": "0" * 64}},
                 "flashed through": {"usb": {"usb_serial": "OTHER"}},
                 "SDK version": {"sdk_version": None},
                 "smoke hook": {"sdkconfig": sdkconfig.replace("CONFIG_MOTORCONTROLCAN_SMOKE_HOOK=y\n", "")},
                 "distinct": {"sdkconfig": sdkconfig.replace("CONFIG_MOTORCONTROLCAN_SMOKE_NODE_B=2",
                                                             "CONFIG_MOTORCONTROLCAN_SMOKE_NODE_B=1")}}
        for number, (text, changes) in enumerate(cases.items()):
            artifact = write_record(self.base / ("record%d" % number), **changes)
            code, output = self.run_main(Untouchable(), artifact=artifact)
            self.assertEqual(code, 2, text)
            self.assertIn(text, output)
            self.assertFalse(self.output.exists())
        # A retained sdkconfig that is not the one the record hashed.
        artifact = write_record(self.base / "edited")
        (artifact / "sdkconfig").write_text("CONFIG_MOTORCONTROLCAN_SMOKE_HOOK=y\n", encoding="utf-8")
        code, output = self.run_main(Untouchable(), artifact=artifact)
        self.assertEqual(code, 2)
        self.assertIn("does not match", output)
        code, output = self.run_main(Untouchable(), artifact=self.base / "absent")
        self.assertEqual(code, 2)

    def test_existing_output_is_refused_before_opening(self):
        write_record(self.artifact)
        self.output.mkdir()
        backend = Backend(self.board())
        code, output = self.run_main(backend)
        self.assertEqual(code, 2)
        self.assertEqual(backend.opened, [])
        self.assertIn("exists", output)

    def test_port_that_cannot_open_is_retained_with_nothing_sent(self):
        write_record(self.artifact)
        code, _ = self.run_main(Backend(OSError("access denied")))
        self.assertEqual(code, 2)
        meta = self.meta()
        self.assertEqual((meta["status"], meta["error"], meta["sent"]), ("failed", "access denied", []))
        self.assertEqual((self.output / "rx.bin").read_bytes(), b"")

    def test_incomplete_trigger_write_is_uncertain(self):
        write_record(self.artifact)
        board = self.board()
        board.write = lambda data: board.writes.append(bytes(data)) or 0
        code, output = self.run_main(Backend(board))
        self.assertEqual(code, 1, "the byte may have reached the image")
        self.assertEqual(board.writes, [b"G"])
        meta = self.meta()
        self.assertEqual((meta["trigger_write"], meta["result"]["verdict"]), ("attempted", "uncertain"))

    def test_interrupt_ends_the_capture_and_sends_nothing(self):
        write_record(self.artifact)
        board = self.board(answer=image_line().encode("ascii"))
        reads = []

        def interrupted(count):
            reads.append(count)
            if board.writes:
                raise KeyboardInterrupt
            return Board.read(board, count)
        board.read = interrupted
        code, _ = self.run_main(Backend(board))
        self.assertEqual(code, 1)
        self.assertEqual(board.writes, [b"G"])
        self.assertTrue(board.closed)
        meta = self.meta()
        self.assertEqual((meta["status"], meta["reason"], meta["result"]["verdict"]),
                         ("interrupted", "interrupted", "incomplete"))

    def test_dry_run_touches_nothing(self):
        write_record(self.artifact)
        out = io.StringIO()
        with contextlib.redirect_stdout(out), mock.patch.dict(sys.modules, {"serial": None, "serial.tools": None}):
            code = smoke.main(["--port", PORT, "--serial", SERIAL, "--artifact", str(self.artifact), "--dry-run"],
                              Untouchable(), self.clock)
        self.assertEqual(code, 0)
        text = out.getvalue()
        for expected in ("DRY RUN", "Verdict: INCOMPLETE", ELF, smoke.PASSIVE, "send once: b'G'",
                         "SMOKE ready elf=%s idf=5.5.5 project=motorcontrol_can_probe" % ELF,
                         "DTR and RTS deasserted", "nodes 1 and 2", "ignored=0"):
            self.assertIn(expected, text)
        self.assertFalse(self.output.exists())
        self.assertEqual(self.clock.seconds, 0.0, "no capture time passed")

    def test_dry_run_reports_a_refused_record(self):
        write_record(self.artifact, status="build_invalid")
        code, output = self.run_main(Untouchable(), "--dry-run")
        self.assertEqual(code, 2)
        self.assertIn("not flashed", output)

    def test_arguments(self):
        write_record(self.artifact)
        with contextlib.redirect_stderr(io.StringIO()):
            for argv in ([], ["--port", PORT, "--serial", SERIAL, "--artifact", "x"],
                         ["--port", PORT, "--serial", SERIAL, "--output", "y"],
                         ["--port", PORT, "--serial", SERIAL, "--artifact", "x", "--output", "y", "--seconds", "0"],
                         ["--port", PORT, "--serial", SERIAL, "--artifact", "x", "--output", "y", "--seconds", "500"],
                         ["--port", PORT, "--serial", SERIAL, "--artifact", "x", "--output", "y",
                          "--boot-seconds", "0.5"]):
                with self.assertRaises(SystemExit, msg=str(argv)):
                    smoke.arguments(argv)

    def test_the_runner_writes_only_one_byte(self):
        source = Path(smoke.__file__).read_text(encoding="utf-8")
        self.assertEqual(source.count(".send("), 1)
        self.assertEqual(source.count(".write("), 0)
        self.assertNotIn("dtr = True", source)  # IO0 is never pulled low: no download mode.
        self.assertEqual(source.count("rts = True"), 1)  # Only the one requested EN pulse.

    def test_reset_en_pulses_once_and_catches_the_boot_lines(self):
        # 2026-11-07: the user asked for the reset through the ESP-Prog-2 instead of the EN button.
        write_record(self.artifact)
        board = self.board(boots_on_reset=True)
        code, _ = self.run_main(Backend(board), "--reset-en")
        self.assertEqual(code, 0)
        self.assertEqual(board.signals, [("rts", True), ("dtr", False), ("rts", False), ("dtr", False)])
        self.assertEqual(board.writes, [smoke.TRIGGER])
        meta = json.loads((self.output / "request.json").read_text(encoding="utf-8"))
        self.assertEqual(meta["reset"], "en_pulse")
        self.assertEqual(meta["en_pulse"]["via"], "RTS, DTR deasserted")

    def test_without_reset_en_a_booted_board_is_missed_and_no_line_moves(self):
        write_record(self.artifact)
        board = self.board(boots_on_reset=True)
        code, output = self.run_main(Backend(board))
        self.assertEqual(code, 2)
        self.assertEqual((board.signals, board.writes), ([], []))
        self.assertIn("--reset-en", output)


class Formats(unittest.TestCase):
    """The patterns against the exact printf formats of examples/common/FirmwareSmoke.cpp."""

    def test_formats_follow_the_firmware_source(self):
        source = (ROOT / "examples/common/FirmwareSmoke.cpp").read_text(encoding="utf-8")
        for text in ('"SMOKE image %s start=%s running=%u transceiver=%s check=%u\\n"',
                     '"SMOKE done; no further requests; ignored=%lu\\n"',
                     '"SMOKE node=%u identity=refused; no request\\n"',
                     '"SMOKE node=%u identity=failed state=%u outcome=%u uncertain=%u\\n"',
                     '"SMOKE node=%u stopped: controller not running failure=%u\\n"',
                     '"SMOKE refused: fixture nodes/budgets/identity not configured; no start\\n"',
                     '"SMOKE output-format-error\\n"',
                     '"SMOKE node=%u identity=ok device=%08lx hw=%04x/%u sw=%04x/%u entries=%u "',
                     '"vendor=%08lx product=%08lx revision=%08lx model=%04x firmware=%04x skew=%llu\\n"'):
            self.assertIn(text, source)
        main = (ROOT / "examples/probe_idf/main/main.cpp").read_text(encoding="utf-8")
        self.assertIn('"elf=%s idf=%s project=%s"', main)
        # The ready line: its print and identity formats are read from the sources and rendered.
        ready = re.search(r'print\("(SMOKE ready %s)\\n", config_\.identity \? config_\.identity : "([^"]+)"\);',
                          source)
        identity = re.search(r'snprintf\(identity, sizeof identity, "([^"]+)"', main)
        self.assertTrue(ready and identity)
        self.assertEqual(ready.group(1) % ready.group(2), smoke.READY_UNCONFIGURED)
        rendered = ready.group(1) % (identity.group(1) % (ELF, "5.5.5", "motorcontrol_can_probe"))
        self.assertEqual(rendered + "\n", ready_line().decode("ascii"))
        self.assertEqual(smoke.READY.fullmatch(rendered).groupdict(),
                         {"elf": ELF, "idf": "5.5.5", "project": "motorcontrol_can_probe"})
        # Printed on the owner task's first turn, after the entry wrote the passive line.
        self.assertLess(source.index("SMOKE ready"), source.index("takeInput"))
        entry = (ROOT / "examples/common/EspProbeMain.cpp").read_text(encoding="utf-8")
        self.assertIn('"%s\\n"' % smoke.PASSIVE, entry)
        header = (ROOT / "examples/common/FirmwareSmoke.h").read_text(encoding="utf-8")
        self.assertIn("TRIGGER = 'G'", header)

    def test_terminal_lines(self):
        for line in ("SMOKE done; no further requests; ignored=0", smoke.REFUSED,
                     image_line(start="failed").strip(), "SMOKE node=2 stopped: controller not running failure=1"):
            self.assertTrue(smoke.terminal(line), line)
        for line in (image_line().strip(), "SMOKE node=1 " + IDENTITY, "SMOKE node=1 identity=refused; no request",
                     smoke.FORMAT_ERROR, smoke.PASSIVE):
            self.assertFalse(smoke.terminal(line), line)


if __name__ == "__main__":
    unittest.main()
