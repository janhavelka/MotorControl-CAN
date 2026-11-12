# Prompt 54 Hardware qualification of SYNC, motion changes and multi-axis operation

Apply [COMMON.md](COMMON.md). Prerequisites: [48](48_hardware_stress.md), [51](51_sync_producer.md), [52](52_motion_changes.md), [53](53_multi_axis.md).

## Objective and boundaries

Qualify on the two-drive bench what prompts 51 to 53 delivered: SYNC production and the synchronous PDO behavior the
trials established, the change operations that measured as working, concurrent motion of both nodes with independent
stops, the group stop and the synchronized start with its measured skew. Results stay scoped to the actual drives,
firmware, wiring, motors and configuration; no family-wide extrapolation. Apply the applicable bench authorization and
the evidence rules of [prompt 47](47_hardware_finite.md) and [prompt 48](48_hardware_stress.md).

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read its hardware campaign conventions for this seam;
RS is authoritative. Inspect FieldCore-node read-only. If HEAD, dirty state and seam match the last recorded row, write
"unchanged since prompt N" and do not reread. A sibling row must name a behavior adopted or rejected, with path and
symbol. This does not relax vendor evidence, safety, uncertainty or verification rules. Read reports 51 to 53, the
[night soak](../reports/2026-11-12_night_soak.md) and the bench record. Apply [CONVENTIONS.md](CONVENTIONS.md),
[MULTIDRIVE.md](MULTIDRIVE.md), [DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and
[DELEGATION.md](DELEGATION.md).

## Implement

1. A short existing-feature regression on the image under test (enable, moves, velocity, stops, console), then the
   focused series below; every run retains raw traffic, timing and service gaps.
2. SYNC: production at the chosen periods with bus load and service gaps; synchronized TPDO sampling of both nodes;
   the synchronous RPDO start trigger and its skew (backlog M13, HM6). The user's target period is 10 ms ("10ms
   updating would be enough for all of my purposes", 2026-11-12), so the qualification run is: both nodes at a 10 ms
   SYNC for at least 30 minutes with one TPDO per SYNC per node (jitter, missed and late samples, bus load, service
   gaps), then, only where prompt 52 opened it, a 10 ms set-point stream latched on SYNC (`60FF` in mode 3; `607A` in
   mode 1 only if its handshake measured as workable), with a stop of one axis during the stream and the group stop.
   A 10 ms result that fails is reported with the longest period that passed.
3. Changes during motion: each operation prompt 52 opened, at several speeds and ramps, with stops during and after a
   change (backlog F13).
4. Multi-axis: both nodes moving at once with one stopped while the other continues, the group stop including a lost
   reply and a refused axis, a fault on one axis with the other stopped explicitly (HM1 to HM9), and a sustained mixed
   soak.
5. Stop reactions (backlog M14 to M16): NMT Stop to one node during a slow velocity run (6 to 30 rpm), then
   Pre-operational and reads of `6041`, `6064` and `606C`, then the run's own stop, reported as stopped, kept running
   or faulted; the shared quick-stop RPDO on both nodes during motion, with the skew between the two stops; and, once
   the user has wired a stop signal to a DI, the DI stop (5) and emergency-stop (6) functions during motion, with the
   final state and what re-enabling needs.
6. Root-cause every anomaly with a native regression where representable; a later passing run is not a fix.

## Verify and exit

Each qualified claim names its exact model, firmware, configuration and evidence directory; unavailable checks stay
explicit. Run the appropriate host gates for any fix and CI on the actual final commit.

## Documentation and delegation

Update coverage, the hardware bench record, backlog, CHANGELOG and the guides' qualification columns. One lead alone
controls the port and CAN requests; agents analyze evidence read-only. Record `docs/reports/prompt_54.md`. Commit and sync
on the current branch before handoff.
