> Executed handoff, retained for traceability. Do not execute this copy. All 22 packets ran between 2 and 4 November 2026. The [execution table](../../../reports/handoff_19_33/README.md) records what each delivered, each packet report records what remains, and the [backlog](../../../backlog.md) rows H03 and H04 track the physical trials still owed. Relative links were relocated; task wording is retained from `2920479`. P4 is still owed. The pre-mutation checklist under "Bounded HIL and verification" below still applies to it.

# Packet 13: I/O configuration and output evidence

Execute this packet fully. Apply [COMMON.md](COMMON.md).

Prerequisite: packet 12, as recorded in this handoff's index. Resume [original prompt 21](../../21_io_settings.md). Complete its independently supported I/O groups and investigate the remaining address/function/output gates. Do not make optional DO/filter qualification a prerequisite for unrelated motion work.

## Read code, corrections and originals

Read `docs/io_settings.md`, `docs/reports/prompts_20_21_audit.md`, original prompt 21 and its report, `docs/audit/LICHUAN_OBJECT_FACTS.md`, the canonical ledger and current bench records. The original report predates implemented DI writes; do not recreate that obsolete blocker.

Inspect `IoSettings.h`, `Settings.h`, `DriverSettings.h`, `Reads.cpp` and `Settings.cpp`. Four fixed read groups already cover 23 rows: input configuration `2400:00..08`, output configuration `:0C..0F`, filters `2401..2407:00`, and command readback `60FE:00..02`. `prepareIoSettings` already writes changed `2400:01..08` fields through `DriverSettingsContext`, `nextDriver`, `noteDriverTransmission`, `advanceDriver` and checked `getIo`. Use the reservation contract delivered by packet 10.

Refresh RS `getIo`, I/O candidate tests and real callers; PCA9555 latch/shadow validity; MCP45HVX1 explicit output controls; ADS1115/INA228 ownership/validity; and FieldCore input projections read-only. Distinguish output-register readback from measured terminal voltage.

Render EN86 p8/p18, ZH86 p8/p19, OP86 p6/p7 and ZH57 p8/p17 alongside EDS `2400`, `2401..2407`, `60FE`. EDS/ZH86 p19 put DO polarity at `:0C` and functions at `:0D/:0E/:0F`; other cited tables shift them down one. Codes 2/3 also exchange meanings. ZH57 labels bits 16/17/18 as DO0/1/2 on p8 and DO1/2/3 on p17. This does not establish an 86C terminal mapping or a fourth output.

## Preserve and finish supported behavior

Verify complete DI candidates retain documented codes 0..13, polarity bits 0..6, unknown high bits from matched prior readback, invalid defaults and exact family binding. Code 0 means **undefined**, not proof an input is disabled or unused. Duplicate codes are not forbidden without source evidence. Changed fields progress in fixed order with per-write acknowledgement and full readback; that host order is not a physically atomic transition.

Keep logical level, configured function, polarity, physical wiring and method prerequisites separate. The current fixture reports functions 1/2/3 on the first three inputs and logical zero observations; unused external wiring does not disable their origin/limit assignments. No startup, recovery or motion-preparation helper may reassign them implicitly.

Use the actual owner/reservation/generation path for DI writes, including queued-work exclusion, retirement, possible-effect notification and current-generation readback. Prove partial configuration and uncertain effects survive terminal failures. Repair confirmed implementation gaps without creating another receiver or settings engine.

For DO settings/output actions, first produce a field-specific evidence decision: applicable model/firmware, exact address/subindex, terminal, function code, polarity bits, value/mask behavior, zero/clear behavior and untouched-bit preservation. Readback at an address does not identify its physical function. Do not write both disputed layouts to discover which works. Keep filters separate: EDS defaults `50,40,100,4000,10,2,16` versus OP86's 2 do not define accepted domains, equations or milliseconds.

When evidence establishes a capability, implement its typed candidate/action with complete prevalidation and the existing lifecycle. An unrestricted U16/U32 accessor is not completion. When it does not, retain the exact observation or manufacturer clarification needed for that field, while closing independent DI work.

## Bounded HIL and verification

COMMON authorizes reviewed physical tests and fault investigation; use packet 11's diagnostics and a single operator. Before a mutation identify the actual connected terminal, electrical observation method, original assignment and possible immediate effects. Protective input reassignment is never a workaround for another failing operation. Test output-command register results and electrical level separately. No unknown-index sweep, guessed output mask, tuning reset or save is part of this packet.

Extend `test/io_settings_test.cpp`, `settings_write_test.cpp` and real `can_bus_owner` tests. Cover every supported code/mask boundary, reserved exclusions, wrong group/model, unsupported terminal, immutable candidate, partial writes, mismatch, stale generations, deadline/cancellation and fault/reboot. Assert exact writes and absence of controlword, save/reset or hidden protective reassignment. Preserve unused-input and method-dependent fixtures without claiming fake signals qualify hardware.

Delegate rendered-source/terminal review separately from negative tests; one implementer owns each mutating sequence. Run focused I/O/settings/read/owner suites, ledger/generator checks, installed consumer and Doxygen/firmware gates. Document each supported electrical claim with fixture/raw evidence and keep unknowns visible. Update coverage, examples and CHANGELOG, report `docs/reports/handoff_19_33/packet_13.md`, then commit and sync under COMMON.
