#!/usr/bin/env python3
"""Replay retained S2 bench captures offline; never opens a port or edits evidence.

A capture is an output directory of scripts/s2_bench.py: rx.bin (authoritative
bytes), rx.txt and request.json. GRAMMAR below is the whole record format: to
support a new firmware record add one row, and name its signals to have it reach
the transition log. Lines matching no row are preserved and counted, never dropped.

Verdicts are separate facts. "complete" means a consistent, fully terminated
capture of a successful record, not a qualified device behavior. A raw upload is
never a typed success, whatever its bytes: only a typed EVIDENCE terminal counts
as one, and width disagreements with the EDS stay listed.
"""

import argparse
from collections import Counter
from dataclasses import dataclass, field
from functools import lru_cache
import hashlib
import json
from pathlib import Path
import sys
from typing import NamedTuple

from s2_bench import COMMANDS


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "docs/bench/SHA256SUMS.json"
TIME_BASIS = "firmware processing microseconds; receive bounds are separate, no wall clock"

# Firmware enumerations as currently declared. Older images may number these
# differently, so raw values are always retained next to the current name.
OUTCOMES = ("SUCCESS", "SERVER_ABORT", "CLIENT_ABORT", "TIMEOUT", "CANCELLED", "BUS_OFF", "REBOOT",
            "BINDING_INVALIDATED", "CORRELATION_LOST", "TRANSPORT_ERROR", "UNSUPPORTED",
            "UNCONFIRMED_SENT")
FAILURES = ("NONE", "AUTHORIZATION", "ADMISSION", "SERVICE", "CORRELATION", "EVIDENCE_LOSS",
            "INVALIDATED", "DEADLINE", "READBACK")
# BenchRoute RouteFailure, printed in a TRIAL header.
TRIAL_FAILURES = ("NONE", "AUTHORIZATION", "ADMISSION", "SERVICE", "CORRELATION", "EVIDENCE_LOSS",
                  "INVALIDATED", "DEADLINE", "PREREQUISITE", "OBSERVATION", "ACTION", "REFUSED", "ABORTED")
RX, TX, TX_REPORT, TERMINAL, CONTROLLER, RECOVERY = range(6)  # owner DiagnosticKind

# Native values worth tracking as status. Anything else stays in the record list.
STATUS_OBJECTS = {
    (0x6041, 0): "statusword_6041", (0x6061, 0): "mode_display_6061", (0x603F, 0): "fault_code_603f",
    (0x1001, 0): "error_register_1001", (0x1017, 0): "heartbeat_ms_1017",
    (0x2100, 0): "monitor_2100", (0x2101, 0): "monitor_2101", (0x2102, 0): "monitor_2102",
    (0x60FD, 0): "digital_inputs_60fd",
}


# ---------------------------------------------------------------- record grammar
def _result_signals(record, node):
    """A failure is a transition too: terminal outcome and uncertainty are health."""
    values = record.values
    out = [("health", "sdo_outcome", node, values["outcome"]),
           ("health", "effect_uncertain", node, values["uncertain"])]
    name = STATUS_OBJECTS.get(tuple(values["object"]))
    if name and values["outcome"] == 0:
        out.append(("status", name, node, "0x" + _value(values).hex()))
    return out


def _value(values):
    """Actual indicated bytes as most-significant-first, for a readable native value."""
    return bytes(reversed(bytes.fromhex(values["data"])[:values["size"]]))


def _frame_signals(record, _node):
    values, kind = record.values, record.values["k"]
    identifier = values.get("id", 0)
    payload = bytes.fromhex(values["data"])
    if kind == RX and 0x700 < identifier <= 0x77F and values.get("dlc") == 1:
        return [("nmt", "wire_state", identifier - 0x700, "0x%02x" % payload[0])]
    if kind == RX and 0x80 < identifier <= 0xFF:
        return [("health", "emcy", identifier - 0x80, payload.hex())]
    if kind == TX:
        return [("controller", "tx_enqueue_ok", None, values["ok"])]
    if kind == TX_REPORT:
        return [("controller", "tx_report_ok", None, values["ok"])]
    if kind == CONTROLLER:
        return [("controller", "controller_state", None, values.get("ctrl", record.raw))]
    if kind == RECOVERY:
        return [("controller", "recovery", None, values.get("rec", record.raw))]
    return []


def _event_signals(record, _node):
    values = record.values
    identifier, payload = values["id"], bytes.fromhex(values["data"])
    if 0x700 < identifier <= 0x77F:
        return [("nmt", "wire_state", identifier - 0x700, "0x%02x" % payload[0]),
                ("nmt", "event_disposition", identifier - 0x700, values["disposition"])]
    if 0x80 < identifier <= 0xFF:
        return [("health", "emcy", identifier - 0x80, payload.hex())]
    return [("nmt", "event_other", None, record.raw)]


def _boot_signals(record, _node):
    return [("health", "boot", None, record.raw)]


# Soak status names, by the question they answer. A value signal keeps its object in its name.
SOAK_HEALTH = {"health", "streak", "lastError"}
SOAK_NETWORK = {"nmt", "eligibility", "heartbeat", "unresolved", "nodeEpoch", "serverSessions", "producer"}
SOAK_VALUES = {"6041", "6061", "2100", "2101", "2102", "6064", "606c", "1000", "1009", "100a", "1018:00",
               "1018:01", "1018:02", "1018:03", "2000", "2001"}
SOAK_INTERFACE = {"controller", "controllerEpoch", "inhibited", "recovery", "firmware", "busOff",
                  "recoveries", "twaiErrors", "twaiFlags", "twaiDropped", "rxDropped", "rxLost",
                  "txRejected", "unmatched", "queueRejected", "resultsRejected", "traceOverwritten",
                  "networkDropped", "submitOverwritten"}


def _soak_category(signal):
    return ("health" if signal in SOAK_HEALTH else "nmt" if signal in SOAK_NETWORK else
            "status" if signal in SOAK_VALUES else "controller")


def _soak_transition(record, _node):
    values = record.values
    return [(_soak_category(values["sig"]), values["sig"], values["node"] or None, "0x%x" % values["to"])]


def _soak_emcy(record, _node):
    return [("health", "emcy", record.values["node"], record.values["data"])]


def _soak_boot(record, _node):
    return [("health", "boot_up", record.values["node"], record.values["data"])]


def _soak_stop(record, _node):
    return [("health", "reads_stopped", record.values["node"], record.values["reason"])]


class Row(NamedTuple):
    """One line form. fields tokens are name:type, optional ?, optional >canonical name."""
    kind: str  # Record kind; several rows may share one across firmware versions.
    head: str  # Exact line prefix.
    role: str  # start, refused, header, terminal, traffic, tail, end, response, boot, error.
    fields: str = ""  # Empty for a fixed text line.
    time: str = ""  # Field holding firmware processing time, if any.
    signals: object = ()  # (category, field, ...) names, or a function(record, node).
    per_node: bool = False  # Tuple signals belong to this record's node field.


# epoch is the controller epoch of that frame. txk/txr/txf are its local TX identity.
_FRAME = ("k:u p:u id:x ext:b rtr:b dlc:u data:h ok:b exact:b t:u bounds:b lo:u hi:u "
          "epoch:u? step:u? op:u? ctrl:s? rec:s? txk:u? txr:u? txf:u? txc:u?")
_FRAME_OLD = "kind:u>k time:u>p id:x ext:b rtr:b dlc:u data:h accepted:b>ok captureKnown:b>exact"
# req is the transfer's request, oreq the owner's result handle, ne the node epoch at admission,
# reason the owner's closure and st its stages: bit0 adapter admitted, bit1 controller accepted,
# bit2 queued, bit3 possible TX, bit4 confirmed TX, bit5 acknowledged, bit6 uncertain, bit7 cleanup.
_RESULT = ("op:u? req:u? oreq:u? step:u object:o dir:u? outcome:u ack:b possible:b uncertain:b abort:x "
           "cleanup:b? size:u data:h admitted:u completed:u epoch:u? ne:u? reason:u? st:x? node:u?")
_STATS = (("controller", "running"), ("controller", "inhibited"), ("controller", "flags"),
          ("health", "dropped"), ("health", "errors"), ("health", "stackFree"))
_PONG = (("health", "base"), ("health", "elf"), ("health", "sdk"), ("controller", "running"),
         ("controller", "active"), ("controller", "silent"), ("controller", "pinsReady"))
_NETWORK = (("nmt", "state"), ("nmt", "qualified"), ("nmt", "pending"), ("nmt", "observed"),
            ("nmt", "expired"), ("nmt", "producerKnown"), ("nmt", "ms"), ("nmt", "epoch"),
            ("health", "dropped"))
_RECORD_HEALTH = (("health", "failure"), ("health", "lost"), ("health", "tailLost"))
# One fresh state read of a trial route: blocks present, then raw 1001/603F/2100/6041/6061/6064/606C.
_TRIAL_SNAPSHOT = "passed:b blocks:x 1001:x 603f:x 2100:x 6041:x 6061:x 6064:i 606c:i"
_TRIAL_VALUES = (("status", "2100"), ("status", "6041"), ("status", "6061"))

GRAMMAR = (
    # Attended bench routes (examples/s2_bench/BenchRoute.h). The admission row precedes the
    # header row, whose prefix it shares. Summaries sit between the header and the terminals.
    Row("ADMITTED", "TRIAL admitted", "start", "route:s node:u trial:u motion:u case:u"),
    Row("REFUSED", "TRIAL_REFUSED ", "refused", "reason:s"),
    Row("TRIAL_ABORTING", "TRIAL_ABORTING", "response"),
    Row("TRIAL_ABORT_REFUSED", "TRIAL_ABORT refused", "response", "reason:s"),
    # lossCancel: lost evidence withdrew an active enable or move locally (no stop, nothing sent).
    Row("TRIAL", "TRIAL ", "header",
        "id:u node:u route:s failure:u why:s lost:b terminals:u traffic:u tail:u tailLost:b summaries:u "
        "step:u cycles:u motion:u case:u unsettled:b gap:u lossCancel:b?",
        signals=_RECORD_HEALTH + (("health", "why"), ("health", "lossCancel")), per_node=True),
    # pred: a rearm's named predecessor; c6: word67's cycles after 0006; rel and base: a move's relative
    # base (0 absolute) and witness position. Records before 2026-11-04 have none of them. reverse (protocol
    # 12): a velocity's reverse probe, with which its rpm is the magnitude of a negative rate.
    Row("TRIAL_PLAN", "TRIAL_PLAN ", "summary",
        "route:s node:u trial:u attended:b cycles:u? interval:u? start:u? shaft:u? status:x? pt:i? vt:i? "
        "deadline:u? kind:u? be:u? sp:i? target:i? rpm:u? up:u? down:u? after:u? sd:u? si:u? pred:u? c6:u? "
        "rel:u? base:i? code:u? case:u? obs:u? dur:u? method:u? search:u? creep:u? ramp:u? offset:i? value:i? "
        "probe:u? reverse:b? word:x?"),
    Row("TRIAL_RES", "TRIAL_RES ", "summary",
        "stack0:u stack1:u heap0:u heap1:u heapMin:u largest:u gap:u dropped:u admission:u started:u completed:u "
        "tec0:u? rec0:u? tec1:u? rec1:u?",
        "completed", (("health", "stack1"), ("health", "heapMin")), True),
    # The first of two fresh check reads (released-quick, word67, a relative move); the check judged both.
    Row("TRIAL_FIRST", "TRIAL_FIRST ", "summary", _TRIAL_SNAPSHOT),
    Row("TRIAL_CHECK", "TRIAL_CHECK ", "summary", _TRIAL_SNAPSHOT),
    Row("TRIAL_LAST", "TRIAL_LAST ", "summary", _TRIAL_SNAPSHOT, signals=_TRIAL_VALUES, per_node=True),
    # The history route's 1003, raw: the count, then only the entries it names. Never interpreted.
    Row("TRIAL_HISTORY", "TRIAL_HISTORY ", "summary", "count:u e1:x? e2:x? e3:x? e4:x?"),
    Row("TRIAL_EMCY", "TRIAL_EMCY ", "summary", "data:h", signals=(("health", "data"),), per_node=True),
    Row("TRIAL_STATUS", "TRIAL_STATUS ", "summary", "code:u detail:i"),
    # The enable, stop and move summaries publish their results under names of their own: one record can
    # hold a move and its planned stop, and the transition log keys on (category, signal, node).
    Row("TRIAL_ENABLE", "TRIAL_ENABLE ", "summary",
        "own:b state:u outcome:u>enableOutcome exec:u completion:u uncertain:b>enableUncertain effects:b mask:x "
        "cursor:u observations:u op:u pt:i vt:i s0:x m0:x p0:i",
        signals=(("health", "enableOutcome"), ("health", "enableUncertain")), per_node=True),
    Row("TRIAL_ENABLE_STEP", "TRIAL_ENABLE_STEP ", "summary",
        "word:u present:b status:x monitor:x position:i velocity:i history:b outcome:u ack:b possible:b "
        "uncertain:b"),
    Row("TRIAL_STOP", "TRIAL_STOP ", "summary",
        "kind:u path:u>stopPath final:u own:b state:u outcome:u>stopOutcome exec:u completion:u uncertain:b "
        "effects:b pred:x documented:b basis:u observations:u op:u",
        signals=(("health", "stopPath"), ("health", "stopOutcome")), per_node=True),
    Row("TRIAL_BEST_EFFORT", "TRIAL_BEST_EFFORT ", "summary",
        "which:u own:b state:u outcome:u exec:u admitted:b effects:b op:u"),
    # The predecessor a rearm followed (pword from 2026-11-04 on), and the session's latest word after
    # it: none after a lost 000F.
    Row("TRIAL_REARM", "TRIAL_REARM ", "summary", "pword:x? predop:u predus:u latest:b word:x latestop:u",
        signals=(("health", "latest"), ("status", "word")), per_node=True),
    # released-quick and word67: the session's latest word at admission, which may be none, and after.
    Row("TRIAL_CONTROL", "TRIAL_CONTROL ", "summary", "prior:b pword:x pop:u pus:u latest:b word:x latestop:u",
        signals=(("health", "latest"), ("status", "word")), per_node=True),
    # relative and a relative move: the 607A read before the writes, the firmware's own record of the
    # last 607A it had acknowledged on that node, and a relative move's witness. Never a base measurement.
    Row("TRIAL_BASE", "TRIAL_BASE ", "summary",
        "read:b target:i known:b prev:i prevTrial:u prevRelative:b basis:u witness:i"),
    # The host's move evidence as the firmware received it, then the session's MoveContext and its
    # five kept samples (admission, latest, acceptance, absence, completed), raw.
    # settings-2202, io-di3 and subdivision: the transaction's result, its configuration epochs and whether the
    # owner and the session followed; then the group raw, which=0 the prior read and which=1 the matched
    # readback (subdivision: TRIAL_DRIVER below).
    Row("TRIAL_SETTINGS", "TRIAL_SETTINGS ", "summary",
        "prepared:b noop:b state:u outcome:u>settingsOutcome changed:x confirmed:x effects:b "
        "uncertain:b>settingsUncertain cfg0:u cfg1:u>configuration followed:b",
        signals=(("health", "settingsOutcome"), ("health", "settingsUncertain"), ("status", "configuration")),
        per_node=True),
    Row("TRIAL_LIMITS", "TRIAL_LIMITS ", "summary", "which:u present:b 2202:x entries:u min:i max:i"),
    Row("TRIAL_IO", "TRIAL_IO ", "summary",
        "which:u present:b entries:u polarity:x di0:x di1:x di2:x di3:x di4:x di5:x di6:x"),
    Row("TRIAL_INPUTS", "TRIAL_INPUTS ", "summary", "present:b 2101:x 60fd:x",
        signals=(("status", "2101"), ("status", "60fd")), per_node=True),
    # A move's observer node: its check, its cycles summarized (recorded, never judged), its check and
    # last reads, then both nodes' copied RX and unaccepted counts and the interface's own counters.
    Row("TRIAL_OBSERVER", "TRIAL_OBSERVER ", "summary",
        "node:u checked:b passed:b why:s cycles:u stopped:b moved:b>observerMoved pmin:i pmax:i vmax:i or2100:x "
        "emcy:b edata:h", signals=(("status", "observerMoved"),), per_node=True),
    Row("TRIAL_OBSERVER_CHECK", "TRIAL_OBSERVER_CHECK ", "summary", _TRIAL_SNAPSHOT),
    Row("TRIAL_OBSERVER_LAST", "TRIAL_OBSERVER_LAST ", "summary", _TRIAL_SNAPSHOT),
    Row("TRIAL_NODES", "TRIAL_NODES ", "summary",
        "gap:u n1rx:u n1unmatched:u n2rx:u n2unmatched:u dropped0:u dropped1:u unmatched0:u unmatched1:u "
        "lost0:u lost1:u"),
    # counter-zero and clear-alarm: the session's clear context, each write by slot, the alarm incident
    # or the counter baseline, and the extra observation, all raw. method (protocol 10): the context's
    # AlarmClearMethod, 0 for 2301 and 1 for 6040 bit 7; TRIAL_PLAN's method is the line's 1 or 2.
    Row("TRIAL_CLEAR", "TRIAL_CLEAR ", "summary",
        "own:b kind:u state:u outcome:u>clearOutcome exec:u completion:u uncertain:b>clearUncertain effects:b "
        "one:b>valueOneMayRemain coords:b final:b observations:u op:u blocked:b>clearBlocked method:u?",
        signals=(("health", "clearOutcome"), ("health", "clearUncertain"), ("health", "valueOneMayRemain"),
                 ("health", "clearBlocked")), per_node=True),
    Row("TRIAL_CLEAR_STEP", "TRIAL_CLEAR_STEP ", "summary",
        "slot:u present:b object:o value:x outcome:u ack:b possible:b uncertain:b"),
    Row("TRIAL_INCIDENT", "TRIAL_INCIDENT ", "summary",
        "present:b 1001:x 603f:x count:u 2100:x 2101:x 2102:x 6041:x 6061:x"),
    Row("TRIAL_BASELINE", "TRIAL_BASELINE ", "summary", "present:b 6064:i 606c:i"),
    Row("TRIAL_EXTRA", "TRIAL_EXTRA ", "summary", "block:u fields:u 1001:x 603f:x 6064:i 606c:i"),
    Row("TRIAL_EVIDENCE", "TRIAL_EVIDENCE ", "summary",
        "record:u as:u am:x av:x cs:u cm:x cv:x rule:u dt:x hw:x sw:x vid:x pc:x rev:x mc:x fc:x rb:u?"),
    Row("TRIAL_MOVE", "TRIAL_MOVE ", "summary",
        "own:b state:u phase:u phases:x outcome:u>moveOutcome int:u setup:u exec:u completion:u>moveCompletion "
        "rearm:u uncertain:b>moveUncertain readUnc:b effects:b cycles:u op:u target:i mask:x ev:x trigger:b "
        "rearmed:b fenced:b>moveFenced code:u detail:i",
        signals=(("health", "moveOutcome"), ("health", "moveCompletion"), ("health", "moveUncertain"),
                 ("health", "moveFenced")),
        per_node=True),
    Row("TRIAL_MOVE_SAMPLE", "TRIAL_MOVE_SAMPLE ", "summary",
        "which:u present:b cycle:u status:x monitor:x mode:x feedback:b position:i velocity:i lo:u hi:u"),
    # home (protocol 6): the reads requestHome consumed and its refusal; gate is always 0 (CLOSED).
    Row("TRIAL_HOME", "TRIAL_HOME ", "summary",
        "method:u gate:u identity:b io:b state:b driver:b 2101:x 2200:x 2201:x admitted:b"),
    # velocity (protocol 6; protocol 7 adds gate, always 2 ATTENDED_EXPERIMENT, with start and stop 0;
    # protocol 9 adds probe, prompt 59's range probe, also on TRIAL_PLAN; protocol 12 adds reverseProbe,
    # prompt 59's reverse probe, TRIAL_PLAN's reverse, while reverse stays the qualification, always 0): the
    # host's evidence as received, the session's VelocityContext, its preparation-anchored timeline and its
    # admission and latest samples, raw. TRIAL_VELOCITY's rpm is the signed rate. Its stop is TRIAL_STOP.
    Row("TRIAL_VELOCITY_EVIDENCE", "TRIAL_VELOCITY_EVIDENCE ", "summary",
        "record:u layout:u gate:u start:b stop:b kind:u max:u reverse:b modeChange:b dt:x hw:x sw:x vid:x pc:x "
        "rev:x mc:x fc:x probe:b? reverseProbe:b?"),
    Row("TRIAL_VELOCITY", "TRIAL_VELOCITY ", "summary",
        "own:b state:u phase:u outcome:u>velocityOutcome first:u int:u setup:u exec:u completion:u>velocityCompletion "
        "stop:u sexec:u uncertain:b>velocityUncertain effects:b needs:b fenced:b>velocityFenced running:b cycles:u "
        "polls:u op:u sop:u rpm:i code:u detail:i",
        signals=(("health", "velocityOutcome"), ("health", "velocityCompletion"), ("health", "velocityUncertain"),
                 ("health", "velocityFenced")), per_node=True),
    Row("TRIAL_VELOCITY_TIME", "TRIAL_VELOCITY_TIME ", "summary",
        "started:u due:u deadline:u stopAt:u serviced:u smask:x cmask:x"),
    Row("TRIAL_VELOCITY_SAMPLE", "TRIAL_VELOCITY_SAMPLE ", "summary",
        "which:u present:b cycle:u status:x monitor:x mode:x position:i velocity:i lo:u hi:u"),
    # Backlog S17 (records from 2026-11-09 after trial 1310): the run's final-enable report window in
    # microseconds, its most extra cycles, the cycles that still showed word 7's report and word 15's
    # acknowledgement time; TRIAL_VELOCITY_SAMPLE which=2 and 3 are the first and last such cycle, which=4
    # the cycle that reported the enable. Earlier records have neither.
    Row("TRIAL_VELOCITY_WINDOW", "TRIAL_VELOCITY_WINDOW ", "summary",
        "window:u spacing:u cycles:u pending:u ack:u"),
    # velocity-target (protocol 8, ledger gate Q03): the line's value and its four bytes as written, the 60FF
    # download's terminal (outcome 255 while none settled), the upload's raw bytes after an acknowledged
    # download and whether they are the written ones, and the read after it (0 none, 1 the check's
    # standstill, 2 not). Raw: what the bytes mean for the drive's signedness is the reviewer's.
    Row("TRIAL_VELOCITY_TARGET", "TRIAL_VELOCITY_TARGET ", "summary",
        "value:i tx:h written:b outcome:u ack:b uncertain:b abort:x read:b size:u data:h match:b after:u"),
    # subdivision (protocol 9, prompt 59): after TRIAL_SETTINGS, 2200 and 2201 raw (2201 decimal) of the prior
    # read (which=0), the readback uploads as delivered (1) and the DRIVER read after the transaction (2),
    # then the 2201 download's own terminal (outcome 255 while none settled). Raw: the reviewer judges them.
    Row("TRIAL_DRIVER", "TRIAL_DRIVER ", "summary", "which:u present:b 2200:x 2201:u"),
    Row("TRIAL_SUBDIVISION", "TRIAL_SUBDIVISION ", "summary",
        "requested:u written:b outcome:u ack:b uncertain:b abort:x"),
    # controlword (protocol 11): the line's word and its exact request, the one 6040 download's terminal (outcome
    # 255 while none settled), the completed cycles and the read after the word that reported 6041 bit 1 or 2 or
    # 606C nonzero and ended the route (marked: 1..cycles a cycle, cycles + 1 the final read, 0 none; alert repeats
    # it). Then each cycle raw, with the read's completion time and its own mark bits. Never judged as a transition.
    Row("TRIAL_CONTROLWORD", "TRIAL_CONTROLWORD ", "summary",
        "word:x tx:h written:b outcome:u ack:b uncertain:b abort:x cycles:u marked:u enabled:b moving:b alert:b"),
    Row("TRIAL_CONTROLWORD_CYCLE", "TRIAL_CONTROLWORD_CYCLE ", "summary",
        "which:u us:u blocks:x 2100:x 6041:x 6061:x 6064:i 606c:i enabled:b moving:b"),
    Row("END", "END trial ", "end", "terminals:u traffic:u tail:u tailLost:b lost:b"),
    Row("RELEASED", "TRIAL record released", "response"),
    Row("RELEASE_REFUSED", "TRIAL release refused", "response"),
    Row("ADMITTED", "MOTION_READINESS admitted", "start"),
    Row("ADMITTED", "IDENTITY admitted", "start"),
    Row("ADMITTED", "HEARTBEAT experiment admitted", "start"),
    Row("ADMITTED", "PDO admitted", "start"),
    Row("ADMITTED", "NMT Pre-operational admitted", "start"),
    Row("REFUSED", "MOTION_READINESS refused", "refused"),
    Row("REFUSED", "IDENTITY refused", "refused"),
    Row("REFUSED", "HEARTBEAT refused", "refused"),
    Row("REFUSED", "PDO refused", "refused"),
    Row("REFUSED", "NMT refused", "refused"),
    Row("REFUSED", "SOAK_REFUSED ", "refused", "reason:s"),
    Row("SOAK_STOPPING", "SOAK_STOPPING", "response"),
    Row("SOAK_STOP_REFUSED", "SOAK_STOP refused", "response"),
    Row("SOAK_BEGIN", "SOAK_BEGIN ", "soak",
        "n:u t:u trial:u dur:u interval:u summary:u monitors:b identity:b opDeadline:u queue:u resp:u "
        "stack:u heap:u heapMin:u stale:u? base:s", "t"),
    Row("SOAK_TR", "SOAK_TR ", "soak", "n:u t:u node:u sig:s from:x to:x first:b ne:u ce:u", "t",
        _soak_transition),
    Row("SOAK_ANOM", "SOAK_ANOM ", "soak", "n:u t:u id:u kind:s node:u dump:b a:x b:x", "t"),
    Row("SOAK_OP", "SOAK_OP ", "soak",
        "n:u t:u node:u op:u local:b step:u? object:o? outcome:u? reason:u? stages:x? abort:x? "
        "kind:u? cursor:u? read:u? detail:u?", "t"),
    Row("SOAK_EMCY", "SOAK_EMCY ", "soak", "n:u t:u node:u dlc:u data:h disposition:u", "t", _soak_emcy),
    Row("SOAK_BOOT", "SOAK_BOOT ", "soak", "n:u t:u node:u dlc:u data:h disposition:u", "t", _soak_boot),
    Row("SOAK_NODE_STOP", "SOAK_NODE_STOP ", "soak", "n:u t:u node:u reason:s lastOp:u", "t", _soak_stop),
    Row("SOAK_T", "SOAK_T ", "soak",
        "n:u a:u i:u k:u p:u id:x ext:b rtr:b dlc:u data:h ok:b ex:b bounds:b dlo:u dhi:u ce:u st:u tr:u tf:u",
        "p"),
    Row("SOAK_TEND", "SOAK_TEND ", "soak", "n:u t:u a:u abandoned:b held:u unprinted:u", "t"),
    Row("SOAK_SUM", "SOAK_SUM ", "soak",
        "n:u t:u trial:u up:u rx:u tx:u txRej:u rxDrop:u rxLost:u unmatched:u qRej:u resFull:u timeouts:u "
        "cancelled:u busOff:u recov:u twaiErr:u twaiFlags:x twaiDrop:u", "t"),
    Row("SOAK_GAP", "SOAK_GAP ", "soak",
        "n:u t:u gapMax:u gapWin:u ownerGap:u qHw:u actHw:u retHw:u trHw:u trOver:u netDrop:u stack:u "
        "heap:u heapMin:u tec:u? rec:u?", "t"),
    Row("SOAK_CON", "SOAK_CON ", "soak",
        "n:u t:u lines:u blocked:u conRej:u conHw:u evFull:u evLimit:u evHw:u anomalies:u dumps:u dumpSkip:u "
        "dumpAb:u trLost:u dumpUs:u defDump:u defBusy:u defTx:u defInh:u slips:u sdkDefer:u uartBytes:u? "
        "queued:u?", "t"),
    Row("SOAK_NODE", "SOAK_NODE ", "soak",
        "n:u t:u node:u ops:u opsOk:u opsFail:u refused:u steps:u stepsOk:u min:u mean:u max:u retireMax:u "
        "earlyReply:u hb:u hbGapMax:u stopped:b", "t"),
    Row("SOAK_HIST", "SOAK_HIST ", "soak", "n:u t:u node:u b:s", "t"),
    Row("SOAK_OUT", "SOAK_OUT ", "soak", "n:u t:u node:u sdo:s reason:s read:s", "t"),
    Row("SOAK_ABORT", "SOAK_ABORT ", "soak", "n:u t:u node:u a0:s a1:s a2:s a3:s other:u", "t"),
    Row("SOAK_END", "SOAK_END ", "soak", "n:u t:u trial:u reason:s up:u lines:u settled:b", "t"),
    Row("RAW", "RAW ", "header",
        "id:u node:u failure:u lost:b terminals:u traffic:u tail:u tailLost:b gap:u epoch:u?",
        signals=_RECORD_HEALTH, per_node=True),
    Row("EVIDENCE", "EVIDENCE ", "header",
        "id:u node:u outcome:u uncertain:b lost:b terminals:u traffic:u gap:u",
        signals=(("health", "outcome"), ("health", "uncertain"), ("health", "lost")), per_node=True),
    # The TPDO1 configuration record of m and E. From the image with E (PONG pdo=2): the node, the
    # transition (1 map3-to-map2, 2 event timer), raw: the requested 1800:05 literal, and the core's
    # confirmed, first-effect and failed step numbers (step 1 the first write, 0 none).
    Row("PDO", "PDO failure", "header",
        "failure:u lost:b terminals:u traffic:u tail:u tailLost:b confirmed:b effects:b uncertain:b "
        "ackMask:x cfg:u migrated:b gap:u node:u? transition:u? raw:u? lastStep:u? effectStep:u? "
        "failedStep:u?",
        signals=_RECORD_HEALTH + (("health", "uncertain"), ("status", "confirmed"), ("status", "cfg"))),
    Row("RESULT", "RESULT ", "terminal", _RESULT, "completed", _result_signals),
    Row("TRAFFIC", "TRAFFIC ", "traffic", _FRAME, "p", _frame_signals),
    Row("TRAFFIC", "TRAFFIC ", "traffic", _FRAME_OLD, "p", _frame_signals),
    Row("TAIL", "TAIL ", "tail", _FRAME, "p", _frame_signals),
    Row("TAIL", "TAIL ", "tail", _FRAME_OLD, "p", _frame_signals),
    Row("END", "END raw terminals", "end", "terminals:u traffic:u tail:u tailLost:b lost:b"),
    Row("END", "END raw evidence retained", "end"),
    Row("END", "END evidence retained", "end"),
    Row("END", "END PDO", "end"),
    Row("END_NETWORK", "END network", "response"),
    Row("PONG", "PONG ", "response",
        "target:s base:s elf:s sdk:s running:b active:b silent:b pinsReady:b soak:b? t:u? routes:u? trial:b? "
        "motion:u? pdo:u? watch:b? capture:b?", "t", _PONG + (("controller", "trial"), ("health", "motion"))),
    Row("STATS", "STATS ", "response",
        "running:b inhibited:b rx:u tx:u dropped:u errors:u flags:x stackFree:u t:u? txRej:u? rxLost:u? "
        "unmatched:u? traceOver:u? ownerGap:u? conRej:u?", "t", _STATS),
    Row("CAN_READY", "CAN ready ", "response", "bitrate:u", signals=(("controller", "bitrate"),)),
    Row("CAN_REFUSED", "CAN start refused", "response"),
    Row("NETWORK", "NETWORK node=", "response",
        "node:u epoch:u state:u qualified:b producerKnown:b ms:u pending:b observed:b expired:b "
        "exact:b bounds:b lo:u hi:u p:u dropped:u", "p", _NETWORK, True),
    Row("NETWORK_UNCONFIGURED", "NETWORK node=", "response", "node:u"),
    Row("EVENT", "EVENT ", "response",
        "id:x dlc:u data:h disposition:u decoded:b kind:u p:u exact:b bounds:b lo:u hi:u", "p",
        _event_signals),
    Row("NMT", "NMT request", "response",
        "request:u target:u cmd:u outcome:u possible:b txConfirmed:b completed:u admitted:u?", "completed",
        (("nmt", "cmd"), ("nmt", "outcome"), ("nmt", "txConfirmed"))),
    Row("RELEASED", "RELEASED record", "response"),
    Row("RELEASED", "PDO record released", "response"),
    Row("RELEASED", "NMT result released", "response"),
    Row("RELEASE_REFUSED", "RELEASE refused", "response"),
    Row("RELEASE_REFUSED", "PDO release refused", "response"),
    Row("RELEASE_REFUSED", "NMT release refused", "response"),
    Row("DUMP_UNAVAILABLE", "DUMP unavailable", "response"),
    # PDO stream sessions (examples/s2_bench/PdoCommand.h, PdoWatch.h, scripts/s2_pdo.py): N, P, R,
    # W and E lines and the watch's numbered lines. A watched frame is a copy, never a PDO sample.
    Row("ADMITTED", "NMT admitted", "start", "node:u cmd:u"),
    Row("ADMITTED", "PDO_READ admitted", "start", "node:u dir:s set:u"),
    Row("ADMITTED", "PDO_WRITE admitted", "start", "node:u set:u sub:u baseline:u value:u"),
    # E lines: the supported TPDO1 event-timer change; its record is the PDO header above.
    Row("ADMITTED", "PDO_CONFIG admitted", "start", "node:u raw:u"),
    Row("REFUSED", "PDO_READ refused", "refused"),
    Row("REFUSED", "PDO_WRITE refused", "refused"),
    Row("REFUSED", "PDO_CONFIG refused", "refused"),
    Row("REFUSED", "PDO_LINE refused", "refused", "reason:s"),
    Row("PDOW_ADMITTED", "PDOW admitted", "response", "seconds:u"),
    Row("REFUSED", "PDOW refused", "refused", "reason:s"),
    Row("PDOW_STOPPING", "PDOW_STOPPING", "response"),
    Row("PDOW_STOP_REFUSED", "PDOW_STOP refused", "response"),
    Row("PDOW_BEGIN", "PDOW_BEGIN ", "watch", "n:u t:u seconds:u ring:u ids:u", "t"),
    Row("PDOW_F", "PDOW_F ", "watch",
        "n:u t:u id:x ext:b rtr:b dlc:u data:s timed:b exact:b lo:u hi:u gap:u", "t"),
    Row("PDOW_SUM", "PDOW_SUM ", "watch",
        "n:u t:u frames:u printed:u unprinted:u tapDropped:u untimed:u sdo:u idOverflow:u ids:u", "t"),
    Row("PDOW_ID", "PDOW_ID ", "watch",
        "n:u t:u id:x ext:b count:u first:u last:u minGap:u maxGap:u meanGap:u sdGap:u changes:u dlc:u "
        "data:s", "t"),
    Row("PDOW_END", "PDOW_END ", "watch",
        "n:u t:u reason:s frames:u printed:u unprinted:u tapDropped:u untimed:u sdo:u idOverflow:u", "t"),
    # The bounded silent capture (examples/s2_bench/SilentCapture.h, the C line, PONG pdo=3). Its
    # record is printed after the end, one line per turn, and held until r. A CAP_F line is a receive
    # copy; the record shows what the software did, never that the bus saw no ACK or TX.
    Row("ADMITTED", "CAP admitted", "start", "cap:u seconds:u frames:u gpio:u silentHigh:b"),
    Row("REFUSED", "CAP refused", "refused", "reason:s"),
    Row("CAP_STOPPING", "CAP_STOPPING", "response"),
    Row("CAP_STOP_REFUSED", "CAP_STOP refused", "response"),
    Row("RELEASED", "CAP record released", "response"),
    Row("RELEASE_REFUSED", "CAP release refused", "response"),
    Row("CAP_REC", "CAP_REC ", "capture",
        "cap:u reason:s seconds:u capacity:u stored:u overflow:u tapDropped:u pre:u post:u untimed:u "
        "txRefused:u lost:b"),
    Row("CAP_TIME", "CAP_TIME ", "capture", "cap:u begin:u end:u nominal:u gap:u clock:s", "end"),
    Row("CAP_LEVEL", "CAP_LEVEL ", "capture",
        "cap:u gpio:u silentHigh:b begin:b exit:u reopened:b at:u latched:b"),
    Row("CAP_CTRL", "CAP_CTRL ", "capture",
        "cap:u which:u t:u epoch:u counters:b tec:u rec:u inhibited:b adapterInhibited:b twaiDropped:u "
        "twaiErrors:u twaiFlags:x", "t"),
    Row("CAP_OWNER", "CAP_OWNER ", "capture",
        "cap:u which:u rx:u dropped:u lost:u unmatched:u tx:u txRej:u busOff:u refused:u"),
    Row("CAP_IMAGE", "CAP_IMAGE ", "capture", "cap:u base:s elf:s"),
    Row("CAP_F", "CAP_F ", "capture",
        "cap:u i:u id:x ext:b rtr:b dlc:u data:s bounds:b exact:b lo:u hi:u epoch:u pre:b post:b"),
    Row("CAP_END", "END capture", "capture", "cap:u stored:u overflow:u lost:b"),
    # The D, K and H records (PONG pdo=4; examples/s2_bench/DiscoveryBench.h, CommunicationBench.h).
    # Each is held until r and printed after its terminal; its RESULT, TRAFFIC and TAIL lines use the
    # rows above. Typed values are raw observations: no model, route, map, unit or active setting.
    # D: the library's DiscoveryScan over both configured nodes, then host selection (sends nothing).
    Row("ADMITTED", "DISC admitted", "start", "mode:u seconds:u select:u expect:b"),
    Row("REFUSED", "DISC refused", "refused", "reason:s"),
    Row("DISC", "DISC id=", "header",
        "id:u mode:u seconds:u targets:u attempted:u completed:u admitted:u terminals:u occupied:u free:u "
        "unresolved:u drops:u cancelled:b deadline:b changed:b failed:b lost:b gap:u",
        signals=(("health", "failed"), ("health", "unresolved"))),
    # outcome: DiscoveryOutcome (1 PRESENCE, 2 GENERIC_IDENTITY, 3 NO_RESPONSE, 6 CONTRADICTORY_IDENTITY);
    # confidence: DiscoveryConfidence (1 responder only, 2 raw identity, 3 caller expectation matched).
    Row("DISC_T", "DISC_T ", "summary",
        "node:u slot:u outcome:u confidence:u read:u state:u fields:u uncertain:b hb:b hbp:u boot:b bootp:u "
        "pdo:b pdop:u dup:b unresolved:b current:b completed:u",
        signals=(("health", "outcome"), ("health", "confidence")), per_node=True),
    Row("DISC_PROBE", "DISC_PROBE ", "summary", "node:u type:x"),
    Row("DISC_ID", "DISC_ID ", "summary",
        "node:u type:x hw:x hww:u sw:x sww:u entries:u vendor:x product:x revision:x model:x firmware:x"),
    Row("DISC_SELECT", "DISC_SELECT ", "summary", "node:u attempted:b selected:b current:b tx0:u tx1:u"),
    Row("END", "END discovery", "end", "terminals:u targets:u"),
    Row("RELEASED", "DISC record released", "response"),
    Row("RELEASE_REFUSED", "DISC release refused", "response"),
    # K: the eighteen typed communication read groups of one node, 95 uploads.
    Row("ADMITTED", "COMM admitted", "start", "node:u groups:u uploads:u"),
    Row("REFUSED", "COMM refused", "refused", "reason:s"),
    Row("COMM", "COMM id=", "header",
        "id:u node:u failure:u groups:u published:u group:u outcome:u uncertain:b status:u detail:i "
        "terminals:u grants:u admission:u gap:u",
        signals=(("health", "failure"), ("health", "uncertain")), per_node=True),
    Row("COMM_NETWORK", "COMM_NETWORK ", "summary",
        "node:u sync:x emcy:x producer:u sdo:u cs:x sc:x lo:u hi:u skew:u",
        signals=(("status", "producer"),), per_node=True),
    Row("COMM_NODE", "COMM_NODE ", "summary", "node:u raw:u lo:u hi:u skew:u"),
    Row("COMM_PDO", "COMM_PDO ", "summary",
        "node:u dir:s pdo:u entries:u cob:x type:u inhibit:u compat:u event:u lo:u hi:u skew:u"),
    Row("COMM_MAP", "COMM_MAP ", "summary",
        "node:u dir:s pdo:u count:u w1:x w2:x w3:x w4:x lo:u hi:u skew:u"),
    Row("END", "END comm", "end", "terminals:u groups:u"),
    Row("RELEASED", "COMM record released", "response"),
    Row("RELEASE_REFUSED", "COMM release refused", "response"),
    # H: the reviewed 1017:00 raw 0 to raw 1000 commissioning. state: CommunicationState (4 SUCCEEDED,
    # 5 FAILED); write and outcome: ReadOutcome; restart and persistence stay 0, unknown.
    Row("ADMITTED", "HB admitted", "start", "node:u raw:u"),
    Row("REFUSED", "HB refused", "refused", "reason:s"),
    Row("HB", "HB id=", "header",
        "id:u node:u failure:u lost:b state:u outcome:u write:u ack:b effects:b uncertain:b known:b "
        "readback:u matched:b published:b raw:u terminals:u gap:u",
        signals=(("health", "failure"), ("health", "uncertain"), ("status", "readback")), per_node=True),
    # The NMT observation given to the profile (nmtState NmtState when nmt=1), the epoch migration, the
    # review scope (1 ISOLATED_EXPERIMENT) and the last refused profile call (Err code, ReadError detail).
    Row("HB_PREP", "HB_PREP ", "summary",
        "node:u nmt:b nmtState:u prepared:b migrated:b cfg:u scope:u restart:u persistence:u status:u "
        "detail:i"),
    Row("END", "END heartbeat", "end", "terminals:u traffic:u tail:u tailLost:b lost:b"),
    Row("RELEASED", "HB record released", "response"),
    Row("RELEASE_REFUSED", "HB release refused", "response"),
    # M (pdo=4): internal heap totals, each region as start:size:free:largest-free-block in the SDK's
    # allocation order, the boot reservation mask (bits: route, session 1, session 2, arena, work) and
    # the shared arena's occupant (0 none, 1 raw, 2 PDO, 3 D, 4 K, 5 H). A heap reading, not a budget.
    Row("HEAP", "HEAP ", "response",
        "free:u largest:u minimum:u regions:u r0:s? r1:s? r2:s? r3:s? reserved:x arena:u arenaBytes:u",
        signals=(("health", "free"), ("health", "largest"))),
    # x (backlog S15): the idle route sessions closed, or a refusal with nothing changed (reason record,
    # state, busy, configuration, owner or none). Nothing is sent either way. kept1/kept2 are BenchRoute
    # SessionKept bits, the facts that still refuse later work: 1 move fence, 2 clear block, 4 fault
    # block, 8 velocity needs a stop, 16 home needs a stop, 32 motion written, 64 a 6060 write admitted.
    Row("SESSIONS_CLOSED", "SESSIONS closed", "response", "node1:b node2:b kept1:x kept2:x motion:u"),
    Row("SESSIONS_REFUSED", "SESSIONS refused", "response", "reason:s node1:b node2:b"),
    # n while a discovery scan runs: its passive evidence is the history the listing would drain.
    Row("REFUSED", "NETWORK refused", "refused", "reason:s"),
    Row("UNKNOWN_COMMAND", "UNKNOWN command", "response"),
    Row("FORMAT_ERROR", "OUTPUT_FORMAT_ERROR", "error"),
    Row("BOOT_READY", "S2_BENCH ready", "boot", signals=_boot_signals),
    Row("BOOT_ROM", "ESP-ROM:", "boot", signals=_boot_signals),
    Row("BOOT_ROM", "rst:0x", "boot", signals=_boot_signals),
    Row("BOOT_ROM", "Build:", "boot"),
    Row("BOOT_ROM", "Saved PC:", "boot"),
    Row("BOOT_ROM", "SPIWP:", "boot"),
    Row("BOOT_ROM", "mode:", "boot"),
    Row("BOOT_ROM", "load:0x", "boot"),
    Row("BOOT_ROM", "entry 0x", "boot"),
)


class Expect(NamedTuple):
    """What one s2_bench.py command may legitimately produce."""
    family: str = ""  # Header kind of an SDO record, if the command creates one.
    start: str = ""  # Required admission line prefix.
    node: int = 0
    inventory: str = ""
    responses: tuple = ()  # Allowed response kinds; any of them satisfies the request.


_RELEASES = ("RELEASED", "RELEASE_REFUSED")
EXPECTED = {
    "ping": Expect(responses=("PONG",)),
    "start": Expect(responses=("CAN_READY", "CAN_REFUSED")),
    "stats": Expect(responses=("STATS",)),
    "release": Expect(responses=_RELEASES),
    "network": Expect(responses=("NETWORK", "NETWORK_UNCONFIGURED", "EVENT", "END_NETWORK")),
    "dump": Expect(family="any", responses=("DUMP_UNAVAILABLE",)),
    "identity1": Expect("EVIDENCE", "IDENTITY admitted", 1, "identity"),
    "identity2": Expect("EVIDENCE", "IDENTITY admitted", 2, "identity"),
    "raw1": Expect("RAW", "IDENTITY admitted", 1, "identity_network"),
    "raw2": Expect("RAW", "IDENTITY admitted", 2, "identity_network"),
    "motion-readiness1": Expect("RAW", "MOTION_READINESS admitted", 1, "motion_readiness"),
    "motion-readiness2": Expect("RAW", "MOTION_READINESS admitted", 2, "motion_readiness"),
    "heartbeat1": Expect("RAW", "HEARTBEAT experiment admitted", 1, "heartbeat"),
    "heartbeat2": Expect("RAW", "HEARTBEAT experiment admitted", 2, "heartbeat"),
    "tpdo1": Expect("PDO", "PDO admitted", 1),
    "soak-stop": Expect(responses=("SOAK_STOPPING", "SOAK_STOP_REFUSED")),
    "preop1": Expect("", "NMT Pre-operational admitted", 1, responses=("NMT",)),
    "preop2": Expect("", "NMT Pre-operational admitted", 2, responses=("NMT",)),
    "trial-abort": Expect(responses=("TRIAL_ABORTING", "TRIAL_ABORT_REFUSED")),
    "close-sessions": Expect(responses=("SESSIONS_CLOSED", "SESSIONS_REFUSED")),
}
# Trial route commands: s2_bench.py sends a ping, then one T line; the node comes from request.json.
# Ctrl-C during the route sends one local abort, so its response may appear too.
_TRIAL_RESPONSES = ("PONG", "TRIAL_ABORTING", "TRIAL_ABORT_REFUSED")
TRIAL_ROUTES = {"readiness-actions": "actions", "readiness-io": "io", "readiness-tuning": "tuning",
                "mode1": "mode1", "enable": "enable", "stop": "stop", "position": "position",
                "rearm": "rearm", "move": "move", "readiness-history": "history",
                "released-quick": "released-quick", "word67": "word67", "relative": "relative",
                "settings-2202": "settings-2202", "io-di3": "io-di3", "counter-zero": "counter-zero",
                "clear-alarm": "clear-alarm", "mode3": "mode3", "velocity": "velocity", "home": "home",
                "velocity-target": "velocity-target", "subdivision": "subdivision", "controlword": "controlword"}
for _command, _route in TRIAL_ROUTES.items():
    EXPECTED[_command] = Expect("TRIAL", "TRIAL admitted", 0, "trial_" + _route, _TRIAL_RESPONSES)

# Fixed firmware inventories: (index, subindex, EDS width in bytes). Tests compare
# them with examples/s2_bench/RawEvidence.cpp and the retained EDS. A disagreement
# between a width here and actual bytes is an observation, never a correction.
_IDENTITY = ((0x1000, 0, 4), (0x1009, 0, 2), (0x100A, 0, 2), (0x1018, 0, 1), (0x1018, 1, 4),
             (0x1018, 2, 4), (0x1018, 3, 4), (0x2000, 0, 2), (0x2001, 0, 2))
_NETWORK_FIELDS = _IDENTITY + (
    (0x1800, 1, 4), (0x1800, 2, 1), (0x1800, 3, 2), (0x1800, 5, 2), (0x1A00, 0, 1), (0x1A00, 1, 4),
    (0x1A00, 2, 4), (0x1A00, 3, 4), (0x1A00, 4, 4), (0x6041, 0, 2), (0x6061, 0, 1), (0x1017, 0, 2),
    (0x2100, 0, 2), (0x603F, 0, 2), (0x6064, 0, 4), (0x2201, 0, 2), (0x606C, 0, 4))
_MOTION = (
    (0x1001, 0, 1), (0x603F, 0, 2), (0x6041, 0, 2), (0x6061, 0, 1), (0x2100, 0, 2), (0x2101, 0, 2),
    (0x6064, 0, 4), (0x606C, 0, 4), (0x607A, 0, 4), (0x6081, 0, 4), (0x6083, 0, 4), (0x6084, 0, 4),
    (0x605D, 0, 2), (0x60FF, 0, 4), (0x2200, 0, 2), (0x2201, 0, 2), (0x2202, 0, 2), (0x607D, 1, 4),
    (0x607D, 2, 4), (0x2500, 0, 2), (0x2501, 0, 2), (0x2400, 1, 2), (0x2400, 2, 2), (0x2400, 3, 2),
    (0x2400, 4, 2), (0x2400, 5, 2), (0x2400, 6, 2), (0x2400, 7, 2), (0x2400, 8, 2), (0x60FD, 0, 4))
# Fixed trial read routes, from examples/s2_bench/BenchRoute.cpp and the typed read plans.
_IMAGE = ((0x2100, 0, 2), (0x2101, 0, 2), (0x2102, 0, 2), (0x6041, 0, 2), (0x6061, 0, 1), (0x6064, 0, 4),
          (0x606C, 0, 4))
_FULL = ((0x1001, 0, 1), (0x603F, 0, 2)) + _IMAGE
_TRIAL_ACTIONS = _FULL + ((0x6081, 0, 4), (0x6083, 0, 4), (0x6084, 0, 4), (0x1017, 0, 2), (0x607A, 0, 4),
                          (0x60FF, 0, 4)) + _IMAGE
_TRIAL_IO = (_IMAGE + tuple((0x2400, sub, 2) for sub in range(9)) +
             tuple((0x2400, sub, 2) for sub in range(0x0C, 0x10)) +
             tuple((index, 0, 2) for index in range(0x2401, 0x2408)) +
             ((0x60FE, 0, 1), (0x60FE, 1, 4), (0x60FE, 2, 4)) + _IMAGE)
_TRIAL_TUNING = _IMAGE + tuple((index, 0, 2) for index in range(0x2502, 0x2521)) + _IMAGE
_ERRORS = ((0x1001, 0, 1), (0x603F, 0, 2))
_HISTORY_ENTRIES = tuple((0x1003, subindex, 4) for subindex in range(1, 5))
# velocity-target: two full reads, the 60FF download and, only after its acknowledgement, its upload and one
# more full read. 60FF is four bytes under both of its claims (EDS I32, EN86/ZH86 p13 U32).
_TRIAL_VELOCITY_TARGET = _FULL + _FULL + ((0x60FF, 0, 4), (0x60FF, 0, 4)) + _FULL
# controlword (protocol 11): the full read, the one 6040 download, at most ten image cycles, one more full read.
CONTROLWORD_CYCLES = 10


def _trial_history(results):
    """The history route's plan: the full read, 1003:00, only the entries its own count names, then
    1001 and 603F again. The count is the record's own successful 1003:00 terminal at that step; one
    the record never read, or above four (the typed read fails then), names no entry."""
    count = 0
    if len(results) > len(_FULL):
        values = results[len(_FULL)].values
        if tuple(values["object"]) == (0x1003, 0) and values["outcome"] == 0 and values["size"] == 1:
            count = bytes.fromhex(values["data"])[0]
    rows = _FULL + ((0x1003, 0, 1),) + (_HISTORY_ENTRIES[:count] if count <= 4 else ()) + _ERRORS
    return rows, (len(rows),)


def _trial_controlword(results):
    """The controlword route's plan: the full read, the 6040 download, then as many image cycles as the record's
    own terminals begin (a 2100 upload where the next cycle would start, at most ten), then the full read. A
    successful record holds at least one cycle and the final read; a marked or failed one may end sooner."""
    cycles = 0
    start = len(_FULL) + 1
    while cycles < CONTROLWORD_CYCLES and len(results) > start + len(_IMAGE) * cycles and             tuple(results[start + len(_IMAGE) * cycles].values["object"]) == (0x2100, 0):
        cycles += 1
    rows = _FULL + ((0x6040, 0, 2),) + _IMAGE * cycles + _FULL
    return rows, ((len(rows),) if cycles else ())


# name: (rows, plan sizes a successful record may have), or a function of the record's terminals
# returning them, for a plan whose length its own replies decide.
INVENTORIES = {
    "identity": (_IDENTITY, (9,)),
    "identity_network": (_NETWORK_FIELDS, (18, 20, 26)),
    "motion_readiness": (_MOTION, (30,)),
    "heartbeat": (((0x1017, 0, 2),) * 3, (3,)),
    "trial_actions": (_TRIAL_ACTIONS, (22,)),
    "trial_io": (_TRIAL_IO, (37,)),
    "trial_tuning": (_TRIAL_TUNING, (45,)),
    "trial_history": _trial_history,
    "trial_velocity-target": (_TRIAL_VELOCITY_TARGET, (29,)),
    "trial_controlword": _trial_controlword,
}


# ---------------------------------------------------------------- line parsing
def _uint(text):
    if not text.isdigit():
        raise ValueError(text)
    return int(text)


def _hex(text):
    return int(text, 16)


def _flag(text):
    if text not in ("0", "1"):
        raise ValueError(text)
    return int(text)


def _int(text):
    if not (text[1:] if text.startswith("-") else text).isdigit():
        raise ValueError(text)
    return int(text)


def _bytes(text):
    bytes.fromhex(text)
    if len(text) % 2 or not text:
        raise ValueError(text)
    return text.lower()


def _object(text):
    index, subindex = text.split(":")
    if len(index) != 4 or len(subindex) != 2:
        raise ValueError(text)
    return [int(index, 16), int(subindex, 16)]


TYPES = {"u": _uint, "x": _hex, "b": _flag, "h": _bytes, "o": _object, "s": str, "i": _int}


@lru_cache(maxsize=None)
def _compile(spec):
    """Return {wire name: (canonical name, converter, required)}."""
    out = {}
    for token in spec.split():
        optional = token.endswith("?")
        name, _, kind = token.rstrip("?").partition(":")
        kind, _, canonical = kind.partition(">")
        out[name] = (canonical or name, TYPES[kind], not optional)
    return out



@dataclass
class Record:
    """One preserved line. status is parsed, malformed or unknown; raw always survives."""
    number: int
    raw: str
    kind: str = "UNKNOWN"
    role: str = ""
    status: str = "unknown"
    reason: str = ""
    row: object = None
    values: dict = field(default_factory=dict)
    extra: dict = field(default_factory=dict)


def _fit(row, text):
    """Fit one row, returning (values, extra, reason). A fixed row ignores its free text."""
    if not row.fields:
        return {}, {}, ""
    specification = _compile(row.fields)
    # Prose after the first ';' is not a field ("...lost=0; r=release").
    words = text.partition(";")[0].split()
    pairs = {}
    for word in words:
        key, separator, value = word.partition("=")
        if not separator:
            continue
        if key in pairs:
            return None, None, "duplicate field " + key
        pairs[key] = value
    values, extra = {}, {}
    for key, value in pairs.items():
        if key not in specification:
            extra[key] = value
            continue
        canonical, convert, _ = specification[key]
        try:
            values[canonical] = convert(value)
        except ValueError:
            return None, None, "bad value %s=%s" % (key, value)
    missing = [key for key, (_, _, required) in specification.items() if required and key not in pairs]
    if missing:
        return None, None, "missing " + ",".join(missing)
    return values, extra, ""


def parse_line(number, text):
    """Classify one line through GRAMMAR; nothing here can drop the raw text."""
    record = Record(number, text)
    if "�" in text or any(ord(c) < 32 and c != "\t" for c in text):
        record.status, record.reason = "malformed", "undecodable or control byte"
        record.kind = "CORRUPT"
        return record
    reason = ""
    matched = None
    for row in GRAMMAR:
        if not text.startswith(row.head):
            continue
        matched = row
        values, extra, reason = _fit(row, text)
        if values is None:
            continue
        record.kind, record.role, record.row = row.kind, row.role, row
        record.status, record.values, record.extra = "parsed", values, extra
        return record
    if matched:
        record.kind, record.role = matched.kind, matched.role
        record.status, record.reason = "malformed", reason
    return record


def _boot_after_reset_noise(number, text):
    """A capture's first line may carry a stray byte or two that the S2's UART emits at reset
    (2026-11-03: one 0xB8 before the boot banner after a reflash). Only then, and only when the
    rest parses as a boot line, is it accepted; the raw line is kept and the noise noted."""
    rest = text.lstrip("�")
    if not 1 <= len(text) - len(rest) <= 2 or not rest.startswith(("S2_BENCH ready", "rst:0x")):
        return None
    record = parse_line(number, rest)
    if record.status != "parsed" or record.role != "boot":
        return None
    record.raw = text
    record.reason = "%d undecodable byte(s) of reset noise before the boot line" % (len(text) - len(rest))
    return record


def parse_bytes(data):
    """Split on LF, tolerating CRLF. Returns (records, unterminated final text or None)."""
    lines = data.split(b"\n")
    tail = lines.pop()
    records = []
    for number, line in enumerate(lines, 1):
        text = line.rstrip(b"\r").decode("utf-8", errors="replace")
        if text:
            record = parse_line(number, text)
            if number == 1 and record.status == "malformed":
                record = _boot_after_reset_noise(number, text) or record
            records.append(record)
    unterminated = None
    if tail:
        unterminated = tail.decode("utf-8", errors="replace")
        # A partial line can look complete; keep it visible and never believe it.
        record = parse_line(len(lines) + 1, unterminated)
        record.status, record.reason = "malformed", "no line terminator"
        records.append(record)
    return records, unterminated


# ---------------------------------------------------------------- SDO correlation
def sdo_mux(payload):
    return [payload[1] | payload[2] << 8, payload[3]]


def sdo_reply(payload):
    """Return (form, indicated width). Width is None when the reply does not state one."""
    command = payload[0]
    if command == 0x80:
        return "abort", None
    if command == 0x60:
        return "download_ack", 0
    if command & 0xE0 == 0x40:
        if command & 2 and command & 1:
            return "upload", 4 - (command >> 2 & 3)
        if command & 2:
            return "upload_unsized", 4
        return "upload_segmented", None
    return "other", None


# ReadOutcome order, which an EVIDENCE header reports. SDO terminals use OUTCOMES.
READ_OUTCOMES = ("NONE", "SUCCESS", "REPLY_ERROR", "PROTOCOL_REJECTED", "TRANSPORT_ERROR", "CANCELLED",
                 "DEADLINE", "INVALIDATED", "SERVICE_TIMEOUT")

# Incomplete: output is missing or damaged. Positional: wire correlation that a lost
# line also shifts, so it cannot accuse the record once output is known damaged.
# Everything else (identity, duplicates, contradictions) is always inconsistent.
INCOMPLETE = {"capture_failed", "empty_capture", "malformed_line", "truncated_line",
              "format_error", "missing_header", "missing_end", "count_short", "missing_start",
              "missing_response", "reboot_in_capture", "soak_no_begin", "soak_no_end", "soak_line_gap",
              "soak_dump_missing"}
POSITIONAL = {"step_sequence", "wrong_object_for_step", "wrong_request_object", "stale_reply",
              "success_without_request", "success_without_reply", "unmatched_request",
              "unmatched_tx_report", "tx_report_not_copied", "reply_form_mismatch",
              "terminal_width_differs_from_wire", "terminal_bytes_differ_from_wire",
              "abort_differs_from_wire", "traffic_time_reversed", "record_order", "terminal_overlap",
              "end_differs_from_header", "failure_differs_from_terminal", "plan_size_not_in_inventory",
              "tx_before_admission", "reply_after_terminal", "wrong_direction", "history_differs_from_terminals",
              "cleanup_differs_from_terminal", "velocity_target_differs_from_record"}
# Matching terminals to copied traffic. A record that itself reports lost=1 has an incomplete trace,
# so these findings cannot accuse it: they stay listed as warnings. Its failure stays its own.
TRACE_MATCHING = {"wrong_request_object", "stale_reply", "success_without_request", "success_without_reply",
                  "unmatched_request", "unmatched_tx_report", "tx_report_not_copied", "reply_form_mismatch",
                  "terminal_width_differs_from_wire", "terminal_bytes_differ_from_wire", "abort_differs_from_wire",
                  "tx_before_admission", "reply_after_terminal", "wrong_direction", "cleanup_differs_from_terminal"}
assert TRACE_MATCHING <= POSITIONAL


@dataclass
class Report:
    """All replay output for one capture; every list element names its source line."""
    path: str
    command: str = ""
    started_utc: str = ""
    serial: str = ""
    bytes: int = 0
    sha256: str = ""
    rx_txt: str = "absent"
    manifest: str = "absent"
    verdict: str = ""
    kinds: Counter = field(default_factory=Counter)
    unknown: list = field(default_factory=list)
    unknown_fields: list = field(default_factory=list)
    malformed: list = field(default_factory=list)
    errors: list = field(default_factory=list)
    warnings: list = field(default_factory=list)
    family: str = ""
    operation: int | None = None
    node: int | None = None
    failure: int | None = None
    failure_name: str = ""
    terminals: int = 0
    traffic: int = 0
    tail: int = 0
    started: bool = False
    ended: bool = False
    gap_us: int | None = None
    raw_uploads_successful: int = 0
    typed_successes: int = 0
    failed: list = field(default_factory=list)
    uncertain: list = field(default_factory=list)
    widths: list = field(default_factory=list)
    width_discrepancies: list = field(default_factory=list)
    typed_width_failures: list = field(default_factory=list)
    epochs: object = "not_emitted"
    trace_lost: bool = False
    tail_lost: bool = False
    inventory_checked: bool = False
    node_epochs: object = "not_emitted"
    soak: dict = field(default_factory=dict)
    records: list = field(default_factory=list, repr=False)

    def error(self, code, line=None, detail=""):
        self.errors.append({"code": code, "line": line, "detail": detail})

    def warn(self, code, line=None, detail=""):
        self.warnings.append({"code": code, "line": line, "detail": detail})

    def public(self):
        out = {key: value for key, value in vars(self).items() if key != "records"}
        out["kinds"] = dict(self.kinds)
        return out


def _tx_identity(record):
    """The local TX identity a frame or report prints (from the soak image on), or None."""
    values = record.values
    return (values["txr"], values["txf"]) if "txr" in values and "txf" in values else None


def _windows(report, frames, node):
    """Split copied frames at each client request. Leading SDO replies have no request. A client
    abort after a request is that request's cleanup frame, its own second TX: its final report is the
    cleanup's, matched by local TX identity where the record prints one, else taken as the report that
    follows the request's own."""
    windows, current = [], None
    server, client = 0x580 + node, 0x600 + node
    for record in frames:
        values = record.values
        identifier, kind = values["id"], values["k"]
        if kind == TX and 0x600 < identifier <= 0x67F:
            if identifier != client:
                report.error("wrong_channel", record.number, "request id %x" % identifier)
            if bytes.fromhex(values["data"])[0] == 0x80 and current is not None:
                current["cleanup"].append(record)  # Client abort belongs to the failed step.
                continue
            current = {"tx": record, "reply": [], "report": [], "cleanup": [], "cleanup_report": []}
            windows.append(current)
        elif kind == TX_REPORT:
            if current is None:
                report.error("unmatched_tx_report", record.number)
                continue
            identity = _tx_identity(record)
            cleanups = {_tx_identity(frame) for frame in current["cleanup"]}
            if current["cleanup"] and (identity in cleanups if identity is not None else bool(current["report"])):
                current["cleanup_report"].append(record)
            else:
                current["report"].append(record)
        elif kind == RX and 0x580 < identifier <= 0x5FF:
            if identifier != server:
                report.error("wrong_channel", record.number, "reply id %x" % identifier)
            elif current is None:
                report.error("stale_reply", record.number, "SDO reply before any request")
            else:
                current["reply"].append(record)
    return windows


def _check_window(report, result, window, documented):
    """Compare one terminal with its own wire frames, returning the actual wire width."""
    values = result.values
    index, tx = values["object"], window["tx"]
    payload = bytes.fromhex(tx.values["data"])
    if sdo_mux(payload) != index:
        report.error("wrong_request_object", tx.number, "terminal %04x:%02x" % tuple(index))
    if tx.values.get("dlc") != 8 or tx.values.get("ext") or tx.values.get("rtr"):
        report.error("bad_sdo_frame", tx.number)
    if tx.values["p"] < values["admitted"]:
        report.error("tx_before_admission", tx.number)
    expected_direction = values.get("dir")
    is_upload = payload[0] == 0x40
    if expected_direction is not None and bool(expected_direction) == is_upload:
        report.error("wrong_direction", tx.number, "dir=%d" % expected_direction)
    matching, stale = [], []
    for frame in window["reply"]:
        (matching if sdo_mux(bytes.fromhex(frame.values["data"])) == index else stale).append(frame)
    for frame in stale:
        report.error("stale_reply", frame.number, "object differs from step %d" % values["step"])
    if len(matching) > 1:
        report.error("duplicate_reply", matching[1].number)
    width = None
    reply = matching[0] if matching else None
    if reply:
        data = reply.values
        if data.get("dlc") != 8 or data.get("ext") or data.get("rtr"):
            report.error("bad_sdo_frame", reply.number)
        if data["p"] > values["completed"]:
            report.error("reply_after_terminal", reply.number)
        if data.get("bounds"):
            if data["lo"] > data["hi"] or data["hi"] > data["p"]:
                report.error("receive_bounds_invalid", reply.number)
            if data["hi"] < tx.values["p"]:
                report.error("stale_reply", reply.number, "received before this request was processed")
        form, width = sdo_reply(bytes.fromhex(data["data"]))
    if len(window["report"]) > 1:
        report.error("duplicate_tx_report", window["report"][1].number)
    if len(window["cleanup_report"]) > len(window["cleanup"]):
        report.error("duplicate_tx_report", window["cleanup_report"][len(window["cleanup"])].number)
    for frame in window["cleanup"]:
        # The cleanup abort names the step's own object and, for a client abort, the terminal's code.
        data = bytes.fromhex(frame.values["data"])
        if sdo_mux(data) != index or (values["outcome"] == 2 and
                                      int.from_bytes(data[4:8], "little") != values["abort"]):
            report.error("cleanup_differs_from_terminal", frame.number,
                         "terminal %04x:%02x abort %08x" % (index[0], index[1], values["abort"]))
    outcome = values["outcome"]
    if outcome == 0:
        _check_success(report, result, tx, reply, window, width, documented)
    else:
        _check_failure(report, result, reply, width)
    return width


def _check_success(report, result, tx, reply, window, width, documented):
    values = result.values
    if not tx.values.get("ok"):
        report.error("success_without_enqueue", tx.number)
    if reply is None:
        report.error("success_without_reply", result.number)
        return
    data = bytes.fromhex(reply.values["data"])
    form, _ = sdo_reply(data)
    upload = bytes.fromhex(tx.values["data"])[0] == 0x40
    if form not in (("upload", "upload_unsized") if upload else ("download_ack",)):
        report.error("reply_form_mismatch", reply.number, form)
    elif width != values["size"]:
        report.error("terminal_width_differs_from_wire", result.number,
                     "terminal %d wire %s" % (values["size"], width))
    elif upload:
        retained = bytes.fromhex(values["data"])
        if retained[:width] != data[4:4 + width]:
            report.error("terminal_bytes_differ_from_wire", result.number)
        if any(retained[width:]):
            report.error("bytes_beyond_actual_width", result.number)
    if window["report"]:
        if not window["report"][0].values.get("ok"):
            report.error("success_with_failed_tx_report", window["report"][0].number)
    if upload:
        if report.family == "EVIDENCE":
            report.typed_successes += 1
        else:
            report.raw_uploads_successful += 1
        row = {"line": result.number, "object": "%04x:%02x" % tuple(values["object"]),
               "actual": values["size"], "documented": documented,
               "wire": reply.values["data"],
               "evidence": "typed terminal" if report.family == "EVIDENCE" else "raw upload"}
        report.widths.append(row)
        if documented is not None and documented != values["size"]:
            report.width_discrepancies.append(row)


def _check_failure(report, result, reply, width):
    values = result.values
    if reply is None:
        return
    data = bytes.fromhex(reply.values["data"])
    form, _ = sdo_reply(data)
    if values["outcome"] == 1:
        code = int.from_bytes(data[4:8], "little")
        if form != "abort" or code != values["abort"]:
            report.error("abort_differs_from_wire", result.number,
                         "terminal %08x wire %s" % (values["abort"], reply.values["data"]))
    elif values["outcome"] == 3:
        report.warn("reply_retained_with_timeout", reply.number)
    if values["outcome"] == 2 and width is not None:
        # A typed width rejection: wire size is the observation, there is no value.
        report.typed_width_failures.append(
            {"line": result.number, "object": "%04x:%02x" % tuple(values["object"]),
             "actual_wire_width": width, "abort": "%08x" % values["abort"],
             "wire": reply.values["data"]})


def _check_terminal(report, result, position, rows, previous):
    """Coherence of one terminal, independent of its frames."""
    values, line = result.values, result.number
    outcome = values["outcome"]
    if outcome == 0 and (not values["ack"] or values["abort"] or values["uncertain"]):
        report.error("terminal_incoherent", line, "success with ack=0, abort or uncertainty")
    if outcome != 0 and (values["ack"] or values["size"] or int(values["data"], 16)):
        report.error("terminal_incoherent", line, "failure publishing a value or acknowledgement")
    if values["uncertain"] and not values["possible"]:
        report.error("terminal_incoherent", line, "uncertain effect that was not possible")
    if values["completed"] < values["admitted"]:
        report.error("terminal_time_reversed", line)
    if previous and values["admitted"] < previous.values["completed"]:
        report.error("terminal_overlap", line)
    if len(values["data"]) != 8 or values["size"] > 4:
        report.warn("terminal_data_exceeds_print_width", line)
    if rows is not None:
        if position >= len(rows):
            report.error("step_outside_inventory", line, "step %d" % values["step"])
            return None
        if tuple(values["object"]) != rows[position][:2]:
            report.error("wrong_object_for_step", line,
                         "step %d expected %04x:%02x" % ((values["step"],) + rows[position][:2]))
        return rows[position][2]
    return None


# Routes whose terminals carry the library's slot identities: a settings transaction writes its changed
# field at that field's slot and reads back from slot 10, and a state read that includes 1003 skips the
# entry slots its count leaves unread. Their steps rise with gaps; the header's terminal count still
# shows a missing RESULT line.
SLOT_STEP_ROUTES = frozenset(("settings-2202", "io-di3", "clear-alarm", "subdivision"))
# Backlog S16: a velocity record may hold stop work after its first failed terminal, the run's own stop
# after a failure or the route's fallback QUICK_STOP after a run that ended needing a stop with none sent
# (BenchRoute::serviceAction). Only a 6040 download of 010F or 000B and the stop cycle's MONITORS, STATUS
# and FEEDBACK reads may follow it; anything else after a failure stays result_after_failure.
STOP_AFTER_FAILURE_ROUTES = frozenset(("velocity",))
# The same for a released-start enable (TRIAL_PLAN start=2, 2026-11-10): the route's own QUICK_STOP after the
# enable failed once word 15 may have reached the drive, word 15's own failed terminal included.
STOP_AFTER_FAILURE_STARTS = {"enable": 2}
_STOP_READS = frozenset(((0x2100, 0), (0x2101, 0), (0x2102, 0), (0x6041, 0), (0x6061, 0), (0x6064, 0),
                         (0x606C, 0)))
_STOP_WORDS = (bytes.fromhex("2b4060000f010000"), bytes.fromhex("2b4060000b000000"))


def _stop_work(result, window):
    """True for a stop word or a stop cycle read; a word that left must be 010F or 000B on the wire."""
    values = result.values
    target = tuple(values["object"])
    if target == (0x6040, 0) and values.get("dir") == 1:
        return window is None or bytes.fromhex(window["tx"].values["data"]) in _STOP_WORDS
    return target in _STOP_READS and values.get("dir") == 0


def _check_steps(report, results, gaps=False):
    """Identities step 1..N, restarting per operation in a multi-operation record; with gaps, rising
    from at least 1 instead."""
    seen = set()
    last_operation, expected = None, 1
    for result in results:
        values = result.values
        operation = values.get("op")
        identity = (operation, values["step"])
        if identity in seen:
            report.error("duplicate_result", result.number, "op %s step %d" % identity)
            continue
        seen.add(identity)
        if operation != last_operation:
            last_operation, expected = operation, 1
        if values["step"] < expected if gaps else values["step"] != expected:
            report.error("step_sequence", result.number,
                         "expected %d found %d" % (expected, values["step"]))
        expected = values["step"] + 1


def _check_frames_order(report, frames):
    """Copied traffic is appended in processing order; a reversal is damaged output."""
    last = None
    for frame in frames:
        if last is not None and frame.values["p"] < last:
            report.error("traffic_time_reversed", frame.number)
        last = frame.values["p"]


def _check_duplicates(report, records):
    seen = {}
    for record in records:
        if record.role in ("terminal", "traffic", "tail"):
            if record.raw in seen:
                report.error("duplicate_record", record.number, "same as line %d" % seen[record.raw])
            seen.setdefault(record.raw, record.number)


def _check_epochs(report, records):
    """Controller/node epochs are checked only where a record actually carries them."""
    observed = {}
    nodes = {}
    for record in records:
        if "epoch" in record.values and record.role in ("header", "terminal", "traffic", "tail"):
            observed.setdefault(record.values["epoch"], record.number)
        if "ne" in record.values and record.role == "terminal":
            nodes.setdefault(record.values["ne"], record.number)
    if observed:
        report.epochs = sorted(observed)
    if nodes:
        report.node_epochs = sorted(nodes)
    if len(observed) > 1:
        report.error("epoch_changed", max(observed.values()), str(sorted(observed)))
    # One operation's terminals are admitted in one node epoch; a later one is a new record.
    if len(nodes) > 1:
        report.error("node_epoch_changed", max(nodes.values()), str(sorted(nodes)))


def _check_record(report, records, expect):
    """Validate an admitted SDO record: identity, counts, wire correlation, terminal."""
    headers = [r for r in records if r.role == "header" and r.status == "parsed"]
    results = [r for r in records if r.role == "terminal" and r.status == "parsed"]
    traffic = [r for r in records if r.role == "traffic" and r.status == "parsed"]
    tail = [r for r in records if r.role == "tail" and r.status == "parsed"]
    ends = [r for r in records if r.role == "end" and r.status == "parsed"]
    report.terminals, report.traffic, report.tail = len(results), len(traffic), len(tail)
    if not headers:
        if results or traffic:
            report.error("missing_header")
        return
    if len(headers) > 1:
        report.error("duplicate_header", headers[1].number)
    header = headers[0]
    values = header.values
    report.family = header.kind
    report.operation = values.get("id", results[0].values.get("op") if results else None)
    report.node = values.get("node", expect.node or None)
    report.gap_us = values.get("gap")
    report.failure = values.get("failure", values.get("outcome"))
    if header.kind == "EVIDENCE" and report.failure == 1:
        report.failure = 0  # ReadOutcome::SUCCESS.
    report.trace_lost = bool(values.get("lost"))
    report.tail_lost = bool(values.get("tailLost"))
    names = (READ_OUTCOMES if header.kind == "EVIDENCE" else TRIAL_FAILURES if header.kind == "TRIAL"
             else FAILURES)
    if report.failure is not None and report.failure < len(names):
        report.failure_name = names[report.failure]
    if expect.family and expect.family != "any" and header.kind != expect.family:
        report.error("wrong_record_family", header.number, header.kind)
    if expect.node and report.node != expect.node:
        report.error("wrong_node", header.number,
                     "request names node %d, record node %s" % (expect.node, report.node))
    node = report.node
    if not node:
        report.error("node_unknown", header.number)
        return
    # Section order: header, terminals, traffic, tail, end. Duplicates surface below.
    ordered = [r.number for r in results + traffic + tail + ends]
    if ordered != sorted(ordered) or (ordered and header.number > ordered[0]):
        report.error("record_order", header.number)
    report.ended = bool(ends)
    if not ends:
        report.error("missing_end")
    elif len(ends) > 1:
        report.error("duplicate_end", ends[1].number)
    for name, count in (("terminals", len(results)), ("traffic", len(traffic))):
        if name in values and values[name] != count:
            code = "count_short" if count < values[name] else "count_excess"
            report.error(code, header.number, "%s header %d found %d" % (name, values[name], count))
    # The header is printed first and terminal fields are frozen, but the tail keeps
    # growing until release: its header count is a lower bound and END is the total.
    end = ends[0].values if ends and ends[0].values else {}
    if "tail" in values and len(tail) < values["tail"]:
        report.error("count_short", header.number, "tail header %d found %d" % (values["tail"], len(tail)))
    elif "tail" in values and len(tail) > values["tail"]:
        report.warn("tail_grew_during_dump", header.number,
                    "tail header %d found %d" % (values["tail"], len(tail)))
    if end:
        if end["tail"] != len(tail):
            code = "count_short" if len(tail) < end["tail"] else "count_excess"
            report.error(code, ends[0].number, "tail END %d found %d" % (end["tail"], len(tail)))
        if end["tail"] < values.get("tail", 0) or end["tailLost"] < values.get("tailLost", 0):
            report.error("end_differs_from_header", ends[0].number, "tail count or loss went backwards")
        for name in ("terminals", "traffic", "lost"):
            if end[name] != values.get(name, 0):
                report.error("end_differs_from_header", ends[0].number, name)
        report.tail_lost = report.tail_lost or bool(end["tailLost"])
    _check_duplicates(report, records)
    _check_frames_order(report, traffic)
    _check_frames_order(report, tail)

    inventory = INVENTORIES.get(expect.inventory)
    if callable(inventory):
        inventory = inventory(results)
    rows = inventory[0] if inventory else None
    report.inventory_checked = rows is not None
    # A move with an observer node holds both nodes' terminals, each RESULT naming its node: each node's
    # terminals are matched to that node's own frames, steps and epochs. The record's outcome follows
    # its own node; the observer's failures stay listed.
    nodes = sorted({result.values.get("node", node) for result in results} | {node})
    route = values.get("route")
    plan = next((r for r in records if r.kind == "TRIAL_PLAN" and r.status == "parsed"), None)
    stop_after_failure = header.kind == "TRIAL" and (
        route in STOP_AFTER_FAILURE_ROUTES or
        (route in STOP_AFTER_FAILURE_STARTS and plan is not None and
         plan.values.get("start") == STOP_AFTER_FAILURE_STARTS[route]))
    terminal_failure = None
    for each in nodes:
        own = [result for result in results if result.values.get("node", node) == each]
        frames = traffic + tail if len(nodes) == 1 else [f for f in traffic + tail if _frame_node(f) == each]
        _check_steps(report, own, header.kind == "TRIAL" and values.get("route") in SLOT_STEP_ROUTES)
        _check_epochs(report, records if len(nodes) == 1 else
                      [r for r in records if r.role != "terminal" or r.values.get("node", node) == each])
        failure = _check_node_terminals(report, own, frames, each, rows if len(nodes) == 1 else None,
                                        stop_after_failure)
        if each == node:
            terminal_failure = failure
    if report.trace_lost:
        report.warn("trace_saturated", header.number, "lost=1: copied traffic is not complete")
    if report.tail_lost:
        report.warn("tail_saturated", header.number, "tailLost=1: post-terminal traffic is not complete")
    _check_record_outcome(report, header, results, terminal_failure, inventory)


def _frame_node(record):
    """The node an SDO frame or TX report belongs to: its identifier, or a report's TX channel (`txc`,
    the firmware's slot node - 1). None for other frames and for a report that names no channel."""
    values = record.values
    identifier, kind = values["id"], values["k"]
    if kind == TX and 0x600 < identifier <= 0x67F:
        return identifier - 0x600
    if kind == RX and 0x580 < identifier <= 0x5FF:
        return identifier - 0x580
    if kind == TX_REPORT and "txc" in values:
        return values["txc"] + 1
    return None


def _check_node_terminals(report, results, frames, node, rows, stop_after_failure=False):
    """One node's terminals against that node's own copied frames; returns its first failed terminal.
    stop_after_failure: the route may send stop work after a failed terminal (STOP_AFTER_FAILURE_ROUTES)."""
    windows = _windows(report, frames, node)
    cursor = 0
    previous = None
    terminal_failure = None
    for position, result in enumerate(results):
        documented = _check_terminal(report, result, position, rows, previous)
        previous = result
        data = result.values
        if terminal_failure is not None and not (
                stop_after_failure and
                _stop_work(result, windows[cursor] if data["possible"] and cursor < len(windows) else None)):
            report.error("result_after_failure", result.number)
        if data["outcome"] != 0:
            terminal_failure = result
            report.failed.append({
                "line": result.number, "step": data["step"],
                "object": "%04x:%02x" % tuple(data["object"]), "outcome": data["outcome"],
                "outcome_name": OUTCOMES[data["outcome"]] if data["outcome"] < len(OUTCOMES) else "",
                "abort": "%08x" % data["abort"], "possible": data["possible"],
                "uncertain": data["uncertain"], "cleanup": data.get("cleanup")})
        if data["uncertain"]:
            report.uncertain.append({"line": result.number, "step": data["step"],
                                     "object": "%04x:%02x" % tuple(data["object"])})
        if not data["possible"]:
            continue  # No frame crossed the application boundary for this terminal.
        if cursor >= len(windows):
            if data["outcome"] == 0:
                report.error("success_without_request", result.number)
            else:
                report.warn("failure_without_copied_request", result.number)
            continue
        _check_window(report, result, windows[cursor], documented)
        cursor += 1
    for window in windows[cursor:]:
        report.error("unmatched_request", window["tx"].number, "no terminal claims this request")
    # A confirmed reply can precede the final TX report; only the last step may
    # lack one, and that is a limit of the copied trace rather than a failure.
    for position, window in enumerate(windows[:cursor]):
        if not window["report"]:
            (report.warn if position == cursor - 1 else report.error)(
                "tx_report_not_copied", window["tx"].number)
    return terminal_failure


def _check_record_outcome(report, header, results, terminal_failure, inventory):
    values = header.values
    if header.kind == "EVIDENCE":
        # ReadOutcome: 0 none, 1 success, anything higher is a failure.
        if (values["outcome"] > 1) != (terminal_failure is not None):
            report.error("failure_differs_from_terminal", header.number)
        return
    failure = values["failure"]
    if failure == 0:
        if terminal_failure is not None:
            report.error("failure_differs_from_terminal", header.number, "failure=0 with failed terminal")
        if values.get("lost"):
            report.error("failure_differs_from_terminal", header.number, "failure=0 with lost evidence")
        if inventory and len(results) not in inventory[1]:
            report.error("plan_size_not_in_inventory", header.number, str(len(results)))


def _verdict(report, records):
    """One summary word. Failed device evidence stays distinct from damaged output."""
    codes = {item["code"] for item in report.errors}
    if codes - INCOMPLETE - POSITIONAL or (codes and not codes & INCOMPLETE):
        return "inconsistent"
    if codes:
        return "incomplete"
    if report.family:
        lost = report.trace_lost
        if report.failed or report.failure or lost:
            return "failed-evidence"
        return "complete"
    if any(r.role == "refused" for r in records):
        return "refused"
    return "control"


def replay_bytes(data, meta=None, path="<bytes>", expect_operation=None):
    """Replay one capture's bytes and optional request.json fields; pure function."""
    meta = meta or {}
    report = Report(path=path, bytes=len(data), sha256=hashlib.sha256(data).hexdigest())
    report.command = meta.get("command", "")
    report.started_utc = meta.get("started_utc", "")
    report.serial = meta.get("serial", "")
    if report.command == "soak":
        report.soak = {"requested_trial": meta.get("trial")}
    records, unterminated = parse_bytes(data)
    report.records = records
    for record in records:
        report.kinds[record.kind] += 1
        if record.status == "unknown":
            report.unknown.append({"line": record.number, "raw": record.raw})
        elif record.status == "malformed":
            report.malformed.append({"line": record.number, "raw": record.raw, "reason": record.reason})
            report.error("malformed_line", record.number, record.reason)
        if record.extra:
            report.unknown_fields.append({"line": record.number, "fields": record.extra})
        if record.role == "error":
            report.error("format_error", record.number, record.raw)
    if unterminated:
        report.error("truncated_line", len(records), unterminated[:80])
    if meta.get("status") not in (None, "captured"):
        report.error("capture_failed", None, str(meta.get("error", meta.get("status"))))
    expect = _check_request(report, meta)
    if not records and report.command:
        report.error("empty_capture")
    _check_run(report, records, expect)
    if expect_operation is not None and report.operation != expect_operation:
        report.error("wrong_operation", None, "expected %d found %s" % (expect_operation, report.operation))
    _check_trial(report, records, meta)
    _excuse_lost_trace(report)
    report.verdict = _verdict(report, records)
    return report


def _excuse_lost_trace(report):
    """A record that reports its own copied trace lost cannot be accused by traffic matching."""
    if not report.trace_lost:
        return
    kept = []
    for item in report.errors:
        if item["code"] in TRACE_MATCHING:
            report.warn(item["code"], item["line"], item["detail"])
        else:
            kept.append(item)
    report.errors = kept


def _check_trial(report, records, meta):
    """A trial record's admission, plan and summaries must name the request and its own header."""
    headers = [r for r in records if r.role == "header" and r.status == "parsed" and r.kind == "TRIAL"]
    if not headers:
        return
    header = headers[0].values
    summaries = [r for r in records if r.role == "summary" and r.status == "parsed"]
    if len(summaries) != header["summaries"]:
        code = "count_short" if len(summaries) < header["summaries"] else "count_excess"
        report.error(code, headers[0].number, "summaries header %d found %d" % (header["summaries"], len(summaries)))
    body = [r.number for r in records if r.role in ("terminal", "traffic", "tail", "end")]
    for summary in summaries:
        if summary.number < headers[0].number or (body and summary.number > body[0]):
            report.error("record_order", summary.number, "summary outside its header")
    for summary in summaries:
        if summary.kind == "TRIAL_PLAN" and any(summary.values[k] != header[n] for k, n in
                                                (("route", "route"), ("node", "node"), ("trial", "id"))):
            report.error("plan_differs_from_header", summary.number)
        if summary.kind == "TRIAL_HISTORY":
            _check_history(report, summary, [r for r in records if r.role == "terminal" and r.status == "parsed"])
        if summary.kind == "TRIAL_VELOCITY_TARGET":
            _check_velocity_target(report, summary, records)
        if summary.kind == "TRIAL_CONTROLWORD":
            _check_controlword(report, summary, records)
    if header["route"] == "clear-alarm":
        _check_clear_method(report, records, meta)
    requested = {"route": TRIAL_ROUTES.get(meta.get("command")), "node": meta.get("node"),
                 "trial": meta.get("trial")}
    for start in (r for r in records if r.role == "start" and r.kind == "ADMITTED" and "route" in r.values):
        if any(start.values[k] != header[n] for k, n in (("route", "route"), ("node", "node"), ("trial", "id"))):
            report.error("start_differs_from_header", start.number)
        for name, wanted in requested.items():
            if wanted is not None and start.values[name] != wanted:
                report.error("wrong_procedure", start.number, "%s requested %s" % (name, wanted))


def _check_history(report, summary, results):
    """TRIAL_HISTORY names a count of at most four and exactly that many entries, and both are the
    record's own successful 1003 terminals: a summary is never a second, different observation."""
    values = summary.values
    count = values["count"]
    named = [n for n in range(1, 5) if "e%d" % n in values]
    if count > 4 or named != list(range(1, count + 1)):
        report.error("history_count_mismatch", summary.number, "count %d with entries %s" % (count, named))
        return
    read = {}
    for result in results:
        data = result.values
        index, subindex = data["object"]
        if index == 0x1003 and data["outcome"] == 0 and subindex not in read:
            read[subindex] = int.from_bytes(bytes.fromhex(data["data"])[:data["size"]], "little")
    wanted = {0: count}
    wanted.update((n, values["e%d" % n]) for n in named)
    if read != wanted:
        report.error("history_differs_from_terminals", summary.number,
                     "summary %s terminals %s" % (sorted(wanted.items()), sorted(read.items())))


# clear-alarm's reset method from protocol 10, as request.json and the C line name it, and the object and the
# two values each writes (examples/s2_bench/BenchRoute.h). TRIAL_CLEAR's method is the library's
# AlarmClearMethod, the line's number less one.
CLEAR_METHODS = {"2301": 1, "controlword": 2}
CLEAR_WRITES = {1: ((0x2301, 0), (0x0001, 0x0000)), 2: ((0x6040, 0), (0x0080, 0x0000))}


def _check_clear_method(report, records, meta):
    """A clear-alarm record names one method throughout: request.json's clear_method, the plan's method, the
    session context's method, the writes it kept and every download among its terminals. A record from before
    protocol 10 names none and is not judged; a request that names one needs a plan that does too."""
    parsed = [r for r in records if r.status == "parsed"]
    plan = next((r for r in parsed if r.kind == "TRIAL_PLAN"), None)
    requested = meta.get("clear_method")
    if plan is None or "method" not in plan.values:
        if requested is not None:
            report.error("clear_method_missing", plan.number if plan else None, "request names %s" % requested)
        return
    method = plan.values["method"]
    if method not in CLEAR_WRITES or (requested is not None and CLEAR_METHODS.get(requested) != method):
        report.error("clear_method_differs", plan.number, "plan method %d, request %s" % (method, requested))
        return
    target, values = CLEAR_WRITES[method]
    clear = next((r for r in parsed if r.kind == "TRIAL_CLEAR"), None)
    # The context is this request's only when own=1; otherwise it and its steps are an earlier clear's.
    own = clear is not None and clear.values["own"] == 1
    if own and clear.values.get("method", method - 1) != method - 1:
        report.error("clear_method_differs", clear.number, "context method %d" % clear.values["method"])
    for step in (r for r in parsed if r.kind == "TRIAL_CLEAR_STEP"):
        v = step.values
        if own and v["present"] and v["slot"] < 2 and (tuple(v["object"]) != target or v["value"] != values[v["slot"]]):
            report.error("clear_write_differs", step.number,
                         "slot %d wrote %04x:%02x=%04x" % (v["slot"], v["object"][0], v["object"][1], v["value"]))
    for result in (r for r in parsed if r.role == "terminal" and r.values.get("dir") == 1):
        if tuple(result.values["object"]) != target:
            report.error("clear_write_differs", result.number, "download of %04x:%02x" % tuple(result.values["object"]))


def _check_velocity_target(report, summary, records):
    """TRIAL_VELOCITY_TARGET repeats the record's own evidence: the plan's value, the one 60FF request on the
    wire, that download's terminal and the upload's bytes. A summary is never a second, different
    observation, and an upload never follows a download that was not acknowledged."""
    values = summary.values
    parsed = [r for r in records if r.status == "parsed"]
    plans = [r.values for r in parsed if r.kind == "TRIAL_PLAN" and "value" in r.values]
    results = [r.values for r in parsed if r.role == "terminal" and tuple(r.values["object"]) == (0x60FF, 0)]
    downloads = [v for v in results if v.get("dir") == 1]
    uploads = [v for v in results if v.get("dir") == 0]
    requests = [bytes.fromhex(r.values["data"]) for r in parsed if r.role in ("traffic", "tail") and
                r.values["k"] == TX and 0x600 < r.values["id"] <= 0x67F and
                bytes.fromhex(r.values["data"])[:4] == bytes.fromhex("23ff6000")]
    problems = []
    written = (values["value"] & 0xFFFFFFFF).to_bytes(4, "little")
    if not plans or plans[0]["value"] != values["value"]:
        problems.append("value differs from TRIAL_PLAN")
    if bytes.fromhex(values["tx"]) != written:
        problems.append("tx is not the value's four bytes")
    if len(downloads) > 1 or len(requests) > 1:
        problems.append("more than one 60FF download")
    if any(request[4:8] != written for request in requests):
        problems.append("the wire request is not the value")
    if values["written"] != bool(downloads):
        problems.append("written differs from the download terminals")
    elif downloads:
        download = downloads[0]
        if (values["outcome"], values["ack"], values["uncertain"], values["abort"]) != (
                download["outcome"], download["ack"], download["uncertain"], download["abort"]):
            problems.append("download terminal differs")
    elif values["outcome"] != 255:
        problems.append("outcome without a download")
    successful = [v for v in uploads if v["outcome"] == 0]
    if uploads and not (downloads and downloads[0]["outcome"] == 0 and downloads[0]["ack"]):
        problems.append("60FF upload after a download that was not acknowledged")
    if values["read"] != bool(successful):
        problems.append("read differs from the upload terminals")
    elif successful:
        upload = successful[0]
        data = bytes.fromhex(upload["data"])[:upload["size"]]
        if values["size"] != upload["size"] or bytes.fromhex(values["data"])[:values["size"]] != data:
            problems.append("readback differs from the upload terminal")
    if values["match"] != (bool(successful) and values["size"] == 4 and bytes.fromhex(values["data"]) == written):
        problems.append("match differs from the bytes")
    if problems:
        report.error("velocity_target_differs_from_record", summary.number, "; ".join(problems))


# controlword's reviewed set (protocol 11; BenchRoute.cpp REVIEWED_CONTROLWORDS): none has bits 0, 1 and 2 all set.
CONTROLWORDS = frozenset((0x0000, 0x0006, 0x000B, 0x0080))
_CONTROLWORD_ENABLED = 0x0006  # 6041 bit 1 switched on, bit 2 operation enabled.


def _check_controlword(report, summary, records):
    """TRIAL_CONTROLWORD repeats the record's own evidence: the plan's reviewed word, the one 6040 request on the
    wire, that download's terminal, the cycle lines and the mark. A summary is never a second, different
    observation, and the first marked read ends the record."""
    values = summary.values
    parsed = [r for r in records if r.status == "parsed"]
    plans = [r.values for r in parsed if r.kind == "TRIAL_PLAN" and "word" in r.values]
    cycles = [r.values for r in parsed if r.kind == "TRIAL_CONTROLWORD_CYCLE"]
    results = [r.values for r in parsed if r.role == "terminal" and tuple(r.values["object"]) == (0x6040, 0)]
    downloads = [v for v in results if v.get("dir") == 1]
    requests = [bytes.fromhex(r.values["data"]) for r in parsed if r.role in ("traffic", "tail") and
                r.values["k"] == TX and 0x600 < r.values["id"] <= 0x67F and
                bytes.fromhex(r.values["data"])[:4] == bytes.fromhex("2b406000")]
    word = values["word"]
    frame = bytes.fromhex("2b406000") + (word & 0xFFFF).to_bytes(2, "little") + bytes(2)
    problems = []
    if word not in CONTROLWORDS:
        problems.append("word %04x is not a reviewed controlword" % word)
    if not plans or plans[0]["word"] != word:
        problems.append("word differs from TRIAL_PLAN")
    if bytes.fromhex(values["tx"]) != frame:
        problems.append("tx is not the word's request")
    if len(downloads) > 1 or len(requests) > 1:
        problems.append("more than one 6040 download")
    if any(request != frame for request in requests):
        problems.append("the wire request is not the word")
    if values["written"] != bool(downloads):
        problems.append("written differs from the download terminals")
    elif downloads:
        download = downloads[0]
        if (values["outcome"], values["ack"], values["uncertain"], values["abort"]) != (
                download["outcome"], download["ack"], download["uncertain"], download["abort"]):
            problems.append("download terminal differs")
    elif values["outcome"] != 255:
        problems.append("outcome without a download")
    if [c["which"] for c in cycles] != list(range(1, values["cycles"] + 1)):
        problems.append("cycle lines are not 1..cycles")
    for cycle in cycles:
        if cycle["enabled"] != bool(cycle["6041"] & _CONTROLWORD_ENABLED) or cycle["moving"] != bool(cycle["606c"]):
            problems.append("cycle %d marks differ from its values" % cycle["which"])
    marked = [c["which"] for c in cycles if c["enabled"] or c["moving"]]
    if len(marked) > 1 or marked != ([values["marked"]] if 0 < values["marked"] <= values["cycles"] else []):
        problems.append("marked differs from the cycle lines")
    if values["alert"] != bool(values["marked"]) or values["alert"] != (values["enabled"] or values["moving"]):
        problems.append("alert differs from the mark")
    if problems:
        report.error("controlword_differs_from_record", summary.number, "; ".join(problems))


def _check_request(report, meta):
    """Pin the capture to the byte actually requested."""
    command = meta.get("command")
    if command is None or command == "soak":  # Its request is a line, checked against SOAK_BEGIN.
        return Expect()
    if command in COMMANDS and meta.get("tx_hex", COMMANDS[command].hex()) != COMMANDS[command].hex():
        report.error("request_byte_mismatch", None, "%s sent %s" % (command, meta.get("tx_hex")))
    if command not in EXPECTED:
        report.warn("command_not_in_grammar", None, command)
        return Expect()
    if command in TRIAL_ROUTES:
        line = meta.get("line")  # Absent when the runner refused before sending it.
        if line is not None and (not isinstance(line, str) or not line.startswith("T")):
            report.error("request_byte_mismatch", None, "%s sent %r" % (command, line))
        return EXPECTED[command]._replace(node=int(meta.get("node") or 0))
    return EXPECTED[command]


class SoakChecker:
    """Line-by-line consistency of one soak trial, used live by s2_soak.py and offline here.

    Every SOAK record but the transition's own values is checked the same way: line numbers run
    without a gap, a transition's old value is the previous new value of that signal, and a
    dump that was announced is present. Notes grow only to a fixed number per code.
    """

    NOTES_PER_CODE = 50

    def __init__(self, trial=None):
        self.trial = trial
        self.errors = []
        self.warnings = []
        self.next_line = None
        self.began = None
        self.ended = None
        self.last = {}
        self.last_time = None
        self.promised = {}
        self.delivered = {}
        self.counts = Counter()
        self.anomalies = Counter()
        self.failed = []
        self.stopped = {}
        self.latest = {}
        self.lost_lines = 0
        self.code_counts = Counter()

    def _keep(self, code):
        # A failing object read several times a second all night must not fill the host's
        # memory. The first of each code is the useful one, and the count covers the rest.
        self.code_counts[code] += 1
        return self.code_counts[code] <= self.NOTES_PER_CODE

    def _error(self, code, line, detail=""):
        if self._keep(code):
            self.errors.append({"code": code, "line": line, "detail": detail})

    def _warn(self, code, line, detail=""):
        if self._keep(code):
            self.warnings.append({"code": code, "line": line, "detail": detail})

    def _sequence(self, record):
        number = record.values["n"]
        if self.next_line is not None and number != self.next_line:
            if number < self.next_line:
                self._error("soak_line_repeated", record.number, "n=%d after %d" % (number, self.next_line - 1))
            else:
                self.lost_lines += number - self.next_line
                self._error("soak_line_gap", record.number, "expected %d found %d" % (self.next_line, number))
        self.next_line = number + 1

    def feed(self, record):
        """Take one parsed record of any kind. Other kinds are ignored here."""
        if record.status != "parsed" or record.role != "soak":
            return
        values = record.values
        self.counts[record.kind] += 1
        kind = record.kind
        if kind == "SOAK_BEGIN":
            if self.began is not None:
                self._error("soak_begin_repeated", record.number)
            elif self.trial is not None and values["trial"] != self.trial:
                self._error("soak_wrong_trial", record.number, "expected %s found %d" % (self.trial, values["trial"]))
            self.began = values
        elif self.began is None:
            # Records of a soak this request did not start, such as one still running.
            self._warn("soak_before_begin", record.number, kind)
            return
        if self.ended is not None:
            self._error("soak_after_end", record.number, kind)
        self._sequence(record)
        if kind == "SOAK_TR":
            self._transition(record)
        elif kind == "SOAK_ANOM":
            self.anomalies[values["kind"]] += 1
            if values["dump"]:
                self.promised[values["id"]] = record.number
        elif kind == "SOAK_T":
            self.delivered.setdefault(values["a"], 0)
            self.delivered[values["a"]] += 1
        elif kind == "SOAK_TEND":
            if values["a"] not in self.promised:
                self._error("soak_dump_unannounced", record.number, str(values["a"]))
            self.promised.pop(values["a"], None)
            if values["abandoned"]:
                self._warn("soak_dump_abandoned", record.number, "%d entries unprinted" % values["unprinted"])
        elif kind == "SOAK_OP":
            if self._keep("failed_operation"):
                self.failed.append({"line": record.number, "node": values["node"], "op": values["op"],
                                    "raw": record.raw})
        elif kind == "SOAK_NODE_STOP":
            self.stopped[values["node"]] = values["reason"]
        elif kind == "SOAK_END":
            self.ended = values
            if values["lines"] != values["n"]:
                self._error("soak_end_differs", record.number, "lines %d n %d" % (values["lines"], values["n"]))
        else:
            self.latest[(kind, values.get("node"))] = values

    def _transition(self, record):
        values = record.values
        key = (values["node"], values["sig"])
        if self.last_time is not None and values["t"] < self.last_time:
            self._error("soak_time_reversed", record.number, "%d after %d" % (values["t"], self.last_time))
        self.last_time = values["t"]
        if values["first"]:
            if key in self.last:
                self._error("soak_first_repeated", record.number, values["sig"])
        elif key not in self.last:
            self._warn("soak_transition_gap", record.number, "%s has no first observation" % values["sig"])
        elif self.last[key] != values["from"]:
            # A record dropped under pressure. The firmware counts it; the value chain shows where.
            self._warn("soak_transition_gap", record.number,
                       "%s from %x, previous to %x" % (values["sig"], values["from"], self.last[key]))
        self.last[key] = values["to"]

    def finish(self):
        """Checks only the whole trial can make."""
        if self.began is None:
            self._error("soak_no_begin", None)
        elif self.ended is None:
            self._error("soak_no_end", None, "trial did not reach SOAK_END")
        elif self.ended["trial"] != self.began["trial"]:
            self._error("soak_wrong_trial", None, "END names %d" % self.ended["trial"])
        if self.ended is not None:
            for identity, line in self.promised.items():
                self._error("soak_dump_missing", line, "anomaly %d" % identity)


def check_soak(report, records, trial=None):
    """Replay one soak trial into a report; a family of its own, as these records are not one SDO."""
    report.family = "SOAK"
    checker = SoakChecker(trial)
    boots = [r for r in records if r.role == "boot"]
    for record in records:
        checker.feed(record)
    checker.finish()
    begin_line = next((r.number for r in records if r.kind == "SOAK_BEGIN" and r.status == "parsed"), None)
    for boot in boots:
        if begin_line is not None and boot.number > begin_line:
            report.error("reboot_in_capture", boot.number, boot.raw)
    for item in checker.errors:
        report.error(item["code"], item["line"], item["detail"])
    for item in checker.warnings:
        report.warn(item["code"], item["line"], item["detail"])
    report.operation = checker.began["trial"] if checker.began else None
    report.started = checker.began is not None
    report.ended = checker.ended is not None
    report.failed = checker.failed
    report.failure = sum(checker.anomalies.values())
    report.failure_name = "anomalies" if report.failure else ""
    report.soak = {"trial": report.operation, "end": checker.ended["reason"] if checker.ended else None,
                   "lines": dict(checker.counts), "anomalies": dict(checker.anomalies),
                   "nodes_stopped": {str(k): v for k, v in checker.stopped.items()},
                   "lost_lines": checker.lost_lines,
                   "signals": len(checker.last), "note_counts": dict(checker.code_counts)}
    return checker


def _check_run(report, records, expect):
    """Does this output belong to this request? Then validate any SDO record."""
    if report.command == "soak":
        check_soak(report, records, report.soak.get("requested_trial"))
        return
    starts = [r for r in records if r.role == "start" and r.status == "parsed"]
    refused = [r for r in records if r.role == "refused"]
    headers = [r for r in records if r.role == "header"]
    report.started = bool(starts)
    if len(starts) > 1:
        report.error("duplicate_start", starts[1].number)
    boots = [r for r in records if r.role == "boot"]
    if boots:
        report.warn("target_boot_in_capture", boots[0].number, boots[0].raw)
    if expect.start and not refused:
        if not starts:
            # A dropped admission line must not pass silently into a record.
            report.error("missing_start")
        elif not starts[0].raw.startswith(expect.start):
            report.error("wrong_procedure", starts[0].number, starts[0].raw)
    if refused and (starts or headers):
        report.error("refused_with_work", refused[0].number)
    if expect.start and starts and expect.family and not headers:
        report.error("reboot_in_capture" if boots else "missing_header")
    if expect.responses:
        got = [r for r in records if r.role == "response" and r.kind in expect.responses]
        if not got and not refused and not headers:
            report.error("missing_response", None, report.command)
    if report.command in EXPECTED:
        allowed = set(expect.responses)
        for record in records:
            if record.role == "response" and record.kind not in allowed:
                report.error("wrong_response", record.number, record.kind)
            if record.role == "start" and not expect.start:
                report.error("wrong_response", record.number, record.kind)
            if record.role == "header" and not expect.family:
                report.error("wrong_response", record.number, record.kind)
    _check_record(report, records, expect)
    if report.command == "network" and any(r.kind == "NETWORK" for r in records) and \
            not any(r.kind == "END_NETWORK" for r in records):
        report.warn("network_end_absent", None,
                    "firmware END network line absent; listing completeness is not proven")
    if refused and not headers:
        report.warn("refused", refused[0].number, refused[0].raw)


# ---------------------------------------------------------------- transition log
class Transitions:
    """Every tracked value change, in capture order. Unchanged observations add nothing.

    Time is firmware processing time. Older captures have STATS and PONG lines with no
    time, and those keep capture order and the capture's host start time. Cumulative
    rx/tx counters are not transitions and stay in the raw line. A boot makes all
    earlier values stale, so the next observation of each signal is after_boot.
    """

    def __init__(self):
        self.entries = []
        self.last = {}
        self.after_boot = False

    def _add(self, report, record, category, signal, node, value):
        key = (category, signal, node)
        missing = key not in self.last
        if not missing and self.last[key] == value:
            return
        values = record.values
        entry = {"seq": len(self.entries) + 1, "capture": report.path,
                 "started_utc": report.started_utc, "line": record.number, "category": category,
                 "signal": signal, "node": node, "from": self.last.get(key), "to": value,
                 "first_observation": missing, "after_boot": missing and self.after_boot,
                 "time_us": values.get(record.row.time) if record.row.time else None,
                 "raw": record.raw}
        if values.get("bounds"):
            entry["receive_bounds_us"] = [values["lo"], values["hi"]]
        self.entries.append(entry)
        self.last[key] = value

    def observe(self, report):
        for record in report.records:
            if record.status != "parsed" or not record.row or not record.row.signals:
                continue
            signals = record.row.signals
            node = record.values.get("node", report.node)
            if callable(signals):
                items = signals(record, node)
            else:
                node_key = node if record.row.per_node else None
                items = [(category, name, node_key, record.values[name])
                         for category, name in signals if name in record.values]
            for category, signal, item_node, value in items:
                self._add(report, record, category, signal, item_node, value)
            if record.row.role == "boot" and record.raw.startswith(("rst:0x", "S2_BENCH ready")):
                boot = {k: v for k, v in self.last.items() if k[1] == "boot"}
                self.last = boot
                self.after_boot = True


# ---------------------------------------------------------------- capture directories
def _manifest_state(directory, manifest_path, report):
    try:
        relative = directory.resolve().relative_to(manifest_path.parent.resolve()).as_posix()
        known = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (ValueError, OSError):
        return "absent"
    states = []
    for name in ("rx.bin", "rx.txt", "request.json"):
        key = relative + "/" + name
        target = directory / name
        if key not in known:
            states.append("absent")
        elif not target.is_file() or hashlib.sha256(target.read_bytes()).hexdigest() != known[key]:
            report.error("raw_hash_mismatch", None, key)
            states.append("mismatch")
        else:
            states.append("match")
    if "mismatch" in states:
        return "mismatch"
    return "match" if set(states) == {"match"} else "absent"


def replay_capture(directory, expect_operation=None, manifest=MANIFEST):
    """Replay one retained directory read-only. rx.bin is authoritative."""
    directory = Path(directory)
    binary, text = directory / "rx.bin", directory / "rx.txt"
    source = binary if binary.is_file() else text
    data = source.read_bytes() if source.is_file() else b""
    request = directory / "request.json"
    meta = json.loads(request.read_text(encoding="utf-8")) if request.is_file() else {}
    report = replay_bytes(data, meta, str(directory), expect_operation)
    if not source.is_file():
        report.error("capture_failed", None, "no rx.bin or rx.txt")
    if binary.is_file() and text.is_file():
        other = text.read_bytes()
        # Older runner versions wrote rx.txt through a CRLF text layer; keep that visible.
        report.rx_txt = ("identical" if other == data else
                         "crlf_only" if other == data.replace(b"\n", b"\r\n")
                         else "differs")
        if report.rx_txt == "differs":
            report.warn("rx_txt_differs", None, "rx.bin is authoritative")
    report.manifest = _manifest_state(directory, manifest, report)
    report.verdict = _verdict(report, report.records)
    return report


def replay_session(directories, manifest=MANIFEST):
    """Replay captures in the given order with one transition log across them."""
    transitions = Transitions()
    reports, problems = [], []
    serials = set()
    operations = {}
    for directory in directories:
        report = replay_capture(directory, manifest=manifest)
        reports.append(report)
        transitions.observe(report)
        if report.serial:
            serials.add(report.serial)
        if any(r.raw.startswith("S2_BENCH ready") for r in report.records):
            operations.clear()  # A new boot restarts firmware operation identities.
        if report.operation is not None:
            key = report.operation
            if key in operations:
                problems.append({"code": "operation_repeated_without_observed_boot",
                                 "captures": [operations[key], report.path]})
            operations[key] = report.path
    if len(serials) > 1:
        problems.append({"code": "mixed_fixture_serials", "serials": sorted(serials)})
    return reports, transitions.entries, problems


def check_image(report, build):
    """Compare a ping capture with a retained s2_flash.py build record; pure function."""
    pongs = [r for r in report.records if r.kind == "PONG" and r.status == "parsed"]
    if len(pongs) != 1:
        return ["expected exactly one PONG, found %d" % len(pongs)]
    values = pongs[0].values
    problems = []
    elf = build.get("image_sha256", {}).get(build.get("app_elf", "motorcontrol_can_s2_bench.elf"))
    for name, wanted, required in (("base", build.get("base_commit"), True), ("elf", elf, True),
                                   ("target", build.get("target"), False),
                                   ("sdk", build.get("sdk_version"), False)):
        # Records older than s2_flash.py name no target or SDK; a missing fact is
        # not a match, so report it only where the record had to carry it.
        if wanted is None and not required:
            continue
        if values[name] != str(wanted).lstrip("v"):
            problems.append("%s: image reports %s, build record has %s" % (name, values[name], wanted))
    if not values["pinsReady"]:
        problems.append("pinsReady=0: the silent output could not be established")
    return problems


def _write_exclusive(path, content):
    """Never overwrite: a report path is a new artifact, like a capture directory."""
    with Path(path).open("x", encoding="utf-8", newline="\n") as output:
        output.write(content)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("captures", nargs="+", type=Path, help="Capture directories, in session order")
    parser.add_argument("--expect-terminals", type=int, help="Required terminal count for every capture")
    parser.add_argument("--expect-traffic", type=int, help="Required traffic count for every capture")
    parser.add_argument("--require-complete", action="store_true",
                        help="Fail unless every capture holds a complete successful record")
    parser.add_argument("--report", type=Path, help="New JSON file for the full report")
    parser.add_argument("--transitions", type=Path, help="New JSONL file for the transition log")
    args = parser.parse_args(argv)
    for directory in args.captures:
        if not directory.is_dir():
            parser.error("not a capture directory: %s" % directory)
    for path in (args.report, args.transitions):
        if path and path.exists():
            parser.error("output already exists: %s" % path)
    reports, transitions, problems = replay_session(args.captures)
    status = 0
    for report in reports:
        print("%s: %s family=%s node=%s op=%s terminals=%d traffic=%d tail=%d raw_ok=%d typed_ok=%d "
              "failed=%d uncertain=%d width_conflicts=%d unknown=%d malformed=%d manifest=%s" % (
                  report.path, report.verdict, report.family or "-", report.node, report.operation,
                  report.terminals, report.traffic, report.tail, report.raw_uploads_successful,
                  report.typed_successes, len(report.failed), len(report.uncertain),
                  len(report.width_discrepancies), len(report.unknown), len(report.malformed),
                  report.manifest))
        for item in report.errors:
            print("  error %(code)s line=%(line)s %(detail)s" % item)
        for item in report.warnings:
            print("  warning %(code)s line=%(line)s %(detail)s" % item)
        for item in report.width_discrepancies:
            print("  width %(object)s actual=%(actual)d documented=%(documented)d (%(evidence)s)" % item)
        for item in report.typed_width_failures:
            print("  typed width failure %(object)s wire=%(actual_wire_width)d abort=%(abort)s" % item)
        if report.verdict in ("inconsistent", "incomplete"):
            status = 1
        if args.require_complete and report.verdict != "complete":
            status = 1
        if args.expect_terminals is not None and report.terminals != args.expect_terminals:
            print("  expected %d terminals" % args.expect_terminals)
            status = 1
        if args.expect_traffic is not None and report.traffic != args.expect_traffic:
            print("  expected %d traffic records" % args.expect_traffic)
            status = 1
    for problem in problems:
        print("session %s" % json.dumps(problem))
        status = 1
    print("transitions: %d" % len(transitions))
    if args.report:
        _write_exclusive(args.report, json.dumps(
            {"captures": [r.public() for r in reports], "session": problems,
             "transitions": len(transitions), "time_basis": TIME_BASIS},
            indent=2) + "\n")
    if args.transitions:
        _write_exclusive(args.transitions, "".join(json.dumps(item) + "\n" for item in transitions))
    return status


if __name__ == "__main__":
    raise SystemExit(main())
