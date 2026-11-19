# 06 — Velocity, profile torque and cyclic modes

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Runs after 05 (06c also after 09 and 09b, and only with seams S13 and S14). Three blocks, each delivered, tested and committed alone. Torque
and cyclic operation are extension scopes under the
[multi-drive decision](../../multidrive_decision.md): native typed APIs only, no common
effort or streaming API unless a separate assignment adds one with units,
deadlines, watchdog and measured transport requirements.

## Work

### 06a Profile Velocity and Velocity mode

1. PV (6060h = 3; N5 pp77-80) maps to the common finite `prepareVelocity`. Target
   60FFh is I32 in 60A9h user units; ramps 6083h/6084h in acceleration units, never
   milliseconds; jerk 60A4h; 607Eh bit 6 inverts the sign. Convert through
   `prepareVelocityTarget` and the 05 relation; refuse an unrepresentable basis.
2. Evidence: bit 10 with halt means target reached (606Dh window, 606Eh time) or
   standstill; bit 12 means speed zero (606Fh/6070h); bit 13 is slippage (60F8h,
   preset 400, over 203Fh; N5 p409). A speed-reached bit is not measured shaft speed.
3. Finite duration is host intent, a timed run under canopen_shared 02 "Operation
   admission" (the Lichuan finite-velocity design: nonzero `durationUs` from
   successful preparation, refused at preparation unless duration plus the caller's
   stop budget fits before the one operation deadline, every later transfer bounded
   by the stop due time, `needsStop`, phase `STOPPING`). At stop due the run issues no
   further work and the application composes its urgent stop: 04's `HALT` (010Fh
   with 605Dh, 6061h 2-4) or `QUICK_STOP` (000Bh or 010Bh by the fresh bit 8,
   605Ah), a separate `NOT_REPEATABLE` operation with its own deadline that the run's
   deadline, identity outcome and a missing acknowledgement never bar. Both rows exist
   for every state this run can reach (Operation enabled, halted or not, in 6061h
   2-4); a run whose mode or state has no such row is refused before its first
   changing TX. Those rows need fresh SDO evidence, so they are not a stop path once
   a lost start reply leaves the route unresolved (S10 `NONE`, the Nanotec policy
   until 02b's markers are reviewed). The run is therefore also admitted only with a
   stop that can still leave then, as canopen_shared 02 "Operation admission" (timed
   runs) requires: the binding's opted-in S3 shape (04 item 8; S3 is opt-in per
   binding, so stage 04 having landed does not supply it) or an independent stop the
   application declares for the run (its own enable, STO or power path), named in the
   result. Without either, it is refused before its first changing TX. The run also
   passes 07 item 8's limit admission. `needsStop` and UNKNOWN execution stay while
   any start word may have left. A deadline never stops the drive by itself. No
   repeated start, no replayed stop.
   Follow [prompt 59](../59_velocity_range_direction.md) for range and direction
   facts, measured per profile.
4. VL (6060h = 2; N5 pp76-77): 6042h is I16 rpm unless 604Ch changes it; ramps
   6048h/6049h are delta speed over delta time; 6046h limits; 604Ah quick stop. A
   native typed operation only; mapping it to common velocity needs its own
   qualification of 604Ch and the gear relation.

### 06b Profile Torque (closed loop only)

5. PT (6060h = 4; N5 pp80-82): 6071h target, 6072h maximum torque, 6073h maximum
   current, 6074h demand and 6087h slope, all in thousandths of the torque that
   corresponds to the rated current 203Bh:01 (N5 p81, p367). The effective limit is
   the lower of 6072h and 6073h, capped by 2031h. No N·m conversion exists in the
   manuals or EDS, and 6076h is absent; report per-mille values only. The 6072h
   preset 0064h is 10 % and limits PT by default (NT-D20): report it, never raise it
   implicitly.
6. Admission: declared closed loop with a completed auto setup or the PD4-C factory
   record, `Capabilities.torque` true, exclusive control, 07 item 8's limit admission,
   a timed run with its urgent stop and unresolved-route stop as in 06a item 3, and
   active closed loop: a fresh 6041h in Operation enabled
   with bit 15 = 1 (N5 p348), sampled in the current server session after the last
   boot-up and after the last write to the motor and feedback objects (3202h, 3203h
   and 03's motor group), immediately before the mode change to 4 or the first nonzero
   6071h. Entry order from Operation enabled: that bit 15 check, a fresh 6071h that
   reads zero (6071h is savable, 10 item 6, and no page documents what an enabled mode
   change does with a retained target; a nonzero or unread value refuses with nothing
   sent and the operation writes no 6071h before the mode change), 6060h = 4 as the
   first changing step, 6061h = 4 confirmed, then the first nonzero 6071h. A lost
   6060h reply leaves the mode UNKNOWN and ends the operation without replay (04
   item 6). This check also governs the public mode transition: 04 item 6's host mode
   table admits mode 4 only inside this operation and refuses enable with 6061h = 4,
   so the generic mode select cannot bypass it. Configuration alone does not suffice: with encoder commutation and no auto
   alignment the drive stays in open loop until the index passes (N5 p53), and bit 15
   is meaningful only in Operation enabled. Absent, stale or contradictory evidence
   refuses before TX. 3202h bit 5 = 0 is torque-limited velocity with 6080h as
   the speed limit; bit 5 = 1 is torque control without a speed limit (N5 p81, p298)
   and is refused in this subset. Target reached is 6077h within 203Dh for 203Eh
   (N5 p80). A current limit is not torque control.

### 06c Cyclic synchronous and interpolated modes

7. CSP, CSV, CST (6060h = 8, 9, 10; N5 pp90-94) and IP (7; N5 pp89-90) need a SYNC
   period of whole milliseconds (60C2h:02 = -3, 60C2h:01 U8, preset 1; N5 p92,
   pp394-395), RPDO setpoints in NMT Operational (09) and a lease contract for
   refreshing current intent that never retriggers a move (multi-drive decision). In
   CSP, CSV and CST statusword bit 12 means "target followed" (N5 pp90-94). IP differs
   (N5 p89): bit 4 activates interpolation, bit 12 means "IP mode active", bit 10 is
   target reached or speed 0 under halt; no completion rule is shared with the cyclic
   modes. IP takes one buffered target in 60C1h:01
   with 60C4h:06 = 1. CST needs closed loop; CSP, CSV and IP are not advisable in
   open loop (N5 p46).
8. Execute only with an explicit assignment that also delivers 09, 09b, the
   [prompt 51](../51_sync_producer.md) SYNC producer integration under this family's
   seam S13 entry (09), the owner-admitted RPDO data transmission of seam S14
   (canopen_shared 02 "CANopen-only rules": route, map and identity binding, SYNC
   coordination, lease expiry, stale setpoint retirement, urgent stop by SDO, bus-off),
   landed or consumed with its consumer gate passed, plus a measured
   service budget ([prompt 56](../56_host_throughput.md)). No family-local raw
   transmitter and no S9 raw transmit kind substitutes for S14. Otherwise this block is
   delivered as ledger and coverage rows with `UNSUPPORTED` capability reasons and
   no API, and 04 item 6's host mode table keeps modes 7-10 unavailable for mode
   select and for enable with one of them active.

## Deliver

`profiles/nanotec_canopen/Velocity.h/.cpp` with `VelocityContext` (06a);
`Torque.h/.cpp` with a native `TorqueContext` (06b); `test/nanotec_canopen_velocity_test.cpp`
and `test/nanotec_canopen_torque_test.cpp`; console routes `nanotec velocity` and
`nanotec torque` with parity tests; coverage rows for 06c.

## Acceptance

Tests cover start and stop frames, direction and range at I32 and I16 limits,
ramps in acceleration units, lost start, lost poll and lost stop, the shared timed-run
cases (a duration that no longer fits with the stop budget refused at preparation, a
lost start reply, an observation in flight near stop due, service resuming after stop
due, a queue-pressured urgent stop, the halted and unhalted stop words), the
unresolved-route branch (with S10 `NONE`, neither the binding's S3 shape nor a
declared independent stop: refused before the first changing frame; with the S3
shape opted in: the start reply lost, the ordinary run retired, at most the one
reviewed 010Bh best-effort frame sent, uncertainty kept and no standstill claimed;
with a declared independent stop: admitted and named in the result), 07 item 8's
limit refusals on each run route with zero TX, priority
stop, unknown terminal motion, PT selected through the generic mode select refused,
PT refusal in open loop or
with 3202h bit 5 = 1, PT entry with a nonzero or unread retained 6071h refused with
zero TX and a lost 6060h = 4 reply ending UNKNOWN without replay or a 6071h write,
PT on an enabled, configured closed-loop axis refused before
and admitted after a fresh bit 15 = 1, and refused again with that evidence stale
after a boot-up or a motor-settings write, per-mille limits and the 10 % preset
reported unchanged. Quick
and full verification and CI on the exact final pushed commit pass per block.
Hardware NOT RUN; no unbounded velocity or torque test is implied by any block.
