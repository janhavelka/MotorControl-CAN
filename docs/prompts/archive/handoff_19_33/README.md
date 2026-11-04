> Executed handoff, retained for traceability. Do not execute this copy. All 22 packets ran between 2 and 4 November 2026. The [execution table](../../../reports/handoff_19_33/README.md) records what each delivered, each packet report records what remains, and the [backlog](../../../backlog.md) rows H03 and H04 track the physical trials still owed. Relative links were relocated; task wording is retained from `2920479`.

# External coder handoff: prompts 19–22 and 29–33

Paste the **entire numbered file** into the external coder's chat while this
repository is open. It tells the coder to read the shared contract from disk;
you do not need to paste every referenced document. Use your selected Opus 5.5
lead with subagents when exposed by that runtime. No model-specific token or
context-size claim is required. Each file is one bounded assignment with its
own evidence, tests, documentation and commit/sync exit.

This is a continuation, not a new promptset or replacement for prompts 1–50.
Keep `promptset/motorcontrol-can` throughout. The original sequence remains in
`../SEQUENCE.json`. These **22 packets** include necessary repairs already
required by the active audit; they do not authorize the rest of prompts 34–50.
Only W1's prerequisite prompt/graph corrections touch that wider range.

For a range, paste this instruction and name the desired numbers:

> Read docs/prompts/handoff_19_33/README.md and COMMON.md. Execute packets 1 through
> 22 in numerical order among ready work, each in full, using bounded subagents.
> Follow prerequisite evidence and retain exact gates. Implement, test, review,
> document, commit and sync each packet on the recorded execution branch. Resume
> the next packet from its report when context is exhausted; do not redo completed
> work or remove a gate to advance. Physical testing and deliberate finite retests
> are authorized under COMMON. Do not stop after planning.

## Execution files and ownership

| Packet | Deliverable | Prerequisite / original responsibility |
| --- | --- | --- |
| 1 | [Current inventory and prompt repair](1_inventory_and_repairs.md) | Entry audit; W1 |
| 2 | [Reusable owner placement](2_owner_structure.md) | 1; W2 |
| 3 | [TWAI submission and retirement](3_transport.md) | 2; W3 / 27 |
| 4 | [Accepted read policy and typed reads](4_typed_reads.md) | 3; W4 / 19 / 29 |
| 5 | [SDO settlement and drain premise](5_sdo_settlement.md) | 4; W5 / 13 / 14 / 24 |
| 6 | [Urgent owner capacity and interruption](6_urgent_owner.md) | 5; W6 owner half / 24 / 31 |
| 7 | [Usable stop actions](7_stop_lifecycle.md) | 6; W6 action/application half / 31 / 32 |
| 8 | [Measured action stack budget](8_stack_budget.md) | 7; W7 |
| 9 | [Rate and coordinate corrections](9_axis_contract.md) | 8; W8 arithmetic / 10 |
| 10 | [Evidence, settings and state seams](10_operation_contracts.md) | 9; remaining W8 / 17 / 19 / 20 / 30 / 31 |
| 11 | [HIL diagnostics and evidence acquisition](11_hil_diagnostics.md) | 10; 29; preserve delivered W9 S2 work |
| 12 | [Driver and motion settings](12_driver_settings.md) | 10, 11; 20 |
| 13 | [I/O settings and output actions](13_io_settings.md) | 12; 21 |
| 14 | [Control/current/tuning groups](14_tuning_settings.md) | 12; 22 |
| 15 | [State, enable and release](15_enable_release.md) | 10–12; 30 and isolated 29 experiments |
| 16 | [Physical stop qualification](16_stop_qualification.md) | 7, 11, 15's usable artifacts; 31 |
| 17 | [Alarm-clear completion](17_alarm_clear.md) | 15, 16's usable artifacts; 32 fault half |
| 18 | [Counter zero and reference invalidation](18_counter_reference.md) | 12, 17's evidence/history contract; 32 counter half |
| 19 | [Position evidence and finite experiment](19_position_evidence.md) | 11, 15, 16, relevant 18 evidence; 29 / 33 |
| 20 | [Finite absolute position](20_absolute_position.md) | 1–10 closed, 12, 15, 16, 19; 33 absolute half |
| 21 | [Relative position and interruption](21_relative_position.md) | 20, relevant reference and relative evidence; 33 relative half |
| 22 | [Finite physical regression and final handoff](22_hil_regression.md) | Implemented applicable 12–21 artifacts; 19–22 / 29–33 closure review |

Prerequisites mean verified consumed artifacts, not a falsely Complete report.
Unresolved DO addressing, tuning ranges, release or a naturally unavailable fault
must not block unrelated arithmetic, diagnostics or a supported position case.
Carry their gates into reports. Do not claim all original prompts Complete when
only a subset passes. W1–W8's mandatory software repairs closed in packet 10;
missing firmware qualification remains separate.

## Where the repository actually stands

Source map: packet 2 moved the owner, diagnostics and counters to `adapters/owner/`, namespace `MotorControlCANOwner`, target `MotorControlCAN::CanBusOwner`. Any `examples/common/CanBusOwner.*` in these packets or the audit means that baseline; do not recreate the old files. Packet 2 also split transport and request values into `CanBusTypes.h`.

Authoring baseline: **4ae93e3**, 2026-11-02. Refresh it on entry. Read
`docs/reports/prompts_20_21_audit.md`, `prompts_22_23_audit.md`,
`prompts_29_30_audit.md`, `prompts_31_32_audit.md`, and reports 19–22/29–33.
Original reports contain historical blockers: current code already writes
6081/6083/6084, DI 2400:01..08 and loop selector 2500 conditionally. Do not rebuild
those groups from scratch. Other native groups still have exact evidence gates.

No `prepareEnable` or `prepareRelease` exists at this baseline. Stop, alarm clear
and counter zero have conditional implementations. Position has pure
`preparePositionParameters`, not a finite move engine. `MotionPreview` is offline
and always has `executionReady=false`; changing that flag is not implementation.

The [repair audit](../../PROMPTSET_AUDIT_34_50_2026-11-02.md) closed W1–W8 in packet 10.
It was written against older code. Current NMT, S2 and
receive-bound work already resolves parts of it. Reproduce each affected finding
against current source before fixing it. The current firmware task stack is
12,288 bytes; the audit's 8,192-byte figure is historical. Read-acquisition
high-water evidence does not establish action call-chain safety.

## Recorded bench and evidence to reuse

Read `docs/hardware_bench.md` and these reports before touching a port:

- `docs/reports/2026-11-02_s2_live_connection.md`
- `docs/reports/2026-11-02_nmt_pdo_checkpoint.md`
- `docs/reports/2026-11-02_pdo_motion_plan.md`
- `docs/adapters/twai_receive_time.md`

Recorded fixture: ESP32-S2FH4 rev1, 4MB flash/no PSRAM; TJA1051 RX16/TX17/silent18;
UART0 TX43/RX44 at 115200; ESP-Prog2 COM15, USB serial `94A990398B40`, VID/PID
303A:1002. Verify enumeration; COM15 is not a universal default. Automatic
EN/BOOT flashing worked, JTAG is untested. Two user-labelled CL86-C nodes1/2 at
1Mbit/s, motors LC60H2127, free shafts bolted to the table, paper shaft markers
and independent power cutoff are user-provided facts. Termination was reported
at PCB JP1 and last-drive SW8. Do not assume present power/wiring from history.

`docs/bench/20261102T173312Z-s2-upload/build.json` identifies the final tested
image and exact sources. `20261102-final-node1/` and `...node2/` each contain
30 successful readiness uploads. Both reported target 607A=5000, velocity
target 60FF=120, status 6041=0x8037, mode 6061=0, monitor 2100=1, actual 6064/606C=0,
2201=1000, 2500=2, 2501=4000 and DI functions1/2/3. None proves a safe enable,
command/feedback scale, physical standstill or disabled unused inputs.

1009=0x0C and 100A=0x20 were **one-byte** replies against EDS U16 declarations.
1018:00=5 also conflicts with metadata. Preserve raw widths and do not invent a
human firmware version or serial. The fixed node 1 TPDO1 map 3→map 2 trial succeeded;
one later physical power cycle returned producer 1017=0/map 3 without save. This
does not qualify persistence, generic remapping or samples on reused routes.
Do not repeat old experiments merely because an older report says untested.

S2 has conservative receive intervals, not exact arrival timestamps. The
existing runner captures one console byte and raw output; `captured` is not a
semantic PASS verdict. Packet 11 makes it a reusable diagnostic/test path.
`docs/bench/SHA256SUMS.json` protects retained evidence. Originals and failed runs
remain immutable. Authoring this handoff performs no hardware operation.
