# Add a CAN drive or protocol profile

Use this playbook when a concrete additional drive is selected for implementation. It complements the numbered stages; it does not authorize implementing every researched candidate automatically. Apply [COMMON.md](COMMON.md), [the multi-drive decision](../multidrive_decision.md) and root AGENTS throughout. Keep sibling repositories read-only.

Apply the exact [conventions](CONVENTIONS.md), continuous [documentation/Doxygen](DOCUMENTATION.md), [maintainability](MAINTAINABILITY.md) and [delegation/model-selection](DELEGATION.md) contracts. Preserve common status/operation/owner/health names; add native names only for real differences. Update README, CHANGELOG, capability/coverage, typed API comments and examples alongside the profile implementation. Delegate independent source review and disjoint native groups; one writer owns shared protocol and public-contract changes.

## Audit and choose the scope

1. Refresh MotorControl-RS, ADS1115/INA228, relevant I2C source/CLI/runners/tests and current FieldCore consumer boundaries. Inspect the existing CAN library code and exact current profile implementation; preserve common names and behavior.
2. Identify manufacturer/product/interface suffix/firmware and motor/feedback configuration. Establish actual protocol: CANopen/CiA402, CANSimple, VESC, TMCL, multiplex/FD or another documented format. CAN connectors or a marketing claim are insufficient.
3. Download primary manuals, EDS/DCF or protocol/DBC/register descriptions and applicable release notes. Record immutable URLs/revisions, hashes, rights and retrieval failures. Build a complete native-function/side-effect ledger; explicitly separate confirmed, disputed and absent features.
4. Declare the implemented subset and intended full-profile scope. Use Nanotec N5 as the recommended next CANopen profile, with EPOS4 as the independent servo contrast. Exact CANopen suffixes matter for every vendor. Do not silently switch to an EtherCAT, pulse-input or serial model.

## Reuse only established layers

For another CANopen profile, reuse existing service/owner and reviewed CiA402 primitives; add native object policies, scaling, state quirks, sequence requirements and typed settings in `profiles/<family>/`. Do not fork the SDO/PDO stack for each motor manufacturer.

For another CAN protocol, add its own bounded codecs/services and route declaration beneath common motion. Reuse CAN transport only where identifier format, RTR/FD/length and callback guarantees match. A CANSimple parameter transaction is not a CANopen SDO. Extended IDs, float/native packing, electrical RPM, no-ack commands and sequence-less feedback must retain their actual meanings.

For a CAN FD requirement, implement and test a concrete bounded FD frame/adapter capability separately; reject incompatible controllers. An explicit logical endpoint maps to controller/channel/node/subaxis, so profiles for multi-axis devices do not need a new common motion API. Do not add a runtime plugin framework or mandatory dependency on every protocol.

## Implement and verify

- Map supported common reads, exact targets, finite moves/velocity, home, enable/release/fault-clear and stop into typed profile operations. Unsupported common features reject before TX with evidence-based capability reasons.
- Preserve commanded/actual/estimated feedback, scale/reference generations, mode/calibration requirements, brake/stop differences and no-replay uncertainty. Audit watchdog/deadline service for every active operation, including finite onboard trajectories. Any required refresh must preserve current intent under a bounded lease and must not retrigger a relative move/restart a trajectory; never disable the watchdog implicitly. Do not fake CANopen state or acknowledgements for another protocol.
- Implement documented native settings/functions through typed APIs. Torque/current/impedance/cyclic capabilities require explicit units, reference/scaling, duration/deadline/service requirements and stop/watchdog semantics. Current limits do not count as effort motion.
- Extend common CLI/JSONL capability output and API mappings; native groups live under the selected profile with one command inventory. Extend Python finite scenarios and evidence parsing rather than creating a second motion path.
- Test valid and malformed native frames, bounds/endianness/quantization, state transitions, asynchronous routing, timeouts/late response ambiguity, owner fairness/stop, partial side effects, reboot/recovery, retained results and no automatic replay.
- Test profile-only and mixed-profile builds, CMake source/install/PlatformIO, optional adapters and native consumer contracts. Common headers must remain framework-independent. No public SDK/stack/FieldCore types.
- Default to isolated protocol domains per physical bus. If coexistence is requested, verify identifiers/format/bitrate/broadcasts/filtering/bus load and response matching before enabling it; node-number differences alone do not prove safety.

## Acceptance and handoff

Write a per-profile report and coverage matrix linking each native function to evidence, API, CLI/rationale, tests and qualification. Record dependency pins and public compatibility changes. Host tests and a contract fixture do not establish hardware support. Qualify actual model/firmware/motor/configuration/fixture with the stage-18 procedure before making physical claims.

Update `docs/backlog.md`, `roadmap.md`, profile inventory, help/caps, package scope and release notes. Run applicable exact-commit CI for a major implementation when available. Keep incomplete confirmed functions and unresolved documents explicit; do not mark a placeholder adapter or raw-object escape hatch a complete new profile.
