# MotorControl-CAN documentation

Execution has completed prompts 1-15, including the [native service fit experiment](reports/prompt_5.md) and [common API/ownership proof](reports/prompt_6.md), on the recorded execution branch. The [minimal native package](getting_started.md) provides status, exact units/quantization, pure axis/reference targets, endpoint, Classical frame validation and pure checked CANopen SDO/network envelopes. Reviewed Lichuan descriptors are available as metadata; no operational drive profile exists yet. The [verification guide](verification.md) documents working local/CI gates; gate results are recorded in the [prompt 8 report](reports/prompt_8.md), with production API results in the [prompt 9 report](reports/prompt_9.md), [axis report](reports/prompt_10.md) and [CAN/SDO report](reports/prompt_11.md). Read [the roadmap](roadmap.md), [progress](prompts/PROGRESS.md), [promptset index](prompts/README.md) and [copyable assignment](prompts/START_HERE.md) before the next assignment. The pasted former Stage 00 is archived; its source audit and original-byte checks are recorded in prompt 1.

| Material | Purpose |
| --- | --- |
| [Status and exact units](units_status.md) | Implemented rational parsing, dimensional conversion, rounding, explicit radians/float quantization and unchanged failure outputs |
| [Axis coordinates and references](axis_contract.md) | Production pure configuration, origins, generation/freshness, position/velocity targets and offline preview |
| [CANopen services](canopen_services.md) | Installed pure SDO checks, channel/width correlation, signed words, abort/segmented evidence and worked vectors; bounded production lifecycle, timestamped observations and ambiguity settlement, passive node events and synchronous cache invalidation in the optional application owner |
| [Reviewed object generation](reference/lichuan_objects.md) and [generated coverage](reference/lichuan_generated.md) | Compact profile-scoped declarations, optional strings, withholding/exclusions, semantic coverage and reproducible freshness command |
| [Live source map](source_map.md) | Actual declarations, implementations, callers, tests and deliberate CAN differences |
| [Verification](verification.md), [contributing](../CONTRIBUTING.md), [security](../SECURITY.md) | Working commands, required tools, retained evidence, current versus planned checks and development reporting limits |
| [Executable backlog](backlog.md) and [roadmap](roadmap.md) | Separate documentary gates, software, integration and physical qualification with current owners |
| [Requirement coverage](coverage.md) | All 35 requirements tracked without turning audit evidence into implemented support |
| [CAN bench facts](hardware_bench.md) | Known documentary facts and missing actual fixture/authorization details |
| [Execution report](reports/prompt_1.md) and [source/tool inputs](reports/prompt_1_inputs.json) | Fresh revisions, scope, deltas, verification and handoff |
| [Stepper and servo scope decision](multidrive_decision.md) | One multi-vendor library; CANopen first and protocol-specific extensions |
| [CANopen drive survey](audit/CANOPEN_DRIVE_SURVEY.md) | Concrete stepper/servo candidates, manuals and recommended order |
| [Other CAN protocol survey](audit/OTHER_CAN_PROTOCOLS.md) | ODrive, moteus and VESC differences that constrain the common boundary |
| [Fresh multi-drive source audit](audit/MULTIDRIVE_SOURCE_REAUDIT.md) | Actual RS/I2C/FieldCore type and ownership assumptions to preserve or remove |
| [Audit summary](audit/SUMMARY.md) | Main findings and consequences |
| [MotorControl-RS audit](audit/MOTORCONTROL_RS_AUDIT.md) | Actual architecture, API, CLI, ownership, tests and package behavior |
| [I2C/FieldCore audit](audit/ECOSYSTEM_CONVENTIONS.md) | Current conventions, intentional differences and real consumer boundary |
| [Protocol research](audit/DEVICE_PROTOCOL_RESEARCH.md) | Manufacturer objects, family scope and contradictory behavior |
| [Original reference pack](reference/README.md) | Five manuals, EDS, source pages and hashed extracts |
| [Lichuan dictionary schema](reference/lichuan_objects.md) and [reviewed decisions](reference/lichuan_review.md) | Complete documentary policy, original claims, four model scopes, qualification checklist and local validation |
| [Drive contrasts and test cases](reference/drive_contrasts.md) | Source-backed N5 stepper/BLDC, EPOS4 servo and proprietary-frame boundaries for later production-code proofs; no installed support |
| [Source baseline](baseline/README.md) | 44 selected unmodified source/reference files with MIT notice and hashes |
| [Architecture plan](architecture_plan.md) | Proposed file layout, responsibilities and CAN service boundary |
| [Architecture](architecture.md), [axis](axis_contract.md), [profile](profile_contract.md), [services](canopen_services.md) and [owner](can_owner.md) | Concrete prompt-6 semantic contracts, fixed ownership table and prototype migration limits |
| [Native ownership proof](../experiments/api_ownership/README.md) | Build all three native tests; portable contracts, actual selected client and test-only drive contrasts |
| [CANopen stack decision](decisions/canopen_stack.md) and [prompt 5 report](reports/prompt_5.md) | Selected CANopenNode application adapter, actual native proof, ownership/resource/retirement evidence and remaining production contracts |
| [Native fit experiment](../experiments/canopen_service_fit/README.md) | Offline C99/C++11 build and executable checks; no SDK, installed driver or hardware support claim |
| [API/CLI mapping](api_cli_mapping.md) | Literal RS names to preserve and CAN-specific differences |
| [Requirements matrix](requirements_matrix.md) | User requirements mapped to implementation and acceptance stages |
| [Implementation progress](prompts/PROGRESS.md) | Current execution branch/base and each prompt's actual status |
| [Concrete conventions](prompts/CONVENTIONS.md) and [fresh source audit](audit/CONVENTIONS_REAUDIT.md) | Exact selected API/status/owner/health/counter names with observed versus proposed distinctions |
| [Documentation](prompts/DOCUMENTATION.md) and [maintainability](prompts/MAINTAINABILITY.md) | Stage-owned general docs, Doxygen, readable direct code and strict checks |
| [Delegation and model selection](prompts/DELEGATION.md) | Authorized bounded subagents, available-model selection by complexity and integrated review |
| [Multi-drive stage criteria](prompts/MULTIDRIVE.md) and [new-drive playbook](prompts/ADD_DRIVE.md) | Concrete extensibility gates and future implementation workflow |
| [RS verification evidence](audit/evidence/motorcontrol_rs_quick/README.md) | Retained passing quick-verifier summary and 15 logs |

Planning integrity check: `python scripts/check_promptset.py`. Add `--check-live-baseline` to compare selected copies against the current sibling checkout; differences after future source evolution are expected audit inputs. The default check is offline and does not run firmware or hardware tests.
