# Prompt 16 NMT requests and service eligibility

Apply [COMMON.md](COMMON.md). Prerequisites: [15 Passive network events and invalidation](15_network_events.md).

## Objective and boundaries

Implement explicit NMT commands and evidence-based CANopen service eligibility, including heartbeat-disabled startup. Complete requested-versus-observed state handling and domain-wide broadcast effects. This is network service work, not drive enable, physical stop or automatic startup/recovery. PDO mapping remains separate and SDO uncertainty rules remain intact.

## Read and audit

Refresh RS, ADS1115 and INA228 and inspect explicit action/evidence and passive-versus-configured state patterns. Inspect FieldCore ownership and binding invalidation read-only. Read primary CANopen NMT service rules, selected stack behavior and Lichuan documented support; use the original ledger evidence for producer heartbeat and absent host-loss-consumer claims. Follow [CONVENTIONS.md](CONVENTIONS.md#4-one-application-can-owner) and [MULTIDRIVE.md](MULTIDRIVE.md#required-contrasting-test-cases).

## Implement

1. Add typed explicit NMT request encoding and bounded application-owned progression using the existing dispatch/storage. Validate addressed nodes separately from broadcast node zero. Support only reviewed commands. Queue/TX evidence is recorded separately from observed NMT state transition; NMT has no SDO-style acknowledgement and cannot return synthetic remote confirmation.
2. Retain requested state, last observed state/time, pending transition and current evidence validity independently. A rejected/proven-not-transmitted request leaves prior observations current according to their policy. Once a transition may have transmitted, the prior state remains history rather than an indefinitely current fact; do not promote to the requested state simply because time elapsed.
3. Define domain broadcast handling: node zero targets the network, not an ordinary axis binding. Record which local bindings/work/cache generations the command affects and invalidate them according to the command's actual effect. Preserve uncertain earlier writes and terminal results. Do not issue hidden broadcast starts/resets as recovery.
4. Implement CANopen service eligibility using the reviewed primary contract: SDO in Pre-operational/Operational, PDO in Operational, neither ordinary service in known-current Stopped. Separate observed exact NMT state from SDO serviceability. A fresh matching SDO response proves service availability, not Operational or the requested exact state.
5. Handle producer heartbeat disabled/unknown without circular admission. Permit caller-requested reviewed SDO writes when actual object/drive prerequisites and service evidence are satisfied; enabling a heartbeat producer cannot inherently require a pre-existing heartbeat. Discovery/reconciliation remain non-changing. After an explicit transition makes prior Stopped evidence no longer current, a bounded reviewed non-changing probe may reconcile unknown serviceability. Do not silently start NMT to obtain evidence.
6. Define how prompt 17 can accept a valid configured TPDO when exact NMT is unknown instead of dropping the first possible evidence. Any Operational inference must cite the reviewed protocol/profile rule and retain provenance. Neither SDO success nor NMT TX alone establishes Operational. Keep network state below common motion; NMT Stopped is not a generic motor state or a CiA402 quick stop.

## Verify and exit

Test addressed and broadcast commands, invalid targets, queue rejection, possible TX without observed transition, observed conflicting state and deadline expiry. Exercise Pre-operational/Operational/Stopped/unknown eligibility, disabled heartbeat, explicit producer configuration after service reconciliation, and stopped-to-requested-transition probing. Verify broadcast invalidation covers the applicable bindings without creating a node-zero axis.

Inject transition observations during SDO activity and ensure deadlines/results remain governed by their own contracts. No automatic NMT traffic, heartbeat configuration, drive enable or fault clear may occur during construction, probe or recovery. Run focused tests, Doxygen/verifier, integrity and diff checks.

Exit with complete explicit network intent/evidence behavior. Close all owned scope in `docs/reports/prompt_16.md`, update PROGRESS and service documentation, then commit and sync the same branch. A complete tested transition group may be checkpointed earlier; no state-promotion shortcuts may remain pending.

## Documentation and delegation

Apply [DOCUMENTATION.md](DOCUMENTATION.md#doxygen-contract-in-the-source) to target scope, evidence, time and invalidation. Include one service-eligibility table and link it from owner docs rather than duplicating it. Apply [MAINTAINABILITY.md](MAINTAINABILITY.md#make-control-flow-and-state-reviewable). Under [DELEGATION.md](DELEGATION.md#task-brief-and-handoff), use stronger independent review for state/serviceability distinctions and broadcast effects; delegate source-backed test vectors after the contract is fixed.
