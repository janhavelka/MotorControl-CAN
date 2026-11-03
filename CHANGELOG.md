# Changelog

## Unreleased

### Bench route review fixes (closing audit)

An independent review of the bench routes found no unrequested motion or controlword write, and these defects, each fixed with a test that fails without it:

- Lost evidence ends every route `EVIDENCE_LOSS` (`why=lost`), never `NONE`, and withdraws an active enable or move locally (`lossCancel=1`); nothing is sent because of the loss. An EMCY or loss withdrawal uses the later of the route's and the owner's time, so the session cannot refuse it.
- `move` and `position` with a planned stop are refused (`why=deadline`) before their first write unless the stop's span fits the route cutoff; plans that never can are refused at admission (`duration`).
- The check step's predecessor is judged again when the first write is offered; `begin()` applies the parser's range check (`reason=plan`).
- Bench line protocol 3: every `T` line ends in `,K` and a CRC-32 (zlib) of the bytes before it; the firmware refuses `reason=crc`; PONG reports `routes=3`, and the host refuses other images. **An image built before this needs reflashing before any route runs.**
- `s2_bench.py` campaign ledger: an exclusive `PATH.lock` per invocation (`campaign busy`; a stale lock is kept for the operator), a reload and recheck before the send entry, a `seq`/`prev` SHA-256 chain and consistency checks; unchained ledgers are refused. A chain cannot detect a tail cut at a line end or a restored older copy, so commit the ledger with each session's evidence.
- `s2_evidence.py`: separate transition names for enable, stop and move results; a `lost=1` record is no longer accused by traffic matching; header field `lossCancel`.

### Bench routes: rearm, gated move and a host campaign ledger (closing audit of packets 19, 20, 22)

- Add the `rearm` route (`TR`) to `examples/s2_bench/BenchRoute`: exactly one `6040=000F` through the session after the acknowledged `001F` of a finished `position` record on that node, then fresh cycles that are recorded and never judged. A missing, uncertain or other latest word refuses with `control`, so a lost `000F` or `001F` can never be followed by another. It is project policy until the mode 1 falling-edge behavior is measured.
- Add the `move` route (`TG`) over `AxisSession::requestMove`: a fresh identity and full read, the `position` check, then the move with `MoveEvidence` built only from the host's line (record, acceptance and completion signals, rule, raw identity). The session runs it to its own terminal and rearm; an optional named stop after a stated number of move cycles uses the `stop` path with the move's last complete cycle. Abort and EMCY end it through `cancelMove`. Relative moves are not routed.
- Both are motion admissions under the per-boot caps; a planned stop in `move` also counts against `stop`. New summaries `TRIAL_REARM`, `TRIAL_EVIDENCE`, `TRIAL_MOVE` and `TRIAL_MOVE_SAMPLE`; PONG reports `routes=2`. The request buffer is `ROUTE_LINE` (256 bytes), and longer text is refused whole. The parser keeps its field tables in flash.
- **Migration:** `scripts/s2_bench.py` requires `--campaign PATH` for every motion route. It appends a flushed `send` entry before each motion line and a `result` entry after, and refuses across boots at twelve motion admissions, three per route and node, or a reused trial identity. A missing ledger needs `--new-campaign`; a damaged one is refused and never repaired.
- `test/bench_route_test.cpp` now fails `position` at each of `607A`, `6081`, `6083`, `6084` and the `001F` by server abort and by silence, with a mismatched and a late reply, a boot-up, the route cutoff and an EMCY during the cycles, and records missing acceptance and stale completion bits without a claim. It runs `rearm` and `move` through refusals, lost replies, the cutoff, cap exhaustion, a named stop, abort and EMCY. No specimen has run either route.

### SDO service qualification from receive bounds

- **Contract change:** a checked SDO reply received with bounds (`SdoClient::receive(frame, earliestUs, latestUs, nowUs)`) now records service evidence, and so `ServiceEligibility::ordinarySdo`, under the same rule as an exact capture. Its whole interval must be after every boundary: `earliestUs` strictly after the request's handoff, after the node boundary (strictly after an invalidation) and, at eligibility, strictly after a possible NMT transition. `earliestUs` orders it against retained evidence and sets its age, so an interval is never fresher than its oldest possible receive time and a late-delivered older reply cannot replace newer evidence. Before this, an interval-only reply settled its request but never qualified ordinary work, so on the S2 fixture writes, enable words and the sequenced stop were refused `INVALID` without a heartbeat.
- An interval that reaches back to its handoff, straddles a boundary, is inverted or lies in the future records nothing. Exact capture, unknown time, SDO matching, quarantine, settlement and NMT handling are unchanged. An exact capture equal to the handoff still records, because equal timestamps cannot order.
- **Migration:** `NodeSnapshot::sdo` and `serviceQualification` can now carry `receiveBoundsKnown`; read them through `observationTimeBounds`. A bounds reply that does not qualify no longer leaves a timeless `sdo` record.
- Decided from the 2026-11-03 S2 soaks (`docs/bench/20261103-soak-night2`, `docs/bench/20261103-soak-stress1`). Bounds are host processing windows, not wire time; see the [TWAI receive-time note](docs/adapters/twai_receive_time.md#sdo-service-qualification). Adds `test/bounded_service_test.cpp` and regressions in `test/network_receive_time_test.cpp`.

### DI state rule and core audit fixes (closing audit, A-10, A-15, A-16, A-18, A-19, G-13)

- **Migration:** `prepareIoSettings` takes a `const ReadContext& state` after `prior`: a clean STATUS and MONITORS read under the reservation, within `maximumAgeUs`. It refuses with `ReadError::STATE` a failed or uncertain read, a missing block, a block older than the reservation or at another target, and any `2100` running, alarm or undocumented bit 7..15 report, as `prepareSoftLimitSettings` and `prepareControlSettings` do. An unchanged candidate is refused the same way, so it cannot report success with no frame while the drive runs. Acquire the state read after the reservation and pass it.
- This is project policy, adopted from MotorControl-RS `validatePriorSettingsAndStandstill`, which requires it for every settings group, I/O included. No source gives a state for `2400`, and a reassignment can change enable, stop and limit inputs at once. A clean report is not evidence of a stopped shaft. An alarm refuses every candidate, so an input cannot be reassigned to get past one. `test/settings_owner_test.cpp` covers it through the real owner.
- `pdoFreshness` moves to `src/canopen/PdoState.cpp`, beside its `PdoState.h` declaration, with no behavior change. A consumer listing core sources by hand must add that file.
- The umbrella `MotorControlCAN.h` now includes every installed core header: `ReadOperation.h`, `ActionOperation.h`, `MoveOperation.h`, `canopen/SdoTransfer.h`, `canopen/Network.h`, `canopen/Pdo.h` and `canopen/PdoState.h` join it. Profile headers stay separate.
- A PDO transmission type outside 0-240, 254 and 255 is still refused, now as `PDO transmission type outside 0-240, 254, 255`. The message no longer calls 241-253 reserved, as no CiA 301 copy is retained to classify them.
- No behavior change: `gcd` and `magnitude` come from one private `src/ExactArithmetic.h`, and settings readback steps use a named offset. `Axis.h` no longer says `configureAxis` is the only epoch writer, and `CanBusOwner.h` states the `now >= deadlineUs` expiry rule.
- The installed consumer also runs `BestEffortStop`, `prepareEscalation` and `validateEscalation`, `NativeRateBasis` through `prepareVelocityTarget`, `pdoFreshness` and the DI state refusal.

### Gated finite relative move and the combined interruption matrix (packet 21)

- Add `prepareMoveRelative` and `validateMoveRelative`, on the same `nextMove`, `beginMoveObservation`, `noteMoveTransmission`, `noteMoveWithdrawal`, `advanceMove` and `MoveContext` as the absolute move: `607A` carries the quantized displacement and the edge is `000F` to `005F`. Never converted to an absolute target, and `007F` immediate update returns `UNSUPPORTED`. **It is gated twice and supported for no specimen:** add `RelativeBase` and `MoveEvidence::relativeBase` (default `UNMEASURED`), the measured base the drive adds a displacement to. A request whose basis it does not name returns `UNSUPPORTED` with the new `MoveError::RELATIVE_BASIS`; a `QUEUED` basis always does, since this lifecycle starts only from an idle drive; bit 5 = 0's deferred update is documented, but with no capacity, acknowledgement or base. See the [move guide](docs/position_parameters.md).
- A relative witness is required: known, idle, stationary, fresh, of the request's basis, with a nonzero age cap. Zero effective displacement is refused before any write (R-08). The displacement must fit `607A` requested and quantized, and both endpoints the axis limits.
- **Contract change:** `noteMoveTransmission` takes the caller's current `AxisConfig` and witness after `current`, and rechecks every parameter and trigger write at its actual admission: `validatePreparedTarget`, a still idle and stationary witness, and the admission read's age. A later `configureAxis`, origin change, `invalidateAxisReference` or changed witness refuses the next write. This applies to absolute moves too; a target with no witness survives a `POSITION` loss.
- **Contract change:** a supplied witness, absolute or relative, must be observed at or after the predecessor `000F` completed, so a witness from before an earlier move or enable is refused with `MoveError::REFERENCE`.
- `AxisSession` gains `requestMoveRelative`. `requestMove` and `requestMoveRelative` borrow the axis and witness until the move is terminal. Withdrawing a move now closes a download the owner has not yet handed to the controller, so a queued parameter or trigger no longer leaves after a cancel, fault or stop request; work that may have left still settles. A `CANCELLED` owner terminal the session did not request, as after `invalidate`, ends the move `INVALIDATED`. `MoveContext` is 3,008 bytes.
- **Review fixes (D1, D2, W1, W2).** Every `AxisSession` member that takes a time refuses one behind the session's or the owner's last time, with nothing changed; a refused stop still withdraws an active enable or move, at the owner's time, so a queued trigger no longer leaves after a stop request. Add `AxisSession::preflight`: it rechecks a move's parameter and trigger downloads when the owner hands them over, and a refusal closes the download unsent with `PREFLIGHT_REJECTED` and ends the move `INVALIDATED`. **Applications must call it from `Transport::beforeTransmit`**; without that, the recheck holds only up to admission. `AxisSession::moveRefusal` keeps why the recheck refused. A download closed provably unsent leaves the previous acknowledged controlword as the latest, so a later stop keeps its sequenced path. `AxisSession` is 23,576 bytes on the S2 and S3 builds.
- `examples/common/FirmwareProbe` gives the owner a forwarding transport whose `beforeTransmit` calls a registered `DownloadPreflight`, and the S2 bench routes register one that asks each attached session's `preflight`. `BenchRoute` aborts at the later of its own and the owner's time.
- Add `test/move_relative_test.cpp`: the matrix of mode confirmation, each parameter, pre-trigger, possible trigger (queued and handed off), protocol confirmation, acceptance, completion and rearm against deadline, cancel, fault, boot, rebind, missing and stale feedback and a real stop, plus queued targets after a stop, poll settlement, urgent stops on two nodes, an ambiguous late ACK, best-effort stop, explicit recovery, repeated values, partial setup, cancellation around handoff, timeout then new motion, a stale completion bit, a completion with a different reference, the rearm and the processing cutoff. Add `test/move_common_test.cpp` (a servo with no full steps, a non-CANopen fixture, and intent vectors matching MotorControl-RS `a13f767`), and `test/support/MoveRig.h`, now shared with `test/move_absolute_test.cpp`. `examples/position_preview` shows the relative refusal; it runs no relative simulation.
- `scripts/check_stack.py` measures `MoveContext`.

### Gated finite absolute move (packet 20)

- Add common `MoveOperation.h` with `MoveRequest` (position and speed intent), `MoveDisplacement`, `MoveInterruption` and `MoveError`, and in `profiles/lichuan_c/Position.h` `prepareMoveAbsolute`, `validateMoveAbsolute`, `nextMove`, `beginMoveObservation`, `noteMoveTransmission`, `noteMoveWithdrawal`, `advanceMove` (terminal and local overloads), `resetMove`, `MoveContext`, `MoveEvidence`, `MoveProfile`, `MoveOptions` and `PreparedMove`. The terminal context is the result. **It is gated: acceptance and completion are not established for any specimen, the default `MoveEvidence` returns `UNSUPPORTED`, and only fake-owner tests have run it.** See the [move guide](docs/position_parameters.md).
- Phase order per D-01: the caller's fresh state read after the actual latest acknowledged `000F` (mode 1, low bits 111, `2100` idle), then `607A`, `6081`, `6083`, `6084`, then `000F` to `001F`, held until the move is terminal (DEC-8), then fresh observation cycles, then `000F` only after an observed completion. Never `6060`, `6061`, an enable, a fault clear or a stop. Immediate update returns `UNSUPPORTED`.
- The speed goes through `prepareVelocityTarget` and is refused unless it comes out integer motor turns per minute: 1 load turn/min at 3,200 increments per turn and gear 5 is 267 under the default basis, and that is refused before any write. Zero ramps, zero speed and a zero known displacement (R-08) are refused before any write; an absolute zero with no witness is an ordinary target.
- Each parameter download calls `retireMotionParameters` on every retained context before it may transmit, and does not transmit if one refuses. The configuration epoch does not move, so the move's own target stays valid while a later settings call needs a fresh read.
- `AxisSession` gains `requestMove`, `cancelMove`, `retainSettings`, `moveAction`, `moveActive`, `moveObservation` and `moveFenced`. **Migration:** `busy()` is also true during a move, `settle` returns false while a move owns the request, and `requestStop` ends an active move first. Observation cycles alternate between two session-owned reads, so the last complete cycle can serve a sequenced stop from the held `001F`. After a trigger that may have reached the drive with no observed completion, an aborted one included, new moves are refused for the object's life. A read reused under `REUSE_STORED` must be named with `retainSettings`. `AxisSession` is now 23,536 bytes on the S2 and S3 builds, of which the move context and its two reads are about 7 KB; no firmware holds one.
- Add `test/move_absolute_test.cpp`, and the gated sequence to `examples/position_preview` with a labelled host simulation.

### Device counter zero: baseline and held value 1 (packet 18)

- **Migration:** `prepareSetDevicePosition` needs a fresh FEEDBACK block in its state read as well as MONITORS and STATUS, and refuses with `ActionError::STATE` unless `606C` reads raw zero. This is project policy beside `2100` running clear, not an observed shaft. Add `StateBlock::FEEDBACK` to the prerequisite read. Nonzero requests still return `UNSUPPORTED` before any other check.
- Add `CounterBaseline` and `ActionContext::baseline`: the admission `6064` and `606C` raw, kept after the cycles replace `extraObservation`, which now holds the admission FEEDBACK until the first cycle. A raw zero after an already-zero baseline shows no change at all.
- Add `ActionContext::valueOneMayRemain`, also used by the alarm clear: set when the action fails after its value-1 write may have reached the drive and the `0` after it has no positive acknowledgement. Nothing clears it or resends a value.
- No wire value, write order, withdrawal point or epoch rule changed. `test/counter_clear_test.cpp` fails each of `0`, `1`, `0` before admission, in the queue, in the controller, after a TX report, on an abort, after an ACK processed at the cutoff and on a reply at the cutoff, and follows each into later reads, target validity and owner admission. See the [guide](docs/fault_reference_actions.md).

### Alarm clear: unresolved fault identity (packet 17)

- Add `AlarmCodeLink` and `AlarmIncident::codeLink`, always `UNRESOLVED` in an alarm clear: `603F=FF05` beside a reported alarm narrows the attempt but does not identify the alarm, since `603F` may keep an earlier code (D-26). The refusal text no longer calls the alarm a documented resettable `FF05`.
- The `2301` writes, the polling of MONITORS and STATUS only, and the single final ERRORS read are unchanged. A failure after a possible `1` without an acknowledged `0` sets `valueOneMayRemain` (D-27).
- Add `test/alarm_clear_test.cpp` and the shared `test/support/ReferenceRig.h`: admission per code and state, a stale `FF05` with a later incident, a persisting alarm, latched error values, the controlword route, every write at every failure stage, a delayed final ACK, boot-up, EMCY during clearing, two-node isolation, cancellation and deadline races, and stale configuration. Each checks the exact frames: no enable, controlword, motion, save, restore or retry.

### Gated enable through the owner; release stays absent (packet 15)

- Add `prepareEnable` and `validateEnable` with `EnableEvidence`, `EnableGate`, `EnableStart` and `ENABLE_WORD_INTERVAL_US` in `Actions.h`, running the documented position-mode words 6, 7, 15 through `nextAction`, `noteActionTransmission` and `advanceAction`. **It is gated: no specimen has run it, the default gate returns `UNSUPPORTED`, and even success is a drive report rather than torque.** See the [transition table](docs/drive_actions.md).
- Refuses mode 3 and every other mode but 1. It never zeroes a target, changes mode, clears a fault, replays a word or issues a stop. `607A` and `60FF` are read once before any word and must equal the reviewed values, zero included, or no word is sent. `2100` decides alarm and running; disputed `6041` bits are compared, never interpreted.
- Add `AxisSession::requestEnable`, `cancelEnable`, `enableAction`, `enableActive` and `motionHistoryWritten`. Its words pass through the session's one path, so an acknowledged `000F` is the latest controlword a stop may follow. The session refuses an enable once it has admitted any write other than an acknowledged `6060=1`, for the object's whole life.
- **Migration:** `AxisSession::busy()` is now also true during an enable, `settle` returns false while the enable owns the request, and `requestStop` ends an active enable even when it refuses the stop. `AxisSession` is one `ActionContext` larger. `ActionKind::ENABLE`, `ActionPhase::TARGETS`, `ActionOptions::commandIntervalUs`, `ActionContext::enable` and `STATUS_LOW_BITS` are additions, with earlier numeric values unmoved.
- Add `examples/action_preview`, a direct consumer of these calls with no console, and `test/enable_action_test.cpp`.
- `prepareRelease` is deliberately absent: `6040=0`, low status bits zero and DI function 4 do not establish a release command. The [guide](docs/drive_actions.md) states what would.
- `EnableEvidence` is bound to one attended trial: a nonzero `trial`, the exact `reservation`, the exact `operation` and `expiresUs`. `AxisSession` consumes each trial once, in rising order, and adds `nextOperation`. A start report of low bits 111 with `2100` released clear, the manual's enabled pattern and the specimens' power-up report, needs `powerUpShaft` (`ShaftObservation`), because word 6 may then drop holding torque.
- The deadline must exceed three intervals plus `ENABLE_SERVICE_STEPS` (26) queue-and-response budgets. That admits only a sequence that can finish when every service answers in budget; a fault or a late report can still leave a partial state.
- Add `noteActionWithdrawal` and `ActionContext::withdrawal`: a withdrawal with work in flight keeps that terminal as evidence, and the outcome is the withdrawal's reason, never success. `AxisSession::cancelEnable` and `requestStop` use it.
- The ledger records one `607A` and one `60FF` upload per enable as an accepted action prerequisite (`read_disposition` `accepted_action_prerequisite`), not a poll. `OBJECT_607A_00.readEffect` becomes `ACCEPTED_UNQUALIFIED`, and the enable's uploads carry that classification. Effects stay unknown.
- **Migration:** `AxisSession::requestStop` no longer refuses after a known word with no stop arrow when there is no fresh idle report: a fresh running report, or none, now takes the best-effort `000B`, as the same word in flight already did. It also ends an active enable when time moved backward.

### Loop selector through the owner; tuning gates per subgroup (packet 14)

- **Migration:** `prepareControlSettings` takes a `const ReadContext& state` after `prior`: a clean STATUS and MONITORS read under the reservation. It refuses a failed or uncertain read and any `2100` running, alarm or undocumented bit 7..15 report, with `ReadError::STATE`, as `prepareSoftLimitSettings` does. This is project policy, since no source gives a state for `2500`, and it is not evidence of a stopped shaft. Acquire the state read after the reservation and pass it.
- **Migration:** `CLOSED_LOOP` is refused with `ReadError::VALUE` while the prior `2501` reads zero. Also project policy. Leaving closed loop is unaffected.
- `test/settings_owner_test.cpp` now runs the `2500` write through the real `CanBusOwner`: literal frames, no-op silence, refusals, lost and failed downloads and readback, cancellation, deadlines, boot-up, bus-off, notice before TX and retirement before epoch migration. 21 hand mutants of the control logic are killed.
- **The `2500` write may act on an energized drive.** Both specimens report `6041=0x8037` and `2100` bit 4 clear with no enable written, and OP86 p5 says the motor is shaft-locked after power-up, so closed to open may snap the rotor or change current. Unlike RS, which needs effects and current-tuple qualification for every control write, this write has no such gate: an attended bench trial is its only evidence gate, and none has run. The manual calls selector 0 noneffective; whether it removes torque is unknown, so it is not a disable, release or stop to rely on.
- `2501..2520` keep their gates with no typed write. Each ledger row now carries `review.next_evidence`, and the checker refuses a Prompt 22 row without it; [the tuning guide](docs/tuning.md#decisions-per-subgroup) gives each subgroup's sources and closing evidence. The ledger keeps the manuals' "locking" label for `2506/2507`, states both `2500` policy rules, and cites every page its prose names, which a test enforces. The guide records that only the product page's Features list calls the 8 A ceiling peak; its spec table does not, and its supply ranges belong to another revision.

### DI proof, per-field output gates and the review of packet 12 (packet 13)

- **Migration:** `prepareDriverSettings`, `prepareSoftLimitSettings`, `prepareIoSettings` and `prepareControlSettings` take the actual current `ReadTarget` after the reservation, and refuse with `ReadError::TARGET` when the reservation's target differs. A frozen token and its old evidence could prepare a no-op reporting `SUCCEEDED` with no frame while the drive held another value, including after a lost reply. Take a new reservation value and fresh evidence.
- **Migration:** remove `prepareDirectionSettings`, `DirectionSettings` and `DefaultDirection`, added earlier in this release. EN86 and ZH86 p18 give `2200` two labels and do not say what the codes do under CAN, so it is a gate with a stated closing observation. `getDriver` still reads it.
- **Migration:** `prepareSoftLimitSettings` refuses `AFTER_HOMING` while `2100` reports homing complete, and refuses on any undocumented `2100` bit 7..15. Bit 4 stays deliberately unused. Ordered `607D` for arming is labelled project policy, and disarming needs no order.
- **Migration:** settings readback now compares the DI group's `2400:00` count too, so a changed count fails with `REPLY_ERROR`. An active DI context refuses a hand-edited code above 13 or word above 16 bits.
- DO polarity and functions, DO codes 2 and 3, `60FE` terminal bits and value/mask semantics, and `2401..2407` keep gates with no typed write; [the I/O guide](docs/io_settings.md#per-field-decisions-for-do-filters-and-output-commands) gives each field's sources and closing evidence.
- `2202` and `607D` cannot close each other at the bench; [the driver guide](docs/driver_settings.md#setting-decisions) says a manufacturer range or coordinate basis is needed first.

### Soft-limit selector, stricter state evidence (packet 12)

- Add `prepareSoftLimitSettings` with `SoftLimitSettings` and `SoftLimitEnable`, a typed write of `2202` codes 0 and 1. They use `DriverSettingsContext`, `nextDriver`, `noteDriverTransmission`, `advanceDriver` and `getDriver`, advance the configuration generation like other explicit settings, and retain and recheck `607D` without writing it. A match proves the stored code only: no physical direction, activation or protection is claimed.
- **Migration:** `prepareDriverSettings` now refuses a failed or uncertain state read even when its STATUS and MONITORS blocks completed, as `prepareStop` does. Take a fresh state read. `getStateBlock` still returns those blocks as history.
- Add `test/settings_owner_test.cpp`, which runs explicit settings through the real `CanBusOwner`.
- `2201`, `607D` and `6060` keep their gates with no typed write; [the guide](docs/driver_settings.md#setting-decisions) gives each one's closing observation.

### S2 bench tools, read-only soak and parser fix (packet 11)

- Add `scripts/s2_flash.py`: it builds the S2 bench image with the pinned tools, requires the recorded USB identity and chip MAC, checks the generated flash arguments and the embedded ELF hash, and retains hashes and logs. `--dry-run` prints the plan only. Identity after a flash comes from `ping` with `--check-ping`.
- Add `scripts/s2_evidence.py`, an offline parser with one record-grammar table. It replays retained captures, extracts a transition log, and preserves and counts unknown lines.
- Add a soak mode in `examples/s2_bench/Soak.h` and `Soak.cpp`, run by `scripts/s2_soak.py`. It uses the ordinary owner and typed reads, one operation at a time on alternating nodes, and reads only accepted objects: `6041`, `6061`, `6064`, `606C`, `2100`, `2101`, `2102` and one identity read per node. It never retries, reopens a route or restarts; an anomaly stops that node's reads and keeps its evidence. It allocates 10,752 bytes of internal heap on first use and never frees them. **A soak is communication evidence on the specimens and image it ran on, not electrical, load, per-family or motion qualification.**
- Add `IdfTwaiDriver::errorCounters(transmit, receive)`, which reads the controller's TX and RX error counters through the SDK for diagnostics. It returns false and leaves both outputs unchanged when no node exists or the SDK call fails. Call it at a diagnostic rate, not per service turn.
- Bench records now separate queue admission, adapter handoff, SDK submission, TX report, receive bounds, terminal and retirement. `END network` is no longer dropped under console pressure, and `TRAFFIC`, `RESULT`, `STATS` and `PONG` carry controller state, epochs and time. Bench identity reads select `IdentityWidths::MEASURED_U8`.
- **Fixed:** the parser read an `EVIDENCE` header's `outcome` with the SDO outcome table. The header reports `ReadOutcome`, where 1 is success, so the first live typed success was marked inconsistent and then failed. A regression on the retained live captures covers it.
- **Migration:** `firmwareStackFreeBytes` is removed from `examples/common/EspProbeMain.h`. The S2 bench reads the owner task's high-water mark itself.
- Add the `s2_flash_contract`, `s2_evidence_contract`, `soak`, `soak_transcript`, `s2_soak_contract` and `s2_soak_transcript` tests. None opens a port. The live baseline and soak results are in the [packet 11 report](docs/reports/handoff_19_33/packet_11.md).

### Service timeouts and withdrawn blocks (audit A-07, A-08, packet 10)

- Add `ReadOutcome::SERVICE_TIMEOUT` and `ActionOutcome::SERVICE_TIMEOUT`. **Migration:** an SDO `TIMEOUT` that completes before the overall deadline now reports this instead of `DEADLINE`, in reads, settings, PDO configuration, actions and best-effort stops. `DEADLINE` now always means the immutable overall cutoff. Code that treated `DEADLINE` as "the reply was lost" must handle both. Whether a frame could have left stays in `transmissionPossible`, `execution` and `uncertain`: this outcome does not imply it. The new values are appended, so earlier numeric values do not move.
- **Migration:** an SDO `CORRELATION_LOST` terminal now withdraws earlier completed state blocks, as `BUS_OFF` does. `getStateBlock` refuses them with `ReadError::TARGET` even though their raw values, provenance and the terminal stay retained in the context.
- `ReadContext::invalidated` is never cleared by a later terminal on the same context.
- `pdoTemplate` refusals now carry a static message. They returned a null `Status::msg`, which is undefined when printed with `%s`.

### Status decoding and monitor bits (audit A-03, packet 10)

- Add `interpretStatusword` and `StatusIndications`, a value function needing no read context, so a STATUS block or a qualified TPDO sample can be decoded without copying masks. `getStateInterpretation` now uses it and is otherwise unchanged. A decoded word is not admission or completion evidence by itself: each caller proves its own route, freshness and firmware applicability.
- Add the reviewed object 2100 masks, `MONITOR_POSITION_COMPLETE` through `MONITOR_NEGATIVE_SOFT_LIMIT` and `MONITOR_DOCUMENTED_BITS`, in `State.h`.

### Move parameter writes and settings (audit A-02, packet 10)

- Add `retireMotionParameters`. A future move calls it for each retained `MOTION_PARAMETERS` context, before any of its own `6081`, `6083` or `6084` downloads may transmit. `607A` is accepted and retires nothing, and the configuration generation never moves, so a move's own target stays valid. A later settings call then needs a fresh read and cannot report success on old values. Explicit settings calls still advance the generation. No caller exists until the move lifecycle does.

### Shared private evidence checks (audit A-04, A-10, A-12, packet 10)

- Terminal, work and overlap checks move to private `src/canopen/SdoEvidence.h` and `src/profiles/lichuan_c/ProfileEvidence.h`. Read group plans move to one table in `src/profiles/lichuan_c/ReadPlan.h` and `DriverReads.cpp`, and control and tuning reads to `Tuning.cpp`. Package source lists gain these files; a consumer listing core sources by hand must add `DriverReads.cpp` and `Tuning.cpp`.
- An SDO terminal must now also match its work's `requirePreOperational`. An empty range no longer counts as overlapping anything; no current caller passes one. Typed group getters now use one set of refusal messages, and identity and configuration getters also check the read's source. Details are unchanged.
- The settings and stop control evidence checks share one helper and keep their different predicates: settings needs `000F`, a bounded age and a nonrunning report, while a stop works on a running drive with its DEC-8 predecessors and no age limit.

### Axis reservation name (audit A-04, packet 10)

- **Migration:** rename `LICHUAN_C::SettingsReservation` to `LICHUAN_C::AxisReservation`, with the same fields and the same header, `Settings.h`. There is no alias, so code using the old name stops compiling. Settings, stop, action and PDO configuration calls all take it. It means actual continuous exclusion of one axis by the sole owner, not a permission a profile can construct. Behavior is unchanged; one status message now says `axis reservation` where it said `settings reservation`.

### Native rate basis and soft-limit recovery (audit W8, packet 9)

- Add `NativeRateBasis`, `AxisConfig::nativeRateBasis`, and the frozen `PreparedVelocityTarget::rateBasis` and `rateTime`. Rates quantize once, in the destination unit. **Migration:** the default `COMMAND_INCREMENTS` keeps old behavior, so a Lichuan rpm caller must set `MOTOR_TURNS` with `MINUTE` and check both before writing. A wrong basis gives an in-range wrong value, and nothing in the library refuses it.
- Wrapped requests follow `supportedRelativeBases`, as relative ones do. **Migration:** the default of 0 rejects them.
- A fresh native-known witness outside the soft limits may recover toward the range when both endpoints land inside (DEC-6). This deliberately differs from RS.
- `configureAxis` treats a rate basis or time change like other rate settings.

### Action stack and context size (audit W7)

- Action, read and settings calls validate first and then write the caller's context in place. No call copies a context onto the stack, and a refused call still changes nothing. Every frame is now under 2,048 bytes and every known chain under 6,144 on both targets at `-Os` and `-Og`; those totals stay lower bounds where the graph is incomplete. `scripts/check_stack.py` runs with `--strict` in CI and the baseline is empty.
- `ActionContext` shrinks from 7,504 to 4,880 bytes on the pinned Xtensa compiler, and `AxisSession` from 16,152 to 10,904.
- Add `validateStop`, `validateEscalation`, `validateRead` and `validateStateBlock`, which report what the matching call would refuse and change nothing, plus `resetAction`, `resetRead` and `CanBusOwner::canRelease`. Reset with these: `context = ActionContext()` builds the whole temporary on the stack.
- **Migration:** `ActionContext::originalState` is replaced by `AlarmIncident incident`, set for an alarm clear only. It keeps the raw prerequisite values and one earliest/latest bound, not each field's provenance, which is a contract change. A counter clear no longer retains its prerequisite read, and a stop never did. An application that needs the full read keeps its own copy.
- **Migration:** `errorObservation` and `feedbackObservation` merge into `extraObservation`, selected by its `block`: raw ERRORS for an alarm clear, FEEDBACK for a counter clear, unused by a stop.
- **Migration:** `commandEvidence` and `commandEvidencePresent` are removed from `ActionContext`. Read `commandHistory`: the effecting command is slot 0 for a stop or alarm clear and slot 1 for a counter clear, present when its `commandEvidenceMask` bit is set. `BestEffortStop` keeps its own fields.
- **Migration:** action options with `identityWidths` other than `DECLARED_U16` are refused at admission by `stopOptionsValid`, `prepareStop`, `prepareEscalation`, `prepareClearAlarm` and `prepareSetDevicePosition`. They were accepted, then refused at the first observation after the command had been acknowledged.
- `AxisSession::requestStop` validates before the owner moves and writes the stop's context only afterward. A state read held inside the `stopAction(kind)` this request overwrites counts as missing, so the request takes the unsequenced branch.

### Usable stop lifecycle (audit W6)

- `prepareStop` accepts the DEC-8 mode 1 predecessors. `ActionContext` gains `predecessorWord`, `predecessorDocumented`, `predecessorBasis`. Undocumented rows are project decisions. An acknowledged controlword no longer expires by age.
- Add `BestEffortStop` with `prepareBestEffortStop`, `nextBestEffortStop`, `noteBestEffortAdmission`, `advanceBestEffortStop`, and `ActionOutcome::STOP_SENT_UNCONFIRMED`. It yields only `000B` and never succeeds.
- Add `prepareEscalation`, `stopOptionsValid`, `stopPredecessorAllowed`.
- Add `AxisSession`, caller-owned and borrowed through `ProbeApp::openAxis`, `axis`, holding the latest controlword per axis and the pending-stop policy. Add `CanBusOwner::nodeEpochs`, `recoveryActive` and `NodeSnapshot::serverSessions`.
- **Migration:** `maximumAgeUs` must now cover the field count times queue plus response budgets, or preparation rejects it. `CLEAR_ALARM` reads ERRORS once after the alarm clears, not each cycle, so its cycle has five fields, then two. Consumers copying `examples/common` also need `AxisSession.cpp`.

### Per-axis urgent work and best-effort stop route (audit W6, owner half)

- `CanBusOwner::URGENT_REQUESTS` is now one slot per channel, so one axis's unread urgent result no longer blocks another's. **Migration:** `TOTAL_REQUESTS` grows by one and `resources()` reports the new capacity. Code that assumed one urgent slot must not.
- Add `CanBusOwner::beginUrgentOperation` and `findSequence`. Unsubmitted old work ends with explicit no-TX results; in-flight work settles. `beginSequence` reuses the slot of an invalidated reservation with nothing live, and retained results stay readable.
- Add `CanBusOwner::admitBestEffortStop`, `SdoClient::checkBestEffortStop`, `admitBestEffortStop` and `SdoOutcome::UNCONFIRMED_SENT` (DEC-2). Only `6040:00=000B` passes. **Migration:** switches over `SdoOutcome` need the new value. It reports that a frame left the controller, not that a drive stopped. The profile action arrives in packet 7.

### SDO settlement and drain marker (audit W5)

- A matched server abort on a clean exchange no longer quarantines its route. It ends queued steps of its sequence and keeps earlier effects uncertain.
- Bus-off and recovery quarantine only routes with a frame that may have left. Boot-up and rebinding still quarantine.
- Add the explicit drain marker: `CanBusOwner::admitDrainMarker`, `SdoDrainPremises`, `SdoClient::checkDrain`, `admitDrain`, `reopenAfterDrain`. No real fixture can use it yet: remote server ordering is unestablished (Q18).
- Evidence loss lets a frame already on its way settle, then withdraws only unstarted work (C-06). `TRACE_CAPACITY` is 24. Add `CanBusOwner::mayHaveTransmitted`.

### Accepted reads and identity widths (audit W4)

- Add `ReadEffect::ACCEPTED_UNQUALIFIED` and `SdoReadEffect::ACCEPTED_UNQUALIFIED` for the owner's DEC-1 list, driven by the ledger's `read_disposition`. This records risk acceptance, not evidence: `effectPossible` stays true.
- `prepareProbe` on `1000:00` now works. A first accepted upload can confirm a channel without claiming freshness. `DeviceEvidence` admits a plan only when every object is accepted. `603F`, `1001`, `1003` and all unlisted objects keep their gates.
- Add `ReadOptions::identityWidths`. `MEASURED_U8` reads `1009` and `100A` as the one byte two specimens returned; nothing selects it automatically, and the default stays the EDS U16. `IdentityObservation` reports the widths.
- Rename `ServiceEligibility::nonChangingProbe` to `probeUpload`.

### TWAI transport (audit W3)

- `TwaiDriver::submit` returns `TwaiSubmit` instead of `bool`: SENT, DEFERRED, FAILED_UNSENT or FAILED_UNCERTAIN. A receive interrupt during promotion now defers instead of quarantining the route.
- Replace `TwaiAdapter::service` with `pollEvents` before `owner.service` and `promote` after it.
- The adapter holds a queued handoff, so its possible effect and response clock start at actual SDK submission. A frame proved unsent ends without quarantine: `TxReport::provedUnsent`, `SdoClient::holdHandoff`, `noteSubmitted`, `withdrawUnsent`, `withdrawNmtUnsent`.
- Set `fail_retry_cnt=-1` (DEC-4): hardware retransmits, so a no-ACK frame stays pending until an application-started recovery. See [the consequences](docs/adapters/twai_sdk.md).
- Accept a received Classical DLC of 9..15 as eight bytes. `FirmwareProbe::clockFailed()` becomes `progressionFailed()` plus `failure()`, which reports the real cause.

### Owner placement (audit W2)

- Move `CanBusOwner`, its diagnostics and counters from `examples/common/` to `adapters/owner/`, in namespace `MotorControlCANOwner`, with optional target `MotorControlCAN::CanBusOwner`. Runtime behavior is unchanged and the target is still not installed.
- Owner value types and the transport boundary are in `CanBusTypes.h`, included by `CanBusOwner.h`. Tests share plumbing through `test/support/OwnerRig.h`.
- Migration: include path `adapters/owner`; replace `MotorControlCANExample::CanBusOwner` and its owner types with `MotorControlCANOwner::`; link `MotorControlCAN::CanBusOwner` instead of `MotorControlCANExampleOwner`. `ProbeApp`, consoles and board code stay in `examples/` under `MotorControlCANExample`.

### Promptset repair (audit W1)

- Remove prompt 18 from prompt 38's prerequisites, make prompt 43 depend on 40, 44 on 42 and 43, and 47 on 29, 44 and 45, so prompts 49-50 can close as a scoped software candidate. New checker guards keep 18 out of 49's ancestors and 41/42 in the ancestors of 44, 45, 47 and 48, and reject mis-decoded UTF-8.
- Adopt the usable-prerequisite rule and an Inherited gates column. Move progress narrative to `docs/reports/progress_history.md`; prompts 23, 25 and 26 become Complete with their gates listed, and no gate closes.
- Reassign 12 read rows from prompt 38 to the prompts that write them, leaving 89 with 38. `607C` moves to prompt 36 for both read and write ownership. The generated `Objects.h` descriptor changes its owner number only.
- Correct the reading of `607A=5000` and `60FF=120`: they equal the EDS defaults, and no retained record shows they were written, so "retained intent" was unsupported. Device conflicts and coverage now carry the November 2 measurements.
- Replace "exclusive" and "inclusive" deadline wording with `now >= deadlineUs` expires. Behavior is unchanged.

### External coder continuation

- Add 22 paste-ready handoff packets for prompts19–22 and29–33, including the
  required architecture repairs, concrete source/test routes, bounded HIL
  diagnostics and deliberate failure reproduction/retesting. Preserve current
  implementation and firmware gates; this documentation performs no hardware test.

### Fixed TPDO1, explicit NMT and honest receive bounds

- Add the fixed reviewed TPDO1 configuration lifecycle, retained per-step effects,
  exact complete readback and same-owner experimental caller; no retry/rollback.
- Add tagged NMT transport and conservative S2/S3 receive intervals without
  claiming exact arrival timestamps or weakening SDO/PDO deadlines.
- Retain node1 heartbeat1000/readback, fresh addressed Pre-operational and all
  24 fixed-map services. Final map2 is confirmed; broader firmware and sample
  qualification remain open. See the [checkpoint](docs/reports/2026-11-02_nmt_pdo_checkpoint.md).
- Retain both boot-up frames after a user power cycle and fresh readback of
  node1 producer0/map3. This is one-cycle reversion, not a persistence guarantee.
  Add finite 30-field readiness uploads per node, preserving nonzero retained
  targets, mode0 and configured input functions without motion or reassignment.
- Keep prompt18 Partial and prompt33 unfinished. No motion/start/reset/save or
  restore was performed. Final integration verification is recorded separately.

### Earlier S2 connection and bounded raw evidence

- Add the pinned IDF 5.5.5 S2 UART/core-0/TWAI port and a separate OPTAK bench
  image with silent startup, explicit controller start and one finite upload-only
  experiment per node. No startup device request, retry or automatic recovery.
- Add exact one-use unknown-effect upload grants and bounded server-indicated
  raw counts without promoting service qualification or changing typed widths.
- Retain 20 successful raw uploads on each of nodes 1/2, 40 RX/40 TX in total,
  zero reported CAN errors/drops and 9852 bytes minimum remaining task stack.
  Both devices indicate one byte for `1009=0x0C` and `100A=0x20`, versus EDS U16.
- Correct the acquisition stack budget/preflight scratch and wait for the
  preceding controller TX report before granting the next raw field. Preserve
  failed attempts and the separate bounded post-terminal traffic tail.
- Add S2 compile/input checks and CI coverage. The [connection report](docs/reports/2026-11-02_s2_live_connection.md)
  separates observations from firmware meaning and qualification; no PDO mutation
  or motor action was performed at that earlier checkpoint; Prompt18 subsequently gains the subset above.

### Native position preparation

- Add pure `preparePositionParameters` with owned exact intent/axis/settings, native target limits, explicit quantization and current reference freshness.
- Keep relative displacement limits separate from endpoint limits; preserve exact and conservative approximate error evidence.
- Add standalone native usage and installed consumers. Finite move lifecycle and firmware qualification remain pending; preparation produces no service work.

### Stop and reference-action audit

- Add real-owner regressions for admission after failed or uncertain device
  counter clears, including retained results, coordinate epochs and attempted
  replacement of a quarantined reservation.
- Name the reviewed monitor masks and make the three action completion checks
  explicit. Wire values, public APIs and firmware gates are unchanged.

### Fault clear and device-position zero

- Add conditional `prepareClearAlarm` and zero-only `prepareSetDevicePosition`
  through the existing action lifecycle, with exact ordered native writes and
  fresh report-based completion. No enable, motion or factory-reset preamble.
- Preserve original errors, raw status and each command terminal, including an
  acknowledged trigger followed by a failed return-to-zero write.
- Withdraw host origins, references and prepared targets before possible counter
  trigger transmission; synchronize owner configuration before new-epoch reads.
- Keep controlword reset, other fault classes and physical home/rearm/internal
  resume semantics explicitly outside the implemented conditional scope.

### Conditional Lichuan stops

- Add finite `prepareStop/nextAction/advanceAction` for the documented position-mode
  `000F -> 010F/000B` transitions, with no enabling preamble or automatic replay.
- Retain command acknowledgement separately from fresh not-running reports,
  interrupted request results and uncertainty after cancellation or failure.
- Exercise real urgent owner admission, old-operation fencing, queue retirement,
  capacity, asynchronous events and channel quarantine with native fakes.
- Keep unsupported predecessor states, braking/torque/queue consequences and
  hardware qualification explicit in the stop guide and prompt 31 report.

### Documentary drive-state interpretation

- Strengthen checked-publication tests for output/current overlap and real
  boot-up invalidation while preserving historical observations.

- Add checked `getStateInterpretation` over existing typed state reads, preserving
  raw values, per-field provenance, unknown bits and status/monitor disagreements.
- Keep EN86, ZH86, ZH57 and OP86 interpretations explicitly separate. Selecting
  a source table does not qualify firmware or establish readiness/completion.
- Record the documented position-enable sequence and its missing admission
  provenance separately from the absent release command. No action stub, hidden
  enable or generic CiA402 fallback is installed.

### Connected S2 board intake

- Record ESP-Prog2 USB identity, duplicate COM15 assignments, inspected board
  power/debug/CAN net mappings and the user's reported DIP settings.
- Verify isolated portable-core builds for S2 and P4; retain the separate
  firmware-adapter, electrical and hardware qualification obligations.
- Resolve the COM15 mapping and verify UART/automatic boot entry with the actual
  S2FH4 ROM. Retain the failed manual-entry attempt; no flash or CAN test performed.

### Finite device evidence readiness

- Add a bounded acquisition collector around the ordinary typed application and
  CAN owner, shared by native and firmware examples. Record exact plans,
  raw traffic, all consumed terminals, controller events, timing and resources.
- Keep ordinary DEVICE acquisition behind Q20. Explicit reviewed unknown-effect
  read experiments retain their exact admission references without granting
  production permission or retrying an uncertain request.
- Interrupt on trace loss or reported RX overflow, distinguish missing ingress
  from captured rejected frames, and preserve prior successful fields on cancel.
- Add offline success/failure demonstrations, adversarial harness and firmware
  regressions, isolated consumers and strict documentation/build registration.
- Recheck original manuals/EDS, retain all conflicts and record user-reported
  fixture facts with precise conditional measurement gates. No live qualification.

### Independent TWAI and firmware audit

- Keep the final captured-RX/fault guard and SDK submission in one critical
  section; a captured boot-up cannot enter between validation and handoff.
- Invalidate backward adapter time using the owner's last accepted timestamp,
  retaining uncertain active effects.
- Bound GPIO numbers before the SDK's shifting macros and match host fixtures
  to the pinned ESP32-S3 pin mask.
- Share SDK virtualenv/source checks between compile gates, reject disabled
  dependency checks, and retain partial IDF diagnostics on failure without
  masking the original error. Add independent interleaving and failure regressions.

### Shared Arduino and ESP-IDF examples

- Add thin ESP32-S3 Arduino and native-IDF startup around the existing application,
  owner and pinned TWAI adapter. No selection or device request occurs at startup.
- Require explicit controller facts; default compile fixtures leave CAN disabled.
  Add a pinned internal owner task, bounded USB byte I/O and signed 64-bit time.
- Latch clock faults, invalidate using authoritative owner time and attempt local
  retirement once. Preserve uncertainty and retained results on failure.
- Add actual firmware and isolated portable-component build gates, compiler-input
  checks, host pressure/fault tests and CI. Core consumers remain SDK-free.
  Compilation and fake evidence do not qualify a physical board or drive.

### Pinned TWAI transport

- Add an application-only ESP32-S3 adapter for ESP-IDF 5.5.5's node API,
  with two owned TX slots, configurable bounded RX storage and exact TX callbacks.
- Retire the old controller before explicit recovery creates a fresh node;
  cancellation and restart never replay queued work or settle unknown effects.
- Keep queued RX ahead of TX promotion, preserve raw error flags and inhibit
  on observable RX loss. Record SDK overflow/timing limitations and source hashes.
- Register host policy and actual-shim callback tests plus a separate pinned SDK
  compile command. No firmware startup, flashing or physical qualification is added.

### Corrected passive PDO delivery history

- Retain checked delivery time for a qualified PDO whose capture is already
  expired. Its stale capture cannot establish READY or change service counters;
  revoked mapping samples and malformed input remain excluded.

### Native application examples

- Shared example-only ProbeApp/ProbePlatform now connect explicit typed reads,
  bounded input/output and retained owner completions. Native fake mode exercises
  the ordinary receive path with interleaved passive traffic and supplied time.
- The existing offline units preview demonstrates a rejected stale target and
  unchanged output. Isolated installed-core consumers build both examples;
  no SDK, sibling checkout or hardware is required. No motion or CLI is added.

### Passive owner health and statistics

- Add cached binding health with explicit timestamp validity, service totals,
  retained prior errors and passive communication-state precedence.
- Count actual controller TX separately from queue acceptance; saturate interface
  counters and preserve live evidence/interlocks/history when clearing totals.
- Add eight bounded copied diagnostics and resource snapshots; slow trace output
  cannot consume normal RX. Keep map/sample provenance and heartbeat policy explicit.
- Extend the offline native consumer and independent health/counter/trace tests.

### Owner cancellation and recovery

- Added reserved urgent admission/results, per-request cancellation, reservation/producer
  invalidation and independently retained finite controller recovery in the example owner.
- Track each external frame through final TX reports or explicit retirement, including
  cleanup frames and uncertain enqueue failures. Controller epochs reject old ingress;
  unknown retirement inhibits reuse without discarding operation results.
- Allow the next operation under the same axis reservation while old terminal results
  remain retained. Live prior work still requires explicit cancellation; no stop or replay
  is implied. The transport enqueue callback now receives the controller epoch.

### Fixed

- Added real application-owner loop-setting failure checks for uncertainty,
  retained acknowledgements, readback failures and unrelated-node progress.
  Removed an unused duplicate generation field from each owner request slot.

### Ordinary application CAN owner

- Add copied admission, per-axis/physical-peer reservations, fair bounded
  two-channel scheduling and immutable results with explicit release.
- Route all ingress through existing passive/SDO storage before observer copies;
  distinguish controller queue acceptance, actual TX and protocol confirmation.
- Preserve initial queue age across dispatch and retire active/queued work on
  peer invalidation; retain uncertainty and permanent SDO quarantine.
- Integrate public reads and confirmed loop settings, including pre-TX
  configuration withdrawal and checked new-epoch readback, plus a native example.
- Keep the owner outside the core package; urgent/cancel/recovery remain 24.

### Control and tuning groups

- Add five finite typed groups covering all 33 U16 tuning/control reads, with
  raw unknowns, per-field provenance, checked atomic getters and bounded work.
- Add explicit model-bound loop-selector settings using the existing reserved
  ACK/readback lifecycle; reject OL86-C closed loop and invalid candidates.
- Harden settings descriptor/mask/cursor validation before yielding work.
- Record exact evidence gates for other tuning writes, independent history/
  communication ownership and 95 generated read descriptors. No new motion API.

### Typed settings audit corrections

- Implement explicit 86C DI function/polarity and position speed/ramp settings;
  correct blanket blockers that confused absent software with missing device facts.
- Validate full candidates, actual control/state evidence and continuous owner
  reservation before work; retain per-write acknowledgements, unknown effects,
  exact readback and unchanged getter outputs on failure.
- Withdraw the actual configuration epoch before possible transmission; add a
  checked idle owner epoch update and explicit PDO-map restoration with sample
  qualification kept independent. No quarantine reopening or automatic commands.
- Add independent real fake-controller write/failure tests, standalone consumers
  and documentation. DO/filter/action and scale/limit/mode gates remain pending.


### Digital I/O reads

- Add four finite I/O groups through `prepareDriverRead/nextDriver/advanceDriver`
  and checked `getIo`, with all 23 raw read rows and documentary DI codes.
- Preserve reserved entries, unknown bits/codes, conflicting output addresses,
  terminal numbering and filter metadata; no electrical-level claim is inferred.
- Add offline usage, 7,230 native checks, installed/exported consumer coverage and
  explicit generated read ownership. Prompt 21 remains Partial: DO/filter writes,
  output actions and their partial-effect/invalidation contracts stay gated by
  the specific source facts in the I/O guide.


This repository contains planning/reference material and native service/API ownership proofs. No CAN library, firmware or hardware qualification is released.

### Added

- Three typed native Lichuan read groups covering nine direction/subdivision,
  software-limit and speed/ramp rows, with checked `getDriver`, per-field age,
  unknown raw values and an offline caller. They reuse the existing finite read
  engine. Prompt 20 remains Partial: no write-state or scale permission is inferred.

- Installed finite raw Lichuan identity, state and communication reads for 30
  explicitly assigned rows, with checked getters, per-field SDO/PDO provenance,
  conservative age/skew, retained aborts and an offline compiled preview.
  Read effects, model matching and disputed state interpretation remain unknown;
  Q20 still blocks a non-consuming probe, so prompt 19 is Partial.
- Independent fake-owner read tests and isolated installed/exported consumers
  cover every owned width/access rule, failure publication and invalidation.

- Added pure checked PDO layouts/decoding, complete SDO map-readback evidence and eight passive routes in the existing owner. Actual map contents, wire-sample provenance and capture freshness remain independent; reused routes require explicit producer/ingress retirement evidence.
- Generated the eight unverified EDS PDO templates and ten reviewed passive layout facts from the single ledger, retaining raw unresolved mode/velocity signedness and all existing SDO/write gates. No PDO configuration write, NMT inference, scaled position or completion is added.
- SDO terminals now retain supplied admission time and node lifecycle epoch so mixed stale readback results cannot certify a newer map.

- Added explicit typed CANopen NMT manager requests through the pinned native producer, with one retained local result, bounded deadlines and addressed/domain-broadcast invalidation. Queue/TX evidence remains separate from observed remote state; nothing starts or resets automatically.
- Added NMT versus SDO service eligibility, heartbeat-disabled reviewed probing/configuration, caller evidence-age policy and separate configured-TPDO receive admission. Known Stopped retires ordinary SDO work while retaining uncertainty and terminals; NMT Stop is not a physical motor stop.

- Added pure configured-route heartbeat/boot-up/EMCY codecs and a single passive projection in the existing optional owner's channel table. Independent capture/processing times, producer interval facts, NMT/SDO/error observations and four copied diagnostic records remain separate from drive state, outcomes and health.
- Added synchronous cache/axis invalidation on reset, binding/dialect/configuration withdrawal and controller recovery, including full-history and generation-exhaustion behavior. No heartbeat configuration, NMT command, drive action or channel reopening is implicit. The passive HB/EM implementation decision now uses checked owner projection with pinned native-consumer parity.

- Added SDO capture/processing precedence, separate retained raw observations, stale/unsolicited-response quarantine, explicit binding withdrawal and copied-TX eligibility checks. Unsupported reopening remains blocked without remote-session and local queue evidence; no reset/retry or device read permission is inferred.
- Added independent adversarial reconciliation tests for duplicate replies, different-value writes, every segmented TX cancellation boundary, delayed processing, clock limits, controller caches, alternate qualified channels and partial effects. Updated retained proofs to fail closed on pre-handoff traffic.

- Added bounded CANopenNode SDO lifecycle with two unique physical channel slots, copied requests, immutable overall/queue/response cutoffs, explicit TX versus protocol evidence and retained results. Unconfirmed failures quarantine channels across release; no automatic retry or controller I/O.
- Added expedited/segmented transfers through 32 bytes, checked unsized scalar normalization, full unsigned abort evidence and bounded unsupported-upload cleanup. Promoted the retained bridge into one optional application adapter; portable service values remain stack-free and independently packaged.

- Added deterministic reviewed Lichuan object generation, 194 compact profile-scoped descriptors, optional descriptive metadata and complete generated source/semantic coverage. Four disputed signed types and four reserved entries remain withheld or excluded; all read effects and write qualifications retain their gates.
- Activated exact object freshness in quick/full verification and CI, with malformed-ledger, deterministic, stale-byte and atomic-publication regressions. Added independent-header, installed/exported and catalogue-free consumers; generated metadata implements no typed operation or device support.

- Added pure checked CANopen SDO request/reply envelopes, explicit channel/binding/width correlation, unsigned and signed little-endian reads, and owned reply evidence. Valid aborts retain every bit; valid segmented initiation remains distinct from malformed input. CANopenNode remains the sole selected encoder/session engine.
- Added Classical BRS/ESI rejection, exhaustive frame/command checks, raw-buffer overlap/atomic-output regressions and actual CANopenNode-generated request tests. Isolated installed consumers and strict API documentation cover the new declarations; no device permission or live exchange is implied.

- Added production pure axis configuration, independent mechanical/encoder origins, exact position/rate preparation, wrapped-angle path primitives and reference-aware admission revalidation. Relative targets require their selected basis; observation age survives unchanged reconfiguration, and exhausted generations fail closed.
- Added offline units preview, installed/native axis consumers and independent coordinate, time, overflow, limit and binary32 regressions. Replaced proof coordinate arithmetic with the production API; synthetic I32 capabilities remain test-only. No device command or motion operation is implemented.

- Added production exact rational parsing/conversion, independent dimensional scales, all integer rounding policies and checked binary32 quantization with exact tolerance tests. Added explicit bounded radians approximation and installed C++11/C++17/no-RTTI consumer coverage.
- Migrated retained proof arithmetic and numeric types to production Units, preserving test-only N5/EPOS4 contrasts and profile-specific coordinate policy.
- Added quick/full verification with bounded commands, retained logs/artifacts, generated-version and actual CTest registration checks. Added Windows GCC quick and Ubuntu GCC/Clang sanitizer full CI with pinned actions and Doxygen source.
- Added strict Doxygen 1.13.2 and installed API extraction checks, deliberate checker/documentation failure tests, contributor/security guidance, formatting policy and a maintained current/planned verification table.

- Added the native C++11 `MotorControlCAN::MotorControlCAN` package with documented status, endpoint identity and Classical CAN validation; relocatable CMake installation, optional IDF registration and explicit source export. Development version 0.1.0 comes only from `library.json`, with checked-in deterministic `Version.h`.
- Registered the retained API/owner and multi-drive proofs under `test/`, plus independent-header, C++11/C++17/no-RTTI source/install consumers, stale-version and missing-public-dependency checks. Synthetic profiles and stack dependencies remain outside installation.
- Completed prompt 6 with concrete C++11 value/operation contracts and one fixed-capacity application owner around the selected CANopenNode client. Added copied read/target-write flows, retained results, deadline/retirement checks, urgent fairness and boot-up invalidation.
- Added core-only N5 stepper/BLDC, EPOS4 and proprietary-binding contrasts, independent scales and capability/ramp-dimension checks. Published five distinct architecture contracts and Doxygen for every public proof declaration; no installed profile or physical completion claim.
- Completed prompt 5 with a pinned CANopenNode C/C++11 fake-controller exchange, retained payload/results, asynchronous events, segmented transfers and failure/retirement checks. Selected one application-side engine while keeping the future installed core independent of stack/SDK dependencies.
- Recorded source-backed cancellation, deadline, callback and TWAI queue limits; preserved uncertain execution after controller handoff. Fixed an expired-frame handoff gap found during independent review.
- Added offline vendor inventory/hash/license validation, CMake hash guards and seven integrity regressions for the retained experiment. Recorded measured host object sizes separately from unmeasured embedded memory/timing.
- Completed prompt 4's source-backed N5 stepper/BLDC and EPOS4 servo contrast ledger, with test-only cases for exact coordinates, native units, frame formats, protocol routing and evidence limits. No additional driver or hardware support is claimed.
- Completed prompt 3 documentary review of every Lichuan entry/function, with per-field policy, four separate model records, source-backed decision tables and 21 bounded qualification gates. No CAN implementation or hardware support is claimed.
- Reviewed-schema guards and regressions retain exact feature owners, manual obligations and original conflict alternatives; reject unconditional writes, false non-consuming reads, erased gates and malformed review data.
- Completed prompt 2 source intake: a traceable Lichuan ledger for all 99 EDS indexes, 202 typed entries, 53 manual semantic claims, 24 unresolved conflicts and the `1018` count anomaly, with independent coverage fields and a source-byte validator.
- Added adversarial ledger-validator tests and integrated ledger accounting into the planning-package check.
- Started promptset execution with a live source/consumer audit, exact responsibility map, requirement coverage, executable backlog/roadmap and an explicit unknown CAN bench record. Original reference verification is separate from future ledger implementation and hardware qualification.
- Complete Lichuan EDS intake: 99 indexes, 202 typed entries, raw fields and original line spans, with a source-comparison integrity check.
- Detailed official-source motion, status, settings, I/O, PDO, commissioning and timing tables; pinned public implementations and 17 synthetic protocol fixtures with provenance and licenses.
- Focused concrete evidence in 40 numbered prompts, a reading map, and an explicit source-check/correction loop in every prompt through COMMON and AGENTS.
- Audits of MotorControl-RS, sibling I2C libraries and the FieldCore consumer boundary, with source revisions and retained verification evidence.
- Manufacturer manuals, EDS and multi-drive protocol research with local provenance and integrity manifests.
- A 20-stage implementation promptset for a portable C++11 stepper/servo library, starting with Lichuan Classical CANopen.
- Concrete API/status/owner/health conventions, continuous documentation and Doxygen requirements, maintainability guidance and task-based subagent/model selection.
- An offline planning validator for prompt structure, requirements, links and reference hashes.

### Changed

- Moved pure PDO evidence/time value types into installed headers, forwarded by
  the existing adapter; shared bounded route comparison avoids duplicate policy.
- Assigned all readable ledger rows separate read owners and read coverage;
  corrected the `2000/2001` ZH86 source citation from PDF page 18 to page 17.
  Whole-object/write coverage and unqualified read effects are unchanged.

- Corrected the PDO blocker rationale: CiA documents the conditional remapping procedure. Q17 now separates Lichuan applicability, changed timing settings and sample qualification so an established narrow subset need not wait for unrelated facts.

- Recorded the Prompt 18 PDO configuration blocker and expanded the reviewed Q17 evidence requirement for edit order, allowed states, readback, timing and persistence. Existing passive PDO support is unchanged; no remap/settings API is claimed.

- Corrected initial network attachment to preserve original SDO service-proof
  time and origin, with independent matching-response history across channels.
  Older or unknown-capture responses no longer erase a valid newer service proof.
- The retained proof facade now forwards processing-only receive timestamps as
  unknown capture; queued passive traffic cannot qualify after a reset through
  a fabricated capture time.

- Clarified that SDO freshness premises cover individual segments during a transfer. Added toggle-wrap ambiguity counterexamples and all segmented-upload cancellation boundaries; corrected a test's caller-time order. No runtime state or device permission changed.
- Split the original 20 implementation stages into 50 complete prompts numbered 1–50, with smaller scopes, explicit prerequisites and focused completion checks. Preserved original obligations in a scope map and historical archive.
- Moved owner/adapters before motion evidence, retained the early conditional experiment path, and separated CLI, runner and qualification work into bounded assignments.
- Added targeted shared-contract reading, single-prompt/range execution instructions, a checked sequence manifest and regression checks for numbering, prerequisites, scope ownership and commit/sync closure.
- Expanded every stage with concrete file/API contracts, failure cases, documentation duties and work splits.
- Refreshed source-backed naming, including RS owner admission/service methods and the distinction between operation results and application health.
- Clarified late SDO reply ambiguity, fixed operation deadlines, explicit stop outcomes and staged native-feature coverage.
- Adapted RS/I2C agent guidance for CAN: one branch for the full promptset, verified commit-and-sync checkpoints during and at the end of each prompt, lead-owned Git operations, proportionate CI, failure investigation and reference maintenance.
- Clarified branch creation: each new promptset execution starts a new dedicated branch, then reuses it across all stages and resumed sessions; maintenance audits do not start implementation.

### Fixed

- Reject missing/null ownership for readable ledger entries, preventing history
  and deferred communication/PDO read obligations from disappearing silently.
  Independent prompt-19 review found no runtime defect; its Q20 probe gate remains.

- Fixed matching server aborts being ignored while the next SDO segment was staged; pending TX is now retired before processing the abort. Delayed controller confirmation for the latest live handoff now remains valid after a nonfinal reply, until the next handoff supersedes its token. Independent upload/download regressions preserve quarantine, deadlines and immutable terminals.

- Fixed object catalogue generation for supplementary Unicode and C++11 trigraph/escape boundaries; invalid UTF-8 surrogates fail before publication. Generated coverage now retains each documentary disposition and all three RPDO1 source mapping words.

- Fresh independent prompts 10-11 audit corrected a stale retained-proof unit/rounding comment and an ADS1115 source citation. Axis/reference arithmetic, CAN/SDO checks and isolated package consumption passed new independent probes and integrated verification; no executable change was needed.

- Independent prompts 7–9 audit made quick-mode omissions explicit in verifier summaries, added a useful failure for empty tool-version output, and corrected stale package inventory and floating-environment documentation. Arithmetic and package consumption passed independent review.
- Corrected the adapted directed binary64 conversion to check both neighboring directions; rare rounded-operand underestimates now return the closest toward-zero value, with independent regression vectors.
- Allocated retained verifier directories atomically so equal Windows clock readings cannot collide; the retention regression now fixes the clock deliberately.
- Preserved arbitrary 32-bit SDO abort words at the experimental C/C++ bridge without an invalid enum load, found by the new GCC/Clang UBSan jobs. Added upload/download high-bit regressions.

- Fresh independent prompt-6 audit clarified the fake owner's fixed per-call RX/TX/event budgets and zero-budget behavior. Additional core/owner probes and all maintained checks passed; no runtime correction was required.
- Fresh independent audit of prompts 4-5 corrected EPOS4 unit configurability, retained N5's already-set-bit4 profile-position entry trigger, and added missing ramp-dimension and optional brake/calibration/missing-mode fixture specifications. The native stack experiment passed fresh ownership/failure/boundary review without a runtime change.
- Corrected fault-reset edge wording, native mode/action ownership and per-entry manual citations. Kept unknown limit/offset/feedback units, peak-current meaning, model-specific output conflicts and raw identity acquisition distinct from qualified behavior.
- Rechecked prompts 1-2 independently: completed the 126-path source-map check record, retained ZH86's same-manual DO contradictions and the separate 57C statusword contrast, and hardened ledger/planning validation for false applicability, missing conflicts and malformed rows.
- Removed the evidence-harness dependency on firmware-gated remapping/settings, preserving unfinished feature and owner-integration obligations; aligned numeric execution order with independent progress after a blocker.
- Clarified that stop/reset controlword arrows never require enabling first; added explicit repeated-stop and fault-reset trace requirements.
- Preserved the 57C manual's conflicting DO0/1/2 versus DO1/2/3 labels, separated SDO access from PDO eligibility, and required qualified provenance for delayed PDOs on reused routes.
- Added planning regressions for transitive evidence deadlocks and README prerequisite drift. Rechecked the five official PDFs and EDS without replacing their identical original bytes.
- Exposed conflicting DO addresses/functions, three status layouts, mode access, velocity sequence/sign, subdivision/soft-limit defaults and undocumented timing instead of treating generic CANopen or example code as firmware truth.
- Documented and rejected public-example SDO command/width errors, shared status, hidden persistence and arbitrary timing assumptions; kept absent following-error/automatic-tuning capabilities out of the new evidence requirements.
- Corrected velocity duration to start at successful preparation, matching RS, with queue/setup time included.
- Corrected SDO upload size/transfer assumptions and scoped non-changing read claims to reviewed objects.
- Clarified service eligibility with disabled heartbeat, first TPDO observations and NMT transition evidence.
- Moved catalogue freshness and console parity gates to the stages that implement their sources; narrowed multi-profile acceptance to the implemented scope and small shared contrast fixtures.
- Removed unnecessary repeated report/audit work while keeping current source inspection and documentation mandatory.
- Preserved original reference/baseline/evidence bytes in Git and added optional HEAD/index artifact checks; line-ending conversion previously broke 52 stored artifact hashes.

The [24 October prompt audit](docs/audit/PROMPTSET_REVIEW_2026-10-24.md) records the findings, fixes, source revisions and validation limits.

The [28 October correction audit](docs/audit/PROMPTSET_REVIEW_2026-10-28.md) records the new source-backed findings, live sibling delta and current prompt measurements.

The multi-drive architecture and test contrasts are plans. Nanotec, maxon and other researched products are not implemented or qualified drivers in this checkout.
