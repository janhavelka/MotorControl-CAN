"""Release a node left enabled and halted in mode 3 with an unrecognised halted report: new MCU boot, read,
one bench quick stop with a best-effort budget (the S18 restart practice), read. Usage: recover_halted.py WT NODE"""
import sys

sys.path.insert(0, r"C:\Users\Honza\AppData\Local\Temp\claude\7a9e2316d506a34f\1ad24d78-b127-4f86-8571-4006da894f8f\scratchpad")
import conmo  # noqa: E402


class Recover(conmo.Session):
    def run(self):
        import os
        import subprocess
        subprocess.run([sys.executable, str(conmo.SCRATCH / "en_reset.py"), str(self.dir / (conmo.stamp() + "-en-reset")),
                        "Release node {} left enabled and halted (6041 1437, 2100 0000)".format(self.node)],
                       env={"PYTHONIOENCODING": "utf-8", **os.environ}, capture_output=True)
        self.bench("start", ["--command", "start", "--output", self.out("start")])
        st = self.read()
        if not ((int(st["6041"], 16) & 0x6f) == 0x27 and st["606c"] == 0 and st["6061"] == "03"):
            raise conmo.Stop("not the halted state this recovery is for: {}".format(st))
        t = self.trial()
        self.bench("stop {}".format(t), ["--command", "stop", "--node", str(self.node), "--trial", str(t), "--kind",
                                        "quick", "--deadline-ms", "2000", "--best-effort-ms", "2000", "--interval-ms",
                                        "250"] + conmo.UNATT + self.camp() +
                   ["--seconds", "60", "--output", self.out("stop-quick-best-effort-{}".format(t))], ok=(0, 3))
        self.bench("release", ["--command", "release", "--seconds", "1.5", "--output", self.out("release")])
        st = self.read()
        self.state["nodes"][str(self.node)]["mode"] = st["6061"]
        self.state["nodes"][str(self.node)]["stoprec"] = None
        if (int(st["6041"], 16) & 0x7f) == 0x50 and st["606c"] == 0:
            self.state["nodes"][str(self.node)]["retired"] = None
            self.say("  node {} released at standstill: back in the soak (no start record: velocity next)".format(
                self.node))
        self.save()


if __name__ == "__main__":
    r = Recover(sys.argv[1], "recover", int(sys.argv[2]))
    try:
        r.run()
    except conmo.Stop as error:
        r.say("!!! recovery stopped: {}".format(error))
        sys.exit(1)
