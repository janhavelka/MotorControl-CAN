import pathlib, ast
s = pathlib.Path("ladder.py").read_text(encoding="utf-8")
s = s.replace('LADDER = REPO / "docs" / "bench" / "20261113-ladder"', 'LADDER = REPO / "docs" / "bench" / "20261116-hil"')
i = s.index("WAIVER = ("); j = s.index("night.UNATT")
s = s[:i] + ('WAIVER = ("do all HIL testing regarding new implementation and finish up all tests that are missing and are able to "\n'
             '          "run now.. Motor power cycled (user, 2026-11-16); standing bench authorization of 2026-11-02 '
             '(docs/prompts/COMMON.md): physical tests generally allowed, two drives, free shafts, motors fixed to the table")\n') + s[j:]
s = s.replace('self.s = {"trial": 5100, "series": 0, "rng": 20261113,', 'self.s = {"trial": 56000, "series": 0, "rng": 20261116,')
assert "56000" in s and "20261116-hil" in s
pathlib.Path("hil.py").write_text(s, encoding="utf-8")
t = pathlib.Path("stuck.py").read_text(encoding="utf-8")
t = t.replace('spec_from_file_location("ladder"', 'spec_from_file_location("hil"').replace("scratchpad\ladder.py", "scratchpad\hil.py")
assert "scratchpad\hil.py" in t
pathlib.Path("hilstep.py").write_text(t, encoding="utf-8")
ast.parse(s); ast.parse(t); print("ok")
