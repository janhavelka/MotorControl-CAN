#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Two independently admitted finite position moves through one existing Console.

The planned per-axis stops share a fixed host cutoff established before setup.
This is sequential SDO admission, never synchronized motion. Native prerequisites
and stop settlement remain firmware-owned; fixture JSON creates no permission.
"""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import sys

import bench_probe as probe
import bench_session
import s2_bench as bench


def validate_plan(axes, *, trial, stop_kind, stop_after_ms, timeout_s):
    """Pure bounded first-fixture policy; no native trajectory/conversion logic."""
    entry = dict(event="paired_reservation", seq=1, prev=bench.LEDGER_GENESIS, trial=trial,
                 axes=axes, stop_kind=stop_kind, stop_after_ms=stop_after_ms, motion=4, stop=2)
    problem = bench._paired_problem(entry)
    if problem:
        raise ValueError(problem)
    probe.positive(timeout_s, "timeout")
    if timeout_s <= stop_after_ms / 1000:
        raise ValueError("timeout must exceed the preparation-to-stop interval")
    return entry


def validate_fixture(fixture, axes, stop_kind):
    """Require independent supplied provenance for each logical axis and node."""
    probe.validate_fixture(fixture)
    rows = fixture.get("axes")
    if type(rows) is not list or len(rows) != 2:
        raise ValueError("paired fixture needs exactly two axes with their own fixture records")
    for axis in axes:
        matches = [row for row in rows if type(row) is dict and
                   row.get("axis_id") == axis["axis_id"] and row.get("node") == axis["node"]]
        if len(matches) != 1:
            raise ValueError("paired fixture logical axis/node does not match the plan")
        for capability in ("position", "stop"):
            probe.validate_fixture(matches[0].get("fixture"), capability)
        if matches[0]["fixture"]["stop"].get("method") != stop_kind:
            raise ValueError("paired fixture stop method differs from plan")
    return rows


def _accepted(console, handle, cutoff):
    for _ in range(handle.turn_limit):
        if console._now() >= cutoff:
            raise probe.BenchError("paired admission still unknown at the immutable stop cutoff")
        console.pump()
        if handle.accepted:
            return
        if handle.terminal is not None:
            raise probe.BenchError("paired move refused before native admission")
        remaining = cutoff - console._now()
        if remaining > 0:
            console.sleep(min(probe.IDLE_SECONDS, remaining))
    console._fail("paired admission turn capacity exhausted")


def _selection_reply(console, handle, cutoff):
    """Yield at the stop cutoff without retiring a still-owned reply correlation."""
    for _ in range(handle.turn_limit):
        if console._now() >= cutoff:
            raise probe.BenchError("paired selection still unknown at the immutable stop cutoff")
        console.pump()
        if handle.phase == "retained":
            result = console.wait(handle)
            if not result["ok"]:
                raise probe.BenchError("paired host selection refused")
            return
        remaining = cutoff - console._now()
        if remaining > 0:
            console.sleep(min(probe.IDLE_SECONDS, remaining))
    console._fail("paired selection turn capacity exhausted")


def trigger_handoff_skew(traces, axes):
    """Exact owner enqueue processing times; neither wire timing nor synchrony."""
    selected = {}
    for axis in axes:
        matches = [row for row in traces if row.get("kind") == 1 and row.get("accepted") is True and
                   row.get("can_id") == 0x600 + axis["node"] and row.get("extended") is False and
                   row.get("rtr") is False and row.get("dlc") == 8 and
                   row.get("payload") == "2B4060001F000000"]
        if len(matches) != 1:
            return dict(observed=False, skew_us=None, reason="exactly one accepted trigger handoff per axis not retained")
        value = matches[0].get("processed_us")
        if not probe._decimal(value) or int(value) > 2**64 - 1:
            return dict(observed=False, skew_us=None, reason="invalid owner processing time")
        selected[str(axis["axis_id"])] = dict(processed_us=value, record=matches[0])
    times = [int(row["processed_us"]) for row in selected.values()]
    return dict(observed=True, skew_us=str(max(times) - min(times)), axes=selected,
                scope="owner accepted SDO enqueue processing times; no wire/synchronized-start claim")


def run_pair(console, record, *, axes, trial, stop_kind, stop_after_ms, timeout_s, campaign):
    """One reservation, two starts, two planned stops; no retries or result eviction.

    Borrow an identified single byte-stream owner and locked Campaign. Each stop
    is handed off before waiting for either result. A poisoned connection issues
    no further commands, and all reached handles remain in the evidence record.
    """
    validate_plan(axes, trial=trial, stop_kind=stop_kind, stop_after_ms=stop_after_ms, timeout_s=timeout_s)
    fixtures = validate_fixture(record.get("fixture"), axes, stop_kind)
    for row in fixtures:
        for capability in ("position", "stop"):
            probe.require_prerequisites(console, row["fixture"], capability)
    if console.operation_capacity < 4:
        raise probe.BenchError("paired route needs at least two configured motion channels")
    record["campaign"] = campaign.reserve_pair(trial=trial, axes=axes, stop_kind=stop_kind,
                                                stop_after_ms=stop_after_ms)
    record["paired_effects_reserved"] = dict(moves=2, planned_stops=2, refunded=False)
    start = console._now()
    due, final = start + stop_after_ms / 1000, start + timeout_s
    record["paired_stop_cutoff_host_s"] = due
    record["paired_final_deadline_host_s"] = final
    record["paired_axes"] = {str(axis["axis_id"]): {} for axis in axes}
    handles, traces = [], []
    primary = None
    emit = console.emit
    def capture(event, **fields):
        emit(event, **fields)
        if (event == "trace" and len(traces) < 4 and fields["record"].get("kind") == 1 and
                fields["record"].get("payload") == "2B4060001F000000"):
            traces.append(fields["record"])
    def begin(axis, role, command, timeout, handoff_cutoff=None):
        identity = console.next_id
        try:
            handle = console.begin(command, timeout_s=timeout, handoff_cutoff_s=handoff_cutoff)
        except BaseException:
            handle = console.pending.get(identity)
            if handle is not None:
                handles.append((axis, role, handle))
            raise
        handles.append((axis, role, handle))
        return handle
    console.emit = capture
    try:
        try:
            # Host intent is per attachment. These do not arm or enable a motor.
            for axis in axes:
                for command in (f"useaddr {axis['node']}", f"accel {axis['ramp_up_ms']} ms",
                                f"decel {axis['ramp_down_ms']} ms"):
                    probe.checked(console, command, record=record, timeout_s=timeout_s, role="paired_setup")
            for index, axis in enumerate(axes):
                if console._now() >= due:
                    raise probe.BenchError("paired setup consumed the immutable movement window")
                if index:
                    selected = begin(axis, "selection", f"useaddr {axis['node']}", final - console._now(), due)
                    _selection_reply(console, selected, due)
                else:
                    probe.checked(console, f"useaddr {axis['node']}", record=record, timeout_s=timeout_s, role="selection")
                if console._now() >= due:
                    raise probe.BenchError("paired selection consumed the immutable movement window")
                command = f"move absolute {axis['target']} steps native {axis['rpm']} configured setup write"
                handle = begin(axis, "move", command, final - console._now(), due)
                _accepted(console, handle, due)
                if handle.axis_id != axis["axis_id"]:
                    console._fail("paired move admission differs from the supplied logical axis")
        except BaseException as error:
            primary = error
            record.setdefault("primary_error", str(error) or type(error).__name__)
        if handles and console.synchronized:
            # No first-stop wait here: refusal on one axis cannot delay the other's request.
            for _ in range(probe._turn_limit(stop_after_ms / 1000)):
                if console._now() >= due:
                    break
                console.pump()
                remaining = due - console._now()
                if remaining > 0:
                    console.sleep(min(probe.IDLE_SECONDS, remaining))
            else:
                console._fail("paired cutoff turn capacity exhausted")
            for axis in axes:
                if console._now() >= final:
                    error = probe.BenchError("paired immutable final deadline expired before planned stop")
                    primary = primary or error
                    record.setdefault("paired_stop_refusals", {})[str(axis["axis_id"])] = str(error)
                    continue
                try:
                    begin(axis, "stop", f"stop {axis['axis_id']} {stop_kind}", final - console._now())
                except Exception as error:
                    primary = primary or error
                    record.setdefault("paired_stop_refusals", {})[str(axis["axis_id"])] = str(error)
                    if not console.synchronized:
                        break
            # Collect the independent stops first. An original motion's missing
            # terminal cannot prevent retention of either planned stop result.
            ordered = [row for row in handles if row[1] == "stop"] + [row for row in handles if row[1] == "move"]
            for axis, role, handle in ordered:
                result = console.wait(handle)
                if role == "stop" and (not result["ok"] or result.get("uncertain") is not False):
                    primary = primary or probe.BenchError("planned paired stop lacks qualified completion")
        elif handles:
            record["paired_stops_skipped"] = "console evidence/connection lost; no replay or new handoff"
        if primary:
            raise primary
        if len([row for row in handles if row[1] == "stop"]) != 2:
            raise probe.BenchError("paired route did not hand off both planned stops")
        record["workload_verified"] = True
    finally:
        console.emit = emit
        for axis, role, handle in handles:
            record["paired_axes"][str(axis["axis_id"])][role] = console.summary(handle,
                evidence_kind=record.get("evidence_kind", "host-tested"))
        record["trigger_handoff"] = trigger_handoff_skew(traces, axes)
        record["synchronized_start_claimed"] = False


def arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    bench_session.add_session_arguments(parser)
    parser.add_argument("--axis", action="append", nargs=6, type=int, required=True,
                        metavar=("ID", "NODE", "TARGET", "RPM", "UP_MS", "DOWN_MS"))
    parser.add_argument("--trial", type=int, required=True)
    parser.add_argument("--stop-after-ms", type=int, required=True)
    parser.add_argument("--stop-policy", choices=("normal", "fast"), required=True)
    parser.add_argument("--campaign", type=Path)
    parser.add_argument("--new-campaign", action="store_true")
    parser.add_argument("--motion-caps", action="store_true",
                        help="Optional guard: packet 22's campaign caps refuse the reservation past them")
    args = parser.parse_args(argv)
    args.axes = [dict(zip(("axis_id", "node", "target", "rpm", "ramp_up_ms", "ramp_down_ms"), row)) for row in args.axis]
    try:
        validate_plan(args.axes, trial=args.trial, stop_kind=args.stop_policy,
                      stop_after_ms=args.stop_after_ms, timeout_s=args.timeout)
        if args.node is not None and args.node != args.axes[0]["node"]:
            raise ValueError("--node must match the first explicitly planned fixture node")
    except ValueError as error:
        parser.error(str(error))
    return args


def main(argv=None):
    args = arguments(argv)
    lock = None
    try:
        inputs = dict(axes=args.axes, trial=args.trial, stop_kind=args.stop_policy,
                      stop_after_ms=args.stop_after_ms, timeout_s=args.timeout)
        fixture = bench_session.load_fixture(args.fixture) if args.fixture else None
        if fixture is not None:
            validate_fixture(fixture, args.axes, args.stop_policy)
        if args.plan_only:
            print(json.dumps(dict(evidence_kind="planned", inputs=inputs, moves=2, planned_stops=2,
                                  synchronized_start=False, replay=False, raw_trace_required=True), indent=2))
            return 0
        for name in ("port", "baud", "profile", "can_bitrate", "expected_library_version", "out", "campaign", "fixture"):
            if getattr(args, name) is None:
                raise ValueError("live paired route requires --" + name.replace("_", "-"))
        validate_fixture(fixture, args.axes, args.stop_policy)
        class Clock:
            @staticmethod
            def utc(): return datetime.now(timezone.utc).isoformat()
        lock = bench.CampaignLock(args.campaign).acquire(Clock())
        campaign = bench.Campaign(args.campaign, args.new_campaign, args.motion_caps).load()
        def run(console, record):
            run_pair(console, record, campaign=campaign, **inputs)
        bench_session.run_recorded(port=args.port, baud=args.baud, out=args.out, scenario="paired-position",
            inputs=inputs, run=run, profile=args.profile, node=args.axes[0]["node"], can_bitrate=args.can_bitrate,
            expected_version=args.expected_library_version, fixture=fixture, timeout_s=args.timeout,
            startup_s=args.startup, firmware=args.firmware, request_timeout_ms=args.request_timeout_ms, trace_raw=True)
        return 0
    except (ValueError, OSError, probe.BenchError, bench.Refused, ImportError, KeyboardInterrupt) as error:
        print(str(error) or "interrupted", file=sys.stderr)
        return 1
    finally:
        if lock is not None:
            lock.release()


if __name__ == "__main__":
    raise SystemExit(main())
