# 09b — Host-loss supervision and reactions

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Runs after 09. NT-D08, NT-D17 and NT-D24 are central. See the repository's
[host-loss guide](../../host_loss.md) and [communication practice](../../communication_practice.md);
the Nanotec facts below are this profile's own.

## Facts to carry, not to resolve by assumption

- With factory settings no host-loss supervision is documented as active: the
  heartbeat consumer 1016h:01 is 0, the producer 1017h is 0, node guarding
  100Ch/100Dh is 0 and the synchronous window 1007h is 0 (N5 pp166-168, pp176-178).
  No documented mechanism makes the drive react when the host goes silent.
- 6007h (INTEGER16, application category, preset 1; N5 p345, rendered; C5E p343,
  CL4E p367, PD4C p332): -1 sets all RPDO inputs to 0 including 6040h, so the motor
  coasts; 0 no reaction; 1 fault with the 605Eh reaction; 2 disable voltage (coast);
  3 quick stop per 605Ah. Its function text speaks of a "SAFEOPERATIONAL" state and
  does not list the triggering events (NT-D08).
- 1029h:01 (communication error) preset 00h: go to Pre-operational if Operational,
  which stops PDOs; 01h no change; 02h Stopped. 1029h:02 (other errors) preset 01h
  (N5 p182). The NMT reaction and the drive reaction are separate objects.
- 1016h has one entry only: time in bits 0-15 (ms), producer node in bits 16-23
  (N5 p177). Errors clear when the heartbeat returns or monitoring is disabled
  (N5 p134). Whether the consumer arms before the first heartbeat, and the order of
  EMCY 8130h, the 1029h change and the 6007h reaction, are not described (NT-D24).
  Heartbeat or guarding activity in NMT Stopped is not shown (N5 p118, NT-D17).
- Heartbeat plus guarding is "not permissible"; with both set, heartbeat wins and
  the guard timer is suppressed without EMCY (N5 p132, p177).
- 3700h (following/slippage) and 3701h (limits) also default to no reaction
  (N5 p337, p66); 07 and 08 report them. FIR-v1650 documents neither 1016h nor 6007h,
  so there the setters refuse through stage 13's `FIR1650_HOST_LOSS` and
  `FIR1650_ERROR_BEHAVIOR` gates (NT-D01, NT-D31) and the snapshot reports them absent.

## Work

1. Typed read-only `Supervision` snapshot, delivered by 03 (so that 04's enable can
   report it) and consumed here: 1016h:01, 1017h, 100Ch, 100Dh, 1007h,
   1029h:01/02, 6007h, 605Eh, 605Ah, 3700h, 3701h. Every axis status and the console
   `caps` output state "no host-loss supervision active" when that is what the
   snapshot shows, and never imply that the drive stops on host loss.
2. Explicit RAM configuration operations, each `IDEMPOTENT_SETTING` with readback and
   never run at startup, selection, probe or recovery: 1016h:01 with the host's node
   ID and a time admitted by item 3's runtime producer check; 6007h in
   {-1, 0, 1, 2, 3}; 1029h:01 in {0, 1, 2}. Refuse 100Ch/100Dh while 1016h:01 or
   1017h is nonzero, and the
   reverse. Persistence of these values is 10's explicit save.
3. Host heartbeat production, seam S11. The owner produces NMT and SYNC only. A
   configured 1016h without a host producer either never arms or triggers the
   reaction, depending on the undocumented arming rule (NT-D24); neither is
   supervision. The 1016h setter therefore depends on S11's landed artifact
   (owner-produced `0x700+host` frames at an application period, with its own
   deadline, missed-service and TX-retirement rules, the producer identifier reserved
   on the interface, and bus-off or reconfiguration stopping and invalidating it) and
   its passed consumer gate. Landing is not runtime evidence: a nonzero 1016h:01 is
   admitted only with the producer's current owner status for the binding's interface
   showing it scheduled and transmitting in the current owner generation (no bus-off,
   recovery or reconfiguration since), its host node ID equal to the 1016h:01 producer
   field (bits 16-23) and reserved, and the consumer time above the producer period
   plus the application's measured worst service gap ([prompt 56](../56_host_throughput.md));
   absent, wrong-node, stale or interrupted evidence refuses with nothing sent. Two
   checks of the S11 row (canopen_shared README; canopen_shared audit application,
   round 4 theme 4) are also prerequisites of every nonzero 1016h:01 write: the
   consumer time exceeds the producer period plus S11's delivery bound, which counts
   TX queueing and arbitration delay under load and not only owner service (the
   prompt 56 gap above is one part of it), and the producer status is rechecked when
   the write is handed to the controller, so a producer stopped, interrupted or
   invalidated while the write waited withdraws it with nothing sent. Until S11
   provides that bound and handoff recheck, the nonzero setter stays BLOCKED. The
   result keeps configured, transmitting and physically demonstrated drive reaction
   separate (only 12A Q10 can show the last); no motion requires supervision
   (item 4). If no family has claimed S11, this stage claims it in the canopen_shared README table and lands it
   as one focused tested commit; if another family claimed it, the setter waits or
   extends it. Until it lands the setter is BLOCKED; no family-local transmitter.
   Node guarding would need owner-admitted RTR frames (S12), a seam meant for families
   that document guarding and no heartbeat; Nanotec documents a heartbeat consumer and
   CiA no longer recommends guarding, so this family does not claim S12 and guarding
   stays BLOCKED.
4. Reaction admission for motion: an application may require supervision before
   long velocity or torque runs; that is application policy ([prompt 73](../73_timing_freshness_guards.md),
   [prompt 74](../74_reference_self_repair_example.md)), reported through the snapshot,
   not a hidden library gate. 6007h = -1 coasts rather than ramping; report it as
   such.
5. Evidence questions for 12A: reaction order and timing after a heartbeat loss,
   whether 1029h:01 = 0 also stops the drive's reaction to RPDO loss, and whether the
   consumer arms before the first heartbeat. Until measured, results say "unknown".

## Deliver

`profiles/nanotec_canopen/Supervision.h/.cpp` with the snapshot and the admitted
setters; `test/nanotec_canopen_supervision_test.cpp`; console route
`nanotec supervision` with parity tests; S11 with its tests and its claim and landed
row in the canopen_shared README when this stage lands it; otherwise coverage rows
marking the 1016h setter BLOCKED on S11; guarding BLOCKED with S12 named.

## Acceptance

Tests cover the snapshot with factory values (reported as unsupervised), each 6007h
and 1029h:01 value, refused out-of-range values, the heartbeat/guarding exclusion,
refusal of the 1016h setter while S11 has not landed and, with S11 compiled in, for an
absent producer, a producer with another host node ID or on another interface, one
stale after bus-off or reconfiguration, one interrupted (missed service), a consumer
time not above period plus measured gap, a consumer time above period plus the owner
service gap but not above period plus S11's delivery bound, and a producer
invalidated between admission and the handoff of the 1016h write, each with zero
1016h TX; no write at startup or
recovery, and console text that never claims a stop on host loss. Quick verification
and CI on the exact final pushed commit pass. Hardware NOT RUN; no reaction timing
or order is claimed.
