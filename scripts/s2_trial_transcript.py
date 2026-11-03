#!/usr/bin/env python3
"""Check that the parser reads every record a real bench-route transcript holds."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from s2_bench_test import check_trial_transcript  # noqa: E402


def main():
    problems = check_trial_transcript(sys.argv[1])
    for problem in problems:
        print(problem)
    print("trial transcript: %s" % ("failed" if problems else "passed"))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
