# 14 — Physical qualification and release reconciliation

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Software closeout can run at any time after the implemented stages; physical work requires
an identified, available and authorized MOONS' fixture. The CL86-C bench is not that fixture.

## Work

### Software closeout

1. Reconcile every ledger entry of both layouts, every `manual_only` function and MO-D01..MO-D33 (plus any later
   IDs) against callable APIs, console routes and tests. Mark missing code separately from unresolved device
   behavior, unavailable fixtures, unsupported modes and the TSM/TXM-C Part B block.
2. Check the coverage file: one row per common operation and per vendor operation, with native objects by
   ledger key, staging and trigger order, acknowledgement and completion evidence, stop interaction,
   `RetryClass`, gates and tests, or the evidence-based reason it is unsupported.
3. Exercise clean source, install and PlatformIO packages, standalone public headers, C++11 consumers of core
   only, Lichuan only, MOONS' only and both, catalogue-free consumers and the vendor-material exclusion in
   `test/package_test.py`. Run `python scripts/verify.py --mode full`, `python scripts/check_firmware.py
   --target all` with the pinned IDF and the family checks; verify CI on the exact final pushed commit.
4. Audit reuse: the seams this family landed or extended (S1, S2, S3, S6, S7, S10, S13 if stage 09 claimed it, S8 if
   extracted) have one implementation each, recorded in the canopen_shared table. Remove duplicate helpers this
   profile introduced while keeping distinct device semantics. Rerun affected tests.
5. Update README scope, CHANGELOG `Unreleased`, `docs/profile_contract.md`, `docs/coverage.md`, the API/CLI map,
   CLI contract, source map, backlog and roadmap to the actual supported subset. No release tag or package
   publication is authorized by this stage.

### Physical qualification when the fixture exists

Start from [14a first-contact questions](14a_first_contact.md) as their routes and the fixture exist; they do
not wait for software closeout and grant no authorization. This stage qualifies a named subset.

1. Record product label, part number, firmware strings, motor, encoder, supply, wiring, switches, bitrate,
   adapter, image and authorization. A responding node does not identify the product; use the stage 03
   identity rules and the 14a results.
2. Begin with non-changing reads and retained raw frames: identity, configuration, state, PDO maps, heartbeat
   and EMCY. No automatic writes, alarm clears, setting changes or probing by motion.
3. Establish command and stop prerequisites and a restoration plan, then qualify per operation: enable and
   release, mode select, small finite position moves, set-point buffering, halt and quick stop, finite velocity,
   SSDC profile torque at small native values, homing per method with its switch fixture, I/O with its loads,
   watchdog arming at standstill, store and restore with restart control. Retain before/after settings and
   every uncertain outcome.
4. Qualify STF-C and SSDC-C independently, and each firmware string separately. Measure units and feedback
   before claiming physical position or velocity accuracy. Software timestamps bound observations; electrical
   timing needs measurement. Mode -2 and Q programs are qualified only with owner-supplied programs.
5. Deliberate wire faults and lost-reply scenarios keep no-replay and need an explicit bounded plan. No
   unbounded velocity run, moving link-loss or unattended soak is implied.
6. If a scenario fails, retain the exact revision, configuration, intent, raw traffic and timing; reproduce
   deliberately within authorization; fix the cause with a native regression; rerun the original scenario and a
   short regression. A later pass, longer timeout or retry is not a fix.

### Checks

Software closeout runs, on the exact final commit: `python scripts/check_promptset.py` (and
`--check-git-ref HEAD`), `git diff --check`, the three family checkers and the generator `--check`,
`python scripts/check_repository.py`, `python scripts/verify.py --mode quick` and `--mode full` (gcc and
clang builds as in [verification.md](../../verification.md)), `python scripts/check_stack.py --strict` where
the IDF job runs it, `python scripts/check_firmware.py --target all` with the pinned IDF, and every required
CI job. Report each result, and each check not run with its reason.

## Deliver

A matrix by exact model, firmware, image and operation: documented, implemented, host-tested,
firmware-built, hardware-qualified, unsupported and unresolved, with local and CI results and any hardware
report with restoration outcome; the updated documentation set of item 5; `docs/reports/moons_canopen_14.md`.

## Acceptance

Every ledger row, manual item and MO-D item has a final disposition with evidence. One free-shaft STF-C does
not qualify SSDC-C, TSM/TXM-C, other firmware or the -FC variants. Archive completed implementation prompts
only after moving remaining qualification obligations to the backlog. Without hardware, finish the software
closeout and leave physical qualification explicitly open. The lead commits and pushes the owned changes.
