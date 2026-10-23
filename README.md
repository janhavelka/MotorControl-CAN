# MotorControl-CAN planning and implementation prompts

This checkout contains the audit, reference material and implementation prompts for a framework-independent C++11 library for Lichuan CANopen stepper drives. **The library has not been implemented.** The first target is CL86-C; related CL/OL 86C and 57C models require explicit capability and firmware evidence.

The design follows `../MotorControl-RS`: the same motion vocabulary, exact units, caller-owned operation contexts, application-owned transport, shared example CLI, and native/Python verification. CANopen has its own service and event handling. FieldCore-node is a reference consumer, not a dependency.

Start with [the promptset](docs/prompts/README.md). To give the whole assignment to a coding agent, paste [START_HERE.md](docs/prompts/START_HERE.md). For separate sessions, execute the numbered prompts in order and carry forward their reports.

- [MotorControl-RS audit](docs/audit/MOTORCONTROL_RS_AUDIT.md)
- [I2C and FieldCore conventions](docs/audit/ECOSYSTEM_CONVENTIONS.md)
- [Device and protocol research](docs/audit/DEVICE_PROTOCOL_RESEARCH.md)
- [Downloaded references and provenance](docs/reference/README.md)
- [Proposed structure and ownership](docs/architecture_plan.md)
- [API and CLI naming map](docs/api_cli_mapping.md)
- [Requirements and acceptance gates](docs/requirements_matrix.md)
- [Copied source baseline](docs/baseline/README.md)

Audit date: **23 October 2026**. Audits identify the exact sibling revisions inspected. Every implementation stage must refresh the relevant source audit; the snapshots are reproducible reference material, not a replacement for current code.

Validate this planning package locally with `python scripts/check_promptset.py`. That validates the prompt/reference package only; it does not test a CAN library or qualify hardware.

Original vendor documents retain their owners' rights. Copied MotorControl-RS files retain their MIT notice. No existing motor bench authorization, COM port, pin assignment or motor limits are transferred to this new CAN setup.
