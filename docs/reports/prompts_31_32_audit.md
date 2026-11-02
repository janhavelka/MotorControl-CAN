# Independent audit of prompts 31 and 32

Date: 2026-11-02. Audited entry commit
`031ca043ea473f738ba5ff011e6d3a5f3b0decfa`, including implementation commits
`1cb0d54` and `031ca04`. Promptset v2 remains on `promptset/motorcontrol-can`,
tracking `origin/promptset/motorcontrol-can`; fresh fetch showed 0 ahead/0 behind.
The index was empty. Three unrelated `.vscode` files and
`docs/audit/PROMPTSET_AUDIT_34_50_2026-11-02.md` remain untouched and excluded.

## Findings and corrections

The original prompts were reread in full. Independent reviewers inspected the
actual implementation, original vendor pages, owner/service/axis paths, tests
and both implementation diffs. **No confirmed runtime defect or vendor-value
transcription error was found in the implemented conditional subsets.** Both
prompts remain Partial for the original firmware and predecessor gates; this
audit does not relabel them complete.

The concrete coverage gap was what happens after a failed counter action when
the application tries to admit later work. Existing tests checked coordinate
withdrawal and failed terminals, but did not follow both outcomes through owner
configuration synchronization and a later reservation. Two regressions now prove:

- Acknowledged counter trigger followed by definite no-TX cancellation of final
  zero preserves the failed action and withdrawn coordinates. The retained old
  result blocks epoch synchronization. After explicit release/synchronization,
  old-epoch work rejects and new-epoch feedback acquisition succeeds. That later
  zero report cannot complete the old action or publish an origin/home.
- An uncertain trigger with an unretired controller frame blocks operation and
  configuration advancement. A late TX report/ACK preserves quarantine. After
  releasing both result and sequence, a fresh reservation still rejects on the
  unresolved actual route; neither local identity replacement nor a new epoch
  causes a trailing write or replay.

Private action code now names the reviewed `2100` monitor masks and uses an
explicit three-case observation check. An independent review confirmed the
same complete predicates: stop rejects wrong mode/alarm/release and observes
not-running; alarm clear rejects wrong mode/running and observes alarm clear;
counter clear rejects wrong mode/running/alarm and observes raw position zero.
Scope failure still takes precedence over completion. No wire value, public
type, deadline, storage capacity, dependency or supported capability changed.
The [action guide](../fault_reference_actions.md) now states the failed-action
handoff explicitly. No generic action framework or second service engine was
introduced.

## Requirement coverage against actual code

| Original scope | Audit result |
| --- | --- |
| 31 transitions and `prepareStop` | One native write; fresh latest acknowledged `000F` under continuous exclusion is a prerequisite. Prior halt/quick-stop/release/target-trigger words reject rather than receiving an enabling preamble. Broader transitions remain gated. |
| 31 urgent admission and retirement | Actual owner and selected SDO engine exercised under ordinary/result pressure; old queued work is cancelled and fenced by operation identity. Other-node work and asynchronous dispatch remain available. Quarantine is not bypassed. |
| 31 independent interruption outcomes | Original operation/result retained independently from stop. Unknown TX, late reply, fault, reboot, cancellation, observation failures and exact processing deadline preserve uncertainty and the first terminal. |
| 32 explicit reset/position scope | FF05 vendor-object attempt and zero-only `2302` edge sequence implemented; arbitrary nonzero position rejects before work. Separate controlword reset remains unresolved. |
| 32 retained fault and prefix history | Original raw603F/status/error history copied; application/owner EMCY and interrupted results retained. Trigger ACK survives final-zero failure. No automatic retry/enable/motion/save/reset. |
| 32 reference withdrawal and later admission | Existing COORDINATES invalidation runs before possible trigger TX, advances both epochs once and invalidates cached/prepared coordinates. New regressions trace failed/uncertain outcomes through actual later owner admission. |
| Common publication/time contract | Atomic rejected calls, borrowed-call inputs, owned admitted payloads, complete route/generation correlation, immutable terminal evidence and nonwrapping time/step handling checked. |
| Native tests and documentation | Public APIs tested independently of CLI; source/installed package checks retained. Named private masks and explicit cases improve reviewability without widening scope. |

## Fresh source and evidence audit

All sibling inspection was read-only; no fetch, edit or unchanged sibling suite
was performed. Revisions and relevant working-file hashes are retained locally
in `.build/audit3132-sources.json`. RS is concurrently dirty, so observations
refer to the inspected working files as well as its HEAD.
At exit RS had 29 dirty paths; the inspected hashes and all sibling revisions
were unchanged from the lead snapshot. The other four checkouts stayed clean.
The exit snapshot is `.build/audit3132-sources-exit.json`.

| Checkout | Revision/state | Actual source seams |
| --- | --- | --- |
| MotorControl-RS | `6ff237471c77a9c4755ca87128c33619e5165f1b`, 15 dirty paths at lead snapshot | Actions, action tests and RtuBusOwner cancellation/result retention. Current ActionOptions adds a move/home running-poll interval; it does not change this CAN action contract. The inspected Actions/ActionEvidence/test hashes match the prior exit snapshot. |
| ADS1115 | `d613a13cd3d43471905c507bfb4094bba1c1da94`, clean | Config callback lifetime, admitted profile copies, bus-silent cancellation, retained first failure, test_basic, owner_safe_poll and actual CI. No sensor retry policy imported. |
| INA228 | `73f954838d3fac8cce747d54d0ad738940d4c625`, clean | Hardware validity after partial/uncertain writes and retained-result ownership tests. |
| PCA9555 | `414e0f66b13082e974ec27a4a6e86985a65d83fd`, clean | Explicit outputs, partial effects and cancellation tests. |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0`, clean | Rs485MotorModule reference/binding invalidation and retained witnesses; motor_module reference consumers. New reference admission remains separate from old operation history. |

Reopened original EN86/ZH86 p11-12, p15, EN86 p16-18/p20, ZH86 p18,
OP86 p1/p10/p13 and EDS `[6040]`, `[605D]`, `[2301]`, `[2302]`, `[603F]`,
`[6064]`. The stop arrows, agreed monitor bits, FF05 mapping/resettable status,
U16 clear objects, position-clear edge/manual return and I32 actual position
match the code. EN86 omits the halt row. OP86's 86C cover versus internal 57C
headings remains a real applicability conflict. No original or ledger fact was
changed. `603F` latching, undocumented fault-code mappings, reset-route
equivalence, broader stop transitions and physical counter/torque/queue effects
remain unknown; no generic CiA402 fallback supplies them.

## Review and verification

Three independent `gpt-6-astra` / high reviewers worked in parallel:
`audit3132_lifecycle` reviewed lifecycle and RS; `audit3132_owner` reviewed
ownership/reference and added only the disjoint reference test changes;
`audit3132_sources` reopened vendor sources and checked requirements/ADS1115.
The lead verified the actual diffs and handled private source simplification,
documentation, combined checks and Git. The lifecycle reviewer independently
checked that the final simplification preserves all observation predicates.

Focused fresh builds passed six lifecycle/read/axis/owner suites and five
owner/action/SDO suites. The extended reference executable reports **10,256
checks passed**. Artifacts: `.build/audit3132-lifecycle` and
`.build/audit3132-owner`.

| Combined check | Result |
| --- | --- |
| Full native verifier | PASS, GCC 15.1: 55/55 CTests, repository/test registration and generated files. Command: `python scripts/verify.py --mode full --compiler gcc --build-dir .build/audit3132-full`. |
| Package consumers | PASS, isolated source/installed/relocated C++11/C++17 consumers with no SDK or sibling dependency. |
| Documentation | PASS, strict Doxygen 1.13.2, public header extraction and updated action guide. |
| Source ledger and generated outputs | PASS, 99 indexes, 202 typed entries, 53 manual items; generator check is current. |
| Promptset integrity | PASS, 222 Markdown files, 2,224 local links and all 137 original hashes. |
| Format and whitespace | PASS, clang-format 14 on the two changed C++ files and `git diff --check`. |

Combined logs: `.build/audit3132-full/20261102T130227639840Z-32868-cfz9eu6x/`.
The final commit and fresh synchronization check are recorded in the handoff;
only the eight audit-owned files are staged.

This audit changes regression coverage, private equivalent control flow and
documentation. Under the impact-based CI policy it does not require manual
remote dispatch or waiting for repeated firmware jobs. Automatic push triggers
remain unchanged; previous green CI is not presented as verification of a new
commit. All relevant local native/package/documentation/integrity gates are
required before synchronization. No serial/CAN port was opened or hardware
qualification performed. The existing resource/firmware limitations remain.
