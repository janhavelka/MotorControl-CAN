# Prompt 41 Typed device and network CLI routes

Apply [COMMON.md](COMMON.md). Prerequisites: [21](21_io_settings.md), [22](22_tuning_settings.md), [37](37_discovery.md), [38](38_communication_commissioning.md), [39](39_persistence.md), [40](40_cli_parser.md).

## Objective and boundaries

Connect implemented typed reads, settings, discovery, commissioning, persistence and CANopen network services to the existing console inventory. Complete these public routes and their tests/docs as one feature group. Finite motion and drive actions remain prompt 42's responsibility. The parser validates syntax and dispatches public requests; it never reconstructs profile sequencing, interprets undocumented bits or bypasses access policy through raw object commands.

## Read and audit

Refresh current RS read/settings/discovery/persistence console routes and application dispatch, plus ADS1115/INA228 distinction between cached diagnostics and active reads. Inspect SHT3x command classifications and relevant FieldCore settings/ownership expectations. Read this repository's actual typed APIs, ledger and CLI mapping, including unresolved feature gates. Apply [CONVENTIONS.md](CONVENTIONS.md), [MULTIDRIVE.md](MULTIDRIVE.md), [DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

### Concrete evidence to recheck

The [object tables](../audit/LICHUAN_OBJECT_FACTS.md) must drive typed command coverage: distinguish observed mode `6061` from requested `6060`, diagnostic `2102` bits0..2 from output commands `60FE` bits16..18, and node-setting `2203` from host selection. Print native values with explicit units and unknown/conflicting interpretation. Source conflicts in DO layout, filters or tuning remain capability gates in CLI; a raw command must not bypass typed policy. Save `2300=2` and factory reset `=1` remain explicit separate intents.

## Implement

1. Add explicit bounded `probe`, `read identity`, `read config` and `read state` paths with their reviewed typed getters. Keep cached `status`, `health`, `stats`, `drv` and `result` unchanged. Unknown/stale fields remain explicit; an active read's success does not fabricate motion state or refresh unrelated evidence.
2. Add actual implemented typed driver, I/O, control, tuning and motion-profile settings groups. Include required snapshot/readback behavior and uncertainty without burying writes in an apparently passive command. Complete argument validation before admission; retain profile-specific ranges, exact units and side-effect classifications from public APIs.
3. Expose bounded discovery and explicit binding through reviewed public workflows. Expose communication changes, persistence and reset only with their full typed prerequisites, partial-effect/readback/restart semantics and capability gates. Selection or connection must not trigger these routes. No automatic recovery, rollback, replay or save is implied by a failure.
4. Add CANopen-specific NMT, PDO, heartbeat and network routes through existing service APIs. Keep requested versus observed node state distinct. Reject mismatched protocol/profile use without transmitting guesses, and never claim NMT stop is motor quick stop. `profile list` and `caps` distinguish compiled support, selected protocol, configured motor/feedback, firmware and qualified features; surveyed fixtures remain excluded from installed support.
5. Complete applicable monitor, trace, `capture-read` and diagnostic-workload routes from the mapping. Passive trace copies bounded owner traffic and never owns RX. `capture-read` is an explicit bounded diagnostic read through the owner, with its side effects classified. Do not create an unrestricted controller sender or raw dictionary escape command.
6. Extend the existing inventory, JSONL records, help and coverage only for working routes. Preserve request/operation correlation, exact numbers and stable asynchronous mode. Protocol abort and native evidence remain available without being confused with application health or host parsing errors.

## Verify and exit

Test route-to-public-API parity and each group's supported/unsupported, legal/illegal and pre-admission rejection cases. Exercise failed read, partial settings effect, save/reset uncertainty, interrupted commissioning, stale binding, malformed arguments and mismatched profile/protocol. Prove passive diagnostics and discovery perform only their documented traffic. Check no hidden retry after a lost reply and no route that implements a capability available only in CLI. Run the native console integration with fake owner events and existing JSONL integrity tests. Required firmware parity and motion routes follow in prompt 42; record these as later scope rather than unfinished routes claimed here.

## Documentation and delegation

Update command grammar, exact invocation examples, side-effect table, native/API/CLI coverage and CHANGELOG with the tested behavior. Document rejected capabilities and actionable prerequisite errors. Add Doxygen for changed console/application contracts. After the lead fixes inventory/schema, normal engineering agents may own disjoint route tests or a settings-group source audit. A stronger reviewer checks persistence/network side effects and uncertain outcomes. Workers do not edit the shared inventory concurrently or send hardware commands.

Close this prompt's owned scope and checks; record `docs/reports/prompt_41.md`, remaining motion-route ownership and evidence gates. Commit and sync on the same recorded execution branch before handoff, including useful complete checkpoints when appropriate.
