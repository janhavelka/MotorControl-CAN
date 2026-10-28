# CANopen service contract

The authoritative service engine is **CANopenNode**, selected in
[the pinned fit decision](decisions/canopen_stack.md), revision
`9b8beed8367241e96ac03f916cd5a500bcb2cf23`. Prompt 6 reuses the exact vendored
sources and prompt-5 channel bridge. It adds no SDO/PDO engine or stack selector.
Its public value declarations do not include stack headers; optional application
code links the stack. Apache-2.0 notices stay with that dependency. There is no
production install/export target yet.

The [owner contract](can_owner.md) owns storage and scheduling. The
[profile contract](profile_contract.md) owns operation interpretation. A selected
service can confirm a native exchange, but cannot declare a motor stopped or an
axis healthy. Generic axis state does not acquire mandatory CANopen fields.

## Concrete request and evidence

[`ServiceWork`](../experiments/api_ownership/Contracts.h) carries a complete copied
binding, operation/step tokens, exclusive absolute deadline and either an SDO
index/subindex/payload or a raw frame. The proof owner admits only CANopen SDO
work on its fixed route. `RAW_FRAME` expresses the future separate protocol
boundary; it is rejected by this adapter before queuing. Uploads have no outgoing
payload, downloads have 1–32 bytes. A read's `mayChange` is an explicit profile
fact, never inferred from an upload or EDS access flag. The synthetic identity
read alone is declared non-consuming.

`ServiceEvidence` retains binding/tokens, owner qualification time, queue and
possible-transmission facts, protocol acknowledgement, copied upload bytes and
raw native failure data. The unsigned abort is independent of signed local
`Status::detail`; `transportDetail` carries the stack return in this adapter.

| Evidence kind | Qualification |
| --- | --- |
| `ACKNOWLEDGED` | Matching service exchange completed; never inferred from queue acceptance or CAN ACK |
| `PROTOCOL_REJECTED` | Checked matching server abort, with all unsigned abort bits |
| `REPLY_ERROR` | Native client detected an invalid response; a local abort is not a server rejection |
| `TRANSPORT_FAILURE` | Adapter failure or loss of qualified route; copied event provides reset evidence when applicable |
| `CANCEL` | Local work ended; physical motion is not stopped |
| `DEADLINE` | Immutable operation deadline reached, including time spent queued |

Any possibly transmitted unsuccessful transfer makes this proof's channel
unresolved, including an upload that may have a delayed reply. A checked write
abort preserves possible partial effects. The operation copies terminal service
evidence into its own context before the owner slot is released.

## Bridge and physical boundary

The private prompt-5 bridge owns one actual SDO client, one fake retirable TX
slot and a 32-byte FIFO. The outer owner is the sole public RX entry and holds
the canonical retained-result table. It delivers only SDO frames to the bridge;
heartbeat/EMCY/PDO frames go directly to the owner's raw observation ring, so
there is no second asynchronous event registry. A bridge terminal is copied and
released synchronously before another transfer starts.

The outer deadline governs queue time, final controller handoff and RX. The
bridge gets a non-earlier 65,535 ms timeout; this is proof host policy, not a
Lichuan timing guarantee. Admission bounds and overflow headroom are in the
owner contract. There is no retry. The upstream close call does not retract TX;
the fake bridge retires its software slot separately. Real TWAI/controller
acceptance can already make transmission possible and must preserve uncertainty
when hardware retirement is unproved.

The fake route begins externally qualified, with no earlier pending/duplicated
responses. On a successfully settled route, reuse assumes the controller/peer
does not later duplicate an old completed exchange. Local tokens/generations
cannot establish this premise or distinguish identical delayed SDO replies.
Failure quarantine is demonstrated; safe real route reuse and recovery remain
prompt 14/24/27 work, with actual queue/peer evidence. No quiet delay or fresh
host epoch is accepted as proof.

Prompt 5 demonstrates expedited and limited segmented exchanges. This bridge
does not drain a full incoming FIFO: 1–28-byte uploads are evidenced; larger
normal segmented uploads are not production-ready. Prompt 13 implements bounded
draining and the required transfer forms. Network state/heartbeat policy,
NMT/SYNC/PDO services, map qualification and bus-off recovery remain prompts
15–18/24. The proof's raw copied events are not a completed network stack port.
