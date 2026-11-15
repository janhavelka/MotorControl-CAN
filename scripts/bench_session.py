#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""One finite console session owns its port, exclusive artifacts and bounded evidence.

Scenario callbacks own planned cleanup. This module performs no motor cleanup,
recovery, replay or private object sequence. Imports and planning need no serial.
"""
from __future__ import annotations

from contextlib import contextmanager
import argparse
import hashlib
import json
from pathlib import Path
import sys
import time
import zipfile

import bench_probe as probe
from console_jsonl import parse_record

CACHED = ("config", "status", "health", "stats", "drv", "memory")
MAX_FIXTURE_BYTES = 16384
MAX_SUMMARY_BYTES = 512 * 1024
MAX_FIRMWARE_BYTES = 64 * 1024 * 1024


def add_session_arguments(parser):
    """Shared explicit console/fixture flags; none selects a device implicitly."""
    parser.add_argument("--plan-only", action="store_true", help="print intent without artifacts, serial import or hardware")
    parser.add_argument("--port", help="explicit serial-console port")
    parser.add_argument("--baud", type=probe._integer(1, 4000000), help="explicit serial-console baud")
    parser.add_argument("--profile", choices=(probe.PROFILE,))
    parser.add_argument("--node", type=probe._integer(1, 127), help="existing CANopen host binding")
    parser.add_argument("--can-bitrate", type=probe._integer(1, 1000000), help="expected cached interface bits/s; never changed")
    parser.add_argument("--expected-library-version", help="exact reported major.minor.patch")
    parser.add_argument("--out", type=Path, help="new artifact prefix: .json, .jsonl and .zip")
    parser.add_argument("--fixture", type=Path, help="bounded supplied prerequisite/provenance JSON; never an arming grant")
    parser.add_argument("--firmware", type=Path, help="optional caller-supplied image to hash, not flash identity attestation")
    parser.add_argument("--timeout", type=probe._seconds, default=15.0, help="immutable per-command host seconds (0,60]")
    parser.add_argument("--startup", type=probe._seconds, default=0.2)
    parser.add_argument("--request-timeout-ms", type=probe._integer(1, 60000), default=1000)


def load_fixture(path):
    """Read at most16KiB of strict JSON before opening a port; reject duplicate keys."""
    with Path(path).open("rb") as stream:
        data = stream.read(MAX_FIXTURE_BYTES + 1)
    if len(data) > MAX_FIXTURE_BYTES:
        raise ValueError("fixture exceeds16KiB")
    value = parse_record(data.decode("utf-8"))
    probe._bounded_json(value)
    probe.validate_fixture(value)
    return value


def _image(path):
    if path is None:
        return None
    path = Path(path)
    size = path.stat().st_size
    if not path.is_file() or size > MAX_FIRMWARE_BYTES:
        raise ValueError("supplied firmware must be a file at most64MiB")
    digest, count = hashlib.sha256(), 0
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(65536), b""):
            count += len(block)
            if count > MAX_FIRMWARE_BYTES:
                raise ValueError("firmware changed beyond the64MiB bound")
            digest.update(block)
    return dict(path=str(path), bytes=count, sha256=digest.hexdigest(),
                provenance="caller-supplied image; console does not attest its flash hash")


def _json(value):
    data = (json.dumps(value, ensure_ascii=True, allow_nan=False, sort_keys=True, indent=2) + "\n").encode("ascii")
    if len(data) > MAX_SUMMARY_BYTES:
        raise probe.BenchError("session summary exceeds512KiB")
    return data


@contextmanager
def _reserve_artifacts(paths, record):
    """Own only this attempt's new files; preserve primary errors through all closes."""
    created = []
    try:
        for path in paths:
            created.append((path, path.open("x+b")))
    except BaseException:
        for path, stream in created:
            try:
                stream.close()
            except BaseException:
                pass
            try:
                path.unlink()  # New empty reservations only; previous evidence is never touched.
            except OSError:
                pass
        raise
    primary = None
    try:
        yield [stream for _, stream in created]
    except BaseException as error:
        primary = error
        raise
    finally:
        close_error = None
        for _, stream in reversed(created):
            try:
                stream.close()
            except BaseException as error:
                close_error = close_error or error
        if close_error is not None:
            record.update(ok=False, verdict="failed", artifact_close_error=str(close_error))
            record.setdefault("primary_error", str(primary or close_error))
            # Archive invalidation cannot depend on repairing another failed medium.
            # In particular, a summary close/reopen error must not preserve a passing ZIP.
            try:
                with paths[2].open("r+b") as output:
                    output.truncate(0)  # A closed-but-unflushed archive cannot claim success.
            except BaseException:
                pass
            try:
                with paths[0].open("r+b") as output:
                    output.truncate(0)
                    output.write(_json(record))
            except BaseException:
                pass
            if primary is None:
                raise close_error


def package_report(summary, transcript, archive):
    """Deterministic allowlisted ZIP, streamed transcript and explicit SHA256 manifest.

    Supplied binary streams are borrowed only during this call. No directory scan,
    host files or firmware image enters the report. Identical input bytes produce
    identical ZIP_STORED bytes with fixed metadata and no filesystem timestamps.
    """
    entries, manifest = (("summary.json", summary), ("transcript.jsonl", transcript)), {}
    for name, stream in entries:
        stream.flush()
        stream.seek(0)
        digest, count = hashlib.sha256(), 0
        for block in iter(lambda: stream.read(65536), b""):
            digest.update(block)
            count += len(block)
        manifest[name] = dict(bytes=count, sha256=digest.hexdigest())
    archive.seek(0)
    archive.truncate()
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_STORED) as bundle:
        for name in ("manifest.json", "summary.json", "transcript.jsonl"):
            item = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
            item.create_system = 3
            item.external_attr = 0o100644 << 16
            if name == "manifest.json":
                bundle.writestr(item, _json(manifest))
            else:
                stream = summary if name == "summary.json" else transcript
                stream.seek(0)
                with bundle.open(item, "w") as output:
                    for block in iter(lambda: stream.read(65536), b""):
                        output.write(block)
    archive.flush()


def _arguments(*, port, baud, profile, node, can_bitrate, expected_version,
               timeout_s, startup_s, evidence_kind, inputs):
    probe.positive(timeout_s, "command timeout")
    probe.positive(startup_s, "startup duration")
    if type(port) is not str or not 1 <= len(port) <= 256 or any(ord(char) < 32 for char in port):
        raise ValueError("explicit serial port required")
    for name, value, limit in (("baud", baud, 4000000), ("node", node, 127), ("CAN bitrate", can_bitrate, 1000000)):
        if type(value) is not int or not 1 <= value <= limit:
            raise ValueError("invalid explicit " + name)
    if profile != probe.PROFILE or type(expected_version) is not str or not probe.re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", expected_version):
        raise ValueError("explicit implemented profile and exact library version required")
    if evidence_kind not in ("host-tested", "live-observed"):
        raise ValueError("execution needs an explicit host-tested/live-observed origin")
    if type(inputs) is not dict:
        raise ValueError("session inputs must be an owned JSON object")
    probe._bounded_json(inputs)
    if len(_json(inputs)) > 4096:
        raise ValueError("session inputs exceed4096 bytes")


def run_recorded(*, port, baud, out, scenario, inputs, run, profile, node, can_bitrate,
                 expected_version, fixture=None, timeout_s=15.0, startup_s=0.2,
                 firmware=None, request_timeout_ms=1000, port_opener=probe.open_port,
                 console_factory=probe.Console, clock=time.monotonic, evidence_kind="live-observed"):
    """Reserve all three artifacts before port ownership; persist and re-raise failures.

    One explicit DEC-1 probe is the first active upload. Other snapshots are cached;
    a declared device fixture additionally gets an ordinary typed identity read.
    Supplied declarations never become observed model/firmware/SDK qualification.
    Session failures perform no cleanup: only run(console,record) may own that plan.
    """
    _arguments(port=port, baud=baud, profile=profile, node=node, can_bitrate=can_bitrate,
               expected_version=expected_version, timeout_s=timeout_s, startup_s=startup_s,
               evidence_kind=evidence_kind, inputs=inputs)
    if type(scenario) is not str or not 1 <= len(scenario) <= 100 or not callable(run):
        raise ValueError("finite named scenario and callback required")
    if type(request_timeout_ms) is not int or not 1 <= request_timeout_ms <= 60000:
        raise ValueError("read budget must be1..60000ms")
    if fixture is not None:
        probe.validate_fixture(fixture)
        _json(fixture)
    image = _image(firmware)  # No implicit host inventory.
    prefix = Path(out)
    if not prefix.parent.is_dir() or prefix.name in ("", ".", ".."):
        raise ValueError("artifact prefix needs an existing parent directory")
    paths = [prefix.with_suffix(suffix) for suffix in (".json", ".jsonl", ".zip")]
    if any(path.exists() for path in paths):
        raise FileExistsError("session artifact already exists; choose a new output prefix")
    record = {}
    with _reserve_artifacts(paths, record) as (summary, transcript, archive):
        # Text wrapper is deliberately separate from owned binary evidence/package handles.
        import io
        text_log = io.TextIOWrapper(transcript, encoding="ascii", newline="\n", write_through=True)
        evidence = probe.EvidenceLog(file=text_log)
        record.update(scenario=scenario, inputs=inputs, fixture=fixture, evidence_kind=evidence_kind,
                      fixture_provenance="caller-supplied; neither observed identity nor native permission",
                      ok=False, verdict="failed", workload_verified=False, operations=[],
                      operations_seen=0, operations_failed=0, automatic_recovery=False,
                      automatic_write_replay=False, hardware_qualified=False,
                      physical_observation=fixture.get("physical_observation") if fixture else None,
                      sdk_identity=fixture.get("sdk_identity") if fixture else None, firmware_epoch=None,
                      firmware_artifact=image, artifacts=dict(zip=(str(paths[2])), summary=str(paths[0]), transcript=str(paths[1])))
        console, failure = None, None
        started = clock()
        try:
            evidence("session_intent", scenario=scenario, inputs=inputs,
                     fixture_sha256=hashlib.sha256(_json(fixture)).hexdigest() if fixture else None,
                     profile=profile, node=node, serial_baud=baud, can_bitrate=can_bitrate,
                     expected_library_version=expected_version, timeout_s=timeout_s)
            with port_opener(port, baud, timeout_s) as connection:
                try:
                    console = console_factory(connection, on_event=evidence)
                    console.drain_startup(startup_s)
                    record["identity"] = console.identify(expected_version=expected_version, profile=profile, timeout_s=timeout_s)
                    console.check_config(console.handshake["config"], can_bitrate=can_bitrate)
                    probe.checked(console, f"useaddr {node}", record=record, timeout_s=timeout_s, role="selection")
                    config = probe.checked(console, "config", record=record, timeout_s=timeout_s, role="selection")
                    console.check_config(config, node=node, can_bitrate=can_bitrate)
                    record["before"] = {name: probe.checked(console, name, record=record, timeout_s=timeout_s, role="cached") for name in CACHED}
                    record["cached_axis"] = None
                    if fixture is not None and console.capabilities.get("axis", {}).get("implemented") is True:
                        probe.checked(console, "axis config", record=record, timeout_s=timeout_s, role="cached")
                        record["cached_axis"] = record["operations"][-1]["records"]
                    probe.checked(console, f"probe {request_timeout_ms}", record=record, timeout_s=timeout_s, role="preflight")
                    if fixture is not None and scenario != "quick":
                        probe.checked(console, f"read identity {request_timeout_ms}", record=record, timeout_s=timeout_s, role="preflight")
                    run(console, record)
                    if record.get("workload_verified") is not True or record.get("primary_error") or record.get("cleanup_error"):
                        raise probe.BenchError("requested finite workload did not complete; cleanup is not a pass")
                    record["after"] = {name: probe.checked(console, name, record=record, timeout_s=timeout_s, role="cached") for name in CACHED}
                    record["ok"], record["verdict"] = True, evidence_kind
                except BaseException as error:
                    failure = error
                    record.setdefault("primary_error", str(error) or type(error).__name__)
                    raise
        except BaseException as error:
            record["ok"] = False
            if failure is not None and error is not failure:
                record["owner_error"] = str(error) or type(error).__name__
            else:
                failure = error
            record.setdefault("primary_error", str(failure) or type(failure).__name__)
            record["verdict"] = "interrupted" if isinstance(failure, KeyboardInterrupt) else "failed"
        finally:
            record["elapsed_s"] = max(0, clock() - started)
            record["client"] = console.diagnostics() if console else None
            record["evidence"] = dict(events=evidence.events, bytes=evidence.bytes,
                                      exhausted=evidence.exhausted, failed=evidence.failed,
                                      max_events=2048, max_bytes=2 * 1024 * 1024)
            try:
                summary.write(_json(record))
                summary.flush()
                text_log.flush()
                text_log.detach()  # Package the exact bytes through the still-owned binary stream.
                package_report(summary, transcript, archive)
            except BaseException as artifact_error:
                record["ok"], record["verdict"] = False, "failed"
                record["artifact_error"] = str(artifact_error) or type(artifact_error).__name__
                if failure is None:
                    failure = artifact_error
                    record.setdefault("primary_error", record["artifact_error"])
                try:
                    archive.seek(0)
                    archive.truncate(0)  # Invalidate any ZIP containing a premature passing summary.
                    archive.flush()
                    summary.seek(0)
                    summary.truncate()
                    summary.write(_json(record))
                    summary.flush()
                except BaseException:
                    pass  # Preserve the original error when the evidence medium also fails.
            try:
                text_log.detach()
            except (ValueError, OSError):
                pass  # Already detached on the normal packaging path.
            if failure is not None:
                raise failure
        return record


def run_cli(args, scenario):
    """Validate complete CLI candidates before reservation/serial; exit0 plan/pass,1 failure,2 input."""
    executing = False
    try:
        fixture = load_fixture(args.fixture) if args.fixture is not None else None
        if args.expected_library_version is not None and not probe.re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", args.expected_library_version):
            raise ValueError("expected-library-version requires exact major.minor.patch")
        if fixture is not None:
            scenario.validate_fixture(fixture)
            for capability in scenario.required_capabilities:
                probe.validate_fixture(fixture, capability)
        if args.plan_only:
            plan = dict(scenario.plan, session_commands=["version", "profile list", "caps", "config", "useaddr NODE", *CACHED,
                         f"probe {args.request_timeout_ms}"] + ([f"read identity {args.request_timeout_ms}"] if fixture and scenario.name != "quick" else []),
                        fixture=fixture, fixture_observed=False, firmware_epoch=None)
            print(json.dumps(plan, allow_nan=False, ensure_ascii=True, indent=2))
            return 0
        for name in ("port", "baud", "profile", "node", "can_bitrate", "expected_library_version", "out"):
            if getattr(args, name) is None:
                raise ValueError("live execution requires --" + name.replace("_", "-"))
        scenario.validate_fixture(fixture)
        for capability in scenario.required_capabilities:
            probe.validate_fixture(fixture, capability)
        executing = True
        record = run_recorded(port=args.port, baud=args.baud, out=args.out, scenario=scenario.name,
            inputs=scenario.inputs, run=scenario.run, profile=args.profile, node=args.node,
            can_bitrate=args.can_bitrate, expected_version=args.expected_library_version, fixture=fixture,
            timeout_s=args.timeout, startup_s=args.startup, firmware=args.firmware,
            request_timeout_ms=args.request_timeout_ms)
        print(f"{record['verdict']}: {args.out.with_suffix('.json')} (not hardware qualification)")
        return 0
    except (ValueError, OSError, AssertionError) as error:
        print(str(error), file=sys.stderr)
        return 1 if executing else 2
    except (probe.BenchError, ImportError, KeyboardInterrupt) as error:
        print(str(error) or "interrupted", file=sys.stderr)
        return 1


def main(argv=None):
    """Session CLI uses the same named plans; --list belongs only to bench_scenarios."""
    from bench_scenarios import SCENARIOS, add_campaign_arguments, build_scenario
    parser = argparse.ArgumentParser(description=__doc__)
    add_session_arguments(parser)
    parser.add_argument("--scenario", choices=tuple(SCENARIOS), default="quick")
    add_campaign_arguments(parser)
    args = parser.parse_args(argv)
    try:
        scenario = build_scenario(args)
    except (ValueError, argparse.ArgumentTypeError) as error:
        parser.error(str(error))
    return run_cli(args, scenario)


if __name__ == "__main__":
    raise SystemExit(main())
