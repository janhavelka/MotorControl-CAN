# MotorControl-CAN planning and implementation prompts

This checkout contains the audit, reference material and implementation prompts for a framework-independent C++11 library for **CAN-controlled stepper and servo drives from multiple manufacturers**. **The library has not been implemented.** Start with Classical CANopen and Lichuan CL86-C; related CL/OL 86C and 57C models require explicit capability and firmware evidence. Nanotec N5 and maxon EPOS4 are mandatory design contrasts and future profile candidates.

The design follows `../MotorControl-RS`: the same motion vocabulary, exact units, caller-owned operation contexts, application-owned transport, shared example CLI, and native/Python verification. CANopen and proprietary CAN protocols have independent services/profiles beneath common motion. FieldCore-node is a reference consumer, not a dependency. Read [the stepper and servo scope decision](docs/multidrive_decision.md) for the researched candidates, boundaries and implementation order.

Start with [the promptset](docs/prompts/README.md). To give the whole assignment to a coding agent, paste [START_HERE.md](docs/prompts/START_HERE.md). For separate sessions, execute the numbered prompts in order and carry forward their reports.

All 20 stages include concrete files/APIs, failure cases, documentation duties and work splits. Shared contracts fix [names/status/health/ownership](docs/prompts/CONVENTIONS.md), require [documentation and Doxygen throughout](docs/prompts/DOCUMENTATION.md), define [simple maintainable code](docs/prompts/MAINTAINABILITY.md), and authorize [subagents with task-based model selection](docs/prompts/DELEGATION.md). The [fresh conventions audit](docs/audit/CONVENTIONS_REAUDIT.md) records the actual source behind those choices.

- [MotorControl-RS audit](docs/audit/MOTORCONTROL_RS_AUDIT.md)
- [Prompt correctness and simplicity review, 24 October](docs/audit/PROMPTSET_REVIEW_2026-10-24.md)
- [Fresh multi-drive source audit](docs/audit/MULTIDRIVE_SOURCE_REAUDIT.md)
- [CANopen drive survey](docs/audit/CANOPEN_DRIVE_SURVEY.md) and [other CAN protocols](docs/audit/OTHER_CAN_PROTOCOLS.md)
- [I2C and FieldCore conventions](docs/audit/ECOSYSTEM_CONVENTIONS.md)
- [Device and protocol research](docs/audit/DEVICE_PROTOCOL_RESEARCH.md)
- [Downloaded references and provenance](docs/reference/README.md)
- [Proposed structure and ownership](docs/architecture_plan.md)
- [API and CLI naming map](docs/api_cli_mapping.md)
- [Requirements and acceptance gates](docs/requirements_matrix.md)
- [Copied source baseline](docs/baseline/README.md)

Initial audit date: **23 October 2026**; prompt correctness review: **24 October 2026**. Audits identify the exact sibling revisions inspected. Every implementation stage must refresh the relevant source audit; the snapshots are reproducible reference material, not a replacement for current code.

Validate this planning package locally with `python scripts/check_promptset.py`. Add `--check-git-ref HEAD` to verify committed artifact bytes or `--check-git-ref INDEX` to check staged artifact bytes before committing. That validates the prompt/reference package only; it does not test a CAN library or qualify hardware.

Original vendor documents retain their owners' rights. Copied MotorControl-RS files retain their MIT notice. No existing motor bench authorization, COM port, pin assignment or motor limits are transferred to this new CAN setup.
