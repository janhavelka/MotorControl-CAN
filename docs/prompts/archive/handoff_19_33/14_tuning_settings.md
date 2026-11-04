> Executed handoff, retained for traceability. Do not execute this copy. All 22 packets ran between 2 and 4 November 2026. The [execution table](../../../reports/handoff_19_33/README.md) records what each delivered, each packet report records what remains, and the [backlog](../../../backlog.md) rows H03 and H04 track the physical trials still owed. Relative links were relocated; task wording is retained from `2920479`. T1 and T2 are still owed. The rule under "HIL, tests and handoff" below, not to vary gains or current blindly, still applies to them.

# Packet 14: control, current and tuning groups

Execute this packet fully. Apply [COMMON.md](COMMON.md).

Prerequisite: packet 12, as recorded in this handoff's index. Resume [original prompt 22](../../22_tuning_settings.md). This packet owns the finite `2500..2520:00` assignment, not every future native feature. Unneeded gain/filter qualification must not block independent motion evidence work.

## Establish the current boundary

Read `docs/tuning.md`, `docs/reports/prompt_22.md`, `docs/reports/prompts_22_23_audit.md`, original prompt 22, the ledger and the packet 10 API repairs. Inspect `Tuning.h`, `Settings.h`, `Reads.cpp` and `Settings.cpp`, then the real callers in `test/tuning_test.cpp`, `test/can_bus_owner_test.cpp` and the installed consumer.

All 33 raw read rows already exist. `prepareControlRead`, `prepareTuningRead`, `getControl`, `getTuning` use five finite groups through `DriverContext` and `nextDriver/advanceDriver`. The groups are control `2500/2501`, current/holding `2502..2507`, filter/position `2508..250D`, loop gains `250E..2519`, and EDS-only gains `251A..2520`. Do not duplicate these reads or create another runtime engine.

`prepareControlSettings` already implements explicit changed-word loop selection at 2500, followed by readback of2500 and unchanged 2501. It validates complete candidate/model/reservation/prior evidence, tracks configuration effects and preserves uncertainty. Use packet 10's actual public reservation name and generation rules.

Refresh RS `Tuning`, `ControlSettings`, validation tests and ProbeApp callers; MB85RC variant-dependent invalidation; INA228 complete configuration and freshness; ADS1115 ownership; and FieldCore parameter/reference consumers read-only. Common naming does not transfer ESS numeric domains or sensor recovery/retry policy.

## Reopen the concrete sources

Render original EN86/ZH86 p19–20 and inspect all 33 EDS sections 2500..2520. EN86 p19 ends at 2516; ZH86 p19 ends at 2505, so later rows require p20. EDS entries are U16 RW/PDO-mappable but provide no defaults or accepted ranges. Preserve original bytes and regenerate any corrected citations from the ledger.

Established labels/units are 2500 selector 0 ineffective/1 open/2 closed;2502 peak mA;2503..2506 percent;2507 ms. Loop selector 2500 differs from motion mode 6060. OL86-C has no established encoder-based closed-loop applicability. Selector 0 is not disable, release or stop. The current CL86-C readback 2500=2 and 2501=4000 does not establish the encoder counting convention or a universal setting.

Remaining gates include 2501 representation/scale; current/holding admissible domains and motor limits; 2508 code set; filter coefficient equations; 250A/B threshold scale; 250C time unit; loop-gain domains/scales; and the EDS-only 251A..2520 meanings. A percentage label does not establish 0..100 as the valid domain. The product's maximum current is not a qualified peak-current setting for the installed LC60H2127. Do not infer measured following error from threshold settings.

## Implement only supported contracts

First verify the existing loop mutation through the actual owner, not only the custom fake SDO fixture. Preserve changed-fields-only behavior, no-op with no TX/epoch change, whole-candidate rejection, continuous exclusion, notification before possible TX, retirement before epoch migration and exact complete readback. The unchanged encoder word must remain unchanged. An ACK is register acceptance, not physical loop activation.

For each newly established tuning subgroup, record exact writable domain, units, model/firmware, prerequisites and cross-field constraints before implementing direct typed preparation and checked results. Keep defaults invalid until explicitly selected. Retain per-field acknowledgements and uncertainty after a later failure; do not automatically restore, retry, reset tuning, enable or save. Native software can be completed independently of unperformed physical qualification, but unknown domains cannot be filled with all U16 values.

Current/holding remains exclusively here, not duplicated in packet 12. Preserve the ledger's separate read/write ownership for history and communication records: `1003`, `1005/1014/1017/1200` reads belong to 19, other assigned communication/PDO reads to 38, mutations to their existing 18/38/39 owners. No row disappears into “remaining tuning.”

## HIL, tests and handoff

Use packet 11 diagnostics and COMMON's bounded test/retest rules. Configuration reads outside packet 4's accepted list need their reviewed explicit acquisition path. Changing loop/current/gains may affect torque immediately; acquire the applicable motor/drive values and independent controls before that exact experiment. Do not vary gains or current blindly to diagnose a failed move. Electrical/current measurements, readback and physical activation are separate evidence.

Extend `tuning_test.cpp`, `settings_write_test.cpp`, real `can_bus_owner` tests and ledger/generator guards. Cover model/enum rejection, unknown prior repair, immutable candidate, documented boundaries, partial ACKs, lost reply, failed/mismatched readback, stale generations, cancellation/reboot/deadline and no hidden actions. Delegate source/domain review and adverse timeline tests independently; one implementer owns shared progression and the lead owns ledger integration.

Run focused tuning/settings/read/owner suites, generator/checker tests, installed package and strict Doxygen, plus affected firmware checks. Update Doxygen, guide, coverage and CHANGELOG with exact supported versus pending scope. Record `docs/reports/handoff_19_33/packet_14.md`; commit and sync checked work under COMMON. Report the next concrete evidence needed for each withheld subgroup rather than claiming an unsupported stub completes prompt 22.
