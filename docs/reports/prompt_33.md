# Prompt 33: native position parameters and finite-motion gates

**Resumed by packet 20, 2026-11-03.** [Packet 20](handoff_19_33/packet_20.md) added the absolute lifecycle: common `MoveOperation.h`, `prepareMoveAbsolute`, `nextMove`, `advanceMove`, `MoveContext` and `AxisSession::requestMove`, with literal-byte tests in `test/move_absolute_test.cpp`. It is gated: the default `MoveEvidence` returns `UNSUPPORTED`, acceptance and completion are caller inputs that packet 19 must measure, and no specimen had run it then; on 2026-11-03 it ran once, unattended, on node 2 ([unattended session](2026-11-03_unattended_bench.md)), which qualifies nothing. Relative moves stay with packet 21 (next note). Two statements below are stale: the requirement 4 row and the blocker row say a stop needs the latest acknowledged `000F`. Since packet 7, `prepareStop` follows the DEC-8 predecessors, and since packet 20 a stop from the held `001F` can be sequenced from the move's last complete cycle, or else take the best-effort `000B`. Neither path has run during a move since (packet 16); the attended moving HALT was the separate `position` experiment. Prompt 33 stays Partial. Gates: local gates. CI failed at `01190cf` on a test registered twice, unrelated to moves, and passed at `f1116b1`.

**Bench continuation packet 2, 2026-11-06.** [Packet 2](bench_closure_18_33/packet_2.md) closed the host gaps: the admission recheck is one nonmutating `validateMoveAdmission`, `preflight` refuses a queued download in an unhandled EMCY's turn and the session then refuses motion (`faultBlocked`), and the two-node, fault-cycle, COMMANDED and RS-vector tests exist. Remaining: node 2's relative base, a negative relative move and stops from `005F` (bench continuation packet 8); prompt 33 stays Partial.

**Packet 21, 2026-11-03.** [Packet 21](handoff_19_33/packet_21.md) added `prepareMoveRelative` on the same `nextMove`/`advanceMove` path, with trigger `005F`, `RelativeBase` and `MoveError::RELATIVE_BASIS`, and `AxisSession::requestMoveRelative`. No relative base is evidenced, so every relative request is refused `UNSUPPORTED` before any work; a queued base is always refused. Zero displacement is refused before TX under every basis (R-08). The interruption matrix runs through the real fake owner in `test/move_relative_test.cpp` and `test/move_common_test.cpp`. Still owed per specimen and firmware: whether `005F` adds to the actual or the last commanded position, acceptance and completion after `005F`, and everything packet 20 owes. **Prompt 33 stays Partial**: the relative base is unresolved and no move has run.

Date: 2026-11-02. **Partial.** Pure native target/parameter preparation, owned
snapshots, focused tests and a native installed-core example are implemented.
The requested finite move lifecycle is not implemented: current source/firmware
evidence cannot establish its acceptance/completion or real stop interruption.
No simulated success or always-rejecting lifecycle is counted as completion.

Entry HEAD `9e5b62db4db9ff71c28a3cca691be37a0300e7ad`, promptset v2, original
base `7878f7c`, branch `promptset/motorcontrol-can`, upstream
`origin/promptset/motorcontrol-can`. Fresh fetch showed 0 ahead/0 behind; index
empty. The three unrelated `.vscode` files and untracked
`docs/audit/PROMPTSET_AUDIT_34_50_2026-11-02.md` are preserved and excluded.
During this work another session committed that audit document as `2fecfa9` on
the same branch. Its sole-file diff was inspected and preserved; it does not
change the library or this prompt. The final checkpoint builds on that commit.

## Owned scope and implementation

`LICHUAN_C::preparePositionParameters` in the installed `Position.h/.cpp` is a
small pure wrapper around production `preparePosition`. It copies the complete
axis configuration, exact target/quantization/reference, explicit 86C model and
native `DriverSettings`. It checks requested and effective `607A` values before
I32 narrowing, U32 rpm/ms bounds and actual current reference freshness. It
retains explicit approximate conversion only under the existing conservative
error bound. No arithmetic engine, service queue, clock, heap, SDK or firmware
qualification flag was added.

Relative native bounds apply to displacement, while common axis limits apply
to the endpoint. Exact requested range is reconstructed from existing
quantization evidence without subtracting an INT64_MIN witness or duplicating
conversion. Approximate intervals compare their upward error bound with exact
distances to the range edges; tiny outward uncertainty cannot round away.
Zero remains numeric intent, not inferred standstill or completed motion.
The [guide](../position_parameters.md) owns this contract and the lifecycle gates.

| Prompt requirement | Delivered scope / exact remaining obligation |
| --- | --- |
| 1. Finite absolute/relative APIs | `prepareMoveAbsolute`, `prepareMoveRelative`, `nextMove`, `advanceMove`, `MoveContext` and common `MoveOperation.h` remain unimplemented. Pure `preparePositionParameters` is a separate real function, not a substitute completion claim or dormant engine. |
| 2. Immutable intent/native bounds | Implemented exact owned request, quantization, axis scale/origins/limits, binding/config/reference provenance and native pulse/rpm/ms checks. A qualified dialect/generation cannot be captured because no applicable record exists. All three relative bases are host arithmetic only. |
| 3. Setup through fresh completion | Existing settings and raw typed reads are reused as evidence of available building blocks. No motion sequence is generated while acceptance/reset/rearm and selected completion semantics remain unresolved. |
| 4. Stop/cancel/fault/deadline | Existing owner/action/axis regressions run. A triggered move cannot use the current latest-000F-only stop contract. Move-specific uncertain trajectory retention and later admission after acknowledged trigger remain unimplemented. |
| 5. Snapshot reuse/zero | Copied preparation is inspectable requested intent, never a verified settings cache. Tests cover mutation independence and actual axis/reference invalidation. No refresh lease or zero-distance completion is inferred. Runtime snapshot reuse/invalidation belongs to the still-owned lifecycle. |
| 6. Physical versus controller evidence | Preparation generates neither. No host fake is presented as qualified acceptance, completion, standstill or supported relative wire basis. |

## Source-confirmed values and exact blockers

Original EN86/ZH86 PDF p11-12 and OP86 p9-10 were reopened, rendered and
visually inspected, together with EDS `[607A]`, `[6081]`, `[6083]`, `[6084]`.
Documented values are I32 target +/-1,000,000 pulses, U32 speed 0..3000 rpm and
U32 ramp times 0..2000 ms. The mode example requests `6060=1` and observes
`6061=1`; it does not authorize writing the display. `000F -> 001F/005F` is the
absolute/relative bit4 rising edge; bit5 immediate forms do not establish a FIFO.

No source replacement, constant correction or physical measurement was needed.
The canonical `position_parameters` row records only the implemented arithmetic
subset; whole-function and hardware coverage remain false. Original hashes,
write policies and unresolved claims are preserved.

| Missing fact | Why it prevents the remaining work | Required evidence |
| --- | --- | --- |
| Selected firmware status/acceptance/completion | EN86 bit12 says target effective without a new-command handshake; bit15 can be stale. ZH86/OP86 dialects differ and OP86 has 57C internal headings. | Actual model/firmware/configuration plus applicable vendor or authorized finite fixture evidence for clearing/reassertion, new acceptance and completion, including zero/short moves and stale reports. Q01/Q02. |
| Stop after acknowledged or uncertain trigger | Current `prepareStop` requires actual latest acknowledged `000F`. Trigger `001F/005F`, or uncertain TX, consumes that prerequisite; copied old evidence or an enabling write cannot restore it. | Qualified transition and resulting observations from triggered/unknown states, including interrupted trajectory/queue/torque/reference consequences. Prompt 31/Q12 remains owner. |
| Relative wire basis and feedback relationship | Common ACTUAL/COMMANDED/QUEUED witnesses are mathematical policy, not drive firmware behavior; raw6064 has no qualified command-coordinate relation. | Exact relative reference rule and independent scale/origin relationship under selected firmware, including consecutive requests and interrupted trajectories. Q04/Q05/Q02. |
| Mode/setup/rearm and timing effects | Mode selection/enable admission remain gated; changed settings advance config and invalidate an earlier prepared target. Zero ramp is numerically listed but operational meaning is unknown. | Evidence-backed current-state ordering, target/trigger rearm and parameter effects; supplied host deadlines do not establish device timing. Q11 plus prompt 30 admission gates. |

The existing SDO owner correctly quarantines a possibly transmitted unconfirmed
trigger and rejects new reservations. A separate important case remains for
the future move lifecycle: a trigger can be acknowledged while motion
completion later times out with a resolved SDO route. The owner intentionally
allows a newer operation and has no trajectory knowledge. A move context must
retain this uncertainty and inhibit new motion independently of communication
health. No current stop path resolves it. Q20 and inherited 20/31/32 obligations
also remain visible. Use the prompt-29 finite evidence route when its actual
fixture and authorization prerequisites exist; no fabricated firmware binding
or qualification Boolean closes these rows.

## Fresh ecosystem audit

Read actual declarations, implementations, callers/tests and CI read-only. No
sibling fetch, edit or unchanged sibling test run was performed.

| Checkout | Revision/state | Relevant behavior and CAN decision |
| --- | --- | --- |
| MotorControl-RS | Entry `6ff237471c77a9c4755ca87128c33619e5165f1b`, 40 dirty paths; later clean `3b9411a1181fb4e1af9bca10ce0bb49d09b0f8ca` | Position/Axis, profile snapshots, exact target and terminal tests, owner/CLI callers. The inspected Position.cpp and position_test.cpp hashes match the later commit. A running-poll option is a host policy; ESS widths/fixture limits, ACTUAL-only wire basis and zero-displacement rejection are not CAN facts. |
| ADS1115 | `d613a13cd3d43471905c507bfb4094bba1c1da94`, clean | Admitted ChannelRequest copy and matched retained-terminal consumption; no motor retry imported. |
| INA228 | `73f954838d3fac8cce747d54d0ad738940d4c625`, clean | Request/config copies, refusal to replace retained results, hardware validity and partial effects. |
| BME280 | `48b8214dcf68e0fd23c849258abfc24bed29c233`, clean | Partial configuration preserves dirty hardware evidence; local rollback before exposed writes is not an undo of motor effects. |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0`, clean | MotorControl.h/MotorModule/Rs485MotorModule exact int64 targets, rational scales, axis/reference generations and separate target/operation/step correlation. |

Live retained hashes/state are in `.build/p33-source-audit.json`. The prior RS
dirty state was not attributed solely to HEAD. The current CAN common axis
implementation already owns exact conversions; the new profile wrapper adds
only documentary native constraints and an owned full configuration snapshot.

## Tests, review and handoff

`p33_ecosystem` (`gpt-6.1-sol`, high) audited siblings and owned only the new
header/source. `p33_owner_review` (`gpt-6-astra`, high) traced unknown/acknowledged
trigger failure and later admission, then owned the disjoint tests/example.
`p33_evidence` (`gpt-6-astra`, high) independently reopened original sources and
reviewed numeric bounds/atomicity. All three independently confirmed the finite
lifecycle gates. The lead owns package/build registration, guides, coverage,
combined verification and Git. No worker touched hardware or Git state.

The focused strict C++11 GCC test passes 175 checks. It covers both signed native
boundaries, unrepresentable values, requested versus rounded limits, endpoint
versus displacement, three host bases, origin/relative rounding parity, explicit
approximation, zero intent, bad settings/model/representation, stale time/epochs,
INT64_MIN witness, copied inputs, nested aliases and actual axis invalidation.
The native example runs without a CLI. Complete combined verification and the
final commit/sync are recorded at handoff. This is a new installed API/package
milestone: exact-final-commit native sanitizer and firmware CI are required.
No serial/CAN port was opened, firmware uploaded or motor command sent.

| Combined verification | Result |
| --- | --- |
| Full Windows GCC 15.1 verifier | PASS: 57/57 CTests, repository/registration and generated checks; command `python scripts/verify.py --mode full --compiler gcc --build-dir .build/p33-full` |
| Package | PASS: source/installed/relocated C++11/C++17/no-RTTI consumers and the new example linked against the relocated core |
| Doxygen | PASS: strict 1.13.2, 26 public headers, 1,821 members and 69 free functions |
| Ledger/generator/integrity | PASS: 99 indexes, 202 entries, 53 manual items, current generated outputs and all 137 original hashes; 225 Markdown files/2,240 links |
| Formatting/diff | PASS: clang-format 14 on all four new C++ files and `git diff --check` |
| Storage | Windows GCC x64 `sizeof(PreparedPositionParameters)=560` bytes. No target stack high-water or execution-time claim. |
| Remote CI | Required on the final commit; exact run and outcomes retained in `.build/p33-final-ci.json` and final handoff |

Full passing artifacts:
`.build/p33-full/20261102T132306114885Z-42340-uju1h6mc/`.
The initial run `20261102T132234417521Z-58728-eqf24cn5` caught the omitted
export allowlist entry for the new guide. The actual allowlist and its independent
test fixture were updated together without weakening validation; the full rerun
passed. The later unrelated audit-document commit required only a fresh integrity
check, which also passed. Originals were not replaced; staged/HEAD hash checks
are included in synchronization because the reviewed ledger note was updated.
