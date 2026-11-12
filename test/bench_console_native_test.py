#!/usr/bin/env python3
"""Consume the real native console producer through Console, not copied success JSON."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from bench_probe import Console  # noqa: E402

BINARY = sys.argv[1] if len(sys.argv) == 2 else None


class NativeConsole(unittest.TestCase):
    def exercise(self, command, count, success):
        self.assertIsNotNone(BINARY, "CTest must supply the actual native_probe executable")
        commands = ["version", "profile list", "caps", "config", "useaddr 5", "config", command]
        if count:
            commands += ["result 1", "release 1"]
        with tempfile.TemporaryDirectory(prefix="CAN Python native ") as directory:
            script = Path(directory) / "input.txt"
            script.write_text("".join(f"@{i} {text}\n" for i, text in enumerate(commands, 1)), encoding="ascii")
            process = subprocess.run([BINARY, "--console-fake", str(script)], check=True,
                                     stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=20)
        self.assertLess(len(process.stdout), 65536)
        replies = {}
        for line in process.stdout.splitlines(keepends=True):
            record = json.loads(line)
            replies.setdefault(record["id"], bytearray()).extend(line)

        class Stream:
            def __init__(self):
                self.buffer = bytearray()
                self.writes = []

            def write(inner, data):
                expected = f"@{len(inner.writes) + 1} {commands[len(inner.writes)]}\n".encode("ascii")
                self.assertEqual(data, expected)
                inner.writes.append(bytes(data))
                inner.buffer.extend(replies[len(inner.writes)])
                return len(data)

            def read(inner, size):
                chunk = bytes(inner.buffer[:min(size, 19)])
                del inner.buffer[:len(chunk)]
                return chunk

        stream = Stream()
        console = Console(stream, first_id=1, host_epoch="native-producer")
        version = json.loads((Path(__file__).resolve().parents[1] / "library.json").read_text(encoding="utf-8"))["version"]
        console.identify(expected_version=version)
        self.assertTrue(console.command("useaddr 5")["ok"])
        console.check_config(console.command("config"), node=5, can_bitrate=500000)
        handle = console.begin(command)
        terminal = console.wait(handle)
        self.assertEqual(terminal["ok"], success)
        self.assertEqual(sum(row["type"] == "value" for row in handle.records), count)
        summary = console.summary(handle)
        self.assertEqual(summary["evidence_kind"], "host-tested")
        self.assertFalse(summary["provenance"]["wire_target_proven"])
        if count:
            lookup = console.command("result 1")
            self.assertEqual(lookup["outcome"], terminal["outcome"])
            self.assertEqual(lookup["command_id"], handle.id)
            self.assertTrue(console.release(handle)["ok"])
            self.assertTrue(handle.released)
            self.assertEqual(console.summary(handle), summary)
        else:
            self.assertEqual(summary["classification"], "local_refusal")
        if count and not success:
            self.assertEqual(summary["classification"], "unknown_after_admission")
            self.assertEqual(terminal["outcome"], "TRANSPORT_ERROR")
        self.assertEqual(len(stream.writes), len(commands))

    def test_probe(self):
        self.exercise("probe 1000", 1, True)

    def test_identity(self):
        self.exercise("read identity 1000", 9, True)

    def test_state(self):
        for block, count in (("status", 2), ("monitors", 3), ("feedback", 2)):
            with self.subTest(block=block):
                # The native demonstration deliberately has identity-only replies.
                # Its state acquisition failure must remain retained/uncertain.
                self.exercise(f"read state {block} 1000", count, False)

    def test_config_remains_q20_gated(self):
        self.exercise("read config 1000", 0, False)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
