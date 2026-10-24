# Prompt 36 Homing and native coverage reconciliation

Apply [COMMON.md](COMMON.md). Prerequisites: [31](31_stop_actions.md), [32](32_fault_reference_actions.md), [35](35_velocity.md).

## Objective and boundaries

Deliver supported homing methods and qualified reference publication through the existing owner, then reconcile the finite native-operation coverage already assigned across prompts. This prompt owns homing, not an unlimited collection of unfinished settings/actions. Prompts 20–22 own settings, 21 output actions, 32 device-reference actions and 38–39 communication/persistence.

Use [CONVENTIONS](CONVENTIONS.md#3-motion-typed-reads-and-terminal-evidence) and [MULTIDRIVE](MULTIDRIVE.md). Homing is capability-specific; a servo contrast can reject an unsupported method without claiming all servos lack homing.

## Read and audit

Refresh MotorControl-RS, ADS1115 and INA228 revisions and dirty state. Read RS `Homing`, `getHomeReference`, prerequisite and interrupted-home tests; inspect RV3032 finite cleanup and MB85RC partial-effect handling. Review FieldCore reference publication/admission read-only. Recheck original Lichuan method tables, particularly 17/18/24/29, and their model/firmware scope against the reviewed ledger and actual evidence disposition.

### Concrete evidence to recheck

Use the [method-by-method homing table](../audit/LICHUAN_MOTION_FACTS.md). `6098` methods 17/18 use negative/positive limit; 24/29 use origin in positive/negative search directions. Method 29 finishes on leaving the switch during positive creep, not simply the reverse of method 24. `6099:01/:02` are U32 search/creep RPM (0..3000); `609A` is U32 ramp ms (0..2000); `607C` is I32 offset (-1000000..+1000000). Mode 6 and controlword bit4 rising/falling edges start/interrupt. Numeric backoff distance and search timeout are unspecified. Reject the example's wrong two-byte writes to `6098` (one byte), `6099:01/:02` and `609A` (four bytes), and its unexplained 5000 ms ramp; controlword itself remains two bytes.

## Implement

1. Implement `prepareHome`, `nextHome`, `advanceHome` and checked `getHomeReference` in the concrete homing profile/context. Preserve direct operation naming and fixed caller-owned storage; no blocking run-home helper or generic executor.
2. For each confirmed method, document method ID, direction, required signals, prohibited assumptions, search/creep values, offset, ramp time, mode/trigger, acknowledgement/completion and reference effect. Validate only signals the actual method requires; do not copy ESS methods or require every switch for every method.
3. Validate the whole candidate before writes, including ranges, signedness, state, binding/configuration generation and physical prerequisite evidence where needed. Unconnected I/O does not mean its configured function is disabled. Do not reassign limits/stops to make a test succeed.
4. Distinguish accepted mode/start commands from new qualified homing completion. Publish the new reference only with matching method/operation/generation and evidenced completion. Host origin, device counter clear and a successful home reference remain different facts.
5. Integrate stop, fault, timeout, impossible signal state, restart, counter overflow and unknown completion with retained outcomes. Recovery never resumes home. Stale home-complete evidence or a late observation cannot rewrite an earlier failed terminal.
6. Reconcile all native coverage rows assigned through this prompt. Confirm each implemented row has its API/tests/docs; identify document conflicts separately from confirmed omissions. A missing earlier owned implementation returns that named prompt to pending. Do not hide it in a raw accessor or silently grow this prompt. Communication/persistence rows remain explicitly assigned to 38–39.

## Verify and exit

Test every supported method/prerequisite, unsupported method before TX, no-input and method-specific input cases, mode/trigger failures, interrupted home, stale completion, timeout after start, lost stop reply and reference invalidation/publication. Use direct public APIs and real fake-owner events; include a later operation attempting to use an invalid home reference.

Exit with complete confirmed homing scope and an accurate coverage reconciliation. Missing firmware evidence stays blocked, not falsely implemented by a stub. Host tests do not qualify physical reference accuracy. Run homing/reference/action/owner suites and relevant coverage/Doxygen gates.

## Documentation and delegation

Apply [DOCUMENTATION](DOCUMENTATION.md), [MAINTAINABILITY](MAINTAINABILITY.md) and [DELEGATION](DELEGATION.md). Publish a method/prerequisite table and Doxygen for reference validity. One implementer owns the lifecycle. Delegate disjoint method source reviews and a separate stronger reference/uncertainty review; canonical ledger edits have one owner.

Record closure in `docs/reports/prompt_36.md`, update coverage/user docs and commit and sync on the existing execution branch. Complete verified checkpoints may be synchronized earlier; unfinished earlier scope remains visibly pending.
