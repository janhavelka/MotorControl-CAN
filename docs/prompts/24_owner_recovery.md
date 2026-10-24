# Prompt 24 Owner urgent capacity and transport recovery

Apply [COMMON.md](COMMON.md). Prerequisites: [23](23_owner_dispatch.md).

## Objective and boundaries

Complete owner mechanisms for urgent work, cancellation, queue invalidation and explicit transport recovery. Prove these mechanisms with concrete test-only work fixtures. Actual Lichuan stop operations arrive in prompt 31; do not add a fake installed stop driver or depend on future motion headers. Transport retirement evidence is established here so later drive actions use a real, bounded ownership path.

Apply [CONVENTIONS](CONVENTIONS.md#4-one-application-can-owner) and [MULTIDRIVE](MULTIDRIVE.md). Local cancellation/recovery does not stop a motor, clear a drive fault or authorize replay.

## Read and audit

Refresh MotorControl-RS, ADS1115 and INA228 revisions and dirty state. Read RS urgent admission, cancellation, sequence invalidation, recovery and retained-result tests. Compare PCA9555 cancellation and INA228 validity handling. Inspect current FieldCore generation/producer invalidation read-only. Read the selected adapter/stack queue and transmission evidence contracts; do not assume SDK queue acceptance proves physical transmission or that reset drains every queue.

## Implement

1. Implement `admitUrgent`, `cancel`, `recover`, `recoveryResult` and `releaseRecovery`; use `beginSequence`, `invalidate` and `invalidateProducer` only where their established meanings have callers. Reserve bounded urgent admission and terminal-result capacity independently of ordinary pressure.
2. Define cancellation at every phase: queued locally, accepted by adapter/controller, physically transmitted, protocol-confirmed and terminal. Retain possible external effects. Never rewrite an earlier eligible terminal event when cancellation arrives afterward.
3. Make local, stack, SDK and controller queued work participate in binding/configuration/controller epochs. Retire stale work before later admission can transmit it. If the adapter cannot establish retirement, preserve uncertainty and inhibit unsafe resumption rather than guessing that a queue was drained.
4. Implement occupied-channel urgent policy using the existing SDO ambiguity rules. Urgency cannot invent a free channel or claim a physically indistinguishable late response belongs to new work. Other channels/nodes and asynchronous receive remain serviceable within budgets.
5. Make recovery explicit host/controller work with a finite budget and retained result. Invalidate dependent evidence, reconcile routes and preserve interrupted operation results. Recovery never replays a write, re-enables, clears alarms or erases required history.
6. Define receive-versus-processing timestamp precedence for deadline/cancel races. Late evidence may update observations without rewriting a failed terminal. Document what the transport proves and what remains unknown.

## Verify and exit

Use test-only fixed work requests to cover urgent capacity under full ordinary/results pressure, cancellation at every wire phase, uncertain submission, queued stale work after bus-off/reboot/rebind, timeout during recovery, generation exhaustion, receive overflow and clock boundaries. Include two-node cases proving urgent work does not starve unrelated receive and that a quarantined channel is not falsely reused.

Do not label these fixtures a validated drive stop. Prompt 31 must integrate real stop actions and their evidence. Exit when the owner mechanisms and all declared adapter limitations are implemented/tested, with no hidden replay path. Run focused owner, service and native consumer tests.

## Documentation and delegation

Apply [DOCUMENTATION](DOCUMENTATION.md), [MAINTAINABILITY](MAINTAINABILITY.md) and [DELEGATION](DELEGATION.md). Document cancellation/recovery timelines, queue ownership and uncertain effects with Doxygen and owner guidance. One implementer owns central state/storage. Delegate adversarial timeline tests; require stronger independent review of stale TX retirement and occupied-channel interruption.

Close the owned mechanisms in `docs/reports/prompt_24.md`, update documentation and retained limitations, then commit and sync on the same execution branch. Complete verified checkpoints may be synchronized during the prompt; fake evidence never closes future hardware qualification.
