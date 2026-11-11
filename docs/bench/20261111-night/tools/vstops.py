"""Per velocity record: the run's own stop observations (2100, 6041, 6064, 606C per stop cycle) and outcome."""
import glob
import os
import re
import sys

NIGHT = sys.argv[1] if len(sys.argv) > 1 else r"C:\Users\Honza\Documents\Projects\MotorControl-CAN\docs\bench\20261111-night"


def dec(h, signed=True):
    return int.from_bytes(bytes.fromhex(h), "little", signed=signed)


rows = []
for d in sorted(glob.glob(os.path.join(NIGHT, "S*", "*-velocity-*"))):
    if d.endswith("-release") or "release" in os.path.basename(d):
        continue
    try:
        text = open(os.path.join(d, "rx.txt"), encoding="utf-8", errors="replace").read()
    except OSError:
        continue
    stop = re.search(r"TRIAL_STOP kind=(\d) .*?state=(\d) outcome=(\d+).*?op=(\d+)", text)
    vel = re.search(r"TRIAL_VELOCITY own=\d state=(\d) .*?rpm=(\d+)", text)
    node = re.search(r"TRIAL id=(\d+) node=(\d)", text)
    if not (stop and vel and node):
        continue
    sop = stop.group(4)
    cycles, cur = [], {}
    for m in re.finditer(r"RESULT op=(\d+) .*?object=(\w{4}):00 dir=0 .*?size=(\d) data=([0-9a-f]{8})", text):
        if m.group(1) != sop:
            continue
        obj, size, data = m.group(2), int(m.group(3)), m.group(4)
        val = dec(data[:2 * size]) if obj in ("6064", "606c") else data[:2 * size]
        if obj == "2100" and cur:
            cycles.append(cur)
            cur = {}
        cur[obj] = val
    if cur:
        cycles.append(cur)
    obs = " | ".join("{} {} {} {}".format(c.get("2100", "?")[:4], c.get("6041", "?")[:4], c.get("6064"), c.get("606c"))
                     for c in cycles)
    rows.append((node.group(2), node.group(1), vel.group(2), "kind%s" % stop.group(1), "st%s" % stop.group(2),
                 "out%s" % stop.group(3), obs))
for r in rows:
    print("n{} t{} {:>2}rpm {} {} {} : {}".format(*r))
