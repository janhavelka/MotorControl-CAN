# Prompt 36: homing and native coverage reconciliation

## Scope and remaining gates

The conditional homing lifecycle covers Lichuan methods 17, 18, 24 and 29,
with explicit mode 6 enabling and interruption through the existing owner.
Six typed homing reads and a manual native-coverage reconciliation belong here.
Original prompt 36 remains **Partial** until the selected firmware, method,
physical inputs, direction, offset and completion/reference facts are qualified.
Host simulation is not that evidence. No physical homing trial was run.

The consumed artifacts are the existing exact-axis/rate preparation, typed
read contexts, action lifecycle and bounded CAN owner. No release command,
automatic mode change, input reassignment, persistence or second service engine
is introduced. Earlier owned omissions stay assigned to their original prompts
in the [coverage reconciliation](prompt_36_coverage.md).

## Source refresh

Entry revision `09a94eee93ee47078e9d5d729b9b152cb23b1e82`, existing execution branch
`promptset/motorcontrol-can`, upstream `origin/promptset/motorcontrol-can`.
Fresh fetch found zero ahead/behind. Three unrelated untracked `.vscode` JSON
files were preserved. Sibling repositories were inspected read-only.

| Source | Revision / dirty state | Behavior adopted or rejected |
| --- | --- | --- |
| MotorControl-RS | `cd27b12389f568198b72cecb6606123ea4c089bc`, clean | `profiles/ess_rs/Homing.h` and `Homing.cpp`: adopt `prepareHome`, `nextHome`, `advanceHome`, checked `getHomeReference`, method-specific admission/poll inputs and retained interrupted outcomes. Reject transfer of ESS method/function numbers or its wire sequence. |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0`, clean | `MotorControl.h::MotorAxisSnapshot/sameMotorAxis` and `Rs485MotorModule.cpp::finish/startMotor/invalidateConfiguration`: adopt explicit reference/configuration generations and withdrawal after possible motion. No homing operation exists. The opt-in `renewReadinessOnState` branch can publish a checked fresh reference; no source/include enables it. The blanket statement in `rs485_devices.md` does not describe that branch. No integration changes made. |
| RV3032-C7 | `1b74184977be42e4145e2bfe47d7029e32480a5b`, clean | `RV3032.h::ConfigurationJobReport` and `RV3032.cpp::pollJob`: adopt independent first operation and stop status retention, analogous to separate operation/cleanup status. Do not borrow its cleanup or restoration behavior as a motor stop policy. |

Original EN86/ZH86 PDF pages 13-15 and the homing EDS sections were reopened.
Both manuals give mode-6 completion bit 8 and bit-4 start/interrupt edges.
Method 29 finishes on leaving origin during positive creep; it is not merely
method 24 with signs exchanged. Pages 17/18 establish `2101` logical effective
input bits and `2400:01..08` polarity/functions. Applying polarity again to
`2101` would be wrong. Matching required function numbers to each homing switch
is an explicit host policy inferred from their names.

The `607C` table does not establish its write order, units, sign or final
coordinate effect. `2200` does not establish homing direction. Mode-6 enabling,
enabled mode changes, initial active switches and origin searches encountering
limits keep their separate gates. Original files and hashes were preserved.
EPOS4 rel13740 page 232 was visually checked: methods 17/18 appear, 24/29 do not;
that contrast rejects those method numbers, not servo homing in general.

## Implementation contract

See [homing](../homing.md) for the authoritative method/prerequisite and evidence
contract, and [native coverage](prompt_36_coverage.md) for prior ownership.

- Exact positive search/creep rates use the existing axis path once, require
  integer motor turns per minute, and reject a nonzero rate rounded to zero.
  Wrong native widths and documentary/admission ranges are checked before work.
- Preparation freezes the method, firmware tuple, native parameters, mapping,
  raw direction and generations. Defaults leave every new physical capability
  closed. Acquisition preserves raw unknown values instead of treating them as
  valid motion settings.
- Homing writes its own parameters, retires retained motion-parameter reads,
  and sends one `001F` trigger. It never enables implicitly or uses the position
  rearm path. Coordinates are withdrawn once before trigger admission and owner
  configuration advancement, so the real trigger ACK and polls share the new
  binding. Parameter qualification explicitly requires no coordinate effect
  before that trigger; immediate-effect `607C` firmware is outside this path.
- Completion needs new qualified operation evidence, including the method's
  input behavior. `getHomeReference` checks the current generations and age;
  it publishes neither a host origin nor an independent physical accuracy claim.
- Local cancellation sends no stop. A separately admitted mode-6 HALT emits
  `000F` from an actual acknowledged `001F`; the whole word is derived, with
  `predecessorDocumented=false`. Uncertain urgent `000B` remains the existing
  explicitly requested, unconfirmed DEC-2 path. Operation and stop failures
  remain separate; recovery does not resume homing.

## Verification

| Check | Result |
| --- | --- |
| Full native/Python verifier | Passed `python scripts/verify.py --mode full --compiler gcc --build-dir .build/p36-verified`: 107 CTest tests, independent source/export/relocated installed consumers, repository registration and generated freshness. GCC 15.1, CMake 4.0.1, Python 3.12.10. |
| Direct homing and owner paths | Passed 4,406 direct homing checks, 3,745 mode-6 action checks and 3,703 owner checks. Covers all four methods, unsigned rate destination/sign/ranges, wrong widths, atomic refusals, fresh completion, stop escalation, lost replies, deadline, boot-up, fault, reference invalidation and terminal retention across reopen. |
| Typed reads and ledger | Driver settings 4,318 checks; 83 ledger and 46 generator tests. All six home read fields retain raw values and Q20 gates. The descriptor sweep confirms 102 readable rows with the same 16 accepted DEC-1 rows. |
| Documentation | Strict Doxygen 1.13.2 and API coverage passed in the full verifier. Promptset links, 140 immutable reference hashes and diff checks passed. |
| S2 firmware | `check_firmware.py --target s2` passed compile/link against reviewed IDF 5.5.5 and pinned Xtensa 14.2.0; 508 compile entries. Evidence: `.build/p36-s2-final/summary.json`. No flash, port or physical traffic. |
| S2/S3 compiler stack checks | `check_stack.py --strict --require-toolchain` passed both targets at `-Os` and `-Og`, no frame over 2048 bytes or known chain over 6144. `HomeContext=3320`, `AxisSession=35368` bytes. Evidence: `.build/p36-stack-final/report.json`. Chain figures exclude reported unknown/indirect calls and are not measured task high-water usage. |
| Independent review | Separate source, typed-read/action, lifecycle and stronger ownership/reference reviews. Final actual-code review found no unresolved correctness issue after the corrections below. |

Review corrected positive request-sign validation, explicit deferred-offset
qualification, reference checks before any setup write, current-generation trigger
admission, first stop failure retention through escalation, and immutable home
history across reopen. The first integrated suite also exposed stale descriptor
count and mode-6 stop-table expectations; both now assert the complete new
contract. The clean final full run above passed. No failing test was removed or
weakened. An initial local SDK invocation lacked the existing IDF tools environment;
using its actual constraints/toolchain directory restored the normal checks,
without disabling dependency verification.

Sibling revisions and clean state were rechecked at exit and remained unchanged.
Code, tests, docs and generated files stay on the recorded execution branch;
unrelated `.vscode` files are excluded. Exact final commit, remote synchronization
and automatic CI results are reported at handoff. The final push's existing CI
covers Windows, Linux GCC/Clang sanitizers, Arduino and IDF builds; local Windows
does not substitute for those remote matrix checks.

## Next physical evidence

No current bench record establishes homing switch wiring, logical polarity,
direction under raw `2200`, safe mode-6 startup, exact `607C` effects, or a
method-specific completion/reference sequence. These are specific prerequisites
for a finite prompt-29 experiment, not permission to run an input-free search.
The [homing guide](../homing.md) lists the measurements. General homing uploads
remain Q20-gated; no successful SDO is used to claim motion or reference semantics.
