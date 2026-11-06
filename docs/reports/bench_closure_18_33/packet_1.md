# Packet 1: measured contracts reconciled

[Packet 1](../../prompts/bench_closure_18_33/1_evidence_contracts.md), executed 2026-11-06 from `89a7eea` on `promptset/motorcontrol-can`, with the user's untracked `.vscode/` files left alone. Host work only: no port was opened and no trial was repeated. Back to the [execution index](README.md).

## Method

Two independent read-only reviews compared the maintained summaries with the retained raw records (`rx.txt`, `request.json`, campaign ledgers) of 2026-11-03 to 2026-11-05: one for stale and overbroad claims (items 2, 3, 6), one for regression vectors and observation provenance (items 4, 5). The lead merged their dispositions, checked the disputed provenance against the session transcript, and applied the code changes; a third worker applied the documentation corrections.

Sibling audit: unchanged since the continuation's authoring rows (MotorControl-RS `cd27b12`, FieldCore-node `5ad9292d`, INA228 `73f9548`, all clean). Adopted from RS: two stopped reports before a position match counts as standstill (`positionMatchEvidence`), which the corrected quick-stop rule now follows. FieldCore's separate ordinary and stop histories needed no change here.

## Requirements

| Item | Disposition |
| --- | --- |
| 1 Execution index | [README](README.md): every partial original 18-22 and 27-33 with existing behavior, missing software, device fact, instrument and owner |
| 2 Typed reads and width options | Already implemented as specified; no code change. `ReadKind` 16 kinds, `DriverGroup` 12 groups; `IdentityWidths::MEASURED_U8` and `CountWidths::MEASURED_U16` with the EDS widths as default; `Detail::measuredWidth` gives 2 bytes only for `607D:00` (`DRIVER_SOFT_LIMITS`) and `60FE:00` (`IO_OUTPUT_COMMAND`); a count of 0 never shortens a plan (`DriverReads.cpp` reads fixed subindices). Covered by `reads`, `driver_settings`, `settings_owner`, `device_evidence`, `bench_route`. One stale Doxygen sentence in `Reads.h` corrected |
| 3 Stale summaries | Corrected in place in the maintained guides, PROGRESS, backlog, coverage, roadmap, README, bench guide and conflict table: `2202` written and read back on both nodes (604-607), `60FE:00..02` read under `MEASURED_U16` (608, 609), enables (108, 118, 206, 210, 405, 704, 712) and moves (absolute 121, 213, 413, 705, 706; relative 411, 708) that ran, and the stops of 2026-11-04 and -05 |
| 4 Regression vectors | (a) quick stop with `2100=0014`, `6041=4050`: new `capturedQuickStopsDuringMoves` replays trials 713 and 413 from their captures, predecessor `001F`; (b) two-cycle standstill: **defect found and fixed**, see below; (c) completion at -999 for target -1000, then -1000: `trial706` in `move_absolute_test.cpp`, delivered with packet 2 ([index](README.md)) because the fake drive it needs is packet 2's; (d) no publication of the target as the observed position: none found in code (`Position.cpp` fills `latest` and `completed` from the `6064` read); `trial706` now pins it for absolute moves |
| 5 Observation provenance | **Conflation found:** see below. Runner fixed with a regression; the records keep their bytes and gain a `NOTE.txt` each |
| 6 Overbroad summaries | Narrowed to the tested modes, predecessors, rates, specimens and images; historical reports carry dated corrections |

## Defects

**Quick-stop standstill rule.** `Actions.cpp` completed the latched-running quick stop on a cycle with `606C` zero and `6064` equal to the previous cycle's, without checking that the previous cycle's `606C` was zero, while `Actions.h`, the stop guide and the 2026-11-04 CHANGELOG entry state two consecutive cycles with `606C` zero. Both cycles must now read zero. `capturedQuickStopsDuringMoves` includes a {5, 0} pair at an unchanged `6064`: without the fix it completed at cycle 3 instead of 4, while the captured 713 and 413 timelines complete at cycle 3 either way (checked by mutation). Retained trials are unaffected: both completed under either rule.

**Shaft observation provenance.** Three enables carry a power-up shaft answer that describes no observation of that power-up, and each passed the library guard that refuses an enabled-looking start report unless an attendant observed the shaft:

- **704 and 712** (2026-11-05): the lead typed `HOLDING`, sent as line field 5 = 2, from the drives' start reports (`6041=8437`, `2100=0001`); nobody felt the shafts. The cause was a lead memory note that turned the user's request not to be asked into "record it as the measured norm"; the note is corrected.
- **108** (2026-11-03): the lead relayed `HOLDING` with no fresh question after the user power-cycled node 1 at 11:14:51Z. The last answer, at 11:06Z, came after trial 105's enable and before that power cycle.

The session transcript shows the other recorded shaft answers are the user's own relayed statements: trials 103 and 118 (2026-11-03 08:30Z and 12:20Z, "HOLDING"), 206 and 210 (2026-11-04 11:44Z, "Both holding") and 405 (2026-11-04 15:38Z, "DOne, both holding"). All eight trials now have a `NOTE.txt` naming the source, as does R1 for the user's hand turn; `request.json` and `rx.*` are unchanged.

The runner's enable question now offers `UNOBSERVED`, which refuses the enable before anything is sent and says why (`test_attended_confirmation_is_typed`). An enable without a shaft check would need the user's own recorded acceptance as a separate field, never `HOLDING`; none is implemented.

## Other corrections

- Drive reports presented as physical facts ("holds no torque", "held torque", "pulled back", "coasted") now read as drive-reported, with torque or shaft not observed, in the guides, `Actions.h` and the 2026-11-04 and -05 reports' corrections.
- The evidence class "physical" in the fault and reference guide is renamed "device-reported".
- The closed campaign ledgers of 2026-11-04 and 2026-11-05 are added to `docs/bench/SHA256SUMS.json`.

## Checks

`stop_action` (15,956 checks) and `s2_bench_contract` (32 tests) pass; prompt integrity, repository checks and `git diff --check` pass. The full verifier and exact-commit CI are recorded with the commit.

## Remaining gates and next measurements

Unchanged physical gates: `2202` enforcement, DO electrical behavior (meters), tuning writes (phase-current probe), an alarm clear on a natural alarm, a `1003` read with the fault LED observed, and an independent shaft or torque observation for release and stop claims. The enable on the next power-up needs the user's own shaft observation, asked together with the power cycle.
