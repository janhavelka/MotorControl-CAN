"""Pair each velocity run's stop (coast) with the next velocity run's word-7 jump on the same node.
Usage: python minepull.py RAW_DIR [RAW_DIR...]"""
import glob, json, os, re, sys

def events(t):
    ev = []
    for m in re.finditer(r"TRAFFIC k=1 p=(\d+) id=6.. .*?data=2b406000([0-9a-f]{4})", t):
        ev.append((int(m.group(1)), "CW", int.from_bytes(bytes.fromhex(m.group(2)), "little")))
    for m in re.finditer(r"object=(6064|606c|6041|2100):00 dir=0 outcome=0 .*?size=(\d) data=([0-9a-f]{8}) .*?completed=(\d+)", t):
        n = int(m.group(2)); signed = m.group(1) in ("6064", "606c")
        v = int.from_bytes(bytes.fromhex(m.group(3))[:n], "little", signed=signed)
        ev.append((int(m.group(4)), m.group(1), v))
    ev.sort()
    return ev

rows = []
for root in sys.argv[1:]:
    for d in glob.glob(os.path.join(root, "**", "*velocity-*-n[12]-*"), recursive=True):
        rx = os.path.join(d, "rx.txt")
        if not os.path.exists(rx):
            continue
        t = open(rx, encoding="utf-8", errors="replace").read()
        plan = re.search(r"TRIAL_PLAN route=velocity node=(\d) trial=(\d+) .*?rpm=(\d+) up=(\d+)", t)
        if not plan:
            continue
        ev = events(t)
        p = [e for e in ev if e[1] == "6064"]
        v = [e for e in ev if e[1] == "606c"]
        cws = [e for e in ev if e[1] == "CW"]
        stamp = os.path.basename(d).split("-")[0]
        adm = p[0][2] if p else None
        w7 = next((e for e in cws if e[2] == 7), None)
        after7 = next((e for e in p if w7 and e[0] > w7[0]), None)
        v7 = next((e for e in v if w7 and e[0] > w7[0]), None)
        stop = next((e for e in cws if e[2] in (0x0B, 0x010F)), None)
        post = [e for e in p if stop and e[0] > stop[0]]
        vpost = [e for e in v if stop and e[0] > stop[0]]
        emcy = re.search(r"TRIAL_EMCY data=([0-9a-f]+)", t)
        rows.append({"stamp": stamp, "node": int(plan.group(1)), "trial": int(plan.group(2)), "rpm": int(plan.group(3)),
                     "stop": ("quick" if stop and stop[2] == 0x0B else "halt" if stop else "none"),
                     "adm": adm, "jump7": (after7[2] - adm) if after7 and adm is not None else None,
                     "v7": v7[2] if v7 else None,
                     "stop_first": post[0][2] if post else None, "stop_last": post[-1][2] if post else None,
                     "v_after_stop": vpost[0][2] if vpost else None, "emcy": emcy.group(1)[:6] if emcy else None,
                     "dir": d})
rows.sort(key=lambda r: (r["node"], r["stamp"]))
print("node stamp       trial  rpm  stop  | prev rest -> this adm (gap)   | jump at word7  v7 | coast after this stop")
prev = {}
for r in rows:
    pr = prev.get(r["node"])
    gap = (r["adm"] - pr["stop_last"]) if pr and r["adm"] is not None and pr["stop_last"] is not None else None
    coast = (r["stop_last"] - r["stop_first"]) if r["stop_first"] is not None and r["stop_last"] is not None else None
    print("%d %s %5d %5d %-5s | gap %8s | jump %9s v7 %4s | coast %8s v@stop %5s %s" % (
        r["node"], r["stamp"][9:15], r["trial"], r["rpm"], r["stop"], gap, r["jump7"], r["v7"], coast,
        r["v_after_stop"], r["emcy"] or ""))
    prev[r["node"]] = r
