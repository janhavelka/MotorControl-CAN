# 12B — Nanotec qualification and release reconciliation

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Software closeout can run whenever the implemented subset is
stable; physical qualification needs an identified, available and authorized
Nanotec fixture. The CL86-C bench is not that fixture.

## Work

### Software closeout

1. Reconcile the object ledger, `manual_only` functions, NT-D01..NT-D32 (and any
   later numbers) and `docs/nanotec_canopen_coverage.md` against the callable APIs,
   console routes and tests. Mark missing code separately from unresolved device
   behavior, unqualified firmware rows and their behavior gates, unavailable fixtures and modes refused by host
   policy (Plug & Drive commands, clock-direction, special drive modes, LSS, cyclic
   modes unless 06c was assigned).
2. Record the canopen_shared seams this family claimed, landed or extended (S1, S2,
   S3, S6, S7, S8, S10, S11, S13, and S14 only if 06c was assigned) and consumed (S4,
   S5), with commits, tested
   interfaces and consumer gates, in the canopen_shared README table; S12 stays
   unclaimed unless a later assignment needs guarding.
   Audit demonstrated reuse: one CiA 402 helper set only where identical, one settings
   context, no duplicate SDO, PDO or owner path. Remove accidental duplicates and rerun
   both profiles' tests.
3. Exercise clean source, install and PlatformIO packages, standalone headers, C++11
   consumers of core only, Lichuan only, Nanotec only and both, codec-only linking
   without catalogue strings, and the exact archive set proving no vendor file is
   packaged. Verify every required CI job on the exact final pushed commit.
4. Update README scope, CHANGELOG `Unreleased` (landed code only), profile, discovery,
   CLI and coverage documents, backlog and roadmap to the actual supported subset.
   Review the redistribution terms of the Nanotec originals (no license found, pack
   README) before any public release. Choose a version only under the real release
   policy; no tag or publication is authorized by this stage.

### Physical qualification when a fixture exists

1. Start with [12A](12a_first_contact.md) questions as their routes exist; they do not
   wait for closeout and do not grant authorization. This stage qualifies a named
   subset and does not run every research question automatically.
2. Record per fixture: model and suffix, nameplate hardware revision, firmware build
   from 100Ah, motor and feedback configuration (open-loop stepper, closed-loop
   stepper with encoder, BLDC with Hall, BLDC with encoder; PD4-C fixed), auto setup
   record, brake, switches, wiring, node, bitrate, transceiver, board, image and
   authorization. A FIR-v1650 or FIR-v2538 unit qualifies nothing of FIR-v2213.
3. Begin with non-changing reads and retained raw frames; establish identity,
   control paths, supervision state, boot behavior and timing. Then, with an explicit
   restoration plan: enable and release kinds, halt and quick stop, small finite PP
   moves (absolute and relative with a qualified basis), PV runs of bounded duration,
   method-specific homing with its switches or index. Retain before and after
   settings and every uncertain outcome.
4. Qualify each motor configuration separately; a stepper result does not transfer to
   BLDC or the reverse, nor N5 to C5-E, CL4-E or PD4-C. Measure shaft displacement
   independently when claiming position accuracy. Software timestamps bound
   observations; electrical timing needs measurement.
5. PT, host-loss reactions, brake, outputs, persistence and communication changes
   each need their own fixture element, restart control and reviewed plan. Lost-reply
   and wire-fault scenarios keep no-replay. No unbounded velocity, torque or unattended
   soak is implied.
6. On a failure, retain the evidence, reproduce deliberately within authorization,
   distinguish causes, fix, rerun the original scenario and a short regression, or
   leave the investigation open with the missing measurement named.

## Deliver

A matrix by model, firmware build, motor configuration, image and operation:
implemented, host-tested, firmware-built, hardware-tested, refused by host policy and
unresolved; `docs/reports/nanotec_canopen_12.md` with local and CI results and any
hardware report with its restoration outcome; updated README, CHANGELOG, coverage,
backlog and roadmap.

## Acceptance

Every ledger row and NT-D item has a disposition in the matrix, and no row is marked
qualified without its own fixture evidence. One free-shaft unit never qualifies the
family. CI passes on the exact final pushed commit. Archive completed stage files
only after moving remaining qualification obligations to the backlog. Without
hardware, finish the software closeout and leave physical qualification open.
Commit and push the owned changes.
