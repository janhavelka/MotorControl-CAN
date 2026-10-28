# Prompt 31 Stop operations and owner integration

Apply [COMMON.md](COMMON.md). Prerequisites: [24](24_owner_recovery.md), [30](30_drive_actions.md).

## Objective and boundaries

Implement reviewed normal/halt and fast/quick-stop actions, then integrate real stop work with the already tested owner urgent/cancellation/retirement mechanisms. This is the first actual Lichuan stop integration; prompt-24 test fixtures did not implement drive behavior. Fault clear, device counter changes and position/velocity operations remain separate tasks.

Use [CONVENTIONS](CONVENTIONS.md#3-motion-typed-reads-and-terminal-evidence), its [owner contract](CONVENTIONS.md#4-one-application-can-owner), and [MULTIDRIVE](MULTIDRIVE.md). NMT stop, local cancel, release and communication silence are not interchangeable with a documented motor stop.

## Read and audit

Refresh MotorControl-RS, ADS1115 and INA228 revisions and dirty state. Read RS stop/action interruption and owner urgency tests, plus its retained uncertain-operation evidence. Compare PCA9555 cancellation and INA228 validity. Inspect FieldCore stop admission and independent outcomes read-only. Recheck original Lichuan stop/option-code/controlword evidence and the selected firmware gate; absent `0x605A/0x6085` or vague `0x605D` semantics cannot be supplied from another drive.

### Concrete evidence to recheck

Recheck [stop evidence](../audit/LICHUAN_MOTION_FACTS.md): ZH86 p12 and OP86 p10 give normal stop `15 -> 271` (`0x000F -> 0x010F`); EN86 omits that row. All three give `15 -> 11` (`0x000F -> 0x000B`) as emergency stop. These are command examples, not a certified emergency-stop function, latency, braking curve or retained-torque guarantee. `0x605A/0x6085` are absent; `0x605D` is I16 RW/default 1 with unresolved options. Do not invent quick-stop settings or infer physical standstill from a reply or NMT state.

## Implement

1. Specify the stop transition table before implementation: supported stop kinds, initial states, command/setup, option-code prerequisites, expected final state, target/queue disposition, reference/torque consequences and required new observations. The manual arrows are transition examples, not mandatory two-write scripts: never send `0x000F` or the `6,7,15` enable sequence to prepare a stop. Use an evidenced current-state transition or reject before TX.
2. Implement `prepareStop` through existing `nextAction`/`advanceAction` and concrete action context evidence. Unsupported stop kinds or unproved prerequisites fail before work. Acknowledged command and observed stopped state remain distinct; neither claims independent physical standstill.
3. Admit stop as its own urgent work with a finite deadline and reserved terminal result. Integrate the real profile action with owner capacity and occupied-channel policy rather than adding a separate priority receiver or bypass path.
4. Retire stale queued work at every established software/adapter/controller boundary. Identify which entries are definitely retired and which submitted work remains uncertain. Stop cannot invent channel availability or claim an ambiguous late SDO reply; a PDO stop path needs confirmed mapping and documented behavior.
5. Preserve interrupted operation identity and outcome. Stop success updates its own evidence but never erases a failed/unknown original operation. Define before/after-stop sequence admission and generation checks so an earlier queued target cannot execute afterward.
6. Keep event/deadline/cancellation precedence deterministic. A failed, cancelled or expired stop retains its uncertainty. Any later stop is another explicitly admitted action, not an extension of an expired operation or automatic replay.

## Verify and exit

Run native owner/action tests for each supported stop kind/state, occupied and quarantined channels, full ordinary/result capacity, stop on another node, stale queued targets before/after stop, adapter retirement uncertainty, lost reply, conflicting completion evidence, fault/reboot and exact deadline boundaries. Use concrete non-motion work fixtures until later move APIs exist; later prompts add their real interrupted contexts.

Assert precise writes and retained outcomes, including repeated stop and stop from halted/released states with no enabling preamble. No test helper may substitute NMT stopped for drive stop or silently clear faults. Exit with the complete reviewed stop/owner contract, leaving unsupported firmware semantics specifically blocked. Run focused stop/owner/service suites and Doxygen.

## Documentation and delegation

Apply [DOCUMENTATION](DOCUMENTATION.md), [MAINTAINABILITY](MAINTAINABILITY.md) and [DELEGATION](DELEGATION.md). Document normal/fast differences, interruption timelines and hardware uncertainty. One implementer owns the stop state machine; stronger independent review traces occupied-channel ambiguity and stale TX all the way to retained results. Delegate separate pressure/timeline tests after interfaces settle.

Record scope/checks in `docs/reports/prompt_31.md`, update coverage/owner guidance, then commit and sync verified work on the same execution branch. Complete independent checkpoints may be synchronized earlier; unresolved owned stop semantics remain pending.
