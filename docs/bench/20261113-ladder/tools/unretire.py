import json, pathlib
p = pathlib.Path(r"C:\Users\Honza\Documents\Projects\MotorControl-CAN\docs\bench\20261113-ladder\state.json")
s = json.loads(p.read_text(encoding="utf-8"))
for n in s["nodes"].values():
    n["retired"] = None
p.write_text(json.dumps(s, indent=2) + "\n", encoding="utf-8")
print("trial", s["trial"], "series", s["series"])
