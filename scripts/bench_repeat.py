#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Finite position setup comparison through ordinary moves and one planned stop.

Only write and verify exist in the CAN public path. Verify can write differences.
No settings, save, restore, arbitrary scenario, retry or endurance loop is accepted.
"""
import argparse
import sys

import bench_probe as probe
import bench_scenarios as scenarios
import bench_session


class PositionComparison(scenarios.Scenario):
    """Retain each policy's actual terminal separately; no fabricated timing metric."""

    def _run_commands(self, console, record):
        record["trials"] = []
        for command in self.commands:
            terminal = probe.checked(console, command, record=record,
                                     timeout_s=self.inputs["timeout"], role="primary", release=True)
            if command.startswith("move "):
                setup = command.split()[8]
                record["trials"].append(dict(setup=setup, target=str(self.inputs["target"]),
                                             rpm=self.inputs["rpm"], terminal=terminal))
        record["comparison"] = {setup: sum(trial["setup"] == setup for trial in record["trials"])
                                for setup in ("write", "verify")}


def build_scenario(args):
    """Plan 2*count relative moves; targets and rates retain exact integer spelling."""
    scenarios._validate_options(args, "repeat-position-setup")
    scenarios.validate_values(args)
    scenarios._require(args, ("target", "rpm", "ramp_up_ms", "ramp_down_ms", "stop_policy"))
    probe.positive(args.timeout, "timeout")
    if type(args.count) is not int or not 1 <= args.count <= 20:
        raise ValueError("count must be 1..20 pairs")
    if args.target == 0:
        raise ValueError("comparison requires a nonzero relative displacement")
    moves = tuple(f"move relative {args.target} steps native {args.rpm} configured setup {setup} basis actual"
                  for _ in range(args.count) for setup in ("write", "verify"))
    commands = (f"accel {args.ramp_up_ms} ms", f"decel {args.ramp_down_ms} ms") + moves
    inputs = scenarios._inputs(args)
    inputs.update(total_moves=2 * args.count,
                  total_requested_displacement=str(args.target * 2 * args.count),
                  comparison="sequential confirmed terminals by write/verify; no physical timing claim")
    return PositionComparison("repeat-position-setup", inputs, commands, ("position", "stop"), args.stop_policy)


def arguments(argv=None):
    """Require explicit finite displacement/rate/ramps; no address or serial defaults."""
    parser = argparse.ArgumentParser(description=__doc__)
    bench_session.add_session_arguments(parser)
    parser.add_argument("--count", type=scenarios.bounded(1, 20, "count"), default=1,
                        help="write/verify pairs, 1..20; one final cleanup stop")
    parser.add_argument("--target", type=scenarios.bounded(-(1 << 63), (1 << 63) - 1, "target"))
    parser.add_argument("--rpm", type=scenarios.bounded(1, 3000, "rpm"))
    parser.add_argument("--ramp-up-ms", type=scenarios.bounded(1, 2000, "ramp-up-ms"))
    parser.add_argument("--ramp-down-ms", type=scenarios.bounded(1, 2000, "ramp-down-ms"))
    parser.add_argument("--stop-policy", choices=("normal", "fast"))
    args = parser.parse_args(argv)
    try:
        build_scenario(args)
    except ValueError as error:
        parser.error(str(error))
    return args


def main(argv=None):
    """Print a distinct plan verdict or execute one recorded finite comparison."""
    args = arguments(argv)
    try:
        return bench_session.run_cli(args, build_scenario(args))
    except (ValueError, argparse.ArgumentTypeError) as error:
        print(str(error), file=sys.stderr)
        return 2
    except (Exception, KeyboardInterrupt) as error:
        print(str(error) or "interrupted", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
