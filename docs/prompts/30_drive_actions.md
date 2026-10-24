# Prompt 30 Drive state enable and release

Apply [COMMON.md](COMMON.md). Prerequisites: [19](19_typed_reads.md), [20](20_driver_settings.md), [29](29_device_evidence.md).

## Objective and boundaries

Deliver the reviewed Lichuan drive-state interpretation and complete explicit enable/release operations for the selected model/firmware. Stop, alarm clearing and device-reference actions have separate prompts 31–32. This prompt does not fill unresolved status masks with generic CiA402 folklore or treat a host simulation as firmware evidence.

Follow [CONVENTIONS](CONVENTIONS.md#3-motion-typed-reads-and-terminal-evidence) and [MULTIDRIVE](MULTIDRIVE.md). Shared action intent preserves native torque, hold/coast/brake and readiness consequences; CANopen NMT state is not a generic motor state.

## Read and audit

Refresh MotorControl-RS, ADS1115 and INA228 revisions and dirty state. Read RS `ActionOperation`, `Actions`, enable/release tests and uncertain-motion interlocks. Inspect INA228 hardware-state validity and actual FieldCore admission/wire/execution projections read-only. Reopen original Lichuan control/status/monitor diagrams and the prompt-29 evidence disposition for EN86/ZH86/ZH57/OP86 conflicts.

### Concrete evidence to recheck

Use [motion objects and state tables](../audit/LICHUAN_MOTION_FACTS.md). Mode request/display are `6060/6061`; documented operation values are 1 position, 3 velocity and 6 homing. Enable sequence is decimal `6,7,15` (`0006,0007,000F`) with approximately 10 ms spacing. `6040` is WO in EDS: do not require a read-modify-write or treat reading controlword as acceptance. EN86 puts alarm at bit7 and standstill at bit10; ZH86 puts alarm at bit3 and running at bit14; OP86 mixes alarm7/running14. Never merge these into one ready mask. ZH86 describes enabling as velocity start; establish separately whether a retained nonzero target can be used.

## Implement

1. Implement an explicit reviewed state decoder and transition table. Retain raw statusword/controlword, mode display, monitor values and unknown/contradictory interpretation. Never union conflicting bit meanings or choose a dialect by language alone.
2. Implement `prepareEnable`, `prepareRelease`, `nextAction` and `advanceAction` in `profiles/lichuan_c/Actions.h` and corresponding source. Common `ActionOperation.h` carries only reusable evidence. Terminal evidence lives in the concrete context; do not add `getActionResult`.
3. Before code, specify each transition's admissible states, explicit setup, command/value, service confirmation, new observation, terminal outcome and torque/reference consequence. Use supplied timing and finite deadlines; the manual's 6/7/15 sequence and nominal spacing do not authorize blocking sleeps or prove readiness.
4. Establish mode/target handling that cannot enable an unknown stale velocity target. Capability/precondition rejection happens before writes. If the selected firmware's ordering cannot be established, keep that action evidence-gated with a precise blocker; an unsupported placeholder does not count as implemented enable.
5. Preserve `NOT_TRANSMITTED`, protocol-acknowledged, rejected and unknown execution independently from observed completion. TX acceptance/CAN ACK is not service success or physical standstill. Connect real action work through the existing owner without duplicating receive or service state.
6. Apply the established receive-versus-processing time precedence. Evidence received before a deadline but processed later is handled deterministically; first received after expiry cannot silently rewrite the terminal. Release invalidates or preserves reference only as the profile supports.

## Verify and exit

Test every reviewed state transition, unknown bits, monitor/status disagreement, wrong mode, stale velocity target, fault at each step, delayed reply, deadline/cancel ordering, reboot and stale generation. Assert exact permitted writes and atomic preparation failure. Exercise the action APIs directly through a native fake owner, without CLI dependence.

Exit only with confirmed enable/release semantics implemented and tested for the stated scope. Keep unresolved action scope pending while independent host work proceeds; do not claim full completion from generic simulated handshakes. Hardware behavior remains separately qualified. Run action/state/owner regressions and documentation gates.

## Documentation and delegation

Apply [DOCUMENTATION](DOCUMENTATION.md), [MAINTAINABILITY](MAINTAINABILITY.md) and [DELEGATION](DELEGATION.md). Publish transition tables, raw-versus-interpreted state and uncertainty consequences with Doxygen. One implementer owns the state machine; use stronger independent review for conflicting state evidence and enable ordering, plus a separate timeline-test review.

Record closure or exact blocked capability in `docs/reports/prompt_30.md`, update coverage/user docs, and commit and sync completed verified work on the existing execution branch. Checkpoint useful complete pieces; do not mark an unfinished owned capability complete.
