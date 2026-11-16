# Independent audit of prompts 52 and 54

Audited `02f404b625feab0c07979964d9985131bf3b400b` on `main` on
16 November 2026. The worktree was clean and a fresh fetch showed no incoming
commits. The original prompts were read in full, together with their shared
contracts. This review inspected the implementation and the actual diffs from
`6775a7ff` through the prompt54 package-link correction.

## Findings and corrections

| Finding | Reproduction and correction |
| --- | --- |
| A queued velocity change could remove the run's own stop | Admit a change just before its complete exchange/observation budget ceases to fit, then delay owner handoff within the queue timeout. Preflight refuses the unsent change, but the session used to synthesize `INVALIDATED`, ending the run with `needsStop` and no scheduled stop. Local time/freshness refusal now preserves the original stop and deadline after rechecking live provenance. Actual generation changes and explicit withdrawals remain interruptions. |
| Experimental change checks used the selected axis | Run node1 velocity, select node2 with a different arming, then change the retained node1 operation. The bench incorrectly rejected it using node2's arming; conversely node2's capacity could mask node1's exhausted cap. Bench and dispatcher now resolve the same retained operation attachment without changing selection. |
| NMT evidence replay accepted contradictory summaries | Mutating an actual native transcript to remove its after sample, change its request ID, or move its heartbeat/sample outside the stage still returned `complete`. Replay now checks the original timing and identity, raw NMT handoff/report, fresh heartbeat, and the seven-field ordinary SDO sample. Valid failed and refused experiments remain representable. |
| SYNC qualification counted malformed target frames | Expected TPDO counts could be satisfied by RTR frames with the same identifier. Raw capture remains transport-neutral; the qualification check requires the reviewed standard data-frame layout and reports malformed target traffic separately. |
| Prompt52's Complete status overstated initial measurement | Bare ramp writes and changed-speed trials do not establish edge-applied ramp-change behavior at nonzero speed. Prompt52 is Partial with its initial Q26 measurement still assigned there; prompt54 retains subsequent qualification. |
| Stop documentation contradicted retained measurements | Q12 and the stop guide now name the specimen's changed-velocity quick stop (52013) and moving `003F -> 000B` observation (52034). Other predecessor/kind/mode behavior and physical standstill remain unqualified. |

No generic motion engine, replay, retry, deadline extension or new drive
qualification was added. The installed change gates remain closed by default.
The changes reuse the existing concrete contexts, one owner and public routes.

## Independent source and evidence checks

MotorControl-RS remains `e56d4c0af6a7d678da11bb672a1b94126812b3a1`, clean:
the position qualification/relation seam is unchanged since prompt52 and the
finite campaign seam since prompt54. Retain the frozen original profile and
original/replacement identity in `Position.cpp` / `MoveRelation.h`; retain
separate primary/cleanup outcomes from `bench_session.py` / `bench_probe.py`.
Reject the RS automatic recovery/stop retry as a CAN policy.
FieldCore-node remains `c40ea8341edf8d665460b902cafefbe857a7f73f`, clean;
`MotorControl.h` / `Rs485MotorModule.cpp` ownership/configuration integration is
unchanged since prompt53. No sibling was modified.

The EN86/ZH86 original PDF pages 12–13 and EDS entries were reopened and their
hashes checked. Bit4/5, parameter ranges and the 60FF signedness/startup-order
conflicts agree with the retained source disposition. No other drive's CiA402
semantics were substituted.

Offline review independently matched all 877 retained P52 terminal results
to one raw reply each and verified all 482 P52 artifact hashes. Decoded motion
cycles agree with the reported tables. P54 runs37/38/40 have valid CRCs,
continuous numbering and the reported frame/TPDO counts and timing spans.
The 236 source hashes in each P54 image were compared: only the documented
four output-drain files differ. These hashes identify the historical images;
they do not identify the corrected audit code as flashed.

Parallel review used `gpt-6-astra` high for lifecycle/uncertainty and
`gpt-6.1-sol` high for diagnostics and independent evidence. Agents returned
isolated reproductions and disjoint changes; the lead reviewed the actual diff
and owns combined verification and Git synchronization.

## Verification

Focused regressions cover queued/pre-admission change refusal, freshness and
provenance distinctions, stop timing, cross-selected axes and target capacity,
and malformed/incomplete NMT and compact-watch evidence.

- `python scripts/verify.py --mode full --compiler gcc --build-dir .build/audit52-54-final`
  passed all 166 CTests, generated/repository/inventory checks, isolated package
  consumers and Doxygen. Tools: GCC15.1, Python3.12.10, Doxygen1.13.2. Logs are
  under `.build/audit52-54-final/20261116T064622670127Z-38844-cjuq7q0n`.
- `python scripts/check_stack.py --target esp32s2 --strict --require-toolchain --build-dir .build/audit52-54-stack --report .build/audit52-54-stack.json --jobs 2`
  passed both `-Os` and `-Og`; known chain bounds remain 5968/6144 bytes,
  below/at the existing 6144-byte cap. No allowance or limit was changed;
  SDK, task-entry and indirect paths remain outside this local measurement.
- Focused checks passed: five lifecycle/owner suites, the bench-console suite,
  148 evidence tests, 12 compact-watch tests and 67 bench-runner tests. Genuine
  native transcripts and retained P54 captures were replayed without hardware.
- `python scripts/check_promptset.py`, its committed-reference check,
  `python scripts/check_repository.py` and `git diff --check` passed.

Exact-commit CI verifies Windows, both Linux sanitizer builds, Arduino and
ESP-IDF firmware/isolated component/stack gates before handoff; its final run
and commit are named in the handoff. Original vendor and bench artifacts are
unchanged. No local physical rerun is claimed.

## Remaining acceptance scope

Prompt52 remains Partial for the initial Q26 ramp experiment. Prompt54 remains
Partial: run38 failed its requested 30-minute 10 ms test at 42988 transmissions;
100 ms passed for 60 seconds only. That failure is not erased by this audit.
Exact intervening phase cost remains unmeasured; throughput work must preserve
the original cutoff and repeat the same sustained qualification.

Both nodes retain their recorded restrictions: node1's unresolved stop/session
and node2's interrupted position/trajectory fence. Further motion needs the
specific external retirement boundary and fresh acquisition. No MCU reset,
ordinary heartbeat or local generation clears that history. Synchronous RPDO
start/stream/shared stop still need M09/Q28; DI5/6 need a wired fixture. There
is no new physical test, flash, mechanical skew or independent standstill
claim in this audit. Native and compile checks cannot close those obligations.

The final commit stays on `main`, is synchronized normally, and is verified
on its own CI run. No release or package publication is part of this task.
