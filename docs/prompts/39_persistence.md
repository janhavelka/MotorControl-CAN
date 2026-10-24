# Prompt 39 Persistence reset and reconciliation

Apply [COMMON.md](COMMON.md). Prerequisites: [24](24_owner_recovery.md), [38](38_communication_commissioning.md).

## Objective and boundaries

Complete reviewed typed save, reset and explicit restore workflows with retained effects and post-change reconciliation. This prompt closes the assigned persistent native rows; it does not invent a universal defaults dump, transactional rollback or a factory-reset-based test setup.

Apply [CONVENTIONS](CONVENTIONS.md#3-motion-typed-reads-and-terminal-evidence), its [owner contract](CONVENTIONS.md#4-one-application-can-owner), and [MULTIDRIVE](MULTIDRIVE.md). Persistent commands are vendor/profile-specific even when another drive uses the same object index.

## Read and audit

Refresh MotorControl-RS, ADS1115 and INA228 revisions and dirty state. Read RS `Persistence`, explicit restore/reset and uncertain-write tests; inspect RV3032 persistent effects and MB85RC partial writes. Review current FieldCore configuration ownership read-only. Recheck original Lichuan `0x2300` semantics, restart/reply behavior, persistence scope and model/firmware applicability against the EDS and reviewed ledger.

## Implement

1. Implement typed operation contexts and direct prepare/next/advance functions for confirmed workflows in `profiles/lichuan_c/Persistence.h` and source where justified. Object `0x2300` value 2 means save and value 1 factory reset in the reviewed source; use distinct APIs/intent, not a generic persist integer accepting magic values.
2. Specify preconditions, affected configuration, expected service reply, volatile/persistent effect, restart requirement, route behavior, required observation and terminal uncertainty for each command. Confirmed protocol success is not automatically proof that persistent state survived restart.
3. Treat a timed-out save/reset as potentially applied. Preserve partial/unknown outcomes and forbid automatic replay. Reconcile through explicit current-route reads and reviewed restart evidence; loss of response alone does not prove reset or successful route change.
4. Preserve original configuration records with identity/model/firmware, route, timestamps and provenance. An explicit restore is another validated sequence of writes that can fail partially; never promise rollback. Expose only documented supported fields, rather than dumping guessed defaults or undocumented EDS values.
5. Keep desired, observed, actual and persistent configuration separate. Reset/restart invalidates mapping, identity/reference and prepared work as applicable. Use established owner retirement evidence before discarding queued work; recovery does not resume old motion, clear unrelated outcomes or re-enable.
6. Complete setup guidance with actual device label/manual revision, transceiver/termination/wiring evidence, node/bitrate, identity/configuration inspection and explicit changes. Keep manufacturer supply-range conflicts visible; do not turn the Alibaba title or RS bench wiring into an electrical specification. I/O requirements follow the selected operation.

## Verify and exit

Test save versus factory reset, unsupported operation before TX, complete candidate validation, lost acknowledgement, reset/restart, wrong readback, partial restore, route changes and generation invalidation. Assert no persistent action is retried automatically and no discovery/helper/runner factory-resets a device to obtain a convenient starting state.

Run direct public APIs through the real fake owner, preserving historical errors and uncertain effects. Exit with all confirmed assigned persistent rows complete, documented reconciliation and honest physical qualification status. Unresolved firmware effects remain specifically pending; raw access and unsupported stubs do not close them. Run persistence/communication/owner regressions and Doxygen.

## Documentation and delegation

Apply [DOCUMENTATION](DOCUMENTATION.md), [MAINTAINABILITY](MAINTAINABILITY.md) and [DELEGATION](DELEGATION.md). Publish commissioning order, save/reset distinctions and restore failure examples with Doxygen. One implementer owns each workflow; delegate independent source verification and require stronger review for uncertain writes, restart and route reconciliation.

Record closure in `docs/reports/prompt_39.md`, update coverage/setup docs and commit and sync on the recorded execution branch. Complete verified checkpoints may be synchronized during the prompt; no hardware action or publication is implied.
