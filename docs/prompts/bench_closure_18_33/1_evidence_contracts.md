# Packet 1 Reconcile measured contracts and remaining scope

Apply [COMMON](COMMON.md). Originals: 19-22 and 29-33. This is host work now;
do not open the port or repeat physical trials just to repair documentation.

## Inspect

Read the originals in full and `docs/prompts/PROGRESS.md`, `docs/backlog.md`,
`docs/reads.md`, `docs/driver_settings.md`, `docs/io_settings.md`, `docs/tuning.md`,
`docs/drive_actions.md`, `docs/stop_actions.md`, `docs/fault_reference_actions.md`
and `docs/position_parameters.md` at the affected sections. Compare retained
Nov4/Nov5 reports and raw trial records, not merely their prose conclusions.
Refresh RS action/position terminal evidence, INA228 hardware validity and
FieldCore's separate ordinary/stop histories read-only.

Read actual `Reads.h`, `Reads.cpp::expectedWidth`, `Actions.cpp`, `Position.cpp`
under the Lichuan profile and `examples/common/AxisSession.cpp`, with callers
and tests. The original/manual conflict tables remain applicable evidence.

## Implement

1. Create the small execution index required by COMMON. For each partial original
   18-22/27-33 identify existing public behavior, missing software, missing device
   fact, missing instrument/observation, and its exact owner. Keep 23-26 delivered.
   Do not manufacture a missing implementation from the word Partial.
2. Verify that all assigned typed read groups, identity MEASURED_U8 and count
   MEASURED_U16 options already exist. The count option covers 607D:00/60FE:00;
   it does not reinterpret count zero as no entries. Preserve raw width/value
   evidence, default EDS behavior and explicit variant selection. Do not loosen
   all read widths or globally accept Q20 reads because a session succeeded.
3. Correct stale maintained summaries: 2202 codes0/1 write/readback completed
   on both nodes in Nov4 trials604-607; 60FE counts already have measured-width
   support and the I/O reads completed; enable and position have run. Keep
   enforcement, DO electrical behavior and tuning writes specifically gated.
   Historical report prose stays dated; use a current correction/addendum where
   necessary rather than altering original traffic or pretending an old run
   used new code.
4. Add independent regression vectors only where absent: quick stop with monitor
   RUNNING latched and RELEASED set; the existing two-cycle standstill rule;
   completion at -999 for target -1000 followed by settling; no publication of
   the target as observed position. Source literals from retained captures.
   Check `test/stop_action_test.cpp`, `move_absolute_test.cpp`,
   `alarm_clear_test.cpp`, `counter_clear_test.cpp`, `axis_session_test.cpp` first.
5. Review observation provenance in bench parsing and action admission. A run
   based on drive reports alone must not set an independently observed shaft or
   torque flag. Preserve a genuine recorded user acceptance as its own provenance,
   not as a measurement. Fix any confirmed conflation with a focused regression.
6. Narrow overbroad live summaries such as quick stop releasing "at any speed"
   or halt clear "never" resuming to the actually tested modes/predecessors,
   rates, specimens and images. Do not derive a universal torque/latency/scale
   guarantee from those trials. Update tests/guides only where behavior changes.

## Verify and finish

Use registered `reads`, `driver_settings`, `io_settings`, `tuning`,
`stop_action`, `move_absolute`, `alarm_clear`, `counter_clear` and `axis_session`
for affected changes; do not repeat unchanged suites merely for a report. Run
ledger/generator checks if modified, prompt integrity and diff checks, and the
appropriate verifier/Doxygen gates from COMMON. Existing correct behavior needs
no artificial code edit. Close the exact packet scope, not unknown firmware.

Delegate source/capture comparison separately from regression review. One lead
merges fact dispositions; the reviewer must challenge the distinction between
drive reports and physical observations. Write packet_1's report and execution
index, update affected original records, then commit and sync.
