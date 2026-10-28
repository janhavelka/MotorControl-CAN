> Historical v1 promptset (00-19), retained for traceability. Do not execute this copy. Use the [current prompt index](../../README.md). Relative documentation links were relocated; task wording is retained from `dc8270740d0e16f19e170e2a982f67389c8a2c56`.

# Stage 01 Reconcile the dictionary and family scope

Apply [COMMON.md](COMMON.md). Prerequisite: stage 00.

## Audit

Read MotorControl-RS generated Registers/Types/Access policy and its single register ledger/generator/checker; compare MB85RC variant policy, PCA9555 masked registers and MCP45HVX1 value/effect distinctions. Inspect native callers, generator tests and CI freshness checks. Read original EN86, ZH86, OP86, ZH57, OP57 and EDS, using the source IDs and rendered conflicts in the protocol research.

## Deliver

1. Create a machine-readable reviewed object ledger, suggested `docs/reference/lichuan_objects.json`, and a readable dictionary/feature matrix. Account for all 99 EDS indexes, subindexes, manual-only entries and each semantic function. Fields: model/dialect/firmware applicability, index/subindex, type/width/signedness, access, min/max/default, units, scaling, mapping eligibility, volatile/persistent effect, prerequisite state, trigger/edge semantics, source ID/page/EDS section, confidence, owner stage, descriptor/read/setting/action/workflow coverage and implementation/test/CLI/HIL status.
2. Preserve raw EDS alongside the reviewed ledger. Classify disagreements individually: resolved by authoritative evidence, conservatively read-only, unavailable pending firmware qualification, or unsupported by the actual profile. Do not silently prefer EDS, English, Chinese, newer file timestamp or generic CiA402.
3. Explicitly track status bits 3/7/8/10/12/14/15, velocity start order, velocity signedness, actual-position scale, 6083/6084/609A millisecond ramp times, 2201 subdivision, 2202 soft-limit discrepancy, 2300 save/reset, 2301 alarm clear, 2302 position-clear edge, output-function encodings, 2400 reserved/filter fields, and tuning entries with missing semantics.
4. Resolve core services: addressed-node range versus broadcast, DIP-selected/parameter node IDs, bitrates including the EDS-only 50 kbit/s claim, default SDO/PDO COB-IDs and possible configured variants, NMT/heartbeat/EMCY/SYNC support, transfer sizes and PDO limits. Record 1017 default zero and absent 1016 evidence; do not invent host-loss stop.
5. Define model/capability entries for CL86-C, OL86-C, CL57-C, OL57-C. Distinguish encoder feedback/closed-loop semantics. EDS generic identity values are insufficient to auto-detect all models. Unknown hardware identities remain explicit; allow a user-selected reviewed binding with provenance, not guessed auto-match.
6. Write a conflict-resolution/qualification checklist mapping every open item to the exact feature it blocks and the required documentary or bench observation. Continue implementing noncontroversial reads/core infrastructure.
7. Apply `MULTIDRIVE.md`: create a source-backed N5 stepper/BLDC and EPOS4 servo contrast ledger before freezing common types. Compare actual command/feedback units, ramp meanings, modes, state/stop/watchdog and native widths; do not turn contrast evidence into Lichuan constants or a claim that a second profile is implemented. Include N5's eight versus Lichuan's four PDO sets and the different meanings of vendor index 0x2300. No generic action-by-object-number or global four-PDO limit is acceptable.

## Concrete implementation contract

Use one reviewed machine-readable ledger as the source for generated object declarations and coverage reports. Keep semantic operation implementation hand-written; the generator must not infer a control sequence from object numbers. Suggested row shape:

```json
{
  "key": "lichuan_c.profile_acceleration_time",
  "index": "0x6083",
  "subindex": 0,
  "units": "ms",
  "dataType": null,
  "access": null,
  "models": [],
  "firmwareScope": "unresolved",
  "sources": [],
  "conflicts": [],
  "ownerStage": "10",
  "disposition": "pending_evidence"
}
```

This is a schema illustration, not a completed device descriptor. Fill type/access/limits/source pages from the originals; `null` means unknown, never a writable default. Every EDS index/subindex and manual-only entry must map to a reviewed row or an explicit exclusion with reason. Arrays, records, reserved entries and gaps must remain distinguishable. Count 99 EDS indexes as an accounting check, not as 99 implemented functions.

The generated header belongs under `include/MotorControlCAN/profiles/lichuan_c/`; retain the selected public namespace `MotorControlCAN::LICHUAN_C`. Keep protocol-standard object descriptions separate from manufacturer meanings. For example, Lichuan `0x2300` save/reset must never be reused as Nanotec's meaning for the same number.

For each disputed control/status field provide a small decision table: source, exact page/section, meaning, model/firmware scope, consequence, chosen policy and evidence still needed. A conservative omission is a feature gate, not a resolved contradiction. For native operations record both transport access and actual effect: readable, writable, triggers action, clears state, persists, or requires restart. A read/write EDS flag alone is insufficient to classify the effect.

## Documentation and work split

Apply [DOCUMENTATION.md](DOCUMENTATION.md) and [MAINTAINABILITY.md](MAINTAINABILITY.md). Publish the readable object dictionary, family coverage, conflict list and generator instructions together. Generated documentation must identify its source file and generation command; do not manually patch generated output.

Under [DELEGATION.md](DELEGATION.md), one reviewer can reconcile EN/ZH manuals while another checks EDS coverage. The lead merges the ledger serially. Use a strong reasoning reviewer for conflicting status/trigger/stop semantics; a smaller model may check links and row accounting after the schema is fixed.

## Verify and exit

Validate ledger schema, uniqueness, widths, bounds, source links and complete EDS/manual accounting. Manually inspect representative PDF table rows and both contradictory status diagrams. No generated constant may be silently supported without evidence. A factory reset/save is not an evidence-gathering action.

Exit with a usable reviewed ledger and bounded uncertainty map. Do not claim full firmware support while the dialect is unresolved.
