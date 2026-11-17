"""HM3 again on the relaxed image (2026-11-17): enables from the drive's own released report (`--released`, no stop
record), paired arming, a group HALT at 3 s, then the group quick stop about 2.3 s later, which the freshest-state rule
should sequence on each HALT's own cycle. The bench's own reads after it show whether the sessions kept any
uncertainty (a best-effort stop would leave them refusing reads with reason=session).

Usage: python relaxhil.py WORKTREE hm3
Built on pairhil.py (sched.py one-port sessions) and hil.py's ladder for reads and mode routes. Nothing is retried.
"""
import importlib.util
import re
import sys

SCRATCH = r"C:\Users\Honza\AppData\Local\Temp\claude\7a9e2316d506a34f\1ad24d78-b127-4f86-8571-4006da894f8f\scratchpad"
spec = importlib.util.spec_from_file_location("pairhil", SCRATCH + r"\pairhil.py")
ph = importlib.util.module_from_spec(spec)
argv = sys.argv
sys.argv = ["x"]
spec.loader.exec_module(ph)
sys.argv = argv
c, night = ph.c, ph.night


def released_enable(lad, node, st):
    """night.Night.enable with --released: the drive's own fresh released report, no stop record."""
    rc, text, d, _ = lad.motion(node, "enable", ["enable", "--status", "0x" + st["6041"], "--position-target",
                                                 str(st["607a"]), "--velocity-target", str(st["60ff"]),
                                                 "--deadline-ms", "10000", "--interval-ms", "1250", "--released"],
                                seconds=60)
    m = re.search(r"TRIAL_ENABLE_STEP word=15 .*? position=(-?\d+)", text)
    if not re.search(r"TRIAL id=\d+ node=\d route=\S+ failure=0 ", text) or not m:
        raise night.Retire("released enable failed")
    return int(m.group(1))


class Relaxed(ph.Pair):
    def to_mode1_released(self, node, label):
        """Power-up state: mode 1 and the best-effort quick stop (as before); then mode 1 if needed."""
        st = self.lad.begin(node, label)
        if self.lad.enabled(st) and st["2100"] == "0001" and st["6061"] in ("00", "01"):
            self.lad.say("  power-up state: mode 1, best-effort quick stop, then a fresh series")
            if st["6061"] == "00":
                self.lad.motion(node, "mode1", ["mode1", "--cycles", "5", "--interval-ms", "500"], seconds=60)
            self.lad.emergency(node)
            st = self.lad.begin(node, label + "b")
        if not st or not self.lad.released(st):
            raise c.Stop("not released: {}".format(st))
        if st["6061"] != "01":
            self.lad.motion(node, "mode1", ["mode1", "--cycles", "3", "--interval-ms", "500"], ok=(0, 3), seconds=60)
            st = self.lad.read(node)
        if not st or st["6061"] != "01" or not self.lad.released(st):
            raise c.Stop("mode 1 not reported released: {}".format(st))

    def hm3r(self):
        for n in (1, 2):
            self.to_mode1_released(n, "hm3r{}".format(n))
        self.lad.begin(1, "hm3r")
        st1, st2 = self.lad.read(1), self.lad.read(2)
        p1, p2 = released_enable(self.lad, 1, st1), released_enable(self.lad, 2, st2)
        steps = self.moves(p1, p2, -4000, 4000, 30, 111) + [
            {"at": 3.0, "from": "t0"}, {"send": "@121 stop all normal", "save": "g1"}, {"pump": 2.0},
            {"send": "@122 result {g1}", "optional": True}, {"send": "@123 release {g1}", "optional": True},
            {"send": "@124 stop all fast", "save": "g2"}, {"pump": 2.0},
            {"terminal": "m1", "timeout": 5, "optional": True}, {"terminal": "m2", "timeout": 5, "optional": True}] + \
            self.export(131, ["g2", "m1", "m2"])
        self.sched("hm3r", steps, [{"send": "@190 stop all fast"}],
                   "HM3 on the relaxed image: released enables without stop records, paired moves 4000 counts at "
                   "30 rpm, group HALT at 3 s, group quick stop 2.3 s later (freshest-state rule)")
        for n in (1, 2):
            st = self.lad.read(n)
            self.lad.say("  HM3r end n{} {}".format(n, st))


if __name__ == "__main__":
    wt, session = argv[1], argv[2]
    t = Relaxed(wt)
    try:
        {"hm3": t.hm3r}[session]()
        t.lad.say("relaxed session {} ended".format(session))
    except (c.Stop, night.Retire) as error:
        t.lad.say("!!! relaxed session {} stopped: {}".format(session, error))
        for n in (1, 2):
            try:
                st = t.lad.read(n)
                if st and (t.lad.enabled(st) or st["606c"] != 0):
                    t.lad.quick(n, "stop-quick-stationary")
            except Exception as e2:  # noqa: BLE001
                t.lad.say("!!! check on node {}: {}".format(n, e2))
        sys.exit(1)
    finally:
        t.lad.save()
