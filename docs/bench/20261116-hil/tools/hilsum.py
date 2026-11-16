"""Summaries of the 2026-11-16 HIL route records (Tv, Tp, Tn): plan, change, sampled 606C/6064 series, stop."""
import glob, re, sys, os
D = r"C:\Users\Honza\Documents\Projects\MotorControl-CAN\docs\bench\20261116-hil"

def series(text):
    out = []
    for m in re.finditer(r"object=(6064|606c):00 dir=0 outcome=0 .*?size=(\d) data=([0-9a-f]{8}) .*?completed=(\d+)", text):
        n = int(m.group(2))
        v = int.from_bytes(bytes.fromhex(m.group(3))[:n], "little", signed=True)
        out.append((int(m.group(4)), m.group(1), v))
    return out

def summarize(path):
    t = open(os.path.join(path, "rx.txt"), encoding="utf-8", errors="replace").read()
    head = re.search(r"TRIAL id=(\d+) node=(\d) route=(\S+) failure=(\d+) why=(\S+)", t)
    plan = re.search(r"TRIAL_PLAN .*", t)
    last = re.search(r"TRIAL_LAST .*", t)
    stop = re.search(r"TRIAL_STOP .*", t)
    chg = re.search(r"TRIAL_(?:VELOCITY_)?CHANGE .*", t)
    s = series(t)
    t0 = s[0][0] if s else 0
    v = [(round((us - t0) / 1e6, 2), val) for us, k, val in s if k == "606c"]
    p = [(round((us - t0) / 1e6, 2), val) for us, k, val in s if k == "6064"]
    return dict(trial=head.group(1) if head else None, node=head.group(2) if head else None,
                route=head.group(3) if head else None, failure=head.group(4) if head else None,
                why=head.group(5) if head else None, plan=plan.group(0)[11:] if plan else "",
                change=chg.group(0) if chg else "", last=last.group(0)[11:] if last else "",
                stop=stop.group(0)[11:] if stop else "", v=v, p=p)

if __name__ == "__main__":
    kinds = sys.argv[1:] or ["velocity-change", "position-change", "velocity-nmt"]
    for kind in kinds:
        for d in sorted(glob.glob(os.path.join(D, "S*", "*-" + kind + "-*"))):
            r = summarize(d)
            print("{route} n{node} t{trial} failure={failure}/{why}".format(**r))
            print("   plan:", r["plan"][:200])
            print("   change:", r["change"][:200])
            print("   606C:", r["v"][:40])
            print("   last:", r["last"][:160])
            print("   stop:", r["stop"][:160])
