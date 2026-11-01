# Native examples and shared application

The [offline units preview](../examples/units_preview/README.md) uses only the
installed pure Axis/Units API. The [fake native probe](../examples/native_probe/README.md)
adds the optional CANopenNode adapter, ordinary CanBusOwner and shared ProbeApp.
Both run without a framework, CLI, physical controller or sibling repositories.
They do not qualify hardware or create motion APIs.

## Responsibilities and explicit read flow

| Component | Responsibility |
| --- | --- |
| [ProbeApp](../examples/common/ProbeApp.h) | One copied selection, one concrete typed read context, explicit identity/config requests and checked getters. No command parser, transport callbacks or firmware startup. |
| [ProbePlatform](../examples/common/ProbePlatform.h) | External time acquisition, bounded byte input/output and one optional bounded idle/diagnostic callback. No CAN receive or operation stepping. |
| [CanBusOwner](can_owner.md) | Sole receive/dispatch path, copied admission, scheduling, passive events and retained terminals. Existing service/node storage is reused. |
| [FakeTransport](../examples/native_probe/FakeTransport.h) | Native-only synthetic server and fixed ingress/TX-report queues. Explicitly supplied time and events; no physical driver. |

All objects are stationary, caller owned and serviced in one cooperative context.
Constructing them, selecting a target, querying cached health and servicing an
idle application generate no CAN command. Selection is explicit route metadata,
not presence detection or model identification. Identity and configuration reads
carry the existing unqualified read-effect policy; Q20 still blocks a documented
non-consuming bootstrap probe. The fake provides an external fresh/usable-session
premise explicitly. That premise is not inferred for a connected drive.

1. Configure the existing service route and lifecycle/network anchor outside the
   application, with applicable session and service evidence.
2. `select` copies a valid target with a known binding. `requestIdentity` or
   `requestConfig` prepares a whole finite raw read and reserves its axis. No TX
   occurs in either call. Operation identities never wrap.
3. Acquire time through `ProbePlatform::nowUs` or supply monotonic microseconds
   directly. `ProbeApp::service(nowUs)` runs the owner first, admits at most one
   new field, then services bounded byte I/O. No text input executes a request.
4. Inspect `owner.result(app.pendingRequest())` explicitly. The pointer is
   non-consuming and remains stable while platform output is blocked. Call
   `app.advanceRead(nowUs)` to consume that canonical terminal and release its
   owner slot. The concrete read context retains copied raw evidence, including
   abort/timeout/uncertain effects. This call can validly advance to failure.
5. After all fields, use `getIdentity` or `getConfig`. They reject wrong kind,
   incomplete, stale or invalidated evidence without changing output. No route,
   heartbeat policy or status dialect is adopted from raw readback.
6. `releaseRead` explicitly releases terminal context/history and reservation.
   It cannot cancel live work, clear channel quarantine or replay a failed read.

Time zero is valid. Backward time is rejected; cutoffs are exclusive. Waiting
for owner result capacity never extends the immutable overall deadline. A
canonical terminal cannot be consumed with a time before its completion. Typed
getters retain the underlying conservative multi-field age/skew semantics;
the read is not an atomic device snapshot. Selection cannot change while a
context is active or terminal-retained. Lifecycle invalidation withdraws current
publication while keeping history inspectable.

## Fixed work and pressure behavior

| Storage/work | Bound and exhaustion behavior |
| --- | --- |
| Selected targets / read contexts | One each; explicit release before replacement. Shared application uses owner producer 1. |
| Input queue | 64 bytes; at most 8 read callbacks/turn. Full storage drops newly consumed bytes and saturates `inputDropped`. `takeInput` consumes bytes only; no parser exists yet. |
| Output queue | 256 bytes; complete submissions copy atomically. Insufficient capacity preserves queued bytes and saturates `outputRejected` once per submission. Invalid null spans reject separately. |
| Output service | At most 16 byte callbacks/turn. A refusal accepts nothing, stops that drain and preserves the suffix; no wait or retry loop in that turn. |
| Idle/diagnostics | At most one optional bounded callback after byte work; no CAN access, recursion or blocking output. Owner diagnostic copies are pulled through the existing `takeDiagnostic` API. |
| Owner service | Existing RX 8, TX 2, reports 8, starts 2 and recovery poll 1 maximum per turn; see the authoritative [owner capacity table](can_owner.md). |
| New read admission | At most one field/turn, with at most one unreleased owner request for this application. Result pressure waits without replay. |
| Fake ingress / reports | 16 owned frames / 4 reports. Full queues refuse new input or a handoff atomically; no silent eviction. |

Callbacks borrow data only during the call. `writeOutput` copies submitted
bytes; controller ingress/report storage owns full values. A slow consumer loses
only the documented input/diagnostic copies, never authoritative owner RX or
retained service results. Callbacks must meet their bounded nonblocking contract;
these budgets are work counts, not a wall-time or embedded scheduling guarantee.
Native sizes and checks are recorded in the [stage report](reports/prompt_26.md).
Firmware adapters and clock wrap extension belong to prompts 27–28; command
parsing belongs to prompt 40.
