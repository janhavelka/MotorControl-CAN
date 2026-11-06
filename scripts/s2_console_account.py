#!/usr/bin/env python3
"""Compare the S2's own count of console bytes with what reached the host, offline.

From the bench image after 2026-11-06 on, each SOAK_CON line carries two counters taken when
the line was formatted: uartBytes, the bytes the UART FIFO accepted since boot (modulo 2^32),
and queued, the bytes still waiting in the console queue. Their sum is the number of bytes
admitted to the console before this line, so it is this line's position in the S2's output
stream. Between two SOAK_CON lines the S2 admitted (sum2 - sum1) bytes; the host received the
bytes between the two lines' starts in rx.bin. When the host holds fewer, bytes the S2 had
admitted were lost, and every byte before the second line had been accepted by the UART FIFO
(the FIFO sends in order), so the loss happened after `uart_tx_chars` accepted them: on the
UART line, in the bridge, in USB or in the host driver. The counters cannot tell those apart;
an independent capture at GPIO43 with scripts/s2_uart_compare.py can.

A window is not compared when it holds a boot line (the counters restart), when either line
lacks the counters (older images), or when the S2's count exceeds what the wire could carry in
the window's S2 time plus the FIFO and queue sizes (a corrupted value or a restart). The
position is exact only if no other writer admitted output between this line's formatting and
its enqueue. A soak line that waits for room can let such output in first; the host then holds
those bytes before the line, which lengthens the window the line ends and shortens the one it
starts. Windows next to a line preceded by a non-soak line are marked for that reason.

This tool opens no port and only reads its input, streaming it in bounded chunks: memory does
not grow with the capture except for the windows it reports, which --max-report bounds.

Exit status: 0 every compared window balances; 1 at least one does not; 2 nothing could be
compared (no counters, unreadable input) or a usage error.
"""
import argparse
import json
import lzma
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import s2_evidence  # noqa: E402

CHUNK = 1 << 16
WRAP = 1 << 32
FIFO_BYTES = 128      # ESP32-S2 UART0 TX FIFO: acceptance can lead the wire by this much.
QUEUE_BYTES = 512     # ProbePlatform::OUTPUT_CAPACITY.
LINE_LIMIT = 4096     # A longer run without LF is no console line; it is skipped, not held.
BOOT_HEADS = (b"S2_BENCH ready", b"rst:0x")


def open_capture(path):
    """Binary reader for rx.bin, or for an .xz container of it (decompressed as a stream)."""
    path = pathlib.Path(path)
    return lzma.open(path, "rb") if path.suffix == ".xz" else path.open("rb")


def lines(stream):
    """Yield (offset, line bytes without LF) in bounded memory. Overlong runs are truncated to
    LINE_LIMIT bytes for classification; offsets stay exact."""
    offset, held, held_offset = 0, bytearray(), 0
    while True:
        chunk = stream.read(CHUNK)
        if not chunk:
            break
        start = 0
        while True:
            end = chunk.find(b"\n", start)
            piece = chunk[start:] if end < 0 else chunk[start:end]
            held += piece[:LINE_LIMIT - len(held)]
            if end < 0:
                break
            yield held_offset, bytes(held)
            held_offset = offset + end + 1
            held = bytearray()
            start = end + 1
        offset += len(chunk)
    if held or held_offset < offset:
        yield held_offset, None  # Unterminated tail: never trusted as a line.


def counters(line):
    """(n, t_us, position) of a parsed SOAK_CON line with both counters, else None."""
    record = s2_evidence.parse_line(0, line.rstrip(b"\r").decode("utf-8", errors="replace"))
    if record.status != "parsed" or record.kind != "SOAK_CON":
        return None
    values = record.values
    if "uartBytes" not in values or "queued" not in values:
        return "old"
    return values["n"], values["t"], (values["uartBytes"] + values["queued"]) % WRAP


def account(stream, baud=115200, max_report=1000):
    """Return the accounting summary of one capture as a JSON-ready dict."""
    wire_per_s = baud / 10.0  # 8N1: ten bit times per byte.
    summary = {"baud": baud, "soak_con": 0, "without_counters": 0, "windows": 0, "compared": 0,
               "balanced": 0, "short": 0, "long": 0, "not_compared": {"boot": 0, "implausible": 0},
               "s2_bytes": 0, "host_bytes": 0, "missing_bytes": 0, "extra_bytes": 0,
               "reported": [], "report_truncated": False}
    previous = None      # (n, t, position, offset) of the last SOAK_CON with counters.
    boot = False         # A boot line since `previous`.
    before_soak = True   # The line before the current one was a soak line (or none).
    for offset, line in lines(stream):
        if line is None:
            break
        if line.startswith(BOOT_HEADS):
            boot = True
        if line.startswith(b"SOAK_CON "):
            summary["soak_con"] += 1
            found = counters(line)
            if found == "old":
                summary["without_counters"] += 1
                previous = None
            elif found:
                n, t, position = found
                if previous is not None:
                    summary["windows"] += 1
                    pn, pt, pposition, poffset, pmarked = previous
                    s2 = (position - pposition) % WRAP
                    host = offset - poffset
                    seconds = max(t - pt, 0) / 1e6
                    window = {"from_n": pn, "to_n": n, "from_t_us": pt, "to_t_us": t,
                              "host_offset": poffset, "host_bytes": host, "s2_bytes": s2,
                              "difference": host - s2,
                              "interleaving_possible": pmarked or not before_soak}
                    if boot:
                        summary["not_compared"]["boot"] += 1
                        window["reason"] = "boot line inside the window"
                    elif t < pt or s2 > seconds * wire_per_s * 1.01 + FIFO_BYTES + QUEUE_BYTES:
                        summary["not_compared"]["implausible"] += 1
                        window["reason"] = "S2 count exceeds the wire's capacity in this window"
                    else:
                        summary["compared"] += 1
                        summary["s2_bytes"] += s2
                        summary["host_bytes"] += host
                        if host == s2:
                            summary["balanced"] += 1
                            window = None
                        elif host < s2:
                            summary["short"] += 1
                            summary["missing_bytes"] += s2 - host
                        else:
                            summary["long"] += 1
                            summary["extra_bytes"] += host - s2
                    if window is not None:
                        if len(summary["reported"]) < max_report:
                            summary["reported"].append(window)
                        else:
                            summary["report_truncated"] = True
                previous = (n, t, position, offset, not before_soak)
                boot = False
        before_soak = line.startswith(b"SOAK_") or not line.strip(b"\r")
    return summary


def text(summary):
    out = ["SOAK_CON lines %d (%d without counters); windows %d, compared %d: balanced %d, host short %d "
           "(%d bytes), host long %d (%d bytes); not compared: %d boot, %d implausible"
           % (summary["soak_con"], summary["without_counters"], summary["windows"], summary["compared"],
              summary["balanced"], summary["short"], summary["missing_bytes"], summary["long"],
              summary["extra_bytes"], summary["not_compared"]["boot"], summary["not_compared"]["implausible"])]
    for w in summary["reported"]:
        out.append("  n %d..%d host offset %d: host %d, S2 %d, difference %+d%s%s"
                   % (w["from_n"], w["to_n"], w["host_offset"], w["host_bytes"], w["s2_bytes"], w["difference"],
                      "; interleaving possible" if w["interleaving_possible"] else "",
                      "; not compared: " + w["reason"] if "reason" in w else ""))
    if summary["report_truncated"]:
        out.append("  (more windows not listed: --max-report)")
    return "\n".join(out)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("capture", help="host rx.bin, or rx.bin.xz")
    parser.add_argument("--baud", type=int, default=115200, help="console baud rate, 8N1 (default 115200)")
    parser.add_argument("--max-report", type=int, default=1000, help="windows listed at most (default 1000)")
    parser.add_argument("--json", help="also write the summary as JSON to this new file")
    args = parser.parse_args(argv)
    if args.baud <= 0 or args.max_report < 0:
        parser.error("--baud must be positive and --max-report not negative")
    try:
        with open_capture(args.capture) as stream:
            summary = account(stream, args.baud, args.max_report)
    except (OSError, lzma.LZMAError, EOFError) as error:
        print("cannot read %s: %s" % (args.capture, error), file=sys.stderr)
        return 2
    print(text(summary))
    if args.json:
        with open(args.json, "x", encoding="utf-8") as handle:
            json.dump(summary, handle, indent=1)
    if not summary["compared"]:
        print("nothing compared: the capture has no pair of SOAK_CON lines with counters", file=sys.stderr)
        return 2
    return 0 if summary["compared"] == summary["balanced"] else 1


if __name__ == "__main__":
    sys.exit(main())
