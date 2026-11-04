> Executed handoff, retained for traceability. Do not execute this copy. All 22 packets ran between 2 and 4 November 2026. The [execution table](../../../reports/handoff_19_33/README.md) records what each delivered, each packet report records what remains, and the [backlog](../../../backlog.md) rows H03 and H04 track the physical trials still owed. Relative links were relocated; task wording is retained from `2920479`.

# Packet 20: Complete finite absolute-position lifecycle

Execute this packet fully. Apply [COMMON.md](COMMON.md). Prerequisites: verified
W1–W8 closure in packets 1–10, packet 12's settings contracts, packet 15's usable
enable/state artifacts, packet 16's stop contract and packet 19's applicable
position evidence. Read repaired [original33](../../33_position_moves.md) in full.
This packet owns absolute behavior and shared move context; packet 21 owns
relative behavior and the complete interruption matrix. Do not claim original 33
Complete at this intermediate boundary.

## Read and implement

Inspect actual `include/MotorControlCAN/profiles/lichuan_c/Position.h`, its source,
`Axis.h/.cpp`, `ActionOperation.h`, current Actions/State/evidence helpers,
application owner and `examples/position_preview`. Read RS `MoveOperation.h`,
ESS Position lifecycle and callers/tests, not only names. Compare BME280 partial
configuration and FieldCore exact target/operation/step snapshots read-only.

Add `prepareMoveAbsolute`, `nextMove`, `advanceMove` and the concrete move context
using the existing preparation/arithmetic and owner work/evidence contracts.
Create common `MoveOperation.h` only for shared intent/outcome distinctions.
Terminal context is the result: no getMoveResult, virtual profile base, generic
trajectory framework, duplicate SDO engine or full embedded read context.
Preserve servo/protocol-neutral common intent; native rpm and status dialect
belong to Lichuan profile data.

Freeze requested/effective target, quantization error, units/frame, scale/origin/
limits/reference, binding/configuration/dialect epochs and immutable deadline.
Reject unsupported model/firmware, invalid widths/ranges, wrong mode, stale
reference, unqualified acceptance/completion or old generation before TX.
Preparation must publish atomically. Missing independent firmware evidence is
a capability gate, not an excuse to present fake success as implementation.

Correct original 33's phase order according to D-01: preconditions are explicitly
enabled mode 1 with current produced evidence, not implicit enable. Confirm that
evidence, then write607A/6081/6083/6084 under the reviewed order, issue the bit4
edge, retain protocol result, collect new acceptance and completion, and perform
any supported visible terminal rearm. Never write6061. Hold bit4 until terminal
under DEC-8; unsupported immediate-update remains rejected. Nonblocking caller
time and finite service budgets replace sleeps. Do not invent a refresh lease.

Use operation-owned parameter phases, not `prepareDriverSettings`: they preserve
this intent's generation while invalidating cached MOTION_PARAMETERS as packet 10
defines. Call `retireMotionParameters(retained, current, work)` for each retained
context, after queue admission and preflight and before each own download may
transmit, and do not transmit if it refuses. Explicit external settings still
invalidate the move. Capture submitted
objects/bytes, service terminals, readbacks and timestamps so a partial setup or
unknown trigger is explainable. Retained profile snapshots need precise reuse
and contradiction invalidation. Successful SDO, old target-reached, or a new
completion sample with wrong reference cannot finish the move.

Implement deadline/cancel/fault/reboot and real urgent-stop integration through
the same owner. Preserve unknown external effects and original operation identity.
Recovery cannot resume a trajectory or replay a trigger. Controller-observed
completion remains separate from independently measured movement/standstill.
Apply R-08: finite move preparation rejects zero effective displacement before
any writes; pure arithmetic may represent zero and an application may separately
report already-at-target. An absolute coordinate zero is not automatically zero
displacement, and absent reference cannot manufacture a no-op success.
Preserve observed completion separately from any subsequent visible rearm result:
failed/uncertain000F rearm neither erases that observation nor authorizes retry.
Apply G-13's processing cutoff: now>=deadline expires even if a frame's receive
interval was earlier; never backdate terminal publication to convert it to success.

## Verify, document and close this scope

One implementer owns the lifecycle; delegate exact-target vectors and strong
unknown-trigger/later-admission review. Use literal bytes through the real fake
owner. Cover positive/negative native endpoints, requested and quantized bounds,
overflow, fractional error, optional reference, zero distance, profile reuse/
mismatch, bad mode/setup, new versus stale acceptance/completion and missing
reference. Test each absolute phase for fault/cancel/deadline/reboot and a stop
interruption. After lost trigger confirmation, new motion must remain fenced.

Provide a native compiled example independent of CLI and an installed consumer.
Run position/axis/action/owner/service regressions, stack budgets, full verifier,
firmware and required CI. Doxygen must explain bases, units, generations,
deadlines, input lifetime and unknown outcomes. Update packet 20, original 33,
coverage and guides; commit/sync. Hardware regression is packet 22; include21's
relative cases only when their implementation and evidence are ready. A blocked
relative base does not hold absolute/action regression. Existing isolated evidence
remains separately identified.
