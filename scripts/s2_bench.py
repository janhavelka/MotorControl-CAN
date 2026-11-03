#!/usr/bin/env python3
"""One bounded S2 bench-console request; preserves raw bytes and never resets/retries."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import time

COMMANDS = {"ping": b"p", "start": b"c", "stats": b"s",
            "identity1": b"1", "identity2": b"2", "raw1": b"3", "raw2": b"4",
            "dump": b"d", "release": b"r", "network": b"n",
            "heartbeat1": b"h", "heartbeat2": b"j", "preop1": b"q", "preop2": b"w", "tpdo1": b"m",
            "motion-readiness1": b"v", "motion-readiness2": b"b", "soak-stop": b"X"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--serial", required=True, help="Exact ESP-Prog2 USB serial number")
    parser.add_argument("--command", choices=COMMANDS, required=True)
    parser.add_argument("--seconds", type=float, default=4)
    parser.add_argument("--output", type=Path, required=True, help="New evidence directory")
    args = parser.parse_args()
    if not 1 <= args.seconds <= 30:
        parser.error("seconds must be 1..30")
    import serial
    from serial.tools import list_ports
    ports = [p for p in list_ports.comports() if p.device.lower() == args.port.lower()]
    if len(ports) != 1 or ports[0].serial_number != args.serial or \
            (ports[0].vid, ports[0].pid) != (0x303A, 0x1002):
        parser.error("port is not the uniquely selected ESP-Prog2")
    args.output.mkdir(parents=True, exist_ok=False)
    meta = {"started_utc": datetime.now(timezone.utc).isoformat(), "port": args.port,
            "serial": args.serial, "baud": 115200, "command": args.command,
            "tx_hex": COMMANDS[args.command].hex(), "seconds": args.seconds,
            "reset": False, "retry": False}
    data = bytearray()
    try:
        with serial.Serial(port=None, baudrate=115200, timeout=0.05, write_timeout=1) as port:
            port.dtr = port.rts = False
            port.port = args.port
            port.open()
            # Retain any already waiting output; never flush unseen evidence.
            data.extend(port.read(port.in_waiting))
            meta["prefix_bytes"] = len(data)
            if port.write(COMMANDS[args.command]) != 1:
                raise RuntimeError("short command write; do not replay")
            until = time.monotonic() + args.seconds
            while time.monotonic() < until:
                data.extend(port.read(min(max(port.in_waiting, 1), 4096)))
                if len(data) > 131072:
                    raise RuntimeError("bounded capture capacity exceeded")
        meta["status"] = "captured"
    except Exception as error:
        meta["status"] = "failed"
        meta["error"] = str(error)
        raise
    finally:
        (args.output / "rx.bin").write_bytes(data)
        (args.output / "rx.txt").write_bytes(data.decode("utf8", errors="replace").encode("utf8"))
        (args.output / "request.json").write_text(json.dumps(meta, indent=2) + "\n", encoding="utf8")
    print(data.decode("utf8", errors="replace"), end="")
    print("\nEvidence:", args.output)


if __name__ == "__main__":
    main()
