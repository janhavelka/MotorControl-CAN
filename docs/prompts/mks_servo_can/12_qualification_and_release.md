# 12B — Qualification and release reconciliation

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Software closeout is actionable at any time; physical qualification needs an identified,
available and authorized MKS fixture with the owner present. The CL86-C bench is not that fixture.

## Work

### Software closeout

1. Reconcile every ledger row (all codes and forms of MK109, MK109A, MK106 and MK100, prose-only functions) and
   MK-D01..MK-D37 against callable APIs, console routes and tests. Mark missing code separately from unresolved device
   behavior, unavailable fixtures, unsupported dialects and deliberately never-written commands (FFH, 9AH, 50H, 0FH).
2. Exercise clean source, install and PlatformIO packages, standalone headers, C++11/C++17/no-RTTI consumers, the
   MKS codec-only consumer, consumers of core only, Lichuan only, MKS only and both, and the multi-profile contract
   tests. Verify all required CI jobs on the exact final pushed commit.
3. Audit demonstrated reuse: the matcher, stream table, unit helpers and one settings context. Remove accidental
   duplicates introduced by this family while keeping distinct dialect semantics; rerun affected tests. Confirm no
   CANopen type, SDO evidence or statusword projection leaked into the profile.
4. Update README scope, `docs/mks_servo_can_coverage.md`, `docs/profile_contract.md`, the CLI contract, API/CLI map,
   source map, backlog, roadmap and release notes to the actual supported subset. Packaging and green builds do not
   qualify any board, firmware edition or operation. Choose any version under the real release policy; no tag or
   publication is authorized by this stage.

### Physical qualification when the fixture exists

5. Start from [12A](12a_first_contact.md) results; this stage qualifies the resulting named subset and does not run
   every research question automatically.
6. Record board code and revision, firmware and edition as reported by 40H and the screen, declared dialect, motor,
   supply, wiring, termination, bitrate, IDs, check mode, reply policy, host board and image, and authorization.
7. Begin with non-changing reads and retained raw frames; establish reply shapes, readable settings, timing and error
   counters. No automatic writes, stall clears, mode changes or probing by motion.
8. A physical case counts toward qualification only under a fixture-specific procedure written, independently
   reviewed (DELEGATION) and approved by the lead under the applicable bench authorization before the run. It names,
   per case: the permitted motion envelope (travel, speed, acceleration and duration for this fixture and load); the
   stop and service timing criteria and how they are measured (step 9); the abort action and who or what takes it;
   and the expected end and restoration state. A run without such a procedure, or one that leaves it, is retained as
   an observation and its qualification row stays open. Under that procedure, with explicit stop prerequisites and a
   restoration plan: enable and release; small finite encoder and pulse moves
   in each direction once direction is qualified; mode stops, F7H and F7H 01 where the dialect has it; finite speed
   runs with drive run time and with host-bounded expiry; homing on the actual fixture; settings through each
   persistence class; streams and latching. Retain before and after settings and every uncertain outcome.
9. Qualify OPEN, CLOSE and vFOC modes independently and 42D and 57D boards independently; one board does not qualify
   the other. Software timestamps bound observations; electrical timing needs measurement.
10. Restart, persistence and communication-change cases only with actual restart control and a restore path.
    Deliberate wire faults and lost-reply scenarios keep no-replay and need an explicit bounded plan. No unbounded run,
    moving link-loss test or unattended soak is implied.
11. On failure, reproduce deliberately within authorization with bounded diagnostics; after a proper fix rerun the
    original scenario and a short regression, or leave the investigation open with the missing measurement named.
12. While COMMON records the owner as away, no physical item here runs; each becomes a deferred-table row. When new
    behavior reaches an authorized MKS bench, run a short regression of already-qualified MKS operations plus the
    focused new checks.
13. Give the support matrix and the ledger reconciliation to an independent reviewer (DELEGATION) with the code and
    evidence, not the author's conclusion; reconcile its findings against source and tests.

## Deliver

A support matrix by board code and revision, firmware and edition, dialect, work mode and operation with independent
columns: documented, implemented, host-tested, firmware-built, hardware-tested, unsupported and unresolved. Attach local
and CI results and any hardware report with restoration outcomes. `docs/reports/mks_servo_can_12b.md`, final PROGRESS
state, and remaining qualification obligations transferred to the maintained backlog.

## Acceptance

Every ledger row has a final disposition; every MK-D is closed with cited evidence for a named scope or kept open with
the next measurement. CI is green on the exact final commit with the commands it ran listed. Every hardware-tested
entry cites its approved step 8 procedure and the measured criteria it met; a run without one is an observation and
qualifies nothing. No claim extends one board, firmware edition, mode or motor to the family. If hardware is absent, the truthful software closeout is
complete and physical qualification stays explicitly open. Archive completed implementation stages only after their
remaining obligations are in the backlog. Commit and push the owned changes.
