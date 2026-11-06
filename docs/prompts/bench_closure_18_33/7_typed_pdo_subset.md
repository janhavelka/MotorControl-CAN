# Packet 7 Integrate the evidenced PDO configuration subset

Apply [COMMON](COMMON.md). Prerequisite [6](6_pdo_measurement.md)'s transition
record plus existing fixed TPDO1 evidence. Original18 owns this work. If the
new experiment did not establish a writable field, improve/integrate the existing
confirmed subset and close its missing checks; do not invent a new capability.

## Inspect

Read original18 in full, `include/MotorControlCAN/profiles/lichuan_c/PdoConfiguration.h`,
`src/profiles/lichuan_c/PdoConfiguration.cpp`, its real callers, owner/SDO service,
`test/pdo_configuration_test.cpp`, `pdo_experiment_test.cpp`,
`pdo_observations_test.cpp` and packet6's literal records. Refresh RS finite
settings state and MB85RC/RV3032-C7 partial-effect seams; FieldCore mapping/config
invalidation read-only. Reopen originals for every changed descriptor.

Existing names are `prepareTpdo1Configuration`, `nextPdoConfiguration`,
`notePdoConfigurationTransmission`, `advancePdoConfiguration` and
`getTpdo1Configuration`. Reuse that lifecycle; do not build a second remap engine.

## Implement

1. List the exact confirmed transition(s) to support and validate before any
   writes: binding/profile evidence, prior map/readback provenance, applicable
   NMT/drive state, COB-ID/frame/route collision, direction, object eligibility,
   count/width/total Classical payload, timing fields and deadline. Reject an
   unsupported field before admission. Do not add arbitrary setters for all
   EDS records just because one TPDO transition worked.
2. Replace only unnecessary fixed-baseline restrictions with measured alternatives.
   Keep a small typed request and explicit phases. Preserve existing API callers
   where practical; document necessary changes. If event timing is qualified,
   give it proved units/ranges. Two measured values do not establish a continuous
   domain: support the measured literal alternatives or a separately sourced
   domain, not every U16 value, zero behavior or unmeasured transmission types.
   If only raw acceptance is known, keep it in the
   evidence harness rather than exposing a misleading unit-bearing setting.
3. Route every confirmed step through the current reservation/SDO/owner path.
   Freeze input payload, deadline and generations. Retain last confirmed step,
   first possible external effect and each failed/unknown step. No automatic
   rollback/replay or reuse of an unresolved SDO route after a client abort.
4. Publish the map only after the applicable readback contract. On possible
   partial writes invalidate the affected configuration and prepared work.
   Preserve unrelated node routes. Do not qualify delayed same-ID/same-DLC PDO
   data by stamping it with the new host generation; use prompt17's evidenced
   boundary, or keep samples diagnostic/unqualified.
5. Integrate the supported lifecycle through an actual application caller and
   native example independent of the full CLI. For a confirmed production path,
   the bench caller should use it rather than retain a competing hand-written
   production sequence. Keep genuinely experimental mutations clearly isolated.
6. Extend independent tests at every write/readback step: rejection, abort, held
   or uncertain TX, cancellation, deadline, boot/rebind, wrong generation,
   malformed map/width/count, identifier collision, stale and unrelated PDOs,
   resource pressure and output-on-preparation-failure. Check exact bytes and
   retained effects through the real fake owner, including another active node.

## Verify and finish

Run focused PDO/owner/service/network tests, full native/package/Doxygen gates,
affected S2 build and exact-commit CI. On the bench, replay the now-production
supported finite transition once from a freshly checked baseline, followed by
readback and a short existing-feature regression. A refusal or unknown result
is not a reason to weaken validation or silently use the experimental sender.

One implementer owns configuration progression; delegate failure vectors and
strong independent provenance review. Update Q17, coverage and original18's
report. Its eventual commissioning38 and CLI41 integration remain owned and
pending; do not implement those whole later prompts here or mark original18
fully complete while those explicit obligations remain. Report packet_7,
commit and sync.
