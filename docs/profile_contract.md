# Typed profile and operation contract

Prompt 6 implements one explicitly synthetic namespace,
`MotorControlCAN::ProofProfile`, under
[experiments/api_ownership](../experiments/api_ownership/Operations.h). It proves
the ownership and publication boundary with a fake peer. It is not an installed
Lichuan, N5 or EPOS4 profile. The [axis contract](axis_contract.md) owns exact
intent/capabilities; the [architecture](architecture.md) owns dependencies;
[CANopen services](canopen_services.md) owns the selected service backend; the
[CAN owner](can_owner.md) owns admission, dispatch, retirement and retained slots.

## Profile responsibility and actual entry points

A real profile owns its typed native observations, object access policy,
checked native conversion, supported sequences and evidence interpretation.
Common logical axes do not acquire mandatory NMT, SDO, statusword or CiA402 state.
Other protocols use their own native services. This proof uses CANopen work
descriptors and explicitly rejects proprietary bindings at preparation.

The actual declarations are in `ProofProfile`; these are declaration excerpts,
not a runnable application or a promise of installed ABI:

```cpp
Status prepareIdentity(const Binding& binding, OperationToken operation,
                       uint64_t nowUs, uint64_t deadlineUs, ReadContext& output) noexcept;
Status nextRead(const ReadContext& context, uint64_t nowUs,
                ServiceWork& output, bool& ready) noexcept;
Status advanceRead(ReadContext& context, const ServiceEvidence& evidence) noexcept;
Status getIdentity(const ReadContext& context, const Binding& currentBinding,
                   Identity& output) noexcept;
Status prepareMoveAbsolute(const PositionRequest& request, const ProofAxisConfig& configuration,
                           OperationToken operation, uint64_t nowUs, uint64_t deadlineUs,
                           MoveContext& output) noexcept;
Status nextMove(const MoveContext& context, uint64_t nowUs,
                ServiceWork& output, bool& ready) noexcept;
Status advanceMove(MoveContext& context, const ServiceEvidence& evidence) noexcept;
```

These preserve the observed RS `prepare…`, `next…`, `advance…` and checked typed
getter vocabulary. The [current source review](reports/prompt_6_source_review.md)
records the actual sibling signatures and deliberate CAN differences. There is
no generic operation engine, result getter or runtime profile registry.

The fake identity is a four-byte U32 marker uploaded from synthetic object
`0x2000:00`. It is not manufacturer, model or firmware identification. The
representative absolute target is a four-byte little-endian signed I32 download
to synthetic `0x2001:00`. These object numbers have no vendor applicability.
That write is neither CiA402 setpoint staging/triggering nor a complete physical
motion sequence. Real vendor object permissions and sequences require the
reviewed ledger and original model-applicable evidence.

## Publication, advancement and retained results

Preparation validates a complete candidate before replacing a caller-owned
context. It copies the binding, intent and any consumed axis configuration;
rejected preparation leaves all previous fields unchanged. Context fields are
inspection-only between calls, and the caller serializes shared access.

`nextRead`/`nextMove` copy the same current work/token while active. They do not
admit it or advance the operation. `ready=true` means work was produced; terminal
contexts return `ready=false` without replacing the work output. Invalid state
or time leaves both outputs unchanged. The owner prevents duplicate admission.
The current proof operations each use one step; their tokens are local identity,
not wire tags capable of disambiguating a delayed identical SDO reply.

Advancement checks the complete binding, operation/step identity, bounded
payload shape and supplied time before publishing a candidate. `observedUs` is
the owner's supplied current service time, not a separately sampled core clock
or drive sample time. It cannot precede preparation/last advancement. Normal
terminal events precede the fixed exclusive deadline; `DEADLINE` events occur
at or after it. The core acquires no time and performs no I/O, allocation,
retries, logging or persistence. The owner supplies qualified native evidence.

An `Ok()` return from advancement means the event was validly consumed, including
a terminal failure. `Status` is a local call result with static diagnostic text;
it is distinct from the retained context's operation `status` and `outcome`.

| Evidence or inspection | Actual proof meaning |
| --- | --- |
| `ReadContext.state/outcome/status/evidence` | Successful marker read or retained failed read, including copied raw service evidence |
| `getIdentity(context, currentBinding, output)` | Publishes only a successful identity of the correct kind and complete current provenance; incomplete, wrong-kind or stale input preserves output |
| `MoveContext.state/outcome/status/execution/completion/uncertain/evidence` | Concrete target-write result; callers inspect these fields directly |
| `ACKNOWLEDGED` write | `SUCCEEDED`, outcome/execution `ACKNOWLEDGED`, completion `NOT_OBSERVED`; no physical completion claim |
| Checked server abort | `PROTOCOL_REJECTED`, execution `REJECTED`; possible partial effects remain `uncertain` after possible transmission |
| Malformed reply/local client abort | `REPLY_ERROR`; preserves native abort evidence and possible-write uncertainty without inventing server rejection |
| Cancellation/deadline/transport failure | Before possible TX: `NOT_TRANSMITTED`; after possible TX without confirmation: `UNKNOWN` and `uncertain` |

Raw native abort bits remain unsigned `abortCode` with `abortValid`, separate
from signed generic `Status::detail`. Service acknowledgement, motion observation
and physical standstill are different evidence. No proof path enables, homes,
clears a fault, saves settings, stops a motor or replays an uncertain write.

Contexts own copied admitted data/results; event input is borrowed only during
the call. Terminal context evidence survives repeated inspection and release of
the owner's service-result slot. Successful explicit preparation replaces the
context; owner result release is a separate lifetime described in
[can_owner.md](can_owner.md).

## API and CLI coverage intent

The [API/CLI map](api_cli_mapping.md) remains the authoritative planned route
inventory. This proof creates no CLI parser, installed drive or discoverable
profile. N5/EPOS4 documentary fixtures remain excluded from support lists.

| Planned user route | Contract evidence present now | Implementation still owned later |
| --- | --- | --- |
| `read identity` | Typed prepare/next/advance/getter and copied result provenance | Reviewed device identity objects and real typed read coverage |
| `move absolute`, `moveto` | Exact intent, synthetic target work and retained acknowledgement/uncertainty | Actual profile staging, trigger, readiness and completion evidence |
| `axis config`, `axis origin`, `prepare` | [Production pure axis arithmetic/reference policy](axis_contract.md), consumed by the synthetic proof | Later CLI and profile/native conversion, permission and device operations |
| `velocity`, `caps` | Wide signed intent and independent capability checks | Velocity sequence, native conversion and CLI routes |

The installed/package migration belongs to prompt 7, strict documentation gates
to prompt 8, and production arithmetic to prompts 9–10. Later stages implement
their specifically assigned native operations; no synthetic success substitutes
for those obligations.
