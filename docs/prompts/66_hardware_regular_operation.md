# Prompt 66 Hardware qualification of self-repair, regular operations and configuration

Apply [COMMON.md](COMMON.md). Prerequisites: [54](54_hardware_sync_multiaxis.md), [55](55_self_repair.md), [56](56_host_throughput.md), [57](57_regular_drive_operations.md), [58](58_measured_records.md), [59](59_velocity_range_direction.md), [60](60_user_units.md), [61](61_drive_configuration.md), [62](62_stop_line.md), [63](63_mixed_protocols.md).

## Objective and boundaries

Qualify on the two-drive S2 bench, as one combined image, what prompts 55 to 63 delivered: communication self-repair,
node presence and the retry policy; the faster event-woken service; regular enable, release and mode select with the
firmware identity policy; records loaded from the console; the measured velocity range and reverse direction; user
units; the node setting, RPDO maps and save trials that are still pending; the wired stop line once the user has wired
it; and the mixed-protocol identifier separation. Each prompt ran its own focused trials; this prompt proves they work
together under sustained mixed load. Results stay scoped to the actual drives, firmware, wiring and configuration.
Apply the applicable bench authorization and the evidence rules of [prompt 47](47_hardware_finite.md) and
[prompt 48](48_hardware_stress.md).

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read its hardware campaign conventions for this seam;
RS is authoritative. Inspect FieldCore-node read-only. If HEAD, dirty state and seam match the last recorded row, write
"unchanged since prompt N" and do not reread. A sibling row must name a behavior adopted or rejected, with path and
symbol. This does not relax vendor evidence, safety, uncertainty or verification rules. Read reports 54 to 63, the
bench record and the night soak. Apply [CONVENTIONS.md](CONVENTIONS.md), [MULTIDRIVE.md](MULTIDRIVE.md),
[DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

## Implement

1. A short existing-feature regression on the image under test, then each prompt's focused checks once more on the
   combined image, with raw traffic, timing and service gaps.
2. A sustained mixed soak (at least 8 hours, unattended only under a written waiver for that night): both axes
   enabled, moving and stopped through the regular commands and console, records loaded from the console, PDO and
   SDO traffic, injected SDO timeouts and an absent node, with every self-repair, restart and retry counted and every
   uncertain motion command checked for the absence of a replay.
3. Pending trials from prompts 59, 61 and 62 that waited for a power cycle or wiring, when those exist.
4. Root-cause every anomaly with a native regression where representable; a later passing run is not a fix.

## Verify and exit

Each qualified claim names its exact model, firmware, configuration and evidence directory; unavailable checks stay
explicit. Run the appropriate host gates for any fix and CI on the actual final commit.

## Documentation and delegation

Update coverage, the hardware bench record, backlog, CHANGELOG and the guides' qualification columns. One lead alone
controls the port and CAN requests; agents analyze evidence read-only. Record `docs/reports/prompt_66.md`. Commit and
sync on the current branch before handoff.
