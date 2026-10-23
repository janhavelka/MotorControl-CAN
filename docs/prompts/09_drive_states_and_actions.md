# Stage 09 Implement drive states, enable, release, fault clear and stop

Apply [COMMON.md](COMMON.md). Prerequisites: stages 07–08 and resolved evidence for the selected dialect.

## Audit

Read RS ActionOperation/Actions and stop/interruption tests, application uncertain-motion interlocks, INA228 hardware-state validity and FieldCore independent admission/wire/execution outcomes. Check original Lichuan control/status diagrams, manufacturer monitor word and stop descriptions; standard CiA402 is background, not proof of Lichuan status masks.

## Deliver

1. Implement an explicit reviewed drive-state decoder and transition table, retaining raw statusword/controlword, mode display, monitor data and unknown/contradictory states. Resolve the EN86/ZH86/ZH57/OP86 bit conflicts via model/firmware evidence; do not choose by language or union all meanings.
2. Implement `prepareEnable`, `prepareRelease`, `prepareClearAlarm`, `prepareStop`, and `prepareSetDevicePosition` only where supported, with `nextAction`/`advanceAction` and terminal evidence in the action context. No invented `getActionResult` convention is needed.
3. Validate prerequisites and observe actual transition results with externally supplied timing/deadlines. The 6/7/15 enable sequence and nominal spacing in the manual are not blocking sleeps or proof of readiness. Mode/target handling must prevent enabling a stale velocity command. If that cannot be established for a firmware, reject the affected action before writes.
4. Define normal/halt and fast/quick-stop behavior, option-code requirements, target/queue disposition, expected final drive state, uncertainty and torque/reference consequences. Missing 605A/6085 or vague 605D semantics cannot be filled with generic defaults.
5. Provide an interrupt path that reserves stop work and reconciles an occupied SDO channel without allowing stale queued motion to follow the stop. A PDO path requires confirmed mapping and documented behavior. Preserve interrupted operation evidence; local cancel or NMT stopped is not the stop command.
6. Treat fault reset, position-clear edge, drive release and factory reset as different operations. Counter reset invalidates origin/reference. Fault clearing preserves original error/EMCY evidence and never automatically resumes or re-enables motion.

## Concrete implementation contract

Keep the public sequence under `profiles/lichuan_c/Actions.h` and its implementation. Common `ActionOperation.h` contains only reusable action evidence. Preserve `ActionState`, `ActionExecution` and `ActionCompletion` meanings from [CONVENTIONS.md](CONVENTIONS.md); raw device status stays separate.

For each action write a transition table before its implementation: admissible initial states, explicit setup, command/value/edge, matching protocol reply, required new observation, terminal outcome and reference/torque consequence. A mask borrowed from generic CiA402 is not a verified Lichuan decoder.

The action context must distinguish `NOT_TRANSMITTED`, acknowledged protocol work, rejected work and unknown execution. An acknowledgement is not physical standstill. Preserve statusword/monitor disagreements as unknown or explicitly qualified evidence. `prepareSetDevicePosition` cannot imply arbitrary setting if the firmware only exposes a zero-clear action; unsupported values fail before work.

Define event precedence with a supplied receive timestamp and processing timestamp. Test response captured before deadline but processed after, first received after deadline, and cancellation/expiry at the same service turn. Use a documented deterministic rule, not callback arrival order. Evidence arriving after a terminal result may update observations but must not silently rewrite a failed operation as successful.

Stop has its own admitted work and finite deadline. It can interrupt ordinary scheduling, but cannot invent a free SDO channel or confirm a response ambiguous with retired work. Show which queued motion entries were retired and which hardware entries remain uncertain. A confirmed stop updates stop evidence; it does not erase the original move's failed/unknown result.

## Documentation and work split

Apply [DOCUMENTATION.md](DOCUMENTATION.md) and [MAINTAINABILITY.md](MAINTAINABILITY.md). Put normal/fast stop behavior, failure consequences and action tables in the profile/owner docs, with Doxygen on preparation and advancement. Describe unknown capability with the exact missing firmware evidence.

Use [DELEGATION.md](DELEGATION.md) for independent high-reasoning review of enable/stop/reset sequences and a separate event-timeline test review. Keep each state machine with one implementer; do not delegate steps of one transition sequence to competing agents.

## Verify and exit

Exercise every reviewed state/transition, unknown bits, conflicting monitor evidence, mode mismatch, fault at every step, delayed acknowledgement, cancelled/failed stop, occupied channel, target queued before/after stop, stale completion and reboot. Assert exactly which writes may occur and that all failures preserve uncertainty. Test action functions independently from CLI.

Exit with documented, bounded action semantics for the selected profile. If actual stop/enable semantics remain unresolved, keep those capabilities gated and continue independent host/core work; do not substitute generic controlword folklore.
