"""Print the controlword/read timeline of a velocity record: python timeline.py RX_TXT [max_lines]"""
import re, sys
t = open(sys.argv[1], encoding="utf-8", errors="replace").read()
ev = []
for m in re.finditer(r"TRAFFIC k=1 p=(\d+) id=6.. .*?data=2b406000([0-9a-f]{4})", t):
    ev.append((int(m.group(1)), "CW", "%04x" % int.from_bytes(bytes.fromhex(m.group(2)), "little")))
for m in re.finditer(r"TRAFFIC k=0 p=(\d+) id=(8.) .*?data=([0-9a-f]{16})", t):
    ev.append((int(m.group(1)), "EMCY", m.group(3)))
for m in re.finditer(r"object=(6064|606c|6041|2100):00 dir=0 outcome=0 .*?size=(\d) data=([0-9a-f]{8}) .*?completed=(\d+)", t):
    n = int(m.group(2)); signed = m.group(1) in ("6064", "606c")
    v = int.from_bytes(bytes.fromhex(m.group(3))[:n], "little", signed=signed)
    ev.append((int(m.group(4)), m.group(1), v if signed else "%04x" % v))
ev.sort()
if not ev:
    sys.exit("no events")
t0 = ev[0][0]
rows, cur = [], None
for e in ev:
    if e[1] in ("CW", "EMCY"):
        rows.append("%8.3f %-4s %s" % ((e[0] - t0) / 1e6, e[1], e[2])); continue
    rows.append("%8.3f %-4s %s" % ((e[0] - t0) / 1e6, e[1], e[2]))
lim = int(sys.argv[2]) if len(sys.argv) > 2 else 60
print("\n".join(rows[:lim]))
