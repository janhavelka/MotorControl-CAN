# Prompt 61 Node setting, bitrate procedure, command-PDO maps and save trials

Apply [COMMON.md](COMMON.md). Prerequisites: [38](38_communication_commissioning.md), [39](39_persistence.md), [41](41_cli_device_routes.md), [42](42_cli_motion_routes.md).

## Objective and boundaries

The user's requirements, verbatim (2026-11-12): "No node ID, bitrate or command-PDO setup - is this somewhere in the
roadmap? If not add it and make prompt/s for it"; "Save and factory restore never ran on a drive - add this to the
promptsets todo when the code is in place (when no prompt implements it - add such prompt)".

Facts ([configuration evidence report](../reports/2026-11-12_configuration_evidence.md)):

- **Save and restore:** `2300:00` U16, 1 restores factory settings, 2 saves (EN86/ZH86 p15, p18); `1010`/`1011` are
  absent. The API, host tests and a console route exist (prompts 39, 41), but no firmware image prepares the context,
  and no bench route exists. Unknown: completion, rearming, whether the drive reboots, what is saved, flash endurance,
  the factory values. Measured: `1017`, the TPDO1 map, `2400:01` and `6083` writes were lost at a power cycle without a
  save. Prompt 48 step 3 owns the trial only in part.
- **Node setting:** `2203` U16 RW 0..127, default 0; DIP SW1-5 select nodes 1-31, all OFF means user-defined; both
  drives answer at their DIP node with `2203=0`, which fits DIP precedence without proving it; activation is
  undocumented.
- **Bitrate:** DIP SW6/7 only; no object, no LSS; the EDS 50 kbit/s flag has no DIP setting.
- **Command PDOs (RPDO):** the EDS allows `1400`-`1403` and `1600`-`1603` with 0..4 entries; the live RPDO1 maps
  `6040`/`6060`/`6098`/`607A`, so it can command motion; the EDS marks `2300` and `2203` as PDO-mappable, so a reviewed
  allowlist is required. Prompt 18 writes TPDO1 only; the host cannot send an RPDO today.

Boundaries: every change is an explicit request, never a side effect of startup, discovery, recovery or another
setting; volatile unless a save is requested separately; readback after each write; a save is never automatic and is
counted per node (cap 4 per node, project policy); a factory restore needs a complete backup first and is a separate
decision; proving a save survives needs a drive power cycle, which waits for the user's next natural power cycle or
one batched request (the user asked not to be asked for power cycles); this prompt must not depend on prompt 18,
whose Q17 gates stay Partial, and consumes its artifacts only when available.

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read its `CommunicationPrerequisites`,
`prepareCommunicationConfirmation` and persistence seams; RS is authoritative for common names. Inspect FieldCore-node
read-only for commissioning ownership. If HEAD, dirty state and seam match the last recorded row, write "unchanged
since prompt N" and do not reread. A sibling row must name a behavior adopted or rejected, with path and symbol. This
does not relax vendor evidence, safety, uncertainty or verification rules. Reopen EN86/ZH86 p15, p18-20 and the EDS
rows, `Persistence.h`, `Communication.h`, `PdoConfiguration.h`, the prompt 18, 38 and 39 reports and backlog D09, Q13,
Q15, Q16, Q17 and M09. Apply [CONVENTIONS.md](CONVENTIONS.md), [MULTIDRIVE.md](MULTIDRIVE.md),
[DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

## Implement

1. Persistence bench route and trials (report proposal P1): fresh reads under one-use grants, one `2300` write, reads
   before and after, the save counter and backup rule; trials S0 to S3 on node 1 (change `6083`, save, natural power
   cycle, read before any write, restore the old value and save again); the factory restore S5 last and separately.
2. Node setting (P2): `prepareNodeSetting` for 1..127 with the drive released, refusing 0 and any configured node, the
   operator's DIP attestation and a recorded route back; one write, readback on the old route, at most two explicit
   confirmation reads, never a scan; trials N0 to N3 as in the report.
3. Bitrate (P2b): a documented DIP procedure and a host path to reopen the controller at a new bitrate; no drive-side
   setter; 50 kbit/s refused.
4. Command PDOs (P3, M09): a general RPDO map and COB-ID setting starting with a controlword-only map, an allowlist that
   refuses `2300`, `2203` and similar objects, the measured TPDO1 write order with full readback, COB-ID collision
   checks, and a one-shot host RPDO send for the trials; trials R1 (configure RPDO4, no frame, power-cycle behavior) and
   R2 (an RPDO `000B` to a released and to an enabled stationary drive, then the RPDO event-timer reaction). No motion
   words by RPDO in this prompt; on any problem the urgent SDO quick stop.
5. Console commands for each, calling the API only.

## Verify and exit

Host tests: ACK, abort, lost reply, boot-up during a save, the save cap, a missing backup, never writing `2300=0`, no
retry, node-setting refusals, RPDO allowlist and collision refusals. Run the native, firmware, stack and documentation
gates and CI on the actual final commit. Report each trial with its raw evidence; a trial waiting for a power cycle
stays pending.

## Documentation and delegation

Update the commissioning, persistence and CANopen services guides, the CLI contract, coverage, backlog (D09, Q13,
Q15-Q17, M09), CHANGELOG and Doxygen in the same stage. A stronger reviewer challenges the save, node-change and RPDO
allowlist rules. The lead integrates and runs combined checks. Record `docs/reports/prompt_61.md`. Commit and sync on
the current branch before handoff.
