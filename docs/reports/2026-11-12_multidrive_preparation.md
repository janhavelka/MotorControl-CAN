# Multi-axis preparation: several drives at once from one application

Date 2026-11-12. Read-only investigation on `main` at `d8fa2f3`. No code, hardware or other files were changed. The
unrelated working-tree changes present at the start (`scripts/console_jsonl.py`, new bench scripts and tests,
`docs/audit/PROMPT_43_SOURCES.md`, `.vscode/`) were not touched. The S2 heap is measured separately in
`2026-11-12_s2_memory.md`. This page quotes only the sizes already recorded in earlier reports.

"Multi-drive" here means **several axes running at once from one application**, for example both nodes moving
together, a coordinated start or stop, per-axis status and a stop of all axes. The project's scope document uses the
word for **multiple manufacturers**. That meaning is covered briefly in the last section.

Sibling state: MotorControl-RS `cd27b12`, clean. FieldCore-node `5ad9292d`, clean, read only for the motor-module
boundary.

## 1. What exists today

### Already built for several axes

| Layer | Symbol and file | Capacity or behavior | Evidence |
| --- | --- | --- | --- |
| Core identity | `Binding::logicalAxisId`, `Endpoint{bus, profile, address, channel, subaxis}` in [Endpoint.h](../../include/MotorControlCAN/Endpoint.h) | The logical axis is independent of the node ID. There is no common 1..127 range, and a subaxis selects an axis inside one endpoint | `protocolNeutralBindingAndFrames` in [multidrive_contract_test.cpp](../../test/multidrive_contract_test.cpp) |
| Adapter | `SdoClient::CHANNELS = 2`, `PDO_ROUTES = 8`, `NETWORK_HISTORY = 4` in [SdoClient.h](../../adapters/canopennode/SdoClient.h) | One active SDO per channel, one node anchor per channel. Heartbeat, boot-up and EMCY are kept per node (`emcy`, `emcyCount` in [NetworkState.h](../../adapters/canopennode/NetworkState.h)) | Two nodes live since 2026-11-02 |
| Owner | `CanBusOwner` enums in [CanBusOwner.h](../../adapters/owner/CanBusOwner.h): `SEQUENCES = 2`, `REQUESTS = 6`, `PRODUCERS = 4`, `START_LIMIT = 2`, `TX_LIMIT = 2`, `RX_LIMIT = 8` per turn; one urgent slot per channel ([can_owner.md](../can_owner.md)) | `select()` takes urgent slots first, then rotates through producers. Each turn scans both channels, so both channels can start and transmit in the same turn. `beginUrgentOperation` retires only its own reservation's queued work | Mutant 29 `stopsBesideObserver` ([packet 8](bench_closure_18_33/packet_8.md)) |
| Axis session | `AxisSession` in [AxisSession.h](../../examples/common/AxisSession.h), one per axis | Holds its own reservation, latest controlword, two stop contexts, enable, move, velocity and home contexts, and the fault fence. 35752 bytes ([prompt 42](prompt_42.md)) | Both specimens, one axis at a time |
| Application | `ProbeApp::axes_[2]`, `openAxis` in [ProbeApp.h](../../examples/common/ProbeApp.h) | Two open sessions. A third is refused. `service()` advances both | Host tests |
| Discovery | `DiscoveryScan::CAPACITY = SdoClient::CHANNELS` in [DiscoveryScan.h](../../examples/common/DiscoveryScan.h) | Two permanent slots. Slots are not recycled (Q18) | Both nodes, 2026-11-09 |
| Bench | `BenchRoute` MOVE with an observer node (packet 22 step 6) in [BenchRoute.h](../../examples/s2_bench/BenchRoute.h) | The other node is read through its own route, session and read context, and is never written. Terminals carry the node and traffic carries the channel | Trials 413 (2026-11-04) and 832 (2026-11-09) |
| Fault isolation | `AxisSession::preflight(driveFault)`, called per node by `BenchRoute` ([packet 2](bench_closure_18_33/packet_2.md)) | An EMCY on one node holds back only that node's queued download | Host tests and bench route |
| Host tests | [move_relative_test.cpp](../../test/move_relative_test.cpp): `bothNodesUrgent`, `urgentBesideOtherWork` and `urgentBesideOtherNode`, `stopsBesideObserver` | Two **concurrent** relative moves on nodes 1 and 2 through two sessions on one owner. A HALT on one node and a quick stop on the other. Node 2's unread results do not block node 1's stop. An urgent stop beside the other node's queued, adapter-held or handed-off download sends no replay and no cross-node words | Simulated drives only |

The library and its host tests already run two independent moves at once and stop each axis on its own. The missing
pieces are a bench path, a group-stop contract, start skew and any scaling beyond two nodes.

### Single-axis limits: deliberate or incidental

| Limit | Where | Deliberate or incidental | Effect on concurrent work |
| --- | --- | --- | --- |
| One occupied bench route | `BenchRoute::occupied_`, one record of 128 terminals and 448 traffic records | Deliberate: one finite trial record per admission, and the 42688-byte block is the largest the heap gives | A second mover needs a paired route that shares the one record, not a second `BenchRoute` |
| Per-boot motion caps | `ROUTE_MOTION_TOTAL = 12`, `ROUTE_MOTION_PER_CASE = 3` (packet 22); `s2_bench.py` carries the count across boots | Deliberate safety budget | A paired run spends two admissions. Planned stops count too |
| Observer is never written | MOVE with observer | Deliberate | Gives two-node evidence without two-node motion |
| One console motion attachment | `ProbeConsole::motion_` and `detachMotion`, which refuses while any motion operation is retained ([ProbeConsole.h](../../examples/common/ProbeConsole.h)); `BenchConsole::attached_` | Deliberate minimum for prompt 42, with switching added | **Gap for concurrency:** while axis A has a retained operation, the console cannot reach axis B, not even to stop it |
| Console stop slots | `stopReplies_[2]`, `TOTAL_OPERATION_CAPACITY = 5` | Sized for one axis (incidental) | Needs one normal and one fast stop slot per axis |
| Bench and console exclusion | `BenchActivity::wire` refuses console lines | Deliberate: one user of the owner on the bench image | Concurrency has to live inside one path, either a route or the console |
| Two channels, two reservations, two sessions | `CHANNELS`, `SEQUENCES`, `axes_[2]` as fixed enums in application and adapter code | Incidental: sized to the two-node fixture | A third node needs a third channel. Time-sharing is impossible, because a channel cannot be rebound without settlement evidence (Q18, D14). RS sizes this storage from the caller instead |
| Shared producer | `ProbeApp::openAxis` opens every session with producer 1 ([ProbeApp.cpp](../../examples/common/ProbeApp.cpp) line 159) | Incidental | Requests from the two axes are ordered by age inside one producer rather than by producer rotation. This is harmless while each node has its own channel |
| Separate context per operation kind | `AxisSession` holds `move_`, `velocity_` and `home_` together | Incidental; the retained fences and history must survive any change | Per-axis memory cost. Only one ordinary motion runs per axis at a time |
| Single selection and read | `ProbeApp::select` and `pending_` | Deliberate, for console reads | Not a blocker, because move observation reads belong to the session |
| Fixed node IDs 1 and 2 | Bench image | Fixture fact | None for the library |

## 2. Bus-time budget for N axes

### Measured on this bench

Night series S0001 (node 1, 2026-11-11 image), 320 SDO exchanges from the route records:

- The S2 receive bounds for the reply were at most 0.31 ms (lower) and 2.15 to 2.69 ms (upper, mean 2.25 ms) after TX
  handoff. These are software bounds, not wire timestamps. Wire timing needs a listen-only analyzer (H04).
- Every reply was processed 3.0 ms after handoff. The owner task runs every 1 ms (`CONFIG_FREERTOS_HZ=1000`).
- Consecutive exchanges on one route were at least 6.0 ms apart, typically 7 ms.
- A seven-field observation cycle (2100..2102, 6041, 6061, 6064, 606C; `Position.h` line 109) therefore takes about
  49 ms.
- The largest service gap in that position run was 4 ms (`gap=4000`).

### Frame arithmetic at 1 Mbit/s, Classical CAN, 11-bit ID

| Frame | Bits, nominal to worst-case stuffing (with 3-bit intermission) | Time |
| --- | --- | --- |
| 8-byte data frame (SDO, RPDO1) | 111 to 135 | 111 to 135 µs |
| 7-byte TPDO1 (`6041`, `6061`, `6064`) | 103 to 125 | 103 to 125 µs |
| Expedited SDO exchange (two frames) | 222 to 270 | 0.22 to 0.27 ms |

The bus carries at most about 7400 worst-case 8-byte frames per second. A planning ceiling of 50% (about 3700 frames
per second) keeps arbitration delay small for urgent frames.

### Axis count by method

| Method, per axis | Frames/s per axis | Bus share per axis (worst case) | Axes at 50% bus | Host limit |
| --- | --- | --- | --- | --- |
| SDO, 7-field cycle every 50 ms (S2 route cadence) | 280 | 3.8% | 13 | One channel per node with one exchange in flight. Owner `START_LIMIT` allows 2 starts per 1 ms turn, at most 2000 exchanges/s, about 14 axes. **Today: 2 channels, so 2 axes** |
| SDO, 7-field cycle at 21 ms (3 ms back-to-back, not reached on the S2) | 667 | 9.0% | 5 | 6 by the turn limit |
| SDO, 3 fields (6041, 6064, 606C) every 10 ms | 600 | 8.1% | 6 | Needs exchanges of 3.3 ms or less |
| TPDO1 every 10 ms (event timer raw 9; period measured as raw + 1 ms, D15) | 100 | 1.25% | 40 | `PDO_ROUTES = 8` per interface: at most 8 TPDO routes |
| TPDO1 + TPDO2 every 10 ms | 200 | 2.6% | 19 | 8 routes allow 4 axes |
| Stop of all axes, SDO | 2 per axis, once | 0.27 ms of wire per axis | n/a | `TX_LIMIT` 2 per turn: the last stop is handed off after ceil(N/2) turns, plus each channel's in-flight exchange (about 3 ms typical, capped by the 250 ms response budget) |
| Stop of all axes, RPDO | 1 per axis, once | 0.135 ms per axis | n/a | No acknowledgement. The mapping problem is listed below |

Conclusions:

- With SDO only, the wire is not the bottleneck. The limits are the per-channel round trip, the 1 ms host turn, the
  owner's per-turn limits and the channel count.
- Two axes at about 50 ms cycles use about 8% of the bus today.
- Beyond about 5 axes at 20 ms, or 13 at 50 ms, monitoring must move to TPDOs.

Limits on the PDO path:

- Sample provenance is not qualified (D11, D15). A confirmed map does not qualify samples, so TPDO data cannot yet
  replace SDO evidence for completion or stop settlement.
- PDOs need NMT Operational (prompts 16 and 38).
- The read soak still counts PDO frames as anomalies (S13).
- The specimens' RPDO1 maps `6040`, `6060`, `6098` and `607A` ([2026-11-06](2026-11-06_pdo_stream.md)). Any RPDO
  frame therefore also writes a target, a mode and a homing method. An RPDO stop or start needs a
  controlword-only map first (Q17, prompt 18).
- No Lichuan SDO maximum response time or internal processing cycle is documented.

## 3. MotorControl-RS and the drive contrasts

### MotorControl-RS (authoritative for names)

| What it has | Path and symbol | Disposition |
| --- | --- | --- |
| Per-axis stop | `prepareStop` in `include/MotorControlRS/profiles/ess_rs/Actions.h` line 93 | Adopted already: a group stop is made of per-axis stops |
| Urgent class with reserved quotas | `RtuBusOwner::admitUrgent`, `urgentPendingCapacity`, `urgentResultCapacity` in `examples/common/RtuBusOwner.h` | Matches the CAN owner's one urgent slot per channel |
| Storage sized by the caller | `BusStorage` (`pendingCapacity`, `resultCapacity`, `producerCapacity` 1..65535) in `RtuBusOwner.h` lines 105-113 | **Adopt** for N > 2 instead of fixed enums (M04) |
| Turn fairness between producers | `docs/bus_owner.md`, "Fairness, dispatch and sequence waits" | Same model |
| Broadcast motion deferred | `docs/architecture.md` lines 335-336 | Consistent: no broadcast or NMT stop is used as a group stop |
| Group operations, stop-all, synchronized start | None in `src`, `include` or `examples` | **No RS name exists.** Do not invent a common-layer group API now. Keep any helper at example level, with names that do not imply an RS-backed meaning |

FieldCore `Rs485MotorModule` (`include/TunnelMonitor/device/Rs485MotorModule.h`) is one module instance per motor,
each with its own runtime configuration. It has no group behavior to copy.

### Contrasts (design input only, not support)

| Drive | Facts relevant to several axes | Source |
| --- | --- | --- |
| Lichuan CL86-C | Modes 1, 3 and 6 only, so no IP or CSP mode. `1005` SYNC COB-ID exists; there is no `1006`. Synchronous PDO types are unqualified. Four RPDO and four TPDO | [lichuan_review.md](../reference/lichuan_review.md) (1005 row, PDO section); D15 |
| Nanotec N5 | Interpolated position "is used to synchronize multiple axes" and needs SYNC (PDF p89). SYNC object and synchronous window `1007` (p119). RPDO types 00h..F0h become valid at the next SYNC (p128); TPDO types 1..240 (p129). 1 ms application and position cycle (p67). Eight RPDO and eight TPDO (p127). The p119 table shows the SYNC ID as "80+NodeID", which differs from the usual fixed 0x80; keep this visible and check it before any use | `docs/reference/multidrive/vendor/nanotec_n5_canopen_v3.3.0.pdf`; extract under `extracted/` |
| maxon EPOS4 | CSP 8, CSV 9 and CST 10 in the cited mode table. Heartbeat error policy `1029` and abort reaction `6007` are distinct objects. SYNC details were not reviewed locally (rel13740 is remote-only) | [drive_contrasts.md](../reference/drive_contrasts.md) |

Consequence: coordinated (interpolated) motion is an extension scope that needs a timing and watchdog contract
([decision](../multidrive_decision.md), Timing row). It cannot run on CL86-C. On the Lichuan, the most that is possible
is a synchronized start of independent profile-position moves.

## 4. Staged plan

### (a) Two independent moves at once, each with its own stop

| Item | Content |
| --- | --- |
| Owner | Host adversarial checks: prompt 45 item 2. Example-level pairing helper: a new packet (prompts 45 to 48 own no application coordinator). Hardware: 47 for finite trials, then 48 item 4 (multi-node load) |
| Prerequisites | None in the core: `AxisSession` is already independent. Node 2's halted state from S20 must be settled first. Velocity pairs wait for S19 and S20 |
| Risks | Shared `REQUESTS = 6` across both axes; the shared producer; caps; 128 terminals shared by two axes (S19 already feels the limit); higher console output raises loss (H04) |
| Memory | Route path: none. The S2 already reserves both sessions, and `RouteWork.observer` exists. Console path: a second `BenchAxis` of 3856 bytes (README) |
| Smallest next step | A `MOVE` route variant with a moving partner: it reuses the observer's route, session and read context and calls the partner session's own `requestMove`. Write the `bench_route_test` case first. A pair counts as two motion admissions |

### (b) Stop of all axes, and its uncertainty rules

Rules to adopt:

1. A group stop is N independent `AxisSession::requestStop` calls, each with its own HALT or quick stop, identity,
   deadline, path and result. Never use NMT Stop or a broadcast: an NMT stop is not a CiA 402 quick stop.
2. Compose every stop first, then admit them. Record each axis's handoff time. The skew between axes is evidence, not
   a guarantee.
3. If one axis refuses (no history, released drive, the S20 report), the other axes still stop. The refusing axis
   takes the best-effort `000B` only if its own policy allows it.
4. The group is terminal only when every member is terminal. Report it as stopped only when every member's completion
   is `OBSERVED`. Any `UNKNOWN` or uncertain member makes the whole group uncertain. Never aggregate into success.
5. Nothing re-enables, resumes or replays. Interrupted moves stay fenced (`moveFenced()`).
6. An EMCY on one axis does not stop the others automatically. Stopping the others is explicit application policy,
   with its own per-axis results.
7. A local cancel or a lost host is not a stop (D10).

| Item | Content |
| --- | --- |
| Owner | Per-axis semantics: prompt 31 (Partial). Integration: prompt 45. Helper: the same new packet as (a) |
| Risks | An in-flight read on a channel delays that axis's stop by one exchange. A quarantined route leaves only the best-effort path. The stop observation cadence is S19 |
| Memory | Negligible: stop contexts already exist per axis; a group record is N copied stop results |
| Smallest next step | Write the contract into [stop_actions.md](../stop_actions.md). Add a host test: one node mid-read, one node refused, both results kept, no aggregation |

### (c) Synchronized start

| Option | What it gives | Honest limit | Gate |
| --- | --- | --- | --- |
| Host-timed SDO triggers, both `001F` in one owner turn | Handoff skew around one frame time (0.1 to 0.3 ms) plus controller queue order | Drive reaction skew is unknown. Acceptance is seen only at the observation cycle (about 49 ms) or the TPDO period | Needs a held-trigger step in `AxisSession`. It must drop the held trigger on epoch change, fault or stale prerequisites |
| RPDO burst, one frame per node | About 0.135 ms per frame | No acknowledgement. RPDO1 currently also writes `607A`, `6098` and `6060`. Needs NMT Operational and a host RPDO producer, which does not exist | Q17 remap, prompt 18 |
| SYNC with synchronous RPDO types | Same-instant validity in the drives, where supported | Lichuan has no `1006`, and its synchronous types are unqualified. Needs a host SYNC producer | Q17 evidence; research only |
| Halt-bit release trick | — | Not documented for the Lichuan. Rejected | — |

Owner: a new prompt after 46, gated by Q17 for the PDO and SYNC options; hardware in 48. Smallest next step: measure the
natural trigger skew of the paired route from (a), using TX handoff times and drive-reported acceptance, before adding
any held-trigger API.

### (d) Bench and console support

| Item | Content |
| --- | --- |
| Console | Allow one `ProbeMotion` per axis (`BenchConsole` already holds one `BenchAxis` per node). Add an explicit axis token on motion and stop lines; `useaddr` stays separate. Give each axis its own pair of stop slots. A `stop all` line expands into per-axis stop operations, each with its own reply. The first requirement: axis B's stop is admitted while axis A's operation is retained |
| Caps and evidence | Count every axis admission. Keep the `s2_bench.py` ledger per axis. Keep node tags on every line. Prefer streamed records at a slower cadence (H04) |
| Owner | C++ console: a new packet (prompt 42 is Complete). Runner scenarios: prompts 43 and 44 (Pending) |
| Memory | Two `BenchAxis` blocks are 7712 bytes against a reported largest free block of 7680 after the console. A third stop pair adds about 1.1 KB. This depends on the memory report |
| Smallest next step | A `sizeof` inventory per extra attachment, then a host test of B's stop while A's move is retained |

### (e) Hardware tests on the two-drive bench

| ID | Test | Prerequisite |
| --- | --- | --- |
| HM1 | Paired absolute moves at 6 to 30 rpm, opposite directions. Both `SUCCEEDED`; each node received only its own words | (a), S20 settled |
| HM2 | HALT node 1 while node 2 completes, then the reverse | HM1 |
| HM3 | Group HALT, then group quick stop, during paired moves. Record handoff skew; each stop settles independently | (b) |
| HM4 | Group stop while node 2's channel has a read in flight; measure the wait | HM3 |
| HM5 | Paired velocity runs, 60 rpm or less, with a HALT per axis | S19, S20 |
| HM6 | Same-turn trigger skew, with TPDO1 at 10 ms on both nodes for acceptance timing | NMT Operational, S13 |
| HM7 | Two-axis console streaming at 100 and 50 ms, with loss accounting | (d), H04 tools |
| HM8 | Wire skew of a group stop | Independent listen-only observer (H04, prompt 47) |
| HM9 | EMCY on one axis during a paired move | A naturally present alarm (H03) |

Every test runs under the standing bench authorization, the existing caps and per-axis records. None is qualification.

## 5. Proposed backlog items, in priority order

| ID | Item |
| --- | --- |
| M01 | Paired `MOVE` route with a moving partner: host test, then HM1 and HM2 |
| M02 | Group-stop contract (rules 1 to 7 in section 4b) and host tests, then HM3 and HM4 |
| M03 | Console: a stop for axis B while axis A's operation is retained, then per-axis attachments and stop slots |
| M04 | Caller-sized owner and adapter storage (`CHANNELS`, `SEQUENCES`, `REQUESTS`, urgent slots, `axes_`) after RS `BusStorage`, for N > 2 |
| M05 | `AxisSession` memory: measure context sizes and consider shared storage for move, velocity and home without losing fences |
| M06 | A distinct producer per session in `ProbeApp::openAxis` |
| M07 | Same-turn trigger skew measurement and a written limit for host-timed start (HM6) |
| M08 | TPDO monitoring of several axes: PDO-aware soak (S13), sample provenance (D11, Q17) |
| M09 | Controlword-only RPDO map review before any RPDO stop or start (Q17, prompt 18) |
| M10 | Lichuan SYNC capability evidence (`1005` present, no `1006`, synchronous types): research only |
| M11 | Cap and ledger rules for paired admissions in `s2_bench.py` |
| M12 | Example policy for a fault on one axis: stop the others explicitly, with per-axis results (HM9) |

## Multiple manufacturers (the scope document's meaning)

Already in place:

- Protocol-neutral `Endpoint` and `Binding`, and the `Protocol` enum.
- Frames keep 11/29-bit IDs and RTR; FD support is an explicit flag.
- Object policy keyed per profile (`0x2300` means different things on Lichuan and N5).
- One owner per interface that rejects overlapping routes.
- N5 and EPOS4 contrast fixtures in `multidrive_contract_test.cpp`, the retained N5 manual and the
  [ADD_DRIVE](../prompts/ADD_DRIVE.md) playbook.

Still pending:

- Prompt 46: promote the prototype capability and ramp types out of `experiments/api_ownership/Contracts.h`, run the
  contrasts through production code and add the non-CANopen fixtures.
- A second real profile (N5 first), which needs an assignment, firmware-matched evidence and two-profile packaging.

This report does not expand that scope.
