# Prompt 39: persistence and explicit reconciliation

## Delivered scope

Conditional software is implemented for save parameters (`2300=2`) and restore
factory settings (`2300=1`). These are distinct one-attempt intents, with native
outcomes retained separately from later current-value and power-cycle evidence.
The raw `2300` read is typed U16 and requires a one-use Q20 acquisition review.
The application can capture and explicitly re-apply three recorded position
parameters through the existing Settings lifecycle. No reset/restart API,
rollback, defaults dump, automatic save, rearm or replay was added.

Prompt39 remains **Partial for Q15 device effects**, not for the delivered host
workflow. Complete saved scope, flash completion/endurance, restart requirement,
rearm, factory route/defaults and physical qualification are unknown. These
facts remain assigned here; a successful host handshake does not resolve them.

Execution remains promptset-v2 on `promptset/motorcontrol-can`, upstream
`origin/promptset/motorcontrol-can`, original base `7878f7c`. Entry `0657edf`
was synchronized after fetch, with an empty index. Existing bench/enable edits
and untracked `.vscode` files belong to concurrent work and were preserved.
No hardware port, CAN traffic, flash, save or factory restore was used by this task.

## Source audit

| Repository | Refreshed revision, state | Adopted / rejected behavior |
| --- | --- | --- |
| MotorControl-RS | `cd27b12389f568198b72cecb6606123ea4c089bc`, clean | `Persistence.h/.cpp::prepareSave/prepareFactoryRestore/nextPersistence/advancePersistence/verifyPersistence` and `test/persistence_test.cpp` retain command uncertainty independently from later live/power-cycle observations. No reset API exists. `examples/probe_cli/MotionProfileApp.h::submitMotionProfile/serviceMotionProfile` performs explicit partial host restoration. Adopt intent/evidence separation, reject RTU register/timing/serial semantics and its delayed-delivery cutoff. |
| RV3032-C7 | `1b74184977be42e4145e2bfe47d7029e32480a5b`, clean | `test/test_native/test_datetime.cpp::test_primary_cell_ambiguous_write_is_reconciled_without_retry`, `src/RV3032.cpp` WRITE_ONE: reconcile ambiguous effects without rewriting. Retain CAN's original failed terminal rather than importing RTC cleanup or durability assumptions. |
| MB85RC | `1a45e882bb3b1d29f5a18538a7658b0ed0f8a424`, clean | `include/MB85RC/Config.h::WriteCommit`, `src/MB85RC.cpp::resumeVerifiedWrite`, `test/test_basic.cpp::test_ambiguous_write_waits_zero_io_then_resumes_verify_only_no_replay`: preserve accepted/indeterminate effects and resume verification only. No FRAM timing/buffer/durability guarantees transfer. |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0`, clean | `src/devices/rs485_motor/Rs485MotorModule.cpp::applyRuntimeConfig` rejects runtime address/baud changes; `src/rs485/Rs485Task.cpp` retires before reconfiguration. No drive-persistence precedent; read-only. |

Original EN86/ZH86 PDF15/18 and EDS `[2300]` lines1310–1318 were reopened:
U16, value2 save, value1 factory restore, zero ineffective. Homing speed/ramps/mode
are non-exhaustive saved examples. Neither full scope nor a flash completion
delay, response/restart behavior or zero rearm is established. N5 PDF283 uses
2300 for NanoJ Control, and EPOS4 rel13740 PDF71–72 uses different 1010/1011
signatures. None supplies a Lichuan fallback.

Original EN86 PDF4, ZH86 PDF3 and OP86 PDF5/9 preserve the supply/boot conflicts
in [bench guidance](../hardware_bench.md). The manufacturer page refetch timed
out; retained panel artwork independently supports the previously recorded
30–110 VDC / 20–80 VAC claim. No source snapshot was replaced.

## Contracts and coverage

- [Persistence.h](../../include/MotorControlCAN/profiles/lichuan_c/Persistence.h)
  validates a complete borrowed candidate before publication. Exact specimen,
  firmware, procedure and authorization records are mandatory; factory restore
  additionally requires complete external backup and route-back records.
  Raw STATUS is retained; MONITORS supplies a conservative host nonrunning,
  no-alarm, known-bit admission policy. It does not prove physical standstill.
- Before possible TX, `notePersistenceTransmission` advances configuration once
  and counts the attempt. The owner caller withdraws producer/PDO/reference and
  prepared evidence while retaining the old-route native terminal. It cannot
  migrate the route until actual queue retirement and explicit result release.
- `nextPersistence` cannot yield another command after the transmission notice.
  ACK is protocol success only. Lost reply, cancellation, reboot and expiry
  preserve effects and native history; processing at the deadline fails even
  when the original native ACK arrived earlier.
- `verifyPersistence` uses independently acquired current-route identity,
  STATUS/MONITORS and motion parameters. Same specimen/provenance, fresh epochs,
  matching identity and post-command times are required. A real externally
  recorded power cycle additionally requires advanced node generation, exact
  post-cycle epoch continuity and independent exclusion of intervening writes. Only
  matching `6081/6083/6084` values receive survival bits. This does not prove
  command causality, full saved state or successful factory restoration.
- [ConfigurationRestore.h](../../examples/common/ConfigurationRestore.h)
  retains the original record and delegates three supported fields to
  `prepareDriverSettings`. Current edit-state/control evidence remains required;
  the helper does not issue an enabling preamble. A failure preserves the written
  prefix and failing native evidence, without rollback, save or automatic retry.
- The object ledger has 192 read shapes; the 16 DEC-1 accepted rows and 14
  existing conditional setting rows are unchanged. Full-function persistence
  coverage remains false until its Q15 obligations are resolved. CLI is still
  future scope; direct public and isolated installed consumers are covered.

## Verification and review

Independent review found and fixed two issues before closure: post-cycle host
re-application could falsely qualify survival, and the reboot test supplied an
invalidated reservation as though it were live. Survival now needs exact
post-cycle epoch continuity plus externally established no-write history;
historical terminal ingestion is explicitly separate from live admission.
Regressions exercise both counterexamples. A fresh independent build and the
two new focused suites passed after the fixes.

The first pinned S2/S3 stack run found 2624–2640-byte aggregate temporaries.
Validation now checks one typed observation at a time, then publishes from the
same immutable borrowed inputs after every predicate succeeds. The second
strict run passes both targets at `-Os` and `-Og`, with no baseline exceptions
or enlarged limits: maximum frame 1728 bytes, all measured call-chain bounds
below 6144. Unresolved/indirect calls remain explicitly incomplete. Contexts
measure 5496 and 4768 bytes and belong in application storage, not nested task
locals. Retained local evidence: `.build/p39-stack-final/report.json`.

The first full run passed 116/117 tests; its generated-file check caught the
ledger narrative being edited while checks ran. The generator was rerun, with
original line endings preserved. The final integrated run passes:

- `python scripts/verify.py --mode full --compiler gcc --build-dir .build/p39-full`:
  117/117 CTests, independent exported/relocated package and native consumers,
  strict Doxygen1.13.2, generated/ledger/Python/registration checks. Evidence:
  `.build/p39-full/20261110T145253029165Z-45080-rluf6ybu/summary.json`.
- New direct/real-owner suites: 4515 persistence checks and 7008 recorded-restore
  checks. Review rebuilt both independently, 2/2 passed.
- Strict pinned S2/S3 stack measurement at both optimization levels: passed,
  no baseline entries or changed limits. No hardware task high-water claim.
- Promptset/source integrity and `git diff --check`: passed. Original vendor
  snapshots and reference hashes remain unchanged.

The automatic push workflow verifies the implementation commit separately.
Native fake-owner tests do not qualify firmware flash behavior or electrical/
torque safety. Firmware persistence tests were not run.

## Remaining evidence

For each selected specimen, retain the exact full backup and tested recovery
route, then establish affected fields, reply timing, restart/rearm requirements,
interrupted-write effects and per-field survival through a finite independently
observed power cycle. Observe torque independently and read `2100`/`6041` before
commands after power-up. Q20 remains a separate per-upload review; timeout alone
never establishes reset or permits channel reuse. No physical trial was run here.
