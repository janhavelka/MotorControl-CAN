#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Finite public position, stop, velocity, homing and settings campaigns.

The default quick scenario is read-only. Every action requires explicit input
and an externally prepared application fixture; this runner never arms a drive.
"""
import argparse
import sys

import bench_scenarios as scenarios
import bench_session


def arguments(argv=None):
    """Reuse the named campaign inputs; --list belongs only to bench_scenarios."""
    parser = argparse.ArgumentParser(description=__doc__)
    bench_session.add_session_arguments(parser)
    parser.add_argument("--scenario", choices=scenarios.SCENARIOS, default="quick")
    scenarios.add_campaign_arguments(parser)
    args = parser.parse_args(argv)
    try:
        scenarios.build_scenario(args)
    except (ValueError, argparse.ArgumentTypeError) as error:
        parser.error(str(error))
    return args


def main(argv=None):
    """Plan without serial, or delegate one owned session and its evidence verdict."""
    args = arguments(argv)
    try:
        return bench_session.run_cli(args, scenarios.build_scenario(args))
    except (ValueError, argparse.ArgumentTypeError) as error:
        print(str(error), file=sys.stderr)
        return 2
    except (Exception, KeyboardInterrupt) as error:
        print(str(error) or "interrupted", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
