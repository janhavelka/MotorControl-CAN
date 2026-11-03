# Audit and repair instructions: prompts 34-50 and the code they build on, 2 November 2026

> **Closed for W1 to W8, 2026-11-03.** Handoff packets 1 to 10 delivered W1 to W8 for host scope, and [packet 10](../reports/handoff_19_33/packet_10.md)'s closure audit lists what stays open. The hold below was lifted on green CI at `5306eaf`, in `19d48e7`. This file stays the source of its decisions and findings, but its baseline line numbers and its description of the current code are historical. A-15, A-16, A-18 and A-19 had no W1 to W8 owner and are tracked in the [backlog](../backlog.md). For the current state read [PROGRESS.md](PROGRESS.md) and the [execution table](../reports/handoff_19_33/README.md). Nothing here is drive evidence: no specimen has run a stop, enable or move.

Instructions for the AI coder who will repair and execute these prompts. This file
gives settled decisions, ordered work packets and the findings behind them. It changes
nothing by itself: writing it edited no prompt, code, ledger or test, and touched no
hardware.

**Baseline.** MotorControl-CAN `c329c7e` on `promptset/motorcontrol-can`; MotorControl-RS
`513251d`, clean. Prompt and code line numbers refer to that state unless a finding says
otherwise. Refresh them before relying on a line number, because other sessions commit here.

**Changes since the baseline.** `7764c89` and `c643af8` landed after this file was verified. This
file now sits in the promptset, but its findings were not re-verified against those commits. Before
acting on an affected item, check it against the current code and these reports:
[connection](../reports/2026-11-02_s2_live_connection.md),
[NMT and TPDO checkpoint](../reports/2026-11-02_nmt_pdo_checkpoint.md),
[TPDO and motion plan](../reports/2026-11-02_pdo_motion_plan.md).

- **Bus evidence now exists.** Two user-labelled CL86-C specimens answered on nodes 1 and 2 at 1 Mbit/s through
  an ESP32-S2. Some W11 measurements may already be recorded, such as retained `607A=5000` and
  `60FF=120` on one specimen. Read the records before repeating one. They cover that firmware
  and cycle only.
- **Prompt 18 is Partial,** with fixed TPDO1 implemented and observed on one specimen. Recheck
  G-01 and G-02, which were written while it was Blocked.
- **The S2 firmware work landed.** Check it against the W9 list and do only what is missing.
- **Owner, SDO client and TWAI code changed.** Recheck G-04, W3, W4, W5 and W6, and every line
  number in `CanBusOwner`, `SdoClient` and `IdfTwaiDriver`. For example, `fail_retry_cnt=0` is
  still set, but now at `IdfTwaiDriver.cpp:84`, so DEC-4 still needs applying.
- **Later corrections, packet 1.** `607A=5000` and `60FF=120` equal their EDS defaults, so "retained" below is unsupported ([corrected reading](../device_conflicts.md#corrected-reading-of-the-november-2-inventory)). The firmware task stack is now 12,288 bytes, so A-11's "exceeds the stack alone" no longer holds numerically; its worst chain of 12,352 was not re-measured, and W7's 6,144-byte budget is a project limit, not a quarter of the current stack. G-04's "acquisition cannot run on the real board" no longer holds. Retained hashed artifacts now number 140, not 137.
- **A small motion experiment is planned** in the motion report, under prompt 29's rule for
  isolated experiments. Rule 1 below holds production lifecycles; it neither authorizes nor blocks
  that experiment. Until C-16 and A-11 are rechecked, do not count on the library's stop in it.

**Verification.** Every finding was re-checked individually by an independent reviewer trying
to refute it, against the code, running probes and original PDF pages. Section 16 records each
verdict. No finding was refuted; 71 needed corrected text, which is applied below.

**Finding status.** CONFIRMED means a reviewer opened the cited source or ran a probe.
PLAUSIBLE means the reasoning holds but something is still unmeasured, and the finding
says what. "Lead checked" means the audit lead also re-read the code or source
independently. Nothing here is a measurement of a real drive.

## Start here

1. **Build a finite move, velocity or homing lifecycle only on consumed, qualified contracts.**
   At the baseline this item said not to build prompt 33's `prepareMoveAbsolute`, `nextMove` and
   `advanceMove`, or prompts 34-36, until W1 to W8 were done, because a running move could not be
   stopped (C-16), a stop would overflow the firmware task stack (A-11) and the velocity API
   returned the wrong unit (A-01). Packets 7, 8 and 9 fixed those seams on the host, and W1 to W8
   closed with packet 10. Packet 20 then built the absolute lifecycle behind a gate that is closed
   by default. The lifted hold qualifies no release command, moving stop, relative wire base or
   physical scale: whether a stop works on the drive is packet 16's measurement, and acceptance
   and completion are packet 19's. Prompt 33's pure `preparePositionParameters` landed at
   `c329c7e`, sends nothing, and stays.
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
6. **Render PDF pages; do not trust text extraction.** Extraction lost a supply range printed on
   the panel artwork (D-07) and misaligned the DI default column on EN86 p8. The Read tool's PDF
   mode needs `pdftoppm`, which is absent here; render with PyMuPDF instead.
7. **Use a short build directory on Windows,** such as `.build/w3`. A long path makes the
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
persistence, CLI or Python runner. At the baseline, prompts 34-50 were Pending, 18 was Blocked,
19-32 were Partial, 33 was Partial with pure preparation only, and no drive had been observed on the
bus. The note on changes since the baseline and [PROGRESS.md](PROGRESS.md) give the later state.

Check results:

| Check | Result |
| --- | --- |
| `python scripts/check_promptset.py`, `check_repository.py`, `git diff --check` | Pass with this file in place, run by this audit |
| `python scripts/verify.py --mode quick --compiler gcc` | Pass by this audit on the tree that became `1d68ccd`, before prompts 31 and 32 |
| Full verifier at `9e5b62d`: 55/55 CTests, package consumers, strict Doxygen | Pass as recorded in `docs/reports/prompts_31_32_audit.md`; not rerun by this audit |
| Stack frames for ESP32-S2 and S3, compile only, pinned GCC 14.2 | Fail against the 8192-byte task stack, measured twice independently (A-11) |
| Baseline CTest at `c329c7e`, run by a reviewer on the frozen snapshot | Pass, 58/58 |
| Probes against the real owner, `SdoClient`, `TwaiAdapter`, `prepareStop` with a fake driver | C-01 to C-07, C-16 to C-19, A-01 to A-09, A-14 and A-20 reproduced as corrected below |
| Sanitizers, Arduino/IDF firmware builds, remote CI for this audit | Not run by this audit |
| Hardware | Not run |

Independent fuzzing of the arithmetic (1.1 million cases against Python `Fraction`,
also under UBSan) found no mismatch, in the first pass; that probe was not retained or rerun. The
arithmetic is not where the risk is.

## 2. Verdict

The numeric device facts are good: every object, width, range and bit checked in
`LICHUAN_MOTION_FACTS.md` and `LICHUAN_OBJECT_FACTS.md` matched the original
documents. The implemented action subsets also do what their guides say. The problems
are in what those subsets leave out and in how the prompts compose them:

1. **A running move cannot be stopped.** `prepareStop` accepts only a latest `000F`, and
   a trigger leaves `001F` (C-16). Even from `000F`, the documented procedure cancels the
   in-flight poll, which quarantines the route (C-17). After an unconfirmed write no stop
   can be sent at all (C-01).
2. **A stop would overflow the firmware task stack.** On ESP32-S2 and S3 with the pinned
   compiler, `prepareStop`'s own frame is 8,608 bytes and the owner task has 8,192. The worst
   static call chain is 12,352 (A-11).
3. **The dependency graph cannot finish** (G-01, G-02). Prompts 49-50 descend from
   Blocked 18 and Partial 30, although the README says they can complete.
4. **Three API gaps would force wrong or copied motion code** (A-01, A-02, A-03). The
   worst writes a command-increment rate into an rpm object.
5. **Discovery, the CLI read routes and the first hardware step are all stopped by gate
   Q20,** which has no practical way to close (DEC-1).
6. **The manuals document no way from homing to a position move** on an enabled drive
   (D-02, D-17). No prompt mentions this.
7. **Two TWAI paths** make a routine CAN event end all SDO service to a drive until the MCU
   resets. Both paths are reproduced with a fake driver; the arbitration-loss trigger in C-03
   is unmeasured on hardware.
8. **The real bench board cannot run the firmware** (G-04).

**Status of the verdict, 2026-11-03.** The list above describes the baseline. Host repairs, with no
drive evidence for any of them:

1. Packets 6 and 7: a stop follows the DEC-8 predecessors, and a best-effort `000B` route serves a
   lost history or an unconfirmed write. Each best-effort stop costs the axis's route until an MCU
   restart, and whether any row stops the drive is packet 16's measurement.
2. Packet 8: every frame and known chain is under the project limits on S2 and S3. These are
   compile-time lower bounds; the live stack during a stop is unmeasured.
3. Packet 1 repaired the graph (W1).
4. Packets 9 and 10: the native rate basis (A-01), `retireMotionParameters` (A-02) and
   `interpretStatusword` (A-03). Packet 20's move routes its speed through the rate basis.
5. Packet 4: DEC-1 admits the accepted read list as risk acceptance. Q20 stays open as a device
   fact.
6. W1 states D-02 in prompt 33. It is still undocumented by the manuals.
7. Packet 3 fixed both TWAI paths on the host (W3). Its physical questions 1, 4 and 6 stay
   unanswered after packet 11.
8. G-04 moved to W9. The S2 board runs the bench firmware, and packet 11's baseline and soak ran
   on it.

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
| DEC-8 | A stop is allowed from a trigger word, as a project decision not vendor evidence, and bit 4 is held until the move is terminal | W6, 33 |

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

All listed objects exist and are readable. `1018`, `1001` and `1003` are EDS-only. `2100`-`2102` are
RW in the EDS and RO in the manuals; `6061` is RW in EN86 p16 and RO in the EDS. Calling a fault code
the most plausible read-to-clear object is this audit's judgment; no source says so.

"Accepted" is the owner's risk acceptance, not evidence that a read has no effect. Results
keep the raw "effect unknown" field, `effectPossible` stays true, and no document may call
these reads non-changing. The first live read still needs the bench prerequisites.

**DEC-2. Stop on an unresolved route.** A motor that may be moving is exactly when the
host needs a stop. An explicitly requested urgent `6040:00=000B` may be transmitted on an
unresolved route. Rules:

- Only `000B`, the arrow `15->11` labelled emergency stop in EN86 p12, ZH86 p12 and OP86 p10. The
  label covers that arrow only; other predecessors are unmeasured (W11 step 6). Never `010F` here: it
  has the enable-operation bits set, and the vendor table `6040=15` gives status low bits `111`, so it
  could enable a released drive under CiA402 (from memory). That `000B` does not enable is also
  CiA402 from memory, because no manual defines bit 2. Bench-check it before relying on it.
- The reply never qualifies, because a late `6040` reply is indistinguishable from the
  stop's reply. `execution` stays UNKNOWN whatever arrives, and the route stays unresolved.
- The stop's effect can only be shown by later independent observation, and it never
  settles the earlier operation's uncertainty.
- One request, one transmission, no resend. A second stop is a new explicit request. This
  matches current RS behavior (`docs/console.md:196-199`, `docs/ess_actions.md:165`).
- A stop held across a recovery has lost its state evidence, so it is re-issued as `000B` even
  if `010F` was requested. RS keeps the requested policy here (`ProbeApp.cpp:2195-2200`); this is an
  intentional difference. Record it.
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
   every pair retained on that route. Each channel retains up to four pairs, from unresolved
   requests and unsolicited frames; on overflow it refuses markers. While a marker is pending, a
   frame with a retained pair is ignored. Any other frame on the route, except the marker's own
   reply, fails the marker and is retained. A frame with no multiplexer refuses markers from then
   on. This check runs before `:410` and the unresolved branch at
   `adapters/canopennode/SdoClient.cpp:399`; keep the pre-handoff check at `:401`.
4. Before reopening, invalidate every reservation on that node. Otherwise `sequence()`
   (`examples/common/CanBusOwner.cpp:74-89`) makes the old reservation live again and `prepareStop`
   accepts the pre-loss `000F`; a probe with the flag simulated showed exactly this. New work needs
   a new reservation.
5. A matched success reopens the route and increments a retained `requalifications` counter.
   Failure leaves it unresolved. Each attempt is a new explicit request.

This supplies the "ordered drain/retirement boundary" that `docs/canopen_services.md:321`
requires. It assumes this host is the sole SDO client on these COB-IDs, and that the server answers
one request at a time and in order. The second is CiA 301 from memory and is not vendor-confirmed, so
record it under Q18 and measure it. A marker
settles the channel only. Earlier effects stay uncertain, `000F` history stays consumed, and
nothing replays or re-enables. Quiet time, heartbeat, a new host epoch, rebuilding the client
and same-object readback remain insufficient, so their negative tests stay.

**DEC-4. TX mode.** `fail_retry_cnt=-1`. HAL v1 offers only this or single-shot
(`docs/adapters/twai_sdk.md:45`; the header says "-1 for re-trans forever"). Hardware retransmits
after arbitration loss, an error frame or no ACK. ACK may come from any node, and a late error can
duplicate delivery (general CAN knowledge, no retained source). It is not an application replay.
Costs: with no ACK the frame stays pending, so at the deadline it stays "possibly transmitted" until
node deletion retires it; and one pending frame stalls the interface for every axis until an
application-started `recover`, which C-20 must handle. The SDK review pin targets ESP32-S3; recheck
it for S2 in W9.

**DEC-5. Owner location.** Add `adapters/owner/` with optional target
`MotorControlCAN::CanBusOwner`, outside the installed portable core. It includes
`CanBusDiagnostics.cpp` and `OwnerCounters.h`, or the target cannot link. Do not move
`experiments/canopen_service_fit/vendor` now: `scripts/check_promptset.py:120-184` hash-checks
it and the move has no behavior value yet. Name it as packaging work for prompt 49.

**DEC-6. Soft limits.** Allow the move when the witness is outside the limits, the endpoint is
inside them and the move goes toward the range. An endpoint still outside stays LIMIT, and a witness
outside the native limits still rejects. This applies only when a `nativeKnown` reference is supplied;
an absolute move with no reference already skips the check. This differs from RS, whose `src/Axis.cpp:168-170,508` still rejects it. Record it as an intentional
difference and report it to the RS owner; do not edit RS.

**DEC-7. EPOS4 evidence.** Keep rel13740 remote-only, with URL and SHA-256 from section 11. A
coder cannot perform a rights check, and retaining a PDF would change the pinned
`hashed_artifacts==137`. This replaces the earlier "retain after a rights check" draft.

**DEC-8. Stop predecessors and bit 4.** The manuals give whole-word stop arrows only from word 15:
`15->11` in EN86, ZH86 and OP86, and `15->271` in ZH86 p12 and OP86 p10 only. No manual defines bit 2
or bit 8: the bit tables cover bits 4 to 6, and bit 7 on p15. No manual says what a falling bit 4 does
in mode 1. So every row below the first rests on derived words. Allowing them is a project decision,
because a running move with no stop is worse, and clearing bit 4 while stopping cannot be the wrong
direction. Carry `predecessorDocumented` in the evidence.

| Mode | Latest acknowledged `6040` | Stop word | Status |
| --- | --- | --- | --- |
| 1 | `000F` | `010F` or `000B` | Whole words documented; `010F` in ZH86 and OP86 only. Implemented today |
| 1 | `001F`, `005F`, `003F`, `007F` | `010F` or `000B` | Arrow not documented; `predecessorDocumented=false`; measure under Q12 |
| 1 | `010F` | `000B` only | Escalation of a running normal stop (R-14); `predecessorDocumented=false` |
| 3 | `000F` while running | `010F` or `000B` | Arrows appear only in the position section; `predecessorDocumented=false`; prompt 35 |
| 6 | `001F`, host word inferred from `15->31` | `000F` | Edge documented: EN86 p15 "1->0: Stopping homing", ZH86 p15 "1->0：中断回零" (interrupt homing). The word is derived. Prompt 36. This is the stop itself, not a preamble |
| Any | Unknown, or state evidence stale | `000B` only | DEC-2 path. This widens DEC-2 to stale evidence on a clean route |

W6 widens mode 1 only; prompts 35 and 36 add modes 3 and 6 with their operations.

Prompt 33 does not clear bit 4 during a move, because its meaning in mode 1 is unspecified. It
holds `001F` or `005F` until the move is terminal, then writes `000F` as a visible step. That is
host policy, not a vendor rearm rule. Immediate update (`15->63`, `15->127`) implies a new edge during
a curve, by inference, so it has its own gate. A prior `000B`, a released state or any other word still
rejects in `prepareStop`, and a prior `010F` allows only `000B`. A later `stop normal` never replaces a
running or held `stop fast`.

**Smaller settled choices.**

| Finding | Choice |
| --- | --- |
| A-02 | A move's own parameter writes are phases of the move: `607A`, `6081`, `6083`, `6084` (33); `60FF`, `6083`, `6084` (35); `6098`, `6099`, `609A` (36). They do not go through `prepareDriverSettings` and do not advance the configuration generation. They do retire retained `MOTION_PARAMETERS` observations, so a later settings call needs a fresh read; without this, settings reports success with no TX while the drive holds the move's value. `607C` needs its own ruling in prompt 36. An explicit settings call still advances the generation. Homing uses the one-expected-advance pattern of `expectedTarget` in `Actions.cpp:40-45` |
| A-07 | `CORRELATION_LOST` withdraws earlier blocks, as BUS_OFF does |
| A-08 | Add a distinct service-timeout outcome to reads and actions. It covers queue and response budgets; "reply lost after trigger" also needs `transmissionPossible` |
| A-14 | Do not add a field in W8. Exact paths are recoverable. If prompt 34 needs it for approximate radians, add `nativeDirectionPositive` |
| C-05 | `recover` quarantines only channels with outstanding work, sampled before `retire` clears it |
| C-14 | Accept Classical DLC 9..15 by normalizing to `dlc=8`; `Frame.cpp:9` rejects anything larger |
| D-10 | Three terms: `save parameters`, `restore factory settings`, `re-apply recorded configuration` |
| G-02 | The usable-prerequisite rule in G-02 |
| G-13 | Processing-time cutoff, as reads and actions already do: `now >= deadlineUs` expires, even if the frame was received earlier |
| R-04 | The console goes in `examples/common/ProbeConsole.*` |
| R-14 | Adopt RS stop escalation upward only, and in-flight settling; other rows as listed in R-14 |

## 4. Work packets

Each packet ends with passing checks and a pushed commit. Use
`python scripts/verify.py --mode full --compiler gcc --build-dir .build/<packet>` for code
packets, and also `python scripts/check_lichuan_ledger.py` and
`python scripts/generate_lichuan_objects.py --check` when the ledger changes. Verify required
CI on the exact final commit after each of W2 to W9. The designs below were reviewed against the
code but not compiled; if a step cannot work as written, keep its safety rule, change the mechanism
and record why. Update `CHANGELOG.md`, the stage report,
`docs/backlog.md` and the affected guide in the same commit as a contract change.

### W1. Repair the promptset (documents, ledger rows and one checker guard)

**Findings:** G-01 to G-14, section 10's audit rule, N-01 to N-05, the per-prompt table in section
12, DEC-7. **G-05 is already resolved.**

1. Done: `COMMON.md` item 5 links this file as active. Remove that item, and the matching
   paragraphs in `README.md` and `START_HERE.md`, once every prompt is repaired and W1 to W8 are done.
2. Apply G-01 and G-02 first: remove 18 from 38 in its three places, replace the start rule, add the
   Inherited gates column, and add the checker guard that 18 is not an ancestor of 49. Apply the
   dependency edits of G-02 and G-03 together with `44` depending on `[42, 43]`; alone they would drop
   41 and 42 as ancestors of 44, 45, 47 and 48.
3. Apply the remaining G items, then section 12 prompt by prompt, then section 10's rule and N-01 to
   N-05. Follow the checker rules in section 13.
4. Write the DEC-2, DEC-3 and DEC-8 obligations into prompts 31, 33, 35, 36, 42 and 45, so that those
   prompts describe the contract W6 delivers. Add the new obligations for prompts 31 and 32 to their `PROGRESS.md` rows.

**Done when:** `python scripts/check_promptset.py`, `python scripts/check_promptset_test.py`, a new test
for the guard, `python scripts/check_lichuan_ledger.py`, `python scripts/check_lichuan_ledger_test.py`,
`python scripts/generate_lichuan_objects.py --check`, `python scripts/generate_lichuan_objects_test.py` and
`git diff --check` pass. No CI dispatch is needed unless a script change warrants it.

### W2. Move the owner and share test fixtures (mechanical)

**Findings:** C-08, C-11, C-12. Run this when no other session has uncommitted owner edits.

1. `git mv` `examples/common/CanBusOwner.*`, `CanBusDiagnostics.cpp` and `OwnerCounters.h` to
   `adapters/owner/`, use namespace `MotorControlCANOwner`, and add the optional CMake target.
2. Update `adapters/twai/TwaiAdapter.h:6`, `examples/common/CMakeLists.txt:4`,
   `examples/common/firmware_sources.json:3`, `scripts/check_firmware.py`, the repository checker
   lists and `CONVENTIONS.md:24,139-140`. ProbeApp, console and firmware code stay in
   `examples/common`.
3. In a second commit, split `CanBusTypes.h` and move shared fixture code into
   `test/support/OwnerRig.h`, keeping each suite's behavioral differences. No DSL.

**Done when:** the full verifier and firmware checks pass with no behavior change. Later packets edit
the new paths.

### W3. Make TWAI transport survive routine events

**Findings:** C-03 (DEC-4), C-04, C-07, C-13, C-14, C-20. Reopens prompt 27; record that in its report.

1. Set `fail_retry_cnt=-1` in `adapters/twai/IdfTwaiDriver.cpp:78`; correct
   `docs/adapters/twai_sdk.md:45` and `adapters/twai/README.md:27`.
2. Make `submit` return SENT, DEFERRED or FAILED. DEFERRED keeps the slot QUEUED, and a withdrawn
   first frame with a proved-unsent report cannot quarantine. Drop the `rxPending` test at
   `TwaiAdapter.cpp:142`, which the split makes stale; DEFERRED covers it.
3. Split the adapter into `pollEvents` (before the owner) and `promote` (after), including
   `FirmwareProbe.cpp:27-30`. This narrows the C-07 race and does not close it, so keep the blocked handling.
4. Fix C-20: start the response clock and `possible` at SDK submit, not adapter handoff, or let a
   proved-unsent timeout skip quarantine.
5. Report real reasons instead of "clock fault" (C-13); accept DLC 9..15 by normalizing (C-14).

**Tests:** change `test/twai_driver_test.cpp:274` as a recorded contract change. Add the three
scenarios in C-03, C-04 and C-07 with the fake driver: arbitration loss, RX interrupt during
promotion, and a same-tick reply followed by an urgent admission. Add C-20: axis 1's frame pending, axis
2's stop never submitted, axis 2 not quarantined. `twai_adapter_test.cpp:334-339` stays valid only as FAILED.

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
   `examples/common/DeviceEvidence.cpp:88-90`. Update `examples/reads_preview/main.cpp:93`. Drive the
   hardcoded `ReadEffect::UNQUALIFIED` at `scripts/generate_lichuan_objects.py:267,297` from
   `read_disposition`. Reconcile with `authorizeUploadExperiment` if the S2 work has landed.
4. Update the Q20 row in `docs/device_conflicts.md`, `docs/canopen_services.md:159-163,306-308` and
   `docs/hardware_bench.md:65-71`.

**Tests:** record the changed pins in `test/reads_test.cpp:201,314`, `test/objects_test.cpp:24` and
`test/device_evidence_test.cpp`. `prepareProbe` on `1000:00` succeeds. `603F` and an unlisted object
still reject as they do today. A successful accepted read keeps `effectPossible=true`. A first accepted upload confirms the
channel without claiming freshness.

**Done when:** the full verifier, ledger check and generator check pass.

### W5. Make SDO channels recoverable (DEC-3)

**Findings:** C-05, C-06, N-03, N-04. Reopens prompts 13, 14 and 24; record that in their reports.

1. In `SdoClient::finish` (`SdoClient.cpp:269-270`), do not set `unresolved` on a matched SERVER_ABORT
   on a clean channel. Also change its `outcome == BUS_OFF` clause, which quarantines queued unsent work.
2. At `adapters/canopennode/Network.cpp:165,206`, for RECOVERY and BUS_OFF set `unresolved` only with
   occupied possible work, `handoffLive` or `cleanup`. Sample these before `retire(c)`, which clears them.
   BOOTUP and BINDING still quarantine. Passive node evidence is invalidated as it is today.
3. Add the drain marker from DEC-3 through a new path that W6 reuses: owner
   `admitDrainMarker(const SdoTransfer&, now, id)` and a flag `Slot.outsideSequence`. `eligible`, `expire`
   and the FIFO check skip `sequence()` for a flagged slot (`CanBusOwner.cpp:260-268,275,281-284`). Use an
   urgent slot, because `select` skips producer 0 in ordinary slots (`:292-297`). A client path bypasses
   only `unresolved`, `ordinarySdo` and, for the fixed stop, `!confirmed` (`SdoClient.cpp:185,194-195,485,519`).
   It never bypasses BUS_OFF, RECOVERING, known STOPPED, INITIALIZING or an exhausted node. The owner returns
   INHIBITED while `external_[c]` is pending or blocked. Ordinary `checkAdmission` does not change. Before
   reopening, invalidate every reservation on the node (DEC-3).
4. Remove the in-place `SdoClient` rebuild at `test/can_bus_health_test.cpp:209-210`, and make the streak
   documentation true.
5. Fix C-06: on trace loss, guard `DeviceEvidence.cpp:195` on no pending request, so the in-flight request
   settles and only the remainder is withdrawn. Size the trace above per-turn output.

**Tests:** change `test/sdo_transfer_test.cpp:308` and `test/device_evidence_test.cpp:578-596` as recorded
contract changes. Add: a read of an absent object leaves the route open; recovery leaves an untouched axis
`unresolved==false`, and it then needs NMT or service evidence for ordinary work; after a marker the old
reservation and old `000F` are refused; an unrecorded stray frame fails the marker; a marker on the same index is refused;
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
   reply would withdraw the stop (`SdoClient.cpp:404`, `TwaiAdapter.cpp:153`). Its replies skip `recordSdo`
   (`:470`), which would otherwise refresh service qualification with false evidence. `transmit`
   (`:318-327`) calls `beforeTransmit` with a null sequence, so a guard must not withdraw a flagged slot.
   Keep ordinary `admit` and `admitUrgent` refusals as they are, so `quarantineAndOtherNodeProgress` lines
   619-634 stay valid.

*Core:*

4. Widen `prepareStop` for mode 1 per the DEC-8 table, and add `predecessorDocumented` to `ActionContext`.
5. Remove `now - e.admittedUs <= age` from `controlEvidence` (`Actions.cpp:119`). The bound applies to reads,
   and a drive idle at `000F` must not lose its stop.
6. Reject at preparation a `maximumAgeUs` below the field count times `queueTimeoutUs + responseTimeoutUs`
   (A-20). Actual cycle time is unknown at preparation, so `Actions.cpp:221` stays reachable through service
   gaps: test that path too, and update the 1000 fixture at `test/reference_action_test.cpp:329`.
7. Add a separate small `BestEffortStop` context keyed by `ReadTarget`, not `ActionContext`, because
   `Actions.cpp:56,91` require a live reservation. It yields word `000B` and needs no state evidence. It never
   reaches SUCCEEDED. A transmitted TX report gives `STOP_SENT_UNCONFIRMED`; possible transmission with no report
   gives DEADLINE; both have `execution=UNKNOWN`. No possible transmission gives NOT_TRANSMITTED. Add the outcome to
   `ActionOperation.h`, and update `Actions.cpp:77-80,215-220` if `ActionContext` shape checks change.
   Separately, make CLEAR_ALARM read ERRORS once, after the alarm clears, instead of on every cycle
   (`Actions.cpp:49-50`), to honor DEC-1.

*Application:*

8. Apply C-10 here. Keep the latest `6040` terminal per axis in ProbeApp's session, and withdraw it on any new
   `6040`, target, mode, RPDO or configuration write, reset, rebind, recovery or uncertain submission, as
   `docs/stop_actions.md:25-28` requires, so a copied `000F` cannot stay eligible (C-19). Steps 5 and 8 share a commit. Console verbs
   wait for prompt 42.
9. A stop never cancels in-flight work. It waits for that work to settle. If the settling request leaves the
   route unresolved and the stop never transmitted, the application issues the best-effort stop with a new
   deadline.
10. Adopt RS: hold a never-transmitted stop (`transmissionPossible==false`, nothing weaker) across one bounded
    recovery, then send it once as `000B` under the deadline fixed at admission. A second fault ends it not
    transmitted. `stop fast` escalates a running `stop normal`, and never the reverse (R-14).

**Tests, as recorded contract changes:** `unsupportedPrerequisitesAreAtomic` lines 426-428 and
`exactDeadlineAndRepeatedStopRejection` lines 799-803 keep rejecting `000B` and released states and reject a
prior `010F` except for `000B`, but now accept trigger words; `freshnessAndStepExhaustion` lines 812-821 drops
the control-age rejection; `test/can_bus_recovery_test.cpp:330,718-719` follow the new urgent rules.

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
- `stop fast` escalates a running `stop normal`; `stop normal` never replaces `stop fast`.
- Idle at `000F` longer than `maximumAgeUs`: the stop is still admitted.

**Docs:** rewrite `docs/stop_actions.md:83-108`, add a best-effort section, and correct `docs/can_owner.md:108-113`
and the `Actions.h` file comment. Update the ledger `implementation_scope` notes for both stop functions and
regenerate.

**Done when:** the full verifier and ledger checks pass and CI passes on the final commit.

### W7. Bring action stack use under budget

**Findings:** A-11, C-15.

1. Add `scripts/check_stack.py`. Run it in the `idf` job (`verify.yml:93`) after `export.sh`, using the
   pinned compilers, and fail if a compiler is missing in CI. `xtensa-esp-elf` covers S2 and S3 after
   `install.sh esp32s3`. Measure `-Os` and `-Og`, because `sdkconfig.defaults` sets no optimization and
   IDF defaults to `-Og`. Include `examples/common`. Budgets: 2,048 bytes per function frame, and a call-chain
   budget of 6,144 bytes through `-fcallgraph-info=su`, leaving a quarter of the task stack for SDK and indirect
   calls. These are project limits; 2,048 has been demonstrated only for `prepareStop`. Skip locally with a
   visible message when no toolchain is present.
2. Remove the candidate copies from `prepare*`, `advanceAction`, `noteActionTransmission`, `advanceRead` and
   `prepareDriverSettings`, and bring these under budget too: `advanceDriver`, `prepareIoSettings`,
   `prepareControlSettings`, `prepare`, `prepareExplicit`, `ProbeApp::request`, `DeviceEvidence::start`.
   Construct in place; never assign a temporary (a probe showed `c = ActionContext()` stays at 8,608 and
   placement new gives 1,152). Preparation can validate first and then commit. `advanceAction` and `advanceRead`
   can fail after mutating a candidate (`Actions.cpp:564-566`), so they need a two-phase design. Rejected calls
   must still leave outputs unchanged; the existing atomicity tests prove it.
3. Shrink `ActionContext`: a stop does not need `originalState`, and the alarm clear keeps only what its incident
   record needs. `originalState` is public and tested (`test/reference_action_test.cpp:441`), so this is a recorded
   contract change. Do not embed a full `ReadContext` in the coming `MoveContext`.
4. Make contexts static or owner-held in the firmware, never task locals. Report the task's high-water mark through
   the diagnostics, and raise `FIRMWARE_STACK_BYTES` only with a measured reason.

**Done when:** the budget check passes on both targets and the full verifier passes. The bench still measures the
high-water mark during a stop (W11).

### W8. Fix the core APIs prompts 33-36 need

**Findings:** A-01 to A-10, A-12, A-13, DEC-6. Reopens prompts 10, 17, 19, 20, 30 and 31; record that in their reports.

1. **A-01:** add the `AxisConfig` native-rate basis as the finding's fix describes, and define `native` for CAN
   velocity in the grammar.
2. **A-02:** document the settled rule in `Settings.h` and in prompts 33, 34, 35 and 36, including retirement of
   retained `MOTION_PARAMETERS` observations.
3. **A-03:** expose `interpretStatusword` and have `getStateInterpretation` call it, amending `State.h:33` ("never
   action admission or completion"). Put the `2100` masks now private to `Actions.cpp:8-10` beside it.
4. **A-04:** merge the two copies of `controlEvidence` into a shared private helper, and rename
   `SettingsReservation` to `AxisReservation` in one mechanical commit (72 occurrences in 17 files, including public
   headers, so add a CHANGELOG entry).
5. **A-10:** create `src/canopen/SdoEvidence.h`, and use it in Reads, Settings, Actions and Position, per the finding.
6. **A-05, A-06, A-07, A-08, A-09 (DEC-6), A-12, A-13:** apply each finding's fix with its regression test. Report
   A-05 and DEC-6 to the RS owner.

**Done when:** the full verifier passes and CI passes on the final commit. Prompt 33's finite lifecycle may then start.

### W9. Port the firmware to the real board

**Finding:** G-04. Record it as a reopened 27/28 assignment and name it in 29's resume conditions.

Deliver single-core operation, a UART console, S2 TWAI retirement evidence, GPIO18 silent control and an S2 compile
fixture. Flash and memory facts are in the bench record: 4 MB flash, no PSRAM. P4 stays in backlog T04. The S2 work
landed in `7764c89` and `c643af8`; build on it rather than redoing it, and check it against this list.

**Done when:** an S2 firmware build passes in CI. No live traffic is part of this packet.

### W10. Execute the prompts

1. Run prompt 40, then 43's offline scope; `43:23`'s probe and read routes follow 41. These have no device gate and
   may run once W1 is done, in parallel with W2 to W9.
2. After W8, finish 33's finite lifecycle on `preparePositionParameters`, then run 34, 35, 36, 37, 38 and 39 in order. Apply C-09 before 38. Each prompt's gated
   features stay visibly gated and do not block its independent work.
3. Run 41, 42, 44, 45, 46, then 47 and 48 when the bench allows, and 49 and 50 as a scoped software candidate listing
   all inherited gates.

### W11. First bench measurements (needs the bench prerequisites and the owner's authorization)

Run with an independent physical stop and the shaft free. Retain raw traffic. In this order:

1. Listen-only capture with the PCAN-USB, or S2 silent mode: heartbeat, boot-up, TPDO and EMCY on the actual bus.
2. Nameplate supply range before energizing (D-07). Power-up shaft state and any holding torque, observed independently
   (D-09). DEC-1 reads on one drive; LED state before and after `603F`.
3. Failed TX count under TPDO load (C-03), and the owner task's high-water mark (A-11).
4. Server order and late-reply behavior for the drain marker (DEC-3, Q18).
5. Enable; stop from `000F`; then `000B` and `010F` from a trigger word in a running move (DEC-8, Q12).
6. `000B` on a released drive: confirm that it does not enable. This comes before any reliance on DEC-2.
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
| Write in flight, stop wants its own deadline | INVALID with its own deadline: `beginOperation` refuses while work is live, and `:189` forces the sequence deadline. Inheriting the deadline is ACCEPTED | `31:23` "its own urgent work with a finite deadline" |
| Move deadline already past | EXPIRED only when reusing the old operation. Idle, `beginOperation` with a new deadline gives ACCEPTED. In flight after an owner turn: TIMEOUT, unresolved, then INVALID | Stop inherits a deadline it should not have |
| Handed-off in-flight request cancelled first | Quarantines the route, then INVALID. A queued-only cancel leaves the route open and is ACCEPTED | Local cancel blocks the physical stop |
| After `invalidate(seq)` while a terminal is retained | INVALID, new reservation refused, until results and the reservation are released | `31:25` requires retaining that terminal |
| Stop on axis 2 while axis 1's stop is unread | RESULTS_FULL on the urgent path only, one urgent slot serves both; ordinary slots still admit | "Stop on another node" in `31:30` |
| Stop from a producer other than the owner of the reservation | No lookup by axis exists, unless the application shares the id | Console stop cannot find the sequence |
| Older queued work on the same sequence | Blocks urgent work until each item is terminal | `:281-284` |

**Fix (W6):** steps 1 and 2. Prompt 31's procedure works around the first, second and seventh rows only
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

**Severity note:** the fake's submit refusal mirrors `IdfTwaiDriver.cpp:137`; the real IDF driver was not run.

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

### C-06 Medium. Diagnostic overflow quarantines the route. CONFIRMED, reproduced; trigger rate unmeasured

`examples/common/DeviceEvidence.cpp:179-183`, `examples/common/CanBusOwner.h:252,256`.
`TRACE_CAPACITY` equals `RX_LIMIT` (8), so one turn with 8 RX frames plus a TX loses
trace entries (rx=7 plus a TX report lost nothing; rx=8 overwrote one). `evidenceLost` then cancels the
transmitted SDO through `ProbeApp.cpp:66-68`. **Fix:** on loss, guard `:195` on no pending request, so the
in-flight request settles and only the remainder is withdrawn. Size the trace above per-turn output.

### C-07 Low to Medium. A same-tick reply overtakes the TX-done report. CONFIRMED, reproduced

`adapters/twai/TwaiAdapter.cpp:114-134`, `examples/common/CanBusOwner.cpp:185-186`,
`:494-498`. The adapter consumes TX-done after `owner.service`. A normal success then
keeps `transmissionConfirmed=0`, and `admitUrgent` returns INHIBITED until the next
owner turn, so a console stop in that window is refused. It lasts one turn, and `confirmed=0` is harmless to
the core (`Actions.cpp:522`, `Reads.cpp:443`). **Fix:** split the adapter into `pollEvents` (before the owner)
and `promote` (after), including `FirmwareProbe.cpp:27-30`. This narrows the race rather than closing it, so
keep the blocked handling.

### C-08 Medium. The production scheduler lives in `examples/`. CONFIRMED

`examples/common/CanBusOwner.*` (namespace `MotorControlCANExample`),
`adapters/twai/TwaiAdapter.h:6`, `examples/common/CMakeLists.txt:4`,
`examples/common/firmware_sources.json:3`. Adapters depend on examples, and firmware
compiles the stack from `experiments/.../vendor`. `CanBusDiagnostics.cpp` defines `CanBusOwner` members. A real
user must copy about 4300 lines plus the pinned CANopenNode sources across three trees, and each later prompt raises the cost of moving it.
Keeping it out of the portable core is correct.

**Fix (DEC-5, W2).**

### C-09 Medium. The owner's TX path is SDO-only. CONFIRMED

`examples/common/CanBusOwner.h:180,192,461`. `Transport.transmit`, `TxReport`,
`ExternalTx` and adapter slots all require `SdoTxId`, and `examples/common` has no NMT
call, so prompts 38/41 would need a parallel TX path. `TwaiAdapter::transport()` also
leaves `beforeTransmit` null, so any future firmware download would skip the pre-TX settings notice
required by `docs/can_owner.md:290`. **Fix:** a `TxToken{kind, sdoId or nmtId}`, an owner `admitNmt`, a separate context for `beforeTransmit`, and NMT's own record, because `ExternalTx` is
indexed by SDO channel (`CanBusOwner.cpp:392-394`).

### C-10 Medium. ProbeApp holds one operation at a time. CONFIRMED shape, PLAUSIBLE cost

`examples/common/ProbeApp.h:126-132`, `examples/common/ProbeApp.cpp:40-45,104-106`.
The owner itself is operation-agnostic: new motion types need no owner change. The
sprawl risk is in `ProbeApp`, with one `read_`, one `pending_` and a reservation tied
to `releaseRead`. Prompt 42 needs concurrent operation, stop and result handling.
`DeviceEvidence.cpp:40-56,92-97` duplicates kind ternaries, and `ProbeApp.cpp:104-106` flattens INHIBITED and
EXPIRED into INVALIDATED. `DeviceEvidence` uses `selected()`, `readContext()` and `pendingRequest()`, so it
changes with the session type. **Fix, no framework:** `AxisSession{binding, sequence,
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

`include/MotorControlCAN/profiles/lichuan_c/Actions.h:7-9`. The header says the application proves there was
no intervening write, but nothing enforces it. Passing a retained `000F` after a `001F` returns OK within
the age bound. Removing the age term (C-18) removes the only accidental limit, so the two go together.
**Fix (W6 step 8):** the application session records the latest `6040` terminal.

### C-20 Medium. One pending frame can quarantine a healthy axis. CONFIRMED by probe; new in verification, exposed by DEC-4

`adapters/canopennode/SdoClient.cpp:504-506` sets `possible` and starts the response clock at adapter handoff, not SDK
submit, and `adapters/twai/TwaiAdapter.cpp:146` holds other frames behind an ACTIVE one. Probe: with axis 1's frame
modelled as pending and no completion, axis 2's urgent `000B` was never submitted, yet ended TIMEOUT, `possible=1`,
unresolved. With retransmission forever, the interface stays stalled for all axes until `recover`, which only the
application can start, and the proved-unsent report arrives after `finish` has already quarantined. **Fix (W3 step 4).**

### Lower-priority owner items

| ID | Where | Defect | Fix |
| --- | --- | --- | --- |
| C-11 | `test/can_bus_owner_test.cpp:78-288`, `can_bus_recovery_test.cpp:65-282`, `can_bus_health_test.cpp:72-260` | Overstated in the first pass. 618 lines of similar fixtures (line similarity 0.27-0.41, and the fakes differ in behavior), plus more in the stop action, drive state and device evidence tests. Only a required `Transport` callback edits every suite; optional callbacks default null | One `test/support/OwnerRig.h` for the truly shared parts; no DSL. Low priority |
| C-12 | `CanBusOwner.cpp:80-84`/`:102-106`, `:153-157`/`:163-167`; `overlap`/`overlaps` in 14 files | Peer scan, admission stats and helper duplicated; `CanBusOwner.h` mixes about 240 lines of value types with the class | `CanBusTypes.h`, one `peerResolved()`, a small scope guard for `entered_`, one private `overlap` |
| C-13 | `examples/common/FirmwareProbe.cpp:27-37`, `EspProbeMain.cpp:47` | Any false return from `ProbeApp::service` or `DeviceEvidence::service` reports "clock fault" and shuts the controller down. A routine admission rejection does not (`ProbeApp.cpp:104-106`) | Rename to `progressionFailed` and report the real reason |
| C-14 | `adapters/twai/IdfTwaiDriver.cpp:217,227`, `TwaiAdapter.cpp:118-120`, `Frame.cpp:9` | PLAUSIBLE: a legal Classical DLC 9..15 counts as `rxDropped`, which inhibits and quarantines | Accept it with an 8-byte payload, normalizing to `dlc=8` |
| C-15 | `examples/common/EspProbeMain.h:20-21` | The 8 KB owner stack's runtime high-water mark is unmeasured. Compile-time frames already exceed it for actions (A-11), and today's deepest firmware chain is 4,752 bytes excluding indirect and SDK calls. `EspProbeMain.cpp:8` rejects S2 at `c329c7e`, so S2 figures are compile-only | Measure the high-water mark on target; see W7 |

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
unit. There is no public motor-turn unit: `TURNS` means load turns (`Units.h:28`). A copied config with
`commandStepsPerMotorTurn=1/1` reaches rpm for mechanical sources (probe: 5) but misconverts a STEPS source
(3200 steps/min, which is 1 rpm, gives 3200). CAN NATIVE accepts only STEPS (`src/Axis.cpp:78-79`), so RS's
`velocity 60 rpm native` grammar is rejected. Prompt 33's `preparePositionParameters` already takes native rpm
(`Position.cpp:86`), so the gap affects velocity and homing rates. EN86 p11 and p13 give rpm; the EDS gives types only.

**Fix:** add an `AxisConfig` native-rate basis, `COMMAND_INCREMENTS` or `MOTOR_TURNS`,
next to `nativeVelocityTime`. For MOTOR_TURNS, non-STEPS sources use a 1/1 scale with destination STEPS, and STEPS
sources divide by `commandStepsPerMotorTurn`. Allow native TURNS for velocity only, because `conversionConfig` also
serves `preparePosition`. Prompts 33, 35 and
36 then share one rate path and add unsigned and width checks on top. Add to 33, 35
and 42: "Never write `effectiveNative` to an rpm object until the basis is rpm.
Quantize once and report error in rpm. Define `native` for CAN velocity in the grammar."

### A-02 High. A move writing parameters through `prepareDriverSettings` invalidates its own prepared target. CONFIRMED, probe, lead checked

**Where:** `src/profiles/lichuan_c/Settings.cpp:329-330`, `src/Axis.cpp:479`.

`noteDriverTransmission` increments `configurationGeneration` even for `6081/6083/6084`,
which do not change coordinate interpretation. So `preparePosition` at generation A,
then a settings write, then `validatePreparedTarget` returns STALE_GENERATION (probe: 7 to 8, and the same on
`preparePositionParameters`' retained target). Homing hits the same trap when a COORDINATES loss or `setAxisOrigin` advances the generation frozen in
its context.

**Fix (section 3, W8):** a move's own parameter writes do not go through settings. Prompts
`33:24` and `34:24` must state this. Homing follows the one-expected-advance pattern that
prompt 32 established.

### A-03 High. The status decoder cannot be used by motion. CONFIRMED, lead checked

**Where:** `src/profiles/lichuan_c/State.cpp:17`, `:62`.

`interpretStatus` is private and reachable only through `getStateInterpretation`, which
needs SDO STATUS and MONITORS blocks. TPDO contexts always fail, because the call to
`getStateBlock` passes no PDO record (probe: TPDO gives detail 1, SDO STATUS-only gives detail 3; `getStateBlock` with
the record returned raw 0x8107, so only the decoder is private). `advanceMove`, `advanceVelocity` and `advanceHome`
must judge bits 8 and 15 and alarm from a TPDO1 or SDO STATUS block, so they would copy the masks. **Fix:** expose
`Status interpretStatusword(uint16_t, StatusLayout, StatusIndications&)`, have `getStateInterpretation` call it, and
amend `State.h:33` ("never action admission or completion").

### A-04 High. The pre-trigger proof is private and the reservation is misnamed. CONFIRMED, partly resolved

`src/profiles/lichuan_c/Settings.cpp:171-191`, `docs/drive_actions.md:76-84`. Prompt 31 added
`ActionContext`; `MoveOperation.h` still does not exist. `controlEvidence` has two byte-identical private copies
(`Settings.cpp:171-185`, `Actions.cpp:106-120`), but the predicates differ: settings requires running clear
(`Settings.cpp:217`) and stop does not. A move needs the settings form. Actions now also use `SettingsReservation`.
**Fix (W8):** merge the copies into a shared private helper, rename the type to `AxisReservation`, and state that moves need caller-supplied `6040=15` evidence until the enable
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
contract, and an external caller printing `status.msg` with `%s` is undefined. No in-repo caller passes invalid
arguments; `reads_preview` never calls `pdoTemplate`. **Fix:** change the generator and regenerate. Do not hand-edit the header.

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
**Fix (section 3, W8):** add a distinct service-timeout outcome to both, before `MoveOutcome` copies it. Update
`reads_test:705`, `can_bus_owner_test:1143`, `device_evidence_test:389`, `tuning_test:696`, the `shape` bound at
`Actions.cpp:61` and the map at `Actions.cpp:176-191`.

### A-09 Medium. A soft-limit overrun cannot be recovered through a witness. CONFIRMED, probe

`src/Axis.cpp:429`, documented in `docs/axis_contract.md:66`, shared with RS. With
limits 0..1000 and witness 1005, relative -10 (endpoint 995) returns LIMIT, and so does
absolute 995 with that witness. `ReferencePolicy::REQUIRED` and wrapped moves can never
recover. **Fix (DEC-6, W8).** Keep the native check and drop the soft check for this case; a probe of this passes every
scenario. Delete the then-unused `integerBounds`, which fails `-Werror`, and change `axis_contract.md:66`.

### A-10 Medium. Evidence checks are hand-copied. CONFIRMED

`src/profiles/lichuan_c/Reads.cpp:232,328,441-451`, `Settings.cpp:125,141,383-392`;
`overlap` in 14 files with three spellings (8 in `src/`, 3 in `adapters/canopennode/`, 3 in `examples/common/`; they
differ only for equal pointers with one zero size, and `Sdo.cpp:50` takes caller sizes);
`gcd`/`magnitude` in `src/Units.cpp` and `src/Axis.cpp`. Prompt 31 added the third copy
(`src/profiles/lichuan_c/Actions.cpp:14-17,106-120,142-157,520-530`; also `Position.cpp:23`). Move, Velocity and Home would add
three more copies of the check that decides whether an effect is uncertain.
**Fix (W8):** one private header, `src/canopen/SdoEvidence.h`, with free functions
`sameSdoWork`, `checkUploadTerminal`, `checkDownloadTerminal` and `overlaps`. This is
not an engine, so it fits `MAINTAINABILITY.md`. Pin zero-size behavior first. Add the header to `test/package_test.py:95`,
or `package_contract` fails. `adapters/` and `examples/` keep their own copies, because a private header is unreachable there.

### A-11 High. Action frames exceed the firmware task stack. CONFIRMED by two independent compile measurements

`include/MotorControlCAN/profiles/lichuan_c/Actions.h`, `examples/common/EspProbeMain.h:20-21`. Atomic
publication through candidate copies puts whole contexts on the stack. Own frames in bytes, with the pinned IDF 5.5.5
toolchain, GCC 14.2 (S3; S2 identical at `-Os`). GCC 8.4 gives nearly the same figures, and no optimization level elides
the context:

| Function | `-Os` | `-Og` |
| --- | --- | --- |
| `prepareStop` | 8,608 | 8,640 |
| `advanceAction` (evidence) | 7,936 | 7,920 |
| `noteActionTransmission` (axis) | 7,920 | 7,936 |
| `prepareClearAlarm` | 7,584 | 7,568 |
| `prepareSetDevicePosition` | 7,584 | 7,600 |
| `advanceAction` (local reason) | 7,552 | 7,552 |
| `prepareDriverSettings` | 4,032 | 4,032 |
| `advanceRead` (reason) | 2,384 | 2,384 |
| `preparePositionParameters` | 816 | 736 |

`FIRMWARE_STACK_BYTES` is 8,192 bytes (`portmacro.h:88,91` `uint8_t`; `sizeof(stack)` at `EspProbeMain.cpp:14,63`).
`sizeof(ActionContext)` is 7,472 on target, `ReadContext` 1,936, `DriverSettingsContext` 2,416. `prepareStop` exceeds the
task stack alone. The worst static chain, `advanceAction` to `finishCycle` to `prepareCycle` to `prepare`, is 12,352 at
`-Os`.

No firmware calls the action or settings functions yet, so this has not failed. Firmware does call `advanceRead`
(`ProbeApp.cpp:69,93,95,106,120`); `FirmwareProbe::service`'s deepest static chain is 4,752 (4,784 at `-Og`), excluding
indirect and SDK calls, under the limit. Unmeasured: the runtime high-water mark. On the bench this would appear as
random faults at the moment of a stop. `docs/fault_reference_actions.md` warns of this without measuring it.

**Fix (W7).** A per-function budget alone does not bound nesting, so W7 also budgets call chains.

### A-12 Medium. Read plans are re-derived by hand. CONFIRMED

`src/profiles/lichuan_c/Reads.cpp:66-108,150-197,679-724,848-863,920-935`,
`Settings.cpp:37-68,413-417`. `DriverGroup` and `ReadKind` are parallel enums, and seven
getters repeat the same preamble (`Reads.cpp:562,591,619,745,817,842,916`). Adding a group for 38-39 takes about six coordinated
edits. `getTuning`/`getIo` live in `Reads.cpp` while `prepareControlSettings` is in
`Settings.cpp`. **Fix:** one private table `{DriverGroup, ReadKind, TuningGroup, Address*, count}`, a
`beginGet` helper, and `DriverReads.cpp` plus `Tuning.cpp`.

### Lower-priority core items

| ID | Where | Defect | Fix |
| --- | --- | --- | --- |
| A-13 | `src/Axis.cpp:93-97`/`:509-512`, `:317-331`/`:460-474` | Polarity workaround and endpoint/path tail duplicated; prompt 34 depends on this logic | Extract `finishPrepared` and `mechanicalUnits`; format as a separate mechanical commit |
| A-14 | `src/Axis.cpp:170,449`, `Axis.h:145` | Overstated in the first pass. `34:23` requires retaining the chosen path and `nativePositive` is discarded, but in 73,332 exact wrapped targets the sign of `requestedNative - retainedReference.nativePosition` never differed from it, so exact paths are recoverable. Only approximate radians (read, not probed) lose it | No W8 change. If prompt 34 needs it, add `nativeDirectionPositive`, native sign, meaningless at zero distance; `pathPositive` misleads under polarity -1 |
| A-15 | `Axis.h:192` | Comment "only this function advances epochs" is false | Reword |
| A-16 | `MotorControlCAN.h:7-13` | Umbrella header says "implemented portable APIs" but omits six core headers (ActionOperation, ReadOperation, canopen/Network, Pdo, PdoState, SdoTransfer), plus the profile headers | Complete it or narrow the comment |
| A-17 | `Types.h:62,89` (generated, so also `scripts/generate_lichuan_objects.py:222`), `Reads.h:232` | `ObjectDescriptor.ownerPrompt` and "Prompt 19/20/21/22" in Doxygen ship process numbers in the API | Keep them in the ledger; remove by prompt 49, with `test/objects_test.cpp:21` |
| A-18 | `src/canopen/Pdo.cpp:251`, `Settings.cpp:308,360` | `pdoFreshness` declared in `PdoState.h` but defined in `Pdo.cpp`; magic readback offset 9 | Named constant; move the function |
| A-20 | `src/profiles/lichuan_c/Actions.cpp:221` | A `maximumAgeUs` shorter than one cycle fails an acknowledged action as OBSERVATION_SCOPE on its first complete cycle, and `running` is never evaluated. No test reaches it | Reject the impossible case at preparation and test the remaining path (W6 step 6). Run: age 20 with 10 us gaps gives OBSERVATION_SCOPE with `observations=0` |
| A-19 | `src/canopen/Pdo.cpp:234-235` | UNVERIFIABLE: no CiA 301 copy is retained, and `CO_PDO.h:141-145` lists only 0, 1-240, 254, 255. Message calls transmission types 252/253 "reserved"; CiA 301 defines them as RTR-only (from memory, no retained copy) | Reword; rejecting them is still correct |

## 7. Dependency graph and shared contracts

### G-01 Critical. Prompts 49-50 cannot close while Q17 stays open. CONFIRMED, computed

**Where:** `38:3`, `README.md:63`, `SEQUENCE.json:497-501`; contradicted by
`README.md:81` and `COMMON.md:38`.

Prompt 38 is the only direct dependent of Blocked 18, and 39, 41-50 descend from it.
A second chain runs 30 to 31-36 to 42 to 43-50. COMMON forbids marking a blocked
predecessor complete, so prompts 47 and 48 send the executor to prompts it cannot finish.

**Fix:** remove 18 from 38 in all three places, which leaves 18 with no descendants (computed). Prompt 38 then covers
node and heartbeat rows plus PDO read rows, and PDO writes stay with 18. Also edit `38:21,24,30`. Change `38:7` to: "PDO remapping remains prompt 18; integrate through this
workflow when 18 resumes." Add a guard near `scripts/check_promptset.py:349-359` that 18
is not an ancestor of 49. G-02 handles the motion chain.

### G-02 Critical. Conflicting rules for when a prompt may start and what Complete means. CONFIRMED

**Where:** `README.md:22,79`, `COMMON.md:36,38`, `PROGRESS.md:115,148`, `START_HERE.md:30,35`.

`README.md:79` says "only when its prerequisites are satisfied", and `COMMON.md:36` allows coherent independent
chunks of a later prompt. Prompts 20-30 ran on Partial predecessors, following COMMON, so that is practice and not a
third rule. `COMMON.md:36` and `PROGRESS.md:115` then disagree about what Complete means. Partial also means both "my work is done" and "my work is
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
| 33 | Pure preparation delivered; finite phases after W1 to W8 | Q02, Q04, Q05, Q10, Q11, Q12 |
| 34 | After 33 | Q04, Q14 |
| 35 | Preparation and timing only | Q03 |
| 36 | Preparation, state machine with fakes | Q21, Q02, Q05, Q10, Q14 |
| 37 | Inventory, capabilities, host selection, passive discovery; `prepareProbe` after W4 and W5 | Q01 |
| 38 | Node/heartbeat read rows; writes gated | Q16, Q18 |
| 39 | Typed save and factory restore | Q15 |
| 40 | **Yes, the best next prompt** | None for local routes |
| 41 | Routes for APIs that exist, after 40 | Follows those APIs |
| 42 | Platform parity; motion routes follow the APIs | Motion APIs |
| 43 | **Offline scope, after 40**; `43:23` needs 41's probe and read routes. Change its prerequisite to 40 and make 44 depend on `[42, 43]`, so 44, 45 and 47 keep 42 as an ancestor (computed) | None offline |
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
(prompt 46 has no physical relevance), with the 44 edit in G-02's table, and make clear that 47 qualifies a finished
feature rather than discovering how the drive works.

### G-04 High. The real fixture has no firmware owner. CONFIRMED

`47:19`, `42:23`, `docs/hardware_bench.md:21`, `docs/firmware_examples.md:7-13`,
`docs/backlog.md:71`, intake `docs/reports/2026-11-02_s2_bench_intake.md`.

The board is an ESP32-S2. Complete firmware is S3 only: `IdfTwaiDriver.cpp` has an S3
retirement guard, `EspProbePlatform.cpp` uses S3 USB, and `EspProbeMain` needs two cores.
There is also no GPIO18 silent control, so normal mode ACKs frames. No prompt 40-50
mentions S2, and acquisition cannot run on the real board.

**Fix:** record backlog T04 as an explicit reopened 27/28 assignment, named in 29's
resume conditions and at `47:3` without a Markdown link there, because linking 27 fails the checker's prerequisite rule.
The S2 work in `7764c89` and `c643af8` overlaps this; recheck it. The assignment covers single-core operation, UART console, S2 TWAI
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
`docs/coverage.md:106`). They include `605D`, `607A`, `607C`, `6098`, `6099`, `609A`,
`60FF`, `2203`, `2300..2302` and all PDO records. Prompt 38's body says only "Earlier
read coverage remains intact". Prompts 33, 35 and 36 therefore lack readback of their
own objects.

**Fix:** of the 13 non-PDO rows, `2203` stays with 38. Reassign the other 12 to their write owners in the ledger
(`2300` to 39, the rest to 31, 32, 33, 35, 36) and regenerate. Prompt 38 keeps 89 rows, including 88 PDO read rows as an
explicit deliverable that does not depend on prompt 18. Update the pin at `check_lichuan_ledger_test.py:275` to 89; a
scratch run shows it otherwise fails (89 != 101).

### G-08 High. CLI routes without owners, and a conflicting console location. CONFIRMED

**Unassigned routes.** `docs/api_cli_mapping.md:143-164` lists `config`, `host`,
`wiring`, `useaddr`, `memory`, `speed`, `accel`, `decel`, `motion` and `stepsperturn`;
no prompt 40-42 assigns them. `41:21` assigns explicit binding, but no earlier route selects a target. RS `moveby VALUE [unit]` has no speed or ramp argument
(`MotorControl-RS/examples/probe_cli/ProbeConsole.cpp:48`), and CAN forbids the boot
defaults it relies on (`api_cli_mapping.md:150`). `motion-profile restore/forget` has no
owning API prompt, while `39:7,24` own generic restore.

**Visibility gap.** `40:16` says unimplemented routes "stay out of working
help/dispatch"; `api_cli_mapping.md:164` and `42:7` say gated capabilities remain
visible. `40:16` also uses `profile list` and `caps`, which need prompt 37's
`getDiscovery*`, and 37 is not an ancestor of 40.

**Fix:** add an owner column to the mapping, which now has a second table at its top. Give `config`, `wiring` and
`memory` to 40, `useaddr` and `host` to 41, and simple-move intent to 42. An unset speed, ramp or scale rejects; `moveto` needs an
explicit `axis origin`. Define two inventory states: implemented, and gated (shown in
`caps` with a reason, absent from dispatch). Prompt 40 ships a compiled-profile inventory
without 37. For location see R-04.

### Other graph and contract findings

| ID | Severity | Where | Problem | Fix |
| --- | --- | --- | --- | --- |
| G-09 | Medium | `49:21`, `45:21`, `36:26` | Exits unreachable while gates are open ("read/move/stop example"; "after 18/20-22 resume"); returning Partial prompts to Pending erases their state | 49: "implemented operations; for gated ones verify the rejection and documented gate". 45: conditional, list the remainder. 36: "keep that prompt Partial and list the row" |
| G-10 | Medium | `36:26`, `46:20`, `49:24`, `50:24`; `docs/architecture_plan.md:56` | Coverage reconciliation demanded four times; `check_lichuan_operations.py` has no owner; 22 and 34 are not ancestors of 36; `607C` is assigned to 32, never mentioned there, used by 36 | 36 reconciles its direct rows by hand, because it precedes 46; 46 creates the checker, and 49 and 50 run it; name `607C` in 32 or move it to 36 |
| G-11 | Medium | `46:24`, `49:25`, `CMakeLists.txt`, `docs/architecture.md:35` | "Core-only and selected-Lichuan consumers" and "selected-profile options" do not exist: one library, Lichuan always included | Redefine as a consumer including only core headers, or assign a real CMake option. Correct `architecture.md`. Tell 33-39 that a new header or source needs `CMakeLists.txt`, `test/package_test.py`, `scripts/check_repository.py` and `test/repository_test.py`; see `c329c7e` |
| G-12 | Medium | `43:32`, `44:32`, `37:38`, `38:38`, `39:38`, `DOCUMENTATION.md:34`, `docs/architecture_plan.md:66` | "Runner guide" has no path; one guide has three names; the plan names `lichuan_api_cli_coverage.md`, which does not exist | Name `docs/runners.md` and `docs/commissioning.md`. Drop that name: objects are in `lichuan_generated.md`, API/CLI in `api_cli_mapping.md`, requirements in `coverage.md` |
| G-13 | Medium | `31:26`, `docs/drive_actions.md:95-102`, `docs/reports/prompt_30.md:123-125` | `31:26` only says "deterministic". Reads and prompts 31-32 already apply one processing-time cutoff, but `stop_actions.md:70` calls it inclusive and `Reads.h:189` calls it exclusive for the same check | Write `now >= deadlineUs` expires, even if received earlier, in `CONVENTIONS.md` section 3, correct those two words, and link it from 33-36 |
| G-14 | Medium | `MULTIDRIVE.md:51`, `45`, `46:18` | Row 45 requires absent ack/done fixtures; 45's body has none, and 46 does | Reword row 45 and extend row 46; the row itself must stay, because `check_promptset.py:368` requires rows 1..50 |

## 8. Datasheet and device-fact findings

Pages are one-based PDF pages. EN86 = `86C_CANopen_user_manual_en.pdf`, ZH86 =
`86C_CANopen_user_manual_zh.pdf`, OP86 = `CL86C_OL86C_operating_manual_zh.pdf`. Each
"CONFIRMED" here means the document gap or conflict is verified; the drive's real
behavior is unknown.

### D-01 High. Prompt 33's phases are in the wrong order. CONFIRMED, lead checked

`33:23` lists "parameter setup, mode confirmation, trigger/rearm". EN86 p12, ZH86 p12
and OP86 p9 give: mode (`6060`, observe `6061`), then controlword 6, 7, 15, then
`607A/6081/6083/6084`, then the bit4 edge. `Settings.h:93` already requires acknowledged `0x000F`, and
`:100` mode 1 and status low bits 7, before writing `6081/6083/6084`, so the listed order would be
rejected. Prompt 34 inherits this.

**Replace with:** "Preconditions: mode 1 observed and drive explicitly enabled (prompt 30;
no implicit enable). Phases in documented order: confirm mode and enabled evidence, write
parameters, write the bit4 edge, confirm the protocol reply, then gather new acceptance
and completion evidence."

### D-02 High. No documented path from homing to a move. CONFIRMED

`33:23`, `35:22`, `36:17`, `47:22-23`. Every procedure (EN86 p12/13/15, ZH86 p14-15) sets
the mode before enabling. No source describes changing `6060` while enabled.
`docs/drive_actions.md:63-67` approves only a position-mode enable and has no release
write. Adjacent facts are not a sequence: the status rows and `2100` bit 4 describe states only. `2202` "回零完成后生效"
(effective after homing completes) and p12's bit 8 imply home-then-move is expected. EDS `[1600]` default maps
`6040`, `60FF` and `6060` together, so a combined write is possible on the wire but undocumented. So the ordinary workflow "home, then move" needs either a `6060` write on an
energized drive or a release, and both are undocumented. `prepareHome` would enable
implicitly (forbidden) or stall. An implementer may silently write `6060` while a target
may be retained.

**Add to 33, 35, 36:** "State the full transition. Enabling in mode 3 or 6 (documented only from a non-enabled start; mode 3 order conflicts,
Q03) and changing `6060` while enabled (no documented sequence, Q11) are separate gated actions. Reject
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

**Add to 36:** "Hold bit4=1 (host word `0x001F`, inferred from mode 1's `15->31`; the manuals give only bit 4 edges) until a terminal result. Write bit4=0 only as
the deliberate interrupt or after completion. Do not share the position rearm path. Bits
5 and 6 are undocumented in mode 6." **To 33:** "Falling-edge meaning in mode 1 is unspecified. Hold bit4 until the move is terminal,
then write `000F` as a visible step (DEC-8)." **To 31, 33, 36:**
"Stops from 31 or 95 are not documented arrows; use the DEC-8 table and carry
`predecessorDocumented=false`."

### D-04 High. Prompts 41, 44 and 47 assume reads Q20 forbids. CONFIRMED

`37:22`, `37:36`, `41:19`, `44:20`, `47:20` versus `examples/common/DeviceEvidence.cpp:86-89`,
`adapters/canopennode/SdoClient.cpp:192-195`, `docs/hardware_bench.md:65-71`. Prompt 47's first step ("Start with
passive/read-only identity/configuration") is rejected by `DeviceEvidence::request`. Only a complete `requestExperiment`
passes that check, and `SdoClient.cpp:192-195` still refuses it on an unconfirmed route. `Reads.cpp:290` covers probe only.
`37:22` states the restriction rather than assuming it; its allowed set is empty. **Replace `47:20` with:** "First a listen-only capture. The first
active upload is a DEC-1 accepted read." `41:19`: `probe` uses the same list, and other objects return
the gate error. `44:20` states the same.

### D-05 High. Communication objects except PDO mappings are EDS-only. CONFIRMED, lead checked

`38:13,17,21,24`, `37:17`. The EN86 pp15-16 table lists only `1000`, `1009`, `100A`,
`1600-1603` and `1A00-1A03`. `1001`, `1003`, `1005`, `1014`, `1017`, `1018`, `1200`, `1400-1403` and
`1800-1803` appear only in the EDS. "NMT", "heartbeat" and pre-operational appear in no
manual; the EDS names `[1017]` "Producer Heartbeat Time" and sets `SimpleBootUpSlave=1`. EN86 p3 says it "supports the
CIA301 protocol" but lists only SDO, PDO and EMCY. Figure labels on unviewed image pages were not checked. So the prompt's "using original
manuals/EDS" and "exact NMT prerequisites" send the coder after rows that do not exist, and
CiA 301 behavior would be imported as a vendor fact. Boot NMT state is undocumented, so it
is unknown whether TPDOs flow or default RPDO1 is live after power-up.

**Add to 38:17:** "`1001/1003/1005/1014/1017/1018/1200`, `1400`- and `1800`-series rows are EDS-only. No manual mentions
NMT, boot state, heartbeat or remap conditions; the EDS gives only names, defaults and DeviceInfo flags. Label standard
behavior 'CiA 301: general vendor claim, service unconfirmed'. Per-row restart and persistence are `unknown`."

### Medium device findings

| ID | Where | Problem and evidence | Fix |
| --- | --- | --- | --- |
| D-06 | `38:17,22,23` | No object or EN86 pp15-20 entry reports DIP position or bitrate. EN86 p10 says only "User-defined"; the link from all-OFF to `2203` is an inference. EN86 p18 gives "CAN ID 0~127" with no activation text. No source offers a software bitrate route | "No object exposes DIP state; all-OFF is a caller attestation. `2203` readback is a stored setting, not the active node; only a reply on the configured server-to-client route (`1200:02`, default node+0x580, unchanged) evidences that. Bitrate is DIP-only in every source: remove it from `38:23` as a software change and keep it as a bench step." |
| D-07 | `39:26`, `47:15`, `LICHUAN_OBJECT_FACTS.md:155`, `docs/hardware_bench.md:59`, Q19 | Three supply ranges, and panel artwork text extraction loses. Panel artwork (EN86 and ZH86 p4, OP86 p3; lead viewed EN86 p4) and the manufacturer page say VDC 30-110, VAC 20-80. Spec tables (EN86 and ZH86 p3) say 36-110 VDC, 18-80 VAC. Wiring (EN86 and ZH86 pp6-7, OP86 p5) says 24-50 V, possibly a 57C carry-over (inference: the 57C panels say 24-50). The conflict is inside each manual. The listing is retained only as a URL, so its AC upper bound is unverified | Record the three-way conflict in Q19 and the facts file. Reword `39:26`: "Panel artwork and the manufacturer page say 30-110 VDC / 20-80 VAC; spec tables say 36-110 / 18-80; wiring says 24-50. Use the nameplate." |
| D-08 | `41:15`, `LICHUAN_OBJECT_FACTS.md:103,17`, `docs/device_conflicts.md` Q02/Q06 | OP86 pp5-14 render pixel-identical to OP57 at 60 dpi, and their running headers say CL57-C. Only the cover, two section numbers, a note color and the p3 panel differ. The `60FE` bits16..18 sentence appears only in OP86 p7, OP57 and ZH57, and covers user-defined DOs (functions 9..11, an inference from the adjacent table). OP86 agreement in status and DO tables looks like independent corroboration but is not | `41:15`: "`60FE` bits16..18 is a 57C-body claim with no 86C CANopen-manual support; it applies only to user-defined DOs." Note in the facts file and C18: "OP86 pp5-14 are identical to OP57; do not count them as independent 86C evidence" |
| D-09 | `47:19`, `48:21`, `39:23` | OP86 and OP57 p5 note 4: "驱动器上电后电机默认即为锁轴状态" (after power-up the motor defaults to shaft-locked; lead checked). Absent from EN86/ZH86 p7, the ledger and the executing prompts; `LICHUAN_OBJECT_FACTS.md:155` records it. It conflicts with OP86 p9 step 2 ("写入完成后，电机即可使能", the motor can be enabled once the writes finish), so power-up torque state is unresolved and `6041` alone does not settle it. Energizing may apply holding current with no CAN traffic, and "no implicit enabling" cannot prevent it | "57C-body manual says the motor locks on power-up; applicability unconfirmed and OP86 p9 points the other way. Treat every power-up or restart as a possible torque state change, observe it independently, and record `2100`/`6041` before any command." |
| D-10 | `39:7,21,23-25,30`, `41:15,21`, `48:21` | "Reset" collides with NMT Reset Node and power cycle. "Restore" means both vendor value 1 and host re-application of writes. EN86 p18: "1: Restoring factory settings". No source defines a restart object | Three terms: `save parameters` (`2300=2`), `restore factory settings` (`2300=1`), `re-apply recorded configuration` (host writes). "No vendor restart command is documented. The manuals' only restart wording, 重启复位, is undefined. This project uses restart to mean a user power cycle, or a separately gated NMT service whose parameter-reload effect is unknown." See R-05 |
| D-11 | `39:17,22`, `38:22`, `48:21` | EN86/ZH86 p15 and OP86 p13 say writing 2 saves current parameters, with homing speed, accel/decel and homing mode as non-exhaustive examples ("此操作常用来保存回零速度，加减速，回零模式等参数"). 重启 (restart) appears only in the alarm tables. `2302` says "manually set to 0"; `2300` has no such text. A boolean restart field forces invention, and copying the `2302` pattern adds a hidden `2300=0` write | "Restart requirement, complete saved set and rearm are tri-state, currently `unknown`. Whether `1017`, `2203` or PDO maps are saved is undocumented. Do not write 0 after a save unless Q15 evidence requires it; if so, make it a visible step." `48:21`: "Count every save and cap the count in the campaign plan." |
| D-12 | `33:17,22`, `36:17,23` | Ranges 0..3000 rpm and 0..2000 ms include 0 with no stated meaning (EN86 p11/13), and Q11 gates zero ramps. A range check admits them (`Settings.cpp` checks upper bounds only), and behavior at 0 rpm is undocumented, so completion cannot be assumed | "The documented range is not the admission range. Reject `6083/6084/609A`=0 under Q11, and `6081/6099:01/6099:02`=0 for motion, before TX until zero semantics are evidenced. `60FF`=0 is equally undocumented (prompt 35); do not treat it as stop." |
| D-13 | `35:21-22`, `33:25` | `6083/6084` are shared by modes 1 and 3 (EN86 p11/13, ZH86 p13). `prepareDriverSettings` requires mode 1 and has no `60FF/6098/6099/609A/607C` | 35, 36: "Own mode-specific parameter writes in the operation; do not route them through `prepareDriverSettings`." 33, 35: "Writing `6083/6084` in either mode invalidates the other mode's snapshot." |
| D-14 | `34:17`, `36:17`, `47:22-23` | `2200` is the only direction-named object (EN86 p18 "Default direction 0: Pulse Sign; 1: Pulse /Sign", ZH86 p18 默认方向; `lichuan_review.md:79` names owners 20 and 34), yet no numbered prompt contains it, although `47:22` says "configured scale/direction". Whether `2200` affects CANopen or homing direction is undocumented; if it does, method 17 could run away from its switch | "Capture raw `2200` in the configuration generation. Direction sign relative to `2200` and the shaft is measured evidence (Q04); a change invalidates prepared work and the reference." |
| D-15 | `36:23` | "Physical prerequisite evidence where needed" names nothing. EN86 p8 gives DI0/1/2 defaults origin/positive/negative limit; the EDS defaults them to 0. `2400:01` polarity is on EN86 p18, logical state `2101` on p17 | "Before admission read `2400:01..08` and `2101`. Require code 1 for methods 24/29, 3 for 17, 2 for 18 (host policy inferred from matching switch and signal names; no document states the requirement). Reject when absent, without writing. Starting on an active switch and meeting a limit during an origin search are undocumented, so they are gated. `60FD` has no documented bit layout (EN86/ZH86 p17 'Digital inputs'); do not substitute it for `2101`." |
| D-16 | `33:17,23` | "Interpreted under the qualified status dialect" implies bit15 varies. All sources agree bit15 is position-mode in-place and bit8 is homing complete; the dispute covers alarm and motion bits. Bit12 exists only in EN86 p12, so ZH86/OP86 layouts have no status acceptance bit. `250B/250C` are named completion threshold and time (EN86 p19, ZH86 p20), and no text links them to bit 15 | "All sources define bit15 as position-mode in-place, listed only in the position-mode table; its meaning in modes 3 and 6 is undocumented. Its deassert behavior, `250B/250C` dependence and soft-limit stop behavior are unqualified. Bit12 is EN86-only; without it, acceptance needs another evidenced signal or the move stays gated. EN86 bit10 is zero speed, not target reached." |
| D-17 | `47:22`, `42:20` | "Qualify explicit enable/release and stop before dependent movement", but no manual gives a bus release command. `6040=0` appears only as a status correspondence with no stated motor effect (EN86 p12, ZH86 p12, OP86 p10), and `2100` bit4 only observes. DI function 4, "Motor enabling signal" (EN86 p8; lead viewed), is a physical input with no stated polarity or behavior, so it is not a qualified route either. `docs/drive_actions.md` says "No approved release write". Qualification deadlocks, or someone tries `6040=0` | 47: "Qualify enable and stop before movement. Release is a separate gated question, not a movement prerequisite, unless a sourced command exists." 42: list release as visibly gated |

### Low device findings

| ID | Where | Problem | Fix |
| --- | --- | --- | --- |
| D-28 | `docs/device_conflicts.md`, W11 | New in verification, lead viewed EN86 pp7-8. The wiring diagrams label DI3 "Enabling signal" (EN86 p7, OP86 p5), but the tables give DI3 default 0, undefined (EN86 p8). DO diagrams show Alarm, Positioning completed and brake release, but DO1 and DO2 default to 0. DI codes 5 "Signal stop" and 6 "Emergency stop signal" exist with no behavior text. External I/O may be unused | Record in the conflicts ledger. Treat the diagram as a wiring example, not a default. DI codes 4 to 6 are not qualified stop or release routes until measured |
| D-29 | `docs/device_conflicts.md` | Reviewer read, not lead checked. DI code 3 is 负限位信号 on ZH86 p8 and OP86 p7, but 反限位信号 (reverse limit) on ZH86 p19. ZH86 p16 agrees with EN86 p16 that `603F` is RW, against the EDS, while C07 cites only EN86. `6099:02` is named "creep time (rpm)" in EN86 p13 and ZH86 p13 but creep velocity in the steps | Add to the ledger as editorial notes. No unit or value changes |
| D-26 | `docs/fault_reference_actions.md:30`, `src/profiles/lichuan_c/Actions.cpp:295-297` (its error string "documented resettable FF05" overclaims too) | EN86 p16 "FF05 refers to too high position deviation" and p20 "Position out-of-tolerance alarm ... Resettable" are linked only by name: p20 has no codes and names no reset route. PLAUSIBLE: the guide also says `603F` may retain its value, so a stale FF05 could admit a clear for a later alarm | Label the link as an inference. Add the stale-value case to Q13 and W11 |
| D-27 | `docs/fault_reference_actions.md:23-28` | PLAUSIBLE: `2301` has no arming zero. If the final 0 was never sent and the object is edge-triggered (undocumented), a later 1 produces no edge | State it in the guide and add it to Q13; no code change without evidence |
| D-19 | `34:17` | "do not substitute 4000 encoder counts/revolution for the configured subdivision" reads as "scale = `2201`", although the sentence opens by requiring a confirmed command-unit scale. No page states pulses per revolution equals `2201`, and the EDS default is 0 | "do not substitute 4000 encoder counts/revolution for a confirmed command scale; `2201` is a candidate and needs Q04 evidence." Note ±1,000,000 pulses is about ±19.5 rev at 51200, so multi-turn absolute targets can be unrepresentable |
| D-20 | `36:17` | "The example" has no antecedent, and 5000 ms is not merely unexplained: it exceeds 0..2000 | Name `docs/reference/public_code/srt/SRT_OpenCan.cpp:146-152`; add wrong-width and out-of-range vectors |
| D-21 | `36:17` | `607C` is in the EN86 p13 table but not in p15's write list | "Write order, units, sign and effect on the final coordinate are unqualified (Q14, Q21)" |
| D-22 | `45:15` | Merges origins: it cites public code and source conflicts together. SRT uses neither `6061` writes nor quick-stop objects; the `6061` write comes from EN86 p12's mistranslation (ZH86 p12 says 监控, monitor) and the quick-stop object from generic CiA402. It omits real SRT defects: gear ratio multiplying ramp times (`:94-95`), 63/127 for ordinary moves (`:98,109`), status timeout returned as "not running" (`:131`) | Label each item by origin, SRT or manual conflict, and add these three |
| D-23 | `47:15` | No alternative to trial writes for the DO dispute. Registers `0x2B..0x2E` agree; with `2400:01..08 = 0x20..0x27` (register = sub + 0x1F), EDS subs `0C..0F` fit and EN86's `0B..0E` do not | ZH86 p19 directly states the EDS numbering (0C=0x2B .. 0F=0x2E), while EN86 p8/p18 and ZH86 p8 use 0B..0E. The formula fails at `0A` in both manuals, so record this as corroboration, not a resolution. In a reviewed read experiment, read `2400:00` and `:0F`, and compare `:0C/:0D` with the DO0 default |
| D-24 | `38:7,17` | Heartbeat unit: EDS `[1017]` gives none. The stack treats it as ms; inhibit and event units differ | "Native unit 1 ms per CiA 301 and public stacks, not vendor-confirmed; verify by measurement. Inhibit (x100 us) and event (ms) units are gated separately (Q17)." `CO_PDO.c:1027,1034` |
| D-25 | `38:21` | PLAUSIBLE: EDS `[1200sub1/2]` is RW, but a setter can sever the only service and recovery route (CiA 301 from memory) | "`1200` is read-only in typed workflows until vendor or firmware evidence supports route writes." |

## 9. MotorControl-RS parity findings

| ID | Severity | Where | Problem and evidence | Fix |
| --- | --- | --- | --- | --- |
| R-01 | High | `35:21` | "Requests specify duration, signed engineering intent, units, native ramp time, stop policy and a fixed service deadline." RS `VelocityRequest` holds no native ramp time or deadline; it holds a ramp policy (`VelocityOperation.h:14-30`). Native ramps are in `VelocityPrerequisites` (`Velocity.h:16`). `deadlineUs` is a separate argument (`Velocity.h:68-71`). CAN's pure `VelocityRequest` already exists (`Axis.h:164-173`) | "Keep `Axis.h` `VelocityRequest` pure. Add a distinct finite-operation request or prerequisites type with `durationUs`, stop policy and verified ramps; pass `deadlineUs` as an argument." Also state: zero speed rejects (`Velocity.cpp:121`); a duration above `maxPolls * pollIntervalUs` rejects (`Velocity.cpp:95`), and it must be below `deadline-now` (`:89`); `serviceVelocity` runs only with no admitted transaction (`Velocity.h:72`); missed when later than `stopDueUs + pollIntervalUs` (`Velocity.cpp:55`) |
| R-02 | High | `44:19` | "`bench_repeat.py` for finite repetition of a named scenario". RS is a fixed comparison of position setup policies, 1..20 repeats (`scripts/bench_repeat.py:1-4,16-27,75`; lead checked). The prompt invites a generic loop over motion, settings or persistence | "`bench_repeat.py`: bounded comparison of position setup policies through ordinary moves, with one planned cleanup. It accepts no arbitrary scenario and never repeats save, restore or settings." |
| R-03 | Medium | `44:19` | "These import `Console` from `bench_probe.py` and interpretation from `_bench_evidence.py`." Only `bench_session.py:11` imports `Console`; only `bench_probe.py:27-28` imports evidence, which is private. Cleanup belongs to the scenario's `run(console, record)`, not the session | "`bench_session` owns `Console`, port and artifacts. Campaigns use `run_recorded` and `bench_probe` helpers. Evidence stays private behind `bench_probe`. Each scenario owns its planned cleanup." |
| R-04 | Medium | `40:15`, `docs/architecture_plan.md:44-45` | `examples/probe_cli/ProbeConsole.*` follows RS, but in CAN that directory is the Arduino entry (`src_dir`, `build_src_filter = +<main.cpp>`) and absent from `firmware_sources.json`. `docs/source_map.md:149-153` deliberately puts console code in `examples/common/`. Native and IDF would reach into the Arduino folder | "Create `examples/common/ProbeConsole.*` beside `ProbeApp.*`." Update `firmware_sources.json`, `examples/common/CMakeLists.txt:11`, `scripts/check_firmware.py:34`, `test/firmware_build_test.py:23` and REQUIRED in `scripts/check_repository.py`. Name `examples/native_probe` as the native entry |
| R-05 | Medium | `39:7,13,21,24`, `CONVENTIONS.md` section 3 | RS has exactly `prepareSave`, `prepareFactoryRestore`, `nextPersistence`, `advancePersistence`, `verifyPersistence` (`Persistence.h:104-122`) and no reset API. RS multi-write restoration is the application `motion-profile restore`. `verifyPersistence` is missing from the audit and CONVENTIONS | Name the five functions. Add the Communication and Persistence families to the CONVENTIONS table |
| R-06 | Medium | `api_cli_mapping.md:150,152,155`, `MOTORCONTROL_RS_AUDIT.md` | RS additions have no adopt-or-reject disposition: routes `velocity read`, `home read`, `home auxiliary`, `home qualification`, `motion-profile start-speed`, `position interrupt`; `prepareVelocityParameters`, `prepareHomingParameters`, `checkHomeQualification`, `revokeHome`, `PositionInterruption`; also `velocity qualification install`, the position-clear qualification route and the start-speed relation route, committed in RS `ba20c8b`. Row 152 `home ...` is only a wildcard. The audit is pinned 58 commits behind `513251d` | Add a dated delta section to the RS audit and one mapping row per addition: adopt, or not applicable with the reason |
| R-07 | Medium, PLAUSIBLE | `37:21-22` | RS core has only inventory (`Discovery.h:31-40`) and the profile header has a one-shot probe (`:43,51`); the scan is application code (`examples/probe_cli/DiscoveryScan.h`). `discoveryProfileCount`, `checkProbe`, `ProbeOutcome`, `ProbeConfidence` may be missed | See G-06 |
| R-08 | Medium | `34:24`, `33:25` | "Apply the existing zero-distance rule", but 33 only says "Define" it. `preparePosition` succeeds with `zeroDisplacement=true` in RS, and `prepareMoveAngle` rejects it, in the helper shared by all `prepareMove*` (`Position.cpp:276`). The RS application reports already-at-target without motion, which is application policy. CAN `PreparedTarget` has neither field, and 34 never names `validatePreparedTarget` | "Move preparation rejects zero effective displacement with no writes; pure arithmetic may report it. Use `validatePreparedTarget` at admission." |
| R-09 | Medium | `33:32`, `45:23` | Defaults differ: `PositionRequest.relative` true in RS, false in CAN; `VelocityRequest.unit` turns/minute in RS, steps/second in CAN; quantization allowance `double` in RS, `Rational` in CAN. A copied vector silently changes meaning | "Comparison vectors set every field explicitly; do not rely on defaults." |
| R-10 | Low | `43:23`, `44:21`, `scripts/verify.py` | RS `bench_probe.py` has no plan or self-test mode; `--list`/`--plan-only` are on campaign tools, with read-only default `--scenario quick`. Do not copy default `COM13` (only on `bench_motion`, `bench_repeat`, `bench_scenarios`; `bench_probe --port` is required, `:3400`) or address 1. `--list` exists only on `bench_scenarios`. CAN already enforces test registration (`check_repository.py:211-213`) | State the spellings and a read-only default scenario |
| R-11 | Low | `41:23` | "Trace" is not a route. RS and the mapping use `debug [off\|raw\|decoded]` and `load` | Use the real names |
| R-12 | Low | `40:23`, `CONVENTIONS.md:281-282`, `api_cli_mapping.md:162` | RS tests also reject `help advanced`, `profile ess_rs enable`, `stop direct`; `40:23` omits `profile ess_rs`; `motor` was never a removed alias | Extend the reject list; call `motor ...` "never-implemented grammar" |
| R-14 | Medium | RS `513251d`, `docs/console.md:196-199`, `docs/bus_owner.md:203-206`, `docs/ess_actions.md:140,165`, `examples/common/ProbeApp.cpp:2167-2208`, `test/stop_path_app_test.cpp:258-275`; `docs/api_cli_mapping.md` | RS added behavior after the audited revision, and CAN has no adopt-or-reject row. Checked against RS code, not only its docs | **Adopt:** a stop with no accepted byte is held across one bounded recovery and sent once; a second fault ends it not transmitted; a lost reply stays UNKNOWN, never resent; `stop fast` escalates `stop normal`; `cancel` of sequenced work or a typed read, and `monitor off`, let in-flight frames settle. **Differ deliberately:** RS has no policy check on supersede (`ProbeApp.cpp:859-869`), so a later `stop normal` could replace a running `stop fast`; CAN escalates upward only. RS keeps the requested policy on re-preparation; CAN re-issues `000B`. **Compare for prompt 33:** `MoveContext::positionMismatch`; moves reject an established (`nativeKnown`) reference that is not idle, stationary and ACTUAL, relative and absolute alike (`Position.cpp:233-236`); the `AxisConfig` note that native bounds also check relative displacement. **Compare for prompt 35:** R-01's duration limits. **Rule shape for prompt 36:** per-method input checks at admission and on each poll when `observeInputs` is set (`Homing.cpp:164,329`); ESS function numbers do not transfer. **Bench runner:** `cleanup_stop_attempts`, `write_rejected`, `home_inputs_permit`. **Not applicable:** ESS target magnitude limit and exception 05 mapping |
| R-13 | Low | RS `Velocity.h:23`, `docs/ess_velocity.md:50` | RS comments say the stop timer starts at "admission", but the code uses preparation time (`Velocity.cpp:144`). Prompt 35 is right | Warn in 35: "Trust the code; 'admission' in the RS comment means the preparation call." |

## 10. Sibling library and FieldCore references

FieldCore has **no** CAN contract. A search of the tracked tree finds only two unrelated lines, `BusId` has no
CAN (`contracts/Health.h:112-120`), and its motor contract is RS485-addressed Move,
ReadState and UrgentStop (`contracts/MotorControl.h:15,54`), pinned to RS v1.0.0. It has
no homing, velocity, angle or drive-persistence operation.

**Replace the recurring audit rule** in `AGENTS.md` (rewrite its Recurring source audit section, or the two conflict),
`COMMON.md:9`, `docs/prompts/ADD_DRIVE.md:9` and each prompt's audit sentence. Only 18 of 50 prompts begin that line with
the literal text, so a single find-and-replace misses 32 variants. 50 of 50 prompts name ADS1115, and 49 of the 52 reports
recording an INA228 revision cite the same older HEAD `b28c5ea`, while RS moves constantly. `check_promptset.py` does not
enforce this sentence, so the edit is safe if the COMMON link and the "Read and audit" heading stay (`:322-324`). Mark the checklist in `ECOSYSTEM_CONVENTIONS.md:346,357`
as superseded.

> For every prompt, refresh MotorControl-RS HEAD and dirty state and read the RS code for
> this seam; RS is authoritative. Check HEAD and dirty state only for siblings this prompt
> names, and inspect only the seam it states. Inspect current FieldCore-node read-only for
> ownership or integration changes. If HEAD, dirty state and seam match the last recorded row,
> write "unchanged since prompt N" and do not reread. A sibling row must name a behavior
> adopted or rejected, with path and symbol. Keep `COMMON.md:9`'s sentence on unavailable sources. This
> does not relax vendor evidence, safety, uncertainty or verification rules.

| ID | Severity | Where | Problem | Replacement |
| --- | --- | --- | --- | --- |
| S-01 | High | `34:13` | No shared I2C convention exists: INA228 keeps output unchanged on non-OK (`README.md:269`), MB85RC and PCA9555 allow unspecified or partial output (`MB85RC.h:502`, `PCA9555.h:261`). The prompt says "inspect", so it misdirects rather than mandates | "Output-on-failure authority is RS `Units.h` and `prepareMoveAngle` tests. I2C libraries differ and set no CAN rule." |
| S-02 | High | `35:13`, `36:13` | "RV3032 bounded cleanup" restores the active EEPROM target or writes a safe hold (`RV3032-C7/docs/ARCHITECTURE.md:403-405`). It invites driver-initiated safe-state writes as a stop model; `ECOSYSTEM_CONVENTIONS.md:195` rejects the motor analogue of restore cleanup | Delete from 35. In 36, at most: "Retain first operation and first stop failure separately (shape: RV3032 `ConfigurationJobReport`). RV3032 is not a stop or restoration model." |
| S-03 | High | `36:13` | "FieldCore reference publication/admission": FieldCore has no homing (`MotorControl.h:13`). `rs485_devices.md:47` contradicts the code at `Rs485MotorModule.cpp:582-607` | "FieldCore has no homing. Compare reference-generation publication, consumption and invalidation (`MotorControl.h:23,33,139`, `Rs485MotorModule.cpp:157,574-607`). `rs485_devices.md:47` contradicts the opt-in `renewReadinessOnState` branch, which no src or include file enables." |
| S-04 | Medium | `33:13` | Retained caller buffers exist in MB85RC only (`MB85RC.h:761,784`), and CAN requires the opposite | "Only MB85RC retains caller buffers across polls; CAN admitted work must own copies, as RV3032 `startWriteUserRamJob` does." |
| S-05 | Medium | `46:7,11,15` | Scheduler and health files have zero motor references; RodTester settings hold only the commissioned motor ratio (`RodTesterSettings.h:39-40`). No CAN need exists. R21 grounds the consumer as a suitability check | "Read `MotorControl.h`, `Rs485MotorModule`, `Rs485Task` motor lanes, `RodCyclePorts`. FieldCore has no motor health row or drive-settings group; a CAN `BusId`, health row and settings are new work." Rename to "owner-module consumer modelled on `Rs485MotorModule`'s prepare/next/advance lanes". Use `MotorControl-RS/docs/fieldcore_handoff.md` as the structural reference, and state that FieldCore has no CAN owner, board route or request for one |
| S-06 | Medium | `39:13` | "FieldCore configuration ownership": no drive save or restore exists | "FieldCore has no drive-persistence precedent; only note that the module rejects runtime address and baud changes (`applyRuntimeConfig`)." |
| S-07 | Medium | `43:11`, `47:11`, `48:11` | `EVIDENCE_REQUIRED` exists only in ADS1115; explicit frames only in INA228; no command-framing option found in SHT3x. The clearest dry-run guard is in PCA9555 | 43: "RS `@ID` protocol and `_bench_evidence.py` govern. Compare ADS1115 contract and evidence verdicts, PCA9555 dry-run guard, INA228/SHT3x exact-version flags; I2C framing is not a model." 47/48: "ADS1115 contract/evidence split and soak bounds; MCP45HVX1 `SKIPPED_UNSAFE` and baseline gate." |
| S-08 | Medium | `37:13` | The I2C scanner is an ACK sweep with no retry; FieldCore's binding inventory is compile-time, while sensor address and baud are runtime | "Compare MB85RC AUTO versus fixed variant. Use FieldCore `rs485 discover` as the bounded-probe contrast. I2C `scan` proves ACK only and is no CAN model." |
| S-09 | Medium | `38:13`, `35:13`, `36:13` | "MB85RC family policy", "INA228 validity", "MB85RC partial-effect handling" have no bearing here | Delete. In 38 keep INA228 `invalidateHardwareState` as a desired-versus-verified analogue |
| S-10 | Low | `40:11`, `41:11`, `42:11` | Largely the same diagnostics and classification audit three times | Audit once in 40. In 41 and 42: "Classify new routes using prompt 40's side-effect classes and completion shapes." |
| S-11 | Low | `34:13`, `35:13` | FieldCore's contract has no angle field (its module sets STEPS at `Rs485MotorModule.cpp:165`) and has no velocity operation | Append "FieldCore has no angle or velocity operation; compare identity and admission only." |
| S-12 | Low | `ECOSYSTEM_CONVENTIONS.md:19,23,27,260`, `CONVENTIONS_REAUDIT.md:225-230` | Revisions drifted; `ownership.md:39` anchor moved to `:54-56`; old stage numbers 02-17 | Add a "historical snapshot; superseded by COMMON" header and correct the anchor |

**Keep these references, they are useful:** `39:13` RV3032 ambiguous-write reconciliation
(`test_primary_cell_ambiguous_write_is_reconciled_without_retry`, a C++ test in `test_datetime.cpp:3772`) and MB85RC `WriteCommit`;
`38:13` FieldCore binding invalidation (`Rs485MotorModule.cpp:659-680`); `42:11`, `45:11`
FieldCore motor result boundary (`MotorControl.h:110-154`, with the v1.0.0 pin caveat);
`33:13` BME280 partial configuration; `37:13` MB85RC family selection.

## 11. Drive-contrast findings

EPOS4 facts come from maxon's Firmware Specification rel13740, fetched during this audit:
[EPOS4-Firmware-Specification-En.pdf](https://www.maxongroup.com/medias/sys_master/root/9579215585310/EPOS4-Firmware-Specification-En.pdf),
310 pages, SHA-256 `66fe32f15ab0bcbf9a008d0d0da8ffe395064c6716edcb3f3b28907f7160e64f`.
Neither the PDF nor anything else from this fetch was added to the repository.

| ID | Severity | Where | Problem and evidence | Fix |
| --- | --- | --- | --- | --- |
| X-01 | High | `46:17-19`, `MULTIDRIVE.md:52` | "Through real common code", "without changing common axis types". `MotorKind`, `FeedbackKind`, `Capabilities`, `requireCapability`, `RampKind`, `NativeRamp`, `ServiceEvidence` exist only in `experiments/api_ownership/Contracts.h:74-151`. `ActionExecution` and `ActionCompletion` are production in `ActionOperation.h:23,30` since prompt 31, but the fixture keeps duplicates at `Contracts.h:176,183`, in the same namespace, so one translation unit cannot include both. No prompt promotes the rest, so 46 needs API additions it does not admit to, or it overclaims | "Before these contrasts, name the production owner of each capability, ramp and outcome type. Promote the needed prototype types in this prompt and remove the prototype copies, or record the contrast as prototype-only. A fixture against `experiments/` is not production evidence." |
| X-02 | High | `46:11,17`, `docs/reference/drive_contrasts.md:16`, `multidrive/SOURCES.json:284-301` | "Original contrast evidence", but no EPOS4 document is retained and the recorded 403 is no longer universal | DEC-7: add to 46: "EPOS4 is remote-only; reopen rel13740 and record the hash actually read." Record the URL and hash above in `drive_contrasts.md` |
| X-03 | High | `46:17`, `test/multidrive_contract_test.cpp:292-298` | "Differing ramp representations" as N5 versus EPOS4: at defaults both are rpm/s (N5 p65, EP p236). The real difference is Lichuan time versus acceleration. The test only asserts two local enums differ, and `Units.h:90-98` has no ramp-time type | "Lichuan time ramps versus N5/EPOS4 acceleration; N5 units follow `60A9`/`6097`, EPOS4 stays rpm/s. Exercise `convertAcceleration`; assert no time/acceleration substitution through a production type." |
| X-04 | Medium | `46:17`, `drive_contrasts.md:52`, test `:156-164` | "Wider signed rates" compares only with RS fields. `60FF` is I32 on N5 (p412), EPOS4 (p256) and Lichuan, and N5 `6042h` is I16 (p349). `multidrive_contract_test` never calls `prepareVelocityTarget`, but `axis_test.cpp:551-555` already proves ±70000 | "Add only a checked width rejection on an I16 target such as N5 `6042h`; no production width helper exists (`Axis.h:174`). This is a width test, not a rate capability." |
| X-05 | Medium | `34:25` | "Float-native conversion ... supported by the contrast evidence": N5 and EPOS4 `60FF/607A` are I32; float evidence comes from moteus and ODrive, owned by 9, 10, 46. `axis_test.cpp:462-500` already covers it | Delete the clause. Add: "A prepared `BINARY32` angle target is rejected by the I32 finite move path before admission." |
| X-06 | Medium | `36:9`, `MULTIDRIVE.md:42` | Method numbers are profile-local. EPOS4 lists 17/18 but not 24/29 (EP p232). N5 `-1` is homing on block replacing method 1's negative limit switch, with index (p84, "no limit switches are used"); EPOS4 `-1` is current threshold, positive speed, with index (p232). Current-position is 35 on N5, 37 on EPOS4. No homing row in the contrast ledger | "Method numbers are profile-local. EP rel13740 p232 lists 17/18 but not 24/29, so by inference from that list EPOS4 would reject those while still supporting homing. Add one homing row to the contrast ledger; no EPOS4 fixture or common numeric method type." |
| X-07 | Medium | `39:9`, `46:19` | "Same object index" hides the real contrast. N5 `2300h` is NanoJ Control; EP has no `0x2300`. N5 and EPOS4 use `1010/1011` signatures | "Lichuan persistence uses vendor `0x2300`; the same index is NanoJ Control on N5 and absent in EP rel13740. N5 and EPOS4 use `1010/1011` signatures, which do not apply to this EDS." |
| X-08 | Medium | test `:103-107`, `drive_contrasts.md:22,50` | `3202h` is a bitfield, not a motor code: bit3 CurRed, bit6 BLDC, bit0 CL/OL (N5 pp297-298). `0x08` means current reduction with BLDC clear. The test writes these into `Endpoint.profile`, conflating motor settings with identity | Remove the assignment. Label them "p38 example values for the `3202h` bitfield", and state which combinations are constrained |
| X-09 | Low | `46:18`, `MULTIDRIVE.md:66`, `drive_contrasts.md:57` | Overstated in the first pass: only `46:18` could read as a pair, because `MULTIDRIVE.md:66` says "or" and C5, C6, C7 are already separate. No retained source pairs extended IDs with RTR. ODrive is 11-bit with optional RTR; VESC is 29-bit data frames; moteus is FD with extended IDs above `0x7ff`. `Frame.h:21,27` caps payload at 8, so C7's 16 bytes never enter the type | "Separate fixtures: VESC extended data; ODrive standard RTR with its empty-data alternative; moteus FD requirement as a flag rejection, stating its payload is not representable in `Frame`." |
| X-10 | Medium | `34:25,32`, `46:17`, `MULTIDRIVE.md:40`, `drive_contrasts.md:3` | Duplicate fixtures: test `:99-176` already covers stepper/BLDC and retains the command and feedback scales, though `:136` claims 1536 feedback counts and asserts only retention of the 2048 scale, so that conversion is unproved. The only new behavior in 34 is a wrapped request reaching `preparePosition`. The 3600 mapping holds only at 1:1, because N5 degrees include gear ratio `6091h` | Step 5 of 34: "Extend `test/multidrive_contract_test.cpp` case C1 with one wrapped-angle vector on N5-documented scale (3600/turn at 1:1) and an abstract non-step servo scale, using production `preparePosition`. No new fixture family." 46 audits completeness and adds only missing cases |
| X-11 | Low | `drive_contrasts.md:51`, test `:121-122` | EP pp221,234 permit only increments, so no independent command/feedback unit at object level | Cite N5 only (`607A` p370 versus `6063` pp360-361); keep EPOS4 as "no step geometry" |
| X-12 | Low, PLAUSIBLE | `46:18`, `MULTIDRIVE.md:67` | "Brake" blurs VESC electrical braking and moteus mode 15 with holding brakes | "Electrical braking modes and holding brakes are distinct; C11 remains abstract." |

## 12. Documentation findings and per-prompt checklist

### Documentation

| ID | Where | Problem | Fix |
| --- | --- | --- | --- |
| N-01 | `AGENTS.md:6,26`; `CONVENTIONS.md:3,44,319,329`; `DOCUMENTATION.md:3` | "Currently an audited implementation promptset, not a working driver"; "not working CAN commands today". False: the verifier, owner and firmware exist. The coder gets the wrong picture of the repo | Delete the planning-only text; describe the implemented and pending scope |
| N-02 | Root `README.md:15-95,106,126`; `PROGRESS.md:3-148` | Reverse-chronological stacked status; README still says to begin at prompt 1; `PROGRESS.md:89` is a 3217-character paragraph with mojibake where an en dash belongs. State must be rebuilt from history | PROGRESS becomes identity, rule, table with Inherited gates and "Next ready"; history stays in reports. README gets one current-status section. Fix the mojibake and add a check for the mis-decoded sequences U+00C3 U+00A2 and U+00E2 U+20AC (`CHANGELOG.md:293,314` has the second) (`check_promptset.py:392` tests only U+FFFD) |
| N-03 | `docs/canopen_services.md:126,138-142` | False: network decoding "remains prompt 15"; owner "explicitly reports `reboot(node)`... `busOff()`"; a false return leaves events for owner dispatch; event codecs "remain assigned". `receive` now routes them itself | Edit the sentences |
| N-04 | `docs/can_owner.md:33-34,388-390` | "All admission, receive, TX and terminal progression passes through this owner", but NMT cannot (C-09). Streak claim unreachable (C-05) | Correct with C-05 and C-09 |
| N-05 | `docs/architecture_plan.md:44-45,56,66`, `docs/architecture.md:35` | Stale paths and "root target is only the common core" | Correct with R-04, G-10, G-11, G-12 |

### What to change in each prompt

Read the cited finding for the wording. "All" items below apply to every prompt in the
range: the audit rule in section 10 and the usable-prerequisite rule in G-02.

| Prompt | Findings to apply |
| --- | --- |
| 31 (Partial; add obligations) | C-01, C-02, C-16, C-17, C-18, C-19, A-20, DEC-2, DEC-3, DEC-8, G-03, G-07, G-13 |
| 32 (Partial; add obligations) | D-26, D-27 (both change guides and Q13, not the prompt), G-03, G-07, G-10 |
| 33 (inherited by 34) | A-08, C-16, G-03, G-07, G-11, G-13, D-01, D-02, D-03, D-12, D-13, D-16, A-01, A-02, DEC-8, R-08, R-09, R-14, S-04 |
| 34 | G-03, G-11, G-13, A-02, A-14, D-14, D-19, R-08, S-01, S-11, X-05, X-10 |
| 35 | G-07, G-11, G-13, A-01, D-02, D-13, DEC-8, G-03, R-01, R-13, R-14, S-02, S-09, S-11 |
| 36 | G-07, G-11, G-13, A-01, D-02, D-03, D-12, D-13, D-14, D-15, D-20, D-21, DEC-8, G-03, G-09, G-10, R-14, S-02, S-03, S-09, X-06 |
| 37 | D-04, D-05, G-06, G-11, G-12, S-08 |
| 38 | G-11, D-05, D-06, D-11, D-24, D-25, G-01, G-07, G-12, S-09 |
| 39 | G-07, G-11, D-07, D-09, D-10, D-11, G-12, R-05, S-06, X-07 |
| 40 | G-08, R-04, R-12, S-10 |
| 41 | D-04, D-08, D-10, G-08, R-11, S-10 |
| 42 | A-01, C-01, C-02, C-10, C-16, D-17, G-04, G-08, R-14, S-10 |
| 43 | G-02 (prerequisite becomes 40), G-12, R-10, S-07 |
| 44 (depends on `[42, 43]`) | D-04, G-12, R-02, R-03, R-10, R-14 |
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
  (`:410-419`). Backticks protect bare paths only: a backticked Markdown link still fails.
- Table column counts must match (`:399-407`), so escape any literal pipe, which matters for G-02 and G-08. Code fences must
  balance (`:393`). The COMMON link must be exactly the file name link (`:322`). `check_lichuan_ledger_test.py:268,275` pins
  ledger counts.
- Tests pin text in prompts 17 and 23, README row 23, `stages==50` and `hashed_artifacts==137`
  (`scripts/check_promptset_test.py:75-76,140-161`). The ledger owner is capped at 50. A prompt 51 passes the checker and fails only
  `stages==50`; an unlisted `31a_x.md` passes silently. Reopen existing prompts instead.
  Requirements must stay exactly R01-R35.
- A new test needs `add_executable(X_test X_test.cpp)` and `add_test(NAME X ...)`
  (`scripts/check_repository.py:206-210`). `library.json` exports must equal EXPORTS.
- Preserve hashed references and `.gitattributes` rules. When touching those, run
  `python scripts/check_promptset.py --check-git-ref INDEX`.

**Not enforced anywhere:** PROGRESS state against prerequisites, paths named in prompts,
ledger owner against prompt text, the API/CLI mapping against route assignments, mojibake.

**Ledger edits** (G-07, Q20 owners, D-07, D-08) go through the reviewed ledger first, then
`python scripts/generate_lichuan_objects.py`, then `--check`. Do not hand-edit generated
files. Keep original conflicting claims visible.

## 14. Checked and found correct

Do not spend time re-verifying these unless their source changes. The device and RS values below were re-checked against
originals in the verification pass. "Absent" claims rest on a text search of EN86, ZH86 and OP86 plus the pages viewed.

**Motion objects (original PDFs and raw EDS):** `607A` I32 ±1,000,000 pulses; `6081` U32
0..3000 rpm; `6083/6084/609A` U32 0..2000 ms; controlword 6, 7, 15, 31, 95, 63, 127 and 11 in all three manuals, 271 and
128 in ZH86 p12 and OP86 p10 only, with bit meanings documented for bits 4 to 7 only; the three-way status table including `2100` inverted bit4;
`60FF` EDS and p17 I32 versus p13 U32, and no documented negative velocity; ZH86 and EN86 velocity
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
save-during-zeroing is real. A GPLv3 copy is retained under `docs/reference/public_code/srt`; none is built into or derived by
the library.

**RS parity:** `AnglePath` and `HalfTurnTie` values; no alternate turn on limit;
`stopDueUs = now + durationUs` at preparation; fixed `deadlineUs`; retained `needsStop`;
cancel without stop; `getHomeReference`; `getDiscoveryProfile`, `getDiscoveryCapabilities`,
`prepareProbe`; all named CLI spellings; 128-byte and 22-token capacity; protocol 3 `@ID`
JSONL; removed aliases rejected; the six-file runner family; `BusOwner` vocabulary; no
`getMoveResult` family; `Status` shape, while `Err` deliberately differs (RS has `CRC_ERROR` and `EXCEPTION`, CAN has
`PROTOCOL_ERROR`). `prepareProbe` lives in the profile header, not core `Discovery.h`.

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
- The stack figures in A-11 come from direct compiles with hand-set flags on the pinned GCC 14.2, not a full ESP-IDF firmware
  build, and no runtime high-water mark was measured.
- DEC-2's safety reasoning about `010F` and `000B`, and DEC-3's in-order server premise, rest on CiA 301/402
  from memory. W11 measures them.
- No drive behavior was measured. Retained targets, mode change while enabled, status-bit
  behavior, zero ramps, negative velocity, stop from word 31, node activation, save
  durability, boot NMT state and read effects all remain open.
- The verification pass rendered and viewed EN86 pp3-4, 7-20; ZH86 pp3-4, 7-8, 11-20; OP86 pp3-7, 9-11, 13-14; ZH57 p4 and OP57
  pp3-4, and compared all 14 OP86/OP57 pages by pixel at 60 dpi. N5 p297 and EP p232 were viewed; other N5 and EP pages were
  extracted text. Unviewed image pages could hold figure labels that a text search misses, including for the absent-NMT claim.
  Reopen the rendered page before relying on an extracted table.
- CiA 301 is not retained, so statements citing it are from memory and marked as such.
- C-01 to C-07 scenarios ran against the real owner, `SdoClient` and `TwaiAdapter` with a fake
  driver, not on the ESP32. Arbitration loss under single shot was not checked on hardware, and no TRM is retained. Probe
  sources are under the disposable `.build/r/` and are not in the repository. Write each regression from the scenario here.
- The drain marker does not exist yet, so its safety findings came from probes that simulated the unresolved flag.
- A comprehensive review of tests for helper-mirroring was not done. No concrete offender was
  found in the files opened.
- Prompts 1-30 were read only where 34-50 depend on them. The prompts 31 and 32 code got one independent
  review for this audit, focused on how later prompts compose with it. Line references for sibling items S-01,
  S-05 and S-08 were not re-read at the current sibling revisions.

## 16. Verification record

Each finding was checked once more, independently, on a frozen copy of `c329c7e`. Reviewers were told to try to refute it,
and device findings had to use original pages rather than repository summaries. The audit lead personally re-checked the
reservation revival in DEC-3, the handoff timing in C-20, the ancestor sets in G-01 to G-03, and EN86 pp4, 7, 8 and 12.

Legend: V = claim holds as first written. C = claim holds, and a detail, citation, severity or fix was corrected above.
O = overstated, downgraded above. U = cannot be verified from what is retained.

| Group | V | C | O | U | Findings |
| --- | --- | --- | --- | --- | --- |
| C-01 to C-19 | 7 | 11 | 1 | 0 | V: 01, 03, 04, 07, 16, 17, 18. C: 02, 05, 06, 08, 09, 10, 12, 13, 14, 15, 19. O: 11 |
| A-01 to A-20 | 6 | 12 | 1 | 1 | V: 05, 07, 09, 13, 15, 18. C: 01, 02, 03, 04, 06, 08, 10, 11, 12, 16, 17, 20. O: 14. U: 19 |
| G-01 to G-14 | 5 | 9 | 0 | 0 | V: 03, 04, 05, 06, 09. C: 01, 02, 07, 08, 10, 11, 12, 13, 14 |
| D-01 to D-27, no D-18 | 8 | 18 | 0 | 0 | V: 09, 13, 20, 21, 24, 25, 26, 27. C: 01-08, 10, 11, 12, 14, 15, 16, 17, 19, 22, 23 |
| R-01 to R-14 | 6 | 8 | 0 | 0 | V: 02, 03, 04, 05, 09, 11. C: 01, 06, 07, 08, 10, 12, 13, 14 |
| S-01 to S-12 | 5 | 7 | 0 | 0 | V: 06, 07, 09, 11, 12. C: 01, 02, 03, 04, 05, 08, 10 |
| X-01 to X-12 | 7 | 4 | 1 | 0 | V: 02, 03, 05, 07, 08, 10, 12. C: 01, 04, 06, 11. O: 09 |
| N-01 to N-05 | 3 | 2 | 0 | 0 | V: 01, 03, 05. C: 02, 04 |
| **Total, 122 findings** | **47** | **71** | **3** | **1** | No finding was refuted |

The CiA 301 parts of D-24 and D-25 are also unverifiable, because the standard is not retained. Their EDS and policy parts hold.

**Decisions.** DEC-2, DEC-3, DEC-4 and DEC-8 needed corrections, now applied. DEC-1 and DEC-7 hold, and the RS claims in DEC-2
and DEC-6 hold.

**Errors in this audit's own first version, found by verification:**

- DEC-3 as written would have revived old reservations and `000F` history after reopening a route. A stray unrecorded frame
  would also have passed the marker. Both are fixed in DEC-3 and W5.
- DEC-8 claimed documented bit meanings for bits 2 and 8, and no manual defines them. It is now stated as a project decision.
- W7's per-function budget did not bound nesting, and `c = ActionContext()` would not have reduced the frame.
- W5 step 2 tested flags that `retire` had already cleared.
- The A-02 rule lost read invalidation, so settings could report success while the drive held a move's value.
- The A-14 fix was unnecessary for exact paths, and `pathPositive` would mislead under polarity -1.
- The G-02 and G-03 dependency edits would have dropped 41 and 42 as ancestors of 44, 45, 47 and 48. G-01 and G-07 contradicted
  each other about 38's PDO read rows.
- C-11 overstated duplicated fixtures, and D-07 and D-08 relied on text extraction that lost panel artwork.

**New in verification:** C-20 (one pending frame quarantines a healthy axis), D-28 (DI3 diagram versus default, and DI stop
functions), D-29 (editorial device notes), and the two-way conflict in D-09 on power-up state.
