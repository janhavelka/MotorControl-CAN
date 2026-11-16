#!/usr/bin/env python3
"""Check that the parser reads every record a real bench-route transcript holds."""
import sys
import re
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from s2_bench_test import check_trial_transcript  # noqa: E402
import s2_evidence  # noqa: E402


def check_changes(path):
    """Replay the actual bounded firmware formatter for all eight position cases and velocity."""
    data = Path(path).read_bytes()
    parts = re.findall(rb"TRIAL id=.*?END trial[^\n]*\n", data, re.S)
    problems = []
    if len(parts) != 9:
        problems.append("expected eight position cases and one velocity record")
    seen = set()
    for part in parts:
        report = s2_evidence.replay_bytes(part)
        if report.errors or report.unknown or report.malformed or report.unknown_fields:
            problems.append("invalid change transcript: %s" % (report.errors or report.unknown or report.unknown_fields))
        plan = next((r.values for r in report.records if r.kind == "TRIAL_PLAN"), {})
        if plan.get("route") == "position-change":
            case = plan.get("case")
            seen.add(case)
            wanted = "complete"
            if report.verdict != wanted:
                problems.append("case %s: expected %s, got %s" % (case, wanted, report.verdict))
        elif plan.get("route") == "velocity-change":
            if report.verdict != "complete" or report.kinds["TRIAL_VELOCITY_CHANGE"] != 1:
                problems.append("velocity change record is incomplete")
        else:
            problems.append("unexpected change route")
    if seen != set(range(1, 9)):
        problems.append("position change case inventory differs")
    return problems


def main():
    # --all checks the transcript of bench_route_test --transcript-all, with protocol 6's routes.
    every = sys.argv[1:2] == ["--all"]
    changes = sys.argv[1:2] == ["--changes"]
    problems = check_changes(sys.argv[2]) if changes else check_trial_transcript(sys.argv[2 if every else 1], every)
    for problem in problems:
        print(problem)
    print("trial transcript: %s" % ("failed" if problems else "passed"))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
