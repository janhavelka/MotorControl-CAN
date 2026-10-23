# MotorControl-CAN planning documents

Start implementation with [the promptset index](prompts/README.md) and [copyable assignment](prompts/START_HERE.md). The current checkout is the planning/reference deliverable; no CAN driver is implemented yet.

| Material | Purpose |
| --- | --- |
| [Stepper and servo scope decision](multidrive_decision.md) | One multi-vendor library; CANopen first and protocol-specific extensions |
| [CANopen drive survey](audit/CANOPEN_DRIVE_SURVEY.md) | Concrete stepper/servo candidates, manuals and recommended order |
| [Other CAN protocol survey](audit/OTHER_CAN_PROTOCOLS.md) | ODrive, moteus and VESC differences that constrain the common boundary |
| [Fresh multi-drive source audit](audit/MULTIDRIVE_SOURCE_REAUDIT.md) | Actual RS/I2C/FieldCore type and ownership assumptions to preserve or remove |
| [Audit summary](audit/SUMMARY.md) | Main findings and consequences |
| [MotorControl-RS audit](audit/MOTORCONTROL_RS_AUDIT.md) | Actual architecture, API, CLI, ownership, tests and package behavior |
| [I2C/FieldCore audit](audit/ECOSYSTEM_CONVENTIONS.md) | Current conventions, intentional differences and real consumer boundary |
| [Protocol research](audit/DEVICE_PROTOCOL_RESEARCH.md) | Manufacturer objects, family scope and contradictory behavior |
| [Original reference pack](reference/README.md) | Five manuals, EDS, source pages and hashed extracts |
| [Source baseline](baseline/README.md) | 44 selected unmodified source/reference files with MIT notice and hashes |
| [Architecture plan](architecture_plan.md) | Proposed file layout, responsibilities and CAN service boundary |
| [API/CLI mapping](api_cli_mapping.md) | Literal RS names to preserve and CAN-specific differences |
| [Requirements matrix](requirements_matrix.md) | User requirements mapped to implementation and acceptance stages |
| [Implementation progress](prompts/PROGRESS.md) | Initially pending; future stages record actual status |
| [Concrete conventions](prompts/CONVENTIONS.md) and [fresh source audit](audit/CONVENTIONS_REAUDIT.md) | Exact selected API/status/owner/health/counter names with observed versus proposed distinctions |
| [Documentation](prompts/DOCUMENTATION.md) and [maintainability](prompts/MAINTAINABILITY.md) | Stage-owned general docs, Doxygen, readable direct code and strict checks |
| [Delegation and model selection](prompts/DELEGATION.md) | Authorized bounded subagents, available-model selection by complexity and integrated review |
| [Multi-drive stage criteria](prompts/MULTIDRIVE.md) and [new-drive playbook](prompts/ADD_DRIVE.md) | Concrete extensibility gates and future implementation workflow |
| [RS verification evidence](audit/evidence/motorcontrol_rs_quick/README.md) | Retained passing quick-verifier summary and 15 logs |

Planning integrity check: `python scripts/check_promptset.py`. Add `--check-live-baseline` to compare selected copies against the current sibling checkout; differences after future source evolution are expected audit inputs. The default check is offline and does not run firmware or hardware tests.
