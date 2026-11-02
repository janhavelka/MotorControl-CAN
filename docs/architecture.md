# Common API and ownership boundary

Prompt 6 freezes the minimum semantic contract in the
[native ownership proof](../experiments/api_ownership/README.md). The concrete
declarations are buildable C++11. Prompt 7 moved stable status, endpoint/binding
and Classical frame validation into the [native package](getting_started.md).
Prompt 9 supplies production [exact units and quantization](units_status.md).
Synthetic operations remain test support; pure axis/reference policy is installed,
and prompt 23 now supplies the ordinary application owner, now in adapters/owner.
No installed driver, hardware support or complete motion implementation
is claimed. The [architecture plan](architecture_plan.md) remains
the target repository layout; this document governs the narrower proven boundary.

| Layer | Owns | Dependencies allowed |
| --- | --- | --- |
| Common values | Exact intent, logical identity, immutable binding/configuration generations, capabilities | Standard integer types only |
| Concrete profile operations | Typed native reads, checked conversion, explicit prepare/next/advance sequences and context terminal evidence | Common values and protocol work/evidence values |
| Protocol service adapter | Wire correlation, selected client lifecycle, raw protocol errors | Common descriptors and optional CANopenNode, outside the installed pure core |
| Application CAN owner | One RX dispatcher, time, reservations, queues, controller handoff, retained results and raw observations | Core plus selected protocol/controller adapters |
| Shared probe application | Explicit selection and typed read stepping; bounded platform I/O. CLI, persistence and board startup remain later work. | Public operations and the existing application owner |

The application passes values between these layers; the core never calls upward
into an owner, framework or SDK. There are no virtual profile bases, callbacks
into an application from core operations, runtime plugin selectors, universal
result getters or sequence-description languages. Profile-specific functions
remain ordinary functions with concrete caller-owned contexts.

Distinct contracts own distinct facts:

- [Axis](axis_contract.md): identity, capability facts, exact units, scales and reference provenance.
- [Profile](profile_contract.md): real proof signatures, candidate publication, typed getters and terminal meanings.
- [CANopen services](canopen_services.md): one selected engine, wire evidence and adapter limits.
- [CAN owner](can_owner.md): the single capacity table, scheduling, lifetimes and retirement.

The root `MotorControlCAN` target contains the installed common core and the
Lichuan C profile, always built together. No selected-profile option exists; its
only options select examples and the optional CANopenNode adapter. A core-only
consumer means one that includes only core headers. The test-only `proof_contract` target contains `Contracts.h`, `Operations.h` and
`Operations.cpp` and links that core. Neither includes nor links CANopenNode. Their source contains
no allocation, clock acquisition, I/O, logging, retry, persistence or framework
types. `api_fixtures` links only this target. Application storage and the optional
stack bridge are compiled separately in `api_owner`. Test executables use host
I/O to report failures; this does not extend the core's claims to an entire host
process. Fixed storage/no allocation is established by source inspection, not
a heap profiler or embedded measurement.

The initial frame stores eight Classical CAN bytes, including independent
standard/extended and data/RTR shape. CANopen service restrictions are enforced
by its adapter. CAN FD is explicitly rejected before admission; a future FD
backend needs a separate bounded frame/capability milestone. A proprietary
binding is representable without CANopen node-range or state fields, but this
proof implements no proprietary service. One protocol domain per interface is
the default; mixed-domain traffic requires an explicit compatibility design.

## Migration and acceptance

Prompt 7 registers these retained proofs in the real build and splits the stable
status/endpoint/frame declarations into public headers. Synthetic
`ProofProfile`, fake controller, fixtures and optional stack dependencies stay out of
installed public includes. Preserve these tests during later migration; do not install
fake N5/EPOS4 profiles or freeze struct byte layout as a binary ABI.
Keep the synthetic I32 wire bounds/capabilities and the documentary ramp fixture
dimensions profile-specific; they are not universal native representations.

Prompt 9 replaces the duplicate numeric types and bounded arithmetic through
production Units; prompt 10 replaces proof coordinate arithmetic with the
[production axis/reference API](axis_contract.md). `ProofAxisConfig` adds only
test capabilities and I32 wire bounds to that real configuration. Prompts
13/15/23 extend the same owner/service storage boundary for transfers, network
observations and application scheduling; they must not add competing client
engines or result tables. Prompt 14 establishes reconciliation evidence. Native
typed features and real motion sequences retain their existing prompt owners.

The [source review](reports/prompt_6_source_review.md),
[independent review](reports/prompt_6_review.md) and
[execution report](reports/prompt_6.md) distinguish retained RS names, deliberate
CAN differences, host tests and remaining physical qualification.

## Confirmed native setting transactions

The [fresh 20/21 audit](reports/prompts_20_21_audit.md) adds one finite
`DriverSettingsContext` for the documented 86C parameter and DI candidates.
It reuses the existing typed group readback and SDO work/result boundary; it
adds no controller, clock, queue, node registry or motion executor. The sole
application owner supplies continuous exclusion, actual control/state evidence
where required, and current bindings. Configuration prerequisites are withdrawn
before possible transmission; only the reserved old-epoch sequence may finish
its copied downloads. Healthy idle service epoch advancement then enables
new-epoch matched readback without reopening quarantine or renewing liveness.
The [owner guide](can_owner.md#native-settings-exclusion-and-configuration-epochs)
owns integration rules; hardware qualification remains separate.

## Application scheduling and local recovery

[CanBusOwner](../adapters/owner/CanBusOwner.h) borrows the sole existing
SdoClient and narrow synchronous controller callbacks. Six ordinary copied
request/result slots, one urgent slot and two reservations add scheduling without another node table or CAN
receiver. Logical-axis/physical-peer exclusion spans multiple operations;
cyclic producer selection and per-channel serialization permit other nodes and
passive traffic to progress. [The owner guide](can_owner.md) owns budgets,
lifetimes, actual TX reports and settings configuration epochs. The compiled
[consumer](../examples/owner_preview/README.md) and direct typed read/loop-setting
tests use this ordinary path. Prompt 24 adds cancellation/invalidation, exact
external-frame retirement and independently retained finite controller recovery.
Local retirement cannot establish remote SDO/PDO freshness. No motion executor
or future motion headers are introduced.

Prompt 25 adds derived binding-local Health, interface Stats and eight copied
diagnostic records in this same application class. [The owner contract](can_owner.md#passive-communication-health)
defines projection and reset rules. Existing node/PDO provenance remains the
source of freshness; diagnostic copies neither create a receiver nor become
current node storage. Core packages acquire no lifecycle, clock or health policy.

## TWAI controller boundary

The application-only [TWAI adapter](../adapters/twai/README.md) supplies the
existing owner transport. The pinned S3 node driver owns one active SDK frame
and bounded ISR copies; the owner alone routes ingress. RX precedes TX promotion,
and explicit recovery deletes old controller storage before creating a new node.
No SDK types enter installed headers; no node/service table or CANopen engine is
duplicated. [Reviewed SDK limits](adapters/twai_sdk.md) remain distinct from
firmware startup and physical qualification.

## Shared firmware composition

[The firmware assembly](firmware_examples.md) borrows the same service store and
constructs the existing owner, application and transport exactly once. Both SDK
entry points create the same pinned internal task. Clock acquisition, USB byte
callbacks and scheduling remain application code; the installed core is unchanged.
A failed clock invalidates at the owner's last accepted time before attempting
local retirement. No startup selection, request or automatic recovery exists.
