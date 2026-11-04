> Executed handoff, retained for traceability. Do not execute this copy. All 22 packets ran between 2 and 4 November 2026. The [execution table](../../../reports/handoff_19_33/README.md) records what each delivered, each packet report records what remains, and the [backlog](../../../backlog.md) rows H03 and H04 track the physical trials still owed. Relative links were relocated; task wording is retained from `2920479`.

# Packet 10: Shared evidence, settings generations and state interpretation

Execute this packet fully. Apply [COMMON.md](COMMON.md). Prerequisite: packet 9.
Complete the remaining W8 scope before declaring W1–W8 closed. Read A-02/03/04/
06/07/08/10/12 in the active audit, original 17/19/20/30/31 and their actual
installed headers, implementations, callers and tests. Refresh RS operation
terminal evidence and I2C configuration-validity seams.

## Implement concrete shared seams

Rename `SettingsReservation` to `AxisReservation` across actual consumers,
including the newer PDO experiment and offline preview. One mechanical commit
must include build/package/docs/tests and the migration entry; do not leave
parallel reservation identities. Its meaning is actual continuous axis exclusion,
not a permission token fabricated by a profile.

Consolidate the duplicated private control-evidence check while preserving the
different action predicates: settings requires nonrunning; stop must work while
running. Keep current session history authoritative. Expose the audit's small
`interpretStatusword` value function and make `getStateInterpretation` reuse it.
Put reviewed2100 masks alongside it. Preserve unspecified layout, unknown bits
and contradiction reporting. SDO/PDO callers can share decoding but must still
prove their own route, freshness and firmware applicability. A decoder is not
admission or completion evidence by itself.

Create the private `src/canopen/SdoEvidence.h` functions described in A-10 for
actual repeated work/terminal/overlap validation. Pin zero-size and alias behavior
first. Reuse them in Reads/Settings/Actions/Position where meaningful; keep
application/adapter helpers outside the private core boundary. Update package
source inventories. No generic operation engine, virtual profile or parser DSL.

Specify A-02's distinction and wire it into current settings/cache behavior:
explicit settings calls advance configuration confidence; a future move's own
607A/6081/6083/6084 writes are phases of that move and cannot invalidate its own
prepared target. They must retire retained MOTION_PARAMETERS observations so a
later settings no-op cannot rely on old values. Document the exact cache/owner
invalidation hook to be called by packet 20; do not add a fake MoveContext here.
Test the hook with concrete work fixtures, including contradictory readback.

Add distinct service-timeout outcomes instead of mapping a queue/response timeout
before the overall deadline to DEADLINE. Keep possible transmission independent.
Withdraw earlier typed blocks on CORRELATION_LOST while preserving historical
terminal bytes. Fix `pdoTemplate`'s null status message in its generator, then
regenerate. Consolidate A-12's duplicated read plan/getter entry checks into one
small private table/helper and cohesive read files. Preserve group-specific
partial validity, native unknowns and atomic getters; update CMake/package lists.

## Tests, closure and delegation

Use independent evidence-corruption/timeline tests while one implementer owns
shared types and generation changes. Test every mismatched target/node/channel/
epoch/object/command/width; early service timeout versus overall cutoff; already
published terminal stability; stale earlier blocks; cached settings invalidation;
status-only/PDO raw decode; null-message negative; and untouched outputs. Extend
installed examples and tests with the new public contract. Reuse `reads`,
`settings_write`, `drive_state`, `stop_action`, `reference_action`, `objects`,
PDO and owner suites, full verifier, ledger/generator and firmware gates.

Audit the packet 1 W1–W8 table against actual delivered tests. Remove the temporary
override only when its stated removal conditions hold; keep unresolved firmware
facts visible. Update every reopened original report and packet 10, required CI
and sync. This exit unlocks production finite-motion work only for consumed
qualified contracts. It does not qualify a release command, moving stop, relative
wire base or a physical scale. Packet 11 now builds on these repaired boundaries.
