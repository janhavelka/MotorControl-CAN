# P54 bounded diagnostics

This note specifies the implemented S2 diagnostic tools. It is not a record of
successful hardware qualification. Read [report54](../../reports/prompt_54.md)
and the [fixture/campaign record](README.md) for the current physical boundary,
including unresolved node history. No command below bypasses that boundary.
The [S2 guide](../../../examples/s2_bench/README.md) owns the other bench commands.
Current image contracts are `PONG routes=15` and `pdo=7`.

## One 30-minute, 10 ms schedule

First establish the actual board/image, released stationary drives, fresh
configuration and Operational heartbeat evidence, both TPDO1 transmission types
1, and an exclusive SYNC producer. A historical map or a cached selected node is
not fresh evidence after a power cycle. The runner neither configures those
premises nor clears quarantine. Do not enable debug raw/decoded output or run a
flash operation alongside this capture. The console and existing CAN owner are
the sole serial/receive path; do not open a second receiver or port session.

A separately identified raw-traffic measurement can preserve node1's unresolved
stop without any SDO, RPDO, NMT or motion sent to it. The lead reviewed its last
type1 map, no subsequent map/RPDO writes and the single-producer fixture. This
does not establish released/standstill state or fresh map qualification. A boot
invalidates the historical-map premise; retain raw frames without decoding them
as qualified observations until current configuration is established.

Offline plan inspection, with no port access:

```text
python scripts/sync_watch.py --dry-run --duration-s 1800 --period-ms 10
```

After the lead has established the fixture, use a new output directory and the
exact lowercase SHA-256 from the intended image's `PONG elf=` field. For example,
in PowerShell (`$elf` must be assigned that verified 64-digit value):

```powershell
python scripts/sync_watch.py --port COM15 --expected-elf $elf --duration-s 5 --period-ms 10 --output docs/bench/20261116-p54/short-sync
python scripts/sync_watch.py --port COM15 --expected-elf $elf --duration-s 1800 --period-ms 10 --output docs/bench/20261116-p54/continuous-sync
```

These are separate explicitly requested tests, not retries. A failed short test
must be investigated before choosing the long test. The tool checks the fixed
ESP-Prog2 USB identity (303A:1002, serial `94A990398B40`), the expected ELF,
`pdo>=7`, running/active controller and no active soak, trial, watch or capture.
It sends `o`, selects `host 0`, turns debug off, requires an idle/disarmed
non-conflicting SYNC producer, and takes baseline counters. It then sends
`P1810,c`, waits for its BEGIN, explicitly arms SYNC and requests
`sync period 10 180000`. It reads the matching retained terminal, cached
`sync timing` and final counters. On a checked native failure with a healthy
stream it still gathers the planned cached diagnostics, disarms locally and
ends the watch with `P0`; it retains the failed native result. On success it
releases the exact native operation before disarming. It never replays a
schedule, recovers the owner, changes a PDO, sends NMT, enables or moves a drive.
A damaged/disconnected stream sends no further commands; the native schedule
remains finite. Local disarm and `P0` are not motor stops.

The CLI permits duration 1..3590 seconds and period 10..1000 ms, with an exact
integer slot count. Watch duration is the requested duration plus 10 seconds;
the fixed outer host deadline is run entry plus duration plus 30 seconds,
including setup and finalization. Each native slot uses the original schedule
origin; a missed slot ends the schedule without catch-up. The 180000 windows
span 1800 seconds; first-to-last handoff spacing would be 1799.99 seconds at an
ideal 10 ms cadence. Actual retained software times determine what occurred.

The new directory contains `request.json`, exact streaming UART `rx.bin` and
`summary.json`, including partial evidence on failure. The raw file is capped
at 64 MiB; lines at 2048 bytes; the pending line queue at 512; cached console
records at 32; handoff records at 512; counter snapshots at 400; and per-ID
metrics at 16 IDs with fixed eight-bin histograms. It does not retain an
unbounded Python frame list. Counter snapshots are requested every 10 seconds.

## Compact PDO watch format (PDO protocol 7)

`P<seconds>` preserves verbose `PDOW_F` output. `P<seconds>,c` selects compact
records; either form permits 1..3600 seconds. `P0` ends the local watch and sends
nothing on CAN. The compact header gives the fixed decimal microsecond origin:

```text
PDOW_BEGIN n=0 t=<output-turn-us> seconds=<seconds> ring=32 ids=16 compact=1 origin=<begin-us>
PC <n>,<id>,<offset>,<width>,<meta>,<payload>,<crc>
```

Every PC field except payload/CRC is canonical lowercase unsigned hex without
`0x` or leading zeros (zero is `0`). `n` is uint32 and shares one global watch
line sequence with decimal `n` fields in BEGIN/SUM/ID/END. Non-watch console and
STATS records do not consume it. `id` is a full Classical CAN identifier, at
most 29 bits; a standard frame is at most 11 bits. Offset and width are full
uint64 values, not truncated or saturated clock deltas.

| Meta bits | Meaning |
| --- | --- |
| 0 | Extended identifier |
| 1 | RTR |
| 2 | Timed |
| 3 | Exact bounds |
| 4 | Upper bound precedes BEGIN origin |
| 5 | Reversed bounds: lower bound exceeds upper bound |
| 6..9 | DLC, 0..8 |
| 10 and above | Invalid, rejected |

For timed frames, `hi = origin - offset` when bit 4 is set, else
`origin + offset`; `lo = hi + width` when bit 5 is set, else `hi - width`.
All arithmetic must remain in uint64. Reversed bounds are retained raw evidence
and invalidate timing interpretation. Exact requires timed, width zero and no
reversed flag. Untimed requires zero offset/width and no exact/sign/reversed
flags. Sign with zero offset or reversed with zero width is invalid. Each row
is independently decodable from BEGIN; losing one row cannot shift later times.
Payload is exactly two lowercase hex digits per DLC byte for a data frame;
zero-length data and RTR use `-`. RTR carries its requested DLC, without data.

CRC is four lowercase hex digits of CRC-16/CCITT-FALSE: polynomial `0x1021`,
initial `0xffff`, no reflection and no final XOR. It covers the exact ASCII
bytes beginning `PC ` through the payload, excluding the final comma, checksum
and newline. For origin 1000000, this valid fixture row has lo 1000400,
hi 1000500 and seven data bytes:

```text
PC 1,181,1f4,64,1c4,01020304050607,24e8
```

`PDOW_SUM`, `PDOW_ID` and `PDOW_END` retain their verbose schemas. Standard SDO
data IDs 581..5FF and 601..67F are counted as `sdo`, without raw frame rows.
The 32-entry tap ring and 32-entry print queue are bounded. On compact finish,
new copies stop and queued frames drain for at most five seconds; undelivered
frames then count as `unprinted`. END itself still needs output room; the host
outer deadline fails if no complete END arrives. No output block extends the
schedule or permits another producer request.

## What the evidence can establish

At 115200 baud, 8N1, the physical UART ceiling is 11520 bytes/s. Two 100 Hz TPDO
streams approach that ceiling even with compact ASCII, before heartbeats,
summary and counter output. Measure actual raw-byte rate, service gaps, output
losses and controller counters. The compact format is not a throughput promise.
Verbose frame output and debug tracing are unsuitable for this sustained plan.

The retained [short 10 ms attempt](33-sync-10ms-short/summary.json) confirmed
500 local SYNC transmissions and native counts of 500 TPDOs from each node, with
no reported CAN receive loss. It failed raw capture: 1011 watched frames,
737 printed, 274 unprinted, tapDropped 0. The running summaries each account for
32 queued frames (`frames - printed - unprinted`); loss accumulated while that
print queue stayed full. The 737 compact rows used 33342 bytes, about 45.24 bytes
per row. Approximately 202 frames/s therefore needs about 9.1 kB/s, before the
other console output. This is a measured diagnostic failure, not a passed
10 ms qualification or a missing-wire-frame claim.

The watch already retains a selected line when its atomic sink refuses it.
The bottleneck was the ordinary platform's 16-byte output budget at roughly 2 ms
application turns: an 8 kB/s upper bound before other work. The diagnostic
correction leaves that default unchanged and permits at most three additional
16-byte output-only drains per S2 turn while a compact watch is active. It
stops on no progress or empty output, adds no input/idle/CAN calls, increases no
buffer and never waits for UART space. A native test through actual
`ProbePlatform` at 2 ms turns and a conservative 23-byte UART allowance reproduces
77 drops from 1010 frames with the 16-byte budget and zero with the 64-byte maximum;
a sustained blocked sink still loses 1000 frames with either budget and retains
that loss in END. This proves the scheduling mechanism in a host model only.
The corrected image still needs a fresh short capture before a 30-minute attempt.

The runner keeps console sequence/CRC failures, `unprinted`, `tapDropped`,
`sdo`, untimed/reversed/backwards timestamps, ID overflow and controller
`dropped/errors/txRej/rxLost` changes separately. Counter saturation also fails.
A TPDO count different from the planned SYNC count is a coverage failure; it is
not alone proof of lost UART records, lost wire frames or drive sampling loss.
`raw_rx_complete`, timing completeness and count coverage are separate claims.
`observed_bounds_valid` describes valid timestamps in the retained prefix;
`timing_complete` additionally requires complete raw reception, including END
and zero raw-capture loss. A valid prefix cannot qualify complete timing.
`ownerGap` is a cumulative high-water mark, so its baseline may predate this run.

RX lo/hi are driver observation bounds, not physical sample or wire timestamps.
For consecutive valid observations the interarrival interval is bounded by
`[max(0, lo_i - hi_previous), hi_i - lo_previous]`; hi-gap statistics include
ISR/FIFO/service delay. Receive order, matching counts or proximity to SYNC
cannot establish same-cycle identity, synchronous sampling or mechanical skew.
TWAI self reception is disabled: an observed received SYNC is a foreign-producer
conflict, not an inferred echo of local SYNC.

TX evidence is aggregate, not a raw record per SYNC. Cached `@ID sync timing`
returns `generation/count/terminals/confirmed/handoffs`, decimal-string
`origin_us/period_us`, `handoff_us=[first,last,minGap,maxGap]` and
`delay_us=[maxHandoffMinusDue,maxTerminalProcessingMinusHandoff]`, all in us.
First/last need a nonzero handoff count; gap extrema need at least two.
`invalid=true` withdraws timing interpretation. Application-owner handoff occurs before the transport callback, not at driver
acceptance; SDK deferral and service delay can follow it. Terminal processing
is also a software timestamp. Neither is the physical transmit time. Release preserves timing; the next successfully prepared schedule resets
it. No measured wire-bus utilization or per-frame TX/RX pairing is claimed.
Q27 same-cycle provenance, M09 controlword-only mapping and Q28 buffered-trigger
withdrawal remain independent gates.

## Explicit console change experiments

Route protocol 15 adds optional final `allowChanges` (0 or 1) to complete `TY`
move and `TB` velocity arming lines. Omission is closed, preserving old grammar;
`TX` witness arming has no such field. Ordinary arming closes both change gates.
The helper's `--allow-changes` appends `,1` before CRC32 framing. An explicit
flag must agree with the option, and using the option without a complete TY/TB
step is refused before I/O.

This executable dry-run uses the native test's synthetic identity and signals,
not specimen evidence. It only prints framed bytes:

```text
python scripts/s2_bench_console.py --dry-run --allow-changes --send "TY1,7,1,0,100,19,1,4096,4096,1,32768,32768,1,262546,18,52,768,34309,65537,134,291,1,0,2000,3000,100"
```

Live use needs the complete fresh reviewed line, original arming age/generation,
per-axis prerequisites and actual returned operation identity. The notice reports
`changes=1 changeRecord=<trial>`. The evidence gate is ATTENDED_EXPERIMENT with
that trial ID and exact model/firmware; it sets no measured capability flag.
For velocity the line's rpm is also the permitted positive maximum. The flag
does not establish a command/feedback relation; endpoint changes still need the
ordinary reviewed relation and remain refused without it. Same-endpoint speed
and ramp changes remain subject to the native evidence and phase checks.

Use the retained operation ID with `velocity change ID ...`, `move speed ID ...`,
`move ramps ID ...` or `move retarget ID ...`, as specified in the
[CLI contract](../../cli_contract.md#changes-during-a-retained-motion-operation-prompt52).
The helper normally waits for a motion terminal before its next step;
`--no-terminal` permits admission-only sequencing, but is not a timing scheduler.
An attended change plan must keep one live owner, use actual receipt IDs, reserve
its original stop and end deadline, and never replay an uncertain write. Arming
or a successful change cannot promote experiment evidence to qualification.

Protocol 14's separate routes remain available under protocol 15: `mode-probe`
(`Tm`) writes one explicitly named 6060 candidate 2/4/7/8/9/10, or 1/3 solely
for explicit restoration, after two fresh released standstill checks; no enable
or implicit restoration occurs. `position-change` (`Tp`) selects one of eight
forward-only experiments: bare 6081, bare 6083, bare 6084, falling bit 4, same-endpoint
speed with 003F, immediate absolute retarget with 003F, buffered retarget with 001F,
or bare 607A. It requires positive measured 606C at the actual write and an
immutable admission-anchored stop plus nonzero fallback. `velocity-change`
(`Tv`) requests one positive replacement at a fresh complete RUNNING-cycle
boundary, preserving its original preparation-time stop and final deadline.
Both rates are at most 60 rpm; changeAfterMs is 1..19999 and before stop due.
These routes retain their raw sequential cycles and qualify no broader feature.
The durable campaign still caps total motion 12 and each route/node 3; position
change caps are separate per case, and mode probes per candidate, with no
implicit cap reset, refund or retry.

## M14: velocity with addressed NMT Stop and Pre-operational

`--command velocity-nmt` produces protocol 15 `Tn`, using the ordinary finite
velocity line's fields and CRC. Its host policy requires 6..30 rpm, interval
4000..5000 ms, QUICK_STOP, no reverse/range probe, nonzero trial at most
2147483647, and the ordinary 20 s experiment maximum and 128-terminal bound.
A plan accepted by the parser is still refused by the native experiment if the
required fresh running gap never occurs.

The following dry-run is syntactically complete but uses synthetic identity,
not measured specimen data. Replace all evidence, trial/record IDs, node and
campaign path with their actual reviewed values before considering live use:

```text
python scripts/s2_bench.py --port COM15 --serial 94A990398B40 --command velocity-nmt --node 1 --trial 54001 --rpm 6 --ramp-up-ms 200 --ramp-down-ms 200 --duration-ms 13000 --interval-ms 5000 --deadline-ms 24000 --kind quick --record 54001 --layout zh86 --device-type 262546 --hardware-version 12 --software-version 32 --vendor-id 4096 --product-code 1 --revision 0 --model-code 1024 --firmware-code 512 --campaign campaign.jsonl --output unused-dry-run --dry-run
```

Live omission of `--dry-run` uses the existing attended confirmation or explicit
recorded user waiver, requires a valid persistent campaign, and creates a new
output directory. It does not silently select a fresh campaign. The ledger
charges the velocity and its planned stop before handoff (two motion weights).

The helper offers exactly once, at a fresh complete forward RUNNING cycle in
the native WAITING phase with acknowledged 000F and fresh Operational evidence.
At offer time it freezes STOP end at +1500000 us and PREOP end at +3000000 us;
both stages must fit strictly before the existing next-poll time. At least
seven service budgets must remain between that next poll and original stop due.
Each addressed request has a 100000 us queue budget and its stage's fixed end;
request IDs are 2*trial and 2*trial+1. No broadcast, retry, NMT Start/reset,
implicit enable or schedule extension occurs. Missing Stopped observation can
still lead to the distinct preplanned PREOP after the STOP local request has
retired; this is not a repeated STOP. A missing terminal or lifecycle failure
keeps the first failure and uncertainty.

`TRIAL_NMT` retains offered/phase/failure and fixed start/stopEnd/preopEnd/resume/
due/end times. `TRIAL_NMT_STEP` separately retains both local request terminals
and qualified heartbeat lower bounds; `TRIAL_NMT_SAMPLE` retains the before and
first post-PREOP sequential seven-field native cycles. These are separate from
the velocity and CiA402 stop results. NMT Stop is not a motor stop. The original
velocity stop due and final deadline never move, and no successful helper
record overrides a failed native stop. No M14 physical result is asserted here.
