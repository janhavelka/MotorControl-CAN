# Communication practice: health, retries, queues and recovery

This guide holds the rules for how the CAN owner and the reference supervisor
treat failed exchanges: health and presence, retries, timeouts, queues, status,
counters, events and recovery. Each rule is stated for CANopen and CAN. The
prompts that implement a rule cite it by number (C1 to C15 of this guide; the
sibling MotorControl-EC guide numbers its own rules separately).

The owner asked for it on 2026-11-18: "I dont know how thats handled with
ethercat - but in rs485, I2C in fieldcore - im dealing with retransmissions
retries, health, statuses, queues - if thats a good engineering practise check
fieldcore, get inspired with it and apply the same/similar rules to this and to
motorcontrol-can repository/its promptsets".

Labels on each rule:

- **Documented**: a primary source states it; the source is named.
- **Practice**: an engineering choice of this project; it can be revised with a
  reason.
- **To measure**: a value or behavior that only a bench run establishes. Until
  then it has no default.

The [AGENTS](../AGENTS.md) rules stay above every rule here: no automatic retry
in the installed core, no replay of motion, enable, controlword edges, HALT,
fault clear, NMT, save or an unconfirmed SDO download, and uncertainty after an
unconfirmed write is kept. Boot-up, bus-off, rebinding and configuration
changes invalidate dependent evidence. This guide is not a safety function.

## 1. Source and verdict

The practices were taken from FieldCore-node (private), paraphrased: its RS485
and I2C owners were read read-only on 2026-11-18 and each practice is described
here in this project's own words. No text, code, path or symbol of that
repository is used in this file. The earlier [FieldCore RS485
audit](reports/2026-11-17_fieldcore_rs485_audit.md) and prompts 68 and 74 hold
the 2026-11-17 adoption decisions; this guide extends them and changes none
that it does not name.

Sources for the rules and what this checkout holds of them:

| Source | Used for | Held here |
| --- | --- | --- |
| ISO 11898-1 | Error states, error counters, automatic retransmission, ACK | Not held. General CAN knowledge; recheck before a constant is written. The [TWAI SDK notes](adapters/twai_sdk.md) record what the SDK exposes and the 2026-11-03 error-passive observation |
| CiA 301 V4.2.0 | SDO protocol and abort codes, toggle bit, heartbeat, node guarding, EMCY | Not held in this pack (members-only terms; the MotorControl-EC store has a local copy that must not be committed here). Code values below are marked "from the rule set, to verify" and are corroborated only by the pinned CANopenNode source in `experiments/canopen_service_fit/vendor/CANopenNode/301/` |
| Lichuan EDS and manual | Which services the drive implements (no heartbeat consumer `1016`, producer `1017` default 0) | [Host-loss guide](host_loss.md), reference pack |

## 2. Rules

### C1 One owner per interface

One owner per physical CAN interface does all receiving and sending and keeps
the queues, deadlines, counters and recovery. Device modules interpret their
own data and never schedule bus work. Observers take copies. *Practice.*
`CanBusOwner` in the application's CAN task is that owner
([CONVENTIONS](prompts/CONVENTIONS.md) section 4, prompt 68).

### C2 Bus-level and node-level faults are different facts

*Practice*; the error states are *documented* (ISO 11898-1).

- Bus or controller level, naming no node: error-passive (TEC or REC at or
  above 128), bus-off (TEC above 255), no ACK (no listener acknowledged a sent
  frame), RX overflow (frames lost in the controller FIFO or the driver queue),
  TX lifetime expiry, a failed install.
- Node level: an SDO reply, abort or timeout on that node's channel; that
  node's heartbeat or guarding answer or the host's consumer timeout for it;
  its boot-up message; its EMCY.
- An ACK proves that some receiver saw the frame, not that the target node did
  (prompt 55 records this). No ACK is bus evidence, not node absence.
- While the controller is bus-off, error-passive with no ACK, or after an RX
  overflow, node misses are not counted. The node rows keep their last state
  marked "not observed". An RX overflow also invalidates evidence that may have
  been in the lost frames (a reply, a heartbeat, an EMCY).
- Consequence for `Health`: bus-off and controller unavailability are an
  interface fact (interface `OFFLINE` through the controller state), not an
  `OFFLINE` of every binding. Prompt 55 plans this revision of prompt 25's
  delivered behavior; [can_owner.md](can_owner.md) describes today's code.

### C3 Evidence classes of a terminal

*Practice.* The owner classifies each terminal once.

| Class | CANopen terminals | Health effect |
| --- | --- | --- |
| Answered | Valid SDO response; heartbeat or guarding reply | Success; the miss count is reset |
| Answered with error | Server abort; wrong multiplexer; toggle error; malformed or wrong-size reply | Node present; an error, not a miss |
| No answer | SDO response timeout with no response frame; heartbeat consumer timeout | One miss (once per operation, after its repeats) |
| Local end | Cancelled; expired in the queue; invalidated by boot-up or rebinding; refused at admission; TX never left; ended while a bus-level fault was active (C2) | None |

Two more facts are not terminals. An EMCY is a node event: it shows presence,
counts as neither success nor miss and completes no transfer. An abort that the
client itself sends after its own timeout (SDO protocol timed out, `0x05040000`
from the rule set, to verify) is cleanup, not evidence.

### C4 Health and presence

*Practice*; the thresholds are caller settings with no default that describes a
real machine (*to measure*; prompt 74 keeps its 2 and 5 as a reference value,
not a measurement).

- Per node: Unknown until the first answer of the current identity, then
  Online; Degraded after `degradedAfter` consecutive misses or while the node
  reports an error; Offline after `offlineAfter` consecutive misses; Disabled
  by the application and never probed. `0 < degradedAfter < offlineAfter`. One
  answer of the current identity restores Online.
- A miss is counted once per operation, after its repeats. Local ends and
  passive age are not misses. A node nobody addressed does not age into
  Degraded. A heartbeat consumer timeout is a miss with its own cause. A
  boot-up is a new identity: the node returns to Unknown until rebound, and its
  evidence is invalidated.
- Application aggregation: the worst Required row; Optional rows visible and
  not aggregated; Disabled ignored. A start-up grace hides a row that has no
  evidence yet and never an explicit fault.
- Health is a projection. It never blocks a stop, a read or a recovery.

### C5 Controller error counters are bus evidence

*Documented* (ISO 11898-1; the SDK's controller status, see
[twai_sdk.md](adapters/twai_sdk.md)): TEC and REC, the error state, bus-off
count, arbitration-lost count, bus-error count and receive overrun counts.
Rule: report each with its read time and its delta since the last read. A
rising bus-error rate above a caller-set threshold makes the interface Degraded
with the cause "bus errors" and never marks a node Offline. Unlike an EtherCAT
port counter, these counters do not locate a node or a cable segment; say so
wherever they are shown. The thresholds are *to measure*.

### C6 Retry classes by operation kind

*Practice*; the classes are prompt 55's (R, W, M, N; default N).

| Operation | Rule |
| --- | --- |
| RPDO set-points and controlwords | Never repeated by policy. The next SYNC or event period carries the held level; a held level sent again is not a replay |
| Controller hardware retransmission | Link level, not an application retry (C7) |
| Heartbeat, guarding | Not repeated; the next period is the repeat |
| SDO upload | R only for a read whose `readEffect` is reviewed non-changing, or accepted under DEC-1, or a recorded per-object permission; every other read is N. Each repeat is counted |
| SDO download (a setting) | W only with readback, and only if the failed attempt never possibly left. An uncertain download is re-read and reported, never sent again |
| Motion start, enable, homing, HALT `010F`, fault reset, save, NMT | M: never repeated. A quick stop may be issued again as a new, separately identified operation (prompt 55); `010F` never |

### C7 Controller retransmission and the SDO toggle are link mechanisms

*Documented* (ISO 11898-1: a transmitter repeats a frame automatically after an
error or a missing ACK; CiA 301: the toggle bit alternates per segment of a
segmented transfer, and a wrong toggle ends the transfer with abort
`0x05030000`, toggle bit not alternated. Code value from the rule set, to
verify against CiA 301 V4.2.0; corroborated by `CO_SDOserver.h` lines 298 and
299 of the pinned CANopenNode copy).

Rule:

- Automatic retransmission by the controller is not an application retry. It
  needs no retry class and is bounded by the TX lifetime (prompt 55 item 2).
  Its hazard is late delivery: a frame not confirmed within its lifetime stays
  "possibly sent" until a controller restart retires it, and nothing is resent
  (the 2026-11-03 stuck frame is the evidence, DEC-4 in the SDK notes).
  *Practice.*
- Single-shot transmission (no hardware retransmission) is a design option to
  evaluate, not a default. Today's configuration keeps hardware retransmission
  (DEC-4). *To measure.*
- A toggle mismatch is "answered with error" (C3). The client never repeats a
  segment by itself.

### C8 Retry budget and backoff

*Practice.* A repeat is a new operation that records `repeatOf`. It runs inside
the original deadline and never extends it. It starts after a backoff that
doubles from a caller-set base on each repeat and is cut by the remaining
deadline. The count is a caller setting, default 0. Exhaustion is its own
terminal, distinct from the last attempt's outcome. A repeat waits until prompt
55's resync has reopened the route (C9).

### C9 Quarantine

*Practice.* After an SDO timeout that may have left, the route is quarantined
under prompt 55's reuse policy; waiting alone reopens nothing. A node with an
old epoch (after a boot-up) gets no work until rebound. An Offline node gets
only the probe (prompt 74 item 1).

### C10 Timeouts and deadlines

*Practice.* One immutable deadline per operation, fixed at admission. Queue
time consumes it. One per-node response timeout derives every attempt's
cutoffs (prompt 73 item 1). Expiry is `now >= deadline`. A real completion
seen before the expiry check wins. Every wait has a bound. The SDO response
timeout is a client setting; CiA 301 does not fix a value (to verify against
V4.2.0). The default comes from prompt 56's measured reply times (*to
measure*).

### C11 Bounded queues, priority and fairness

*Practice.* Reserve, then enqueue; refuse when full and change nothing; never
drop admitted work or an unread result; one terminal per accepted command
(prompt 68). Order: stops, then recovery and urgent work, then ordinary work
that rotates between nodes and channels, first in first out within a channel.
Every queue (command, TX, RX, event, trace) reports depth, capacity, high-water
mark and overflow count. An RX queue overflow is a bus fact (C2). The event
queue may lose entries, each counted; it never loses a terminal.

### C12 Status and stale marking

*Practice.* Each value carries its acquisition time and node epoch. Only a
completed exchange refreshes it; a cached copy, a cancellation, a result take or
a local refusal never does. A stale value is flagged and kept, never zeroed.
Heartbeat freshness is liveness and stays separate from state freshness
(prompt 73 item 4).

### C13 Statistics

*Practice.* Counters saturate, start at zero at boot and are cleared only by
`clearStats`, which keeps states, generations, uncertainty and retained
results. Per node and per interface: successes, errors, misses, timeouts,
aborts sent and received, repeats, resyncs, controller restarts started,
succeeded and failed, TX expiries, RX overflows, bus-off events, queue refusals
by reason, high-water marks, event drops and suppressions, the last error with
its time, observed extremes. An extreme is an observation, not a guarantee.
Node counters come only from node-level evidence (C2).

### C14 Events

*Practice.* Events report transitions: Degraded, Offline, returned, boot-up,
EMCY, bus-off, error-passive, recovery started, succeeded or failed, queue
overflow. Routine success is a counter. A policy layer limits each event kind
per node to one per caller-set interval and counts what it suppresses; status
and counters stay current when an event is suppressed. Recovery events are
reported once per episode.

### C15 Recovery and its rate limit

*Practice.* Recovery is an explicit request by default: single-flight, with its
own retained result. It invalidates bindings and motion evidence and inhibits
motion. An automatic schedule is optional and bounded: a minimum interval, a
backoff to a maximum, a cap per window and per boot, no catch-up after a pause
and no replay. Ordinary SDO timeouts and misses never trigger a controller
restart by themselves; bus-off, RX overflow, a TX expiry and a failed install
may (prompt 74 item 4). No NMT reset on loss. A failed recovery stays visible.

## 3. What does not transfer

From FieldCore (as prompt 68 already says): half-duplex timing, inter-request
gaps, one transaction per bus, fixed poll delays, and sensor-style retries for
motor commands. From the EtherCAT guide: working counter, AL status codes,
per-port ESC counters and link localisation, the mailbox repeat request,
generations named as such (CAN uses node epochs and boot-up) and distributed
clocks.

## 4. Where the rules are enforced

| Rule | Prompts and guides |
| --- | --- |
| C1 | 68; CONVENTIONS section 4 |
| C2, C3 | 55 item 3; 74 items 1 and 6; CONVENTIONS section 5 |
| C4 | 74 items 1 and 6 |
| C5 | 55 (counters delivered), 74 item 6 |
| C6, C8 | 55 item 4; 74 item 2 |
| C7 | 55 item 2 |
| C9 | 55 item 1; 74 items 1 and 2 |
| C10 | 73 item 1; 55 item 2 |
| C11 | 56 item 1; 68 |
| C12, C14 | 73 item 4; 74 item 6; CONVENTIONS section 5 |
| C13 | 55 (delivered counters); 74 item 6 |
| C15 | 55 item 2; 74 item 4 |
