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
