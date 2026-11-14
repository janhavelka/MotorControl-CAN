#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Named finite CAN console campaigns; importing and planning perform no I/O.

The firmware public paths own preparation, native units and completion evidence.
These campaigns never enable, reset, save, restore or replay a motor command.
Caller fixture records describe supplied evidence; they cannot arm the firmware.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
from pathlib import Path
import re
import sys

import bench_probe as probe


SCENARIOS = {
    "quick": "one DEC-1 identity read after the session's initial probe; no motor writes",
    "position": "one explicit position move and one preplanned stop",
    "stop": "one standalone explicit stop; no moving-stop qualification claim",
    "velocity": "one finite velocity run, native stop, and separate cleanup only on failure",
    "homing": "one method-specific zero-offset home and one preplanned stop",
    "settings": "one complete driver-settings candidate; no restore or persistence",
}


def integer(text, low, high, name):
    """Parse exact decimal integers without a float conversion or exponent syntax."""
    if type(text) is not str or not re.fullmatch(r"-?(0|[1-9][0-9]*)", text):
        raise argparse.ArgumentTypeError(f"{name} must be a decimal integer")
    value = int(text)
    if not low <= value <= high:
        raise argparse.ArgumentTypeError(f"{name} must be in {low}..{high}")
    return value


def bounded(low, high, name):
    """Build an argparse integer converter with an inclusive finite range."""
    return lambda text: integer(text, low, high, name)


def _inputs(args):
    return {name: str(value) if isinstance(value, Path) or name == "target" and value is not None else value
            for name, value in vars(args).items() if name != "list"}


@dataclass
class Scenario:
    """A finite, prevalidated public-command plan and its scenario-owned cleanup.

    ``run`` uses an already identified/selected Console owned by bench_session.
    The record must contain its validated caller ``fixture`` for device writes.
    Commands/operation summaries are retained by the shared checked helper.
    """
    name: str
    inputs: dict
    commands: tuple[str, ...]
    required_capabilities: tuple[str, ...] = ()
    cleanup: str | None = None
    cleanup_on_failure_only: bool = False

    @property
    def plan(self):
        """JSON-compatible intent only; no observation or qualification verdict."""
        return dict(name=self.name, evidence_kind="planned", inputs=self.inputs,
                    commands=list(self.commands),
                    required_capabilities=list(self.required_capabilities),
                    cleanup=self.cleanup, cleanup_on_failure_only=self.cleanup_on_failure_only,
                    automatic_write_replay=False, restoration_claimed=False,
                    physical_observation="unmeasured", hardware_qualified=False)

    def validate_fixture(self, fixture):
        """Check scenario-specific declarations; shared validation checks provenance.

        This is pure and is also called before port access by the session. The
        native application still checks actual current qualification at admission.
        """
        if not self.required_capabilities:
            return
        if type(fixture) is not dict:
            raise ValueError("device campaign requires an explicit --fixture record")
        if self.cleanup or self.name == "stop":
            stop = fixture.get("stop")
            if type(stop) is not dict or stop.get("method") != self.inputs["stop_policy"]:
                raise ValueError("fixture stop method does not match the planned stop policy")
        if self.name == "homing":
            io = fixture.get("io")
            if (type(io) is not dict or io.get("home_inputs_permit") is not True or
                    type(io.get("method")) is not int or io["method"] != self.inputs["method"]):
                raise ValueError("home_inputs_permit and exact method-specific I/O evidence required")

    def run(self, console, record):
        """Execute once; preserve primary failure even when planned cleanup succeeds.

        No cleanup follows a pure prerequisite refusal. On connection/evidence
        loss the shared helper retains the skipped stop and sends no new command.
        A successful stop means firmware evidence only, never physical standstill.
        """
        fixture = record.get("fixture")
        self.validate_fixture(fixture)
        for capability in self.required_capabilities:
            probe.require_prerequisites(console, fixture, capability)
        primary = None
        try:
            self._run_commands(console, record)
            record["workload_verified"] = True
        except BaseException as error:
            primary = error
            record["primary_error"] = str(error) or type(error).__name__
            record["workload_verified"] = False
            raise
        finally:
            if self.cleanup and (primary is not None or not self.cleanup_on_failure_only):
                clean = probe.cleanup_stop_attempts(console, record, stop=self.cleanup,
                                                   timeout_s=self.inputs["timeout"])
                if not clean and primary is None and record.get("motion_handoff"):
                    record["workload_verified"] = False
                    raise probe.BenchError("planned stop did not obtain qualifying completion evidence")

    def _run_commands(self, console, record):
        for command in self.commands:
            probe.checked(console, command, record=record,
                          timeout_s=self.inputs["timeout"], role="primary", release=True)


def add_campaign_arguments(parser):
    """Explicit native finite motion/settings inputs; no raw command escape hatch."""
    parser.add_argument("--stop-policy", choices=("normal", "fast"))
    parser.add_argument("--target", type=bounded(-(1 << 63), (1 << 63) - 1, "target"),
                        help="exact native command increments; firmware validates native range")
    parser.add_argument("--kind", choices=("relative", "absolute"))
    parser.add_argument("--rpm", type=bounded(1, 3000, "rpm"))
    parser.add_argument("--ramp-up-ms", type=bounded(1, 2000, "ramp-up-ms"))
    parser.add_argument("--ramp-down-ms", type=bounded(1, 2000, "ramp-down-ms"))
    parser.add_argument("--setup", choices=("write", "verify"))
    parser.add_argument("--duration-ms", type=bounded(1, 30000, "duration-ms"),
                        help="finite velocity duration; campaign policy maximum 30 seconds")
    parser.add_argument("--method", type=int, choices=(17, 18, 24, 29))
    parser.add_argument("--search-rpm", type=bounded(1, 3000, "search-rpm"))
    parser.add_argument("--return-rpm", type=bounded(1, 3000, "return-rpm"))
    parser.add_argument("--home-ramp-ms", type=bounded(1, 2000, "home-ramp-ms"))
    parser.add_argument("--driver-settings", nargs=3, metavar=("RPM", "UP_MS", "DOWN_MS"),
                        help="one complete explicit driver set candidate; no automatic restoration")


def _require(args, names):
    for name in names:
        if getattr(args, name, None) is None:
            raise ValueError("--" + name.replace("_", "-") + " is required for this campaign")


def _validate_options(args, selected):
    scopes = {
        "stop_policy": ("position", "stop", "velocity", "homing", "repeat-position-setup"),
        "target": ("position", "repeat-position-setup"),
        "kind": ("position",), "rpm": ("position", "velocity", "repeat-position-setup"),
        "ramp_up_ms": ("position", "repeat-position-setup"),
        "ramp_down_ms": ("position", "repeat-position-setup"),
        "setup": ("position",), "duration_ms": ("velocity",), "method": ("homing",),
        "search_rpm": ("homing",), "return_rpm": ("homing",), "home_ramp_ms": ("homing",),
        "driver_settings": ("settings",),
    }
    for option, allowed in scopes.items():
        if getattr(args, option, None) is not None and selected not in allowed:
            raise ValueError("--" + option.replace("_", "-") + " does not apply to " + selected)


def validate_values(args):
    """Validate direct Python callers as well as argparse-produced namespaces."""
    limits = {"target": (-(1 << 63), (1 << 63) - 1), "rpm": (1, 3000),
              "ramp_up_ms": (1, 2000), "ramp_down_ms": (1, 2000),
              "duration_ms": (1, 30000), "search_rpm": (1, 3000),
              "return_rpm": (1, 3000), "home_ramp_ms": (1, 2000)}
    for name, (low, high) in limits.items():
        value = getattr(args, name, None)
        if value is not None and (type(value) is not int or not low <= value <= high):
            raise ValueError(name + " is outside its exact integer range")
    for name, choices in (("stop_policy", ("normal", "fast")), ("kind", ("relative", "absolute")),
                          ("setup", ("write", "verify")), ("method", (17, 18, 24, 29))):
        value = getattr(args, name, None)
        expected_type = int if name == "method" else str
        if value is not None and (type(value) is not expected_type or value not in choices):
            raise ValueError("invalid " + name)


def build_scenario(args):
    """Build a deterministic bounded plan without reading a file or opening serial.

    Motion coordinates use native command increments explicitly; no shaft-angle,
    feedback conversion or RPM interpretation is inferred in Python. The public
    firmware preparation performs native conversion and all device range checks.
    """
    name = args.scenario
    if name not in SCENARIOS:
        raise ValueError("unknown finite scenario")
    _validate_options(args, name)
    validate_values(args)
    probe.positive(args.timeout, "timeout")
    capabilities, cleanup, failure_only = (), None, False
    if name == "quick":
        commands = (f"read identity {getattr(args, 'request_timeout_ms', 1000)}",)
    else:
        if name != "settings":
            _require(args, ("stop_policy",))
        capabilities = (name, "stop") if name != "stop" else ("stop",)
        if name == "position":
            _require(args, ("target", "kind", "rpm", "ramp_up_ms", "ramp_down_ms", "setup"))
            if args.target == 0 and args.kind == "relative":
                raise ValueError("position campaign requires a nonzero relative displacement")
            commands = (f"accel {args.ramp_up_ms} ms", f"decel {args.ramp_down_ms} ms",
                        f"move {args.kind} {args.target} steps native {args.rpm} configured setup {args.setup}" +
                        (" basis actual" if args.kind == "relative" else ""))
            cleanup = args.stop_policy
        elif name == "stop":
            commands = (f"stop {args.stop_policy}",)
        elif name == "velocity":
            _require(args, ("rpm", "duration_ms"))
            if args.timeout <= args.duration_ms / 1000 + 10:
                raise ValueError("velocity host timeout must exceed duration plus the firmware's 10-second cutoff")
            commands = (f"velocity {args.rpm} rpm motor {args.duration_ms} configured {args.stop_policy}",)
            cleanup, failure_only = args.stop_policy, True
        elif name == "homing":
            _require(args, ("method", "search_rpm", "return_rpm", "home_ramp_ms"))
            commands = (f"home {args.method} {args.search_rpm} {args.return_rpm} {args.home_ramp_ms} zero",)
            cleanup = args.stop_policy
        else:
            _require(args, ("driver_settings",))
            if not isinstance(args.driver_settings, (tuple, list)) or len(args.driver_settings) != 3:
                raise ValueError("driver-settings requires exactly three explicit native integers")
            values = tuple(integer(text, 0, high, label) for text, high, label in
                           zip(args.driver_settings, (3000, 2000, 2000), ("rpm", "up-ms", "down-ms")))
            commands = ("driver set " + " ".join(map(str, values)),)
            capabilities = ("settings",)
    return Scenario(name, _inputs(args), commands, capabilities, cleanup, failure_only)


def arguments(argv=None):
    """Parse one named plan; live/session validation remains shared with the family."""
    import bench_session
    parser = argparse.ArgumentParser(description=__doc__)
    bench_session.add_session_arguments(parser)
    parser.add_argument("--scenario", choices=SCENARIOS, default="quick")
    parser.add_argument("--list", action="store_true", help="list names without file or serial access")
    add_campaign_arguments(parser)
    args = parser.parse_args(argv)
    if not args.list:
        try:
            build_scenario(args)
        except (ValueError, argparse.ArgumentTypeError) as error:
            parser.error(str(error))
    return args


def main(argv=None):
    """Return 0 for an explicit plan/success, 1 for execution failure, 2 for bad input."""
    import bench_session
    args = arguments(argv)
    if args.list:
        print(json.dumps(SCENARIOS, indent=2))
        return 0
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
