"""One-port scheduled console session for the S2 bench (2026-11-16 HIL campaign). The lead alone runs it.

Usage: python sched.py WORKTREE OUTDIR PLAN.json

Why: scripts/s2_bench_console.py opens the port once per invocation, so console records that arrive between
invocations (a velocity_change report, a run's own-stop terminal, group_stop_axis records) are never captured, and a
planned stop cannot be timed inside a running operation. This keeps ONE port session for a whole trial, copies every
received byte to OUTDIR/rx.bin, and sends each step at its planned time. It reuses s2_bench_console's framing
(CRC-32 T lines, the explicit allowChanges flag), record parser, port identity check and line reader unchanged.

Plan: {"purpose": str, "steps": [...], "on_failure": [...]}; a step is one of
  {"send": "@ID ...", "save": NAME?}      console line; waits for its reply record (ok:false is a refusal);
                                           with "save", keeps the reply's operation_id as {NAME}
  {"send": "T...", "allow": bool?}        bench T line; waits for TRIAL admitted, and for an arming line its
                                           CONSOLE armed notice
  {"send": "r"|"o"|"M"|"s"}                one bench byte; `r` only after the admitted route record's END trial
  {"mark": NAME}                           remembers the current time
  {"at": SECONDS, "from": NAME}            pumps input until NAME + SECONDS
  {"terminal": NAME, "timeout": S}         pumps input until the terminal record of operation {NAME}
  {"pump": SECONDS}                        pumps input
  "optional": true on a send step keeps going after its refusal (the refusal is still recorded).
Strings may use {NAME} for a saved operation ID. Nothing is retried. A refusal or timeout of a non-optional
step runs "on_failure" once (each step there optional), pumps 3 s and exits 3. Exit 0 is a complete plan.
Records: OUTDIR/rx.bin (raw), transcript.txt (timestamped lines), records.jsonl (console JSON records),
summary.json (plan, saved IDs, marks, step outcomes, refusals).
"""
import json
import pathlib
import sys
import time

wt, outdir, planfile = sys.argv[1], pathlib.Path(sys.argv[2]), pathlib.Path(sys.argv[3])
sys.path.insert(0, str(pathlib.Path(wt) / "scripts"))
import s2_bench_console as sbc  # noqa: E402

plan = json.loads(planfile.read_text(encoding="utf-8"))
outdir.mkdir(parents=True, exist_ok=False)
(outdir / "plan.json").write_text(json.dumps(plan, indent=2) + "\n", encoding="utf-8")
raw = open(outdir / "rx.bin", "wb")
lines = open(outdir / "transcript.txt", "w", encoding="utf-8")
records = open(outdir / "records.jsonl", "w", encoding="utf-8")
clock = time.monotonic
start = clock()
saved, marks, outcomes, refusals = {}, {"start": start}, [], []
terminals = {}


def echo(text):
    lines.write(text + "\n")
    lines.flush()
    body = text.split(" ", 1)[1] if " " in text else text
    rec = sbc.console_record(body)
    if rec is not None:
        records.write(json.dumps(dict(rec, host_s=round(clock() - start, 3))) + "\n")
        records.flush()
        if rec.get("type") == "terminal" and rec.get("operation_id") is not None:
            terminals[rec["operation_id"]] = rec
    print(text[:400], flush=True)


backend = sbc.RealBackend()
sbc.check_identity(sbc.FIXTURE["port"], backend.comports())
port = backend.open(sbc.FIXTURE["port"])
session = sbc.Session(port, clock, raw, echo)


def pump(seconds):
    try:
        session.until(lambda line: None, seconds)
    except sbc.Failure:
        pass  # the planned time passed; pumping is never a failure


def sub(text):
    for k, v in saved.items():
        text = text.replace("{" + k + "}", str(v))
    return text


def do_send(step):
    text = sub(step["send"])
    if text.startswith(("TY", "TB")):
        text = sbc.change_arming(text, bool(step.get("allow")))
    data, kind = sbc.frame(text)
    if kind == ("byte", "r") and session.record_open:
        session.until(lambda line: True if line.startswith("END trial ") else None, 15)
        pump(0.4)  # END trial prints before the firmware clears its dumping flag: an immediate r can be refused
    session.send(data)
    t_sent = round(clock() - start, 3)
    if kind[0] == "console":
        rid = kind[1]
        rec = session.until(lambda line: (lambda r: r if r and r.get("type") == "reply" and r.get("id") == rid else None)(
            sbc.console_record(line)), step.get("timeout", 20))
        ok = bool(rec.get("ok"))
        if ok and step.get("save") and rec.get("operation_id") is not None:
            saved[step["save"]] = rec["operation_id"]
        return ok, dict(sent=text, t=t_sent, reply=rec)
    if kind[0] == "route":
        head = session.until(lambda line: line if line.startswith(("TRIAL admitted", "TRIAL_REFUSED")) else None, 15)
        if not head.startswith("TRIAL admitted"):
            return False, dict(sent=text, t=t_sent, reply=head)
        if kind[1]:
            note = session.until(lambda line: line if line.startswith(("CONSOLE armed", "CONSOLE arm refused")) else
                                 None, 15)
            return note.startswith("CONSOLE armed"), dict(sent=text, t=t_sent, reply=note)
        return True, dict(sent=text, t=t_sent, reply=head)
    replies = sbc.BYTE_REPLIES[kind[1]]
    hit = session.until(lambda line: next(((p, bad) for p, bad in replies if line.startswith(p)), None), 15)
    return not hit[1], dict(sent=text, t=t_sent, reply=hit[0])


def run(steps, failing=False):
    for i, step in enumerate(steps):
        if "mark" in step:
            marks[step["mark"]] = clock()
            outcomes.append(dict(step=i, mark=step["mark"], t=round(clock() - start, 3)))
            continue
        if "at" in step:
            due = marks[step.get("from", "start")] + step["at"]
            pump(max(0.0, due - clock()))
            continue
        if "pump" in step:
            pump(step["pump"])
            continue
        if "terminal" in step:
            op = saved.get(step["terminal"])
            end = clock() + step.get("timeout", 20)
            while op is not None and op not in terminals and clock() < end:
                pump(0.2)
            got = op in terminals
            outcomes.append(dict(step=i, terminal=step["terminal"], op=op, got=got, t=round(clock() - start, 3)))
            if not got and not failing and not step.get("optional"):
                refusals.append(dict(step=i, why="no terminal for {}".format(step["terminal"])))
                return False
            continue
        try:
            ok, info = do_send(step)
        except sbc.Failure as error:
            ok, info = False, dict(sent=step.get("send"), error=str(error))
        info["step"] = i
        outcomes.append(info)
        if not ok:
            refusals.append(info)
            if not failing and not step.get("optional"):
                return False
    return True


complete = run(plan["steps"])
if not complete:
    print("!!! plan step refused or timed out; running on_failure once", flush=True)
    run(plan.get("on_failure", []), failing=True)
    pump(3.0)
pump(0.5)
summary = dict(purpose=plan.get("purpose"), complete=complete, saved=saved,
               marks={k: round(v - start, 3) for k, v in marks.items()}, outcomes=outcomes, refusals=refusals,
               terminals={str(k): v for k, v in terminals.items()}, elapsed_s=round(clock() - start, 3))
(outdir / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
port.close()
sys.exit(0 if complete else 3)
