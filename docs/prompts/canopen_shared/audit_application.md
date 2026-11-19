# Application of the shared profile promptset audit, 19 November 2026

Each finding of [audit `aud-20261119-171059-b00ba0`](audit.md) was checked against the code
and documents at `f3c3870a` (`main`) before anything changed. Following the user's direction
of 2026-11-19, the fixes stay with the findings and add no unrelated scope. Only the three
stage files of this set changed; no source, test, main prompt or family promptset was
edited. MotorControl-RS was read at `eb0dbad` (dirty: `CHANGELOG.md`, `CMakeLists.txt`, docs):
`include/MotorControlRS/Discovery.h` `DriveProfile` and
`profiles/ess_rs/Discovery.h` `prepareProbe(..., DriveProfile profile, ...)` select a family
by the closed enum. Adopted: the family identity is `DriveProfile`. Rejected: RS has no
application-assigned endpoint number, so CAN carries the enum value in `Endpoint.profile`
and does not add a parameter to every preparation.

## Findings

| Finding | Disposition | Evidence checked | Change |
| --- | --- | --- | --- |
| F1 | Accepted in part | Stage 01's non-CANopen section; old stage 02 lines 32-34, 125-127, 172-173; `63_mixed_protocols.md` objective (a second protocol next to CANopen); `64_common_axis_interface.md` verify list ("mixed profiles under one owner"); `multidrive_decision.md` "CAN itself is another explicit boundary" | Stage 02 split into common, CANopen-only and other-protocol rules. Other protocols use their native service through the real owner after S9 and are Blocked otherwise, never SDO; their ledger is in stage 01's intake rules. Multi-profile consumers run on separate interfaces by default; same-interface negative tests run at registration. A shared-interface positive test waits for prompt 63's artifacts when the family uses another protocol. **Not adopted as written:** for a second CANopen family in the same CANopen domain the gate is a recorded same-domain analysis (node IDs, configured COB-IDs, bitrate, NMT-broadcast and SYNC effects), not prompt 63, which covers a second protocol, while prompt 64 already tests mixed profiles under one owner |
| F2 | Accepted | `Endpoint.h` `Endpoint::profile` (application-assigned `uint32_t`, zero invalid); `src/Endpoint.cpp` `validateBinding` (nonzero only); `Discovery.h` `DriveProfile::LICHUAN_C` (implicit 0); `src/profiles/lichuan_c/Reads.cpp` `validTarget` calling `src/canopen/Sdo.cpp` `validateSdoChannel` (no profile check); `PdoConfiguration.cpp` and `Reads.cpp` `pdoProfile(binding.endpoint.profile)` with `src/canopen/Pdo.cpp` requiring `layout.profile == endpoint.profile`; Lichuan tests use 41, examples 1 | One representation chosen in stage 02 "Profile resolution": `Endpoint.profile` holds the family's `DriveProfile` value, with explicit nonzero values from `LICHUAN_C = 1`; one pure lookup in `Discovery.h` also checks `Endpoint.protocol`; unknown, zero and mismatched values refuse; preparation, owner defaults, console and JSONL use it; static relation, changed only by rebinding. Explicit migration (refuse other values, move callers, keep `PdoProfile.profile` equal, CHANGELOG). Rejected: casting the implicit zero (invalid in a binding); an application table (every preparation would need it or a caller-asserted family, the label-only gap of F3) |
| F3 | Accepted | `Discovery.h` `DiscoveryCapabilities` ("no field grants a write"); `src/profiles/lichuan_c/Position.cpp` `checkMove` consuming `IdentityObservation` with `sameFirmware`; prompt 57 "Identity policy"; [backlog](../../backlog.md) D18 (`1018` collisions between DS-C, TL-C and CL86-C) | Stage 02 "Identity and device evidence": per operation the consumed identity by ledger key, holder and invalidators; bootstrap read versus later writes; tests with the correct label and wrong, missing, ambiguous and stale identity; exact allowed traffic. Coverage rows gain "identity consumed". Lichuan's policy stays prompt 57's |
| F4 | Accepted | `CanBusTypes.h` `SdoReuse` constructor and `SdoReusePolicy::ACCEPTED_RISK_WITH_MARKER`; `CanBusOwner.h` `setSdoReuse` ("the default, before any call, is SdoReuse()") and private `resync_[CHANNELS]`; `CanBusOwner.cpp` terminal path calling `armResync` on TIMEOUT, CANCELLED or BUS_OFF with possible TX, the controller-event retirement loop, `resyncRoutes` issuing an `ACCEPTED_UNQUALIFIED` marker; `SdoClient::configure` refusing an already configured route | The policy is per channel binding, so route-specific policies already exist; the defect is the Lichuan default on every binding. New seam S10 (README); stage 01 owner row corrected; stage 02 CANopen-only rule: reviewed policy or `NONE` before first admission, non-Lichuan default `NONE` through S2, Lichuan unchanged, tests for timeout and late reply on a two-family owner without configuration |
| F5 | Accepted | `adapters/canopennode/SdoClient.cpp` `validBestEffortStop` (fixed `6040:00`, two bytes `0B 00`, `checkShape` against the channel's binding, COB-IDs and generation only) and `checkBestEffortStop`; `CanBusOwner.cpp` `admitBestEffortStop` (cancels automatic markers, ends resync steps, sets `stopped`); `Actions.h` `BestEffortStop`, `advanceBestEffortStop`, `STOP_SENT_UNCONFIRMED` | Today the Lichuan word is admissible on any configured channel whatever its family. Stage 02 S3: opt-in per binding with a reviewed exact shape; admission checks the exact current binding and channel generation; refusal without TX and without fallback; Lichuan semantics and tests unchanged; direct-admission tests listed. README S3 row updated |
| F6 | Accepted | `scripts/check_lichuan_operations.py` `check` (rglob of `include/MotorControlCAN`; every header with public functions must be a mapped family) | Pack, ledger, generator and checker rules moved from stage 02 to stage 01 "Intake rules"; S7 lands before or with the first new public header, claimed by that stage, with disjoint-and-complete and still-fails tests; registration consumes them. README rows and S7 owner changed |
| F7 | Accepted | Old README lines 22-30; `COMMON.md` usable-prerequisite rule; AGENTS Git ownership (lead commits) | README "Seams: claim, land, consume": claim (lead, stage, date) before shared edits; landed commit, tested interface and consumer gate; consume a passed artifact regardless of its prompt's state; block only the dependent deliverable; what runs before S4 and S5. Table gains claim and landed columns |
| F8 | Accepted | `examples/common/ProbeConsoleText.h` `start` (fixed `"profile":"lichuan_c"`); `scripts/console_jsonl.py` `PROFILE` and `record_problems`; record types `reply`, `terminal`, `trace`, `help`, `capability` and others | Stage 02 S6 record rule: `null` without a target or for an unresolved target, the target's `profileId` on targeted replies and refusals, the admission `profileId` on operation records and retained results after rebind; validation against the request correlated by `id`; schema revision with the runner, `s2_console.py` and tests together; listed tests |
| F9 | Accepted | `scripts/check_promptset.py`: `reference_manifests` glob of `docs/reference/*/SOURCES.json`, required packs `multidrive`, `other_can`, `public_code`, `check_records(..., within=refs)` with size and SHA-256 checks on files and Git blobs | Stage 01 manifest row and intake "Pack" bullet (discovered, not edited; optional required-pack entry with a negative test); old stage 02 instruction removed; README checks paragraph says it verifies artifact sizes and hashes and compiles nothing |
| F10 | Accepted | `Actions.h` `StopKind` (HALT 010F in modes 1/3, derived 000F in mode 6; QUICK_STOP 000F to 000B) and `stopPredecessorAllowed` (DEC-8) | Stage 01 Lichuan-specific list describes StopKind as mode- and predecessor-specific, points to its declaration and keeps it apart from the DEC-2 best-effort stop |
| F11 | Accepted | AGENTS CI policy; old stage 02 status and acceptance | Stage 02 status separates standing rules, registration acceptance (once, and again for major changes: full matrix and exact-commit CI) and other stages (affected local checks, required automatic checks, checks not run recorded); README table rows split accordingly |

## Unresolved questions

1. **Answered by the code.** Reuse is selected per configured SDO channel binding:
   `CanBusOwner::setSdoReuse(const Binding&, ...)` stores `resync_[channel].reuse`, and a
   channel is configured once. Every channel starts with `SdoReuse()`. The owner can emit the
   first default marker itself, with no application call, in the first `service` turn
   (`resyncRoutes`) after an ordinary request on an unresolved route ends TIMEOUT, CANCELLED
   (including `recover()`'s cancellation) or BUS_OFF with possible TX, or after a controller
   event's retirement. That happens once the request frame and its cleanup abort are locally
   retired, the controller is usable, no stop waits on the node, the urgent slot is free and
   marker budgets are known (configured, or inherited from the last request). It never
   follows a best-effort stop. A two-family owner can therefore hold different reviewed
   sets; S10 covers the unsafe default.
2. **Answered in part.** No such artifact exists today: every console SDO read goes through
   `ProbeApp::requestOrdinaryRead`, `requestIdentity` or `requestDiscoveryRead`, typed on
   `LICHUAN_C::ReadKind` and `ReadTarget` (`ProbeApp.h`; `DeviceEvidence.cpp` uses the same
   path); only NMT reaches the owner directly (`ProbeDevice.cpp` `admitNmt`). The family's
   console probe, common-verb dispatch and API/CLI parity are therefore Blocked until S4.
   The API-level probe, refusals and owner tests can close the rest of registration.
   **Open:** whether a minimal family read may extend the Lichuan-typed `ProbeApp` before
   prompt 64 replaces it. That is a decision for the prompt 64 lead or the user.
3. **Answered by the code: by pattern.** `library.json` `export.include` lists `include/`
   and `src/`; `scripts/export_package.py` `export` walks directories recursively;
   `scripts/check_repository.py` `EXPORTS` requires exactly that allowlist. A family edits
   neither file. `test/package_test.py` `HEADERS` and its exact archive set, root
   `MOTORCONTROLCAN_SOURCES` and `install(FILES ...)` are explicit per family. The old
   stage 02 instruction to edit "the export list in `scripts/check_repository.py`" was
   wrong and was removed. Anything under `include/` or `src/` ships, so vendor files never go there.

## Remaining open items

- Question 2's `ProbeApp` decision, above.
- S2 changes the public meaning of `Endpoint.profile`. FieldCore-node was not inspected
  here. The S2 lead checks it read-only for `Endpoint.profile` values before landing the
  migration.
- F1's same-domain CANopen gate (a recorded analysis instead of prompt 63) is a scoping
  decision recorded above. The user may require prompt 63 for every shared interface instead.
- Concurrent, untracked family promptsets (`lichuan_c_extensions`, `lichuan_ds2_canopen`,
  `maxon_epos4_canopen`, `mks_servo_can`, `moons_canopen`, `nanotec_canopen`) were not
  edited. Each should be rechecked against S2, S3, S6, S7 and S10 as revised here.

Checks: `python scripts/check_promptset.py` reports no error in `docs/prompts/canopen_shared/`.
Its remaining errors are missing stage links inside those untracked family promptsets.
`git diff --check` is clean for this set. Nothing was compiled; no hardware was used.

## Round 2: family audits, 19 November 2026

Two family audits at `139748e9` raised cross-cutting findings: MOONS'
`aud-20261119-175226-dd78d8` and Nanotec `aud-20261119-175227-c23b91` (reports kept by the
auditor, outside the repository). Only the shared-contract part of each was applied here;
the family sets take their own fixes. Each item was checked against the contract text and,
where named, the code before it changed. Only this set's README, 01, 02 and this file
changed.

| Item | Source findings | Evidence checked | Change (where) |
| --- | --- | --- | --- |
| 1 S13 SYNC admission | MOONS' F1; Nanotec F5 | README S13 row (unknown or harmful effects only); `CanBusOwner.h` `armSync` ("Caller ... resolves any remote buffered RPDO hazard first"), `admitSync`, `disarmSync` (never clears a remote RPDO buffer) | 02 CANopen-only S13 bullet: declared bus population, live NMT and PDO state, known-harmful effects (pending synchronous RPDO, watchdog fed for another application, program start), revocation during a schedule, zero SYNC TX on refusal, the three acceptance classes with a non-target node; README S13 row and 01 SYNC row |
| 2 S14 RPDO data TX | Nanotec F12; MOONS' unresolved question 2 | `CanBusTypes.h` `BusRequest` (an `SdoTransfer` only); `CanBusOwner.h` admission surface (SDO, best-effort stop, drain marker, NMT, SYNC); `canopen/Pdo.h` `PdoObject` ("grants no ... RPDO TX"); prompt 51 item 6 and its report (no RPDO path enabled) | New README S14 row; 02 CANopen-only S14 bullet (binding, SYNC coordination, lease, stale setpoint retirement, urgent stop, bus-off; consume or stay unsupported; no family raw transmitter); 01 owner and PDO rows |
| 3 S3 default deny with S2 | Nanotec F1 | `SdoClient.cpp` `validBestEffortStop` (fixed `6040:00`, `0B 00`, any configured channel); `CanBusOwner.cpp` `admitBestEffortStop`; `Sdo.cpp` `validateSdoChannel` (CANopen bindings only) | 02 S3 bullet: default deny lands in the S2 commit, tested after timeout and rebind; family shape later; README S3 row; 02 Deliver |
| 4 S7 package entries | Nanotec F13; MOONS' F14 (claim timing) | `scripts/export_package.py` via `library.json` (recursive); `test/package_test.py` `HEADERS`, exact archive set and install set; root `CMakeLists.txt` explicit `install(FILES ...)`; `test/installed_consumer/CMakeLists.txt` per-header `headers_11`/`headers_17` | 01 S7 bullet: install rule, package-test entries and per-header compile in the first header's commit; shared registration is an S7 edit claimed first; 01 packaging row; 02 package bullet and status line; README S7 row and work table |
| 5 First-contact questions | MOONS' F10; Nanotec F9 | `docs/device_conflicts.md` "Conditional experiment prerequisites"; 02 bench section (no first-contact rule) | New 02 subsection "First-contact questions": production route, vendor evidence or a reviewed isolated experiment with actions, observation, snapshot and restoration boundary, otherwise NOT RUN; no raw bypass or weakened refusal |
| 6 Write preflight | MOONS' F2 | 01 ledger rule (`review.write_policy`; an RW flag grants no write); 02 had no preflight rule | 02 "Operation admission", write preflight bullet |
| 7 One operation deadline | MOONS' F12 | 02 said only "one immutable deadline per operation"; `communication_practice.md` C10; `src/profiles/lichuan_c/Position.cpp` `download` copies the operation deadline into each transfer | 02 "Operation admission", deadline bullet: captured at preparation, bounds every inner deadline, checked before each changing step, family coverage rows and cumulative-delay test |
| 8 State-transition table | Nanotec F3 | AGENTS (manual arrows are examples); 02 had only "never enable to prepare a stop" | 02 "Operation admission", state transitions bullet: source-state and intent table before the first TX, already-reached state, refusing states, first-frame tests |
| 9 Principles pack | MOONS' and Nanotec unresolved questions on the principles input | `git show --stat e02ab04f`; `docs/reference/canopen_principles/README.md` (AKD remote-only). No shared text called the pack working-tree-only, and none cited it | 02 CANopen-only first bullet: link to the committed pack, its AKD remote-only status and its secondary role |

Not applied here, as family matters: MOONS' F3-F9, F11, F13 and Nanotec F2, F4, F6-F11,
F14, F15. MOONS' F5 (mode -2 positive path) stays refusal-only under S13 as written; a
positive SYNC-triggered program start would need its own shared-contract change.

Checks: `python scripts/check_promptset.py` passes; `git diff --check` is clean for this set.
Nothing was compiled; no hardware or port was used.

## Round 3: cross-cutting findings of the family round-2 audits, 19 November 2026

Audits at `edcf9621` (gpt-6.2-sol, standard depth): MOONS' `aud-20261119-184315-2fffe2`,
Nanotec `aud-20261119-184316-126cde`, C extensions `aud-20261119-184724-01623c`, DS2
`aud-20261119-184724-17daf9`. The lead verified and applied the findings that recur across
families once here and in prompt 57; each family's own round-2 application consumes them.

| Theme | Findings | Evidence checked | Change |
| --- | --- | --- | --- |
| 1 Stops on a refused identity | MOONS' F2; Nanotec F5; C extensions F1 | Prompt 57 Identity policy (stop exemption from `d0108871`); C extensions 01 withdraws all traffic on CONTRADICTED | Prompt 57: the exemption removes only the identity refusal (route, generation and source-state checks stay), a later outcome change does not withdraw a prepared or queued stop, other pending changing steps retire, an NMT command is a changing request and never this stop. 02 "Identity and device evidence" makes prompt 57's outcomes, precedence and stop exemption the policy of every CANopen family (a family may refuse more, never admit more) |
| 2 Identity precedence | C extensions F3; DS2 F1 | Prompt 57 outcome definitions; 02 test bullet refused "ambiguous" identity, which contradicted prompt 57's "the declaration selects" | Prompt 57 "Admission by outcome": CONFIRMED and UNCONFIRMED with a declared-model match admit (the result names every matching model); UNCONFIRMED without such a match, CONTRADICTED and no current outcome refuse vendor downloads and changing requests, stops exempt. 02 tests aligned. DS2 F1's refusal of a colliding sibling match is rejected as policy: the declaration selects (engineering direction 2026-11-17, no lock-out); the family records the ambiguity in the result |
| 3 Timed runs and their stop | MOONS' F4; Nanotec F8; C extensions F6; DS2 F3, F4 | `include/MotorControlCAN/VelocityOperation.h` `durationUs` (preparation-anchored); `src/profiles/lichuan_c/Velocity.cpp` (`requiredStopBudgetUs` must fit before the deadline, transfers bounded by `stopDueUs`, `needsStop`, phase `STOPPING`) | 02 "Operation admission", new timed-runs bullet: families reuse that design; the run's stop is the application's separate urgent stop, never barred by the run's deadline, identity or a missing acknowledgement; a run without a reviewed stop row for its mode is refused before its first changing TX |
| 4 S3 shape and an unknown predecessor | Nanotec F6; C extensions F2; DS2 F3 | Principles pack controlword table and CP-D10 (Quick stop command: transitions 7, 10, 11, or 11 only; no enabling transition) | 02 S3 bullet and README row: the shape establishes no state; a family opts in only with a review of the word's reaction from every state reachable after an uncertain write, including a set halt bit |
| 5 S13 and NMT | MOONS' F1; Nanotec F1; C extensions F4; DS2 F5, F6 | 02 S13 bullet (node-0 NMT named but no entry point, no default deny, no NMT-driven revocation) | 02 S13 and README row: gate at the owner's NMT admission; default deny in the S2 commit (no SYNC production and no node-0 NMT on an interface with a non-Lichuan binding); families supply per-command NMT effects; admitting an NMT command that can change an evaluated node's state revokes the approval first; new tests |
| 6 Registration acceptance without an identity outcome | Nanotec F10; DS2 F7 | 02 "Registration acceptance" and "Deliver" | 02 "Registration acceptance": when registration delivers no outcome it shows every identity-dependent route unavailable; the identity refusal tests land with the identity stage; the probe's read effect is reviewed in the family ledger before it runs |

Family-specific findings of these audits are applied in each family's `audit_application.md`
round 2.

## Round 4: cross-cutting findings of the round-2 applications and round-3 audits, 19 November 2026

Sources: MOONS' round 3 `aud-20261119-191534-e56216` (F1, F2, F5), C extensions round 3
`aud-20261119-191534-a7642a` (F3), and the Nanotec and DS2 round-2 applications' requests.

| Theme | Findings | Evidence checked | Change |
| --- | --- | --- | --- |
| 1 Stop path after a lost start reply | MOONS' F2; C extensions F3 | 02 S10 (`NONE`: a timeout leaves the route unresolved); 02 S3 (default deny); 02 timed runs (a stop row only) | 02 timed runs: admission also needs a stop that can leave on an unresolved route, the binding's opted-in S3 shape or the family's native equivalent, or an application-declared independent stop named in the result; otherwise refused before the first changing TX |
| 2 Halt bit in stop words | MOONS' F1; Nanotec round-2 F6 (N5 pp68/76/77/80/89: clearing bit 8 restarts motion; p57 gives no priority) | 02 State transitions; principles pack CP-D10 | 02 State transitions: a stop word keeps a set halt bit unless the family's evidence shows quick-stop precedence; unknown halt keeps bit 8 set where admitted, else the row refuses; tests include halted and unknown-halt predecessors |
| 3 Stranded NMT-Stopped node | MOONS' F5; DS2 round-2 request | Prompt 57 admission by outcome (no current outcome refuses changing work); a Stopped node answers no SDO | Prompt 57: targeted NMT Enter Pre-operational to a node whose fresh heartbeat or node-guarding state reports Stopped is a bootstrap command, admitted under every outcome except CONTRADICTED; without such a report the node is reported stranded and recovery is the application's |
| 4 Heartbeat producer bound | Nanotec round-2 open question 2 | README S11 row | S11 row: the period check counts TX delay under load and the producer is rechecked when a `1016h` write is handed to the controller |
| 5 Owner-level S3 boundary for Lichuan extension models | C extensions round 3 F1 | 02 S3 (Lichuan bindings keep DEC-2); C extensions round-2 lead decision 2 (family-local refusal) | 02 S3: an explicit per-binding no shape overrides the profile default, set before first traffic and kept across rebind; TL-C and CL2 use it, so a direct `admitBestEffortStop` sends nothing. Supersedes the C extensions round-2 lead decision 2 |
| 6 Unknown halt state | DS2 round-3 F4 application (every stop row refused on attachment without history) | Theme 2's rule; principles pack controlword table (bit 8 halt is a standard CiA 402 bit); stop priority | 02 State transitions: with the halt state unknown a stop word sets bit 8, since setting halt cannot start motion; a row refuses only where the family's evidence gives bit 8 another meaning; a stop is never refused merely for an unknown halt state |
| 7 Homing as a host-ended run | C extensions round 4 F2; DS2 and MOONS' applications extended the stop-path rule to homing by analogy | 02 timed runs | 02 timed runs: homing is such a run, with a caller-supplied latest interruption time in place of the duration, an optional travel bound from observations of bounded age, the same stop budget, transfer cap and stop path |
| 8 Undecided identity reference | C extensions round 4 unresolved question | Prompt 57 outcome definitions (CONTRADICTED: none of the declared model matches; unread fields listed under UNCONFIRMED) | Prompt 57: a reference with an unread, wrong-width or aborted required field is undecided; CONTRADICTED needs every declared-model reference decided non-matching, otherwise UNCONFIRMED without a declared match |
| 9 Stop path on a resolved route | DS2 round 4 F2; Nanotec round 4 F1 | 02 timed runs (S3 covers only unresolved routes); State transitions (stop rows need fresh source state) | 02: a stop word reviewed from every reachable state (the S3 review) is admitted without a fresh report and records the state unknown; the timed-run stop path must also hold on a resolved route with a stale, missing or alarmed report, otherwise the declared independent stop |
| 10 Halt evidence from words sent | DS2 round 4 F1 | 02 State transitions | 02: only an acknowledged bit-8-clear word with no later possibly sent bit-8-set word makes the halt state clear |
| 11 Queued NMT | DS2 round 4 F4; Nanotec round 4 F6 | 02 S13 (admission at request time only) | 02 S13: NMT admission is rechecked at hand-off; a binding change, conflicting operation (in-flight save included) or stale evidence retires a not-yet-sent command; a handed-off one is reported possibly sent |
| 12 Deadline for the declared independent stop | C extensions round 5 F1 (aud-20261119-201350-68dc0f) | 02 timed runs (declared stop named, no invocation rule) | 02 timed runs: the declared stop has a caller-supplied fallback deadline; the application invokes it when the urgent stop is not admitted, misses its window or leaves the effect unconfirmed at that deadline; admission, TX or acknowledgement never retire the need |
| 13 Leaving the explicit no shape | C extensions round 5 F5 | 02 S3 (no path back to the profile default) | 02 S3: clearing the explicit no shape restores the profile default only after the route and previous work are reconciled, before the re-declared binding's first traffic, with a readback |
