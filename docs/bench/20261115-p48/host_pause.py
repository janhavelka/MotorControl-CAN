#!/usr/bin/env python3
"""Prompt48 finite host-read pause and explicit read-workload withdrawal.

Requires --execute and a new output directory. Uses the existing serial backend,
soak command encoder and evidence parser. One lead owns the port throughout.
No CAN sender, drive writes, reconnect, reset or motor-stop interpretation.
"""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import sys
import time

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "scripts"))
from s2_soak import RealBackend, check_identity, soak_line  # noqa: E402
from s2_evidence import parse_line  # noqa: E402


def main():
    """Capture one 30 s read plan; pause host reads 3 s and request X once at 20 s.

    Exit zero means the end was captured. The ordinary evidence checker and
    campaign review judge settlement, workload and loss separately.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--execute", action="store_true")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not args.execute:
        print("Plan only:30s/200ms/1s full reads, host pause at5s for3s, X at20s; no port access")
        return 0
    backend = RealBackend()
    usb = check_identity("COM15", "94A990398B40", backend.comports())
    args.output.mkdir(parents=True, exist_ok=False)
    trial = int(time.time())
    meta = dict(command="soak", trial=trial, port="COM15", serial="94A990398B40",
                usb=usb, status="running", retry=False, reset=False,
                started_utc=datetime.now(timezone.utc).isoformat(),
                duration_s=30, interval_ms=200, summary_s=1, reads="full",
                host_pause_after_s=5, host_pause_s=3, stop_after_s=20,
                interpretation="Host consumer pause and read scheduling stop only")
    def save_request():
        (args.output / "request.json").write_text(json.dumps(meta, indent=2) + "\n", encoding="utf-8")
    save_request()
    port = None
    pending = bytearray()
    sequence = []
    started = time.monotonic()
    phase, began, paused, stop_sent = "ping", None, False, False
    complete = False
    status = 1
    try:
        port = backend.open("COM15")
        with (args.output / "rx.bin").open("xb") as raw, (args.output / "events.jsonl").open("x", encoding="utf-8") as events:
            def event(kind, **values):
                values.update(event=kind, elapsed_s=time.monotonic()-started,
                              utc=datetime.now(timezone.utc).isoformat())
                events.write(json.dumps(values) + "\n")
                events.flush()

            def send(data):
                event("send_attempt", hex=data.hex())
                count = port.write(data)
                event("send_completed", hex=data.hex(), count=count)
                if count != len(data):
                    raise RuntimeError("Short write; no retry")

            send(b"p")
            while not complete and time.monotonic() - started < 55:
                now = time.monotonic()
                if began is not None and not paused and now - began >= 5:
                    paused = True
                    event("host_read_pause_begin")
                    time.sleep(3)
                    event("host_read_pause_end", waiting=port.in_waiting)
                if began is not None and not stop_sent and time.monotonic() - began >= 20:
                    send(b"X")
                    stop_sent = True
                chunk = port.read(max(1, min(port.in_waiting, 4096)))
                if not chunk:
                    continue
                raw.write(chunk)
                raw.flush()
                event("received", count=len(chunk))
                pending.extend(chunk)
                while b"\n" in pending:
                    line, _, tail = pending.partition(b"\n")
                    pending[:] = tail
                    text = line.rstrip(b"\r").decode("utf-8", errors="replace")
                    sequence.append(text)
                    record = parse_line(len(sequence), text)
                    if phase == "ping" and record.kind == "PONG" and record.status == "parsed":
                        v = record.values
                        if (v["base"] != "bf8a0d9518c6b42ff06d4f29516bf82ee66abf79" or
                                v["elf"] != "9ca7701bb4f20efe9c383fd8fe4929e0314980bd2f2f1899652abcaa687b0399" or
                                not v["active"] or not v["running"] or v["soak"]):
                            raise RuntimeError("Image or inactive-work prerequisite mismatch; no soak sent")
                        meta["image"] = {key: v[key] for key in ("base", "elf", "sdk", "target")}
                        request = soak_line(trial, 30, 200, 1, 3)
                        meta["tx_hex"] = request.hex()
                        save_request()
                        send(request)
                        phase = "begin"
                    elif phase == "begin" and record.kind == "SOAK_BEGIN":
                        if record.status != "parsed" or record.values["trial"] != trial:
                            raise RuntimeError("Unexpected soak begin")
                        began = time.monotonic()
                        phase = "active"
                    elif record.kind == "REFUSED":
                        raise RuntimeError("Firmware refused: " + text)
                    elif record.kind == "SOAK_END":
                        if record.status != "parsed" or record.values["trial"] != trial:
                            raise RuntimeError("Unexpected soak end")
                        meta["end"] = record.values
                        meta["status"] = "captured"
                        meta["reason"] = "end:" + record.values["reason"]
                        status = 0
                        complete = True
                        break
            if not complete:
                raise TimeoutError("Finite host deadline; no reconnect or resend")
    except BaseException as error:
        meta.update(status="failed", reason=type(error).__name__ + ": " + str(error))
        raise
    finally:
        if port is not None:
            try:
                port.close()
            except Exception as error:
                meta.update(status="failed", close_error=str(error))
                status = 1
        meta["stop_sent"] = stop_sent
        meta["pause_performed"] = paused
        meta["ended_utc"] = datetime.now(timezone.utc).isoformat()
        save_request()
    return status


if __name__ == "__main__":
    sys.exit(main())
