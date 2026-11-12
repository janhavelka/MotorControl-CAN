# ESP32-S2 board evidence harness

This is the OPTAK_FGI_2025 fixture, with ESP-Prog2 UART0 on GPIO43/44,
TJA1051 CAN TX17/RX16 and active-high silent GPIO18. The reported drive nodes
are 1 and 2 at 1 Mbit/s. These are explicit fixture facts, not library defaults.
The normal installed core, application, receive owner and CANopenNode adapter
are shared with the other examples. P4 is not supported by this firmware.

Build with the pinned ESP-IDF 5.5.5 tool environment:

```text
python scripts/check_firmware.py --target s2 --idf-path PATH_TO_IDF --idf-python PATH_TO_IDF_PYTHON
```

This checks compilation and never flashes. Add `--temporary-root C:/Temp` when
that existing short path is needed for Windows compiler dependency files. For
a retained local build, from an exported IDF shell, use:

```text
idf.py -C examples/s2_bench -B .build/s2-bench-live -DIDF_TARGET=esp32s2 -DSDKCONFIG=ABSOLUTE_BUILD_PATH/sdkconfig build
```

Use the generated flash arguments with esptool, explicitly selecting the
identified board's port, S2 target and 115200 baud. The actual board's faster
460800-baud backup read failed; do not infer that speed is reliable.
No eFuse, full-flash erase or drive reset is part of the upload command.

On startup S18 is set high and CAN stays disabled. A console request is one
fixed byte; this is an evidence procedure, not the future production CLI or a
raw CAN sender. `p` returns image identity, `c` initializes the local controller
once and lowers S18, and `s` returns cached counters and stack headroom. Normal
CAN mode acknowledges received frames after `c`. It sends no device request
until an explicit acquisition command. There is no retry or automatic recovery.

| Control | Explicit finite behavior |
| --- | --- |
| `p` / runner `ping` | Cached image/base/SDK identity and local startup facts |
| `c` / `start` | One local CAN initialization attempt; lower S18 on success, no device request |
| `s` / `stats` | Cached controller/owner counters and task stack high-water mark |
| `M` | PONG `pdo=4`: one `HEAP` line, internal heap totals and regions, the boot reservation and the record arena's occupant (heap plan, below); sends nothing |
| `1`, `2` / `identity1`, `identity2` | Typed nine-field identity plan, selecting `MEASURED_U8` because `DECLARED_U16` fails on these specimens |
| `3`, `4` / `raw1`, `raw2` | Upload-only raw experiment on node 1/2, with the fixed 26-field inventory below |
| `v`, `b` / `motion-readiness1`, `motion-readiness2` | Separate finite 30-field upload-only readiness inventory on node 1/2; no motion command |
| `h`, `j` / `heartbeat1`, `heartbeat2` | Separate exact1017 experiment: require zero, write1000ms, read back; one attempt per node |
| `q`, `w` / `preop1`, `preop2` | Explicit addressed NMT Pre-operational through the same owner/TWAI result path |
| `n` / `network` | Two cached node snapshots and at most eight copied network events; no traffic |
| `m` / `tpdo1` | Node1 only: wait for qualified Pre-operational, nine exact baseline reads, six fixed mapping writes, nine exact final reads |
| `d` / `dump` | Reprint the retained record, without another upload |
| `r` / `release` | Release after output and local TX/cleanup retirement; never reopen or retry |
| `S...` / `scripts/s2_soak.py` | Finite communication soak, below; the one request longer than a byte |
| `X` / `soak-stop` | Stop a running soak after any started read settles |
| `T...` / `scripts/s2_bench.py --command readiness-actions` and the other routes | One named attended route, below |
| `A` / `trial-abort` | Withdraw the active route locally; sends no stop |
| `x` / `close-sessions` | Close the idle route sessions so that `K`, `H` and the raw and PDO experiments can reserve their nodes again in this boot (backlog S15); sends nothing, no stop or disable; drops only the latest controlwords, keeps every fence, trial identity and cap |
| `N<node>,<o\|p\|s>` / `scripts/s2_pdo.py` | One addressed NMT Start, Enter Pre-operational or Stop through the owner; resets are not offered |
| `P<seconds>`, `P0` | Watch received frames for 1..600 s at the driver tap, or end the watch; sends nothing |
| `R<node>,<t\|r>,<set>` | Nine uploads of one transmit or receive PDO set 0..3: communication `:01/:02/:03/:05`, mapping `:00..04` |
| `W<node>,<set>,<sub>,<baseline>,<value>` | One TPDO communication entry `1800+set:1/2/3/5`: exact read equal to baseline, one write, exact readback |
| `E<node>,<0\|100>` | The supported TPDO1 event-timer change on node 1 or 2: `1800:05` from raw 100 to raw 0 or from raw 0 to raw 100, through the profile's typed lifecycle (below) |
| `C<capture>,<seconds>,<frames>`, `C0` | One bounded silent capture, 1..600 s and 1..128 stored frames, with every transmission refused and S18 requested high; `C0` ends it early (below) |
| `D<o\|p\|i>,<seconds>,<select>,<expect>` | Prompt 37: the library's `DiscoveryScan` over both configured nodes within a 1..60 s window, `o` passive (nothing sent), `p` one accepted `1000:00` probe per node, `i` nine accepted identity uploads per node; then host selection of node 1 or 2 (0 none), which sends nothing; `expect` 1 (with `i` only) compares the raw identity with the retained fixture identity (below) |
| `K<node>` | Prompt 38: all eighteen typed communication read groups of node 1 or 2, 95 uploads with one reviewed grant each, no write (below) |
| `H<node>,1000` | Prompt 38: the guarded `1017:00` commissioning on node 1 or 2, exact baseline raw 0, one write of raw 1000, epoch migration, exact readback; any other value is refused as syntax and no reverse transition exists (below) |
| `o` | The shared console's premise: both nodes' bench routes configured as every command configures its node on first use, and the controller's interface facts (interface 0, 1 Mbit/s) given to the application; sends, selects and grants nothing (shared console, below) |
| `@ID ...` | One JSONL line of the shared console ([cli_contract.md](../../docs/cli_contract.md)), collected whole and judged against the bench's activity before the console sees it (shared console, below) |

While a soak runs, only `p`, `s`, `X` and a `P` watch line are served; anything else answers
`SOAK_REFUSED reason=active` (an `N`, `R`, `W` or `E` line: `PDO_LINE refused reason=active`). While a route's record is held, only `p`, `s`, `d`, `r` and `A`
are served; anything else answers `TRIAL_REFUSED reason=active`. While a capture collects, its record is held or its
transmit gate stays closed, only `p`, `s`, `n`, `d`, `r` and `C0` are served; any other byte answers `CAP refused reason=active`
(`held`, `closed`), any other line `PDO_LINE refused reason=capture`, and a soak or trial line `reason=capture`. While a
`D`, `K` or `H` record runs or is held, every acquisition, NMT, route, soak and capture is refused (`PDO_LINE refused
reason=active` for a line); `p`, `s`, `d`, `r`, a `P` watch and, once a scan has ended, `n` are served. A response that finds the console queue full waits in
one retained buffer, and no further command is taken until it has gone. While the shared console retains an
operation, traces or scans, every bench command that admits owner work is refused with reason `console` (below).

### PDO stream sessions

The `N`, `P`, `R`, `W` and `E` lines (`PdoCommand.h`) end with a line feed and use decimal numbers. `scripts/s2_pdo.py run` sends a JSON plan of them, with single-byte commands such as `c`, `h`, `j` and `r`, and keeps one capture for the whole session. Each step waits for its expected reply; a refusal or silence ends the plan, and only a running watch is then ended with `P0`. `scripts/s2_pdo.py analyze DIR` reports each received ID's frames and timing, every NMT request with its TX completion time, the first and last frame of each ID around it, heartbeat states and every PDO record read or write.

- **Watch** (`PdoWatch.h`): the TWAI tap copies every frame the adapter delivers into a 32-frame ring, without taking it from the owner, so the watch runs beside any record, NMT request or soak. SDO frames are only counted. Per ID it keeps count, gaps, mean and deviation, data changes and the last payload, and prints each frame as `PDOW_F` while the console has room. A frame's `hi` is the ISR clock sample at enqueue, an upper bound and never a bus timestamp, so gaps include ISR and service latency. `tapDropped` or `unprinted` above 0 qualifies the output. A watched frame is not a qualified PDO sample.
- **NMT** (`N`): the owner admits one request at a time, with a 250 ms queue budget and a 5 s deadline; release its `NMT request` result with `r` before the next. TX completion is local evidence; the drive's state comes only from its heartbeat or from frames that follow. NMT Stop is not a motor stop, and in Stopped the owner refuses SDO until Pre-operational or Start.
- **PDO record and write** (`R`, `W`): raw evidence experiments with one-use grants that follow the fixed experiments' grants on the same slot. Run `h`/`j` and `m` before them in a boot, or they refuse. `W` stays an isolated raw experiment; the supported event-timer change is `E`. A write changes only bit 31 of `:01`, or sets `:02` to 0..240, 254 or 255, or `:03`/`:05` to 0..65535. Each needs its PDO record read first, because an exact read of a different width would be aborted by the client and leave the node's route unresolved until an MCU restart. A write qualifies no sample and establishes no unit, persistence or drive effect.

- **TPDO1 event timer** (`E`), host-tested and replayed once on node 2 in both directions (2026-11-06, [packet 7](../../docs/reports/bench_closure_18_33/packet_7.md#bench-replay-2026-11-06)): the profile's supported production transition, run by `PdoExperiment` through `prepareTpdo1Configuration` with an `EVENT_TIMER` request, `nextPdoConfiguration`, `notePdoConfigurationTransmission`, `advancePdoConfiguration` and `getTpdo1Configuration`. It writes only `1800:05`, from raw 100 to raw 0 or from raw 0 to raw 100, in qualified Pre-operational. It waits for a node heartbeat after its reservation, then makes nine exact baseline reads (`1800:01/02/03/05`, `1A00:00..04`), one acknowledged download (`2B 00 18 05 64 00 00 00` for 100, `2B 00 18 05 00 00 00 00` for 0) and the same nine reads again. The profile refuses before the write unless the drive is a CL86-C and the baseline is COB-ID `0x180+node` valid and standard, type 255, inhibit raw 100, count 3, `60410010 60610008 60640020 0` and the other literal; any other line value (200, 10 and so on) is refused by the parser. Scope is `FIXTURE_QUALIFIED` for these two specimens. Deadline 8 s; an admitted line sets the node's evidence age policy to 3 s for the rest of the boot, as `m` does, and a refused line leaves it as it was. It can repeat after `r`. Its nineteen grants come from the `R`/`W` sequence on that slot (above 10000), and grants on a slot only increase, so after any `R`, `W` or `E` on a node these fixed-grant commands fail at their first grant on that node until the next MCU boot, with nothing sent (an `AUTHORIZATION` record, or an interrupted typed read for `1`/`2`): `1`/`2` (typed identity, grants from 1), `3`/`4` (1000), `h`/`j` (2000), `m` (3000, node 1) and `v`/`b` (4000). Run them first. A failure ends the record with the confirmed, first-effect and failed step and any uncertainty; nothing is rolled back or retried, and a restoration is a separate `E` line after a known outcome. An unresolved route is refused at the next reservation. The node's own TPDO1 route, if one is installed, is withdrawn before the write and again after the epoch advance, and the readback starts after that boundary, so the confirmed readback can be installed later. The values are raw: on these specimens raw 100 gave periods of about 101 ms on the S2's clock while Operational, and raw 0 sent no periodic frame while stationary; no unit, zero meaning or persistence is claimed. A confirmed change qualifies no PDO sample.

  The expected form of the output, not a retained capture:

  ```text
  E2,100
  PDO_CONFIG admitted node=2 raw=100; TPDO1 1800:05 raw only: nine exact reads, one write, nine exact reads; no retry
  PDO failure=0 lost=0 terminals=19 traffic=... confirmed=1 effects=1 uncertain=0 ackMask=01 cfg=2 migrated=1 gap=... node=2 transition=2 raw=100 lastStep=10 effectStep=1 failedStep=0
  RESULT ... (19 lines, the tenth the 1800:05 download), TRAFFIC ..., END PDO; map readback and sample qualification are separate; r=release
  ```

  `PDO_CONFIG refused; no work` names no reason: an active record, dump, network listing or NMT result, an exhausted identity or grant, or an invalid plan. The `PDO` header of both `m` and `E` now ends with `node`, `transition` (1 map, 2 event timer), `raw` (the requested `1800:05` literal, no unit) and `lastStep`, `effectStep`, `failedStep` (step 1 is the first write, 0 none). PONG reports `pdo=2` for this line set; `scripts/s2_pdo.py` refuses to send an `E` line to an image that reports `pdo=1`.

The raw inventory is `1000:00`, `1009:00`, `100A:00`, `1018:00..03`,
`2000:00`, `2001:00`, `1800:01/:02/:03/:05`, `1A00:00..04`, `6041:00`
`6061:00`, `1017:00`, `2100:00`, `603F:00`, `6064:00`, `2201:00` and `606C:00`. Each inventory upload has four-byte capacity, server-indicated actual size
and unknown read effects. There is no arbitrary object selector or write sender.
The separate readiness inventory is `1001`, `603F`, `6041`, `6061`, `2100`,
`2101`, `6064`, `606C`, `607A`, `6081`, `6083`, `6084`, `605D`, `60FF`,
`2200`, `2201`, `2202`, `607D:01/:02`, `2500`, `2501`, `2400:01..08` and
`60FD`. It retains exact server-indicated bytes and unknown read effects;
it does not establish a safe enable predecessor, motion scale or physical
input state. It has its own one-attempt latch per node, so it may follow a
released baseline inventory without repeating it.

Queue/response budgets are 250 ms and the inventory/heartbeat deadline is 5 seconds; the PDO deadline is 8 seconds, all
host policies. The fixture requires exclusive ownership and an externally
established fresh SDO server session. Restarting the MCU does not establish a
fresh remote session after an uncertain transfer.

The raw collector uses the ordinary owner with one exact one-use adapter grant
per field. The typed collector uses `DeviceEvidence::requestExperiment`; both
retain external review references and unknown read effects. Neither changes
the generated read policy or establishes production service qualification.
One accepted inventory consumes the node's inventory latch even on failure, shared
between its typed and raw controls. Heartbeat and PDO experiments have separate
one-attempt latches and increasing exact grant/operation identities. Release does not remove uncertainty,
reopen an SDO channel or permit repetition of that experiment.

The finite serial runner verifies the ESP-Prog2 USB identity, sets neither EN
nor BOOT, sends one byte and saves raw output plus request metadata:

```text
python scripts/s2_bench.py --port COM15 --serial 94A990398B40 --command ping --seconds 2 --output NEW_EVIDENCE_DIRECTORY
```

For an admitted raw experiment, the operator uses `start`, `raw1`, `release`,
`raw2` and `stats` as separate requests with a new output directory each time.
For example, the already reviewed node-1 request was:

```text
python scripts/s2_bench.py --port COM15 --serial 94A990398B40 --command raw1 --seconds 5 --output NEW_NODE1_EVIDENCE_DIRECTORY
```

This documents the procedure; another run still needs its actual reviewed
fixture/session and finite experiment. The runner never resets the MCU,
automatically retries a write or interprets captured text as command acceptance.

Use only the confirmed port/USB serial for this fixture. Every output directory
must be new. Capturing bytes is not an assertion that the firmware accepted the
request. Compare the reported ELF hash with the build record. The Git revision
is the build's base commit; retained source hashes identify any working changes.

Output is bounded and does not delay CAN service. Raw history holds 30 terminals,
112 traffic copies and a separate 16-entry post-terminal tail. The collector waits
for each preceding accepted TX's final report before granting another field;
the deadline stays fixed and no refused grant is replayed. Its first terminal
facts remain frozen while the tail can retain later reports/cleanup. The earlier
typed width-mismatch captures have no such tail and are explicitly incomplete
continuous bus captures.

The original 8192-byte stack was insufficient for the compiled typed acquisition
call chain. Preflight now uses owned record scratch, and the task has 12288 bytes.
The [live report](../../docs/reports/2026-11-02_s2_live_connection.md) retains the
failed reset, compiler stack facts, pacing defect and corrected runs separately.

The earlier connection checkpoint
[node-1](../../docs/bench/20261102-s2-raw-fixed-node1/rx.txt) and
[node-2](../../docs/bench/20261102-s2-raw-fixed-node2/rx.txt) records each contain
20 successful raw uploads. Both indicate one byte for `1009=0x0C` and
`100A=0x20`, versus the retained EDS U16 widths. The default typed identity path
(`DECLARED_U16`) rejects the width mismatch; an application that has established
its fixture selects `IdentityWidths::MEASURED_U8` explicitly, and nothing detects
it. Raw success does not revise the EDS metadata or establish a human firmware
version. On 2026-11-04 both specimens answered the EDS U8 counts `607D:00` and
`60FE:00` with two bytes of 0 ([trial 313](../../docs/bench/20261104T1405Z-settings2202-node1-313/rx.txt),
[trial 303](../../docs/bench/20261104T1309Z-io-node1-303/rx.txt) and their node 2
records), so the `settings-2202` route's `SOFT_LIMITS` reads and the
`readiness-io` route's `IO_OUTPUT_COMMAND` read select
`CountWidths::MEASURED_U16`; every other group keeps its declared widths. Raw `6041/6061` values establish neither
operation-mode meaning nor physical standstill.
The [post-run counters](../../docs/bench/20261102-s2-raw-fixed-stats/rx.txt)
report 40 RX/40 TX, zero errors/drops and 9852 bytes minimum remaining stack.
These runs establish the computer/probe/UART/CAN/two-drive connection, with
unknown read effects. No PDO write or motor action was sent in that earlier
checkpoint. The later [NMT/TPDO checkpoint](../../docs/reports/2026-11-02_nmt_pdo_checkpoint.md)
records heartbeat acquisition, addressed Pre-operational and the fixed TPDO1
map change with complete readback. Prompt18 is Partial; broader PDO and
motor/electrical/sustained qualification remain pending.

The later user physical power cycle and fresh readback found node1 back at
producer0/map3 without save/restore, for that one cycle only. Both nodes then
completed 30 readiness uploads. Nonzero retained `607A=5000`/`60FF=120`, mode0
and configured DI functions remain admission questions; no motion command or
blind enable sequence is implemented. No input assignment was changed.

The heartbeat experiment has three exact U16 services and stops if the baseline
is not zero. Its one download changes producer timing; this does not configure
a drive host-loss response. The effective producer fact becomes unknown before
possible TX and is published only after matching final readback. No automatic
restore or save occurs. The 1000ms value is an explicit experimental choice.

NMT uses a tagged owner transport identity and retains its own result. TX
confirmation does not confirm the remote transition. Release the result with
`r` after retaining it, then inspect `n`; a fresh post-handoff heartbeat is
required. No NMT reset/start/stop is hidden in any command.

The fixed PDO experiment (`m`) is documented in
[the reviewed procedure](../../docs/reports/2026-11-02_pdo_motion_plan.md).
Its scope remains `ISOLATED_EXPERIMENT`; only the `E` event-timer change is the
supported production subset. It preserves COB-ID, type255, raw
inhibit100, event0 and inactive slots, changes the active map from status/mode/
position to status/position, and stops at the first failure. Every queued
service carries a Pre-operational prerequisite rechecked through SDK submission.
Partial or uncertain changes are retained without rollback. Complete readback
confirms contents only; samples remain unqualified. After the epoch advance
that precedes the final reads, the harness withdraws the node's TPDO1 route once
more and reads back strictly after that boundary, so an application can install
the published readback with `configurePdo`; the bench configures no PDO route.
No motion is implemented by this harness.

The S2 reports exact capture as unknown and retains separate conservative
ingress bounds. See [the timing proof](../../docs/adapters/twai_receive_time.md).
A processing timestamp is never relabeled as arrival. A boot-up is recorded
only when an actual matching `00` event is received; absence of such an event
is not proof of a fresh server session. Network history and serial output are
bounded, with loss/truncation reported explicitly.

### Silent capture

`C<capture>,<seconds>,<frames>` ([`SilentCapture.h`](SilentCapture.h), packet 4) records copies of what the ordinary owner
receives for a stated time, into a record of stated capacity, while the tap's transmit gate refuses every submission and
S18 requests the TJA1051 Silent mode. Entry is refused unless the controller runs, nothing is queued or active, and every
slot, the NMT handoff and the tap prove local TX retirement; unknown retirement refuses. The gate closes before the silent
request. At the end the copies stop, S18 is requested low, and the gate opens only once the owner is idle and retired
again. The record (`CAP_REC`, `CAP_TIME`, `CAP_LEVEL`, two `CAP_CTRL`, two `CAP_OWNER`, `CAP_IMAGE`, one `CAP_F` per frame,
`END capture`) is printed after the end and released with `r`. `SilentCapture` is allocated from internal heap on first
use (7608 bytes on S2, measured with the pinned compiler). PONG reports `pdo=3` and `capture`, 1 while a capture
collects, its record is held or its gate is closed. The controller stays in normal mode: the record shows what the
software refused and received, never that the bus saw no ACK or TX from the S2. Lines, refusal reasons and limits are in
[hardware_bench.md](../../docs/hardware_bench.md#silent-capture).

### Discovery, communication reads and heartbeat commissioning

PONG `pdo=4` adds three lines, each one held record released with `r` and printed after its terminal through the same
dump path. [`DiscoveryBench`](DiscoveryBench.h) (`D`) runs the shared `DiscoveryScan` over the firmware's own `ProbeApp`,
owner and service store with a zero owner budget, so the ordinary firmware turn stays the only owner progression; it copies
each upload terminal and then makes the requested host selection through `ProbeApp::select`, which reads cached route facts
only. [`CommunicationReads`](CommunicationBench.h) (`K`) reads `NETWORK`, `NODE_SETTING` and the sixteen PDO records in a
fixed order, each group through `prepareCommunicationRead`, `nextCommunication`, `advanceCommunication` and
`getCommunication`, with one exact reviewed upload grant per unqualified field (Q20) from the same increasing per-slot
sequence as `R`, `W` and `E`. [`HeartbeatCommission`](CommunicationBench.h) (`H`) runs the profile's `CommunicationContext`
for the reviewed `1017:00` raw 0 to raw 1000 transition (scope `ISOLATED_EXPERIMENT`) and is the only one of the three in
the download dispatcher; it refuses before anything is sent unless the node premises of the owner's epoch migration hold
(`SdoClient::configurationEligible`): `reason=service` without ordinary service, which an accepted `D` probe or identity
scan after any `N` line provides, and `reason=transition` while an `N` line's transition is pending, which at producer
time 0 lasts its whole 5 s window. `K` and `H` refuse `reason=reserved` while an open route session keeps the axis's
reservation, until `x` closes it, and `reason=grant` if their identities are not above the channel's last grant, from
which the image continues them. The three records are 5784, 9712 and 10168 bytes
on S2 (pinned compiler, `sizeof`) and live in the shared record arena of the heap plan below. After `K` or `H` on a node the
fixed-grant commands (`1`/`2`, `3`/`4`, `h`/`j`, `m`, `v`/`b`) fail on that node until the next MCU boot, as after `R`, `W`
and `E`. Lines, refusal reasons, runner checks and plans are in
[hardware_bench.md](../../docs/hardware_bench.md#discovery-communication-reads-and-heartbeat-commissioning).

### Heap plan

The S2 internal heap is a few regions, allocated first-fit; the one after `.bss` is the only one large enough for the
route or a session. On 2026-11-09 the image at `8293b3e` refused a node-2 route after a node-1 one
(`TRIAL_REFUSED reason=memory`): after `BenchRoute` (42624 bytes) and the first `AxisSession` (35592) the largest free
block was 34816. [`BenchHeap.h`](BenchHeap.h) and `main.cpp` now (1) keep the raw, PDO, `D`, `K` and `H` collectors in one
shared arena of 23688 bytes instead of static DRAM, as only one of them ever holds a record, which moves `_heap_start`
from `0x3FFDDC20` to `0x3FFD2800` (that region from 123872 to 169984 bytes), and (2) reserve `BenchRoute`, both
`AxisSession`s, the arena and `RouteWork` once at the start of the owner task, largest first, before the console,
the controller or any command allocates. The first route then attaches both nodes' sessions and the work block from
their reservations; a refused reservation falls back to the ordinary heap on first use, as before. The soak, watch and
capture stay lazy. Replacing the arena's occupant restarts that collector's own last-identity check; the firmware's
identities still increase. `M` prints one `HEAP` line: totals, each internal region as `start:size:free:largest` in
allocation order, the reservation mask (bits route, session 1, session 2, arena, work) and the arena's occupant
(0 none, 1 raw, 2 PDO, 3 `D`, 4 `K`, 5 `H`). It sends nothing.

## Communication soak

`S` followed by `TRIAL,DURATION_S,INTERVAL_MS,SUMMARY_S,READS` and a line end starts one
finite soak. [`Soak`](Soak.h) runs in the owner task through the ordinary `ProbeApp` typed
reads and the sole owner, one finite operation at a time, alternating nodes 1 and 2. It sends
no write, NMT command or controlword, and has no raw sender or second receiver. Startup stays
passive: nothing runs before the request, and the routes are configured by the request with the
same exclusive-session premise as the other procedures. Details, record grammar and the runner
are in [hardware_bench.md](../../docs/hardware_bench.md#communication-soak).

`Soak` is allocated once, from internal heap, on the first request (10752 bytes on S2), because the
linker's `dram0_0_seg` limit has little room left. `SOAK_REFUSED reason=memory` reports a failed
allocation, and a build that never soaks spends nothing. Free heap is reported in `SOAK_BEGIN`
and every `SOAK_GAP`.

## Attended bench routes

`T` followed by a route letter and its fields starts one finite route of
[`BenchRoute`](BenchRoute.h): the packet 13, 14 and 15 reads, one `6060:00=1`, the session's
enable and stop, the isolated packet 19 position and packet 21 relative experiments, the rearm after
a named predecessor, the packet 20 move (absolute, or relative on a measured base, optionally with
the other node observed), the `000B` and words `0006`/`0007` on an evidenced released drive (packets
16 and 15), the `2202` and DI3 settings transactions (packets 12 and 13), the session's counter
zero and alarm clear, and for prompts 35 and 36 one `6060:00=3` on a drive that reports itself
released (`mode3`), the session's finite velocity run from released mode 3 with its own stop at the
due time (`velocity`, the library's attended experiment scope, which claims no start or stop
qualification, with the rest of its evidence from the host's line only), the reads `requestHome` consumes with
its refusal before any write (`home`, gate always closed), and for ledger gate Q03 one `60FF` write of a
signed value -3000..3001 with its raw readback on a drive that reports itself released in mode 3
(`velocity-target`, attended, no enable). The settings routes, `velocity`, `home` and
an observed move use one more block, `RouteWork` (8432 bytes on the S2), allocated on first use like
the route's. The
line is drained whole each turn, because it can be longer than the platform's 64-byte input ring,
and it ends in `,K` and the CRC-32 of every byte before it, which the firmware checks before parsing
(`TRIAL_REFUSED reason=crc`). `BenchRoute` and one `AxisSession` per node are allocated from internal
heap on first use and never freed; `PONG` gains `routes=8` (this line protocol), `trial` and
`motion`. While a velocity runs the firmware refuses `A`: its own stop at the due time is its only
software bound, and the attendant's cutoff is the immediate control. A run that ends needing a stop
with none of its own sent, as when word 15's reply is lost, gets one fallback QUICK_STOP (best-effort
`000B` allowed) from the route before its record ends, unless a drive fault withdrew it; after a
record whose run still needs a stop and in which no stop left, `s2_bench.py` prints
`ALERT: node N MAY BE RUNNING` and exits 4 (backlog S16, trial 1009). Routes, prerequisites, records and the runner are in
[hardware_bench.md](../../docs/hardware_bench.md#attended-bench-routes). `MotionPreview` stays an
offline review artifact, and its `executionReady` stays false.

The shared console's arming lines `TY`, `TB` and `TX` are routes of the same object, with no write; they
are described with the console below.

## Shared console

The image also carries the shared JSONL console of prompts 40 to 42
([`ProbeConsole`](../common/ProbeConsole.h), contract in [cli_contract.md](../../docs/cli_contract.md)) as a bench
facility, [`BenchConsole`](BenchConsole.h). It is one `ProbeConsole` with one `ProbeDevice` over the firmware's own
application, owner, service store and `DeviceEvidence`, wired as `EspProbeMain.cpp` wires them, and served after the
firmware turn and every bench record's, before input. It adds no receiver, sender, retry or recovery, and nothing runs
before the first `o` or `@` byte: until then the image is byte for byte the bench image above, and `scripts/s2_bench.py`
never sends either byte. `PONG` keeps `routes=8`: the arming lines below are new route letters, and no earlier line
changed.

**Allocation.** The console is three blocks from internal heap, allocated on the first `o` or `@` and never freed, like
the soak, watch and capture: `ProbeConsole` 6744, `ProbeDevice` 6664 and the rest 2672 bytes on the S2 (pinned
compiler, `sizeof`). It is split because the largest free block after the boot reservations is smaller than the whole:
21504 bytes on 2026-11-09 (`HEAP` line of `20261109T1206Z-r3-heap-boot`), and this image's `.dram0` statics end about
12 KB later than that image's (`_heap_start` `0x3FFD5850` against `0x3FFD2800`), so that block is likely near 9 KB now.
Each node's console motion attachment, `BenchAxis` (3856 bytes: its `ProbeMotion`, live route, `AxisConfig` and
`AxisReference`), is allocated on that node's first arming. After them a soak, watch or capture can refuse
`reason=memory` in the same boot, and the other way round. Check `M` first: no measurement of this image's heap exists.
A failed allocation answers `CONSOLE refused reason=memory` (`o`), `CONSOLE_REFUSED reason=memory; no work` (an `@` line,
swallowed whole) or `TRIAL_REFUSED reason=memory` (an arming).

**Premise (`o`).** The console's `host SLOT` and `useaddr NODE` select only an existing configured slot. `o` configures
both nodes' routes exactly as every bench command configures its node on first use (`ensureRoute`: the predefined
connection set, an exclusive initially unused server session that is not service confirmation, network route epoch 1),
so slot 0 is node 1 and slot 1 is node 2 (`BENCH_NODES` in [BenchNodes.h](BenchNodes.h) fixes the fixture at
these two drives; an image built with more SDO channels leaves the others unused), the same channels `BenchRoute`
opens its sessions on through
`ProbeApp::openAxis`. It also gives the application the controller's interface facts (interface 0, the 1 Mbit/s `c`
applied), which `discover` and `nmt` need. It needs `c` first and sends, selects and grants nothing:
`CONSOLE open slot0=node1 slot1=node2 bitrate=1000000; ...`, or `CONSOLE refused reason=state|route|memory|facts; no work`.
The read policy is the bench's measured identity width: both specimens answer `1009` and `100A` in one byte (packet 4,
as `1`/`2` and `sdkconfig.optak_s2_smoke` read them), so the console's identity reads use `MEASURED_U8`, scoped by
`ProbeDevice::setReadOptions` to identity reads only; budgets are the routes' 100 ms queue and 250 ms response. Every
other console read keeps its declared widths and the console's own DEC-1 gate, so for example `read state errors`
(`1001`, `603F`) is refused as Q20, as on every other image.
The device helper refuses a policy change while any read is retained, so a console allocated while a bench record
holds the application's read takes the policy at its first identity read after that record's release; until then
`read identity` and `discover identity` answer `read_width_policy_pending_release_bench_record`. Nothing else
waits for it.

**Input.** A line whose first byte at idle is `@` belongs to the console whole, in every bench state: it is collected up
to its CR or LF (drained whole each turn, as a `T` line), and none of its bytes is ever taken as a bench command. A line
of 256 bytes or more answers `CONSOLE_REFUSED reason=length; no work`, and one still open 2 s after its `@` answers
`CONSOLE_REFUSED reason=incomplete; no work`; neither reaches the console, so a partial command is never dispatched. A
complete line is then judged here (`benchConsoleGate`) and either fed to the console whole, which replies itself, or
refused with a reply in the console's own form, which main.cpp prints as a one-shot response:

```text
{"schema":"motorcontrolcan.console/1",...,"type":"reply","id":5,"command":"read","ok":false,"status":"INVALID_CONFIG","detail":0,"error":"bench_wire_work_active"}
```

**Input loss.** The 64-byte input ring drops the newest bytes when it is full, as while a retained one-shot response
waits. main.cpp then does what the console does after a loss: every buffered byte predates the loss and is retired
unserved, the line open at that boundary is dropped with its own refusal, and input is discarded through the next CR or
LF (or until 2 s pass without input). A console line answers in the console's form, correlated by its `@ID` when that
prefix arrived whole (`"command":"input"`, `"error":"input_lost"`), else `CONSOLE_REFUSED reason=input-loss; no work`;
a `T`, `S` or PDO line answers `TRIAL_REFUSED`, `SOAK_REFUSED` or `PDO_LINE refused` with `reason=input-loss`; with
no line open, `INPUT_LOST; buffered input retired through the next line end, no work`. A whole line still in the
buffer is retired with it, unanswered. A `T` refused by its first byte (`SOAK_REFUSED reason=active` or
`TRIAL_REFUSED reason=active`) has the rest of its line discarded the same way, so no byte of it is served as a bench
command. `scripts/s2_bench.py` checks `PONG` before every line, so neither changes what it sends or reads.

**Exclusion.** The sole owner is never shared between a running bench activity and console work:

- While the bench holds owner work (a route record, active or held, other than a settled arming record; a raw, PDO, `D`,
  `K`, `H` or identity record; a retained NMT request; a soak; a capture) the console takes only inspection and its own
  retained results: `help`, `version`, `profile`, `caps`, `config`, `status`, `health`, `stats`, `drv`, `memory`, `load`,
  `network`, `heartbeat`, `pdo SLOT`, `debug`/`debug off`, `result`, `release`, `cancel`, `monitor off`,
  `discover inspect|cancel` and the host intent commands (`wiring`, `prepare`, `axis config`, `speed`, `accel`, `decel`,
  `motion`, `home methods`). Everything else, `host` and `useaddr` included, answers `bench_wire_work_active`. So does
  `stop`: during a bench route its own planned stop, `A` (local, sends no stop) and the attendant's independent power
  cutoff stay the controls, as before.
- `discover` is also refused (`bench_network_listing_active`) while the bench's `n` listing runs, because both drain the
  same network observations; `n` in turn answers `NETWORK refused reason=discovery` while a console scan runs.
- `recover` is always refused (`bench_image_has_no_host_recovery`): host recovery invalidates every reservation, session
  and epoch, and this image has no recovery path.
- While the console retains an operation (from admission until `release ID`, active or terminal, delivered or not), has
  `debug raw|decoded` on (it would drain the owner's diagnostic copies that every bench record keeps) or monitors or
  scans, the bench refuses every command that admits owner work: `T`, `S`, a `C` capture, the `D`/`K`/`H`/`E`/`N`/`R`/`W`
  lines (`PDO_LINE refused reason=console`), `1` to `4`, `v`/`b`, `h`/`j`, `m`, `q`/`w` and `x`, each with its own
  refusal line and reason `console` where it has one. `p`, `s`, `M`, `n`, `d`, `r`, `A`, `X`, `P`, `C0` and `o` are served.
  A held console read also holds the application's read context, which `BenchRoute::begin` refuses (`state`) by itself.
- A gated console line other than `host`/`useaddr` is refused (`selection_changed_outside_console_select_again`) when
  the application's selection is no longer the one the console's last such line left, as after a bench `1` selected
  node 1 for its own identity read: the console never reads or acts on a node it did not select. Select again.

**Output.** The console writes its complete records into its own `ProbePlatform`; `BenchConsole` hands each record,
newline included, to the UART queue in one `writeOutput` call, exactly as every bench line is queued. A console record
and a bench line therefore never interleave: each appears whole, in queue order. A console record also waits while the
bench prints a record dump or `n` listing, or holds a one-shot response or an unreported NMT result, so a bench record's
lines stay contiguous apart from the one-shot responses the bench already prints between them (the watch's rule). The
console counts a record delivered, which `release` requires, once its newline is in the UART queue. A bench stream that
keeps the 512-byte queue full delays console records; they wait and are not lost, and the console then refuses ordinary
commands for output pressure as its contract says (`consoleDropped`). `stop normal|fast` keeps its own reserved reply.

### Console motion and arming

The console's motion routes act on one `ProbeMotion` per node, over `BenchRoute`'s own `AxisSession` of that node, with
that node's live route, `AxisConfig` and `AxisReference`. Nothing in the console acquires or invents a prerequisite: an
arming route acquires it exactly as the bench's own routes do, and its line carries every evidence value. Three arming
routes, `T` lines with the bench's `,K` integrity field, attended, reads only, no write and no motion weight:

| Line | Reads and check | Hands the console |
| --- | --- | --- |
| `TY<node>,<trial>,1,<startPosition>,<intervalMs>,<record>,<acceptSource>,<acceptMask>,<acceptValue>,<completeSource>,<completeMask>,<completeValue>,<rule>,<1000>,<1009>,<100A>,<1018:01>,<1018:02>,<1018:03>,<2000>,<2001>,<relativeBase>,<base>,<stopDeadlineMs>,<stopBestEffortMs>,<stopIntervalMs>` | MOVE's: identity, full read (two for a relative base), the move check against the reviewed 6064 after the session's acknowledged `000F` | `setMovePrerequisites` with `MoveEvidence` built as MOVE builds it (gate MEASURED, the line's record, signals, rule, raw identity and base), MOVE's budgets and age cap (3.15 s), the line's interval; a relative base's witness as MOVE's (ACTUAL: the read 6064; COMMANDED: the line's base); the stop budgets |
| `TB<node>,<trial>,1,<rpm>,<upMs>,<downMs>,<intervalMs>,<record>,<layout>,<stopKind>,<1000>,...,<2001>,<stopDeadlineMs>,<stopBestEffortMs>,<stopIntervalMs>` | VELOCITY's: identity, full read, the released mode 3 standstill check | `setVelocityPrerequisites` with `VelocityEvidence` built as VELOCITY builds it (`VelocityGate::ATTENDED_EXPERIMENT`, no start, stop, reverse or mode change claim, the line's rpm as the forward maximum, at most 60), the line's ramps, VELOCITY's budgets, enable report window and stop reserve, `maxPolls` for the experiment's 20 s maximum; the console's `normal` or `fast` must name the line's stop kind (1 HALT, 2 QUICK_STOP) |
| `TX<node>,<trial>,1,<startPosition>` | two full reads at the reviewed 6064, both idle and enabled as MOVE's relative witness requires or both released as RELEASED_QUICK requires; no `000F` needed | a stationary ACTUAL witness for the console's `stepsperturn` and `axis origin`, which consume it; no motion prerequisite |

A successful arming record attaches that node's `ProbeMotion` to the console (another node's only while the console
retains none of its motion) and prints one notice, `CONSOLE armed node=N trial=T route=arm-move|arm-velocity|arm-witness
relative=R witness=W scale=S origin=O; prerequisites named, nothing sent`, or `CONSOLE arm refused node=N trial=T
reason=...; nothing named`. An arming without `o` first is refused `TRIAL_REFUSED reason=console`. The settled arming
record holds only its dump: console work may run before `r`, and `r` may refuse while that work's frames are in flight
or the record is still printed. Every later `T` line, an arming included, needs that `r` first. A refused arming applies
nothing: its reads are not current (the console's motion lines answer `arming_reads_replaced_arm_again`), and the
attachment, the earlier arming and its stop stay as they were. Stop budgets are kept per node: an arming names only its
own node's, and a witness arming names none.
Then select the node (`@ID host 0` for node 1) and send the motion lines; the console accepts `speed`, `accel`, `decel`,
`motion write|verify`, `stepsperturn`, `axis origin`, `moveby`, `moveto`, `move ...`, `velocity ...`, `stop normal|fast`,
`result`, `release` and `cancel` on the armed node. `cancel` never withdraws a running velocity (below).

- **Age.** A move's prerequisites must be younger than 3.15 s at its admission and at each write (MOVE's age cap). Send
  the motion line right after the notice, in the same `scripts/s2_bench_console.py` invocation.
- **One use.** An admitted move or velocity consumes its prerequisites (ProbeMotion), and any later route replaces the
  arming's reads: the console then refuses a motion line with `arming_reads_replaced_arm_again`, or the console itself
  says the prerequisites are missing. Arm again for each motion.
- **Host coordinates before the enable.** `stepsperturn` and `axis origin` advance the binding's configuration epoch,
  which drops the session's latest controlword, and the session refuses a second enable once it has enabled. So set a
  scale and an origin, each with its own `TX` witness (the scale consumes the witness), before the bench's `TE` enable
  in a boot; afterwards `TY` and the motion. `moveby` and `moveto` need the scale; `moveto` needs the origin;
  `move relative N steps native RPM configured` needs neither.
- **Caps.** Each admitted console motion counts as a bench route of its kind against the per-boot caps (`motion=` in
  `PONG`, at most 12 per boot and 3 per route kind and node): a move as MOVE, a velocity as VELOCITY with its own stop as
  STOP, an enable, alarm clear or position clear as their routes. A line the caps no longer admit answers
  `bench_motion_budget_spent` before the console sees it. A console `stop` is counted as STOP and never refused for the
  caps.
- **Stop.** Before a `stop` reaches the console its prerequisites are named again: the arming line's stop budgets from
  this request (deadline, best-effort deadline, interval, as `BenchRoute::startStop` sets them) and the latest complete
  state: the session's last move or velocity cycle, else the arming's read while it stands, else the session's empty
  read, which leaves only the best-effort `000B` (give `stopBestEffortMs` in the arming line so that a stop stays
  possible on stale state). It keeps its reserved reply under output pressure. Not available while a bench route holds
  the owner (above).
- **Velocity.** A finite console velocity ends with its own stop at the due time, its only software bound. `cancel`
  (with or without its ID) is refused `not_cancellable` while the run is retained, because a local cancel would also
  withdraw that stop while the drive may turn. End a run early with `stop normal|fast`: the session withdraws the run
  and its stop first, then sends the explicit one (`test/bench_console_test.cpp` shows a stop word leaving for an
  accepted HALT and QUICK_STOP, with or without a best-effort budget, and for a refused or unparsed explicit stop).
- **Faults.** The console's motion downloads go to `ProbeMotion::preflight` first in main.cpp's dispatcher, never on to
  the route's more permissive check: more than one EMCY of that node since the arming, or one with a nonzero code,
  holds the next download back unsent and fences the session (`DRIVE_FAULT`). This is stricter than BenchRoute's own
  rule, which applies the same test each service turn and then moves its baseline: the console's baseline is set once,
  at the node's successful move or velocity arming, so two no-error (code 0) EMCYs at any time after it hold every
  later download too. Nothing is cancelled or stopped because of an EMCY between downloads: a stop stays the operator's
  explicit request, and the attendant's cutoff the immediate control.
- **Not armed here.** `enable` (its evidence expires one second after its fresh read, which no console round trip
  meets: use `TE`), `home` (no homing record exists; HOME stays closed), `alarm-clear` and `position-clear` have no
  arming; the console refuses them for missing prerequisites.

Example, node 1 released in mode 1 at 6064 0, scale and origin first (every evidence value below is a placeholder,
never a measurement; take them from the reviewed record):

```text
python scripts/s2_bench_console.py --send c --send o --send "@1 host 0" --send "TX1,501,1,0" --send "@2 stepsperturn 1600" \
    --send r
python scripts/s2_bench_console.py --send "TX1,502,1,0" --send "@3 axis origin 0" --send r
python scripts/s2_bench.py ... enable ...   # the bench's TE route, as before; release its record as before
python scripts/s2_bench_console.py --send "@4 speed 30" --send "@5 accel 500" --send "@6 decel 500" \
    --send "TY1,503,1,0,100,<record>,...,1,0,2000,3000,100" --send "@7 moveby 800" --send "@8 release 1" --send r
```

Each `r` releases the arming record before the next `T` line or `scripts/s2_bench.py` run, which checks `PONG trial=0`
first; the helper sends it only after that record's `END trial` line.

`scripts/s2_bench_console.py` sends each step only after the previous one was answered (a console line's reply and,
when admitted, its terminal; a `T` line's `TRIAL admitted` and, for an arming, its notice; a bench byte's own reply,
and for `r` first the `END trial` of a record it saw admitted),
opens the recorded ESP-Prog2 without driving EN or BOOT, never retries, and stops at the first refusal, `ok:false` or
timeout with the remaining steps unsent. `--output` keeps every received byte; `--dry-run` prints the framed bytes.

Host tests: `test/bench_console_test.cpp` (the gate table, collection, held and whole output, the premise, console reads
on bench slots, every exclusion rule above, then the arming routes and console moves through the real firmware assembly,
owner, `BenchRoute`, `AxisSession` and `ProbeMotion` against the bench route suite's synthetic drives
(`test/support/BenchDrive.h`): a relative move, the caps, the replaced arming, a stop during a move, an EMCY holding back
the first download, scale, origin, `moveby` and `moveto` in the order above, and a velocity arming from a released
mode 3 standstill with the console's finite run and its own stop); `scripts/s2_bench_console_test.py`
(the helper offline). Nothing here has run on a drive: console motion on the bench is unqualified until a bench run.
