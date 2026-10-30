# Fresh independent audit of prompts 20 and 21

## Entry, scope and result

Both original prompts were reread in full against current declarations,
implementation, callers, tests, original sources and commits `6eb900b`/`afc51c0`.
Entry was `afc51c057c788568c2e2d6a5dd49691873ac05e7`, clean index/tree, fresh
fetch ahead/behind 0/0, on `promptset/motorcontrol-can` with upstream
`origin/promptset/motorcontrol-can`. Execution identity remains version 2,
original base `7878f7c`; no new branch, sibling change or later prompt execution.

The existing 32 raw read rows and getters had no reproduced runtime defect.
**Two substantive scope gaps were confirmed and fixed:** missing settings
software was being treated as a blanket manufacturer-evidence blocker. The
confirmed parameter and DI setting groups are now implemented, rather than
closed by raw reads or unsupported setters. Both prompts remain **Partial** for
their genuinely unresolved groups below, with physical qualification separate.

## Primary evidence and corrections

| Earlier claim | Rechecked original evidence / applicability | Corrected scope |
| --- | --- | --- |
| No admissible position-parameter write can be implemented without a future control-history API. | EN86/ZH86 PDF p11: 6081 U32 0..3000 rpm, 6083/6084 U32 0..2000 ms. P12 places writes after position mode/6040=15 and before a separate bit-4 trigger. EDS 6040 is WO. | Implement actual retained control completion, checked state and continuous owner exclusion. Never upload 6040 or emit enable/mode/trigger preambles. |
| Missing DI drive-state/uniqueness/activation rules prohibit explicit reassignment. | EN86 p8/p18 and ZH86 p8/p18-19 consistently permit CANopen DI configuration: 2400:01 bits0..6 and :02..08 codes0..13, RW U16. | Implement whole-candidate explicit DI register settings and exact readback; do not invent state/uniqueness/order rules or claim electrical safety/activation. |
| Unknown zero-speed/ramp physics excludes native zero settings. | Both original mode tables explicitly include zero; the parameter-setting step precedes the motion trigger. | Accept all documented native bounds. Zero speed is not a stop API, and zero ramp establishes no physical acceleration/trajectory guarantee. |

Original EDS/PDF hashes match the canonical source records. Relevant originals
were rendered under `.build/audit20-sources`, `.build/audit21-sources` and
`.build/audit2021-lead`. No immutable bytes/extracts/conflict alternatives were
edited. The canonical ledger records the corrected prerequisites and eleven
conditional setting rows separately from read/whole-object/hardware coverage.
Generic descriptor write policy remains qualification-required; no raw escape
or blanket writable capability was generated.

Source-confirmed facts are the native addresses, widths, access, code/range and
documented sequence distinctions above. Continuous exclusion, inclusive age,
host write order and the conservative monitor admission subset are explicit
application policies. DO/range/frame conflicts remain contradicted or unknown.
No device/physical behavior was measured.

## Implemented contracts

`Settings.h`/`Settings.cpp` introduce one concrete `DriverSettingsContext` for
two finite native groups, reusing existing group readback and SDO work/terminal
types. No second service engine, node table, receiver, scheduler, heap, clock or
future motion context was added. The context is **2432 bytes** on local GCC
Windows x86-64; this is not an embedded stack/resource qualification.

`prepareDriverSettings` accepts explicit CL86-C/OL86-C identity, the actual
successful matching `6040:00=0x000F` download, subsequent checked STATUS/MONITORS
and current native parameter snapshot. Mode1, agreed status low bits7 and monitor
running/alarm/disabled bits clear define the supported pre-trigger subset.
Age starts at the earliest observation/admission bound, including time zero.
The owner-issued reservation precedes all prerequisites and remains live through
readback; every step checks it and the actual current binding. Tokens are local
exclusion, never CANopen wire correlation. Other control/mode/target/RPDO work
must revoke or be excluded by the sole application owner.

`prepareIoSettings` accepts only the documented DI candidate. All seven
functions and polarity bits must be explicitly selected; defaults are invalid
so partial initialization cannot silently replace protective assignments.
Unexplained polarity bits come from fresh matched prior readback. Duplicate
documented codes are legal; no invented firmware constraint is introduced.
Only changed fields are written, in deterministic source order. Changing an
active input assignment/polarity can immediately affect enable/stop/limit
behavior; no electrical/physical safety is implied by a register operation.

Both preparation paths validate complete candidates atomically. Overloaded
`nextDriver/advanceDriver` copy yielded payloads and consume matched retained
terminals. Per-field acknowledgements, requested words, times, raw aborts and
possible/uncertain effects survive later failures. Readback uses the same native
types and exact full candidate; mismatch prevents publication. No-op candidates
succeed from fresh prior evidence with no TX or epoch change. Local cancel is
neither rollback nor motor stop. No automatic retry, save, reset, fault clear,
controlword/mode write or hidden protective reassignment is emitted.

`noteDriverTransmission` advances the actual application/axis configuration
epoch once, before possible controller handoff. Stale prepared targets/read
snapshots then fail normal epoch checks. Only the exact continuously reserved
transaction finishes its frozen old-epoch downloads. After releasing old results,
`SdoClient::advanceConfiguration` updates the same healthy idle route for actual
new-epoch readback. It rejects pending/retained/cleanup/quarantined work, wrong
identities, generation exhaustion and NMT conflicts; it cannot reopen ambiguity
or renew liveness. Cache callbacks/PDO revocation precede binding publication.
Node state, timestamps, histories and original sample tags remain independent.

Independent integration review caught and corrected two defects in the new
implementation before delivery: local withdrawal during unqualified readback
must propagate nested read uncertainty; and old PDO slots need explicit fresh
new-configuration map replacement after withdrawal. New map evidence does not
qualify samples: same-ID delayed bytes still require producer/ingress retirement
or another existing proven boundary. Repeated configuration advances before
map restoration are covered without allowing rebind or quarantine reopening.

## Independent review and live sibling delta

Two `gpt-6-astra` high agents independently challenged prompt-20 and prompt-21
sources and then reviewed state/effect/epoch integration. A `gpt-6.1-sol` high
agent audited actual software/diffs and owned the independent write suite.
Adapter/PDO changes and their regression had disjoint ownership; the lead owned
portable contracts/implementation, canonical ledger/generation, build/package,
guides and Git. Worker results were followed by integrated checks.

All seven sibling worktrees were clean, with no revision delta. Implementations,
actual callers/tests and workflows were reopened; unchanged suites were not run.

| Checkout / current HEAD | Rechecked implementation and caller/test seam |
| --- | --- |
| MotorControl-RS `f812d7c1d364a03be86f02ae8f648dc8fca23d42` | DriverSettings/ControlSettings, getDriver/getIo, whole-candidate and partial-effect tests, installed consumer/ProbeApp and motion-profile snapshots. Names and role meanings reused, not ESS widths/state limits. |
| ADS1115 `235af3e8c722661a89a44766a4ccd903a654f6fe` | Owner-safe-poll terminal/config generation consumption; sensor auto-recovery is not motor policy. |
| INA228 `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` | Configuration/calibration validation, read effects, freshness/generation handling and callers/tests; no sensor rollback imported. |
| BME280 `d538f2e203ee26d6ddbc86f86d2b0cf4c73a6e98` | Staged candidate/readback and stale-sample generation regression. |
| PCA9555 `498178e4d9e4acb6fab7d8b816ad5021efe8806a` | Shadow/observation fencing, partial masks and uncertain output tests; latch readback is not measured level. |
| MCP45HVX1 `c6c7feefd79b9fc16ddc7b7786ad187e5185fde6` | Explicit terminal/shutdown controls and firmware/native callers; register acknowledgement versus electrical effect. |
| FieldCore `5ad9292dc8481cf08be9a61f662495a6474d1da0` | Motor configuration/reference invalidation and AuxInputs enabled/known/active projections/tests; read-only consumer reference. |

## Verification and documentation

Fresh baseline GCC15.1 strict Release audit passed 11 focused CTests, including
3,091 read, 2,108 driver-read and 7,230 I/O-read checks. The new independent
settings suite passes **26,992 checks**: real fake-controller prerequisite
acquisition, literal writes, exclusion/revocation, every partial write/readback
failure position, raw aborts, masks/codes/ranges including zero, no-op/sparse
requests, invalid defaults, immutable inputs, matching/new epochs, old target
rejection, mismatches, stale/wrong-kind atomic getters and deadline/cancellation
uncertainty. Existing adapter regressions plus new callback/PDO restoration
vectors pass. The preview and isolated native consumer use actual public APIs.

| Integrated check | Actual result / retained evidence |
| --- | --- |
| `python scripts/verify.py --mode quick --build-dir .build/audit2021-quick` | PASS: 37 registered contracts, repository/registration and real version/object freshness. `20261030T125112005341Z-90456-aygm4r1d/summary.json`. |
| `python scripts/verify.py --mode full --build-dir .build/audit2021-full` | PASS: 38 registered contracts, independent exported/relocated installed consumers, C++11/C++17/no-RTTI and strict Doxygen/API extraction of all 21 public headers. `20261030T125112005341Z-42448-uuqzqboa/summary.json`. |
| Settings and PDO native regression | PASS: 26,992 / 591 checks respectively, including repeated epoch changes before map restoration. Existing read/driver/I/O and service/network/proof suites also pass integrated registration. |
| Ledger / generator Python contracts | PASS: 69 / 38 cases, including wrong setting scope/owner/access, absent implementation versus host-test claims and false hardware qualification. |
| `python scripts/check_promptset.py --check-git-ref HEAD` | PASS: 99 indexes, 202 entries, 53 manual functions, 24 conflicts and all 137 original/committed artifact hashes. INDEX and post-commit HEAD checks accompany delivery. |
| `git diff --check`, new/changed handwritten portable C++ formatter check | PASS; generated files were regenerated, not manually edited. |

Local tools: Python 3.12.10, CMake/CTest 4.0.1, Ninja 1.12.1, GCC 15.1.0,
Doxygen 1.13.2 and formatter 14.0.0. Windows has no supported Linux sanitizer
runtime; exact-final-commit GCC/Clang sanitizer CI is verified separately at
handoff, never inferred from local or older green runs. No physical test,
firmware qualification or full application-owner completion is claimed.

Public Doxygen, driver/I/O guides, preview, isolated consumers, generated coverage,
README/CHANGELOG, source/API maps, owner/services/architecture docs, PROGRESS,
roadmap/backlog and coverage now describe actual settings support. CLI remains
planned; original checkpoint reports link this correction without rewriting their
historical verification. Commit/sync and required exact-commit CI accompany handoff.

## Remaining scope

Prompt 20 still owns direction/CAN meaning, subdivision conflicting range/default
and scale activation, soft-limit frame/bounds/enforcement and mode-transition
effects. Current/holding remains solely 22. Prompt 21 still owns disputed DO
address/function/polarity, unbounded filter semantics and physical output actions:
86C sources do not establish terminal bit identity; OP86's 57C header and ZH57's
DO0/1/2 versus DO1/2/3 labels cannot be imported as an 86C writable union.
Those dependent groups retain their own validation/effect/readback obligations;
they are not closed by this audit. Actual settings must be integrated through
the full owner when prompts 23/45 resume. Missing hardware blocks qualification,
not the implemented independently supported software.
