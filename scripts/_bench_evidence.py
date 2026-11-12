# SPDX-License-Identifier: MIT
"""Pure summaries of supplied MotorControl-CAN console evidence.

The Console owns wire validation, correlation, deadlines and bounded raw lines.
These helpers consume its checked records without opening a port, reading a
clock, retrying work or inferring motor state. Native status, command outcome,
execution/completion and uncertainty remain separate. A terminal reports the
firmware's evidence; it never proves physical standstill or qualification.

Summaries own their JSON values. Decimal strings and Python integers keep their
exact values, including numbers above 2**53. Record/provenance copies are bounded
to 256 records, 262144 serialized ASCII JSON bytes and 16 container levels.
"""

from __future__ import annotations

import json
import math


MAX_SUMMARY_RECORDS = 256
MAX_SUMMARY_BYTES = 262144
MAX_SUMMARY_DEPTH = 16
MAX_OPERATIONS = 16
EVIDENCE_KINDS = ("planned", "skipped", "host-tested", "live-observed")
INTERRUPTIONS = (None, "framing_loss", "reboot", "disconnect", "deadline")


class BenchError(RuntimeError):
    """Supplied evidence or a bounded console operation cannot be accepted."""


def unique_object(pairs: list[tuple[str, object]]) -> dict:
    """JSON object hook rejecting ambiguous repeated keys, as in the RS client."""
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def invalid_constant(value: str) -> None:
    """JSON parse-constant hook rejecting NaN and infinity spellings."""
    raise ValueError(f"invalid JSON number: {value}")


def operation_id(value: object) -> bool:
    """Whether a value is an actual nonzero uint32 operation/request identity."""
    return type(value) is int and 1 <= value <= 0xFFFFFFFF


def _take(budget: list[int], size: int) -> None:
    budget[0] -= size
    if budget[0] < 0:
        raise BenchError("evidence exceeds the summary byte bound")


def _owned_json(value: object, budget: list[int], depth: int = 0):
    """Copy only JSON primitives while accounting for encoded size before copying."""
    if type(value) in (dict, list):
        if depth >= MAX_SUMMARY_DEPTH:
            raise BenchError("evidence exceeds the summary nesting bound")
        _take(budget, 2)
        if type(value) is list:
            result = []
            for index, item in enumerate(value):
                if index:
                    _take(budget, 1)
                result.append(_owned_json(item, budget, depth + 1))
            return result
        result = {}
        for index, (key, item) in enumerate(value.items()):
            if type(key) is not str:
                raise BenchError("evidence object keys must be strings")
            if index:
                _take(budget, 1)
            _owned_json(key, budget, depth + 1)
            _take(budget, 1)
            result[key] = _owned_json(item, budget, depth + 1)
        return result
    if value is not None and type(value) not in (bool, int, float, str):
        raise BenchError("evidence contains a non-JSON value")
    if type(value) is float and not math.isfinite(value):
        raise BenchError("evidence contains a non-finite number")
    if type(value) is str and len(value) > budget[0]:
        raise BenchError("evidence exceeds the summary byte bound")
    if type(value) is int and value.bit_length() > MAX_SUMMARY_BYTES * 4:
        raise BenchError("evidence integer exceeds the summary byte bound")
    try:
        encoded = json.dumps(value, ensure_ascii=True, allow_nan=False)
    except (ValueError, OverflowError) as error:
        raise BenchError("evidence cannot be represented as bounded JSON") from error
    _take(budget, len(encoded))
    return value


def summarize_operation(records: list[dict], *, intent: str, evidence_kind: str,
                        request_id: int | None = None,
                        operation_id: int | None = None, transmitted: bool = False,
                        interruption: str | None = None,
                        provenance: dict | None = None) -> dict:
    """Own a bounded summary of one checked command and its actual records.

    ``records`` belongs to one already-correlated command, including any result
    lookup. The Console validates schema/identity, ordering, duplicate terminals
    and native field types before calling. This helper rejects non-JSON values
    and storage overflow; it is not a second wire validator.

    ``transmitted`` means a serial write was attempted, not CAN transmission or
    acceptance. ``operation_id`` is an externally confirmed admitted identity,
    if supplied. Missing acceptance after a write and interruption therefore
    remains unknown. ``admission`` means confirmed admission; false alone does
    not prove that unacknowledged work was refused. Native ``uncertain`` is
    copied verbatim, or null if absent;
    the classification also expresses unknown handoff without inventing native
    uncertainty evidence. A later stream failure cannot erase a known terminal.

    Evidence kind is an explicit caller claim: planned/skipped have no records or
    write attempt; host-tested never becomes live-observed through successful
    parsing. Provenance is supplied metadata, not discovered hardware identity.
    Raises BenchError atomically on invalid arguments or exceeded copy bounds.
    """
    if type(records) is not list or len(records) > MAX_SUMMARY_RECORDS:
        raise BenchError("operation records exceed the summary record bound")
    if any(type(record) is not dict for record in records):
        raise BenchError("operation records must be JSON objects")
    if type(intent) is not str or not intent:
        raise BenchError("operation intent must be a nonempty string")
    if evidence_kind not in EVIDENCE_KINDS or type(evidence_kind) is not str:
        raise BenchError("unknown evidence kind")
    if type(transmitted) is not bool or interruption not in INTERRUPTIONS:
        raise BenchError("invalid write-attempt or interruption metadata")
    for identity in (request_id, operation_id):
        if identity is not None and not (type(identity) is int and 1 <= identity <= 0xFFFFFFFF):
            raise BenchError("summary identity must be a nonzero uint32 integer")
    if provenance is not None and type(provenance) is not dict:
        raise BenchError("provenance must be a JSON object")
    if evidence_kind in ("planned", "skipped") and (
            records or transmitted or operation_id is not None or interruption is not None):
        raise BenchError("planned/skipped evidence cannot contain execution")
    receipt = next((record for record in records if record.get("result") == "accepted"), None)
    terminal = next((record for record in records if record.get("type") == "terminal" or (
        record.get("command") == "result" and "command_id" in record
        and "outcome" in record)), None)
    admitted = operation_id is not None or receipt is not None or terminal is not None
    final = terminal if terminal is not None else (records[-1] if records else {})
    if operation_id is None:
        operation_id = (terminal or receipt or {}).get("operation_id")
    if request_id is None:
        request_id = (terminal or receipt or {}).get("command_id")
        if request_id is None and records:
            request_id = records[0].get("id")
    if evidence_kind in ("planned", "skipped"):
        classification = evidence_kind
    elif terminal is not None:
        classification = ("unknown_after_admission" if terminal.get("uncertain") is True else
                          "native_rejection" if terminal.get("abort_code") is not None else "terminal")
    elif admitted:
        classification = "unknown_after_admission" if interruption else "pending"
    elif any(record.get("type") == "reply" and record.get("ok") is False for record in records):
        classification = "local_refusal"
    elif any(record.get("type") == "reply" and record.get("ok") is True
             and record.get("result") not in ("accepted", "pending") for record in records):
        classification = "local_reply"
    elif transmitted:
        classification = "unknown_after_handoff"
    else:
        classification = "no_admission"
    summary = dict(intent=intent, evidence_kind=evidence_kind,
                request_id=request_id, operation_id=operation_id, transmitted=transmitted,
                admission=admitted, terminal_known=terminal is not None,
                classification=classification, interruption=interruption,
                stream_integrity="lost" if interruption in ("framing_loss", "reboot", "disconnect") else "intact",
                status=final.get("status"), detail=final.get("detail"),
                outcome=final.get("outcome"), uncertain=final.get("uncertain"),
                abort_code=final.get("abort_code"), abort_known=final.get("abort_code") is not None,
                execution_code=final.get("execution_code"), completion_code=final.get("completion_code"),
                native_operation=final.get("native_operation"),
                provenance=provenance or {}, records=records)
    return _owned_json(summary, [MAX_SUMMARY_BYTES])


def summarize_execution(operations: list[dict], *, provenance: dict | None = None) -> dict:
    """Own up to 16 operation summaries without an aggregate device verdict.

    Counts retain each explicit evidence kind and classification. No count or
    successful host terminal promotes a plan/test into live hardware evidence.
    The entire copied execution shares the byte/nesting bounds above.
    """
    if type(operations) is not list or len(operations) > MAX_OPERATIONS:
        raise BenchError("execution exceeds the operation bound")
    if provenance is not None and type(provenance) is not dict:
        raise BenchError("provenance must be a JSON object")
    for operation in operations:
        if (type(operation) is not dict or operation.get("evidence_kind") not in EVIDENCE_KINDS
                or type(operation.get("classification")) is not str):
            raise BenchError("execution requires operation summaries")
    evidence_counts = {kind: 0 for kind in EVIDENCE_KINDS}
    classification_counts = {}
    for operation in operations:
        evidence_counts[operation["evidence_kind"]] += 1
        classification = operation["classification"]
        classification_counts[classification] = classification_counts.get(classification, 0) + 1
    summary = dict(operations=operations, provenance=provenance or {},
                   operation_count=len(operations), evidence_counts=evidence_counts,
                   classification_counts=classification_counts)
    return _owned_json(summary, [MAX_SUMMARY_BYTES])
