"""Summarize the night soak's results.jsonl: routes, arrivals, errors, velocity and anomalies."""
import collections
import json
import pathlib
import re
import sys

NIGHT = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else
                     r"C:\Users\Honza\Documents\Projects\MotorControl-CAN\docs\bench\20261111-night")
rows = [json.loads(line) for line in (NIGHT / "results.jsonl").read_text(encoding="utf-8").splitlines() if line.strip()]
routes = collections.Counter()
rcs = collections.Counter()
arrivals = collections.defaultdict(list)
anomalies = []
series = set()
speeds = collections.Counter()
velocity = []
for r in rows:
    if r.get("kind") == "arrival":
        arrivals[r["node"]].append(r)
        speeds[r["rpm"]] += 1
        continue
    if r.get("kind") == "anomaly":
        anomalies.append(r)
        continue
    series.add(r["series"])
    routes[(r["node"], r["route"])] += 1
    rcs[(r["route"].split("-")[0], r["rc"])] += 1
    if r["route"].startswith("velocity"):
        velocity.append(r)
print("series:", len(series), "first", min(series) if series else None, "last", max(series) if series else None)
print("motion routes:", sum(routes.values()))
for (node, route), count in sorted(routes.items()):
    print("  node {} {:<22} {}".format(node, route, count))
print("exit codes:", dict(sorted(rcs.items())))
for node in sorted(arrivals):
    errs = [a["error"] for a in arrivals[node]]
    nonzero = [a for a in arrivals[node] if a["error"]]
    print("node {} arrivals {} exact {} max |error| {}".format(node, len(errs), errs.count(0),
                                                             max((abs(e) for e in errs), default=None)))
    for a in nonzero[:10]:
        print("   nonzero:", a)
print("arrival speeds:", dict(sorted(speeds.items())))
vel_ok = collections.Counter((v["node"], v["route"], v["rc"], v.get("failure")) for v in velocity)
print("velocity:", dict(vel_ok))
print("anomalies:", len(anomalies))
for a in anomalies:
    print("  ", a)
