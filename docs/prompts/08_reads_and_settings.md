# Stage 08 Implement typed reads, settings and native access

Apply [COMMON.md](COMMON.md). Prerequisites: stages 01, 04, 06–07.

## Audit

Read RS `Reads`, `DriverSettings`, `ControlSettings`, `Tuning`, observation getters and whole-candidate validation tests. Compare BME280 staged configuration, PCA9555 output validity, MB85RC variant policy and INA228 read freshness. Inspect FieldCore motor observation projection and actual typed API callers.

## Deliver

1. Implement `prepareProbe`, `prepareIdentity`, `prepareState`, `prepareConfig`, `nextRead`, `advanceRead` and checked typed getters such as `getIdentity`, `getConfig`, `getStateBlock`. Preserve raw identity/model/firmware values and model-match confidence. Probe is minimal and non-changing; it does not start NMT or sweep all state.
2. Publish observations only from matched service/PDO evidence with node binding, object/map/configuration generation, provenance and supplied timestamp. Multi-object reads are not necessarily an atomic snapshot. Expose per-block validity and skew where needed.
3. Implement typed settings groups for motor/current/holding/tuning, scale/subdivision, soft limits, digital I/O/functions/polarity/filter, motion parameters and supported native controls. Follow direct RS group/accessor names where semantics match. Keep physical wiring, configured function and observed level independent.
4. Validate a complete candidate before any write is yielded, including cross-field constraints, reserved bits, state requirements, units and family-specific bounds. Configuration sequences retain partial effects and uncertain writes, perform documented readback and invalidate dependent axis/mapping evidence. No implicit save/reset/enable.
5. Expose all reviewed read/settings groups assigned to stage 08 through typed functions, including groups with no common-axis analogue. Assign actions/motion/commissioning to stages 09–11/13 without dropping their rows. A low-level object builder is useful but cannot close the full-coverage requirement by itself. Keep unresolved EDS-only fields explicit and gated; expose independently verified reads where appropriate.
6. Generate/update `docs/lichuan_api_cli_coverage.md` with one row per object/function or justified group: evidence, public function, implementation file, positive/negative test, CLI route or explicit rationale, hardware status and remaining blocker.

## Concrete implementation contract

Implement the read/settings rows assigned to this stage; retain stage-owned action, motion and commissioning rows for 09–11 and 13. A descriptor alone closes no typed-operation row.

| Public surface | Required names or responsibility | Result access |
| --- | --- | --- |
| Minimal presence/identity | `prepareProbe`, `prepareIdentity` | `getIdentity` where that read provides identity |
| Configuration/state blocks | `prepareConfig`, `prepareState` | `getConfig`, `getStateBlock` |
| Shared read stepping | `nextRead`, `advanceRead` | Checked concrete context and typed getter |
| Driver and I/O groups | Direct typed RS-style group operations | `getDriver`, `getIo` when corresponding result exists |
| Other native groups | Source-backed direct typed names | One checked accessor per meaningful result type |

Do not add `getReadResult`, `getMoveResult` or arbitrary getters to make every operation look identical. A wrong getter/context combination returns a clear validation error and leaves output unchanged. Define whether a multi-object result is fully valid or exposes separately valid blocks; never present a torn read as a single device snapshot.

Explicitly account for error history `0x1003`, communication objects `0x1005/0x1014/0x1017/0x1200`, PDO communication/mapping records, digital-output value/mask `0x60FE`, `0x2400` and `0x2401–0x2407`, and tuning ranges `0x2500–0x2520`. These are accounting groups from the source audit, not blanket writable declarations. Review each actual subindex/type/access/effect. Verified read-only fields can be exposed while a related write remains unresolved.

For every setting group, prepare a complete candidate, reject all invalid fields before first write, and retain which individual writes were confirmed if a later step fails. Check readback using the same type/unit semantics as the write. Update configuration/reference generations only according to confirmed or unknown effects. Examples: subdivision changes invalidate dependent position scale; output-function reassignment is not an ordinary output-level observation; save is never implicit.

## Documentation and work split

Apply [DOCUMENTATION.md](DOCUMENTATION.md) and [MAINTAINABILITY.md](MAINTAINABILITY.md). Update the per-stage coverage rows, typed-read/settings examples and Doxygen for validity, partial writes and readback. Name unresolved fields and their blockers without withholding unrelated proven reads.

Use [DELEGATION.md](DELEGATION.md) to split disjoint typed groups once shared read/result types are stable. One agent owns the canonical ledger and generated headers; independent reviewers verify group coverage against manual and EDS.

## Verify and exit

Test each supported type/range/access, complete candidate rejection before TX, documented read-only policy, wrong identity/context, mixed-age snapshots, partial group writes, readback mismatch, reboot/config epoch changes and atomic publication. Tests must prove no unexpected factory reset, save, I/O reassignment or enabling occurs.

Exit with typed observations/settings and honest native coverage for stage-08 rows. Contradictory fields remain specifically blocked; do not mark this stage fully complete if its required confirmed fields are merely omitted. Later-stage operations retain their named owner and pending status until implemented.
