# Several drives, stop-all, SYNC production and changes during motion: how drives and libraries do it (2026-11-12)

Read-only survey. No library, test, bench or other file was changed, no serial port was opened and no frame was
sent. This page extends three earlier reports and does not repeat them:

- [multi-drive preparation](2026-11-12_multidrive_preparation.md): what already exists, the bus budget, stop-all rules 1
  to 7, tests HM1 to HM9 and items M01 to M12;
- [on-the-fly changes](2026-11-12_on_the_fly_changes.md): the Lichuan set-point sources, gates Q22 to Q26, trials T0
  to T8 and items F01 to F16;
- [S2 memory](2026-11-12_s2_memory.md): bytes per axis and items R01 to R06.

M13, the requested SYNC trial, is in the [backlog](../backlog.md).

Marks:

- **D**: documented by the cited source, for its own product or stack.
- **M**: measured on the S2 bench (two CL86-C specimens with one firmware tuple; see the on-the-fly report).
- **I**: inferred here.
- **U**: not documented in any retained source.

**Only rows whose source is LC describe the Lichuan.** Every other row describes another drive or library. Those rows
are design contrast and never a CL86-C capability.

Sibling state:

- MotorControl-RS `cd27b12` is clean and unchanged since the multi-drive preparation report: same HEAD, dirty state and
  seam. It has no SYNC, periodic-producer or group-motion symbol.
  `include/MotorControlRS/profiles/ess_rs/Codec.h` line 26 defers broadcast ("Supported unicast range 1..247;
  broadcast/vendor extensions deferred"). The earlier adoptions still stand: per-axis `prepareStop` as the building
  block of a group stop, and caller-sized `BusStorage`.
- FieldCore-node `5ad9292d` is unchanged and was read only.

## Sources

| ID | Source and version | Read by |
| --- | --- | --- |
| LC | EN86/ZH86/ZH57 p3; [EDS V1.1](../reference/vendor/LC_StepDriverCAN_V1.1.eds) lines 225 (`1005`), 353-406 (`1400`), 794-847 (`1800`), 1850 (`605D`); live PDO records in the [2026-11-06 report](2026-11-06_pdo_stream.md) | Lead |
| N5 | [Nanotec N5 CANopen Technical Manual 3.3.0, FIR-v2213](../reference/multidrive/vendor/nanotec_n5_canopen_v3.3.0.pdf), PDF pages | Lead: pp119-120, 165-166. Reviewer: pp68-70, 77-78, 128-130, 345-358 |
| FH | [Faulhaber MC V3 CANopen 7000.05050](../reference/multidrive/vendor/faulhaber_mc_v3_canopen_05050.pdf), 7th edition 2024-02-02, pp17-18 and 26; [Drive functions 7000.05048](../reference/multidrive/vendor/faulhaber_mc_v3_drive_functions_05048.pdf), 6th edition 2025-05-05, pp103-107 | Lead |
| EP | maxon EPOS4 Firmware Specification, edition 2026-07, rel13740, at the URL in [drive_contrasts.md](../reference/drive_contrasts.md). SHA-256 `66fe32f15ab0bcbf9a008d0d0da8ffe395064c6716edcb3f3b28907f7160e64f`, the same file as in the 2026-11-02 audit. PDF pages | Lead: pp23-26, 70, 82, 95, 213, 241, 279 |
| EP20 | EPOS4 Firmware Specification 2020-04 rel9415 (reseller copy https://cetoni.com/downloads/manuals/EPOS4-Firmware-Specification.pdf); Application Notes 2019-11 rel8760 (university mirror) | Reviewer |
| TS | Technosoft CANopen Programming, P091.063.CANopen.UM (c) 2025, https://technosoftmotion.com/wp-content/uploads/P091.063.CANopen.UM_.1224_ESM2.pdf | Reviewer |
| AM, SY | Applied Motion 920-0025 Rev K (mirror only, secondary); Synapticon legacy profile-position page | Reviewer |
| CiA | CiA 301 v4.2.0 (third-party mirror, printed pages); CAN Newsletter 4/2024 p10, "PDO cross communication" | Reviewer |
| CN | CANopenNode https://github.com/CANopenNode/CANopenNode/blob/9b8beed8367241e96ac03f916cd5a500bcb2cf23/ (local copies of `CO_PDO`, `CO_HBconsumer`, `CO_Emergency`) | Reviewer |
| CL | CANopenLinux https://github.com/CANopenNode/CANopenLinux at `f1348d40` (canopend, CiA 309-3 gateway) | Reviewer |
| PC | python-canopen https://github.com/canopen-python/canopen/blob/14b3336bc973023e82325426adf6d71046ebcf55/ | Reviewer |
| RC | ros2_canopen https://github.com/ros-industrial/ros2_canopen/blob/e13479d1165a1f42a40da30bfbf7295b451195b5/ | Reviewer |
| LY | lely-core https://gitlab.com/lely_industries/lely-core/-/blob/88848aa28599ea5d6d7e766a9ffa2054446821b5/ and https://opensource.lely.com/canopen/docs/ | Reviewer |
| MO, SRT | moteus `8f747f4` ([transport](../reference/other_can/moteus_transport.py) `_cycle_batch`, [timeout](../reference/other_can/moteus_timeout.md)); SRT_CAN_Motor `30ec0a6` ([SRT_OpenCan.cpp](../reference/public_code/srt/SRT_OpenCan.cpp), [main.cpp](../reference/public_code/srt/main.cpp)) | Lead |
| IDF, CS | ESP-IDF release/v5.5 documentation and source `56b130f5`; embedded-office canopen-stack v4.4.0 `f4e4db8` | Reviewer |

Delegated read-only reviewers read the sources marked "Reviewer", with file, line or page citations. Not every one of
those facts was reopened for this page, so reopen the source before quoting it in a contract. The EP20 facts that
matter here were rechecked in EP and agree unless stated otherwise.

## 1. What the Lichuan sources say (LC only)

| Item | Content | Mark |
| --- | --- | --- |
| Claim | "can carry up to 32 axes for multi-axis high-speed bus synchronization control" (EN86 p3). No mechanism, object or timing is given, nor a reason for 32 | D (claim only) |
| `1005` | RW U32, default `0x80` | D |
| `1006`, `1007`, `1019`, `1800:06` | Absent from the EDS | D (absence) |
| Producing SYNC | Without `1006` the drive has no period to produce SYNC with. The host must produce it | I |
| PDO transmission types | `1400:02`..`1403:02` and `1800:02`..`1803:02` are RW U8, default 255, with no value range | D. Which values the firmware accepts or honours: U |
| RPDO event timer | `1400:05`..`1403:05` RW U16, default 1000. Live value 1000 on every RPDO of both specimens | D, M. Meaning (CiA 301 deadline monitoring or not), unit and reaction: U |
| TPDO timers | `1800:05` default 100. Live TPDO1: event timer 0, inhibit time 100. Period consistent with raw + 1 ms | D, M |
| Live RPDO1 map | `6040`, `6060`, `6098`, `607A`, unlike the EDS default | M |
| Host loss | No `1016`, `6007` or `1029`; `1017` is producer only | D (absence). Reaction: U |
| Stop objects | `605D` RW I16 default 1, with no option meanings (Q12). No `605A`, `605C`, `605E`, `6067`, `6068`, `606D` or `606E` | D |
| Modes | 1, 3 and 6 only | D |
| Set-point bits | Bits 4, 5 and 6; bit 12 in EN86 only; no bit 9; no buffer statement (on-the-fly report, section 2) | D, M, U |

On the Lichuan the host must produce SYNC. Whether the drive consumes SYNC at all is untested (M13). Every statement
below about synchronous behavior comes from other drives or from CiA 301.

## 2. (a) Several drives at once, with a stop per axis

| Source | Unit of control | Per-axis stop | Concurrency | Mark |
| --- | --- | --- | --- | --- |
| CL canopend | One gateway command per node (`[net] node r/w`), one command at a time. Node 0 exists only for NMT | None. The user writes `6040` by SDO | One command queue | D |
| PC | A `Network` dictionary of `BaseNode402` objects | `node.state = 'QUICK STOP ACTIVE'`. `CW_HALT` is defined but unused | Setters block, with fixed 0.2 to 0.8 s timeouts. Callbacks run in the `can.Notifier` thread | D |
| RC | One `Cia402Driver` per node and one `Motor402` per channel | The `halt` service actually requests Quick Stop Active. `handleWrite` sets bit 8 whenever the mode has no target | Lely loop thread. Mode and state switches block for up to 5 s inside `write()`, which probably stalls the other axes (I) | D |
| LY | `BasicMaster` maps each node ID to one `DriverBase`. Logical devices are offset by 0x800 | None: the stack has no CiA 402 code | Event loop. `AsyncMaster` posts to each driver's executor | D |
| MO (not CANopen) | `_cycle_batch` groups each cycle's commands per device into one transaction | A stop command per servo. Watchdog "timeout mode 11" is left only by a stop command | One transaction per cycle | D |
| SRT (Lichuan-mentioned, firmware unknown) | Fixed array `opencans[NUM_OPENCAN_MOTORS]`. One receive loop hands every frame to every object, and each object filters by node | `stop()` writes `0x010F` and `Estop()` writes `0x000B` | Blocking `delay(10)` between SDOs; no SDO response is checked | D (anti-example) |
| EP20 application notes | Examples stop each node with its own controlword write (`0x000B`, `0x010F`) | | | D |

**Common practice.** There is one object per axis, every motion command goes to one node, and a single bus loop
dispatches every frame. The weak point in each library is blocking: a per-axis wait inside the shared loop delays the
other axes' stops.

**For this library.** The shape already exists: one `AxisSession` per axis, one urgent slot per channel, and stops
admitted beside the other node's work (preparation report, section 1). Three additions:

1. Keep `StopKind::HALT` and `QUICK_STOP` distinct. ros2_canopen calls a quick stop "halt"; a common-layer "halt" must
   mean bit 8.
2. Make "no wait inside an owner turn" an explicit review rule for any pairing or group helper (M01).
3. The moteus cycle corresponds to M01's paired route here: compose both axes' work, then let one owner turn hand it
   off. No new core API is needed.

## 3. (b) Stop-all and group semantics

| Source | Group mechanism | When one axis or node fails | Mark |
| --- | --- | --- | --- |
| LY with CiA 302 `1F80` | dcfgen `stop_all_nodes` (bit 6) sends NMT Stop to all nodes, the master included, when a mandatory slave fails. `reset_all_nodes` (bit 4) resets them all | Without those bits: NMT Reset Node to the failing node, then automatic boot (`co_nmt_boot_req`) and NMT Start, with no application involvement (`nmt.c` L1412-1481, L2143-2181) | D |
| PC | `NmtMaster(0)` broadcast only. Heartbeats are stored, with no timeout | Nothing | D |
| RC | `Cia402System::write` loops over the axes and always returns OK. No stop-all | Nothing; heartbeat loss falls through to the Lely default | D |
| CL | NMT broadcast to node 0 only | Nothing | D |
| CN heartbeat consumer | Per-node state; on timeout an EMCY 0x8130 and callbacks | Reported only: it sends no NMT and retries nothing | D |
| MO | Broadcast ID 0x7f, at most one broadcast frame needing replies per transaction | A watchdog per servo | D |
| Shared RPDO identifier | CiA: sending one PDO to several devices is optional; "Not all on the market available CANopen devices support this optional feature" | | D |
| Drive reaction to host or bus loss | EP `6007`: 2 = disable voltage, 3 = quick stop (default 3), applied to class "a" errors, including heartbeat 0x8130 and RPDO timeout 0x8250 (EP pp213, 279). N5 `6007`: -1 zeroes RPDO inputs including the controlword (coast), 0 none, 1 fault (default), 2, 3. TS: 0 to 3, default depends on firmware | Lichuan: no `6007` and no `1016`, so U | D |

**Common practice.** No library implements a CiA 402 group stop. The group actions in these stacks are NMT stop and
reset, which are not motion stops. A drive's reaction to losing its master is a drive object, where one exists.

**For this library**, in addition to rules 1 to 7 of the preparation report:

1. **A stop set is an application-chosen list of logical axis IDs**, not "every node on the bus". The record keeps N
   per-axis results. The group counts as stopped only when every member is `OBSERVED` (rule 4).
2. **Never use NMT Stop or Reset as a group stop.** Reject Lely's default after a heartbeat timeout (reset, boot,
   start). Adopt its boot-up handling: the node is not ready, and its pending SDOs and queued work are cancelled. The
   owner's boot-up invalidation already does this.
3. **Send stops asynchronously:** by SDO today, or after M09 by an asynchronous (type 254/255) controlword-only RPDO per
   node. Never put a stop in a synchronous RPDO. Under CiA 301 that data is actuated only at the next SYNC, so a late
   or lost SYNC delays the stop (I for any drive; U on the Lichuan).
4. **No shared-identifier group RPDO on the Lichuan.** The feature is optional, undocumented for this drive (U), and
   the live RPDO1 also writes the target, the mode and the homing method.
5. **Order inside an owner turn** when a stop and a SYNC are both due: urgent stops, then the SYNC (its lateness is
   recorded), then synchronous RPDOs, then ordinary SDO (I; host policy).
6. **Host loss cannot be configured on the Lichuan** (no `6007` or `1016`). Do not document any group stop on host loss;
   a watchdog is application policy only.

## 4. (c) The MCU as SYNC producer

### CiA 301 v4.2.0 (reviewer, printed pages)

| Topic | Rule | Page |
| --- | --- | --- |
| Role | One producer and zero or more consumers; unconfirmed. The high-priority identifier keeps jitter near one frame time. How a consumer synchronizes is application-specific | 62 |
| `1005` | Bit 30 = generate, bit 29 = 29-bit frame. Bits 0 to 29 must not change while bit 30 is set. A device that cannot generate answers abort `0609 0030h` | 95-96 |
| `1006`, `1007` | Period and window in µs; 0 disables. When the window expires, synchronous TPDOs and RPDOs *may* be discarded and an EMCY *may* be sent | 96-97 |
| `1019`, counter | 0 gives DLC 0. 2 to 240 gives DLC 1 with a counter that starts at 1 and wraps to 1; 1 and 241 to 255 are reserved. The counter resets to 1 on the transition from stopped to pre-operational or operational. Changing `1019` while `1006` is not 0 is refused | 63, 111-112 |
| NMT | SYNC runs in pre-operational and operational; PDOs only in operational | 80 |
| RPDO types | 0 to 240: actuated at the next SYNC. 254/255: applied on receipt | 131, 33 |
| TPDO types | 0 acyclic synchronous, 1 to 240 every n-th SYNC, 252/253 RTR only, 254/255 event-driven. `1800:06` is the counter value of the first SYNC | 138-139 |
| Deadline | `1400:05` is optional deadline monitoring, armed by the next RPDO received; EMCY `8250h` | 132, 66 |
| SYNC loss | No rule and no EMCY code. "The SYNC consumer may use the communication cycle period manufacturer specific." `1029` does not list SYNC loss | 62, 123 |

### How the stacks produce SYNC

| Stack | Timing | Counter | Transmit failure | Window | Consumer side | Mark |
| --- | --- | --- | --- | --- | --- | --- |
| CN `CO_SYNC_process` | `timer += dt`, send when `timer >= 1006`. `timer = 0` on every send and on any *received* SYNC. The overshoot is dropped, so each cycle stretches by the call lateness; there is no catch-up. Returns `timerNext_us` | Increments and wraps to 1. `1019` is clamped at init | `(void)CO_SYNCsend`: ignored. The RPDO buffers toggle anyway | Returns `PASSED_WINDOW` once; the PDO code ignores it | Timeout after 1.5 x `1006` from the first SYNC: EMCY 0x8100. Wrong DLC: EMCY 0x8240 | D |
| LY `sync.c` | A `can_timer` firing at whole multiples of the period in network time; the application sets the time (`can_net_set_time`) | 1 to `1019` | The `can_net_send` result is ignored | A late synchronous RPDO is not latched; a TPDO sampled after the window is not sent | `OnSyncError` only on a length error | D |
| PC `SyncProducer` | `start(period)` calls the python-can `send_periodic` thread and skips `Network.send_lock` | None | Inside the thread | None | None | D |
| RC | Lely master; `bus.yml` `sync_period` (µs), `sync_window`, `sync_overflow`. Per-driver updates run in `OnSync` only with `polling: false`; the docs say the reverse | Lely | Lely | Lely | Lely | D |
| CL canopend | No command-line flag. Configure its own dictionary by local SDO: `1019`, then `1006`, then `1005 = 0x40000080` | | | | | I (not run) |

### Drives as SYNC consumers (other drives, except the last row)

| Drive | Facts | Mark |
| --- | --- | --- |
| N5 | Can produce (`1005` bit 30, `1006` in multiples of 1000 µs, pp165-166) and consume. RPDO types 0 to 240 are buffered until the next SYNC (p128). Window `1007`: "all synchronous TxPDOs are rejected and an EMCY message sent. The RxPDOs are also rejected up to the next SYNC message" (p166). Cycle phases (p120): SYNC; the slaves' TPDOs; the master's RPDOs; SDO only after the window closes. p119 prints the SYNC ID as "80+NodeID", which conflicts with 0x80; keep the conflict visible | D |
| EP 2026-07 | `1005` is RW, but its default and only allowed value is `0x80` (p70, section 6.2.4): consumer only. RxPDO types 1 or 255 (p82); TxPDO types 1, 253, 255 (p95). `60C2` 0 to 100 ms, "the time between two PDOs" (p241). 0x8250 RPDO timeout: "Interpolation aborted in cyclic mode due to no PDO received after elapsed interpolation time period", or the master aborted; the reaction comes from `6007` (p279). EP20 adds "only Sync rates of 1" and has no `1006`, `1007` or `1019` | D |
| FH | SYNC "is used to trigger synchronous PDOs and at the same time to start processes on various items of equipment". A synchronous RxPDO "is not executed until a SYNC object is received"; RxPDO types 1 to 240 behave like 0 (7000.05050 p26). Types 252/253 exist for TxPDOs only (p18) | D |
| TS | Consumer and producer. The RPDO is processed right after the SYNC. The example resends the controlword RPDO until the statusword shows the state, with a 4 ms period in CST | D (reviewer) |
| LC | Section 1: only `1005`; transmission types unknown | D, U |

Naming trap: Faulhaber's "synchronous drive" (7000.05048 sections 4.3 and 5.2.4) is open-loop stepper commutation, not
SYNC.

**Common practice.**

- One producer per network, owned by the master. The period is set once in configuration, typically in
  milliseconds outside cyclic modes.
- Consumers actuate synchronous RPDOs and sample TPDOs at the SYNC. SDO traffic follows the window.
- A synchronized start or update without CSP is a synchronous RPDO carrying the controlword (and the target), followed
  by a SYNC: explicit in FH, implied by the N5 phases.
- Every stack reviewed ignores a SYNC transmit failure, and none reports lateness.

### Recommendation

The names are candidates. They need a decision in [CONVENTIONS.md](../prompts/CONVENTIONS.md) and the API/CLI map, and
there is no RS precedent.

| Part | Proposal |
| --- | --- |
| Codec (core) | A pure `makeSyncFrame(counter, overflow, Frame&)` beside `makeNmtRequest` in `canopen/Network.h`. 11-bit `0x80` by default; DLC 0 when the overflow is 0, otherwise DLC 1. Refuses a counter outside 1 to overflow |
| State (core, owned by the caller) | A `SyncProducer` value: COB-ID, period in µs, overflow (0 or 2 to 240; 1 and values above 240 refused), wrap-safe next due time, counter, epoch, sent and late counts, skipped slots, last handoff time |
| Lifecycle | `prepareSyncProducer(config, nowUs, state&)` arms it. `nextSync(state, nowUs, Frame&)` returns "not due" or "due", with the slot and its lateness. `advanceSync(state, txReport)` advances the counter only on an accepted handoff. Disarming is explicit. After missed slots, send one SYNC, move the due time to the next future slot and count the skipped slots: no burst. Due times stay aligned to the period as in LY, not reset as in CN, which drifts |
| Owner (adapter) | SYNC is its own owner service with one reserved TX slot, ordered after urgent stops and before ordinary work. The application wakes the owner at the returned due time. No ISR or timer thread transmits frames, so there is one TX path. Each SYNC handoff time goes into the trace |
| Invalidation | Bus-off, owner rebinding or interface restart disarm the producer and advance its epoch. Re-arming needs an explicit request and starts the counter at 1. A drive boot-up does not stop SYNC, but it invalidates that node's synchronous routes |
| Conflict | A received `0x80` frame while producing is reported as a conflict. It does not reset the phase (CN does) |
| Window | The Lichuan has no `1007`. A host deadline for handing off synchronous RPDOs after the SYNC is host evidence, not drive behavior |
| Loss | A refused SYNC handoff or a bus error at a SYNC makes that cycle's synchronous RPDO effects UNKNOWN on every axis. A SYNC is never resent to "retry" |
| Never | Setting a drive's `1005` bit 30 (no `1006` on the Lichuan). Starting SYNC at startup, probe or recovery. Treating SYNC as an implied NMT Start |

**Latent command (I, from CiA 301 and the CN buffer).** A synchronous RPDO stays armed in the drive until the next
SYNC, and CN keeps the last one received. If a start was armed and its SYNC failed, any later SYNC triggers it,
including one from the periodic producer. Rules:

- In early trials, a synchronized start uses a one-shot SYNC with the periodic producer disarmed.
- After a failed or unconfirmed start SYNC, the start stays UNKNOWN, and the next SYNC is a possible motion trigger.
- Disarming needs an evidenced mechanism, either a new synchronous RPDO plus a SYNC, or an NMT change whose effect on
  the buffer has been measured. Both are U on the Lichuan.

**S2 timing and bus budget (I).**

- Timing: the owner turn is 1 ms and the largest measured service gap is 4 ms, so expect a few ms of SYNC lateness.
  Plan a period of 10 ms or more.
- Bus: with the preparation report's 1 Mbit/s worst case of 135 µs per frame, one SYNC plus one TPDO and one RPDO per
  axis is 9 frames for 4 axes (1.2 ms of a 10 ms period, 12%) and 17 frames for 8 axes (2.3 ms, 23%). SDO uses the
  remaining time.
- Owner turns: `TX_LIMIT` is 2 per 1 ms turn, so 8 axes need 5 turns per period. RPDOs only have to arrive before the
  next SYNC, so they can be spread across the period.

**Additions to M13** (no motion; a PDO reconfiguration under the prompt 18 and Q17 rules, restored without a save):

| Step | Question | Action | Outcomes that discriminate |
| --- | --- | --- | --- |
| (d) | Does a synchronous RPDO wait for the SYNC? | Idle drive, Operational. Map RPDO2 to `607A` only, type 1. Send RPDO2 with X, read `607A` by SDO, send one SYNC, read `607A` again | X before the SYNC (type ignored or applied at once); X only after the SYNC (buffered); never (refused) |
| (e) | Last RPDO wins? | Two RPDO2 frames, X then Y, before one SYNC | Y, X, or something else |
| (f) | What does `1400:05` = 1000 do? | After (d), stop RPDO2 for 3 s and watch EMCY, `1001` and `6041` | EMCY or fault after about 1 s, or nothing. A latched fault is cleared only by an explicit action |

A bare `607A` write on an idle drive is the library's ordinary pre-trigger step and starts nothing. The live RPDO1 must
not be used, because it also writes `6040`, `6060` and `6098`.

## 5. (d) Changes during motion

### Profile position set-point handling

| Source | Bit 5 | Buffer and bit 12 | Bit 9 | Profile parameters and PDO order | Mark |
| --- | --- | --- | --- | --- | --- |
| EP 2026-07, pp23-24 | 0: "Finish actual positioning, then start next positioning"; completed "as soon as the position demand value reaches the target position". 1: "Abort actual positioning and start next positioning" | Bit 12 = 1: "The previous setpoint has been assumed and no additional setpoint may be accepted". Bit 12 = 0: "Positioning to the previous setpoint is ongoing and a new setpoint may be accepted". Depth not stated | Not among the PP-specific bits (bits 14 to 9 refer to the general table) | Not stated | D |
| FH 7000.05048 pp104-107 | 1: immediate. 0: after the preceding set-point is reached, with a brief stop | Set of set-points. The master may reset bit 4 only after bit 12 = 1. "If the drive can make an advance note of further set-points, bit 12 ... is reset to 0" | 1: passes through the previous target without braking | "The set-point including the profile parameters is loaded at the rising edge of bit 4". In one PDO, `607A` is written first and the controlword is evaluated afterwards | D |
| N5 pp68-70 | 1: immediate. 0: buffered | One buffered set-point; the newest is ignored when full. A command sent while bit 12 is set is ignored. `60F2` can clear bit 4 | 0/0 stops, 1/0 passes through. Ignored if the ramp speed is not reached | Not stated | D |
| TS pp124-125 | 1 only with a linear ramp; an S-curve gives a command error. The handshake is "not necessary" with 1 | No buffer. Bit 12 clears only after the demand position reaches the target | Reserved | Not stated | D (reviewer) |
| AM, SY | AM: two set-points; bit 12 high means the buffer is full (secondary). SY: single set-point only | | | | D (secondary) |
| LC | Bit 5 sentence; arrows only from `000F` | Bit 12 in EN86 only; measured latched until `000F` | Absent | Absent | D, M, U |

Libraries:

| Library | Behavior | Mark |
| --- | --- | --- |
| RC `ProfiledPositionMode` | Bit 5 always set. When bit 12 is clear and the target changed: if bit 4 is high, drop it and wait a cycle; otherwise write `607A`, then raise bit 4. Bit 4 is dropped once bit 12 is seen. Bit 10 is unused. Targets are clamped to the integer range | D |
| PC | No profile-position handshake | D |
| LY | `TpdoEventMutex` holds acyclic and event PDOs, so a target and its controlword leave together | D |
| SRT | Writes `0x3F` (absolute, immediate) or `0x7F` after a `0x010F` HALT and an enable, and never reads an acknowledgement | D (anti-example) |

### Profile velocity and ramps

| Source | When a new `60FF` applies | Mark |
| --- | --- | --- |
| EP 2026-07 p26 | "A new target velocity is not assumed before the controlword is written". The application note writes `60FF`, then `000F` | D |
| TS | Every `60FF` write updates the demand velocity | D (reviewer) |
| N5 | Not stated | U |
| LC | Not stated (Q25) | U |

Ramps: FH loads the profile parameters with the set-point. EP gives ramps in acceleration units ([contrasts](../reference/drive_contrasts.md)).
LC gives milliseconds with an unknown formula (Q26).

**Common practice.** In profile position a change is a new set-point with the bit 4 and bit 12 handshake, and profile
parameters travel with the set-point. Bit 5 selects immediate change. Bit 12 means acceptance only; completion is
bit 10. Buffer depth is a per-drive fact. In profile velocity a change is a `60FF` write, which on some drives also
needs a controlword write.

**For this library** (extends F01 to F16):

1. **Make set-point handling a per-profile record** (candidate name `SetPointPolicy`), each field with its evidence:
   - immediate change, gated;
   - buffer depth: 0, 1, 2 or unknown;
   - bit 9;
   - what releases bit 12;
   - the outcome of a command sent while bit 12 is set;
   - object order inside one PDO;
   - how a velocity change is applied: the write alone, a controlword write, or unknown;
   - when ramps take effect: with the set-point, immediately, or unknown.

   The Lichuan values today:
   - immediate change documented, gated by Q23;
   - depth U (Q24);
   - bit 9 U;
   - bit 12 measured as latched until `000F`;
   - velocity change U (Q25);
   - ramp timing U (Q22, Q26).

   The measured latch means the generic handshake (bit 12 falls when bit 4 falls) is not established on the Lichuan,
   so the ros2_canopen edge order cannot be copied as is.
2. **Write the handshake as explicit steps per axis:** write the target, raise bit 4, drop it on bit 12, and raise it
   again only when bit 12 is low. Bit 5 is chosen per command, not always set. Out-of-range targets are rejected by
   checked quantization, never clamped.
3. **When the depth is unknown, allow at most one outstanding set-point.** A set-point sent while bit 12 is set is
   UNKNOWN, not accepted.
4. **Keep acceptance separate from completion.** Bit 12 is acceptance. Completion is bit 10 plus a window rule; the
   Lichuan has no `6067` or `6068`, so the existing `CompletionRule` stays.
5. **Periodic synchronized updates.**
   - In profile position each new set-point needs at least two observed cycles (raise and acknowledge, then drop and
     release), so set-points can change at most at half the status sampling rate (I). A SYNC with TPDO type 1 gives
     every axis the same sampling instant.
   - In profile velocity each SYNC carries one synchronous RPDO per axis with `60FF`, plus the controlword where the
     profile needs it.
6. **Change ramps in profile position through a new set-point** with new profile parameters, as FH does, never by bare
   writes during the curve (consistent with the on-the-fly report).
7. **Keep `607A` and the controlword in separate, ordered transfers** until the Lichuan's evaluation order inside one
   PDO is measured. FH documents `607A` first; the Lichuan order is U.

## 6. (e) Capacities sized to the MCU, optionally in PSRAM

| Source | How capacity is set | Who owns the storage | Mark |
| --- | --- | --- | --- |
| CN | `CO_config_t` counts with `CO_MULTIPLE_OD`, otherwise the dictionary's counts. The receive slot count `CNT_ALL_RX_MSGS` is derived | Static globals (`CO_USE_GLOBALS`) or heap through the overridable `CO_alloc` | D |
| CS v4.4 | Compile-time service counts | `CO_NODE_SPEC` takes caller arrays: dictionary, EMCY table, timer memory (`TmrMem`, `TmrNum`) and SDO buffers. The docs allow timer memory in internal or external RAM | D |
| LY | `LELY_NO_MALLOC` is "preliminary" and disables C++. `CO_NMT_MAX_NHB` is 127 | Partly the caller (`membuf_init`, `can_buf_init`) | D |
| PC, RC | Unbounded containers | Heap | D |
| RS | `BusStorage` with `pendingCapacity`, `resultCapacity` and `producerCapacity` | Caller | D |

| ESP32 fact | Content | Mark |
| --- | --- | --- |
| External RAM API | `heap_caps_malloc(MALLOC_CAP_SPIRAM)` and `heap_caps_malloc_prefer` | D |
| `EXT_RAM_BSS_ATTR` | Needs `CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY`, which makes PSRAM mandatory for boot. Without that option the attribute is empty and the object silently stays in internal RAM | D |
| Restrictions | PSRAM cannot be accessed while the flash cache is disabled, and IRAM ISRs must not touch it. Frequently used data runs near internal speed; scans over 32 KB run at PSRAM speed and evict cached flash code | D |
| TWAI node API (v5.5) | The node context is internal. The TX queue holds pointers to caller frames, which must stay valid until the transmission completes and are read in the ISR. RX happens only in `on_rx_done`, into the caller's buffer | D |
| This adapter | [IdfTwaiDriver.cpp](../../adapters/twai/IdfTwaiDriver.cpp) refuses `CONFIG_TWAI_ISR_CACHE_SAFE` and `CONFIG_TWAI_ISR_IN_IRAM` (line 17) and sets `tx_queue_depth = 1` (line 95). During a flash write the ISR waits, and only the controller FIFO buffers frames | D (code), I (consequence) |
| Bench board | ESP32-S2FH4, no PSRAM (memory report) | D |
| Arduino | `psramFound()` and `ps_malloc`. The stock build configuration probably leaves the BSS option off | D, U |

**For this library:**

1. **Carry out M04 with explicit storage.**
   - Per axis: SDO channel, urgent slot, reservation, health record, TPDO routes, a stop pair and the operation slot.
   - Per bus: one SYNC producer state, the ordinary slots and the trace.
   - Per group: N copied stop results.

   The caller passes arrays and counts (pointer and count in C++11, or template sizes) that examples can check with
   `static_assert`.
2. **Add a capacity calculator to the examples.** It takes the largest free block and the bytes per axis (the memory
   report gives 41424 today, 44848 with four TPDO routes, and 26736 or 30160 after R03) and chooses N at startup. Report
   `heap_caps_get_free_size` and `heap_caps_get_largest_free_block` for INTERNAL and SPIRAM. Reserve every axis at boot,
   as `BenchHeap.h` does, and refuse more axes than were reserved.
3. **Placement.** Internal RAM for everything the receive, transmit and urgent-stop paths touch: driver-held frames,
   route tables, the in-flight SDO, per-axis command and stop contexts, and the SYNC producer. PSRAM only for cold,
   bulky, task-context data: retained histories, provenance copies, traces, bench records and catalogue strings. Make
   one startup allocation with `heap_caps_malloc_prefer` (SPIRAM, then INTERNAL), so one image runs with or without
   PSRAM. Never use `EXT_RAM_BSS_ATTR` in shared examples.
4. **No flash or NVS writes while SYNC or motion runs on the S2 image**, unless the FIFO overflow risk has first been
   measured, because a flash write stalls this adapter's TWAI ISR.
5. **Size N by bus time as well as memory.** The per-period frame budget and the owner's turn limits in section 4 also
   bound N.

## 7. Proposed items (rename at adoption)

| ID | Item | Owner | Gate | Smallest next step |
| --- | --- | --- | --- | --- |
| Y01 | SYNC frame codec and caller-owned producer state (`prepare`/`next`/`advance`). Tests: no catch-up, counter and overflow rules, wrap-safe due time, refused handoff | New prompt after 46 (M13 design) | Name decision | Native tests |
| Y02 | Owner SYNC service: reserved TX slot after urgent stops, handoff trace, disarm on bus-off or rebinding, conflict on a received `0x80` | Owner adapter | Y01 | Fake-owner tests |
| Y03 | M13 steps (d) RPDO buffering, (e) last wins, (f) `1400:05` deadline, with a `607A`-only RPDO2 | 18 and 48, through 29 | M09 review; restore without a save | Step (d) on node 1 |
| Y04 | Latent-start rule: one-shot SYNC only, periodic producer disarmed, UNKNOWN after a failed SYNC | 45, with M13 (c) | Y03 | Contract text and host test |
| Y05 | Per-profile set-point record (section 5) with the Lichuan evidence values and N5 and EPOS4 contrast fixtures | 46, with 33 and 35 | F01 | Contrast fixture test |
| Y06 | Stop set: application-chosen axes, per-axis results, asynchronous transport only, no NMT | 45 (M02) | None | Host test |
| Y07 | Caller-owned storage counts (M04), example capacity calculator and PSRAM placement rule | 45, 46, examples | R03 for the larger counts | `sizeof` table and startup report |
| Y08 | No flash writes during SYNC or motion on the S2 image, or a FIFO measurement | Firmware examples | None | Review of the bench image's flash use |

## 8. Not established by this report

- No retained Lichuan source documents:
  - SYNC consumption;
  - the accepted PDO transmission types;
  - the meaning of the RPDO event timer;
  - the reaction to host loss;
  - a set-point buffer;
  - a velocity change rule;
  - the object order inside one PDO.
- Facts marked "Reviewer" were not all reopened here.
- Nothing was measured for this report. Y03 and M13 define the first measurements, and any result would qualify only
  the connected firmware tuple.
