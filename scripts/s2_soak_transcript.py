#!/usr/bin/env python3
"""Check that the parser reads every record a real firmware transcript holds."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from s2_soak_test import check_transcript  # noqa: E402


def main():
    problems = check_transcript(sys.argv[1])
    for problem in problems:
        print(problem)
    print("transcript: %s" % ("failed" if problems else "passed"))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
