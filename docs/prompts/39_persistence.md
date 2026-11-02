# Prompt 39 Persistence reset and reconciliation

Apply [COMMON.md](COMMON.md). Prerequisites: [24](24_owner_recovery.md), [38](38_communication_commissioning.md).

## Objective and boundaries

Complete reviewed typed save, reset and explicit restore workflows with retained effects and post-change reconciliation. This prompt closes the assigned persistent native rows; it does not invent a universal defaults dump, transactional rollback or a factory-reset-based test setup.

Apply [CONVENTIONS](CONVENTIONS.md#3-motion-typed-reads-and-terminal-evidence), its [owner contract](CONVENTIONS.md#4-one-application-can-owner), and [MULTIDRIVE](MULTIDRIVE.md). Persistent commands are vendor/profile-specific even when another drive uses the same object index.

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read the RS code for this seam; RS is authoritative. Check HEAD and dirty state only for the siblings named below, and inspect only the seam stated. Inspect current FieldCore-node read-only for ownership or integration changes. If HEAD, dirty state and seam match the last recorded row, write "unchanged since prompt N" and do not reread. A sibling row must name a behavior adopted or rejected, with path and symbol. This does not relax vendor evidence, safety, uncertainty or verification rules. Read RS `Persistence` (`prepareSave`, `prepareFactoryRestore`, `nextPersistence`, `advancePersistence`, `verifyPersistence`; RS has no reset API) and its uncertain-write tests. RS restores multi-write configuration only in the application `motion-profile restore`. Keep RV3032 ambiguous-write reconciliation (`test_primary_cell_ambiguous_write_is_reconciled_without_retry`) and MB85RC `WriteCommit` as references. FieldCore has no drive-persistence precedent; note only that its module rejects runtime address and baud changes (`applyRuntimeConfig`). Recheck original Lichuan `0x2300` semantics, restart/reply behavior, persistence scope and model/firmware applicability against the EDS and reviewed ledger.

### Concrete evidence to recheck

The [source tables](../audit/LICHUAN_OBJECT_FACTS.md) identify `0x2300:00` as U16, value 2 save and value 1 restore factory parameters. Standard `0x1010/0x1011` objects are absent; do not substitute generic `save`/`load` signatures. The same index `0x2300` is NanoJ Control on Nanotec N5 and absent in EPOS4 rel13740, whose `1010/1011` signatures do not apply to this EDS. No usable flash completion time, endurance or complete persistence scope is established. The [public example](../audit/LICHUAN_PUBLIC_IMPLEMENTATIONS.md) saves during zeroing; reject that hidden coupling and test that reference/fault operations cannot issue persistence writes.

## Implement

Use three terms: `save parameters` (`2300=2`), `restore factory settings` (`2300=1`) and `re-apply recorded configuration` (host writes). No vendor restart command is documented. The manuals' only restart wording, 重启复位, is undefined. This project uses restart to mean a user power cycle, or a separately gated NMT service whose parameter-reload effect is unknown. This prompt also owns the read row `2300`.

1. Implement typed operation contexts and direct prepare/next/advance functions for confirmed workflows in `profiles/lichuan_c/Persistence.h` and source where justified. Object `0x2300` value 2 means save and value 1 factory reset in the reviewed source; use distinct APIs/intent, not a generic persist integer accepting magic values.
2. Specify preconditions, affected configuration, expected service reply, volatile/persistent effect, restart requirement, route behavior, required observation and terminal uncertainty for each command. Restart requirement, complete saved set and rearm are tri-state and currently `unknown`: the manuals give homing speed, ramps and homing mode only as non-exhaustive examples of saved parameters, and whether `1017`, `2203` or PDO maps are saved is undocumented. Do not write 0 after a save unless Q15 evidence requires it; if so, make it a visible step. Count every save. Confirmed protocol success is not automatically proof that persistent state survived restart.
3. Treat a timed-out save/reset as potentially applied. Preserve partial/unknown outcomes and forbid automatic replay. Reconcile through explicit current-route reads and reviewed restart evidence; loss of response alone does not prove reset or successful route change.
4. Preserve original configuration records with identity/model/firmware, route, timestamps and provenance. An explicit re-application of recorded configuration is another validated sequence of writes that can fail partially; never promise rollback. Expose only documented supported fields, rather than dumping guessed defaults or undocumented EDS values.
5. Keep desired, observed, actual and persistent configuration separate. Reset/restart invalidates mapping, identity/reference and prepared work as applicable. Use established owner retirement evidence before discarding queued work; recovery does not resume old motion, clear unrelated outcomes or re-enable.
6. Complete setup guidance with actual device label/manual revision, transceiver/termination/wiring evidence, node/bitrate, identity/configuration inspection and explicit changes. Keep manufacturer supply-range conflicts visible: panel artwork and the manufacturer page say 30-110 VDC / 20-80 VAC, spec tables say 36-110 / 18-80, and wiring says 24-50. Use the nameplate; do not turn the Alibaba title or RS bench wiring into an electrical specification. The 57C-body manual says the motor locks on power-up; applicability to the 86C is unconfirmed and OP86 p9 points the other way. Treat every power-up or restart as a possible torque state change, observe it independently, and record `2100`/`6041` before any command. I/O requirements follow the selected operation.

The `2300` read row is outside the DEC-1 accepted list; it stays Q20 gated and uses an explicit reviewed acquisition.

## Verify and exit

Test save versus factory reset, unsupported operation before TX, complete candidate validation, lost acknowledgement, reset/restart, wrong readback, partial restore, route changes and generation invalidation. Assert no persistent action is retried automatically and no discovery/helper/runner factory-resets a device to obtain a convenient starting state.

Run direct public APIs through the real fake owner, preserving historical errors and uncertain effects. Exit with all confirmed assigned persistent rows complete, documented reconciliation and honest physical qualification status. Unresolved firmware effects remain specifically pending; raw access and unsupported stubs do not close them. Run persistence/communication/owner regressions and Doxygen.

## Documentation and delegation

Apply [DOCUMENTATION](DOCUMENTATION.md), [MAINTAINABILITY](MAINTAINABILITY.md) and [DELEGATION](DELEGATION.md). A new header or source needs `CMakeLists.txt`, `test/package_test.py`, `scripts/check_repository.py` and `test/repository_test.py` updated together. Publish, in `docs/commissioning.md`, the commissioning order, save/reset distinctions and restore failure examples with Doxygen. One implementer owns each workflow; delegate independent source verification and require stronger review for uncertain writes, restart and route reconciliation.

Record closure in `docs/reports/prompt_39.md`, update coverage/setup docs and commit and sync on the recorded execution branch. Complete verified checkpoints may be synchronized during the prompt; no hardware action or publication is implied.
