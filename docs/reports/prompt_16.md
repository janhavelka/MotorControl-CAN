# Prompt 16: explicit NMT and service eligibility

## Scope and implementation

Implements reviewed generic CANopen NMT request encoding and the selected
native producer in the existing optional owner. One bounded manager admission
and immutable local result accompany the existing two SDO/node slots and
four copied diagnostic records. No extra receiver, node registry, local NMT
slave, startup traffic, retry, drive operation or PDO implementation was added.
Installed consumers retain only the pure stack-free envelope API.

`Nmt.h`/`Nmt.cpp`, shared `NetworkState.h`/`Network.cpp` and `SdoClient` own
explicit intent, affected binding/epoch snapshots, native staging, possible TX,
actual TX, immutable deadline and retained terminal. Addressed node 1–127 is
separate from bus-wide target zero. Membership is frozen through release.
NMT has no remote acknowledgement. Independent snapshot fields retain requested
command and observed state, pending/matched/conflicting/expired/invalidated
wait evidence. Only qualified later capture can update current qualification.

Start/Pre-operational leave active SDO deadlines intact. Stop or known-current
Stopped retires affected ordinary SDO work through its existing uncertainty
rules. Reset revokes actual dependencies/epochs and keeps permanent SDO
quarantine. Prior terminals, faults and uncertain effects survive every path.
Nonreset callbacks revoke dependent service/prepared eligibility while retaining
unchanged map/configuration/scale/reference facts. No NMT request implements
physical stop or CiA402 quick stop.

The single [service contract/table](../canopen_services.md#service-eligibility)
separates exact NMT, SDO service evidence, producer interval and application
health. Admission, handoff and enqueue checks enforce it. Explicit external
confirmation has separate provenance from a matching SDO observation. Age is
caller policy: nonzero inclusive microseconds; documented default UINT64_MAX
uses lifecycle validity only, without inventing a device timeout. Heartbeat-
disabled/unknown startup permits an explicit reviewed non-changing probe and
then prerequisite-qualified requested writes, without an implicit Start or
producer write. Unknown exact NMT allows configured TPDO receive validation;
this stage performs no Operational inference. PDO work remains prompt 17.

## Refreshed sources and evidence limits

Read current implementation, callers/tests and workflow seams read-only:

| Source | Revision and dirty state | Relevant evidence / delta |
| --- | --- | --- |
| MotorControl-RS | `e44110f866a2d162b10039e2f5825ff87f47caf1`, dirty | Advanced from prompt-15 `a5a02b7`; committed stored-position interruption plus concurrent uncommitted ProbeApp/ProbeConsole/runner/inventory work. `examples/probe_cli/StateCache.h:26–75` distinguishes attempted, historical and current observations; `test/probe_app_test.cpp:1248–1272` proves unsent cancellation versus possible release effect. Dirty `ProbeApp.cpp:303–353` preserves uncertainty across correlation recovery and revokes derived currentness, retaining raw history. These working-file observations are not identified by HEAD alone. |
| ADS1115 | `235af3e8c722661a89a44766a4ccd903a654f6fe`, clean | Unchanged; `src/ADS1115.cpp:308–337,1040–1103`, owner-safe-poll example and basic cancellation tests separate candidate configuration, possible effect and retained token-matched result. |
| INA228 | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b`, clean | Unchanged; `src/INA228.cpp:528–563,1330–1359`, bringup owner caller and no-I/O bind tests separate desired configuration, verified state and explicit reset effects. No sensor timing/retry policy is imported. |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0`, clean | Unchanged; `Rs485MotorModule.cpp:50–75,640–679`, `Rs485Task.cpp:1131–1147` and host motor module tests preserve terminals while withdrawing binding/reference/readiness. No FieldCore includes or edits. |

CANopenNode pin `9b8beed8367241e96ac03f916cd5a500bcb2cf23`:
`301/CO_NMT_Heartbeat.h:40–56,81–87,272–281` supplies service rules, five
command bytes, manager role and domain target. `CO_NMT_Heartbeat.c:274–288`
is the actual producer. Restricted producer-field initialization is documented
and tested; full init/process would require local OD/EM and emit boot-up even
when producer heartbeat is zero. Neither is called. The original SDO engine
remains active; no native HB/EM consumer is duplicated.

The offline subset now preserves 14 unmodified pinned blobs with licenses and
exact hashes. Three added files are the NMT C/header and emergency header needed
to compile its inline queries; EMCY features are disabled. The new C file was
freshly fetched independently from the pinned primary URL and is byte-identical:
11,192 bytes, SHA-256
`2ad2b2f92ea1198de0ca3b28465d50039a8de09f6f33587ee9735c7b63e2589d`,
Git blob `55b12d8f2b5697c6093088f64e46603a38ba9bbc`.

Original EN86 and ZH86 PDF page 3 / printed 1, sections 1.1–1.2, visually
rechecked: general CiA301/CiA402 support, with SDO/PDO/EMCY listed, without a
command-specific NMT procedure. Page 15 / printed 13 repeats protocol support;
its 6040-bit-7 reset is not an NMT reset. Original EDS lines 26–27 claim
SimpleBootUpMaster=0 / Slave=1; 1017 is RW U16/default zero; 1016/100C/100D
are absent. No measured startup/reset interval, command-specific firmware
behavior or host-loss stop guarantee follows. These are documentary claims;
native command support is generic CANopen support, not Lichuan qualification.
Original bytes/attributes and all unresolved ledger permissions are preserved.

## Independent review and corrections

Parallel source and literal-vector review used gpt-6.1-sol/high; independent
state/broadcast/deadline review used gpt-6-astra/high. The lead owns production
storage, CMake, integration and Git; workers edited only the assigned core
codec/vector files or the independent NMT test file. No worker Git mutations.

Review exposed and corrected admission-only SDO eligibility, lifecycle events
defeating NMT cutoff priority, and pending intents incorrectly spanning unrelated
epochs. Current Stopped now retires affected work immediately; cached handoffs
and later segments recheck eligibility. Expiry-only processing precedes lifecycle
termination. Invalidated intents retain history without matching new-epoch
heartbeat. Qualified reset boot-up is distinguished from unknown/equal capture
and explicit host reboot reports. Tests also cover UINT64_MAX arithmetic.

The old passive two-node interleave fixture now uses Pre-operational instead
of Stopped, preserving its SDO service premise. Dedicated Stopped tests exercise
the new retirement contract, including proven-unsent and uncertain changing/
non-changing requests. Earlier terminals remain byte-identical. No assertions
or integrity gates were removed. Adding three pinned source files required
updating the independent artifact-count expectation from 134 to 137; the first
full run caught that stale fixture, with its failed log retained.

## Verification and handoff

Independent strict C++11 core codec compilation and actual native producer
build pass. The integrated NMT owner test reports 1,531 checks; focused SDO,
network and reconciliation tests pass after the reviewed corrections.

- `python scripts/verify.py --mode quick --compiler gcc --build-dir .build/p16-quick-complete`:
  passed 28/28 contracts, run `20261029T204951585104Z-76260-7ct6lcm3`.
- `python scripts/verify.py --mode full --compiler gcc --build-dir .build/p16-final`:
  passed 29/29 non-package contracts, separate source/export/relocated-install
  consumers, 13 independent public headers, C++11/C++17/no-RTTI and strict
  Doxygen, run `20261029T204838002749Z-87660-unu66hyh`.
- Final integrated `python scripts/verify.py --mode full --compiler gcc --build-dir .build/p16-complete`
  passes all 29 contracts, package consumers and strict documentation after the
  reset-command notification addition, run `20261029T205236252998Z-11128-hicqlia3`.
  The expanded native target passes 1,531 checks; direct Doxygen 1.13.2 passes.
- `python scripts/check_promptset.py` and `git diff --check` pass. Original
  bytes are checked against INDEX before commit and HEAD after commit, including
  all 137 artifacts, 99 indexes, 202 typed entries and 53 manual items.

Failed integration runs remain retained: initial full
`20261029T204507564046Z-8524-8kqr_fpo` caught the stale artifact-count fixture;
quick `20261029T204716981168Z-71392-2ehybotd` ran before the new report link
target was present. The final linked tree passes integrity; no gate was weakened.
Current Windows x86-64 storage: SdoClient 3,536 bytes, NodeSnapshot 360,
NmtTransfer 72 and NmtCompletion 176. These are nested sizes, not additive
embedded RAM/stack or timing qualification. Capacities remain two node/channel
slots, one NMT request/result and four copied diagnostics.

Local tools remain Python 3.12.10, CMake/CTest 4.0.1, Ninja 1.12.1,
GCC 15.1.0 and Doxygen 1.13.2. Local Linux sanitizer runtimes/Clang are absent;
the existing Windows quick and Linux GCC/Clang sanitizer workflow verifies the
exact final commit. No unchanged sibling suite or physical test is run.

Execution continues on `promptset/motorcontrol-can`, upstream
`origin/promptset/motorcontrol-can`, entry/base for this prompt
`a2e982c814b28be94d2961e3b50b04c231aa0f22`; entry fetch was 0 ahead/0 behind,
clean worktree/index. Completed changes are committed and synchronized on that
same branch, with exact-commit CI checked before handoff. No publication/merge.

Reused/simplified: one existing node/channel table and dispatch, selected native
producer and SDO engine, existing callback, public frame/Status types and gates.
New code is a pure encoder and one concrete manager request/projection, without
a universal engine or second registry. Doxygen, service/owner/adapter guides,
README, CHANGELOG, source map, coverage, backlog, roadmap and PROGRESS accompany
the contract. No confirmed prompt-16 software work is assigned to later stages;
prompt 17 owns PDO decoding/maps. Hardware, controller timing and command-specific
firmware qualification remain unperformed.
