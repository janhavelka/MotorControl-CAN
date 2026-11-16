# Finite host SYNC

The portable `canopen/Sync.h` codec and `prepareSync` / `nextSync` / `advanceSync`
context perform no I/O. The application supplies monotonic microseconds and owns
one `ProbeSync` over the existing `CanBusOwner`. There is no second receiver,
CANopen stack scheduler, timer, automatic retry or background producer.

## Requests and timing

A schedule has nonzero count and period, a fresh generation and a fixed origin.
Slot k is eligible in `[origin + k*period, origin + (k+1)*period)`. Preparation
checks the entire arithmetic range before publishing. An expired slot ends the
schedule; queue delay consumes its window. Neither completion nor a late turn
moves the origin. A missed slot is never sent as catch-up. The uint32 count
supports the 60,000 slots needed for ten minutes at 10 ms without retaining an
unbounded per-slot history.

The application helper retains bounded processing-time measurements for every
consumed owner terminal. `sync timing` inspects them without sending a frame.
They distinguish terminal count, actual local TX confirmations and handoffs;
the first/last handoff, minimum/maximum handoff interval, maximum lateness from
the fixed due time and maximum terminal-processing delay are microseconds.
These are host scheduling measurements, not wire jitter. Inspection and release
preserve them; only a successful new schedule resets them. A refused request
cannot erase the preceding measurement.

`ProbeSync::service` and `cancel` require time at least as recent as both the
owner's last accepted time and the schedule's last progression. Backwards time
rejects without consuming a terminal or cancelling either side. A caller cannot
report cancellation while leaving the owner request eligible to transmit.

The current owner arms only standard COB-ID `0x80`. The pure codec validates
other supported standard identifiers, but the owner rejects custom routes until
the physical network's identifier/broadcast compatibility is reviewed. No counter
byte is generated: the Lichuan EDS has no `1019`. No drive `1005` write is made.

One independent owner request, retained terminal and adapter queue entry are
reserved for SYNC. Ordinary SDO/result pressure does not consume them. Pending
urgent work takes precedence at owner handoff and again at SDK submission.
Already submitted hardware cannot be preempted. A quarantined SDO channel does
not become available because a SYNC or urgent request exists.

Queue admission establishes no transmission. `LOCAL_TX` needs a matching final
controller report; it proves neither a consumer received it nor any drive effect.
Terminal deadline/cancel/fault/conflict outcomes stay stable. Later reports retire
local storage and remain diagnostic evidence. Release cannot erase an outstanding
external queue entry or make rearming safe. Bus-off, recovery and RX loss disarm;
recovery never rearms or replays the schedule.

## One producer and receive evidence

A valid standard data frame at `0x80` with zero or one byte is a foreign-producer
conflict, even though the local codec supports zero bytes only. Its exact receive
time or interval stays as supplied; processing time is not physical arrival time.
Conflict is latched through host recovery. The application must explicitly record
resolution evidence while disarmed and retired, then separately arm again. An
identifier/byte match never establishes a self echo. The current TWAI adapter has
self-reception disabled; another adapter must provide the same contract.

This passive observation never consumes a configured native route. A custom
heartbeat or PDO at `0x80` still reaches its normal decoder and lifecycle once;
the conservative SYNC conflict stays latched and local production is refused.
In particular, a boot-up must invalidate queued work before it can transmit.

The owner retains passive SYNC observation separately from PDO observations. It
does not attach the latest host slot to the next TPDO and call it a simultaneous
sample. Lichuan supplies no on-wire cycle marker; delayed or duplicated same-ID
frames can cross cycle boundaries. Q27 requires applicable production/retirement
bounds and sample association evidence before qualified same-cycle publication.
Raw finite trial traces can compare timing, but cannot close that gate alone.

## Conditional TPDO1 type

The existing typed communication reads already cover `1005` and all eight PDO
transmission-type objects. `Tpdo1Transition::TRANSMISSION_TYPE` extends the existing
TPDO1 lifecycle with one U8 `1800:02` write, followed by all nine exact readbacks.
It accepts only the explicit opposite 255/1 transition on the reviewed original
three-entry map with event timer zero. A nonzero fixture/firmware/procedure/
authorization record and explicit experiment or qualified scope are required.
The current map/evidence is withdrawn before possible TX; partial effects stay
retained. Restore to 255 is another explicit operation. No save, NMT transition,
remap, motion or automatic restoration accompanies it.

## Buffered RPDO starts remain gated

M09 lacks a reviewed controlword-only RPDO map. Q28 also lacks measured remote
buffering, event-timer and withdrawal semantics. No start RPDO is sent by this
implementation. A later start implementation must prove every required RPDO
actually left locally before sending one SYNC and must inhibit all later SYNC
after a possibly buffered start until its remote withdrawal is proved. An
asynchronous `6040` without a start edge does not establish buffer retirement;
local recovery or a successful SDO stop is not that proof either.

The next isolated no-motion trial needs the reviewed `607A`-only RPDO2 map and
explicit target/readback plan. The common-start trial additionally needs the M09
controlword map, a bounded motion/stop plan and independent skew evidence.
Prompt56 owns the reviewed throughput changes and their timing evidence.
No slower run or fake success qualifies the requested 10 ms rate.

Prompt54's explicit request permits collecting the bounded pre-optimization
10 ms measurements now, using the existing owner and its unchanged cutoffs.
This does not claim prompt56 is implemented or close Q27. A missed slot ends
the schedule and its failed measurement is retained. Complete sample provenance
and actual wire timing remain separate evidence requirements.

### Current two-node qualification

[Prompt54](https://github.com/janhavelka/MotorControl-CAN/blob/main/docs/reports/prompt_54.md) tested the two CL86-C specimens on the S2 image
identified in its manifest. After correcting diagnostic output throughput, the
short 10 ms/500-slot capture passed without receive-record loss. The requested
1800-second run stopped at 42988 of 180000 slots with `MISSED_DEADLINE`: the
application consumed a confirmed result 724 us beyond its fixed slot cutoff.
All 42988 TPDOs from each node were captured; this is not a 30-minute pass.

A separate 100 ms/600-slot (nominal 60-second) baseline passed on the same image.
That is the longest tested passing period and duration, not a general maximum
rate or long-duration qualification. Measured values describe owner processing
and S2 receive bounds; wire occupancy, same-cycle sampling and physical skew
remain unmeasured. Prompt56 owns the phase-cost and scheduling follow-up before
the same 10 ms sustained request is repeated.
