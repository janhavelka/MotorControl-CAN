# Packet 2 Close finite-position admission and regression gaps

Apply [COMMON](COMMON.md). Prerequisite: [1](1_evidence_contracts.md)'s current
fact dispositions. Originals 31 and 33; no hardware required.

## Inspect

Read original 31/33, `docs/reports/handoff_19_33/packet_21.md`, then actual
`include/MotorControlCAN/profiles/lichuan_c/Position.h`, its implementation,
`examples/common/AxisSession.cpp` and `test/support/MoveRig.h`. Trace
`noteMoveTransmission`, `recheckMove`, `preflight`, `withdrawMove`,
`bothNodesUrgent` and `commandedBase`, including their actual owner calls.
Refresh RS `Position.cpp` relative-basis/witness admission, BME280 partial-setting
evidence and FieldCore reference withdrawal/history. Do not import ESS frames.

## Implement

1. In `test/move_relative_test.cpp`, add the missing two-node cases: urgent stop
   on node1 while node2's parameter or trigger is queued locally, held by the
   adapter, or already handed off. Swap nodes. Verify exact transmitted writes,
   unaffected asynchronous RX, isolation, both retained outcomes and no replay.
   Existing `bothNodesUrgent` stops two observed moves and is not this test.
2. Deliver a fault through a relative operation's real observation cycle,
   rather than calling local `cancelMove(DRIVE_FAULT)`: before acceptance, after
   acceptance and in a partially assembled read cycle. REARM has a controlword
   download, not a read cycle: separately route a copied EMCY through the normal
   application during queued and handed-off rearm. Preserve possible effects
   of an already submitted rearm. Assert observation precedence, the admission
   fence and absence of an unqualified trailing 000F; do not add a fabricated
   rearm read or second receiver to make the test possible.
3. Extend COMMANDED-basis coverage beyond its golden path: stale and wrong-basis
   witnesses, generation change, endpoint/displacement bounds, quantization to
   zero, borrowed witness mutation before handoff, and contradictory observation
   after acceptance. These are synthetic contract tests. The actual specimen
   evidence names ACTUAL; do not enable COMMANDED on that basis.
4. Remove the duplicated admission checks between `AxisSession::recheckMove`
   and `Position.cpp::noteMoveTransmission` only if a small nonmutating checked
   function expresses both uses. Preserve status/detail, atomic publication,
   borrowed lifetimes and both admission-time and physical-handoff validation.
   Keep mutation in the original lifecycle. Do not create a generic executor.
5. Make the RS common-intent vectors in `test/move_common_test.cpp` reproducible:
   retain exact inputs, source revision and a small optional producer/build
   procedure. Normal core builds/tests/install consumers must not require the
   sibling checkout or network. Verify output against the pinned semantics;
   source drift is an explicit audit difference, not silent regenerated truth.

## Verify and finish

Run registered `move_relative`, `move_absolute`, `move_common`, `axis_session`,
`enable_action`, `stop_action`, `alarm_clear`, `counter_clear` and owner/service
tests affected by a fix. Demonstrate each confirmed bug's regression fails
without the fix. A missing test may confirm existing correct behavior.
Run full package/Doxygen checks if exposing a shared helper, then required CI
on the final behavior commit. Update position/owner guidance and original33's
remaining list; physical evidence is unchanged.

One implementer owns Position/AxisSession. Delegate adversarial vectors and
RS provenance separately; a stronger reviewer traces a cancelled/unknown
trigger through the next admission. Report packet_2, commit and sync.
