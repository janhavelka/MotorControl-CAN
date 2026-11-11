#!/usr/bin/env python3
"""s2_bench_console.py against a fake board and clock: framing, step answers, refusals; no port, no pyserial."""

from pathlib import Path
import sys
import tempfile
import unittest
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parent))
import s2_bench_console as tool  # noqa: E402

HEAD = '{"schema":"motorcontrolcan.console/1","protocol":3,"profile":"lichuan_c","type":"%s","id":%d,"command":"%s"'


def record(kind, ident, command, body):
    return (HEAD % (kind, ident, command) + body + "}\n").encode("ascii")


class Port:
    """Answers each complete step with scripted bytes, as the bench image would print them."""

    def __init__(self, answers):
        self.answers, self.queue, self.sent, self.closed = answers, b"", [], False
        self.partial = b""

    def write(self, data):
        self.sent.append(data)
        self.partial += data
        # A single bench byte or a whole line is one step.
        if len(data) == 1 and data != b"\n":
            key, self.partial = data, b""
        elif self.partial.endswith(b"\n"):
            key, self.partial = self.partial, b""
        else:
            return len(data)
        self.queue += self.answers.get(key, b"")
        return len(data)

    def read(self, size):
        data, self.queue = self.queue[:size], self.queue[size:]
        return data

    def close(self):
        self.closed = True


class Item:
    def __init__(self, device, serial, vid=0x303A, pid=0x1002):
        self.device, self.serial_number, self.vid, self.pid = device, serial, vid, pid


class Backend:
    def __init__(self, port, serial="94A990398B40"):
        self.port, self.serial, self.opened = port, serial, None

    def comports(self):
        return [Item("COM15", self.serial)]

    def open(self, device):
        self.opened = device
        return self.port


class Clock:
    def __init__(self):
        self.now = 0.0

    def __call__(self):
        self.now += 0.01
        return self.now


ARM = "TY1,7,1,0,100,19,1,4096,4096,1,32768,32768,1,262546,18,52,768,34309,65537,134,291,1,0,2000,3000,100"


class Tests(unittest.TestCase):
    def run_tool(self, steps, answers, **extra):
        port = Port(answers)
        lines = []
        with tempfile.TemporaryDirectory() as directory:
            raw = Path(directory) / "rx.bin"
            argv = ["--output", str(raw), "--timeout", "2", "--terminal-timeout", "3"]
            for step in steps:
                argv += ["--send", step]
            code = tool.main(argv + extra.get("args", []), Backend(port, extra.get("serial", "94A990398B40")),
                             Clock(), lines.append)
            transcript = raw.read_bytes() if raw.exists() else b""
        return code, port, lines, transcript

    def test_route_lines_are_signed_as_the_runner_signs_them(self):
        data, answer = tool.frame("TA1,5")
        self.assertEqual(data, ("TA1,5,K%08x\n" % zlib.crc32(b"TA1,5")).encode("ascii"))
        self.assertEqual(answer, ("route", False))
        self.assertEqual(tool.frame(ARM)[1], ("route", True))
        self.assertEqual(tool.frame("@12 read state")[0], b"@12 read state\n")
        self.assertEqual(tool.frame("o"), (b"o", ("byte", "o")))
        for bad in ("@0 version", "@x version", "TA1,5,K00000000", "q", "", "@1 read\x01", "T" + "1" * 260):
            with self.assertRaises(tool.Failure):
                tool.frame(bad)

    def test_dry_run_opens_nothing(self):
        lines = []
        self.assertEqual(tool.main(["--dry-run", "--send", "o", "--send", "@1 version"], None, Clock(), lines.append), 0)
        self.assertEqual(lines, [repr(b"o"), repr(b"@1 version\n")])

    def test_arming_then_console_move_waits_for_each_answer(self):
        signed = (tool.sign_line(ARM) + "\n").encode("ascii")
        answers = {
            b"o": b"CONSOLE open slot0=node1 slot1=node2 bitrate=1000000; routes configured, nothing sent or selected\n",
            b"@1 host 0\n": record("reply", 1, "host", ',"ok":true,"status":"OK","wire_work":false'),
            signed: b"TRIAL admitted route=arm-move node=1 trial=7 motion=2 case=0; finite typed route, no retry\n"
                    b"TRIAL id=7 node=1 route=arm-move failure=0\n"
                    b"CONSOLE armed node=1 trial=7 route=arm-move relative=1 witness=1 scale=0 origin=0; x\n",
            b"@2 move relative 16 steps native 1 configured\n":
                record("reply", 2, "move", ',"ok":true,"status":"OK","result":"accepted","operation_id":1') +
                b"END trial terminals=27 traffic=60 tail=0 tailLost=0 lost=0; r=release\n" +
                record("value", 2, "move", ',"operation_id":1') +
                record("terminal", 2, "move", ',"ok":true,"status":"OK","outcome":"OBSERVED"'),
        }
        code, port, lines, transcript = self.run_tool(
            ["o", "@1 host 0", ARM, "@2 move relative 16 steps native 1 configured"], answers)
        self.assertEqual(code, 0, lines)
        self.assertEqual(port.sent, [b"o", b"@1 host 0\n", signed, b"@2 move relative 16 steps native 1 configured\n"])
        self.assertTrue(port.closed)
        self.assertIn(b"CONSOLE armed node=1", transcript)
        self.assertTrue(transcript.endswith(b'"outcome":"OBSERVED"}\n'))

    def test_refusals_end_the_run_and_send_nothing_more(self):
        refused = {b"@3 read state\n": record("reply", 3, "read", ',"ok":false,"status":"INVALID_CONFIG","detail":0,'
                                                              '"error":"bench_wire_work_active"')}
        code, port, lines, _ = self.run_tool(["@3 read state", "@4 version"], refused)
        self.assertEqual(code, 2)
        self.assertEqual(port.sent, [b"@3 read state\n"])
        self.assertTrue(lines[-1].startswith("STOPPED: refused"))
        signed = (tool.sign_line(ARM) + "\n").encode("ascii")
        arm_refused = {signed: b"TRIAL admitted route=arm-move node=1 trial=7\n"
                               b"CONSOLE arm refused node=1 trial=7 reason=route; nothing named\n"}
        code, port, _, _ = self.run_tool([ARM, "@5 moveby 16"], arm_refused)
        self.assertEqual((code, len(port.sent)), (2, 1))
        bench = {b"r": b"TRIAL release refused\n"}
        self.assertEqual(self.run_tool(["r", "p"], bench)[0], 2)
        terminal = {b"@6 stop fast\n": record("reply", 6, "stop", ',"ok":true,"result":"accepted","operation_id":3') +
                                      record("terminal", 6, "stop", ',"ok":false,"outcome":"DEADLINE"')}
        self.assertEqual(self.run_tool(["@6 stop fast"], terminal)[0], 2)

    def test_other_lines_and_other_ids_do_not_answer_a_step(self):
        answers = {b"@7 version\n": b"TRAFFIC k=1 p=2 id=581\n" + record("reply", 70, "version", ',"ok":true') +
                                    b'{"schema":"other"}\n' + record("reply", 7, "version", ',"ok":true')}
        self.assertEqual(self.run_tool(["@7 version"], answers)[0], 0)
        self.assertEqual(self.run_tool(["@8 version"], {})[0], 1) # Nothing answers: a timeout.
        self.assertEqual(self.run_tool(["@9 version"], {}, serial="0000")[0], 1) # Not the fixture.


if __name__ == "__main__":
    unittest.main()
