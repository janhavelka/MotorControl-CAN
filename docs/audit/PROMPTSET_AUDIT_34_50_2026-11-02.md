# Audit and repair instructions: prompts 34-50 and the code they build on, 2 November 2026

Instructions for the AI coder who will repair and execute these prompts. This file
gives settled decisions, ordered work packets and the findings behind them. It changes
nothing by itself: writing it edited no prompt, code, ledger or test, and touched no
hardware.

**Baseline.** MotorControl-CAN `9e5b62d` on `promptset/motorcontrol-can`; siblings as
read on 2 November 2026 (MotorControl-RS `6ff2374`, dirty with uncommitted qualification
work). Prompt and code line numbers refer to that state. Refresh them before relying on
a line number, because other sessions commit here. While this file was written, another
session was adding pure position preparation for prompt 33 to the working tree, uncommitted
(`preparePositionParameters`, Partial); line numbers exclude that work.

**Finding status.** CONFIRMED means a reviewer opened the cited source or ran a probe.
PLAUSIBLE means the reasoning holds but something is still unmeasured, and the finding
says what. "Lead checked" means the audit lead also re-read the code or source
independently. Nothing here is a measurement of a real drive.

## Start here

1. **Do not build any finite move, velocity or homing lifecycle yet.** That means prompt 33's
   `prepareMoveAbsolute`, `nextMove` and `advanceMove`, and prompts 34-36. Finish W1 to W8 first.
   On the current code a running move cannot be stopped (C-16), a stop would overflow the
   firmware task stack (A-11) and the velocity API returns the wrong unit (A-01). Building on
   those seams copies the defects into four more operations. Prompt 33's pure
   `preparePositionParameters` sends nothing and may be finished and kept.
2. **The decisions in section 3 are settled.** Do not reopen them or ask again. If
   primary evidence contradicts one, record the evidence, stop that packet and report it.
3. **Work through section 4 in order,** one packet at a time. Prompts 40 and 43 are the
   only exceptions: W10 says when they may run.
4. **Where this file and a prompt disagree, this file wins** until W1 has repaired the
   prompt.
5. **The repository rules still apply.** Keep the one execution branch, stage explicit
   paths, preserve concurrent work and never force-push. Commit and push each verified
   packet or useful part of one. A changed test is a recorded contract change in the
   stage report, never a weakened one. No live CAN or motion without the bench
   prerequisites in `AGENTS.md`; a decision here is not bench authorization.
6. **Use a short build directory on Windows,** such as `.build/w3`. A long path makes the
   configure step fail with a missing `rules.ninja`; that is not a repository defect.

## 1. What the library is and where it stands

MotorControl-CAN is a framework-independent C++11 library for CAN-controlled stepper
and servo drives from several manufacturers. The first backend is CANopen/CiA402 and
the first target is the Lichuan CL86-C. Common motion names and meanings follow
`../MotorControl-RS`. N5 and EPOS4 are design contrasts, not supported drives.
The installed core is pure: no I/O, clock, heap or retries. Applications own the
controller, time, queues and policy.

Implemented and host-tested today: exact units and axis targets, CAN frames, SDO and
network codecs, generated object metadata, the bounded SDO lifecycle over CANopenNode,
typed reads and a subset of settings, `CanBusOwner`, a TWAI adapter, shared firmware
examples, a passive status decoder, and conditional stop, alarm-clear and counter-zero
actions (prompts 31 and 32, both Partial).

Not implemented: enable, release, finite move, velocity, homing, discovery, commissioning,
persistence, CLI or Python runner. Prompts 34-50 are Pending, 18 is Blocked, 19-32 are
Partial, and 33 is in progress as pure preparation only. No drive has been observed on the bus.

Check results:

| Check | Result |
| --- | --- |
| `python scripts/check_promptset.py`, `check_repository.py`, `git diff --check` | Pass with this file in place, run by this audit |
| `python scripts/verify.py --mode quick --compiler gcc` | Pass by this audit on the tree that became `1d68ccd`, before prompts 31 and 32 |
| Full verifier at `9e5b62d`: 55/55 CTests, package consumers, strict Doxygen | Pass as recorded in `docs/reports/prompts_31_32_audit.md`; not rerun by this audit |
| Stack frames for ESP32-S2 and S3, compile only | Fail against the 8192-byte task stack, measured by this audit (A-11) |
| Sanitizers, Arduino/IDF firmware builds | Not run by this audit |
| Hardware | Not run |

Independent fuzzing of the arithmetic (1.1 million cases against Python `Fraction`,
also under UBSan) found no mismatch. The arithmetic is not where the risk is.

## 2. Verdict

The numeric device facts are good: every object, width, range and bit checked in
`LICHUAN_MOTION_FACTS.md` and `LICHUAN_OBJECT_FACTS.md` matched the original
documents. The implemented action subsets also do what their guides say. The problems
are in what those subsets leave out and in how the prompts compose them:

1. **A running move cannot be stopped.** `prepareStop` accepts only a latest `000F`, and
   a trigger leaves `001F` (C-16). Even from `000F`, the documented procedure cancels the
   in-flight poll, which quarantines the route (C-17). After an unconfirmed write no stop
   can be sent at all (C-01).
2. **A stop would overflow the firmware task stack.** On ESP32-S2 and S3 `prepareStop`'s
   own frame is 8,624 bytes and the owner task has 8,192 (A-11).
3. **The dependency graph cannot finish** (G-01, G-02). Prompts 49-50 descend from
   Blocked 18 and Partial 30, although the README says they can complete.
4. **Three API gaps would force wrong or copied motion code** (A-01, A-02, A-03). The
   worst writes a command-increment rate into an rpm object.
5. **Discovery, the CLI read routes and the first hardware step are all stopped by gate
   Q20,** which has no practical way to close (DEC-1).
6. **The manuals document no way from homing to a position move** on an enabled drive
   (D-02, D-17). No prompt mentions this.
7. **Two reproduced TWAI bugs** make a routine CAN event end all SDO service to a drive
   until the MCU resets (C-03, C-04).
8. **The real bench board cannot run the firmware** (G-04).

## 3. Settled decisions

The repository owner asked the audit lead on 2 November 2026 to select the recommended
option for each open decision. These are those selections. Record them in the affected
contracts as you implement them.

| ID | Decision | Packet |
| --- | --- | --- |
| DEC-1 | Q20 closes through a named list with a new disposition, "read effect accepted, not qualified" | W4 |
| DEC-2 | A stop may be sent on an unresolved route, as `000B` only, and its reply can never qualify | W6 |
| DEC-3 | A matched server abort does not quarantine; a distinct-index drain marker can reopen a route | W5 |
| DEC-4 | TX uses hardware retransmission, `fail_retry_cnt=-1` | W3 |
| DEC-5 | `CanBusOwner` moves to `adapters/owner/`; the CANopenNode copy stays where it is for now | W2 |
| DEC-6 | A witness outside the soft limits may still move inward to an endpoint inside them | W8 |
| DEC-7 | The EPOS4 PDF stays remote-only, with its revision and hash recorded | W1 |
| DEC-8 | A stop is allowed from a trigger word, and bit 4 is held until the move is terminal | W6, 33 |

**DEC-1. Read effects.** `docs/device_conflicts.md` says "Stable repeated values alone
cannot rule out consumed events", so a manufacturer statement is the only other route and
Q20 cannot close in practice. Yet every path to device evidence begins with an upload.

- *Accepted, polling allowed:* `1000`, `1018:00..03`, `1009`, `100A`, `2000`, `2001`,
  `6041`, `6061`, `6064`, `606C`, `2100`, `2101`, `2102`.
- *Explicit single reads only, never polled:* `603F`, `1001` and `1003` history. These stay
  `UNQUALIFIED` and are reached through `requestExperiment` or an action's prerequisites. A
  fault code is the most plausible read-to-clear object, so the first bench read of each
  records the LED fault state before and after.
- *Everything else* stays gated by Q20.

"Accepted" is the owner's risk acceptance, not evidence that a read has no effect. Results
keep the raw "effect unknown" field, `effectPossible` stays true, and no document may call
these reads non-changing. The first live read still needs the bench prerequisites.

**DEC-2. Stop on an unresolved route.** A motor that may be moving is exactly when the
host needs a stop. An explicitly requested urgent `6040:00=000B` may be transmitted on an
unresolved route. Rules:

- Only `000B`, the vendor-labelled emergency stop in EN86, ZH86 and OP86. Never `010F`
  here: it has the enable-operation bits set, so under CiA402 (from memory, not
  vendor-confirmed) it could enable a released drive. `000B` has bit 2 clear.
- The reply never qualifies, because a late `6040` reply is indistinguishable from the
  stop's reply. `execution` stays UNKNOWN whatever arrives, and the route stays unresolved.
- The stop's effect can only be shown by later independent observation, and it never
  settles the earlier operation's uncertainty.
- One request, one transmission, no resend. A second stop is a new explicit request. This
  matches current RS behavior (`docs/console.md:196-199`, `docs/ess_actions.md:165`).
- A stop held across a recovery has lost its state evidence, so it is re-issued as `000B` even
  if `010F` was requested.
- "Never transmitted" means `transmissionPossible==false`, nothing weaker. A refused enqueue
  may still have been accepted (`examples/common/CanBusOwner.h:194`).
- A CAN stop is best effort and not a safety function. An independent physical stop stays
  mandatory on the bench. Say so in `docs/stop_actions.md`, `docs/can_owner.md` and the
  Doxygen.
- A PDO stop is not a shortcut: boot NMT state and PDO behavior are undocumented (D-05).

**DEC-3. Quarantine.** A matched server abort has the same correlation premise as a
success, so it leaves the channel open. An unresolved route reopens only through an explicit
drain marker:

1. The application requests it. Nothing triggers it automatically.
2. All TX on the route is retired, as proved by owner reports or node deletion.
3. The marker is one upload of an accepted DEC-1 object whose index and subindex differ from
   every unresolved request and unsolicited frame recorded on that route. Each channel retains
   up to four such pairs; on overflow it refuses markers. The marker's index check runs before
   the unresolved branch at `adapters/canopennode/SdoClient.cpp:399`, and a nonmatching frame
   changes nothing, so an old reply cannot satisfy it. Ordinary work keeps its check at `:419`.
4. A matched success reopens the route and increments a retained `requalifications` counter.
   Failure leaves it unresolved. Each attempt is a new explicit request.

This supplies the "ordered drain/retirement boundary" that `docs/canopen_services.md:321`
requires. It assumes the server answers one request at a time and in order. That is CiA 301
from memory and is not vendor-confirmed, so record it under Q18 and measure it. A marker
settles the channel only. Earlier effects stay uncertain, `000F` history stays consumed, and
nothing replays or re-enables. Quiet time, heartbeat, a new host epoch, rebuilding the client
and same-object readback remain insufficient, so their negative tests stay.

**DEC-4. TX mode.** `fail_retry_cnt=-1`. HAL v1 offers only this or single-shot
(`docs/adapters/twai_sdk.md:45`). It is not an application replay: hardware retransmits only
a frame that no node accepted. The cost is that with no ACK the frame stays pending, so at
the deadline it stays "possibly transmitted" until node deletion retires it.

**DEC-5. Owner location.** Add `adapters/owner/` with optional target
`MotorControlCAN::CanBusOwner`, outside the installed portable core. Do not move
`experiments/canopen_service_fit/vendor` now: `scripts/check_promptset.py:120-184` hash-checks
it and the move has no behavior value yet. Name it as packaging work for prompt 49.

**DEC-6. Soft limits.** Allow the move when the witness is outside the limits, the endpoint is
inside them and the move goes toward the range. An endpoint still outside stays LIMIT. This
differs from RS, whose `src/Axis.cpp:168-170,508` still rejects it. Record it as an intentional
difference and report it to the RS owner; do not edit RS.

**DEC-7. EPOS4 evidence.** Keep rel13740 remote-only, with URL and SHA-256 from section 11. A
coder cannot perform a rights check, and retaining a PDF would change the pinned
`hashed_artifacts==137`. This replaces the earlier "retain after a rights check" draft.

**DEC-8. Stop predecessors and bit 4.** The manuals give stop arrows only from word 15, and no
stop from 31, 95, 63 or 127. The documented bit meanings still apply, and clearing bit 4 while
stopping moves in the wanted direction. Note EN86 p15 says a falling bit 4 interrupts homing, so
it can matter. Allow these, and carry `predecessorDocumented` in the evidence:

| Mode | Latest acknowledged `6040` | Stop word | Status |
| --- | --- | --- | --- |
| 1 | `000F` | `010F` or `000B` | Documented; implemented today |
| 1 | `001F`, `005F`, `003F`, `007F` | `010F` or `000B` | Bits documented, arrow not; `predecessorDocumented=false`; measure under Q12 |
| 3 | `000F` while running | `010F` or `000B` | Arrows are only in the position section; `predecessorDocumented=false`; prompt 35 |
| 6 | `001F` | `000F`, the documented "1->0: Stopping homing" | Documented; prompt 36. This is the stop itself, not a preamble |
| Any | Unknown, or state evidence stale | `000B` only | DEC-2 path |

W6 widens mode 1 only; prompts 35 and 36 add modes 3 and 6 with their operations.

Prompt 33 does not clear bit 4 during a move, because its meaning in mode 1 is unspecified. It
holds `001F` or `005F` until the move is terminal, then writes `000F` as a visible step. That is
library policy, not a vendor rearm rule. Prior `010F`, `000B`, released or other words still
reject in `prepareStop`.

**Smaller settled choices.**

| Finding | Choice |
| --- | --- |
| A-02 | A move's own `607A`, `6081`, `6083`, `6084` writes are phases of the move, do not go through `prepareDriverSettings` and do not advance the configuration generation. An explicit settings call still advances it. Homing uses the one-expected-advance pattern of `expectedTarget` in `Actions.cpp:40-45` |
| A-07 | `CORRELATION_LOST` withdraws earlier blocks, as BUS_OFF does |
| A-08 | Add a distinct service-timeout outcome to reads and actions |
| A-14 | Add `pathPositive` to `PreparedTarget` |
| C-05 | `recover` quarantines only channels with outstanding work |
| C-14 | Accept Classical DLC 9..15 with an 8-byte payload |
| D-10 | Three terms: `save parameters`, `restore factory settings`, `re-apply recorded configuration` |
| G-02 | The usable-prerequisite rule in G-02 |
| G-13 | Inclusive processing-time cutoff, as reads and actions already do: processing at or after the deadline expires |
| R-04 | The console goes in `examples/common/ProbeConsole.*` |
| R-14 | Adopt RS stop escalation and in-flight settling; other rows as listed in R-14 |

## 4. Work packets

Each packet ends with passing checks and a pushed commit. Use
`python scripts/verify.py --mode full --compiler gcc --build-dir .build/<packet>` for code
packets, and also `python scripts/check_lichuan_ledger.py` and
`python scripts/generate_lichuan_objects.py --check` when the ledger changes. Verify required
CI on the exact final commit after each of W2 to W9. Update `CHANGELOG.md`, the stage report,
`docs/backlog.md` and the affected guide in the same commit as a contract change.

### W1. Repair the promptset (documentation only)

**Findings:** G-01 to G-14, section 10's audit rule, N-01 to N-05, the per-prompt table in section
12, DEC-7. **G-05 is already resolved.**

1. Add one paragraph to `docs/prompts/COMMON.md` linking this file as active until every prompt is
   repaired.
2. Apply G-01 and G-02 first: remove 18 from 38 in its three places, replace the start rule, add the
   Inherited gates column, and add the checker guard that 18 is not an ancestor of 49.
3. Apply the remaining G items, then section 12 prompt by prompt, then section 10's rule and N-01 to
   N-05. Follow the checker rules in section 13.
4. Write the DEC-2, DEC-3 and DEC-8 obligations into prompts 31, 33, 35, 36, 42 and 45, so that those
   prompts describe the contract W6 delivers. Add the new obligations for prompts 31 and 32 to their `PROGRESS.md` rows.

**Done when:** `python scripts/check_promptset.py`, `python scripts/check_promptset_test.py` and
`git diff --check` pass. No CI run is needed.

### W2. Move the owner and share test fixtures (mechanical)

**Findings:** C-08, C-11, C-12. Run this when no other session has uncommitted owner edits.

1. `git mv` `examples/common/CanBusOwner.*` to `adapters/owner/`, use namespace
   `MotorControlCANOwner`, and add the optional CMake target.
2. Update `adapters/twai/TwaiAdapter.h:6`, `examples/common/CMakeLists.txt:4`,
   `examples/common/firmware_sources.json:3`, `scripts/check_firmware.py`, the repository checker
   lists and `CONVENTIONS.md:24,139-140`. ProbeApp, console and firmware code stay in
   `examples/common`.
3. In a second commit, move the near-copied `Fake`/`Rig` code into `test/support/OwnerRig.h` and
   split `CanBusTypes.h`. No DSL.

**Done when:** the full verifier and firmware checks pass with no behavior change. Later packets edit
the new paths.

### W3. Make TWAI transport survive routine events

**Findings:** C-03 (DEC-4), C-04, C-07, C-13, C-14. Reopens prompt 27; record that in its report.

1. Set `fail_retry_cnt=-1` in `adapters/twai/IdfTwaiDriver.cpp:78`; correct
   `docs/adapters/twai_sdk.md:45` and `adapters/twai/README.md:27`.
2. Make `submit` return SENT, DEFERRED or FAILED. DEFERRED keeps the slot QUEUED, and a withdrawn
   first frame with a proved-unsent report cannot quarantine.
3. Split the adapter into `pollEvents` (before the owner) and `promote` (after).
4. Report real reasons instead of "clock fault" (C-13); accept DLC 9..15 (C-14).

**Tests:** change `test/twai_driver_test.cpp:274` as a recorded contract change. Add the three
scenarios in C-03, C-04 and C-07 with the fake driver: arbitration loss, RX interrupt during
promotion, and a same-tick reply followed by an urgent admission.

**Done when:** the full verifier and firmware builds pass, and CI passes on the final commit. The
bench still has to count failed TX under TPDO load (W11).

### W4. Close Q20 for the accepted list (DEC-1)

1. Add a ledger field `read_disposition: accepted_not_qualified` on the DEC-1 rows. Record the owner's
   selection, date and "fixture not yet run". Correct Q20 `owner_prompts` to 19, 29, 37, 41, 44, 47.
2. Add one enum value, `ACCEPTED_UNQUALIFIED`, to `ReadEffect` and `SdoReadEffect`, and regenerate. Do
   not use `REVIEWED_NON_CHANGING`: that value would make `effectPossible` false in
   `SdoClient.cpp:251-253`.
3. Map the value one-to-one at `src/profiles/lichuan_c/Reads.cpp:224-226`: today anything not
   `UNQUALIFIED` becomes `REVIEWED_NON_CHANGING`, which would silently claim no effect. Treat it
   like `UNQUALIFIED` at `Reads.cpp:440`. Admit it at `SdoClient.cpp:192-195` (first upload on an
   unconfirmed channel), `adapters/canopennode/Nmt.cpp:65`, `prepareProbe` (`Reads.cpp:290`) and
   `examples/common/DeviceEvidence.cpp:88-90`. Drive the hardcoded `ReadEffect::UNQUALIFIED` at
   `scripts/generate_lichuan_objects.py:267,297` from `read_disposition`.
4. Update the Q20 row in `docs/device_conflicts.md`, `docs/canopen_services.md:159-163,306-308` and
   `docs/hardware_bench.md:65-69`.

**Tests:** record the changed pins in `test/reads_test.cpp:201,314`, `test/objects_test.cpp:24` and
`test/device_evidence_test.cpp`. `prepareProbe` on `1000:00` succeeds. `603F` and an unlisted object
still reject as they do today. A successful accepted read keeps `effectPossible=true`. A first accepted upload confirms the
channel without claiming freshness.

**Done when:** the full verifier, ledger check and generator check pass.

### W5. Make SDO channels recoverable (DEC-3)

**Findings:** C-05, C-06, N-03, N-04. Reopens prompts 13, 14 and 24; record that in their reports.

1. In `SdoClient::finish` (`SdoClient.cpp:269-270`), do not set `unresolved` on a matched SERVER_ABORT
   on a clean channel.
2. At `adapters/canopennode/Network.cpp:165,206`, for RECOVERY and BUS_OFF set `unresolved` only with
   occupied possible work, `handoffLive` or `cleanup`. BOOTUP and BINDING still quarantine. Passive node
   evidence is invalidated as it is today.
3. Add the drain marker from DEC-3 through a new path that W6 reuses: owner
   `admitDrainMarker(const SdoTransfer&, now, id)` and a flag `Slot.outsideSequence`. `eligible`, `expire`
   and the FIFO check skip `sequence()` for a flagged slot (`CanBusOwner.cpp:260-268,275,281-284`). A client
   path bypasses only `unresolved` and `ordinarySdo` (`SdoClient.cpp:185,485,519`), never BUS_OFF, RECOVERING
   or known STOPPED. The owner returns INHIBITED while `external_[c]` is pending or blocked. Ordinary
   `checkAdmission` does not change. In the receive path the marker's index check runs before
   `SdoClient.cpp:399`.
4. Remove the in-place `SdoClient` rebuild at `test/can_bus_health_test.cpp:209-210`, and make the streak
   documentation true.
5. Fix C-06: on trace loss, let the in-flight request settle and withdraw only the remainder. Size the
   trace above per-turn output.

**Tests:** change `test/sdo_transfer_test.cpp:308` as a recorded contract change. Add: a read of an absent
object leaves the route open; recovery leaves an untouched axis open; a marker on the same index is refused;
a late old reply during a marker is ignored; a failed marker stays unresolved; earlier effects stay uncertain
after a successful marker. Keep the negative tests for quiet time, heartbeat, epochs, rebuilds and
`uncertainCounterTriggerCannotReopenThroughEpochSync` in `test/reference_action_test.cpp`. Review
`test/sdo_reconciliation_test.cpp` and `test/can_bus_recovery_test.cpp` before changing behavior; they hold
most of the existing quarantine checks.

**Docs:** add a drain-marker row to `docs/canopen_services.md:319-326` and correct `:294-296`.

### W6. Make stop usable (DEC-2, DEC-8)

**Findings:** C-01, C-02, C-16, C-17, C-18, C-19, A-20. Reopens prompts 24 and 31, and closes the stop
gates recorded in `docs/stop_actions.md:21-22,105-108,115-117` as far as software can.

*Owner:*

1. Set `URGENT_REQUESTS = SEQUENCES`, and make the urgent slot index `REQUESTS` plus the channel, so that one
   axis cannot take both slots and an unread stop on one axis does not block the other.
2. Add `beginUrgentOperation(id, operation, deadline, now)`. It retires this sequence's unsubmitted work as
   terminal with no TX, leaves an in-flight request to settle, and gives the stop its own operation and
   deadline. `beginSequence` reuses an invalidated slot once `live` is false, so it stops blocking its axis
   while retained results stay readable (`CanBusOwner.cpp:110,116,145`). Add `findSequence(binding)`.
3. Add `admitBestEffortStop(const SdoTransfer&, now, id)` on W5's `Slot.outsideSequence` path. It accepts only
   `6040:00=000B` and uses the reserved urgent slot. Until its final TX report, a frame received on a
   best-effort route is recorded as unqualified and does not retire the handoff, because otherwise a late
   reply would withdraw the stop (`SdoClient.cpp:404`, `TwaiAdapter.cpp:153`). Keep ordinary `admit` and
   `admitUrgent` refusals as they are, so `quarantineAndOtherNodeProgress` lines 619-634 stay valid.

*Core:*

4. Widen `prepareStop` for mode 1 per the DEC-8 table, and add `predecessorDocumented` to `ActionContext`.
5. Remove `now - e.admittedUs <= age` from `controlEvidence` (`Actions.cpp:119`). The bound applies to reads,
   and a drive idle at `000F` must not lose its stop.
6. Reject at preparation a `maximumAgeUs` shorter than one observation cycle (A-20).
7. Add a separate small `BestEffortStop` context keyed by `ReadTarget`, not `ActionContext`, because
   `Actions.cpp:56,91` require a live reservation. It yields word `000B` and needs no state evidence. It never
   reaches SUCCEEDED. A transmitted TX report gives `STOP_SENT_UNCONFIRMED`; possible transmission with no report
   gives DEADLINE; both have `execution=UNKNOWN`. No possible transmission gives NOT_TRANSMITTED. Add the outcome to
   `ActionOperation.h`.
   Separately, make CLEAR_ALARM read ERRORS once, after the alarm clears, instead of on every cycle
   (`Actions.cpp:49-50`), to honor DEC-1.

*Application:*

8. Apply C-10 here. Keep the latest `6040` terminal per axis in ProbeApp's session, and consume it on any new
   `6040` admission, so a copied `000F` cannot stay eligible (C-19). Steps 5 and 8 share a commit. Console verbs
   wait for prompt 42.
9. A stop never cancels in-flight work. It waits for that work to settle. If the settling request leaves the
   route unresolved and the stop never transmitted, the application issues the best-effort stop with a new
   deadline.
10. Adopt RS: hold a never-transmitted stop across one bounded recovery, then send it once as `000B` under the
    deadline fixed at admission. `stop fast` escalates a running `stop normal` (R-14).

**Tests, as recorded contract changes:** `unsupportedPrerequisitesAreAtomic` lines 426-428 and
`exactDeadlineAndRepeatedStopRejection` lines 799-803 keep rejecting `010F`, `000B` and released states but
now accept trigger words; `freshnessAndStepExhaustion` lines 812-821 drops the control-age rejection.

**New tests through the real owner:**

- Enable, trigger `001F`, poll in flight, stop: the stop is sent after the poll settles.
- Lost trigger reply, best-effort stop admitted and transmitted, identical late reply: stays unqualified and
  the route stays unresolved.
- Lost poll reply while a stop waits.
- Stop on axis 2 while axis 1's stop is unread.
- Stop from another producer through `findSequence`.
- `000B` never replaced by `010F` on the best-effort path, including a stop held across recovery.
- A late reply while a best-effort stop is pending does not withdraw the stop.
- One axis cannot occupy the other axis's urgent slot.
- Idle at `000F` longer than `maximumAgeUs`: the stop is still admitted.

**Docs:** rewrite `docs/stop_actions.md:83-108`, add a best-effort section, and correct `docs/can_owner.md:108-113`
and the `Actions.h` file comment. Update the ledger `implementation_scope` notes for both stop functions and
regenerate.

**Done when:** the full verifier and ledger checks pass and CI passes on the final commit.

### W7. Bring action stack use under budget

**Findings:** A-11, C-15.

1. Add `scripts/check_stack.py`: compile the installed Lichuan sources with the pinned ESP32-S2 and S3
   compilers using `-Os -fstack-usage`, and fail above a per-function budget of 2,048 bytes. Run it in the firmware
   CI job. Skip it locally with a visible message when no toolchain is present.
2. Remove the candidate copies from `prepare*`, `advanceAction`, `noteActionTransmission`, `advanceRead` and
   `prepareDriverSettings`: validate first, then commit changed fields in place. Rejected calls must still leave
   outputs unchanged, and the existing atomicity tests prove it.
3. Shrink `ActionContext`: a stop does not need `originalState`, and the alarm clear can keep only what its
   incident record needs. Do not embed a full `ReadContext` in the coming `MoveContext`.
4. Make contexts static or owner-held in the firmware, never task locals. Report the task's high-water mark through
   the diagnostics, and raise `FIRMWARE_STACK_BYTES` only with a measured reason.

**Done when:** the budget check passes on both targets and the full verifier passes. The bench still measures the
high-water mark during a stop (W11).

### W8. Fix the core APIs prompts 33-36 need

**Findings:** A-01 to A-10, A-12, A-13, A-14, DEC-6. Reopens prompts 10, 19 and 30; record that in their reports.

1. **A-01:** add the `AxisConfig` native-rate basis, and define `native` for CAN velocity in the grammar.
2. **A-02:** document the settled rule in `Settings.h` and in prompts 33 and 34.
3. **A-03:** expose `interpretStatusword` and have `getStateInterpretation` call it. Put the `2100` masks now
   private to `Actions.cpp:8-10` beside it.
4. **A-04:** move the enabled-state proof into a shared private helper, and rename `SettingsReservation` to
   `AxisReservation` in one mechanical commit.
5. **A-10:** create `src/canopen/SdoEvidence.h`, and use it in Reads, Settings and Actions (`Actions.cpp:14-17,
   106-120,142-157`).
6. **A-05, A-06, A-07, A-08, A-09 (DEC-6), A-12, A-13, A-14:** apply each finding's fix with its regression test.
   Report A-05 and DEC-6 to the RS owner.

**Done when:** the full verifier passes and CI passes on the final commit. Prompt 33's finite lifecycle may then start.

### W9. Port the firmware to the real board

**Finding:** G-04. Record it as a reopened 27/28 assignment and name it in 29's resume conditions.

Deliver single-core operation, a UART console, S2 TWAI retirement evidence, GPIO18 silent control and an S2 compile
fixture. Flash and memory facts are in the bench record: 4 MB flash, no PSRAM.

**Done when:** an S2 firmware build passes in CI. No live traffic is part of this packet.

### W10. Execute the prompts

1. Run prompt 40, then 43. These have no device gate and may run once W1 is done, in parallel with W2 to W9.
2. After W8, finish 33's finite lifecycle on `preparePositionParameters`, then run 34, 35, 36, 37, 38 and 39 in order. Apply C-09 before 38. Each prompt's gated
   features stay visibly gated and do not block its independent work.
3. Run 41, 42, 44, 45, 46, then 47 and 48 when the bench allows, and 49 and 50 as a scoped software candidate listing
   all inherited gates.

### W11. First bench measurements (needs the bench prerequisites and the owner's authorization)

Run with an independent physical stop and the shaft free. Retain raw traffic. In this order:

1. Listen-only capture with the PCAN-USB, or S2 silent mode: heartbeat, boot-up, TPDO and EMCY on the actual bus.
2. DEC-1 reads on one drive; LED state before and after `603F`.
3. Failed TX count under TPDO load (C-03), and the owner task's high-water mark (A-11).
4. Server order and late-reply behavior for the drain marker (DEC-3, Q18).
5. Enable; stop from `000F`; then `000B` and `010F` from a trigger word in a running move (DEC-8, Q12).
6. `000B` on a released drive: confirm that it does not enable.
7. Bit 4 falling after completion in mode 1; retained targets; mode change while enabled (D-02, D-03, Q11).

Each result supplements the documents for that firmware only. One passing run does not close a gate for other firmware.

## 5. Owner and adapter defects

### C-01 Critical. No stop can be sent after an unconfirmed motion write. CONFIRMED, reproduced, lead checked

**Where:** `examples/common/CanBusOwner.cpp:80-86` (`sequence()` returns null on an
unresolved route), `:100` (`beginSequence` refuses), `:179-181` (`admit` returns
INVALID); `adapters/canopennode/SdoClient.cpp:269-270`; `docs/canopen_services.md:294-304`.

**Problem:** a move trigger whose reply is lost makes the channel unresolved. From
then on both `admit` and `admitUrgent` refuse every request on that route, including
a stop. Quiet time, recovery and release do not reopen it, and firmware keeps the
owner in function statics (`examples/common/EspProbeMain.cpp:22-25`), so only an MCU
reset does.

**Consequence:** the most dangerous moment (the motor may be moving and the host does
not know) is exactly when the library cannot command a stop. These prompt scenarios
cannot run as written: `45:19` ("lose the trigger response, request stop, deliver a
late acknowledgement"), `35:32` ("a later separately admitted stop"), `33:30`,
`36:30`, `42:28`. Prompt `31:24` ("Stop cannot invent channel availability") points
the other way, so the contract contradicts itself.

**Status at `9e5b62d`:** `docs/stop_actions.md:105-108` now records this as intended, and
`test/stop_action_test.cpp:619-634` pins refusal of ordinary work. That refusal stays.

**Fix (DEC-2, W6):** add the separate best-effort path. Change `31:24`, `31:30` and `45:19` to
this contract and add the regression: lost trigger reply, stop admitted and transmitted,
identical late reply stays unqualified.

### C-02 High. Urgent capacity does not give a usable stop. CONFIRMED, reproduced, lead checked

**Where:** `examples/common/CanBusOwner.cpp:134`, `:182-192`, `:196`, `:281-284`.

Each of these was reproduced against the real owner:

| Situation | Result | Conflict |
| --- | --- | --- |
| Write in flight, stop wants its own deadline | INVALID: `beginOperation` refuses while work is live, and `:189` forces the sequence deadline | `31:23` "its own urgent work with a finite deadline" |
| Move deadline already past | EXPIRED | Stop inherits a deadline it should not have |
| In-flight request cancelled first | Quarantines the route, then INVALID | Local cancel blocks the physical stop |
| After `invalidate(seq)` while a terminal is retained | INVALID, new reservation refused | `31:25` requires retaining that terminal |
| Stop on axis 2 while axis 1's stop is unread | RESULTS_FULL, one urgent slot serves both | "Stop on another node" in `31:30` |
| Stop from a producer other than the owner of the reservation | No lookup by axis exists | Console stop cannot find the sequence |
| Older queued work on the same sequence | Blocks urgent work until each item is cancelled | `:281-284` |

**Fix (W6):** steps 1 and 2. Prompt 31's procedure works around the first and sixth rows only
for queued work; C-17 covers in-flight work.

### C-03 High. Single-shot TX turns arbitration loss into permanent quarantine. CONFIRMED path, PLAUSIBLE hardware trigger, lead checked

**Where:** `adapters/twai/IdfTwaiDriver.cpp:78`, `:195`; `examples/common/CanBusOwner.cpp:409-412`;
`adapters/canopennode/SdoClient.cpp:269`. The setting is deliberate
(`docs/adapters/twai_sdk.md:45`), but its rationale never weighs arbitration.

**Problem:** with `fail_retry_cnt=0`, a frame that loses arbitration or takes one error
frame reports `is_tx_success=false`, which leads to `transportFailure` and quarantine.
This is routine: the host's `0x601` loses arbitration to any TPDO (`0x18x..0x48x`),
EMCY (`0x08x`) or another drive's SDO reply (`0x58x`). The bench has two drives.

**Consequence:** the first collision ends all SDO service to that drive. A stop frame
can fail for the same reason.

**Fix (DEC-4, W3):** `recover` already retires a pending frame by deleting the node. Measure on
the bench: count failed TX under TPDO load (W11).

### C-04 High. A receive interrupt during TX promotion quarantines the route. CONFIRMED, reproduced, lead checked

**Where:** `adapters/twai/IdfTwaiDriver.cpp:137`, `adapters/twai/TwaiAdapter.cpp:142`, `:159-160`.

**Scenario:** `events()` returns `rxPending=false`; a heartbeat interrupt then sets
`count_`; `submit` refuses; the slot goes to REPORT with `transmitted=false`, giving
TRANSPORT_ERROR and quarantine. No frame ever left the controller.

**Fix:** `submit` returns SENT, DEFERRED or FAILED, and DEFERRED keeps the slot QUEUED.
Add a proved-unsent report flag so a withdrawn first frame cannot quarantine.

### C-05 High. Quarantine has no exit and a matched abort triggers it. CONFIRMED, lead checked

**Where:** `adapters/canopennode/SdoClient.cpp:269-270`, `:350`; pinned by
`test/sdo_transfer_test.cpp:308`; documented in `docs/canopen_services.md:294`.

**Problem:**

- One server abort, such as reading an absent object, ends all SDO service to that
  route. A matched abort has the same correlation premise as a success, which leaves
  the channel open.
- `recover` quarantines every channel, including an untouched axis (reproduced).
  A 16-frame RX overflow or bus-off therefore ends SDO service to both drives.
- `consecutiveFailures` can only reach 1 and DEGRADED cannot clear on a route. The doc
  claim that "a successful independent reconciliation service may clear the current
  streak" works only because `test/can_bus_health_test.cpp:209-210` rebuilds
  `SdoClient` in place under a live owner.

**Consequence:** on the bench, one abort during evidence reads ends the session until
an MCU reset. Discovery of an absent node burns one of only two channels permanently.

**Fix (DEC-3, W5).** Change `sdo_transfer_test.cpp:308` as a recorded contract change, not as a
weakened test.

### C-06 Medium. Diagnostic overflow quarantines the route. CONFIRMED by trace

`examples/common/DeviceEvidence.cpp:179-183`, `examples/common/CanBusOwner.h:252,256`.
`TRACE_CAPACITY` equals `RX_LIMIT` (8), so one turn with 8 RX frames plus a TX loses
trace entries. `evidenceLost` then cancels the transmitted SDO. **Fix:** on loss, let
the in-flight request settle and withdraw only the remainder (`:195-197` already do
this). Size the trace above per-turn output.

### C-07 Medium. A same-tick reply overtakes the TX-done report. CONFIRMED, reproduced

`adapters/twai/TwaiAdapter.cpp:114-134`, `examples/common/CanBusOwner.cpp:185-186`,
`:494-498`. The adapter consumes TX-done after `owner.service`. A normal success then
keeps `transmissionConfirmed=0`, and `admitUrgent` returns INHIBITED until the next
owner turn, so a console stop in that window is refused. **Fix:** split the adapter
into `pollEvents` (before the owner) and `promote` (after).

### C-08 Medium. The production scheduler lives in `examples/`. CONFIRMED

`examples/common/CanBusOwner.*` (namespace `MotorControlCANExample`),
`adapters/twai/TwaiAdapter.h:6`, `examples/common/CMakeLists.txt:4`,
`examples/common/firmware_sources.json:3`. Adapters depend on examples, and firmware
compiles the stack from `experiments/.../vendor`. A real user must copy about 3000
lines across three trees, and each later prompt raises the cost of moving it.
Keeping it out of the portable core is correct.

**Fix (DEC-5, W2).**

### C-09 Medium. The owner's TX path is SDO-only. CONFIRMED

`examples/common/CanBusOwner.h:180,192,461`. `Transport.transmit`, `TxReport`,
`ExternalTx` and adapter slots all require `SdoTxId`, and `examples/common` has no NMT
call, so prompts 38/41 would need a parallel TX path. `TwaiAdapter::transport()` also
leaves `beforeTransmit` null, so firmware downloads skip the pre-TX settings notice
required by `docs/can_owner.md:290`. **Fix:** a `TxToken{kind, sdoId or nmtId}`, an
owner `admitNmt`, and a separate context for `beforeTransmit`.

### C-10 Medium. ProbeApp holds one operation at a time. CONFIRMED shape, PLAUSIBLE cost

`examples/common/ProbeApp.h:126-132`, `examples/common/ProbeApp.cpp:40-45,104-106`.
The owner itself is operation-agnostic: new motion types need no owner change. The
sprawl risk is in `ProbeApp`, with one `read_`, one `pending_` and a reservation tied
to `releaseRead`. Prompt 42 needs concurrent operation, stop and result handling.
`DeviceEvidence.cpp:40-56,92-97` duplicates kind ternaries and flattens INHIBITED and
EXPIRED into INVALIDATED. **Fix, no framework:** `AxisSession{binding, sequence,
pending, urgent}`, one concrete context member per operation, one
`admitPrepared(session, transfer, now)`. Each operation then costs one member, one
`request...`, one service branch and one `advance...`. Do this in W6 step 8.

### C-16 Critical. A running move cannot be stopped. CONFIRMED by trace, lead checked

**Where:** `src/profiles/lichuan_c/Actions.cpp:115,329`, `docs/stop_actions.md:22`; contradicted by
`33:24,30`.

**Sequence:** enable leaves latest `000F`; the trigger `001F` is acknowledged, so latest is `001F`;
the operator requests stop; `prepareStop` returns INVALID_CONFIG/STATE with no traffic. Mode 3 and
mode 6 also reject (`:334`), although ZH86 p13 runs velocity on word 15 and EN86 p15 documents the
homing interrupt. No test traverses `001F`: `runningPollingAndDeadline` manufactures running after
`000F`. Rearming to `000F` mid-curve to regain the predecessor is not a safe answer, because the
falling edge in mode 1 is undocumented.

**Fix (DEC-8, W6).**

### C-17 High. The documented stop procedure destroys the route. CONFIRMED by trace

**Where:** `docs/stop_actions.md:85-86`, `adapters/canopennode/SdoClient.cpp:269-270`,
`examples/common/CanBusOwner.cpp:100,134,179-181`.

**Problem:** step 2 says to cancel every pending request. A move must poll, and a handed-off poll that is
cancelled sets `unresolved`. Then `sequence()` is null, `beginOperation` is false, `admitUrgent` is
INVALID and `beginSequence` is false, while `nextAction` still yields ready work. Whether a stop reaches
the drive depends on whether a read happened to be in flight.

**Fix (W6 steps 2, 3 and 9):** cancel queued work only; let in-flight work settle.

### C-18 Medium. A stop expires while the drive idles. CONFIRMED

`src/profiles/lichuan_c/Actions.cpp:119`, pinned at `test/stop_action_test.cpp:812-821`. The one
`maximumAgeUs` also bounds the age of the `000F` acknowledgement, so a drive idle at `000F` longer than the
read-freshness bound cannot be stopped even with fresh reads. Raising the bound weakens read freshness.
**Fix (W6 step 5).**

### C-19 Medium. A copied `000F` stays eligible. CONFIRMED

`include/MotorControlCAN/profiles/lichuan_c/Actions.h:9`. The header says the application proves there was
no intervening write, but nothing enforces it. Passing a retained `000F` after a `001F` returns OK within
the age bound. Removing the age term (C-18) removes the only accidental limit, so the two go together.
**Fix (W6 step 8):** the application session records the latest `6040` terminal.

### Lower-priority owner items

| ID | Where | Defect | Fix |
| --- | --- | --- | --- |
| C-11 | `test/can_bus_owner_test.cpp:78-288`, `can_bus_recovery_test.cpp:65-282`, `can_bus_health_test.cpp:72-260` | About 750 lines of near-copied `Fake`/`Rig`; a new `Transport` callback edits every suite | One `test/support/OwnerRig.h`; no DSL |
| C-12 | `CanBusOwner.cpp:80-84`/`:102-106`, `:153-157`/`:163-167`; `overlap()` in 12 files | Peer scan, admission stats and helper duplicated; `CanBusOwner.h` mixes about 240 lines of value types with the class | `CanBusTypes.h`, one `peerResolved()`, a small scope guard for `entered_`, one private `overlap` |
| C-13 | `examples/common/FirmwareProbe.cpp:27-37`, `EspProbeMain.cpp:44-48` | Any app or evidence rejection reports "clock fault" and shuts the controller down | Rename to `progressionFailed` and report the real reason |
| C-14 | `adapters/twai/IdfTwaiDriver.cpp:217` | PLAUSIBLE: a legal Classical DLC 9..15 counts as `rxDropped`, which inhibits and quarantines | Accept it with an 8-byte payload |
| C-15 | `examples/common/EspProbeMain.h:20-21` | PLAUSIBLE: 8 KB owner stack is unmeasured; host frames reach about 6.5 KB (`prepareDriverSettings` plus `prepare`), and S2 runs this too | Measure the high-water mark on target before prompt 33; see A-11 |

## 6. Core API and code defects

### A-01 High. Native velocity output is the wrong unit for this drive. CONFIRMED, probe, lead checked

**Where:** `src/Axis.cpp:514`, `include/MotorControlCAN/Axis.h:236-241`,
`docs/axis_contract.md:133-141`. RS differs: `MotorControl-RS/src/Axis.cpp:609-611`.

**Problem:** `prepareVelocityTarget` only produces command increments per time, while
every Lichuan rate (`60FF`, `6081`, `6099:01/:02`) is rpm. With 3200 increments per
motor turn, gear 5 and request 1 load turn/min, the probe returned
`effectiveNative=16000`, where `60FF` needs 5.

**Consequence:** writing `effectiveNative` commands a speed wrong by the steps-per-turn
factor. Converting afterward quantizes twice and applies the allowance in the wrong
unit. There is no public motor-turn unit and no public path from `NativeQuantity` to
`Rational`. CAN NATIVE accepts only STEPS (`src/Axis.cpp:78-79`), so RS's
`velocity 60 rpm native` grammar is rejected.

**Fix:** add an `AxisConfig` native-rate basis, `COMMAND_INCREMENTS` or `MOTOR_TURNS`,
next to `nativeVelocityTime`, and select the destination from it. Prompts 33, 35 and
36 then share one rate path and add unsigned and width checks on top. Add to 33, 35
and 42: "Never write `effectiveNative` to an rpm object until the basis is rpm.
Quantize once and report error in rpm. Define `native` for CAN velocity in the grammar."

### A-02 High. A move invalidates its own prepared target. CONFIRMED by trace, lead checked

**Where:** `src/profiles/lichuan_c/Settings.cpp:329-330`, `src/Axis.cpp:479`.

`noteDriverTransmission` increments `configurationGeneration` even for `6081/6083/6084`,
which do not change coordinate interpretation. So `preparePosition` at generation A,
then a settings write, then `validatePreparedTarget` returns STALE_GENERATION. Homing
hits the same trap when a loss or `setAxisOrigin` advances the generation frozen in
its context.

**Fix (section 3, W8):** a move's own parameter writes do not go through settings. Prompts
`33:24` and `34:24` must state this. Homing follows the one-expected-advance pattern that
prompt 32 established.

### A-03 High. The status decoder cannot be used by motion. CONFIRMED, lead checked

**Where:** `src/profiles/lichuan_c/State.cpp:17`, `:62`.

`interpretStatus` is private and reachable only through `getStateInterpretation`, which
needs SDO STATUS and MONITORS blocks. TPDO contexts always fail, because the call to
`getStateBlock` passes no PDO record. `advanceMove`, `advanceVelocity` and `advanceHome`
must judge bits 8 and 15 and alarm from a TPDO1 or lone `6041` sample, so they would
copy the masks. **Fix:** expose
`Status interpretStatusword(uint16_t, StatusLayout, StatusIndications&)` and have
`getStateInterpretation` call it.

### A-04 High. The pre-trigger proof is private and the reservation is misnamed. CONFIRMED, partly resolved

`src/profiles/lichuan_c/Settings.cpp:171-191`, `docs/drive_actions.md:76-84`. Prompt 31 added
`ActionContext`; `MoveOperation.h` still does not exist. The only proof of an enabled position
state is private to settings, and a move needs exactly that proof. Actions now also use
`SettingsReservation`. **Fix (W8):** move the proof into a shared private helper, rename the type to
`AxisReservation`, and state that moves need caller-supplied `6040=15` evidence until the enable
gate closes.

### A-05 Medium. Wrapped requests skip the relative-basis policy. CONFIRMED, probe, lead checked

`src/Axis.cpp:422`. With an ACTUAL-only policy, a wrapped request using a QUEUED
witness returned OK, while the same witness on a relative request returned
UNSUPPORTED_BASIS. Inherited from RS. `test/axis_test.cpp` runs wrapped requests only
with policy 7. **Fix:** test `(r.relative || r.wrapped)`, add a regression, and report
the bug to the RS owner rather than editing the sibling.

### A-06 Medium. `pdoTemplate` returns a null message. CONFIRMED, probe, lead checked

`include/MotorControlCAN/profiles/lichuan_c/Pdo.h:61`, generated by
`scripts/generate_lichuan_objects.py:377`. This breaks the `Status.msg` static-text
contract, and printing `status.msg` with `%s` (`examples/reads_preview/main.cpp:15`) is
undefined. **Fix:** change the generator and regenerate. Do not hand-edit the header.

### A-07 Medium. `CORRELATION_LOST` leaves earlier blocks current. Behavior CONFIRMED, verdict PLAUSIBLE

`src/profiles/lichuan_c/Reads.cpp:456`. After ERRORS completes and STATUS ends in
CORRELATION_LOST, `getStateBlock(ERRORS)` still returns OK, although that reply can no
longer be assumed fresh. Keeping a prior SUCCESS is deliberate at the service layer, but the
getter's freshness claim is not. **Fix (section 3):** treat it like BUS_OFF. Add the missing test.

### A-08 Medium. A reply timeout is reported as the overall deadline. CONFIRMED

`src/profiles/lichuan_c/Reads.cpp:337`, `Settings.cpp:145`, `include/MotorControlCAN/ReadOperation.h:24`;
pinned at `test/reads_test.cpp:705`. A response timeout at t=10 with deadline 100 gives
outcome DEADLINE, documented as "Immutable overall cutoff reached". Prompt 33 must tell
a reply lost after trigger from an exhausted budget. Prompt 31 copied it:
`src/profiles/lichuan_c/Actions.cpp:162-163` maps SDO TIMEOUT to `ActionOutcome::DEADLINE`.
**Fix (section 3, W8):** add a distinct service-timeout outcome to both, before `MoveOutcome` copies it.

### A-09 Medium. A soft-limit overrun cannot be recovered through a witness. CONFIRMED, probe

`src/Axis.cpp:429`, documented in `docs/axis_contract.md:66`, shared with RS. With
limits 0..1000 and witness 1005, relative -10 (endpoint 995) returns LIMIT, and so does
absolute 995 with that witness. `ReferencePolicy::REQUIRED` and wrapped moves can never
recover. **Fix (DEC-6, W8).**

### A-10 Medium. Evidence checks are hand-copied. CONFIRMED

`src/profiles/lichuan_c/Reads.cpp:232,328,441-451`, `Settings.cpp:125,141,383-392`;
`overlap` in several `.cpp` files with two formulas (`src/canopen/Sdo.cpp:13` differs);
`gcd`/`magnitude` in `src/Units.cpp` and `src/Axis.cpp`. Prompt 31 added the third copy
(`src/profiles/lichuan_c/Actions.cpp:14-17,106-120,142-157`). Move, Velocity and Home would add
three more copies of the check that decides whether an effect is uncertain.
**Fix (W8):** one private header, `src/canopen/SdoEvidence.h`, with free functions
`sameSdoWork`, `checkUploadTerminal`, `checkDownloadTerminal` and `overlaps`. This is
not an engine, so it fits `MAINTAINABILITY.md`.

### A-11 High. Action frames exceed the firmware task stack. CONFIRMED by compile measurement, lead measured

`include/MotorControlCAN/profiles/lichuan_c/Actions.h`, `examples/common/EspProbeMain.h:20-21`. Atomic
publication through candidate copies puts whole contexts on the stack. Own frames from `-Os -fstack-usage`,
with the PlatformIO `xtensa-esp32s2` and `xtensa-esp32s3` toolchains (GCC 8.4.0, compile only, identical on
both):

| Function | Own frame, bytes |
| --- | --- |
| `prepareStop` | 8,624 |
| `advanceAction` (evidence) | 7,952 |
| `noteActionTransmission` (axis) | 7,920 |
| `prepareClearAlarm`, `prepareSetDevicePosition` | 7,584 |
| `advanceAction` (local reason) | 7,552 |
| `prepareDriverSettings` | 4,048 |
| `advanceRead` | 2,384 |

`FIRMWARE_STACK_BYTES` is 8,192. `sizeof(ActionContext)` is 7,472 on this target, `ReadContext` 1,936,
`DriverSettingsContext` 2,416. `prepareStop` exceeds the task stack alone, and `advanceAction` plus nested
`advanceRead` exceeds it too. No firmware calls these yet, so this has not failed: it would first appear on
the bench, as random faults at the moment of a stop. Unmeasured: the pinned IDF 5.5.5 toolchain is newer and
frames may differ, but the frame contains the 7,472-byte context.
`docs/fault_reference_actions.md` already warns of this without measuring it.

**Fix (W7).**

### A-12 Medium. Read plans are re-derived by hand. CONFIRMED

`src/profiles/lichuan_c/Reads.cpp:66-108,150-197,679-724,848-863,920-935`,
`Settings.cpp:37-68,413-417`. `DriverGroup` and `ReadKind` are parallel enums, and six
getters repeat the same preamble. Adding a group for 38-39 takes about six coordinated
edits. `getTuning`/`getIo` live in `Reads.cpp` while `prepareControlSettings` is in
`Settings.cpp`. **Fix:** one private table `{DriverGroup, ReadKind, Address*, count}`, a
`beginGet` helper, and `DriverReads.cpp` plus `Tuning.cpp`.

### Lower-priority core items

| ID | Where | Defect | Fix |
| --- | --- | --- | --- |
| A-13 | `src/Axis.cpp:93-97`/`:509-512`, `:317-331`/`:460-474` | Polarity workaround and endpoint/path tail duplicated; prompt 34 depends on this logic | Extract `finishPrepared` and `mechanicalUnits`; format as a separate mechanical commit |
| A-14 | `src/Axis.cpp:170,449`, `Axis.h:145` | `34:23` requires retaining the chosen path, but `nativePositive` is discarded | Add `bool pathPositive` to `PreparedTarget` |
| A-15 | `Axis.h:192` | Comment "only this function advances epochs" is false | Reword |
| A-16 | `MotorControlCAN.h:7-13` | Umbrella header says "implemented portable APIs" but omits five headers | Complete it or narrow the comment |
| A-17 | `Types.h:62,89`, `Reads.h:231` | `ObjectDescriptor.ownerPrompt` and "Prompt 19/20/21/22" in Doxygen ship process numbers in the API | Keep them in the ledger; remove by prompt 49, with `test/objects_test.cpp:21` |
| A-18 | `src/canopen/Pdo.cpp:251`, `Settings.cpp:308,360` | `pdoFreshness` declared in `PdoState.h` but defined in `Pdo.cpp`; magic readback offset 9 | Named constant; move the function |
| A-20 | `src/profiles/lichuan_c/Actions.cpp:221` | A `maximumAgeUs` shorter than one cycle fails an acknowledged action as OBSERVATION_SCOPE on its first complete cycle, and `running` is never evaluated. No test reaches it | Reject at preparation (W6 step 6) |
| A-19 | `src/canopen/Pdo.cpp:234` | PLAUSIBLE: message calls transmission types 252/253 "reserved"; CiA 301 defines them as RTR-only (from memory, no retained copy) | Reword; rejecting them is still correct |

## 7. Dependency graph and shared contracts

### G-01 Critical. Prompts 49-50 can never close. CONFIRMED

**Where:** `38:3`, `README.md:63`, `SEQUENCE.json:497-501`; contradicted by
`README.md:81` and `COMMON.md:38`.

Prompt 38 is the only direct dependent of Blocked 18, and 39, 41-50 descend from it.
A second chain runs 30 to 31-36 to 42 to 43-50. COMMON forbids marking a blocked
predecessor complete, so prompts 47 and 48 send the executor to prompts it cannot finish.

**Fix:** remove 18 from 38 in all three places. Prompt 38 then covers node and heartbeat
rows only. Change `38:7` to: "PDO remapping remains prompt 18; integrate through this
workflow when 18 resumes." Add a guard near `scripts/check_promptset.py:349-359` that 18
is not an ancestor of 49. G-02 handles the motion chain.

### G-02 Critical. Three different rules for when a prompt may start. CONFIRMED

**Where:** `README.md:22,79`, `COMMON.md:36,38`, `PROGRESS.md:112,145`.

`README.md:79` says "only when its prerequisites are satisfied", `COMMON.md:36` says
"coherent independent chunks of a later prompt may proceed", and prompts 20-30 ran on
Partial predecessors. Partial also means both "my work is done" and "my work is
missing", so one gate marks every later prompt Partial forever.

**Fix.** Replace those sentences with:

> A prerequisite is usable when the specific artifact consumed exists and its checks
> passed, regardless of that prompt's state. Start when the consumed artifacts are
> usable and name them in the report. If an artifact is missing, the dependent feature
> is Blocked. Complete means the prompt's owned scope and checks closed. Record
> predecessor gates in an Inherited gates column; they do not change the state.

Add that column to the `PROGRESS.md` table and re-evaluate prompts 23-29.

**Blockage under the new rule:**

| Prompt | Can proceed today | Real gate |
| --- | --- | --- |
| 31 | Delivered Partial; W6 extends it | Q02, Q12 |
| 32 | Delivered Partial | Q13, Q14 |
| 33 | Pure preparation now; finite phases after W1 to W8 | Q02, Q04, Q05, Q10, Q11 |
| 34 | After 33 | Q04, Q14 |
| 35 | Preparation and timing only | Q03 |
| 36 | Preparation, state machine with fakes | Q21, Q02, Q05, Q10, Q14 |
| 37 | Inventory, capabilities, host selection, passive discovery; `prepareProbe` after W4 and W5 | Q01 |
| 38 | Node/heartbeat read rows; writes gated | Q16, Q18 |
| 39 | Typed save and factory restore | Q15 |
| 40 | **Yes, the best next prompt** | None for local routes |
| 41 | Routes for APIs that exist, after 40 | Follows those APIs |
| 42 | Platform parity; motion routes follow the APIs | Motion APIs |
| 43 | **Yes, after 40**: needs only the schema, so change its prerequisite to 40 | None offline |
| 44 | Fake campaigns | Motion routes |
| 45 | Owner, read and settings traces now; the rest as APIs arrive | Motion APIs |
| 46 | Items 1-5 are mostly independent | X-01; X-02 settled by DEC-7 |
| 47 | No | W9, fixture facts, bench authorization |
| 48 | No | 47 |
| 49, 50 | Scoped software candidate, with gated features listed | G-09 |

### G-03 High. Evidence arrives after the work that needs it. PLAUSIBLE as stated, structure CONFIRMED

`47:3,21`. Prompt 47 resolves source conflicts, yet it requires 44-46 and so the motion
prompts waiting on those same gates. Only prompt 29's experiments break the loop
(`COMMON.md:38`), and nothing in 31-36 says so. An executor can wait at 31-36 for
evidence 47 will never supply in time. Hardware now exists: an S2 board and two CL86
drives, with board testing authorized (`docs/hardware_bench.md`).

**Fix:** add to 31-36: "Evidence route: prompt 29 conditional experiment or manufacturer
clarification; do not wait for prompt 47." Reduce 47's prerequisites to 29, 44, 45
(prompt 46 has no physical relevance), and make clear that 47 qualifies a finished
feature rather than discovering how the drive works.

### G-04 High. The real fixture has no firmware owner. CONFIRMED

`47:19`, `42:23`, `docs/hardware_bench.md:21`, `docs/firmware_examples.md:7-13`,
`docs/backlog.md:71`, intake `docs/reports/2026-11-02_s2_bench_intake.md`.

The board is an ESP32-S2. Complete firmware is S3 only: `IdfTwaiDriver.cpp` has an S3
retirement guard, `EspProbePlatform.cpp` uses S3 USB, and `EspProbeMain` needs two cores.
There is also no GPIO18 silent control, so normal mode ACKs frames. No prompt 40-50
mentions S2, and acquisition cannot run on the real board.

**Fix:** record backlog T04 as an explicit reopened 27/28 assignment, named in 29's
resume conditions and at `47:3`. It covers single-core operation, UART console, S2 TWAI
retirement evidence, silent control and an S2 compile fixture. Flash and memory facts
are in the bench record (4 MB flash, no PSRAM).

### G-05 Resolved at `1cb0d54`

Prompt 31 created `ActionOperation.h` and `profiles/lichuan_c/Actions.h` with the lifecycle, and stop
as the first real action. Prompt 30 later adds enable and release to the same context. No action needed.

### G-06 High. Prompt 37's scan has no qualified read and burns channels. CONFIRMED

`37:21-22,30`, `Reads.h:191-194`, `src/profiles/lichuan_c/Reads.cpp:290`,
`docs/canopen_services.md:131,151,159-163,294,306-308`.

No device read qualified under Q20 and `prepareProbe` always rejected. There are two
channels; any unsuccessful transmitted request left a channel unresolved, and an
unconfirmed route admitted only a reviewed non-changing upload. So each absent node burned
one of two slots permanently. W4 and W5 remove all three premises.

**Fix:** 37 delivers inventory, capabilities, host selection and passive heartbeat and
boot-up discovery. SDO discovery follows W4 and W5, with a written slot and timeout design before
implementation: a probe timeout still leaves that slot unresolved until a drain marker, so a scan
over absent nodes must use a free slot and report what it consumed. Replace `37:22` with: "Reads are
typed uploads with no write intent. `prepareProbe` uses `1000:00` under DEC-1, whose effect stays
recorded as unknown. Identity discovery runs only on explicit request." Change `37:36` to document
"issues no download or NMT" rather than "no change".

Scope (`R-07`): installed code holds the inventory, `prepareProbe` and `checkProbe`; the
scan loop and retained findings live in `examples/common`, as RS does.

### G-07 High. 101 read rows have no real owner. CONFIRMED, lead checked

`read_owner_prompt=38` holds 101 ledger rows (`docs/reports/prompt_19.md:44`,
`docs/coverage.md:104`). They include `605D`, `607A`, `607C`, `6098`, `6099`, `609A`,
`60FF`, `2203`, `2300..2302` and all PDO records. Prompt 38's body says only "Earlier
read coverage remains intact". Prompts 33, 35 and 36 therefore lack readback of their
own objects.

**Fix:** reassign the 13 non-PDO rows to their write owners in the ledger (`2203` to 38,
`2300` to 39, the rest to 31, 32, 33, 35, 36) and regenerate. State the remaining 88 PDO
read rows as an explicit deliverable of 38 that does not depend on prompt 18.

### G-08 High. CLI routes without owners, and a conflicting console location. CONFIRMED

**Unassigned routes.** `docs/api_cli_mapping.md:139-160` lists `config`, `host`,
`wiring`, `useaddr`, `memory`, `speed`, `accel`, `decel`, `motion` and `stepsperturn`;
no prompt 40-42 assigns them. A console without target selection cannot reach
`read identity`. RS `moveby VALUE [unit]` has no speed or ramp argument
(`MotorControl-RS/examples/probe_cli/ProbeConsole.cpp:49-54`), and CAN forbids the boot
defaults it relies on (`api_cli_mapping.md:146`). `motion-profile restore/forget` has no
owning API prompt.

**Visibility conflict.** `40:16` says unimplemented routes "stay out of working
help/dispatch"; `api_cli_mapping.md:160` and `42:7` say gated capabilities remain
visible. `40:16` also uses `profile list` and `caps`, which need prompt 37's
`getDiscovery*`, and 37 is not an ancestor of 40.

**Fix:** add an owner column to the mapping. Give `useaddr`, `config` and `host` to 40
and simple-move intent to 42. An unset speed, ramp or scale rejects; `moveto` needs an
explicit `axis origin`. Define two inventory states: implemented, and gated (shown in
`caps` with a reason, absent from dispatch). Prompt 40 ships a compiled-profile inventory
without 37. For location see R-04.

### Other graph and contract findings

| ID | Severity | Where | Problem | Fix |
| --- | --- | --- | --- | --- |
| G-09 | Medium | `49:21`, `45:21`, `36:26` | Exits unreachable while gates are open ("read/move/stop example"; "after 18/20-22 resume"); returning Partial prompts to Pending erases their state | 49: "implemented operations; for gated ones verify the rejection and documented gate". 45: conditional, list the remainder. 36: "keep that prompt Partial and list the row" |
| G-10 | Medium | `36:26`, `46:20`, `49:24`, `50:24`; `docs/architecture_plan.md:56` | Coverage reconciliation demanded four times; `check_lichuan_operations.py` has no owner; 22 and 34 are not ancestors of 36; `607C` is assigned to 32, never mentioned there, used by 36 | Limit 36 to direct rows; 46 creates the checker, and 36, 49, 50 run it; name `607C` in 32 or move it to 36 |
| G-11 | Medium | `46:24`, `49:25`, `CMakeLists.txt`, `docs/architecture.md:35` | "Core-only and selected-Lichuan consumers" and "selected-profile options" do not exist: one library, Lichuan always included | Redefine as a consumer including only core headers, or assign a real CMake option. Correct `architecture.md`. Tell 33-39 that a new header needs the header lists in `CMakeLists.txt` and `test/package_test.py` |
| G-12 | Medium | `43:32`, `44:32`, `37:38`, `38:38`, `39:38`, `DOCUMENTATION.md:34`, `docs/architecture_plan.md:66` | "Runner guide" has no path; one guide has three names; the plan names `lichuan_api_cli_coverage.md`, but the file is `docs/coverage.md` | Name `docs/runners.md` and `docs/commissioning.md`; correct the coverage name |
| G-13 | Medium | `31:26`, `33:24`, `docs/drive_actions.md:95-102`, `docs/reports/prompt_30.md:120` | Prompts only say "deterministic". Reads and prompts 31-32 already use an inclusive processing-time cutoff | Write that rule in `CONVENTIONS.md` section 3 and link it from 33-36 |
| G-14 | Medium | `MULTIDRIVE.md:51`, `45`, `46:18` | Row 45 requires absent ack/done fixtures; 45's body has none, and 46 does | Move the obligation to 46 |

## 8. Datasheet and device-fact findings

Pages are one-based PDF pages. EN86 = `86C_CANopen_user_manual_en.pdf`, ZH86 =
`86C_CANopen_user_manual_zh.pdf`, OP86 = `CL86C_OL86C_operating_manual_zh.pdf`. Each
"CONFIRMED" here means the document gap or conflict is verified; the drive's real
behavior is unknown.

### D-01 High. Prompt 33's phases are in the wrong order. CONFIRMED, lead checked

`33:23` lists "parameter setup, mode confirmation, trigger/rearm". EN86 p12, ZH86 p12
and OP86 p9 give: mode (`6060`, observe `6061`), then controlword 6, 7, 15, then
`607A/6081/6083/6084`, then the bit4 edge. `Settings.h:100` already requires `6040=0x000F`,
mode 1 and status low bits 7 before writing parameters, so the listed order would be
rejected. Prompt 34 inherits this.

**Replace with:** "Preconditions: mode 1 observed and drive explicitly enabled (prompt 30;
no implicit enable). Phases in documented order: confirm mode and enabled evidence, write
parameters, write the bit4 edge, confirm the protocol reply, then gather new acceptance
and completion evidence."

### D-02 High. No documented path from homing to a move. CONFIRMED

`33:23`, `35:22`, `36:17`, `47:22-23`. Every procedure (EN86 p12/13/15, ZH86 p14-15) sets
the mode before enabling. No source describes changing `6060` while enabled.
`docs/drive_actions.md:63-67` approves only a position-mode enable and has no release
write. So the ordinary workflow "home, then move" needs either a `6060` write on an
energized drive or a release, and both are undocumented. `prepareHome` would enable
implicitly (forbidden) or stall. An implementer may silently write `6060` while a target
may be retained.

**Add to 33, 35, 36:** "State the full transition. Enabling in mode 3 or 6 and changing
`6060` while enabled are separate gated actions with no documented sequence (Q11). Reject
before TX if the observed mode differs and the drive is enabled. Enable in mode 6 is an
explicit prompt-30-style action, not a homing side effect." Add a measurement to Q11 and
to 47 and name an owner for the mode-6 enable.

### D-03 High. Bit4 means different things by mode, and stops assume word 15. CONFIRMED

`36:17`, `33:23`. EN86 p15 and ZH86 p15: homing bit4 "1->0: Stopping homing". EN86 p12
documents only the rising edge in position mode and no rearm. Every stop row starts at 15
(ZH86 p12: `15->271`, `15->11`). So a position-style 31-then-15 rearm during homing
interrupts the home. While homing the word stays 31, so the documented stops do not apply,
and writing 15 first would interrupt. Whether bit4 falling aborts a position curve is
undocumented.

**Add to 36:** "Hold bit4=1 (word `0x001F`) until a terminal result. Write bit4=0 only as
the deliberate interrupt or after completion. Do not share the position rearm path. Bits
5 and 6 are undocumented in mode 6." **To 33:** "Falling-edge meaning in mode 1 is unspecified. Hold bit4 until the move is terminal,
then write `000F` as a visible step (DEC-8)." **To 31, 33, 36:**
"Stops from 31 or 95 are not documented arrows; use the DEC-8 table and carry
`predecessorDocumented=false`."

### D-04 High. Prompts assume reads Q20 forbids. CONFIRMED

`37:22`, `37:36`, `41:19`, `44:20`, `47:20` versus `Reads.cpp:290`,
`examples/common/DeviceEvidence.cpp:88-89`, `docs/hardware_bench.md:65-69`. Prompt 47's
first step ("Start with passive/read-only identity/configuration") is rejected by the
harness it must use. **Replace `47:20` with:** "First a listen-only capture. The first
active upload is a DEC-1 accepted read." `41:19`: `probe` uses the same list, and other objects return
the gate error. `44:20` states the same.

### D-05 High. All communication objects except PDO mappings are EDS-only. CONFIRMED, lead checked

`38:13,17,21,24`, `37:17`. The EN86 pp15-16 table lists only `1000`, `1009`, `100A`,
`1600-1603` and `1A00-1A03`. `1005`, `1014`, `1017`, `1018`, `1200`, `1400-1403` and
`1800-1803` appear only in the EDS. "NMT", "heartbeat" and pre-operational appear in no
retained manual, and EN86 p3 lists only SDO, PDO and EMCY. So the prompt's "using original
manuals/EDS" and "exact NMT prerequisites" send the coder after rows that do not exist, and
CiA 301 behavior would be imported as a vendor fact. Boot NMT state is undocumented, so it
is unknown whether TPDOs flow or default RPDO1 is live after power-up.

**Add to 38:17:** "All `1005/1014/1017/1200`, `1400`-series and `1800`-series rows and
`1018` are EDS-only. No retained vendor document mentions NMT, boot state, heartbeat
behavior or remap conditions. Label standard behavior 'CiA 301, not vendor-confirmed'.
Per-row restart and persistence are `unknown`."

### Medium device findings

| ID | Where | Problem and evidence | Fix |
| --- | --- | --- | --- |
| D-06 | `38:17,22,23` | No object or EN86 pp15-20 entry reports DIP position or bitrate. EN86 p10 says only "User-defined"; the link from all-OFF to `2203` is an inference. EN86 p18 gives "CAN ID 0~127" with no activation text. No source offers a software bitrate route | "No object exposes DIP state; all-OFF is a caller attestation. `2203` readback is a stored setting, not the active node; only a reply on the node+0x580 route evidences that. Bitrate is DIP-only in every source: remove it from `38:23` as a software change and keep it as a bench step." |
| D-07 | `39:26`, `47:15`, `LICHUAN_OBJECT_FACTS.md:155`, `docs/hardware_bench.md:59`, Q19 | The manufacturer's own product page says DC30-110 V / AC20-80 V. EN86/ZH86 p3 say 36-110 VDC, 18-80 VAC. EN86 pp6-7 say 24-50 VDC. The record frames this as a conflict within EN86 and an Alibaba title | Record the three-way conflict. Reword `39:26`: "the listing title and manufacturer page agree with each other but not with the manuals; use the nameplate" |
| D-08 | `41:15`, `LICHUAN_OBJECT_FACTS.md:103,17`, `docs/device_conflicts.md` Q02/Q06 | OP86 is the OP57 body with an 86C cover (normalized text differs only in title, one TOC number, spacing; lead checked). The `60FE` bits16..18 sentence appears only in OP86 p7, OP57 and ZH57 p8, and covers only DOs with user-defined functions 9..11. OP86 agreement in status and DO tables looks like independent corroboration but is not | `41:15`: "`60FE` bits16..18 is a 57C-body claim with no 86C CANopen-manual support; it applies only to DOs assigned user-defined functions." Note in the facts file and C18: "OP86 body is identical to OP57; do not count it as independent 86C evidence" |
| D-09 | `47:19`, `48:21`, `39:23` | OP86/OP57 p5 note 4: the motor locks on power-up (lead checked). Absent from EN86/ZH86, the ledger and the executing prompts. Energizing may apply holding current with no CAN traffic, and "no implicit enabling" cannot prevent it | "57C-body manual says the motor locks on power-up; applicability unconfirmed. Treat every power-up or restart as a possible torque state change, observe it independently, and record `2100`/`6041` before any command." |
| D-10 | `39:7,21,23-25,30`, `41:15,21`, `48:21` | "Reset" collides with NMT Reset Node and power cycle. "Restore" means both vendor value 1 and host re-application of writes. EN86 p18: "1: Restoring factory settings". No source defines a restart object | Three terms: `save parameters` (`2300=2`), `restore factory settings` (`2300=1`), `re-apply recorded configuration` (host writes). "No vendor restart command exists. Restart means a user power cycle, or a separately gated NMT service whose parameter-reload effect is unknown." See R-05 |
| D-11 | `39:17,22`, `38:22`, `48:21` | EN86/ZH86 p15 and OP86 p13 say only that writing 2 saves current parameters. 重启 (restart) appears only in the LED tables. `2302` says "manually set to 0"; `2300` has no such text. A boolean restart field forces invention, and copying the `2302` pattern adds a hidden `2300=0` write | "Restart requirement, saved set and rearm are tri-state, currently `unknown`. Whether `1017`, `2203` or PDO maps are saved is undocumented. Do not write 0 after a save unless Q15 evidence requires it; if so, make it a visible step." `48:21`: "Count every save and cap the count in the campaign plan." |
| D-12 | `33:17,22`, `36:17,23` | Ranges 0..3000 rpm and 0..2000 ms include 0 with no stated meaning (EN86 p11/13), and Q11 gates zero ramps. A range check admits them, and a move at 0 rpm only times out | "The documented range is not the admission range. Reject `6083/6084/609A`=0 under Q11, and `6081/6099:01/6099:02`=0 for motion, before TX until zero semantics are evidenced." |
| D-13 | `35:21-22`, `33:25` | `6083/6084` are shared by modes 1 and 3 (EN86 p11/13, ZH86 p13). `prepareDriverSettings` requires mode 1 and has no `60FF/6098/6099/609A/607C` | 35, 36: "Own mode-specific parameter writes in the operation; do not route them through `prepareDriverSettings`." 33, 35: "Writing `6083/6084` in either mode invalidates the other mode's snapshot." |
| D-14 | `34:17`, `36:17`, `47:22-23` | `2200` is the documented direction selector (EN86/ZH86 p18; `lichuan_review.md:79` names owners 20 and 34), yet no numbered prompt mentions it. A flipped `2200` sends method 17 away from its switch | "Capture raw `2200` in the configuration generation. Direction sign relative to `2200` and the shaft is measured evidence (Q04); a change invalidates prepared work and the reference." |
| D-15 | `36:23` | "Physical prerequisite evidence where needed" names nothing. EN86 p8 gives DI0/1/2 defaults origin/positive/negative limit; the EDS defaults them to 0. `2400:01` polarity is on EN86 p18, logical state `2101` on p17 | "Before admission read `2400:01..08` and `2101`. Require code 1 for methods 24/29, 3 for 17, 2 for 18. Reject when absent, without writing. Starting on an active switch and meeting a limit during an origin search are undocumented, so they are gated. `60FD` is not `2101`." |
| D-16 | `33:17,23` | "Interpreted under the qualified status dialect" implies bit15 varies. All sources agree bit15 is position-mode in-place and bit8 is homing complete; the dispute covers alarm and motion bits. Bit12 exists only in EN86 p12, so ZH86/OP86 layouts have no status acceptance bit. `250B/250C` set the completion threshold and time (EN86 p19) | "All sources define bit15 as position-mode in-place, relevant in mode 1 only. Its deassert behavior, `250B/250C` dependence and soft-limit stop behavior are unqualified. Bit12 is EN86-only; without it, acceptance needs another evidenced signal or the move stays gated. EN86 bit10 is zero speed, not target reached." |
| D-17 | `47:22`, `42:20` | "Qualify explicit enable/release and stop before dependent movement", but no manual gives a release command. `docs/drive_actions.md` says "No approved release write"; `2100` bit4 only observes. Qualification deadlocks, or someone tries `6040=0` | 47: "Qualify enable and stop before movement. Release is a separate gated question, not a movement prerequisite, unless a sourced command exists." 42: list release as visibly gated |

### Low device findings

| ID | Where | Problem | Fix |
| --- | --- | --- | --- |
| D-26 | `docs/fault_reference_actions.md:30`, `src/profiles/lichuan_c/Actions.cpp:295` | EN86 p16 "FF05 refers to too high position deviation" and p20 "Position out-of-tolerance alarm ... Resettable" are linked only by name: p20 has no codes and names no reset route. PLAUSIBLE: the guide also says `603F` may retain its value, so a stale FF05 could admit a clear for a later alarm | Label the link as an inference. Add the stale-value case to Q13 and W11 |
| D-27 | `docs/fault_reference_actions.md:23-28` | PLAUSIBLE: `2301` has no arming zero. If the final 0 was never sent and the object is edge-triggered (undocumented), a later 1 produces no edge | State it in the guide and add it to Q13; no code change without evidence |
| D-19 | `34:17` | "Substitute 4000 counts for the configured subdivision" reads as "scale = `2201`". No page states pulses per revolution equals `2201`, and the EDS default is 0 | "...for a confirmed command scale; `2201` is a candidate and needs Q04 evidence." Note ±1,000,000 pulses is about ±19.5 rev at 51200, so multi-turn absolute targets can be unrepresentable |
| D-20 | `36:17` | "The example" has no antecedent, and 5000 ms is not merely unexplained: it exceeds 0..2000 | Name `docs/reference/public_code/srt/SRT_OpenCan.cpp:146-152`; add wrong-width and out-of-range vectors |
| D-21 | `36:17` | `607C` is in the EN86 p13 table but not in p15's write list | "Write order, units, sign and effect on the final coordinate are unqualified (Q14, Q21)" |
| D-22 | `45:15` | Attributes `6061` writes and quick-stop objects to the public code; SRT uses neither. It omits real SRT defects: gear ratio multiplying ramp times (`:94-95`), 63/127 for ordinary moves (`:98,109`), status timeout returned as "not running" (`:131`) | Label each item by origin, SRT or manual conflict, and add these three |
| D-23 | `47:15` | No alternative to trial writes for the DO dispute. Registers `0x2B..0x2E` agree; with `2400:01..08 = 0x20..0x27` (register = sub + 0x1F), EDS subs `0C..0F` fit and EN86's `0B..0E` do not | Record as supporting evidence, not a resolution. In a reviewed read experiment, read `2400:00` and `:0F`, and compare `:0C/:0D` with the DO0 default |
| D-24 | `38:7,17` | Heartbeat unit: EDS `[1017]` gives none. The stack treats it as ms; inhibit and event units differ | "Native unit 1 ms per CiA 301 and public stacks, not vendor-confirmed; verify by measurement. Inhibit and event units are gated separately (Q17)." |
| D-25 | `38:21` | PLAUSIBLE: EDS `[1200sub1/2]` is RW, but a setter can sever the only service and recovery route (CiA 301 from memory) | "`1200` is read-only in typed workflows until vendor or firmware evidence supports route writes." |

## 9. MotorControl-RS parity findings

| ID | Severity | Where | Problem and evidence | Fix |
| --- | --- | --- | --- | --- |
| R-01 | High | `35:21` | "Requests specify duration, signed engineering intent, units, native ramp time, stop policy and a fixed service deadline." RS `VelocityRequest` holds only duration, value/unit and stop (`VelocityOperation.h:14-30`). Native ramps are in `VelocityPrerequisites` (`Velocity.h:16`). `deadlineUs` is a separate argument (`Velocity.h:68-71`). CAN's pure `VelocityRequest` already exists (`Axis.h:164-173`) | "Keep `Axis.h` `VelocityRequest` pure. Add a distinct finite-operation request or prerequisites type with `durationUs`, stop policy and verified ramps; pass `deadlineUs` as an argument." Also state: zero speed rejects (`Velocity.cpp:121`); a duration above `maxPolls * pollIntervalUs` rejects (`Velocity.cpp:95`); `serviceVelocity` runs only with no admitted transaction (`Velocity.h:72`); missed when later than `stopDueUs + pollIntervalUs` (`Velocity.cpp:55`) |
| R-02 | High | `44:19` | "`bench_repeat.py` for finite repetition of a named scenario". RS is a fixed comparison of position setup policies, 1..20 repeats (`scripts/bench_repeat.py:1-4,16-27,75`; lead checked). The prompt invites a generic loop over motion, settings or persistence | "`bench_repeat.py`: bounded comparison of position setup policies through ordinary moves, with one planned cleanup. It accepts no arbitrary scenario and never repeats save, restore or settings." |
| R-03 | Medium | `44:19` | "These import `Console` from `bench_probe.py` and interpretation from `_bench_evidence.py`." Only `bench_session.py:11` imports `Console`; only `bench_probe.py:27-28` imports evidence, which is private. Cleanup belongs to the scenario's `run(console, record)`, not the session | "`bench_session` owns `Console`, port and artifacts. Campaigns use `run_recorded` and `bench_probe` helpers. Evidence stays private behind `bench_probe`. Each scenario owns its planned cleanup." |
| R-04 | Medium | `40:15`, `docs/architecture_plan.md:44-45` | `examples/probe_cli/ProbeConsole.*` follows RS, but in CAN that directory is the Arduino entry (`src_dir`, `build_src_filter = +<main.cpp>`) and absent from `firmware_sources.json`. `docs/source_map.md:149-153` deliberately puts console code in `examples/common/`. Native and IDF would reach into the Arduino folder | "Create `examples/common/ProbeConsole.*` beside `ProbeApp.*`." Update `firmware_sources.json`, `examples/common/CMakeLists.txt:11`, `scripts/check_firmware.py:34`, `test/firmware_build_test.py:23` and REQUIRED in `scripts/check_repository.py`. Name `examples/native_probe` as the native entry |
| R-05 | Medium | `39:7,13,21,24`, `CONVENTIONS.md` section 3 | RS has exactly `prepareSave`, `prepareFactoryRestore`, `nextPersistence`, `advancePersistence`, `verifyPersistence` (`Persistence.h:104-122`) and no reset API. RS multi-write restoration is the application `motion-profile restore`. `verifyPersistence` is missing from the audit and CONVENTIONS | Name the five functions. Add the Communication and Persistence families to the CONVENTIONS table |
| R-06 | Medium | `api_cli_mapping.md:143,148,151`, `MOTORCONTROL_RS_AUDIT.md` | RS additions have no adopt-or-reject disposition: routes `velocity read`, `home read`, `home auxiliary`, `home qualification`, `motion-profile start-speed`, `position interrupt`; `prepareVelocityParameters`, `prepareHomingParameters`, `checkHomeQualification`, `revokeHome`, `PositionInterruption`. The audit is pinned 49 commits behind | Add a dated delta section to the RS audit and one mapping row per addition: adopt, or not applicable with the reason |
| R-07 | Medium, PLAUSIBLE | `37:21-22` | RS core has only inventory and a one-shot probe (`Discovery.h:31-34`); the scan is application code (`examples/probe_cli/DiscoveryScan.h`). `discoveryProfileCount`, `checkProbe`, `ProbeOutcome`, `ProbeConfidence` may be missed | See G-06 |
| R-08 | Medium | `34:24`, `33:25` | "Apply the existing zero-distance rule", but 33 only says "Define" it. `preparePosition` succeeds with `zeroDisplacement=true` in RS, and `prepareMoveAngle` rejects it (`Position.cpp:273`). CAN `PreparedTarget` has neither field, and 34 never names `validatePreparedTarget` | "Move preparation rejects zero effective displacement with no writes; pure arithmetic may report it. Use `validatePreparedTarget` at admission." |
| R-09 | Medium | `33:32`, `45:23` | Defaults differ: `PositionRequest.relative` true in RS, false in CAN; `VelocityRequest.unit` turns/minute in RS, steps/second in CAN; quantization allowance `double` in RS, `Rational` in CAN. A copied vector silently changes meaning | "Comparison vectors set every field explicitly; do not rely on defaults." |
| R-10 | Low | `43:23`, `44:21`, `scripts/verify.py` | RS `bench_probe.py` has no plan or self-test mode; `--list`/`--plan-only` are on campaign tools, with read-only default `--scenario quick`. Do not copy default `COM13` or address 1. RS `verify.py:67` enforces `*_test.py` registration, and CAN appears not to (PLAUSIBLE) | State the spellings and a read-only default scenario; add the registration check |
| R-11 | Low | `41:23` | "Trace" is not a route. RS and the mapping use `debug [off\|raw\|decoded]` and `load` | Use the real names |
| R-12 | Low | `40:23`, `CONVENTIONS.md:281-282`, `api_cli_mapping.md:158` | RS tests also reject `help advanced`, `profile ess_rs enable`, `stop direct`; `40:23` omits `profile ess_rs`; `motor` was never a removed alias | Extend the reject list; call `motor ...` "never-implemented grammar" |
| R-14 | Medium | RS `6ff2374`, `docs/console.md:196-199`, `docs/bus_owner.md:203-206`, `docs/ess_actions.md:140,165`; `docs/api_cli_mapping.md` | RS added behavior after the audited revision, and CAN has no adopt-or-reject row. RS's working tree is dirty with more qualification routes; give those no row until committed | **Adopt:** stop held across one bounded recovery and sent once; lost reply stays UNKNOWN, never resent; `stop fast` escalates `stop normal`; `cancel` and `monitor off` let in-flight frames settle (W6). **Compare for prompt 33:** `MoveContext::positionMismatch`; relative moves rejecting a reference that is not idle, stationary and ACTUAL; the `AxisConfig` note that native bounds also check relative displacement. **Compare for prompt 35:** R-01's duration limit. **Rule shape for prompt 36:** per-method input checks at admission and on each poll; ESS function numbers do not transfer. **Bench runner:** `cleanup_stop_attempts`, `write_rejected`, `home_inputs_permit`. **Not applicable:** ESS target magnitude limit and exception 05 mapping |
| R-13 | Low | RS `Velocity.h:23`, `docs/ess_velocity.md:50` | RS comments say the stop timer starts at "admission", but the code uses preparation time (`Velocity.cpp:144`). Prompt 35 is right | Warn in 35: "Trust the code; the RS comment is stale." Report it to the RS owner |

## 10. Sibling library and FieldCore references

FieldCore has **no** CAN contract. A search finds only two unrelated lines, `BusId` has no
CAN (`contracts/Health.h:112-120`), and its motor contract is RS485-addressed Move,
ReadState and UrgentStop (`contracts/MotorControl.h:15,54`), pinned to RS v1.0.0. It has
no homing, velocity, angle or drive-persistence operation.

**Replace the recurring audit rule** in `AGENTS.md`, `COMMON.md:9` and each prompt's
opening sentence. 50 of 50 prompts name ADS1115, and 39 of 42 reports cite the same INA228
HEAD with repeated rows, while RS moves constantly. `check_promptset.py` does not enforce
this sentence, so the edit is safe. Mark the checklist in `ECOSYSTEM_CONVENTIONS.md:346,357`
as superseded.

> For every prompt, refresh MotorControl-RS HEAD and dirty state and read the RS code for
> this seam; RS is authoritative. Check HEAD and dirty state only for siblings this prompt
> names, and inspect only the seam it states. If HEAD and seam match the last recorded row,
> write "unchanged since prompt N" and do not reread. A sibling row must name a behavior
> adopted or rejected, with path and symbol. This does not relax vendor evidence, safety,
> uncertainty or verification rules.

| ID | Severity | Where | Problem | Replacement |
| --- | --- | --- | --- | --- |
| S-01 | High | `34:13` | "I2C immutable-input/output-on-error conventions" do not exist: INA228 leaves output unchanged (`INA228.h:570`), MB85RC and PCA9555 allow partial output. It legitimizes partial output against AGENTS | "Output-on-failure authority is RS `Units.h` and `prepareMoveAngle` tests. I2C libraries differ and set no CAN rule." |
| S-02 | High | `35:13`, `36:13` | "RV3032 bounded cleanup" restores the active EEPROM target or writes a safe hold (`RV3032-C7/docs/ARCHITECTURE.md:403-405`). It invites driver-initiated safe-state writes as a stop model, which `ECOSYSTEM_CONVENTIONS.md:195` already rejects | Delete from 35. In 36, at most: "Retain first operation and first stop failure separately (shape: RV3032 `ConfigurationJobReport`). RV3032 is not a stop or restoration model." |
| S-03 | High | `36:13` | "FieldCore reference publication/admission": FieldCore has no homing (`MotorControl.h:13`). `rs485_devices.md:47` contradicts the code at `Rs485MotorModule.cpp:582-607` | "FieldCore has no homing. Compare only reference-generation consumption and invalidation (`MotorControl.h:23,33`, `Rs485MotorModule.cpp:574-580`). Read code; guideline line 47 is stale." |
| S-04 | Medium | `33:13` | "I2C admitted-buffer ownership" exists in MB85RC only, and CAN requires the opposite | "MB85RC retains caller buffers; CAN admitted work must own copies. Contrast only." |
| S-05 | Medium | `46:7,11,15` | Scheduler, settings and health files have zero motor references. No CAN need exists. R21 grounds the consumer as a suitability check | "Read `MotorControl.h`, `Rs485MotorModule`, `Rs485Task` motor lanes, `RodCyclePorts`. FieldCore has no motor settings or health row; a CAN `BusId`, health row and settings are new work." Rename to "owner-module consumer modelled on `Rs485MotorModule`'s prepare/next/advance lanes". Use `MotorControl-RS/docs/fieldcore_handoff.md` as the structural reference, and state that FieldCore has no CAN owner, board route or request for one |
| S-06 | Medium | `39:13` | "FieldCore configuration ownership": no drive save or restore exists | "FieldCore has no drive-persistence precedent; only note that the module rejects runtime address and baud changes (`applyRuntimeConfig`)." |
| S-07 | Medium | `43:11`, `47:11`, `48:11` | `EVIDENCE_REQUIRED` exists only in ADS1115; explicit frames only in INA228; no command-framing option found in SHT3x. The clearest dry-run guard is in PCA9555 | 43: "RS `@ID` protocol and `_bench_evidence.py` govern. Compare ADS1115 contract and evidence verdicts, PCA9555 dry-run guard, INA228/SHT3x exact-version flags; I2C framing is not a model." 47/48: "ADS1115 contract/evidence split and soak bounds; MCP45HVX1 `SKIPPED_UNSAFE` and baseline gate." |
| S-08 | Medium | `37:13` | The I2C scanner is an ACK sweep with no retry; FieldCore bindings are compile-time | "Compare MB85RC AUTO versus fixed variant. Use FieldCore `rs485 discover` as the bounded-probe contrast. I2C `scan` proves ACK only and is no CAN model." |
| S-09 | Medium | `38:13`, `35:13`, `36:13` | "MB85RC family policy", "INA228 validity", "MB85RC partial-effect handling" have no bearing here | Delete. In 38 keep INA228 `invalidateHardwareState` as a desired-versus-verified analogue |
| S-10 | Low | `40:11`, `41:11`, `42:11` | The same cached-diagnostics and SHT3x classification audit three times | Audit once in 40. In 41 and 42: "Classify new routes using prompt 40's execution and safety classes." |
| S-11 | Low | `34:13`, `35:13` | FieldCore's contract has no angle field (its module sets STEPS at `Rs485MotorModule.cpp:165`) and has no velocity operation | Append "FieldCore has no angle or velocity operation; compare identity and admission only." |
| S-12 | Low | `ECOSYSTEM_CONVENTIONS.md:19,23,27,260`, `CONVENTIONS_REAUDIT.md:225-230` | Revisions drifted; `ownership.md:39` anchor moved to `:54-56`; old stage numbers 02-17 | Add a "historical snapshot; superseded by COMMON" header and correct the anchor |

**Keep these references, they are useful:** `39:13` RV3032 ambiguous-write reconciliation
(`test_primary_cell_ambiguous_write_is_reconciled_without_retry`) and MB85RC `WriteCommit`;
`38:13` FieldCore binding invalidation (`Rs485MotorModule.cpp:659-679`); `42:11`, `45:11`
FieldCore motor result boundary (`MotorControl.h:110-154`, with the v1.0.0 pin caveat);
`33:13` BME280 partial configuration; `37:13` MB85RC family selection.

## 11. Drive-contrast findings

EPOS4 facts come from maxon's Firmware Specification rel13740, fetched during this audit:
[EPOS4-Firmware-Specification-En.pdf](https://www.maxongroup.com/medias/sys_master/root/9579215585310/EPOS4-Firmware-Specification-En.pdf),
310 pages, SHA-256 `66fe32f15ab0bcbf9a008d0d0da8ffe395064c6716edcb3f3b28907f7160e64f`.
Neither the PDF nor anything else from this fetch was added to the repository.

| ID | Severity | Where | Problem and evidence | Fix |
| --- | --- | --- | --- | --- |
| X-01 | High | `46:17-19`, `MULTIDRIVE.md:52` | "Through real common code", "without changing common axis types". `MotorKind`, `FeedbackKind`, `Capabilities`, `requireCapability`, `RampKind`, `NativeRamp`, `ServiceEvidence`, `ActionExecution`, `ActionCompletion` exist only in `experiments/api_ownership/Contracts.h:74-188`. No prompt promotes them, so 46 needs API additions it does not admit to, or it overclaims | "Before these contrasts, name the production owner of each capability, ramp and outcome type. Promote the needed prototype types in this prompt, or record the contrast as prototype-only. A fixture against `experiments/` is not production evidence." |
| X-02 | High | `46:11,17`, `docs/reference/drive_contrasts.md:16`, `multidrive/SOURCES.json:284-301` | "Original contrast evidence", but no EPOS4 document is retained and the recorded 403 is no longer universal | DEC-7: add to 46: "EPOS4 is remote-only; reopen rel13740 and record the hash actually read." Record the URL and hash above in `drive_contrasts.md` |
| X-03 | High | `46:17`, `test/multidrive_contract_test.cpp:292-298` | "Differing ramp representations" as N5 versus EPOS4: at defaults both are rpm/s (N5 p65, EP p236). The real difference is Lichuan time versus acceleration. The test only asserts two local enums differ, and `Units.h:90-98` has no ramp-time type | "Lichuan time ramps versus N5/EPOS4 acceleration; N5 units follow `60A9`/`6097`, EPOS4 stays rpm/s. Exercise `convertAcceleration`; assert no time/acceleration substitution through a production type." |
| X-04 | Medium | `46:17`, `drive_contrasts.md:52`, test `:156-164` | "Wider signed rates" compares only with RS fields. `60FF` is I32 on N5, EPOS4 and Lichuan, and N5 `6042h` is I16. The test never calls `prepareVelocityTarget` | "I32 `60FF` exceeds RS native fields: pass ±70000 through `prepareVelocityTarget`, then a checked width check, including rejection on an I16 target such as N5 `6042h`. This is a width test, not a rate capability." |
| X-05 | Medium | `34:25` | "Float-native conversion ... supported by the contrast evidence": N5 and EPOS4 `60FF/607A` are I32; float evidence comes from moteus and ODrive, owned by 9, 10, 46. `axis_test.cpp:462-500` already covers it | Delete the clause. Add: "A prepared `BINARY32` angle target is rejected by the I32 finite move path before admission." |
| X-06 | Medium | `36:9`, `MULTIDRIVE.md:42` | Method numbers are profile-local. EPOS4 lists 17/18 but not 24/29 (EP p232). N5 `-1` uses a negative limit, EPOS4 `-1` is current threshold, positive direction. Current-position is 35 on N5, 37 on EPOS4. No homing row in the contrast ledger | "Method numbers are profile-local. EP rel13740 p232 lists 17/18 but not 24/29, so EPOS4 would reject those while still supporting homing. Add one homing row to the contrast ledger; no EPOS4 fixture or common numeric method type." |
| X-07 | Medium | `39:9`, `46:19` | "Same object index" hides the real contrast. N5 `2300h` is NanoJ Control; EP has no `0x2300`. N5 and EPOS4 use `1010/1011` signatures | "Lichuan persistence uses vendor `0x2300`; the same index is NanoJ Control on N5 and absent in EP rel13740. N5 and EPOS4 use `1010/1011` signatures, which do not apply to this EDS." |
| X-08 | Medium | test `:103-107`, `drive_contrasts.md:22,50` | `3202h` is a bitfield, not a motor code: bit3 CurRed, bit6 BLDC, bit0 CL/OL (N5 pp297-298). `0x08` means current reduction with BLDC clear. The test writes these into `Endpoint.profile`, conflating motor settings with identity | Remove the assignment. Label them "p38 example values for the `3202h` bitfield", and state which combinations are constrained |
| X-09 | Medium | `46:18`, `MULTIDRIVE.md:66`, `drive_contrasts.md:57` | "Valid extended ID and RTR semantics": no retained source pairs them. ODrive is 11-bit with optional RTR; VESC is 29-bit data frames; moteus is FD with extended IDs above `0x7ff`. `Frame.h:21,27` caps payload at 8, so C7's 16 bytes never enter the type | "Separate fixtures: VESC extended data; ODrive standard RTR with its empty-data alternative; moteus FD requirement as a flag rejection, stating its payload is not representable in `Frame`." |
| X-10 | Medium | `34:25,32`, `46:17`, `MULTIDRIVE.md:40`, `drive_contrasts.md:3` | Duplicate fixtures: test `:99-176` already covers stepper/BLDC and command versus feedback scale. The only new behavior in 34 is a wrapped request reaching `preparePosition`. The 3600 mapping holds only at 1:1, because N5 degrees include gear ratio `6091h` | Step 5 of 34: "Extend `test/multidrive_contract_test.cpp` case C1 with one wrapped-angle vector on N5-documented scale (3600/turn at 1:1) and an abstract non-step servo scale, using production `preparePosition`. No new fixture family." 46 audits completeness and adds only missing cases |
| X-11 | Low | `drive_contrasts.md:51`, test `:121-122` | EP pp221,234 permit only increments, so no independent command/feedback unit at object level | Cite N5 only (`607A` versus `6063`, pp360-361); keep EPOS4 as "no step geometry" |
| X-12 | Low, PLAUSIBLE | `46:18`, `MULTIDRIVE.md:67` | "Brake" blurs VESC electrical braking and moteus mode 15 with holding brakes | "Electrical braking modes and holding brakes are distinct; C11 remains abstract." |

## 12. Documentation findings and per-prompt checklist

### Documentation

| ID | Where | Problem | Fix |
| --- | --- | --- | --- |
| N-01 | `AGENTS.md:6,26`; `CONVENTIONS.md:3,44,319,329`; `DOCUMENTATION.md:3` | "Currently an audited implementation promptset, not a working driver"; "not working CAN commands today". False: the verifier, owner and firmware exist. The coder gets the wrong picture of the repo | Delete the planning-only text; describe the implemented and pending scope |
| N-02 | Root `README.md:15-92,103,123`; `PROGRESS.md:3-145` | Reverse-chronological stacked status; README still says to begin at prompt 1; `PROGRESS.md:86` is a very long paragraph with mojibake where an en dash belongs. State must be rebuilt from history | PROGRESS becomes identity, rule, table with Inherited gates and "Next ready"; history stays in reports. README gets one current-status section. Fix the mojibake and add a check for the mis-decoded UTF-8 sequence U+00C3 U+00A2 (`check_promptset.py:392` tests only U+FFFD) |
| N-03 | `docs/canopen_services.md:126,138-142` | False: network decoding "remains prompt 15"; owner "explicitly reports `reboot(node)`... `busOff()`"; a false return leaves events for owner dispatch; event codecs "remain assigned". `receive` now routes them itself | Edit the sentences |
| N-04 | `docs/can_owner.md:33-36,388-390` | "All TX passes through the owner", but NMT cannot (C-09). Streak claim unreachable (C-05) | Correct with C-05 and C-09 |
| N-05 | `docs/architecture_plan.md:44-45,56,66`, `docs/architecture.md:35` | Stale paths and "root target is only the common core" | Correct with R-04, G-10, G-11, G-12 |

### What to change in each prompt

Read the cited finding for the wording. "All" items below apply to every prompt in the
range: the audit rule in section 10 and the usable-prerequisite rule in G-02.

| Prompt | Findings to apply |
| --- | --- |
| 31 (Partial; add obligations) | C-01, C-02, C-16, C-17, C-18, C-19, A-20, DEC-2, DEC-8 |
| 32 (Partial; add obligations) | D-26, D-27 |
| 33 (inherited by 34) | C-16, D-01, D-02, D-03, D-12, D-13, D-16, A-01, A-02, DEC-8, R-08, R-09, R-14, S-04 |
| 34 | A-02, A-14, D-14, D-19, R-08, S-01, S-11, X-05, X-10 |
| 35 | A-01, D-02, D-13, DEC-8, G-03, R-01, R-13, R-14, S-02, S-09, S-11 |
| 36 | A-01, D-02, D-03, D-12, D-13, D-14, D-15, D-20, D-21, DEC-8, G-03, G-09, G-10, R-14, S-02, S-03, S-09, X-06 |
| 37 | D-04, D-05, G-06, S-08 |
| 38 | D-05, D-06, D-11, D-24, D-25, G-01, G-07, G-12, S-09 |
| 39 | D-07, D-09, D-10, D-11, G-12, R-05, S-06, X-07 |
| 40 | G-08, R-04, R-12, S-10 |
| 41 | D-04, D-08, D-10, R-11, S-10 |
| 42 | A-01, C-01, C-02, C-10, C-16, D-17, G-04, G-08, R-14, S-10 |
| 43 | G-02 (prerequisite becomes 40), G-12, R-10, S-07 |
| 44 | D-04, G-12, R-02, R-03, R-10, R-14 |
| 45 | C-01, C-16, D-22, G-09, G-14, R-09 |
| 46 | DEC-7, G-10, G-11, G-14, S-05, X-01, X-02, X-03, X-04, X-07, X-08, X-09, X-10, X-12 |
| 47 | D-02, D-04, D-07, D-09, D-14, D-17, D-23, G-03, G-04, S-07 |
| 48 | D-09, D-10, D-11, S-07 |
| 49 | A-17, DEC-5 (CANopenNode packaging), G-01, G-09, G-10, G-11 |
| 50 | G-01, G-10; also add 'prompts 49-50 close as a scoped software candidate listing all inherited gates' |

## 13. Rules for editing prompts without breaking the checker

**Free to edit.** Prompt bodies are not hashed or size-limited. `PROMPT_SIZING.json` and
`PLANNING_VALIDATION.json` are retained snapshots no script reads; the latter is already
stale (123 hashed artifacts recorded, 137 current).

**Enforced in each prompt** (`scripts/check_promptset.py:298-343`): title `# Prompt N `,
the five `## ` headings and five contract links, `docs/reports/prompt_N.md`, the words
commit and sync, and a Markdown link to COMMON.md. The first line containing `Prerequisites:`
must link exactly the `depends_on` prompts.

**Changing a dependency:** edit three places together: `SEQUENCE.json`, the `README.md`
row (`a, b` format) and the prompt's prerequisite line. Dependencies must point to earlier
IDs.

**Traps:**

- Any table row starting `| <digits> |` in README, PROGRESS or MULTIDRIVE must form exactly
  1..50 across that file (`:295-297`, `:366-369`). Key any extra table as `P31`.
- A link to a file that does not exist yet fails everywhere. Put future paths in backticks
  (`:410-419`).
- Tests pin text in prompts 17 and 23, README row 23, `stages==50` and `hashed_artifacts==137`
  (`scripts/check_promptset_test.py:75-76,140-161`). The ledger owner is capped at 50. A new
  prompt 51 or a lettered prompt fails checks, so reopen existing prompts instead.
  Requirements must stay exactly R01-R35.
- A new test needs `add_executable(X_test X_test.cpp)` and `add_test(NAME X ...)`
  (`scripts/check_repository.py:200-207`). `library.json` exports must equal EXPORTS.
- Preserve hashed references and `.gitattributes` rules. When touching those, run
  `python scripts/check_promptset.py --check-git-ref INDEX`.

**Not enforced anywhere:** PROGRESS state against prerequisites, paths named in prompts,
ledger owner against prompt text, the API/CLI mapping against route assignments, mojibake.

**Ledger edits** (G-07, Q20 owners, D-07, D-08) go through the reviewed ledger first, then
`python scripts/generate_lichuan_objects.py`, then `--check`. Do not hand-edit generated
files. Keep original conflicting claims visible.

## 14. Checked and found correct

Do not spend time re-verifying these unless their source changes.

**Motion objects (original PDFs and raw EDS):** `607A` I32 ±1,000,000 pulses; `6081` U32
0..3000 rpm; `6083/6084/609A` U32 0..2000 ms; controlword 6, 7, 15, 31, 95, 63, 127, 271, 11,
128 and their bit meanings; the three-way status table including `2100` inverted bit4;
`60FF` EDS I32 versus table U32 and no documented negative velocity; ZH86 and EN86 velocity
start-order conflict; homing methods 17, 18, 24, 29, including "method 29 finishes on leaving
the switch"; `6098` one byte, `6099:01/:02` U32, `607C` I32; no numeric backoff or timeout;
`2302` 0-to-1 then manual 0; `605A/6085` absent, `605D` I16 default 1; "about 10 ms" enable
spacing; the 5-second LED cycle; no documented SDO, save, stop or bus-off bound.

**Objects and wiring:** EDS identity vendor `0x1000`, product 1, revision 0, no `1018:04`;
`1009/100A` DataType 0x0006 (U16) with defaults `0x2608`/1, so the CiA string type does not
apply; `2000/2001`; SW1-5 weights and all-OFF "user-defined"; SW6/7 ON/ON 125k, OFF/ON 250k,
ON/OFF 500k, OFF/OFF 1M; SW8 120 ohm; RJ45 pins 1/2/3; `2203` U16 RW default 0;
`LSS_Supported=0`; `BaudRate_50=1`; `2300` values 2/1/0 with no disagreement between manuals;
`1010/1011` absent; `2102` versus `60FE` are different layouts; DO subindex dispute; 99
indexes, 202 scalars, 23 headers (recounted from the EDS); SRT `srt/LICENSE` is GPLv3 and its
save-during-zeroing is real, with no SRT code in this repository.

**RS parity:** `AnglePath` and `HalfTurnTie` values; no alternate turn on limit;
`stopDueUs = now + durationUs` at preparation; fixed `deadlineUs`; retained `needsStop`;
cancel without stop; `getHomeReference`; `getDiscoveryProfile`, `getDiscoveryCapabilities`,
`prepareProbe`; all named CLI spellings; 128-byte and 22-token capacity; protocol 3 `@ID`
JSONL; removed aliases rejected; the six-file runner family; `BusOwner` vocabulary; no
`getMoveResult` family; `Status` and `Err` shape.

**Code:** `Units.cpp`, `quantizeInteger`, `quantizeFloat`, `rationalToDouble`,
`convertAcceleration` and the wrapped path (differential fuzz, UBSan); `parseExactNumber`;
SDO envelopes, sign extension and abort handling; atomic publication in `prepare`, `advance`
and `get`; `6041`/`2100` masks; one receive path; deadline arithmetic near `UINT64_MAX`;
first terminal immutability; stale-epoch rejection; recovery interlock; TWAI ISR handoff and
slot accounting; `verify.py` rejecting skipped tests. In `Actions.cpp`: stop and clear bytes, the `2100` masks,
atomic rejection, terminal immutability, deadline equality, step exhaustion, WAITING cancellation and the
counter-zero epoch handling. Prompt 34's "existing exact axis/angle
policy" is real: it needs composition plus A-14. No prompt places the owner in the core. The
41/42 boundary is clear.

**Contrasts:** N5 `3202h/3203h` separate motor and feedback; `607A` versus `6063`; EPOS4 modes
1/3/6/8/9/10; ODrive RTR real with standard IDs; moteus FD-required; VESC 29-bit IDs and scale;
`Frame.h` Classical rules; `Endpoint.h` address above 127 and subaxis.

## 15. Limits of this audit

- The designs in W4 to W7 were checked against the code by one independent static read. None of them has been
  compiled or prototyped. If a step cannot work as written, keep its safety rule, change the mechanism and
  record why.
- The stack figures in A-11 come from a compile with an older toolchain than the pinned IDF 5.5.5. Rerun them
  with the pinned one in W7.
- DEC-2's safety reasoning about `010F` and `000B`, and DEC-3's in-order server premise, rest on CiA 301/402
  from memory. W11 measures them.
- No drive behavior was measured. Retained targets, mode change while enabled, status-bit
  behavior, zero ramps, negative velocity, stop from word 31, node activation, save
  durability, boot NMT state and read effects all remain open.
- Most manual pages were read from page-tagged text extractions. Rendered and viewed: EN86
  pp9-10 and pp12-15, ZH86 pp13-14 and p19. OP86 and the 57C manuals were text only. Reopen the
  rendered page before relying on an extracted table.
- CiA 301 is not retained, so statements citing it are from memory and marked as such.
- C-01 to C-07 scenarios ran against the real owner, `SdoClient` and `TwaiAdapter` with a fake
  driver, not on the ESP32. Probe source files stayed in the audit's temporary directory and
  are not in the repository. Write each regression from the scenario described here.
- A comprehensive review of tests for helper-mirroring was not done. No concrete offender was
  found in the files opened.
- Prompts 1-30 were read only where 34-50 depend on them. The prompts 31 and 32 code got one independent
  review for this audit, focused on how later prompts compose with it. Line references for sibling items S-01,
  S-05 and S-08 were not re-read at the current sibling revisions.
