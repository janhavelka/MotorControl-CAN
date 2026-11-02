# Packet 21: Relative position and complete interruption behavior

Execute this packet fully. Apply [COMMON.md](COMMON.md). Prerequisite: packet 20's
checked absolute lifecycle and applicable reference/base evidence from 18/19.
Read repaired original 33, its remaining acceptance criteria and the current
Position/MoveOperation/Axis/Actions/owner code. Finish original 33's supported
relative scope without a second lifecycle engine.

Implement `prepareMoveRelative` using the same concrete nextMove/advanceMove
progression, immutable intent and retained outcome. Distinguish host arithmetic
reference from the drive's actual native relative base. Support each actual,
commanded or queued basis only when its wire semantics and witness are evidenced.
Do not expose all three because Axis arithmetic can calculate them. Queue support
does not follow from a documented relative trigger. Unsupported bases must reject
before work, with the smallest precise evidence gate and no success stub.

Use the reviewed000F→005F trigger for ordinary relative work. Keep003F/007F
immediate-update behavior separate unless specifically evidenced. Never silently
convert an unknown relative command into absolute, apply a new origin, zero a
counter or restart a relative trigger after timeout. Capture displacement,
effective quantization, absolute endpoint and the exact reference snapshot;
validate both displacement native limits and endpoint axis limits. Recheck
freshness/configuration/reference epochs at actual admission, not only prepare.

Finish the combined interruption matrix through the real fake owner. For mode
confirmation, each parameter, pre-trigger, possible trigger, protocol confirmation,
acceptance, completion and rearm: inject deadline, cancel, fault, boot/rebind,
missing/stale feedback and a real urgent stop. Preserve possibly executed earlier
work, terminal stability and stop's separate identity/deadline. Include queued
old targets after stop, in-flight poll settlement, urgent pressure on both nodes,
ambiguous late ACK, best-effort stop and explicit recovery. No test fabricates a
free SDO channel or substitutes NMT stopped for motor stopped.

Check identical repeated relative values as separate explicitly admitted edges,
not automatic retry; partial setup then failure; cancellation before and after
SDK handoff; and timeout after trigger followed by attempted new motion. Check
zero-displacement rejection before TX under each supported basis (R-08), unknown reference, counter-zero
invalidation, all positive/negative native boundaries and exact quantization.
An acknowledged trigger with stale bit15 cannot succeed. A new completion with
different reference cannot establish the requested endpoint. Reference revalidation
is explicit and never resumes interrupted motion. Keep controller-observed
completion independent of a later rearm failure/uncertainty, with no automatic
rearm retry. Use the settled processing-time deadline cutoff, not backdated RX.

Refresh RS Position/MoveOperation terminal behavior and I2C partial-effect
patterns, and compare common exact intent vectors without device-frame copying.
Use test-only servo/no-full-step and non-CANopen fixtures to check shared type
neutrality. Keep wrapped angles, velocity, homing and coordinated cyclic motion
outside this assignment; their later prompts consume the corrected contract.

One writer owns relative integration; an independent reviewer traces uncertainty
through later admission, and a disjoint test worker owns the phase matrix. Add
native relative examples only for genuinely supported bases. Run focused
position/axis/action/reference/owner/service suites, full package/Doxygen,
target stack and firmware checks and required CI. Update packet 21, original 33,
examples, coverage and API comments; commit/sync. If a wire base remains unknown,
complete independent tests/code and retain that precise unfinished scope. Do not
block packet 22's qualified absolute/action cases or claim original 33 Complete.
