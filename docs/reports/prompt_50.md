# Prompt50: independent software-candidate review

Date: 2026-11-16, Europe/Prague. Implementation began after the requested midnight.
Starting revision: `6db3448909f139721836b9040f6ce8c159326aad`, clean `main`,
freshly fetched and synchronized with `origin/main`. The version0 instruction
keeps this work on main. No tag, publication, merge, sibling change or hardware
operation belongs to this review.

## Verdict and scope

Owned software review and local verification: Complete. Exact-commit CI and
synchronization are checked after the commit, before handoff, as described below.
This is an unreleased **0.1.0 software candidate**, not completion of every
planned feature or physical qualification. Prompt49 supplies the usable package
and documentation prerequisite. Prompt18/Q17 is an inherited feature gate, not
a prerequisite to this review. Earlier Partial prompts remain Partial; prompts
51–72 are separate assignments.

The installed C++11 core provides exact units/quantization, logical axes and
references, Classical CAN framing, checked CANopen envelopes and conditional
Lichuan typed operations. The optional application owner/stack, shared console
and finite Python clients consume those public operations. Only Lichuan C is
installed; N5 stepper/BLDC, EPOS4 and proprietary-frame fixtures demonstrate
specific common contracts, not additional drivers. FieldCore integration remains
prospective. No new runtime capability or firmware gate is enabled by this review.

## Independent review and resolved findings

Read the original prompt and pasted attachment in full, shared contracts, all
49 numbered report verdicts, actual coverage/code/tests and source artifacts.
[Report inventory](prompt_50_report_inventory.json) records the reviewed report
hashes and scopes: 31 completed owned/software scopes and 18 Partial scopes
before this prompt. Detailed code review is sampled; mechanical reconciliation
does not claim every source line was independently inspected.

Three bounded reviewers ran in parallel: `gpt-6-astra` at high effort for
state/stop/ownership, and two `gpt-6.1-sol` high reviewers for coverage/evidence
and package/portability. The lead reproduced findings and reviewed the integrated
diff. The stronger reviewer also reviewed the final package correction. Workers
did not mutate Git, siblings or hardware.

| Finding | Correction and evidence |
| --- | --- |
| README relative `-D` paths split after `=` in Windows PowerShell | Quote all three complete arguments. The old install command configured an empty prefix. The package harness now executes the actual exported README shell commands, checks the install prefix before installation and checks the consumer resolved that exact installation. The old-command negative case stops after configuration, before build/install; the corrected strict C++11 consumer prints `1536`. |
| Two ledger fault-clear claims cited preliminary research | Point to the actual later November13 campaign and the [raw recheck](prompt_50_fault_evidence.json); regenerate maintained output. No object type/value/access/qualification flag changes. |
| Fault-clear counts and observation timing were conflated | Preserve initial failed trial5323 separately from nine later explicit OBJECT attempts and eleven CONTROLWORD attempts. Later acquisitions reported eight sustained OBJECT clears/one recurrence and eleven CONTROLWORD released reports. Immediate CONTROLWORD observations were `2100=0001`, then `6041=8050`; separate later reads reported `0011/8050`. Correct the maintained guide, ledger, coverage and progress without rewriting original captures or historical reports. |
| Stale scope/build statements | Remove profile-subset-build wording from requirements/new-drive guidance; the single library always builds Lichuan. Record the delivered installed consumer/production owner integration, retained physical observations and later `2500` bench experiments without qualifying them. |

The 21 fault-clear archive/member hashes match the retained manifest. The lead
independently inspected initial5323, controlword5352 and its follow-up5353 raw
bytes. Success of a later attempt never rewrites initial5323's failed lifecycle.
These are two specimens, one reported firmware, mode3/FF05; physical release,
other faults, internal rearm and other firmware remain unqualified.

No new runtime defect was confirmed in the reviewed owner, service or motion
paths. Review covered copied admission and retained results, immutable deadlines,
urgent pressure, occupied-channel ambiguity, external queue retirement, held stop
and recovery, passive startup/getters, health separation, exact units and rate
bases. Local cancellation is not stop; a matching service reply is not physical
completion; recovery never replays uncertain work. Host epochs cannot qualify
old same-route PDO samples. These limits remain explicit in the public contracts.

## Sources and accounting

[Source audit](prompt_50_sources.json) records exact live revisions, dirty state,
working/Git-byte hashes, symbols and adopted/rejected behavior. MotorControl-RS
advanced to clean `bbacead2023d1e207b80b8439f27103184f642d2`; its explicit release,
retained copied results and execution evidence were reopened. CAN deliberately
does not import RTU drain timing or RS whole-sequence cancellation. ADS1115,
INA228 and FieldCore seams are unchanged since prompt49, including file hashes.
No sibling was fetched or changed.

Original EN86 PDF pages16/18, ZH86 page13, EN86 page13 and raw EDS sections were
sampled independently against implementation and tests. Reviewed facts include
U16 action objects, heartbeat default0, millisecond ramps and conflicting
controlword access/mode types/velocity startup. Pinned CANopenNode
`9b8beed8367241e96ac03f916cd5a500bcb2cf23` upload index/subindex, expedited size,
segmented toggle/count and FIFO bounds were inspected. No generic CiA402 fallback,
new timing promise or invented physical observation resolved those conflicts.

The checker reconciles 99 EDS indexes, 202 typed scalar entries and 53 manual
obligations; 192 typed read rows, 15 conditional setting rows, 22 conditional
manual workflows and 191 public overloads remain separately classified. These
counts are not a count of qualified motor features. Four confirmed standalone
read omissions remain with their original owners: `605D` (31), `2301/2302` (32),
and `607A` (33). Typed raw access does not resolve read-effect gate Q20.

## Verification

Focused checks passed: 39 reused unchanged native contract suites; 46 operation
checker adversarial cases; generator/ledger/operation/promptset integrity; actual
PowerShell README positive and old-command negative runs; Python offline entry
points. Both retained P47 trial replays47301/47302 pass with 88 terminals and
264 traffic records each, without promoting physical qualification.

Final integrated command:

```text
python scripts/verify.py --mode full --compiler gcc --build-dir .build/p50-final
```

[Verification record](prompt_50_verification.json) retains the complete passed
local summary and commands. All147 selected native/Python CTest contracts passed
(261.20s), followed by32 package commands and strict Doxygen1.13.2. XML inventory
contains all37 public headers,150 free-function names and3859 members. Tools:
GCC15.1.0, CMake4.0.1, Ninja1.12.1 and Python3.12.10 on Windows. Generated/version/
operations, ledger, promptset and whitespace checks passed. The literal README
consumer built in an isolated exported directory containing spaces and printed1536.

These are working-tree checks based on6db34489, not a claim that the old commit
contains this correction. Required push CI checks the actual final commit with
Windows quick, Linux GCC/Clang ASan/UBSan, Arduino3.3.11 and IDF5.5.5
S3/S2/board/bench/component/stack gates. Local Windows testing does not claim
Linux sanitizers or new physical tests.

The candidate is the commit that introduces this report, obtainable without the
conversation using the following command. Its GitHub commit checks and retained
workflow artifacts identify the exact CI revision and logs; an older green run
is not substituted. The lead verifies all five required jobs and fresh upstream
ahead/behind equality before the final handoff. No manual dispatch or publication
is part of this workflow.

```text
git log -1 --format=%H --diff-filter=A -- docs/reports/prompt_50.md
```

## Consumption and next work

Use the [README](../../README.md)'s checked install and complete C++11 consumer.
Include `MotorControlCAN/MotorControlCAN.h` and link
`MotorControlCAN::MotorControlCAN`; the installed core needs no SDK, Python,
stack headers or sibling checkout. Full-checkout native read/move/stop and angle/
velocity previews use fake evidence. See [candidate notes](../releases/0.1.0.md)
for the descriptor migration and distinct support layers.

Firmware entry points and pins are in [firmware examples](../firmware_examples.md)
and [S2 bench](../../examples/s2_bench/README.md). Build commands include
`python scripts/check_firmware.py --target arduino`, `--target idf` and
`--target s2`; building is not flashing or qualifying. Pins/dependencies remain
Arduino3.3.11, PlatformIO6.1.19 and ESP-IDF5.5.5. S2 Arduino and a complete P4
application remain outside the demonstrated builds.

For offline runner intake use `python scripts/bench_probe.py --plan-only --scenario identity`,
`python scripts/bench_probe.py --self-test` and `python scripts/bench_scenarios.py --list`.
Live invocation needs the actual console/fixture facts and finite intent from
[runners](../runners.md); no historical COM port or drive state is a current fact.

Next work is the separately assigned ready prompts, starting51 when requested,
plus earlier Partial rows' named omissions and measurements. The [roadmap](../roadmap.md)
and [backlog](../backlog.md) retain release/mode, read effects, scale, I/O, remapping,
homing, communication/persistence, related-model and platform obligations.
P47/P48 remain Partial: independently observed shaft/stop/electrical behavior,
home/limit fixtures, failure/related-model campaigns and the unlocalized64-byte
serial diagnostic loss still need evidence. No full-library-completion claim follows.

## Local reproduction residue

The first literal README reproduction installed43 files outside the intended
workspace prefix, under `C:/include/MotorControlCAN`, `C:/lib` and
`C:/share/MotorControl-CAN`. Prior existence was not inventoried, so possible
overwrites are unknown. The exact reconstructed list and policy response are
retained in [local reproduction record](prompt_50_local_reproduction.json).
Tool policy rejected manifest-scoped cleanup with only `blocked by policy` as
the reason. Cleanup was not retried or bypassed; these files remain an explicit
local residue. The new package checks cannot be made green by that installation:
they verify the isolated consumer's resolved package path.

No architecture or AGENTS change was needed. Reused the package harness and
ledger generator; no new runtime framework, state, queue or public declaration.
Source Doxygen contracts remain unchanged and are rechecked by the full gate.
