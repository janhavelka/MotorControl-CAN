# Prompt 6 independent boundary review

Date: 2026-10-28. Scope: the actual common declarations, synthetic operations,
application owner, retained-result path and native contrast/flow tests in
`experiments/api_ownership/`. This is a review of a bounded prototype, not a
CAN motor driver or a hardware qualification. The reviewer made no Git
mutations, sibling edits or hardware calls.

The reviewer reread prompt 6, COMMON, CONVENTIONS sections 3-4, DOCUMENTATION,
MAINTAINABILITY, the corrected prompt-4 contrasts, the prompt-5 decision/report
and the actual selected CANopenNode experiment. The existing service engine and
its caller-owned callback/FIFO/terminal storage were inspected directly.

## Findings and disposition

| Finding | Checked correction |
| --- | --- |
| Initial common Frame included 64-byte FD storage despite the separate-FD milestone. | Frame now owns eight Classical bytes; extended IDs and RTR/DLC remain independent. FD requirements reject as UNSUPPORTED and do not imply an adapter. |
| Checked server abort cleared possible write effects. | `ActionExecution::REJECTED` now retains `uncertain` after possible transmission. Raw unsigned abort bits remain separate from Status detail. |
| `advanceRead` documentation promised a future-time check without a separate current-time input. | `observedUs` is explicitly supplied owner service/delivery time; validation enforces retained monotonic time and deadline ordering. |
| A late dispatch near UINT64_MAX could pass outer admission but overflow the bridge's fixed millisecond timeout and falsely quarantine a fresh route. | Admission rejects immutable deadlines above UINT64_MAX minus 65,535,000 microseconds. Bounding the deadline, rather than admission time alone, also covers queue delay. Rejection leaves clock, step and ID unchanged. |
| Boot-up could otherwise be merely a queued observation. | Boot-up invalidates the route and pending work independently of observation-ring capacity; active possible transmission remains visible. |
| The lead found that immutable per-request deadlines alone allowed later steps to renew the same operation's lease. | First successful admission freezes the reservation deadline. Ordinary and urgent later steps reject any different deadline atomically; release/new reservation establishes a new operation. Independent probes confirm rejected admission changes neither deadline, clock, step nor output ID. |

The generic binding accepts proprietary address 500 and subaxis 2 without
CANopen network/drive state. The synthetic CANopen profile rejects that protocol
before preparing service work; the application owner also rejects the route.
This separation does not implement a proprietary service or manufacturer
subaxis mapping.

One stationary private bridge owns the actual selected SDO client. Each admitted
request already owns a canonical terminal slot. Completion bytes are copied
there and the bridge's transient result is released before another transfer can
start. A retained terminal does not hold the channel, and releasing it does not
release the axis reservation or clear channel uncertainty. Asynchronous frames
have one public receive entry and copied bounded observation storage.

Urgent capacity is one separate request/result slot. It cannot interrupt an
active SDO or overtake an earlier nonterminal step in the same reservation.
Eligible ordinary axes rotate, and a newly eligible ordinary request follows
one urgent start. A sequence is a reservation and ordering contract, not an
operation engine: callers must not preadmit success-dependent continuation
before advancing the preceding operation result. Local cancellation is not stop.

## Independent checks

An independent pure-core harness compiled with GCC/G++ 15.1.0, C++11 and
`-Wall -Wextra -Wpedantic -Werror` and passed these attempts:

- BLDC/encoder servo with no full-step geometry; half a load turn with 3/2
  motor/load ratio and 4096 command increments/motor turn produces 3072.
- Feedback scale 2048 is independent and may be absent for command preparation.
- Arbitrary proprietary address/subaxis binding is valid; synthetic preparation
  refuses it atomically. A CANopen servo requires no native state field.
- Stale configuration, absent position capability and stale typed-read provenance
  reject without replacing output. The retained identity owns its bytes.
- A high-bit abort value `0xDEADBEEF` survives with REJECTED and uncertainty;
  extended RTR shape is preserved and FD is rejected.

A separate owner harness passed retention across multiple completions, ordinary
and urgent scheduling, same-sequence ordering, explicit reservation release,
stale handles, transmitted-read cancellation/quarantine, exact-deadline TX
retirement, mismatched protocol rejection, clock-end atomic admission rejection,
and boot-up invalidation with an already-full event ring.
An additional direct regression passed later-step deadline extension/shortening
rejection, unchanged IDs and monotonic clock after rejection, a rejected first
admission leaving the deadline unset, and a fresh operation after reservation
release. The limited I32 origin/native bounds belong to the synthetic profile's
representation, not a future integer or float-native profile requirement.

An independent fresh Release configure/build used:

```text
cmake -S experiments/api_ownership -B .build/p6_boundary_review/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build .build/p6_boundary_review/build
ctest --test-dir .build/p6_boundary_review/build --output-on-failure -V
```

All three registered tests passed: `api_ownership`, `api_fixtures` and the actual
`canopen_service_fit`. Measured overlapping host object sizes were ServiceWork
112 bytes, ServiceEvidence 104 bytes and complete application owner 1384 bytes.
These are not embedded stack/RAM or latency measurements. Additional reviewer
harnesses are ignored local audit probes, not registered production tests.

## Source delta and limits

Live sibling revisions and clean states matched the preceding audit:

| Checkout | Revision | Directly inspected seam |
| --- | --- | --- |
| MotorControl-RS | `bd831c859daa9c8c07b8d27a2dd879bb4e750511` | Units/Axis and Read/Action/Move/Velocity declarations, ESS read preparation/typed getters, application RtuBusOwner, verification workflow. |
| ADS1115 | `235af3e8c722661a89a44766a4ccd903a654f6fe` | Atomicity differences in operation start, retained-terminal admission blocking, workflow. |
| INA228 | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` | Typed retained result identity and consuming takeJobResult, workflow. |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0` | Actual motor nextStep/poll/acceptObservation integration and immutable deadline handling, read-only. |

CAN intentionally preserves admission output on rejection and exposes
non-consuming result/release instead of copying sensor token-reset or consuming
result conventions. It also keeps protocol addressing, scales and native widths
independent of RS-specific fields. Unchanged sibling suites were not rerun.
The fuller stage audit is [the source review](prompt_6_source_review.md).

The selected bridge still needs a qualified initial route. Host tokens do not
resolve delayed identical replies; successful fixture exchanges must not be
presented as proof that arbitrary duplicate wire replies are correlated.
Production retirement/reconciliation, complete network/PDO provenance, actual
controller timing and manufacturer sequences remain with their assigned later
prompts. The synthetic write ends ACKNOWLEDGED with completion NOT_OBSERVED.
No CAN ACK, physical motion/standstill, installed profile or live-device support
claim follows from this review. Remote CI and hardware tests were not run by the
reviewer; final integrated repository checks and synchronization remain lead-owned.
