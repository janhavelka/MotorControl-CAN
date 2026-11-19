# 12 — Qualification and release reconciliation

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Software closeout is actionable after 11. Physical work requires an identified, available and
authorized DS2-family fixture; the CL86-C bench is not that fixture. MULTIDRIVE rows 47, 48, 49, 50 and 66 apply.

## Work

### Software closeout

1. Reconcile every ledger entry, `manual_only` row, coverage row and DS-D01..DS-D34 (DS-D33 and DS-D34 from stage
   01's intake corrections) against actual callable APIs, console routes and tests. Mark missing code separately from unresolved device
   behavior, unreachable P-parameters, unavailable fixtures and undocumented modes. No placeholder counts as coverage.
2. Confirm the Lichuan C boundary held: no DS2 code uses a must-not item of the execution contract, no Lichuan C
   behavior or frame changed, and shared seams S1, S2, S3, S6, S7, S10 (and S8 if extracted) and the identity
   comparison of prompt 57's policy have one implementation each with both families' tests.
3. Run clean source, install and PlatformIO consumers, standalone headers, C++11 and C++17, no-RTTI and codec-only
   links without catalogue strings, the multi-profile consumers, the DS2 and Lichuan C checkers, quick and full
   verification, all firmware gates of the contract, and required CI on the exact final commit.
4. Update README scope, `docs/profile_contract.md`, `docs/coverage.md` and the DS2 coverage file, the CLI and API
   maps, backlog (D18 disposition for DS2, the common torque intent, `MotorKind` for servo motors, the prompt 70/75
   identity claim set), roadmap and CHANGELOG to the actual supported subset. Choose any version under the real
   release policy; this stage authorizes no tag or publication.
5. Audit reuse: remove accidental duplicates introduced by DS2 while keeping distinct device semantics; rerun affected
   tests.
6. Reconcile declarations and decisions: list every caller declaration the API offers (MEASURED identity references, rate and
   ramp units, signed encodings, relative basis, index availability, wiring) and every `DS-DEC-n` with its selector,
   date and scope. A declaration never appears as a closed DS-D; a decision taken while the owner was away stays
   marked for the owner's review.
7. Release notes. If a release is prepared under the real policy, `docs/releases/<version>.md` lists the DS2
   subset by model with its separate host, firmware-build and hardware verdicts and the unsupported set with reasons;
   vendor material stays out of every archive (`test/package_test.py`).

### Physical qualification when the fixture exists

1. Start with [12a](12a_first_contact.md) as soon as each route and the fixture exist; it does not wait for closeout
   and grants no authorization. This stage qualifies the resulting named subset only.
2. Record the exact model, suffix, encoder, brake, supply, firmware identity and identity outcome, wiring and DI
   assignments, node, bitrate, termination, image revision and authorization. Model identity comes from the
   declaration and nameplate, never from a reply.
3. Begin with non-changing reads and retained raw frames. Then explicit enable and release, small finite position
   moves with prepared stops, relative and absolute policies, and only then bounded velocity, torque and homing runs
   with their written plans. Negative encodings run only after 12a Q8a's or Q8b's conditions are met.
4. Qualify each status dialect, unit, ramp, stop and completion rule per model and firmware. A DS2-10C result does not
   qualify IBL60-C or ISL80-C, and one free-shaft specimen does not qualify the family or a loaded axis.
5. Brake, I/O, limit-switch, homing-switch and restart cases need their actual fixtures and restart control. Wire
   faults and lost-reply cases need an explicit bounded plan and preserve no-replay. No unbounded run, moving link
   loss or unattended soak is implied.
6. When a scenario fails, retain it, reproduce deliberately within authorization, establish the cause, fix it with a
   native regression where representable, then rerun the original scenario and a short regression. Leave an
   unresolved investigation with the exact missing measurement when the cause is not established.
7. When new DS2 behavior reaches the fixture, run a short existing-feature regression (probe, identity, state, one
   small finite move with its stop) plus the focused new checks, and retain the evidence with the image revision.

## Deliver

- A matrix per model, firmware identity, image and operation: documented, implemented, host-tested, firmware-built,
  hardware-qualified, unsupported (with reason) and unresolved (with the DS-D or `DS-G`).
- `docs/reports/lichuan_ds2_canopen_12.md` with local and CI results, the declarations and decisions list, and any hardware
  report with restoration outcomes; updated user documentation, backlog, roadmap and PROGRESS.

## Acceptance

Every documented native function and common operation has one disposition in the matrix, and no row claims more than
its evidence. Required CI passed on the exact final commit. If no fixture exists, the software closeout is finished
truthfully and physical qualification stays open in PROGRESS and the backlog. Implementation stages are archived only
after their remaining qualification obligations moved to the backlog. Commit and push the owned changes.
