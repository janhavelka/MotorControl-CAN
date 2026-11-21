# MotorControl-CAN documentation

Start with the [README](../README.md) for a self-contained native install,
include/link example and a compiled read/move/stop simulation. The
[0.1.0 candidate notes](releases/0.1.0.md) distinguish software scope, firmware
builds and physical evidence, and explain the descriptor migration.
The [final software-candidate handoff](reports/prompt_50.md) records the independent
review, corrections, verification and inherited work without requiring this chat.
The [fresh prompts50–51 audit](reports/audit_50_51.md) adds owner/SYNC corrections,
adversarial regressions and the latest bounded physical check.

## User contracts

The [prompt 54 campaign](reports/prompt_54.md) records current extension
measurements, diagnostic fixes and the exact remaining motion/fixture gates.

| Topic | Maintained guide |
| --- | --- |
| Native use and example application | [Package use](getting_started.md), [shared application](native_application.md) |
| Exact intent and references | [Units/status](units_status.md), [axis contract](axis_contract.md), [speed and ramp units](speed_units.md) |
| Typed native observations | [Reads](reads.md), [driver settings](driver_settings.md), [I/O](io_settings.md), [tuning](tuning.md) |
| Explicit actions | [Drive state/enable](drive_actions.md), [stop](stop_actions.md), [fault/counter clear](fault_reference_actions.md) |
| Conditional finite motion | [Position and angle](position_parameters.md), [velocity](velocity.md), [homing](homing.md) |
| Discovery, changes and persistence | [Commissioning](commissioning.md) |
| CANopen services and application ownership | [Services](canopen_services.md), [owner/capacity/health](can_owner.md), [communication practice](communication_practice.md), [TWAI adapter](../adapters/twai/README.md) |
| Finite host synchronization | [SYNC producer and conditional TPDO1 type](sync.md); Q27/Q28 and RPDO-map gates remain open |
| Concurrent axes | [Independent/group stop, capacity and storage](multi_axis.md) |
| Firmware | [Arduino and ESP-IDF](firmware_examples.md), [S2 bench image](../examples/s2_bench/README.md) |
| Console and host clients | [CLI/JSONL](cli_contract.md), [API/CLI map](api_cli_mapping.md), [Python runners](runners.md), [finite scenarios](bench_scenarios.md) |
| Physical evidence and open questions | [Drive questions and examples](device_conflicts.md#questions-in-plain-language), [owner-return checklist](prompts/README.md#deferred-to-owner-return), [bench facts](hardware_bench.md), [host-loss limits](host_loss.md) |

Examples use the same public operations as callers. The
[units](../examples/units_preview/README.md),
[read/move/stop](../examples/position_preview/README.md),
[angle](../examples/angle_preview/README.md),
[velocity](../examples/velocity_preview/README.md),
[discovery](../examples/discovery_preview/README.md) and
[TPDO1 subset](../examples/pdo_configuration_preview/README.md) previews are offline.
Their fake events and explicit evidence records qualify no hardware.

## Integration and maintenance

- [Architecture](architecture.md), [profile contract](profile_contract.md),
  [multi-drive decision](multidrive_decision.md) and [new-drive playbook](prompts/ADD_DRIVE.md):
  current dependency boundaries and the work a real additional profile needs.
- [Prospective FieldCore handoff](fieldcore_handoff.md): independent installed
  consumer and product integration responsibilities; no FieldCore CAN integration.
- [Source map](source_map.md): declarations, implementations, callers and tests.
- [Verification](verification.md), [contributing](../CONTRIBUTING.md),
  [security](../SECURITY.md), [license](../LICENSE) and [changelog](../CHANGELOG.md).
- [Coverage](coverage.md), [operation map](reference/lichuan_operations.json),
  [generated coverage](reference/lichuan_generated.md) and
  [reviewed ledger](reference/lichuan_objects.md): separate metadata, typed
  implementation, CLI, tests and physical qualification.
- [Progress](prompts/PROGRESS.md), [backlog](backlog.md), [roadmap](roadmap.md)
  and [promptset](prompts/README.md): current work and precise inherited gates.

## Historical and source evidence

Dated reports preserve what was checked at that revision. Maintained contracts
above own current behavior; historical claims are not silently rewritten.

- [Documentation/package audit](reports/prompt_49.md), [finite campaign](reports/prompt_47.md)
  and [failure/load campaign](reports/prompt_48.md).
- [Prompt51 implementation and trials](reports/prompt_51.md): finite host SYNC,
  typed TPDO1 experiment and remaining sampling/RPDO/10 ms qualification.
- [Prompt52 implementation](reports/prompt_52.md) and [raw motion-change analysis](reports/prompt_52_measurements.md):
  typed finite changes, initial specimen observations and remaining qualification.
- [Independent auditor disposition](audit/2026-11-15_auditor_disposition.md),
  [RS audit](audit/MOTORCONTROL_RS_AUDIT.md), [ecosystem audit](audit/ECOSYSTEM_CONVENTIONS.md)
  and [audit summary](audit/SUMMARY.md).
- [Original references](reference/README.md), [reviewed device decisions](reference/lichuan_review.md),
  [drive contrasts](reference/drive_contrasts.md) and [copied baseline](baseline/README.md).
- [Architecture plan](architecture_plan.md) and [retained ownership proof](../experiments/api_ownership/README.md):
  planning/prototype material, not an installed feature list.

Prompt18/Q17 remapping, feature-specific firmware facts and physical qualification
remain open. Closing a documentation audit does not close those obligations.
