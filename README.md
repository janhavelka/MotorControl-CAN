# MotorControl-CAN

A framework-independent C++11 library for **CAN-controlled stepper and servo drives from multiple manufacturers**. Common motion is motor- and protocol-neutral. CANopen/CiA402 is the first protocol backend and the Lichuan CL86-C the first target; CL/OL 86C and 57C models keep separate applicability and firmware evidence. Nanotec N5 and maxon EPOS4 are design contrasts and future profile candidates, not supported drives. Read [the stepper and servo scope decision](docs/multidrive_decision.md).

The design follows `../MotorControl-RS`: the same motion vocabulary, exact units, caller-owned operation contexts, application-owned transport, a shared example CLI and native/Python verification. FieldCore-node is a reference consumer, not a dependency.

## Current status

[PROGRESS.md](docs/prompts/PROGRESS.md) is the authoritative per-prompt state; this summary links its evidence. **Finite motion is not qualified: the library's gated absolute and relative move lifecycles have run on two specimens of one firmware, judged from the drives' own reports, and no prompt-level hardware qualification exists.**

- **Implemented and host tested.** Status and [exact units](docs/units_status.md); [axis targets and references](docs/axis_contract.md); [CAN frames and SDO/network codecs](docs/canopen_services.md); [generated object metadata](docs/reference/lichuan_objects.md); the bounded SDO, NMT and PDO adapter over [CANopenNode](adapters/canopennode/README.md); [typed reads](docs/reads.md); [driver](docs/driver_settings.md), [I/O](docs/io_settings.md) and [tuning](docs/tuning.md) settings subsets; the [application CAN owner](docs/can_owner.md); the [TWAI adapter](adapters/twai/README.md); [shared native and firmware examples](docs/firmware_examples.md); [state interpretation and a gated enable](docs/drive_actions.md); conditional [stop](docs/stop_actions.md), [alarm-clear and counter-zero](docs/fault_reference_actions.md) actions; and pure [position parameter preparation](docs/position_parameters.md) with gated absolute and relative move lifecycles (`prepareMoveAbsolute`, `prepareMoveRelative`, `AxisSession::requestMove` and `requestMoveRelative`). Of these, the enable, the moves, the stops, the counter zero and the `2202` and DI writes have run on specimens (below); the alarm clear has not.
- **Observed on one bench, no qualification.** Through the [S2 bench](examples/s2_bench/README.md), two user-labelled CL86-C specimens answered explicit uploads on nodes 1 and 2 at 1 Mbit/s ([connection](docs/reports/2026-11-02_s2_live_connection.md), [NMT and TPDO checkpoint](docs/reports/2026-11-02_nmt_pdo_checkpoint.md)). Node 1 accepted a fixed TPDO1 map change, which a later power cycle reverted in that one cycle. Both report mode 0 with `607A=5000` and `60FF=120`, values equal to the EDS defaults. Handoff packet 11 then read typed identity and the 30-field readiness image live on both, and ran a read-only communication soak ([packet 11](docs/reports/handoff_19_33/packet_11.md)). An [attended session](docs/reports/2026-11-03_attended_bench.md) then enabled both through `AxisSession::requestEnable`, after a fix found there. On node 1 it ran one application-local absolute move (+250 pulses, about a quarter turn counter-clockwise), its rearm, and a HALT during a 2 rpm move, after which the drive reported a stop and `6064` stayed 316 for 26 s (the attendant was not sure about the shaft). An [unattended session](docs/reports/2026-11-03_unattended_bench.md), run on the user's written waiver with nobody at the bench, then ran the library's own move lifecycle for the first time through `AxisSession::requestMove` on node 2 (trial 121: 0 to 250 pulses, `SUCCEEDED` with its rearm), and a stationary `000B` on each node, after which each drive reported low bits `000` and released (`2100` bit 4). The library ended both stops outside its observation scope; it now completes a quick stop on that report, retested on hardware (trial 208). No start, reset, save or restore was sent in either session. Sessions on [2026-11-04](docs/reports/2026-11-04_bench.md) and [2026-11-05](docs/reports/2026-11-05_motion_and_stress.md), judged from the drives' reports, then ran library absolute moves (213 on node 2; 413, 705 and 706 on node 1) and relative moves (411, 708, node 1), halts (207, 406, 709), quick stops (208, 213, 413, 503, 713), the counter zero (323), `2202` codes 1 and 0 with readback on both nodes (604 to 607) and DI3 writes on node 1 (319 to 321). These observations do not establish harmless reads, a safe enable, fresh PDO samples or any other firmware.
- **Repair audit closed.** The [repair audit](docs/prompts/PROMPTSET_AUDIT_34_50_2026-11-02.md)'s W1 to W8 are done on the host; the drive evidence for stops and moves comes from the 2026-11-03, -04 and -05 bench sessions above. All 22 [handoff packets](docs/prompts/archive/handoff_19_33/README.md) have run and are archived; [their table](docs/reports/handoff_19_33/README.md) records what each delivered, and the [backlog](docs/backlog.md) tracks the physical trials still owed.
- **Not implemented.** Release, velocity, homing, discovery, commissioning, persistence, CLI and Python runner. Open device gates are in the [conflict table](docs/device_conflicts.md).

## Start here

- Use the library: the [native package guide](docs/getting_started.md), [application boundary](docs/native_application.md) and [offline examples](examples/owner_preview/README.md). Examples and firmware bring their own CAN controller and time; the installed core performs no I/O.
- Check it: [quick and full verification](docs/verification.md), [contributor guidance](CONTRIBUTING.md) and the [security policy](SECURITY.md).
- Work on it: [the 50 numbered prompts](docs/prompts/README.md), [START_HERE.md](docs/prompts/START_HERE.md), [source map](docs/source_map.md), [backlog](docs/backlog.md), [roadmap](docs/roadmap.md) and [bench facts](docs/hardware_bench.md). Decisions are recorded in the [frozen semantic boundary](docs/architecture.md) and the [CANopen stack decision](docs/decisions/canopen_stack.md).

## Audits and references

- [MotorControl-RS audit](docs/audit/MOTORCONTROL_RS_AUDIT.md)
- [Prompt correctness and simplicity review, 24 October](docs/audit/PROMPTSET_REVIEW_2026-10-24.md)
- [Source and execution corrections, 28 October](docs/audit/PROMPTSET_REVIEW_2026-10-28.md): evidence dependency, stop/reset interpretation, output labels and PDO provenance.
- [Prompt length and workload review](docs/audit/PROMPT_SIZING_2026-10-24.md) and [complete scope migration](docs/prompts/SPLIT_MAP.md)
- [Fresh multi-drive source audit](docs/audit/MULTIDRIVE_SOURCE_REAUDIT.md)
- [CANopen drive survey](docs/audit/CANOPEN_DRIVE_SURVEY.md) and [other CAN protocols](docs/audit/OTHER_CAN_PROTOCOLS.md)
- [I2C and FieldCore conventions](docs/audit/ECOSYSTEM_CONVENTIONS.md)
- [Device and protocol research](docs/audit/DEVICE_PROTOCOL_RESEARCH.md)
- [Concrete Lichuan evidence for each prompt](docs/prompts/DEVICE_EVIDENCE.md): complete EDS inventory, motion/settings/timing tables and audited public code, with unresolved manufacturer contradictions kept explicit.
- [Evidence enrichment review, 24 October](docs/audit/LICHUAN_ENRICHMENT_2026-10-24.md): findings, prompt-size measurements and validation scope.
- [Downloaded references and provenance](docs/reference/README.md)
- [Proposed structure and ownership](docs/architecture_plan.md)
- [API and CLI naming map](docs/api_cli_mapping.md)
- [Requirements and acceptance gates](docs/requirements_matrix.md)
- [Copied source baseline](docs/baseline/README.md)

Initial audit date: **23 October 2026**; latest prompt/source correction review: **28 October 2026**. Audits identify the exact sibling revisions inspected. Every implementation stage must refresh the relevant source audit; the snapshots are reproducible reference material, not a replacement for current code.

Validate this package locally with `python scripts/check_promptset.py`. Add `--check-git-ref HEAD` to verify committed artifact bytes or `--check-git-ref INDEX` to check staged artifact bytes before committing. This includes the experiment's pinned source inventory, but does not compile it. Use the [ownership proof CMake/CTest commands](experiments/api_ownership/README.md) to run all three native tests, including the service fit experiment; neither check qualifies hardware.

Original vendor documents retain their owners' rights. Copied MotorControl-RS files retain their MIT notice. The isolated CANopenNode build subset retains its Apache-2.0 license and notices. No existing motor bench authorization, COM port, pin assignment or motor limits are transferred to this new CAN setup.
