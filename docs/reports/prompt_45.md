# Prompt 45: adversarial ownership and motion integration

2026-11-15. Software audit and corrections on `main`, starting at
`818d36186a558e650e3b39d92dbec731a9d6d487`. No hardware or sibling changes.
The [source audit](../audit/PROMPT_45_SOURCES.md) records revisions, actual seams
and adopted/rejected behavior. This closes the assigned integration audit, not
the outstanding physical or firmware capabilities of earlier Partial prompts.

## Reproduced defects and corrections

All three findings below are host observations. Their failing evidence is retained
under [prompt45 evidence](../audit/evidence/prompt_45/README.md); the regressions
run the existing public paths, not alternate executors.

| Finding and origin | Observable failure, cause and correction | Regression |
| --- | --- | --- |
| P45-1, native ownership | A real fake-owner move loses the `001F` response and requests DEC-2 `000B`. With TX reporting withheld, the active best-effort context still exposed `NOT_TRANSMITTED` and no possible effect after controller handover. `noteBestEffortAdmission` only set admission/time. It now sets `UNKNOWN` and possible effects immediately; a matching retained owner terminal can refine this to definitely unsent. No ACK can confirm this stop. | `move_absolute_test.cpp::lostTrigger`, `axis_session_test.cpp::coreBestEffortUnits`: late identical ACK before/after TX report, no `010F`, immutable move/stop after recovery, definitely-unsent refinement. |
| P45-2, Python framing/evidence | Ctrl-C during the received-line evidence callback escaped the ordinary `Exception` fence. The scenario could send cleanup after bytes were consumed without a checked record. Independent review reproduced the same defect between `_read` and parsing, during startup, and after a short/full write returned but before its count was checked. Fence the complete pump/startup/write-accounting interval and evidence callbacks; preserve the original interrupt and forbid further traffic after framing/evidence loss. Central `_poison` only records the existing failure state. | `bench_campaign_adversarial_test.py` interrupted intent/received evidence, post-read clock, startup and write-return cases; original exception, retained primary identity and zero cleanup/replay traffic. |
| P45-3, input classification | Deep JSON within the 16 KiB fixture limit escaped as a recursion/runtime failure (exit1) instead of a pre-I/O input failure (exit2). Normalize parser/contract depth errors to `ValueError`; retain the existing byte/depth bounds. | `bench_campaign_plans_test.py::test_malformed_fixture_corpus_is_input_failure_before_artifacts_or_serial`: eight deterministic malformed cases, no port or artifacts. |

Coverage gaps, without a production defect: added another-node storm under
ordinary capacity pressure, queued versus remote old-map samples through the
owner, a different-value write after timeout/release, and a move blocked by all
retained ordinary results. The common velocity vector helper now sets every
request field explicitly instead of inheriting rounding/allowance defaults.

## Requirement-to-test review

These are real registered suites, inspected and rerun together. Fake state and
qualification records remain synthetic. No simulated reply qualifies firmware.

| Prompt item | Evidence and scope |
| --- | --- |
| 1: lost trigger, separate stop, late ACK, recovery | Extended `move_absolute::lostTrigger` uses actual preparation, `AxisSession`, owner and selected SDO engine. Ordinary motion stays refused on the unresolved route; one `000B` is transmitted, and both terminals remain unchanged. `interruptions` sweeps five lifecycle phases with stop/cancel/fault/reboot/deadline; its WAITING case specifically proves DEC-8 stop from running `001F`. `motion_console::stopNeverReplacesUnknownMotion` and `bench_session::test_uncertain_move_keeps_failure_after_confirmed_cleanup` retain separate ordinary/cleanup identities and raw evidence. |
| 2: unrelated traffic and pressure | New `can_bus_owner::otherNodeStormAndUrgentAdmission`: node1's active SDO and full ordinary slots while node2 sends 36 heartbeat/EMCY/PDO frames, observer reentry, history/trace overwrite and urgent TX. New `motion_console::urgentStopAtEveryPhaseUnderPressure` combines all five move phases with full blocked output, dropped ordinary replies, node2 heartbeat/EMCY bursts and constrained RX4/TX1/report1 service; urgent stop retains separate results without replay after output resumes. Existing `congestedEscalationKeepsIndependentResults` covers normal-to-fast escalation. `can_bus_health` heartbeat loss, overflow/saturation and `can_bus_recovery` controller faults/queued retirement; `sdo_reconciliation` duplicate/malformed/delayed replies. Bounded callbacks, one receiver, no replay. |
| 3: generations and old wire bytes | New `queuedOldMapFramesNeedProvenanceBoundary` separately delivers pre-change locally queued frames and post-change remote old-map frames with identical ID/DLC. Both stay unqualified until a **synthetic caller-supplied** producer/ingress retirement boundary; this is not physical proof. `lostWriteCannotConfirmDifferentValue` blocks later value admission before/after release and late ACK. Existing move `atomicity/recheckAtHandover`, settings `generationsAndOldTargetAdmission/rebootEndsTheTransaction`, and owner recovery cover admission and accepted-work invalidation. |
| 3: actual settings/remapping APIs | `settings_owner` runs position parameters, soft limits, DI, loop tuning and subdivision through typed APIs and owner, including partial ACK, lost reply, cancellation, readback mismatch and reboot. `pdo_experiment::timerOnOneNodeWhileAnotherStreams/timerFailureAtEveryStep/timerUnresolvedRouteIsNotReused/timerStateHeldTxAndPressure` and `probe_commissioning_app::pdoTimerRoundTripKeepsSampleQualificationSeparate/pdoPartialMapAbortKeepsUnsignedNativeEvidence` exercise the supported typed TPDO1 subset and conditional map path. General remapping/activation still depends on prompt18 evidence; no synthetic test closes it. |
| 4: deadlines, no progress, retention and health | New `move_absolute::retainedResultsPreventProgress` fills every ordinary retained slot, then separately cancels, expires at the exact cutoff, or admits urgent stop; releasing slots cannot revive motion. Existing `processingCutoff/lateReadIsHeldBack`; `sdo_reconciliation::capturedAndProcessingTime/terminalPrecedenceAndLaterObservations/nonwrappingTimeAndZeroCapture`; owner ID exhaustion, copied buffer/release and cancellation tests. `can_bus_health::statisticsResetPreservesUncertaintyHistoryAndLiveWork/recoveryTimeoutAndCounterSaturation` keep streak/evidence across stats reset. Python exact deadlines, frozen clock and partial trailing input are included. |
| 5: common RS meaning | `move_common::intentVectors`: all 12 retained exact position cases reproduced against current RS source (nine source blobs match). `axis::commonRateIntentVectors` names every field, checks signed rates, units, ties-to-even, missing geometry and stale generation; its common in-range expectations match RS `testSharedNumericPreparation`. CAN's wider pure integer target and zero arithmetic are intentional differences from RS's profile-bound helper. No RS frames, address limits or defaults copied. Move/stop/cancel/result integration above checks independent admission, execution and completion. FieldCore's actual `MotorResult` seam keeps the same separation without making FieldCore a dependency. |
| 6: malformed input, portability and resources | Existing byte/command/DLC SDO corpus, console malformed/correlation tests, plus the deterministic fixture and interruption corpus. Strict native warnings, full native/Python/package/Doxygen gates, S2/S3 static resource measurement and exact-commit CI as recorded below. No general fuzz framework, coverage quota or timeout relaxation. |

## Public-code counterexamples and source conflicts

Rechecked the retained SRT source, not just its summary. These existing tests
already reject or preserve uncertainty for the following origins; tests were not
deleted or relabeled as new fixes.

| Origin | Counterexample | Native/public-path protection |
| --- | --- | --- |
| SRT `send_sdo_read`, lines57-78 | `4F/4B/43` as an upload request | `sdo_codec::requests` checks literal `40` and rejects response commands as requests; ordinary typed uploads use this codec. |
| SRT homing, lines146-152 | Two-byte method/rates/ramp and 5000 ms ramp | `sdo_codec::requests`, `homing::failuresAndCorrelation/validation` and homing owner exact sequence enforce one/four-byte destinations and bounded ramps. |
| SRT file-global caches, lines3-36 | One motor's status reused for another | New cross-node owner storm plus typed read/binding and wrong-node tests. No process-wide status cache. |
| SRT cached status/unchecked enable, lines81-109/120-134 | Old completion, sleeps as confirmation, timeout as not-running | Move `staleReports/missingCompletion/lostTrigger`, action/stop service-failure tests retain uncertainty. No response does not become stopped. |
| SRT move, lines93-109 | Gear multiplier on ramp milliseconds; ordinary63/127 triggers | `move_absolute::golden` with gear5 asserts literal ramps100/200 and trigger31; relative sequence asserts95. Immediate update is a separately gated request, not default motion. |
| SRT zero routine, lines166 onward | Hidden save while clearing position | `motion_console::deviceZeroNeverSavesOrSetsAHostOrigin`, reference-action owner traces and persistence tests keep `2302`, host origin and `2300` distinct. |
| EN86 p12 versus ZH86 p12, **manual conflict** | Write mode display `6061` | Typed mode writes target6060; display stays a read observation. Move golden trace never writes6061. This is not a SRT defect. |
| Generic CiA402, **not a Lichuan/SRT claim** | Assume605A/6085 quick-stop configuration | Typed stop prerequisite rejection and exact command tests use only the reviewed native contract. No generic option object is introduced. |

## Storage and bounded work

No new storage or service engine. Installed-core include/source inspection found
only library/C++ headers, pure arithmetic/copy operations and caller storage.
`<new>` is used for placement construction of already-owned contexts, not heap
allocation. No controller I/O, clock, framework, logging or automatic retry was
added. Package consumers verify the dependency boundary; this is not a claim
that optional adapters or host test machinery never allocate.

The existing strict stack tool measured both pinned ESP32-S2 and S3 compilers at
`-Os` and `-Og`, with no baseline exemptions. Maximum individual frame:
2000/2016 bytes. Largest reported project call chain: S2 5952/6064 and S3
5968/6064 bytes, within the unchanged 6144 limit. These chains are **lower bounds**:
SDK, libc and indirect calls are incomplete. A board high-water test is still
required for real runtime stack use.

Measured target sizes are identical on S2/S3: `ReadContext`1944,
`ActionContext`5152, `BestEffortStop`336, `MoveContext`3008,
`VelocityContext`3208, `HomeContext`3320, `AxisSession`35752,
`ProbeConsole`6744 bytes. Host GCC owner test reports `CanBusOwner`14432,
`Completion`272, `Health`72, `Stats`64, `Diagnostic`416 bytes; these are host
layout values, not a board RAM budget. `nm -u` of the built portable archive
showed only internal library symbols, arithmetic runtime calls and exception
personality metadata; no allocation, device-I/O or clock import. The two-channel owner retains fixed
request/result/urgent queues. New storm measurements assert at most RX8, TX2
and report8 callbacks per turn despite oversized caller budgets, with36
observations and12 EMCYs retained/dispatched under bounded diagnostic loss.
This measures work counts, not wall-clock WCET.

## Verification and independent review

Two stronger reasoning reviewers owned motion and owner slices; a coding worker
owned Python. They used disjoint files, with no Git mutations or hardware.
Motion independently reviewed owner correlation/provenance; owner independently
reviewed best-effort settlement and Python interruption. That review found the
post-read Ctrl-C window; the lead then identified its write-accounting counterpart.
Both were reproduced, corrected and independently rechecked before closure.
The lead inspected the actual combined diff, console/FieldCore boundaries and
source-backed common vectors, then ran integration gates.

Commands:

```text
python scripts/verify.py --mode full --compiler gcc --build-dir .build/p45-full
python scripts/check_stack.py --require-toolchain --strict --build-dir .build/p45-stack --report .build/p45-stack/report.json --jobs 2
python scripts/check_promptset.py
python scripts/check_lichuan_ledger.py
python scripts/generate_lichuan_objects.py --check
git diff --check
```

Final local verification passes with GCC15.1, Python3.12.10 and Doxygen1.13.2:
142 registered integration contracts, separate source/install/export consumers
(C++11/C++17/no-RTTI, relocated install and isolated applications), and strict
documentation extraction of35 installed headers and146 free functions. Logs and
summary: `.build/p45-full/20261115T082155736082Z-52492-ne9t0pyx/`.
The seven focused Python suites also pass149 methods normally and149 under `-O`.
Stack, generated objects, ledger, planning/index provenance and diff checks pass.

The first combined run had140 passing contracts and two planning failures because
report links were tested before the report file was created. Those failures remain
in `.build/p45-full/20261115T081235103163Z-28580-f693973l/`; both focused checks and
the complete final run pass after the files were finished. No test was weakened.

Linux ASan/UBSan are provided by the pinned GCC/Clang CI jobs; this Windows host does
not claim a sanitizer run.
Automatic firmware jobs compile Arduino and ESP-IDF without flashing. Required
CI is checked on the exact final commit and reported in the handoff; an older
green run is not reused.

## Remaining obligations and next work

- Prompt18's general remap/activation and old remote PDO retirement need their
  exact device evidence. Current typed TPDO1 subset and conditional host tests
  do not establish arbitrary mappings or erase Q18 route ambiguity.
- Prompts20-22/29-33/35-36/38-39 retain their existing per-row firmware, input,
  reference, reverse/start-order, route and persistence gates. The source ledger
  and owning reports remain authoritative; no Partial row was reset to Pending.
- Backlog M01/M02/M03/M12 describe new coordinated/example multi-axis workflows,
  not acceptance criteria delivered by this audit of existing operations.
  Cross-node receive/urgency tests do not implement stop-all or multi-axis CLI.
- Physical motion, electrical faults, timestamp capture and sustained-load
  qualification remain separate47-48 work. No serial port, board, drive or motor
  was accessed here.

Next assigned software work is prompt46's independent package/consumer/drive
contrast audit. No new branch, release, tag or package publication is included.
