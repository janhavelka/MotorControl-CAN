# MotorControl-CAN

**Position parameters:** [checked native preparation](docs/position_parameters.md) now retains exact absolute/relative host intent and validates the documented pulse/rpm/ms ranges. The [native example](examples/position_preview/README.md) performs no CAN I/O. Finite motion remains blocked on qualified acceptance/completion, relative-base and post-trigger stop evidence.

**Explicit fault and counter actions:** [alarm clearing and device zero](docs/fault_reference_actions.md)
now use the same finite action lifecycle and owner. The supported alarm is
documented position deviation `FF05`; device-position requests support zero only.
Command history and coordinate uncertainty survive failures. Broader reset
semantics and physical qualification remain pending under prompt 32.

**Conditional stop path:** [halt and quick-stop actions](docs/stop_actions.md)
now use one documented write followed by fresh controller-report observations
through the existing owner. The supported predecessor is an evidenced `000F`
in position mode. Broader motion-state transitions and physical stop behavior
remain unqualified; [prompt 31](docs/reports/prompt_31.md) stays Partial.

**Drive-state inspection:** [checked documentary interpretation](docs/drive_actions.md)
now separates each manual's status layout, raw monitor values, unknown bits and
disagreements. This does not qualify a firmware dialect or complete enable/release.
Position enabling still needs established initial/retained-motion provenance;
the sources do not specify a release command. [Prompt 30](docs/reports/prompt_30.md)
records the implemented subset and precise remaining work.

**Device evidence readiness:** the [finite acquisition harness](examples/device_evidence/README.md)
uses public typed reads, ProbeApp and the same CAN owner in native and firmware
examples. It retains exact requests/replies, failures, controller events and
capture-loss diagnostics. Startup and cached inspection remain passive. The
[conflict table](docs/device_conflicts.md) specifies measurements for every open
Lichuan gate; ordinary live acquisition remains blocked by unqualified read
effects (Q20). Host fakes and firmware builds pass; physical qualification remains
pending. [Prompt 29's report](docs/reports/prompt_29.md) separates delivered
software from conditional acquisition.

**Connected board intake:** the [ESP-Prog2/S2 inspection](docs/reports/2026-11-02_s2_bench_intake.md)
records USB identity, resolution of duplicate COM15 names, matching design pin
nets and a successful S2 ROM/automatic-boot check. Power/cable checks remain.
The isolated core also compiles for S2 and P4;
complete S2/P4 firmware adapters and physical tests remain pending.

**TWAI transport:** the optional [ESP32-S3 adapter](adapters/twai/README.md)
uses pinned ESP-IDF 5.5.5, copied bounded queues and identifiable TX callbacks.
Host fakes and an SDK compile check cover explicit retirement/recovery without
replaying queued work. It is application code, excluded from the installed core.
The [Arduino and native-IDF examples](docs/firmware_examples.md) now compile the
same application and owner, with explicit configuration, bounded USB I/O and
a separate isolated IDF component consumer. Compile fixtures leave CAN disabled;
no firmware was flashed and physical qualification remains pending.

**Native application:** the [fake native probe](examples/native_probe/README.md)
uses shared ProbeApp/ProbePlatform code, ordinary owner dispatch, explicit typed
identity/configuration reads and retained results. The [units preview](examples/units_preview/README.md)
shows exact offline targets, rejection and preserved output. See the
[application boundary](docs/native_application.md) for bounded I/O and ownership.
No framework, CLI or hardware is required; fake values qualify no device.

**Application owner:** [dispatch and retained results](docs/can_owner.md)
now work over the existing CANopen adapter, with one receive path, bounded
producer scheduling and typed read/settings integration. The
[offline native consumer](examples/owner_preview/README.md) needs no hardware.
Reserved urgent capacity, explicit cancellation/invalidation and finite host
recovery are implemented. Controller queue retirement needs positive adapter
evidence; recovery never reopens uncertain SDO channels or qualifies PDO samples.
The optional TWAI adapter supplies controller evidence; no motor stop is claimed.

**Passive diagnostics:** the application owner now exposes checked cached
`Health`/`DriverState`, saturating `Stats`, resource snapshots and eight bounded
traffic/result history copies. [Health and counter rules](docs/can_owner.md#passive-communication-health)
keep communication, faults and command uncertainty separate. No retry,
automatic recovery or diagnostic-driven admission policy is added.

**Control and tuning:** [five finite groups](docs/tuning.md) cover all 33 native
`2500..2520` reads, with checked typed getters and explicit 86C loop selection.
Other tuning writes retain field-specific evidence gates; prompt 22 is Partial.
No torque/current motion API or hardware qualification is implied.

**Native settings:** [driver parameters](docs/driver_settings.md) and
[DI function/polarity](docs/io_settings.md) now have bounded typed write/readback
paths for explicitly bound CL86-C/OL86-C models, with actual prerequisites,
whole-candidate validation and retained partial/uncertain effects. The
[fresh audit](docs/reports/prompts_20_21_audit.md) corrects the earlier blanket
write blockers. Prompts 20/21 remain Partial: direction/scale/limits/mode and
DO/filter/output-action gates remain explicit. No physical qualification is claimed.

**Typed reads:** finite raw Lichuan identity, state and communication reads are
implemented and host tested. See the [read guide](docs/reads.md) and
[offline example](examples/reads_preview/README.md). Model matching, disputed
state interpretation and physical scaling remain unqualified. Prompt 19 is
Partial: Q20 still blocks a documented non-consuming probe.

**PDO configuration status:** [Prompt 18](docs/reports/prompt_18.md) is blocked
on Lichuan applicability of standard remapping and timing-setting semantics. Passive PDO
layout/readback support is implemented; typed remap/settings APIs are not.

This checkout contains the audit, reference material and implementation prompts for a framework-independent C++11 library for **CAN-controlled stepper and servo drives from multiple manufacturers**. **A minimal native package is implemented:** local status, exact units and quantization, pure axis/reference targets, endpoint identity, Classical CAN frame validation, checked CANopen SDO envelopes and passive heartbeat/boot-up/EMCY codecs. Reviewed Lichuan object metadata is installed separately; no operational drive
profile is implemented. An [optional CANopenNode application adapter](adapters/canopennode/README.md) now implements bounded SDO transfers with retained uncertainty, timestamped late observations and permanent ambiguity quarantine, without controller I/O. It also provides continuous passive network routing, copied event history and synchronous node/cache invalidation; see [Prompt 15](docs/reports/prompt_15.md). Explicit bounded NMT requests, broadcast effects and evidence-based service eligibility now share that owner; [Prompt 16](docs/reports/prompt_16.md) records checks. These are generic CANopen services, without measured Lichuan NMT behavior. [Prompt 17](docs/reports/prompt_17.md) adds checked PDO templates, actual-map evidence and separately qualified passive observations in that same owner, with no implicit remapping or device support claim. [Prompt 14](docs/reports/prompt_14.md) records settlement policy and the remaining reopening evidence gates. The source audit, intake, [Lichuan documentary review](docs/reports/prompt_3.md) and [test-only drive contrasts](docs/reference/drive_contrasts.md) are complete on `promptset/motorcontrol-can`; see [progress](docs/prompts/PROGRESS.md), [source map](docs/source_map.md), [backlog](docs/backlog.md), [roadmap](docs/roadmap.md) and [bench facts](docs/hardware_bench.md). The [reviewed dictionary](docs/reference/lichuan_objects.md) and [decision tables](docs/reference/lichuan_review.md) preserve conflicting sources and precise qualification gates; they grant no device write or implemented support. Start with Classical CANopen and Lichuan CL86-C; CL/OL 86C and 57C models retain separate applicability and firmware evidence. Nanotec N5 and maxon EPOS4 are mandatory design contrasts and future profile candidates.

Prompt 5 supplies a [native service experiment](experiments/canopen_service_fit/README.md) and [selects CANopenNode](docs/decisions/canopen_stack.md) as the single optional application-side CANopen engine. Prompt 6 adds the [buildable API/ownership proof](experiments/api_ownership/README.md): portable contracts, typed synthetic read/write flows, fixed owner storage and stepper/servo/protocol contrasts. Read the [frozen semantic boundary](docs/architecture.md) and [prompt 6 report](docs/reports/prompt_6.md). The [native package guide](docs/getting_started.md) documents source/install/export use and the registered proof tests. Synthetic operations and N5/EPOS4 fixtures remain test-only. Prompt 7 packages the minimal common core. [Quick/full verification](docs/verification.md), [contributor guidance](CONTRIBUTING.md) and the [security policy](SECURITY.md) now accompany it; prompt 8's verified gates and CI results are recorded in [its report](docs/reports/prompt_8.md).

The [status and exact-units guide](docs/units_status.md) documents the implemented rational conversions, rounding and explicit radians/binary32 quantization. The [axis guide](docs/axis_contract.md) documents production configuration, independent origins, reference epochs/freshness and checked position/velocity targets. The [offline preview](examples/units_preview/README.md) runs illustrative servo and float-native cases without a controller. The [CANopen service guide](docs/canopen_services.md) documents installed pure SDO checks, signed little-endian reads, full abort evidence and worked bytes around the selected application-side engine. The optional application adapter now supplies production transfer sessions and passive network evidence; channel reopening remains blocked by the source qualifications recorded in prompt 14. The [reviewed object guide](docs/reference/lichuan_objects.md)
describes generated compact declarations, optional catalogue strings and
[generated accounting](docs/reference/lichuan_generated.md). Descriptors preserve
qualification gates and implement no semantic function.

The design follows `../MotorControl-RS`: the same motion vocabulary, exact units, caller-owned operation contexts, application-owned transport, shared example CLI, and native/Python verification. CANopen and proprietary CAN protocols have independent services/profiles beneath common motion. FieldCore-node is a reference consumer, not a dependency. Read [the stepper and servo scope decision](docs/multidrive_decision.md) for the researched candidates, boundaries and implementation order.

Start with [the 50 numbered prompts](docs/prompts/README.md), beginning at [prompt 1](docs/prompts/1_source_audit.md). [START_HERE.md](docs/prompts/START_HERE.md) provides copyable instructions for one prompt, a range or the whole sequence. Complete each ready prompt with tests, documentation and commit/sync. Keep blocked scope explicit, continue independent assigned work and return when evidence arrives; Partial is not Complete.

All 50 prompts include concrete files/APIs, failure cases, documentation duties and work splits. Shared contracts fix [names/status/health/ownership](docs/prompts/CONVENTIONS.md), require [documentation and Doxygen throughout](docs/prompts/DOCUMENTATION.md), define [simple maintainable code](docs/prompts/MAINTAINABILITY.md), and authorize [subagents with task-based model selection](docs/prompts/DELEGATION.md). The [fresh conventions audit](docs/audit/CONVENTIONS_REAUDIT.md) records the actual source behind those choices.

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
