#!/usr/bin/env python3
"""One finite smoke check of the shared S2 image that s2_flash.py --app probe-idf-optak-smoke wrote.

The image (examples/probe_idf with sdkconfig.optak_s2 and sdkconfig.optak_s2_smoke, see
examples/common/FirmwareSmoke.h) prints two lines once per boot, then nothing until its trigger:

  MotorControl-CAN: board accepted; CAN inactive until explicit start.
  SMOKE ready elf=<64 hex> idf=<version> project=<name>    (or: SMOKE ready identity=unconfigured)

One byte G makes it print its identity again with the explicit start result, run one typed
identity read on each configured node, print each result and end with "SMOKE done". This runner:

1. reads the retained upload record (--artifact) before anything else: it must be a flashed
   probe-idf-optak-smoke record made through this USB serial, with the ELF SHA-256, project, SDK
   version and a retained sdkconfig whose hash the record holds and which names the two nodes;
2. requires the recorded ESP-Prog-2 uniquely on --port (s2_bench.py's rule) and opens it with DTR
   and RTS deasserted before opening, so that opening drives neither EN nor BOOT;
3. captures for at most --boot-seconds and requires exactly that passive line, then within
   READY_S the ready line as the very next line, with elf=, idf= and project= equal to the record,
   then SETTLE_S with no further byte. Anything else sends nothing: a missed passive line, another
   first line (the bench image's S2_BENCH ready, a fixture or failure banner, a SMOKE line), a
   missing, malformed or unconfigured ready line, an identity mismatch or more output. The lines
   are printed once per boot: when the board booted before the port opened (as after an upload)
   the run records passive_state_missed. Start a new run and reset the board while it waits: by
   hand (EN button or power), or with --reset-en, one EN pulse through the ESP-Prog-2's RTS line
   after the port is open, with DTR held deasserted so the chip boots its application (the hard
   reset esptool performs after an upload). Without --reset-en, DTR/RTS never reset it;
4. sends exactly one byte G, once, and captures until a terminal SMOKE line (done, refused, a
   failed start or a stopped node) or --seconds, then TAIL_S more;
5. writes rx.bin (authoritative raw bytes), rx.txt and request.json to a new --output directory,
   compares the image line's elf=, idf= and project= with the record again, requires start=ok
   running=1 transceiver=normal-requested, identity=ok on each configured node and "ignored=0",
   and reports each node's line or failure.

No second G, no other byte, no retry, and at most the one requested EN pulse. Ctrl-C ends the
capture and sends nothing.
--dry-run reads the record and prints the plan; it enumerates, opens and creates nothing. Exit
status: 0 every check passed; 1 G was sent (or its write did not complete) and a check failed or
the capture ended early; 2 nothing was sent.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

from check_firmware import sdkconfig_values
from s2_bench import BAUD, FIXTURE, Capture, RealBackend, RealClock, Refused, check_identity, repository_state

SMOKE_APP = "probe-idf-optak-smoke"
PASSIVE = "MotorControl-CAN: board accepted; CAN inactive until explicit start."
BENCH_READY = "S2_BENCH ready"  # The special bench image's first line: never sent a G.
TRIGGER = b"G"
# The ready line follows the passive line within a few owner turns of one scheduler tick each;
# this host bound only ends the wait, it is no firmware timing claim.
READY_S = 2.0
SETTLE_S = 0.5  # Quiet time required after the ready line before G.
TAIL_S = 0.5  # Retained after the terminal line; nothing is sent in it.
BOOT_LINES = 64  # Lines before the passive line kept in request.json; rx.bin keeps every byte.
SECONDS = {"boot": (20.0, 1, 120), "smoke": (30.0, 1, 120)}  # (default, low, high)

# The exact formats FirmwareSmoke.cpp prints; the identity text is probe_idf main.cpp's.
READY = re.compile(r"SMOKE ready elf=(?P<elf>[0-9a-f]{64}) idf=(?P<idf>\S+) project=(?P<project>\S+)")
READY_UNCONFIGURED = "SMOKE ready identity=unconfigured"
IMAGE = re.compile(r"SMOKE image elf=(?P<elf>[0-9a-f]{64}) idf=(?P<idf>\S+) project=(?P<project>\S+) "
                   r"start=(?P<start>ok|failed) running=(?P<running>\d+) transceiver=(?P<transceiver>\S+) "
                   r"check=(?P<check>\d+)")
NODE = re.compile(r"SMOKE node=(?P<node>\d+) (?P<rest>.+)")
IDENTITY_OK = re.compile(
    r"identity=ok device=(?P<device>[0-9a-f]{8}) hw=(?P<hw>[0-9a-f]{4})/(?P<hw_width>\d+) "
    r"sw=(?P<sw>[0-9a-f]{4})/(?P<sw_width>\d+) entries=(?P<entries>\d+) vendor=(?P<vendor>[0-9a-f]{8}) "
    r"product=(?P<product>[0-9a-f]{8}) revision=(?P<revision>[0-9a-f]{8}) model=(?P<model>[0-9a-f]{4}) "
    r"firmware=(?P<firmware>[0-9a-f]{4}) skew=(?P<skew>\d+)")
IDENTITY_FAILED = re.compile(r"identity=failed state=(?P<state>\d+) outcome=(?P<outcome>\d+) "
                             r"uncertain=(?P<uncertain>\d+)")
IDENTITY_REFUSED = "identity=refused; no request"
STOPPED = re.compile(r"stopped: controller not running failure=(?P<failure>\d+)")
DONE = re.compile(r"SMOKE done; no further requests; ignored=(?P<ignored>\d+)")
REFUSED = "SMOKE refused: fixture nodes/budgets/identity not configured; no start"
FORMAT_ERROR = "SMOKE output-format-error"


def terminal(line):
    """A line after which the image admits nothing more."""
    if line.startswith(("SMOKE done", "SMOKE refused")):
        return True
    if line.startswith("SMOKE image ") and " start=failed " in line + " ":
        return True
    node = NODE.fullmatch(line)
    return bool(node and node.group("rest").startswith("stopped:"))


def load_record(artifact, serial):
    """The upload this run is compared with; any refusal happens before a port is enumerated."""
    artifact = Path(artifact)
    try:
        record = json.loads((artifact / "build.json").read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        raise Refused("build record %s is unreadable: %s" % (artifact / "build.json", error))
    app = record.get("app") if isinstance(record.get("app"), dict) else {}
    if app.get("name") != SMOKE_APP:
        raise Refused("build record is the %s image, not %s" % (app.get("name") or "s2-bench", SMOKE_APP))
    if record.get("status") != "flashed":
        raise Refused("build record status is %s, not flashed" % record.get("status"))
    if record.get("target") != "esp32s2":
        raise Refused("build record target is %s, not esp32s2" % record.get("target"))
    hashes = record.get("image_sha256") if isinstance(record.get("image_sha256"), dict) else {}
    elf = hashes.get(record.get("app_elf"))
    if not isinstance(elf, str) or not re.fullmatch(r"[0-9a-f]{64}", elf):
        raise Refused("build record holds no ELF SHA-256 for %s" % record.get("app_elf"))
    flashed_through = (record.get("usb") or {}).get("usb_serial")
    if flashed_through != serial:
        raise Refused("the record was flashed through USB serial %s, not %s" % (flashed_through, serial))
    try:
        sdkconfig = (artifact / "sdkconfig").read_bytes()
    except OSError as error:
        raise Refused("retained sdkconfig unavailable: %s" % error)
    if hashlib.sha256(sdkconfig).hexdigest() != hashes.get("sdkconfig"):
        raise Refused("retained sdkconfig does not match the record's hash")
    values = sdkconfig_values(artifact / "sdkconfig")
    if values.get("CONFIG_MOTORCONTROLCAN_SMOKE_HOOK") != "y":
        raise Refused("retained sdkconfig has no smoke hook")
    try:
        nodes = [int(values["CONFIG_MOTORCONTROLCAN_SMOKE_NODE_A"]), int(values["CONFIG_MOTORCONTROLCAN_SMOKE_NODE_B"])]
    except (KeyError, TypeError, ValueError):
        raise Refused("retained sdkconfig names no smoke nodes")
    if not all(1 <= node <= 127 for node in nodes) or nodes[0] == nodes[1]:
        raise Refused("retained sdkconfig smoke nodes %s are not two distinct node IDs" % nodes)
    project, sdk = record.get("project_name") or app.get("project_name"), record.get("sdk_version")
    if not isinstance(project, str) or not project or not isinstance(sdk, str) or not sdk:
        raise Refused("build record names no project or SDK version to compare the ready line with")
    return {"record": str(artifact / "build.json"), "elf": elf, "app_elf": record.get("app_elf"),
            "project": project, "sdk": sdk, "base_commit": record.get("base_commit"), "nodes": nodes}


def identity_problems(fields, expected):
    """elf=, idf= and project= of a ready or image line against the record; the SDK version is
    compared without a leading v."""
    problems = []
    if fields["elf"] != expected["elf"]:
        problems.append("elf: image reports %s, build record has %s" % (fields["elf"], expected["elf"]))
    if fields["project"] != expected["project"]:
        problems.append("project: image reports %s, build record has %s" % (fields["project"], expected["project"]))
    if fields["idf"].lstrip("v") != str(expected["sdk"]).lstrip("v"):
        problems.append("idf: image reports %s, build record has %s" % (fields["idf"], expected["sdk"]))
    return problems


def evaluate(lines, expected):
    """Every check on the lines printed after G; pure, so the retained rx.bin can be re-read."""
    problems, other = [], []
    image, done, refused = None, None, False
    nodes = {}
    for line in lines:
        if not line.strip():
            continue
        node = NODE.fullmatch(line)
        if line.startswith("SMOKE image "):
            match = IMAGE.fullmatch(line)
            if image is not None:
                problems.append("a second image line: %s" % line)
            image = dict(match.groupdict(), line=line) if match else {"line": line, "malformed": True}
        elif node:
            number, rest = int(node.group("node")), node.group("rest")
            result = {"line": line}
            for name, pattern in (("ok", IDENTITY_OK), ("failed", IDENTITY_FAILED), ("stopped", STOPPED)):
                match = pattern.fullmatch(rest)
                if match:
                    result.update(match.groupdict(), result=name)
                    break
            else:
                result["result"] = "refused" if rest == IDENTITY_REFUSED else "malformed"
            if number in nodes:
                problems.append("node %d reported twice" % number)
            nodes[number] = result
        elif DONE.fullmatch(line):
            done = int(DONE.fullmatch(line).group("ignored"))
        elif line == REFUSED:
            refused = True
            problems.append("the image refused the check: " + line)
        elif line == FORMAT_ERROR:
            problems.append("the image reported an output format error")
        elif line.startswith("SMOKE"):
            problems.append("unknown SMOKE line: %s" % line)
        else:
            other.append(line)
    if image is None and not refused:
        problems.append("no SMOKE image line")
    elif image is not None and image.get("malformed"):
        problems.append("malformed image line: %s" % image["line"])
    elif image is not None:
        problems += identity_problems(image, expected)  # Again after the start, as it reports it.
        if (image["start"], image["running"], image["transceiver"]) != ("ok", "1", "normal-requested"):
            problems.append("start=%s running=%s transceiver=%s, not ok/1/normal-requested" % (
                image["start"], image["running"], image["transceiver"]))
    for number in expected["nodes"]:
        result = nodes.get(number)
        if result is None:
            problems.append("node %d: no result line" % number)
        elif result["result"] != "ok":
            problems.append("node %d: %s" % (number, result["line"][len("SMOKE "):]))
    for number in sorted(set(nodes) - set(expected["nodes"])):
        problems.append("node %d was not configured" % number)
    if done is None and not refused:
        problems.append("no SMOKE done line")
    elif done:
        problems.append("the image counted %d bytes other than the one G" % done)
    for line in other:
        problems.append("unexpected output: %s" % line)
    return {"image": image, "nodes": {str(number): nodes[number] for number in sorted(nodes)}, "done": done is not None,
            "ignored": done, "problems": problems}


def line_from(capture, clock, index, seconds, predicate):
    """Index of the first complete line at or after index that satisfies predicate, reading until
    the deadline; None at the deadline. Unlike Capture.until it also examines lines that arrived
    earlier in the same read, such as a ready line in the passive line's chunk."""
    deadline = clock.monotonic() + seconds
    while True:
        for number in range(index, len(capture.lines)):
            if predicate(capture.lines[number]):
                return number
        index = max(index, len(capture.lines))
        if clock.monotonic() >= deadline:
            return None
        capture.read()


def _after(capture, index):
    """Every byte after line index: complete lines, then any unterminated rest."""
    return capture.lines[index + 1:] + ([capture.partial.decode("utf-8", errors="replace")]
                                        if capture.partial else [])


def ready_reason(line, expected, meta):
    """None when the ready line names the recorded image, else the reason nothing is sent."""
    if line == READY_UNCONFIGURED:
        return "identity_unconfigured"
    match = READY.fullmatch(line)
    if not match:
        return "ready_line_malformed" if line.startswith("SMOKE ready") else "ready_line_missing"
    meta["ready"] = match.groupdict()
    meta["ready_problems"] = identity_problems(meta["ready"], expected)
    return "identity_mismatch" if meta["ready_problems"] else None


EN_LOW_S = 0.1  # As esptool's hard reset holds EN low; the S2 needs far less.


def pulse_en(port, clock, meta):
    """One EN reset through the ESP-Prog-2: RTS asserted pulls EN low while DTR stays deasserted,
    so IO0 stays high and the chip boots its application. DTR is rewritten after each RTS change,
    as esptool does, so that Windows' usbser.sys sends the new line state. Never retried."""
    import time
    started = clock.monotonic()
    port.rts = True
    port.dtr = False
    time.sleep(EN_LOW_S)
    port.rts = False
    port.dtr = False
    meta["en_pulse"] = {"via": "RTS, DTR deasserted", "low_s": EN_LOW_S,
                        "released_s": clock.monotonic() - started}


def smoke(args, capture, clock, meta, expected):
    """Passive line, ready line naming the recorded image, quiet, one G, capture to a terminal
    line. Sends at most the one byte, and only after every check before it passed."""
    opened = clock.monotonic()
    times = meta["times"]

    def startup(line):
        return "MotorControl-CAN:" in line or "SMOKE" in line or line.startswith(BENCH_READY)
    index = line_from(capture, clock, 0, args.boot_seconds, startup)
    meta["boot_lines"] = capture.lines[:index if index is not None else len(capture.lines)][-BOOT_LINES:]
    resets = [line for line in meta["boot_lines"] if line.startswith("rst:")]
    meta["reset_reason"] = resets[-1] if resets else None
    if index is None:
        meta["reason"] = "passive_state_missed"
        return
    times["boot_line_s"] = clock.monotonic() - opened
    boot = meta["boot_line"] = capture.lines[index]
    if boot != PASSIVE:
        meta["reason"] = ("bench_image" if boot.startswith(BENCH_READY) else
                          "ready_before_passive" if boot.startswith("SMOKE ready") else
                          "smoke_output_before_trigger" if "SMOKE" in boot else "not_passive")
        return
    # The very next line must be the ready line; nothing may come between them.
    ready = line_from(capture, clock, index + 1, READY_S, lambda line: True)
    if ready is None:
        meta["after_passive"] = _after(capture, index)
        meta["reason"] = "ready_line_missing"
        return
    times["ready_line_s"] = clock.monotonic() - opened
    meta["ready_line"] = capture.lines[ready]
    reason = ready_reason(capture.lines[ready], expected, meta)
    if reason:
        meta["after_passive"] = _after(capture, index)
        meta["reason"] = reason
        return
    capture.until(SETTLE_S, lambda line: False)
    if len(capture.lines) != ready + 1 or capture.partial:
        meta["after_ready"] = _after(capture, ready)
        meta["reason"] = "output_after_ready"
        return
    first = len(capture.lines)
    meta["trigger_offset"] = len(capture.data)
    times["trigger_s"] = clock.monotonic() - opened
    # The only write of this tool. A write that raises may still have delivered the byte.
    meta["trigger_write"] = "attempted"
    capture.send(TRIGGER)
    meta["trigger_write"] = "complete"
    meta["sent"].append(TRIGGER.hex())
    end = None
    try:
        end = line_from(capture, clock, first, args.seconds, terminal)
        if end is not None:
            times["terminal_s"] = clock.monotonic() - opened
            capture.until(TAIL_S, lambda line: False)
    finally:
        meta["lines_after_trigger"] = capture.lines[first:]
    meta["reason"] = "terminal" if end is not None else "no_terminal"


def summary(meta, expected):
    """The verdict, per-node results and problems for request.json and the operator."""
    if meta.get("trigger_write") == "attempted":
        return {"verdict": "uncertain", "problems": [
            "the G write did not complete (%s); it may have reached the image, which then runs its check "
            "once on this boot" % meta.get("error")]}
    if not meta["sent"]:
        return {"verdict": "not_sent", "problems": [meta.get("reason") or meta.get("error") or "nothing sent"] +
                meta.get("ready_problems", [])}
    result = evaluate(meta.get("lines_after_trigger", []), expected)
    if meta.get("reason") != "terminal":
        result["problems"].insert(0, "capture ended without a terminal SMOKE line: %s" % (
            meta.get("reason") or meta.get("status")))
    result["verdict"] = "passed" if not result["problems"] else (
        "failed" if meta.get("reason") == "terminal" else "incomplete")
    return result


def dry_run(args, expected):
    print("DRY RUN: no port enumerated or opened, nothing sent or created. Verdict: INCOMPLETE.")
    print("  build record %s: %s flashed, ELF %s, project %s, sdk %s, nodes %s" % (
        expected["record"], SMOKE_APP, expected["elf"], expected["project"], expected["sdk"],
        " and ".join(str(node) for node in expected["nodes"])))
    print("  require USB %s %04X:%04X uniquely on %s; open with DTR and RTS deasserted (EN and BOOT not driven)" % (
        args.serial, FIXTURE["vid"], FIXTURE["pid"], args.port))
    print("  capture up to %s s for the line: %s" % (args.boot_seconds, PASSIVE))
    print("    then within %s s as the next line: SMOKE ready elf=%s idf=%s project=%s" % (
        READY_S, expected["elf"], expected["sdk"], expected["project"]))
    print("    then %s s with no further byte; otherwise stop with nothing sent" % SETTLE_S)
    print("  then send once: %r; capture until a terminal SMOKE line, at most %s s, then %s s" % (
        TRIGGER, args.seconds, TAIL_S))
    print("  require elf=%s project=%s idf=%s start=ok running=1 transceiver=normal-requested," % (
        expected["elf"], expected["project"], expected["sdk"]))
    print("    identity=ok on nodes %s and SMOKE done with ignored=0" % " and ".join(str(n) for n in expected["nodes"]))
    print("  write %s: rx.bin, rx.txt, request.json%s" % (
        args.output, "; would refuse now: it exists" if args.output and args.output.exists() else ""))


def arguments(argv):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", required=True)
    parser.add_argument("--serial", required=True, help="Exact ESP-Prog2 USB serial number")
    parser.add_argument("--artifact", type=Path, required=True,
                        help="s2_flash.py --app %s --flash upload directory" % SMOKE_APP)
    parser.add_argument("--output", type=Path, help="New evidence directory")
    for name, (default, low, high) in SECONDS.items():
        parser.add_argument("--%sseconds" % ("boot-" if name == "boot" else ""), type=float, default=default,
                            help="%s..%s (default %s)" % (low, high, default))
    parser.add_argument("--reset-en", action="store_true",
                        help="After opening, reset the board once through RTS (EN), DTR deasserted, then wait")
    parser.add_argument("--dry-run", action="store_true", help="Read the record and print the plan; touches nothing")
    args = parser.parse_args(argv)
    for name, value in (("boot", args.boot_seconds), ("smoke", args.seconds)):
        _, low, high = SECONDS[name]
        if not low <= value <= high:
            parser.error("--%sseconds must be %s..%s" % ("boot-" if name == "boot" else "", low, high))
    if args.output is None and not args.dry_run:
        parser.error("--output is required")
    return args


def main(argv=None, backend=None, clock=None):
    args = arguments(argv)
    try:
        expected = load_record(args.artifact, args.serial)
    except Refused as error:
        print("REFUSED: %s; nothing enumerated, opened, sent or created" % error, file=sys.stderr)
        return 2
    if args.dry_run:
        dry_run(args, expected)
        return 0
    backend = backend or RealBackend()
    clock = clock or RealClock()
    try:
        usb = check_identity(args.port, args.serial, backend.comports(), True)
        args.output.mkdir(parents=True, exist_ok=False)
    except Refused as error:
        print("REFUSED: %s; nothing opened, sent or created" % error, file=sys.stderr)
        return 2
    except FileExistsError:
        print("REFUSED: %s exists; nothing opened or sent" % args.output, file=sys.stderr)
        return 2
    meta = {"tool": "s2_smoke", "started_utc": clock.utc(), "port": args.port, "serial": args.serial, "usb": usb,
            "baud": BAUD, "artifact": str(args.artifact), "expected": expected, "boot_seconds": args.boot_seconds,
            "seconds": args.seconds, "settle_seconds": SETTLE_S, "tail_seconds": TAIL_S,
            "dtr_rts": "deasserted before open", "reset": "en_pulse" if args.reset_en else False,
            "retry": False, "sent": [], "times": {}}
    meta.update(repository_state())
    capture = None
    try:
        port = backend.open(args.port)
        try:
            capture = Capture(port, clock)
            if args.reset_en:
                pulse_en(port, clock, meta)
            smoke(args, capture, clock, meta, expected)
        finally:
            port.close()
        meta["status"] = "captured"
    except KeyboardInterrupt:  # Ends the capture; nothing is sent for it.
        meta["status"] = "interrupted"
        meta.setdefault("reason", "interrupted")
    except Exception as error:  # Raw bytes and the reason are kept; nothing is replayed.
        meta["status"] = "failed"
        meta["error"] = str(error)
    finally:
        meta["ended_utc"] = clock.utc()
        data = bytes(capture.data) if capture is not None else b""
        meta["result"] = summary(meta, expected)
        (args.output / "rx.bin").write_bytes(data)
        (args.output / "rx.txt").write_bytes(data.decode("utf8", errors="replace").encode("utf8"))
        (args.output / "request.json").write_text(json.dumps(meta, indent=2) + "\n", encoding="utf8")
    result = meta["result"]
    print(data.decode("utf8", errors="replace"), end="")
    print("\nEvidence:", args.output)
    print("passive line: %s" % ("seen" if meta.get("boot_line") == PASSIVE else meta.get("reason")))
    if meta.get("boot_line") == PASSIVE:
        print("ready line: %s (%s)" % (meta.get("ready_line"), "matches the build record" if meta.get("ready") and
                                       not meta.get("ready_problems") else meta.get("reason")))
    print("sent: %s" % ("G once" if meta["sent"] else "nothing"))
    if result.get("image"):
        image = result["image"]
        print("image: %s; build record ELF %s" % (image["line"], expected["elf"]))
    for number, node in sorted(result.get("nodes", {}).items()):
        print("node %s: %s" % (number, node["line"]))
    for problem in result["problems"]:
        print("problem: %s" % problem)
    print("verdict: %s" % result["verdict"])
    if meta.get("reason") == "passive_state_missed":
        print("The passive and ready lines are printed once per boot. Run again with a new --output and "
              "--reset-en, or reset the board by hand (EN button or power) while it waits.")
    return 0 if result["verdict"] == "passed" else 2 if result["verdict"] == "not_sent" else 1


if __name__ == "__main__":
    raise SystemExit(main())
