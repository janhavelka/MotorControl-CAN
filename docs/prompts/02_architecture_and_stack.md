# Stage 02 Freeze architecture and service ownership

Apply [COMMON.md](COMMON.md). Prerequisites: stages 00–01.

## Audit

Read actual MotorControl-RS Units/Axis/operation headers, concrete ESS sequences, owner interfaces and the current FieldCore motor contract/module. Compare ADS1115's owner-loop and INA228 transport boundaries. Inspect current primary CANopenNode source/API/license and the platform APIs under consideration; record immutable revisions. The existing RS feasibility report recommends evaluating a stack, not mandatory adoption without a fit assessment.

## Deliver

1. Write `docs/architecture.md`, `axis_contract.md`, `profile_contract.md`, `canopen_services.md`, `can_owner.md` and a focused stack decision record. Use the provided architecture/naming plans, refining them with evidence.
2. Build a tiny native service proof using a pinned CANopenNode adapter, or an equivalently concrete evaluation sufficient to identify unacceptable costs. Measure/inspect ownership, memory allocation, C/C++ boundary, configuration, scheduling, SDO-client capability, event routing, license, dependency export and test port. Compare with a bounded local subset. Select one authoritative implementation; no duplicated active SDO/PDO engines.
3. Freeze portable value types: CAN frame flags/length, node/channel binding, service request/response, operation/step tokens, controller evidence, NMT/drive states, raw SDO abort/EMCY, profile capabilities and generation counters. Avoid public dependency on stack-specific or FieldCore types.
4. Define work/event/result contracts and ownership table. Use concrete read/action/move/velocity/home contexts with the RS vocabulary. Document result access as actually used by the source, atomic output guarantees, lifetime, capacities, deadline arithmetic and cancellation/stop separation.
5. Define application owner scheduling before implementation: per-channel SDO serialization, ordinary per-axis reservation, urgent-stop capacity, continuous receive dispatch, fairness, queued TX retirement, stack callbacks, timestamp meaning and late-response quarantine. Local IDs do not disambiguate identical late SDO replies on the wire.
6. Produce a planned API/CLI coverage map and complete dependency direction. Choose initial units/profile limits from source evidence, never RS bench defaults. Identify native fields leaking ESS representation and replace them with justified types.
7. Apply `MULTIDRIVE.md` with document-backed N5 stepper/BLDC and EPOS4 semantic contrasts. Build test-only N5 and EPOS4 common-contract fixtures through the common types and fake service boundary, using only source-backed capability/unit/state facts; no installed placeholder driver or invented sequence when evidence is missing. Keep logical axis identity separate from wire node/channel/subaxis, and CAN framing separate from CANopen. Use ODrive/VESC/moteus research to check non-CANopen/FD extension boundaries without building those backends now.

## Concrete implementation contract

Use [CONVENTIONS.md](CONVENTIONS.md) as the selected naming contract. Draw the dependency boundary in code with a native compile proof before writing broad engines:

| Layer | Concrete responsibility | Must not own |
| --- | --- | --- |
| `Units.h/Axis.h` | Exact intent, configuration, logical axis, reference evidence | CAN node range, stack objects, clocks |
| Common operation headers | Work/evidence vocabulary and concrete operation context contracts | Runtime plugin registry, device I/O |
| `profiles/lichuan_c/` | Verified typed objects and finite sequences | CAN controller or hidden scheduler |
| `canopen/` | CANopen service values and pure state/codec helpers | A second active stack |
| `examples/common/CanBusOwner.*` | Single receive path, admission, channel/node storage, adapters | Application machine policy |
| `examples/probe_cli/ProbeApp.*` | Selected axis/config/cache, explicit operation policy | Private replacement motion implementation |

Stage 06 builds the service part of this owner boundary; stage 07 adds events to the same storage; stage 12 integrates scheduling and diagnostics. Do not independently allocate three node tables or service engines. Establish fixed capacities and ownership of each buffer/table/context in one table, including results retained after completion.

The stage-02 proof is deliberately small: enough concrete declarations and representative values to expose ownership and protocol assumptions. Stage 03 registers/migrates it into the real build; stage 04 replaces prototype arithmetic with production units and keeps the contrast assertions. Do not implement a duplicate complete motion library just to pass an early fixture.

Write one small flow for a read and one for a move: prepare candidate; yield work; admit copied request; service transport; route evidence; advance original context; retain terminal result; release slot. Mark when a write may have reached hardware. Distinguish operation ID, owner request ID, step ID and binding generation without putting fictitious IDs on SDO wire messages.

Choose the stack using a buildable narrow experiment, not a generic feature comparison. Record whether the port allocates only at setup, how callbacks retain data, how TX retirement works, and which services are actually required by the ledger. If the stack cannot meet a required boundary, record the concrete blocker and choose the smallest viable local subset.

Keep extension points concrete: profile-owned typed functions and capability values, one service boundary, and test-only alternate representations. Do not add virtual base classes, a configurable workflow language, a global object-number action dispatcher or a plugin loader merely to accommodate future drives.

## Documentation and work split

Apply [DOCUMENTATION.md](DOCUMENTATION.md) and [MAINTAINABILITY.md](MAINTAINABILITY.md). Document the public prototypes, namespace policy, ownership diagram, service choice, memory table, deadline rules and deliberate RS differences. Annotate any public proof headers with Doxygen; identify prototype code that is not yet production API.

Use [DELEGATION.md](DELEGATION.md) for parallel stack feasibility and independent architecture review. One lead owns common headers. Require the architecture reviewer to attempt a servo configuration with no full-step geometry and a non-CANopen logical binding before accepting the boundary.

## Verify and exit

Compile the proof on the native toolchain without Arduino/ESP-IDF/FieldCore. Demonstrate ownership with a fake service exchange and asynchronous event during it. Review no-I/O/no-heap claims at the correct layer; a setup-allocating SDK does not invalidate or extend a core-only guarantee.

Exit with a selected stack/service boundary and enough concrete types to implement subsequent stages without architecture churn. The multi-drive fixture must demonstrate common code without mandatory stepper geometry or CANopen state/address semantics. A prose-only selection without examining actual APIs/resource policy is incomplete.
