# Native examples and shared application

The shared console now connects typed reads, conditional settings/network work
and finite motion/actions through the same owner. [The CLI contract](cli_contract.md)
owns exact grammar, supplied deadlines, result lifetimes and evidence gates.
`ProbeMotion` attaches the caller's actual live axis/session/reference and explicit
native prerequisites. `MotionIntent` supplies reusable pure host intent; neither
creates qualification, receives CAN or duplicates a native operation state machine.


The shared [console](cli_contract.md) now uses this application. Offline
axis facts live in ProbeAppLocal.cpp, separately from the selected live route;
ProbeConsole performs parsing/correlation/formatting, never target arithmetic or
CAN receive. The native entry offers bounded `--console-fake FILE` in addition
to the unchanged explicit read demonstration.

The [offline units preview](../examples/units_preview/README.md) uses only the
installed pure Axis/Units API. The [fake native probe](../examples/native_probe/README.md)
adds the optional CANopenNode adapter, ordinary CanBusOwner and shared ProbeApp.
The [finite evidence procedure](../examples/device_evidence/README.md) adds the
same application's bounded acquisition collector. These examples run without a framework, CLI, physical controller or sibling repositories.
They do not qualify hardware or create motion APIs.

The [finite velocity preview](../examples/velocity_preview/README.md) adds exact
rate conversion and a closed firmware gate. `AxisSession::requestVelocity` uses
the same owner and urgent-stop path, with its [fixed timing contract](velocity.md).
It does not auto-run at firmware startup.

## Responsibilities and explicit read flow

| Component | Responsibility |
| --- | --- |
| [ProbeApp](../examples/common/ProbeApp.h) | One copied selection, one concrete typed read context, explicit identity/configuration/state requests, local cancellation and checked getters. No command parser, transport callbacks or firmware startup. |
| [DeviceEvidence](../examples/common/DeviceEvidence.h) | Explicit finite acquisition, copied native terminals and bounded diagnostic traffic from this application/owner. No protocol engine, hardware receiver or parser. |
| [ProbePlatform](../examples/common/ProbePlatform.h) | External time acquisition, bounded byte input/output and one optional bounded idle/diagnostic callback. No CAN receive or operation stepping. |
| [CanBusOwner](can_owner.md) | Sole receive/dispatch path, copied admission, scheduling, passive events and retained terminals. Existing service/node storage is reused. |
| [FakeTransport](../examples/native_probe/FakeTransport.h) | Native-only synthetic server and fixed ingress/TX-report queues. Explicitly supplied time and events; no physical driver. |

All objects are stationary, caller owned and serviced in one cooperative context.
Discovery borrows the same read context through
`requestDiscoveryRead(PROBE|IDENTITY, target, ...)`. Its explicit acquisition target
does not change `selected()`, even temporarily. The normal selected-target getters
still reject observations for another target; the scan retains its own copied findings.
Constructing them, selecting a target, querying cached health and servicing an
idle application generate no CAN command. Selection is explicit route metadata,
not presence detection or model identification. Identity and configuration reads
carry the existing unqualified read-effect policy; Q20 still blocks a documented
non-consuming bootstrap probe. The fake provides an external fresh/usable-session
premise explicitly. That premise is not inferred for a connected drive.

1. Configure the existing service route and lifecycle/network anchor outside the
   application, with applicable session and service evidence.
2. `select` copies a valid target with a known binding. `requestIdentity` or
   `requestConfig` or `requestState` prepares a whole finite raw read and reserves its axis. State reads require explicit nonzero block selection. No TX
   occurs in either call. Operation identities never wrap.
3. Acquire time through `ProbePlatform::nowUs` or supply monotonic microseconds
   directly. `ProbeApp::service(nowUs)` runs the owner first, admits at most one
   new field, then services bounded byte I/O. No text input executes a request.
4. Inspect `owner.result(app.pendingRequest())` explicitly. The pointer is
   non-consuming and remains stable while platform output is blocked. Call
   `app.advanceRead(nowUs)` to consume that canonical terminal and release its
   owner slot. The concrete read context retains copied raw evidence, including
   abort/timeout/uncertain effects. This call can validly advance to failure.
5. Use `getIdentity`, `getConfig` or `getStateBlock` for the applicable publication
   boundary; a completed state block can survive failure of a later block. They reject wrong kind,
   incomplete, stale or invalidated evidence without changing output. No route,
   heartbeat policy or status dialect is adopted from raw readback.
6. `releaseRead` explicitly releases terminal context/history and reservation.
   It cannot cancel live work, clear channel quarantine or replay a failed read.

`cancelRead(nowUs)` withdraws a live read locally and retains any pending owner
terminal for normal advancement. Earlier terminal/deadline evidence wins.
Cancellation is not a motor stop and does not settle unknown read effects.

## Finite acquisition collector

`DeviceEvidence::request` copies an exact typed plan and its fixture/revision/
observation references. Default DEVICE provenance rejects ordinary acquisition
while Q20 read effects are unqualified. `requestExperiment` admits only the
separate reviewed unqualified-read path; references record external assertions,
not authenticated permission or a harmless-read claim. HOST_FAKE provenance is
fixed at construction and applies only to offline fake transports.

During acquisition, service the application first, then the collector. The
collector owns diagnostic draining and typed advancement until terminal release;
do not advance the app, drain diagnostics or reset statistics independently.
`record()` is a passive borrowed view of owned history: intended descriptors,
raw typed values, up to 15 full terminals and 64 traffic copies, initial/final
owner statistics/resources and observed service gaps. Trace or reported RX loss
interrupts further acquisition and remains visible. Timeouts, aborts, boot-up,
local interruption and unknown effects retain their failed evidence without retry.

The [fake procedure](../examples/device_evidence/README.md) documents build/run
commands and bounded output. [Harness tests](../test/device_evidence_test.cpp)
exercise failures and provenance; [firmware tests](../test/firmware_probe_test.cpp)
prove automatic collection through the ordinary assembly. Applicable source
questions and physical prerequisites remain in the [conflict gates](device_conflicts.md)
and [bench guide](hardware_bench.md). No state-changing experiment executor or
physical qualification is implemented.

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
| Output queue | 512 bytes, enough for the bench soak's longest line at its field maxima; complete submissions copy atomically. Insufficient capacity preserves queued bytes and saturates `outputRejected` once per submission. Invalid null spans reject separately. |
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
[Shared firmware startup and platform time](firmware_examples.md) now reuse this
application unchanged. Command parsing remains prompt 40.

The optional [TWAI transport](../adapters/twai/README.md) now supplies the
pinned S3 controller boundary. The Arduino/IDF assembly uses checked 64-bit
ESP timer microseconds, with no 32-bit wrap extension or hidden core clock.

A motion application registers its actual `AxisSession` with `ProbeApp::openAxis`,
constructs `ProbeMotion(app,owner,session,currentTarget,axis,reference)` in stationary
storage, supplies actual typed prerequisites, and calls `console.attachMotion`.
Call `app.service(now)` once before `console.service(now)`; the console snapshots
its helper after that owner/session turn. A direct caller may snapshot the helper
itself without a console. All operation requests for the attached session must use
that attachment while results are retained. Replaced native contexts are detected
by exact local identities; local IDs still cannot distinguish identical late SDOs.

Downloads require exact `ownsDownload` -> `preflight` dispatch at the owner's
before-transmit boundary, with the application's actual undecided-drive-fault fact.
A matching guard's false result cannot fall through to a permissive route. Firmware
`setPreflight` is explicit and defaults closed. Device counter zero/home can advance
native coordinates; helper and selection follow only that owned exact epoch, keeping
old read provenance stale. Host scale/origin similarly require the existing idle,
stationary evidence and owner migration. Neither path reopens a session or clears
its historical uncertainty fences.

Measured records (prompt 58) reach motion through the same attachment. The
application attaches one caller-owned `AxisRecords` per axis (`attachRecords`;
`SelectedAxis` does it for its axis) with four slots: enable, absolute position,
relative position and velocity, so an application holds at most four records per
attached axis, in RAM only (1408 bytes per axis in the host and S2 builds, compile-time sizes).
`loadRecord` decodes and validates a whole record line (or a record built in code)
into its empty slot, `useRecord` binds it to the axis's binding, configuration,
node and controller generations, `showRecord` copies it with its state (LOADED,
BOUND or INVALIDATED) and `dropRecord` forgets it. None of them sends, enables or
moves anything. A bound record governs the regular path of its slot: unless the
setter named an attended experiment for that operation (velocity or enable
`ATTENDED_EXPERIMENT`, which takes precedence in either order, since unmeasured
ranges stay under the attended experiment), `requestMove`, `requestVelocity` and
`requestEnable` (and `startAction`'s enable) build the evidence with
`makeMoveEvidence`, `makeVelocityEvidence` or `makeEnableEvidence` and refuse a
request outside the record with `checkRecordMove`, `checkRecordVelocity` or
`checkRecordEnable` before the session's own preparation, which keeps every library
limit. `MotionResult::attendedExperiment` and `MotionResult::record` say which ran;
a run under a record refuses `requestMoveReplacement` and `requestVelocityChange`
before any TX. A record narrows prerequisites the application named, so it reaches
`requestMove` and `requestVelocity` only where `setMovePrerequisites` or
`setVelocityPrerequisites` named them (the S2 bench console's arming does;
`SelectedAxis` and the probe consoles do not, so there only the enable record
affects a route). A rebinding, configuration change, drive boot-up (as the owner observes it) or
controller recovery invalidates the binding, and requests of that slot are refused
until the record is used again or dropped; a restart forgets every record and
restores none. Without a bound record every gate is as before.

Register multiple stationary attachments to the same console for independent axes.
The application assigns a distinct fairness producer to each open session. Changing
selection never changes a retained operation's owner. A `GroupStop` borrows a
separate caller-owned result array and fans out explicit per-axis requests; ordinary
pressure cannot consume its storage. See [concurrent axes](multi_axis.md) for
individual stop, group results, capacity and storage lifetimes.

## Explicit example storage migration

`DeviceEvidence` and `FirmwareProbe` now take an explicit `AcquisitionRecord*`.
`BenchRoute` takes an explicit `RouteRecord*`. The storage must be constructed,
aligned and stationary while bound; a null pointer refuses acquisition. Only an
idle collector without a retained result can detach or bind another record.
The S2 bench shares these records in its existing exclusive arena; the ordinary
firmware example uses a separate caller-owned record. This changes optional
example constructors, not the installed core ABI.

`AxisSession` shares home/velocity storage. Successful cross-kind admission ends
borrowed references to the earlier native context; copy history needed afterward.
The inactive getter returns an immutable EMPTY context. Failed admission preserves
the old context, and stop/fault fences and published references remain independent.
