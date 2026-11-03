# Packet 8: measured action stack and atomic publication

Executes audit W7, A-11 and C-15. Host-side compiler measurements only. **The live stop stack is not measured: no firmware holds an `AxisSession` yet, and packet 16 owns that measurement.** Passing here says the statically known chains fit; it does not bound the stack, because every chain has unknown edges.

## Delivered

- **`scripts/check_stack.py`** compiles with the pinned toolchain for S2 and S3, at `-Os` and `-Og`, with the real IDF flags, and parses `-fstack-usage` and `-fcallgraph-info=su`. Project limits: 2,048 bytes per function, 6,144 bytes per known chain. It records compiler and source hashes, and fails on unknown growth. CI runs it `--strict --require-toolchain` in the `idf` job with the build's own compile databases. A missing compiler locally is a visible skip, never a pass.
- **Validate, then publish.** Prepare, advance and notice paths no longer copy a whole context. `validateStop`, `validateEscalation`, `validateRead` and `validateStateBlock` run the refusals with no output, then the call publishes in place. Rejected calls still leave outputs unchanged. `resetAction` and `resetRead` construct in place, because `c = ActionContext()` measured at 7,536 bytes and placement at 32.
- **Smaller `ActionContext`.** See the [changelog](../../../CHANGELOG.md) for the migration: `AlarmIncident` replaces `originalState`, one `extraObservation`, and no duplicate `commandEvidence`.
- **One bug found on the way.** `MEASURED_U8` was accepted at admission and refused at the first observation, after the command was already acknowledged. It is now refused at admission.

## Before and after

S2 and S3 agree. Budgets: 2,048 and 6,144. Baseline entries: 71 to 0.

| Measure | Before `-Os` / `-Og` | After `-Os` / `-Og` |
| --- | --- | --- |
| Largest frame | 7,968 / 7,968 | 1,712 / 1,664, `getStateInterpretation` |
| Worst known chain | 20,448 / 20,816 | 2,832 / 3,040, `FirmwareProbe::service` to `dispatchPdo` |
| `requestStop` chain | 20,448 / 20,816 | 2,096 / 2,464 |
| `sizeof(ActionContext)` | 7,504 | 4,880 |
| `sizeof(AxisSession)` | 16,152 | 10,904 |

| Function, frame | Before `-Os` / `-Og` | After `-Os` / `-Og` |
| --- | --- | --- |
| `advanceAction`, terminal | 7,968 / 7,952 | 448 / 448 |
| `noteActionTransmission`, axis | 7,952 / 7,968 | 112 / 144 |
| `prepareClearAlarm` | 7,616 / 7,600 | 112 / 96 |
| `prepareSetDevicePosition` | 7,616 / 7,632 | 96 / 112 |
| `publishStop` | 7,552 / 7,552 | under 64 / 48 |
| `requestStop` | 7,616 / 7,648 | 160 / 192 |
| `AxisSession::open` | 7,616 / 7,632 | 448 / 464 |
| `prepareDriverSettings` | 4,032 / 4,032 | 624 / 608 |
| `prepareIoSettings` | 3,408 / 3,408 | 976 / 976 |
| `advanceDriver`, terminal | 2,864 / 2,848 | 464 / 464 |
| `advanceRead`, terminal | 2,352 / 2,400 | 192 / 240 |
| Reads `prepare` | 2,320 / 2,352 | 112 / 96, plus `validateInputs` 368 / 384 |
| `ProbeApp::request` | 2,096 / 2,096 | 144 / 192 |

## What these numbers do not cover

- **Every chain is a lower bound.** The graph holds 36 unknown targets on S3 and 41 on S2, all libc or libgcc, plus 28 located indirect call sites. At `-Os` GCC also writes 230 unlocated indirect records, reported separately.
- **Not compiled:** `IdfTwaiDriver.cpp`, `EspProbeMain.cpp`, `EspProbePlatform.cpp`, and the `s2_bench` and `probe_idf` `main.cpp` files, which need SDK headers. Arduino's flags differ (`-fexceptions`, `-fstack-protector`).
- **SDK and ISR use.** 12,288 less 6,144 leaves 6,144 bytes for those. That remainder is stated, not measured.
- **`FIRMWARE_STACK_BYTES`** did not change.

## Contract changes

Recorded in the changelog, each with its test:

1. `originalState` becomes `AlarmIncident`, for an alarm clear only. **Per-field provenance is lost**: one earliest and latest bound remains. An application needing more keeps its own read.
2. `errorObservation` and `feedbackObservation` merge into `extraObservation`, selected by `block`.
3. `commandEvidence` is gone; read `commandHistory`.
4. `MEASURED_U8` is refused at admission.
5. `requestStop` treats a `state` held inside the overwritten slot as missing.

## Review findings

An independent reviewer tried to tear a rejected output: reading every exit, 158,863 logged refusals with a full-output compare, a tamper run of 20,000 seeds across 7 flows, 36 aliasing cases, and a differential against HEAD of 445,229 trace lines flipping every input byte. **No rejected call changed an output byte.** Only the five intended `MEASURED_U8` lines differ from HEAD. PDO routes, tuning and session logic were outside that differential, and no Xtensa padding check ran. It spot-checked `.su` frames against this report, and they match.

| Finding | Fix |
| --- | --- |
| A `MEASURED_U8` policy refused every stop, including `000B`, though that width concerns identity reads. The bench specimens need it, so shared options would have hit this at packet 16 | `requestStop` ignores the width |
| `requestStop` formed `stops_[kind]` before checking `kind` | Checked first |
| `check_stack.py` did not cover or list `examples/s2_bench` or `probe_idf`, though the packet requires application paths | Added; the two `main.cpp` files are listed as unmeasured |
| `validateStateBlock` had no overlap check on its output | Added |
| `extraObservation` reads as ERRORS with `fieldCount` 0 before its first cycle | Documented |
| The task high-water mark was reachable only in `s2_bench` | `firmwareStackFreeBytes` in `EspProbeMain`, on demand, because it scans the unused stack |

A `state` held inside the slot being overwritten now downgrades a stop to the best-effort path, or to refusal, where it used to be accepted. This is recorded and tested. A caller cannot tell this cause from others.

## Sibling refresh

| Sibling | Revision | Dirty | Seam | Disposition |
| --- | --- | --- | --- | --- |
| MotorControl-RS | `e59c5ca` | Clean | Concrete-context publication | Delta since packet 7 is one audit log commit. `src/profiles/ess_rs/Actions.cpp` holds no whole-context candidate copy. RS has no comparable stack limit. Adopted the validate-then-publish direction |
| ADS1115, INA228 | Unchanged since packet 5 | Clean | Borrowed input, terminal | Same rule: inputs are borrowed during the call, so overlap is rejected before publishing |
| FieldCore-node | `5ad9292` | Clean | Not touched | Unchanged; read-only |

## Checks

See the [execution table](README.md).
