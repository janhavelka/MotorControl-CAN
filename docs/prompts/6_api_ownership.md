# Prompt 6 Common API and ownership proof

Apply [COMMON.md](COMMON.md). Prerequisites: [5 CANopen service fit experiment](5_stack_decision.md).

## Objective and boundaries

Freeze the minimum concrete public and owner contracts needed by later implementation. Demonstrate those boundaries in one small native proof using prompt 5's selected service approach. Do not build production arithmetic or duplicate complete motion/profile engines; prompts 9–10 replace prototype math, and later operations implement their own reviewed sequences.

## Read and audit

Refresh RS, ADS1115 and INA228. Read actual RS Units/Axis, ReadOperation/ActionOperation/MoveOperation/VelocityOperation and ESS callers, including how terminal evidence is accessed. Inspect the owner interface and FieldCore's motor endpoint/configuration contract read-only. Use the current source map and prompt 4 contrasts. Apply [CONVENTIONS.md](CONVENTIONS.md#3-motion-typed-reads-and-terminal-evidence), its owner section, and [MULTIDRIVE.md](MULTIDRIVE.md#required-contrasting-test-cases).

## Implement

1. Write concise `docs/architecture.md`, `axis_contract.md`, `profile_contract.md`, `canopen_services.md` and `can_owner.md`; each owns distinct facts and links the others. Freeze dependency direction: common exact intent and concrete operation contexts; profile-owned typed objects/sequences; protocol services; application CAN owner; shared probe application. No core clocks, I/O, framework types, logging, allocation or persistence.
2. Define concrete frame, endpoint/binding, service work/evidence, operation/step token and generation types sufficient for the proof. Separate logical axis from wire node/channel/subaxis; raw CANopen abort/EMCY and NMT/drive state stay native observations rather than mandatory generic axis fields. Use separate motor, feedback and mode capability facts.
3. Specify operation contexts with RS `prepare...`, `next...`, `advance...` vocabulary, checked typed getters and actual context terminal evidence. Preserve atomic candidate publication and borrowed-call input rules. Do not invent universal result getters, virtual profile bases, runtime plugins or workflow languages. Application owner names remain `admit/admitUrgent/service/result/release/cancel` where meanings match.
4. Freeze a single fixed-capacity ownership table: node/channel storage, active transfers, queued/copied requests, axis reservations, urgent capacity, observations and retained terminal results. Define continuous RX dispatch, per-channel SDO serialization, fairness, cancellation versus physical stop, immutable deadlines, TX retirement and unresolved-channel handling. Local tokens cannot disambiguate identical delayed SDO replies on the wire. Prompts 13, 15 and 23 extend this same storage and service boundary.
5. Demonstrate a read flow and representative move flow through fake evidence: prepare candidate, yield work, admit copied request, service, route observation, advance original context, retain result and explicitly release slot. Mark the first point at which a write may have reached hardware; queue acceptance and native acknowledgement are different evidence.
6. Add small test-only N5 stepper/BLDC and EPOS4 common-contract fixtures using prompt 4's backed facts. Prove optional full-step geometry, independent command/feedback scale, endpoint/subaxis identity and a non-CANopen binding. No installed placeholder profile or invented native sequence is permitted. Record API/CLI coverage intent without claiming implemented routes.

## Verify and exit

Compile the proof with the native toolchain and no Arduino/ESP-IDF/FieldCore dependency. Interleave an asynchronous event during a fake service exchange. Check copied request lifetime and retained results after completion. Verify no heap/I/O claim at the layer it describes; setup allocation in an optional adapter is separately documented.

Require a reviewer to attempt a servo axis without full-step geometry and a protocol endpoint without CANopen state/address requirements. Exit with concrete declarations and enough decisions to avoid later duplicate tables/engines. Identify prototype code explicitly for migration into prompt 7; production arithmetic remains scheduled. Close scope in `docs/reports/prompt_6.md`, update PROGRESS and affected coverage, then commit and sync this branch.

## Documentation and delegation

Apply [DOCUMENTATION.md](DOCUMENTATION.md#doxygen-contract-in-the-source) to every public proof declaration, including units, lifetime, time and failure guarantees. Apply [MAINTAINABILITY.md](MAINTAINABILITY.md#make-control-flow-and-state-reviewable). Under [DELEGATION.md](DELEGATION.md#task-brief-and-handoff), one agent owns shared headers; an independent stronger reviewer challenges ownership, uncertain outcomes and protocol neutrality. Delegate fixture inputs separately after the type contract is fixed. Useful complete, checked chunks may be committed and synced during the prompt.
