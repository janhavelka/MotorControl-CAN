# Host throughput: loop pacing, frames per pass, polling and owner turn cost

Date 2026-11-12. Revision `2c5d832` on `main`. Read-only research. Host measurements came from
disposable scratch builds (`.build/throughput-*`) of `git archive HEAD` copies, three of them patched
as described in section 5. No hardware was used and no repository file other than this report changed.

The user's questions, verbatim: "One frame per loop pass - what does this mean? Could i send more? Do i
need polling in practise? Can i easily send more frames per loop? Is it real anyway? You are counting the
loop passes at 1ms intervals, right? It can be much faster and therefore fit more polls per 50ms, right?",
"Motion is watched by polling - right, but future prompts are setting periodic data sent by the drives,
right? PDOs - that solves the issue as the drives send data periodically on its own, right?" and, about
the owner turn cost, "how come mcu would become 'slower'?"

## Short answers

- **What "one frame per loop pass" means.** The TWAI adapter keeps at most one frame inside the CAN
  controller. It hands the controller the next frame only in a later loop pass, after that pass has seen
  the "frame sent" event. So at most one frame leaves per pass.
- **Is "about 1000 frames/s" real?** No. That figure is tick arithmetic: one pass per 1 ms FreeRTOS tick.
  Nobody measured it. The retained bench traces of 2026-11-11 show that during SDO work the owner ran
  about **once every 3 ms**. That means at most about **300 frames/s**, not 1000.
- **Yes, the loop is paced at 1 ms.** Each pass ends with `vTaskDelay(1)`, with `CONFIG_FREERTOS_HZ=1000`.
  The task sleeps until the next tick even when work is waiting. Nothing wakes it when a frame is sent or
  received. Passes can therefore never be more often than 1000 per second. Measured passes were rarer,
  because one pass seems to take 2 to 3 ms of CPU on the bench image (160 MHz, `-Og`). That cause is
  inferred, not measured.
- **Could you send more?** Yes. The bus is not the limit: at 1 Mbit/s it carries about 7,400 to 9,000
  eight-byte frames per second. Smallest steps first:
  1. Make the pass short. Measure where the 2 to 3 ms go, and build the image at 240 MHz with `-O2`.
  2. Wake the owner task from the TWAI "sent" and "received" interrupts, instead of waiting for the next
     tick. This keeps one frame in flight, so the existing proof still holds. Expected: a few thousand
     frames per second if the woken pass is short. If every wake runs today's 2 to 3 ms pass, the gain is
     small.
  3. Several short owner sub-turns per tick. This is easy and safe, but it spins unless combined with step 2.
  4. More than one frame in the controller. This gets close to the bus rate, but it breaks the current
     stop and uncertainty proof. It needs its own design and proof.
- **More polls per 50 ms?** Yes, but the frame limit is not what limits polling today. One SDO read takes
  about 7 ms per node: 3 ms until the reply is processed, then about 4 ms until the next request leaves.
  So 7 fields take about 49 ms. Faster or event-driven passes could bring one read down to perhaps 1 to 3 ms
  (estimate). The drive's own reply time is not known yet, only that it is under about 2.3 ms.
- **Do you need polling in practice?** For watching motion on many axes, mostly no. A TPDO is sent by the
  drive on its own timer, so the host sends nothing to get the data, and the host TX limit does not apply.
  You still need SDO for settings, identity, every write and the readback that confirms it, and anything
  not mapped into a PDO. Today the library also needs SDO for motion completion and stop settlement, because
  PDO samples are not yet "qualified" (D11).
- **Do the future prompts switch to PDOs?** Partly. Prompt 18 (Partial) can already set the TPDO1 event
  timer. Prompts 51 and 54 add SYNC and synchronized TPDO sampling as trials. No prompt yet moves motion
  observation from SDO polling to TPDO samples. That is backlog M08, which waits for S13, D11 and Q17.
- **D11 in simple words.** After a change to a PDO (new map, reboot, NMT change), frames with the same CAN ID
  and the same length keep arriving. A frame produced before the change can still arrive after it. Its bytes
  look valid but mean the old thing. The host's new "generation" number is not inside the frame, so it cannot
  tell old from new. The library therefore keeps such samples unqualified until there is a provable boundary:
  the stream visibly stopped and restarted, a different ID was used, or the frame carries a marker.
- **Why the MCU "becomes slower" at large N.** It does not get slower. It is given more work per turn. Today
  the owner repeats an expiry scan for every channel. That scan walks every slot, and each slot looks up its
  channel among all channels. Work grows roughly with N³. A desktop core does the same work very roughly 50 to
  100 times faster than an ESP32-S2/S3 core at 240 MHz (estimate). So the host's 186 us at N = 32 means roughly
  10 to 20 ms per turn on the board, and more on the bench image. While one turn runs, no other frame is sent
  or routed.
- **Verdict on the candidate fix** (return from `CanBusOwner::transmit` before `expire()` when the channel has
  nothing staged): it is **safe for what goes on the wire**. `expire()` never stages a frame, so an empty
  channel cannot send either way, and every frame that is handed over still passes an `expire()` first. It
  never drops a stop frame and never clears uncertainty. But it **is not order-preserving**. A state change made
  during the TX scan can then be expired up to one turn later. That can delay a peer's invalidation and the node
  invalidation it triggers, and move a best-effort stop's start or invalidation by one turn. **Do not apply it as
  is.** Apply the smallest safe alternative instead: skip empty channels in the scan loop, keep `transmit()`
  unchanged, and call `expire(now)` once after the scan, before `select()`. Every decision then sees state at
  least as fresh as today. Host turn cost at N = 32 drops from 186 us to about 21 us, and all 140 native tests
  pass on the patched copy.

## 1. How the firmware loop is paced

### Code

| Fact | Evidence |
| --- | --- |
| The owner task runs `firmware.service()`, the console or test hook, then `console.idle()` in an endless loop | `examples/common/EspProbeMain.cpp:71-102` |
| `idle()` is `vTaskDelay(1)`: sleep until the next tick | `examples/common/EspProbePlatform.cpp:128-131`; fallback `EspProbeMain.cpp:101` |
| One firmware turn: `pollEvents`, then `app_.service` (owner, sessions, console bytes), then `promote` (SDK submit), then evidence | `examples/common/FirmwareProbe.cpp:198-212`; `examples/common/ProbeApp.cpp:299-332` |
| S2 bench loop: same pattern, then every bench collector, then `console.idle()` | `examples/s2_bench/main/main.cpp:1039`, `:1065-1100`, `:1952` |
| Tasks are created at priority 1, pinned | `EspProbeMain.cpp:111-112`; `s2_bench/main/main.cpp:1957-1958` (core 0) |
| Arduino entry starts the same owner task; Arduino's own `loop()` only sleeps | `examples/probe_cli/main.cpp:7-13` |
| Tick rate 1000 Hz | `examples/s2_bench/sdkconfig.defaults:2`; `examples/probe_idf/sdkconfig.defaults:3` |
| TWAI callbacks only copy data and set flags, and return `false`. No task notification exists anywhere in `examples/` or `adapters/` | `adapters/twai/IdfTwaiDriver.cpp:251-264` (TX done), `:265-310` (RX), `:311-329` |

So the owner wakes only on the timer tick, never on a TWAI event.

The image used for the 2026-11-11 night series has `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=160`,
`CONFIG_COMPILER_OPTIMIZATION_DEBUG=y` (`-Og`), an 8 KB instruction cache and `CONFIG_FREERTOS_HZ=1000`
(`docs/bench/20261111T1736Z-s2-upload/sdkconfig:505`, `:734`, `:739`, `:843`). Neither
`sdkconfig.defaults` file sets the CPU frequency or the optimization level, so the IDF defaults apply. The
locally installed Arduino 3.3.11 libraries use 240 MHz and `-Os` (`C:/p55s/packages/framework-arduinoespressif32-libs/esp32s2/sdkconfig`,
a local package, not repository evidence).

### What the retained traces show

Night series S0001 (`docs/bench/20261111-night/packed/S0001-n1-Pa.tar.xz`, node 1, eight finite trial
records) holds 320 SDO exchanges. Each has three trace lines: the owner handoff (`k=1`, time = the pass's
`now`), the controller TX report (`k=2`) and the reply (`k=0`, with receive bounds `lo`..`hi`). Relative to
the handoff pass:

| Quantity | All 320 exchanges |
| --- | --- |
| TX report consumed | 2,997 to 3,002 us later, every time |
| Reply consumed | in the same pass as the TX report |
| `lo`: last time the driver saw the RX FIFO empty | 0.29 to 0.31 ms |
| `hi`: reply interrupt time | 2.15 to 2.69 ms (mean 2.25 ms) |
| Reply processed after its interrupt | at most 0.85 ms |

A frame completes on the wire about 0.1 to 0.2 ms after submission. If any pass had run at +1 ms or +2 ms,
it would have consumed the TX report there. It would also have moved `lo` forward, because every
`pollEvents` and `receive` call refreshes it (`IdfTwaiDriver.cpp:201-214`, `:215-234`, `:235-239`). Neither
happened in any of the 320 exchanges. So **no owner pass ran from about 0.3 ms after the handoff until the
reply arrived about 2.2 ms later**. The next pass started on the tick at +3 ms.

Reading: during SDO work, one pass of the bench loop lasted more than about 2.2 ms, so `vTaskDelay(1)`
woke the task only at the third tick. Passes were then about 3 to 4 ms apart, which fits the records' own
largest service gap, `gap=4000`. This is inferred from traces, not a direct measurement of the pass. What
costs the 2 to 3 ms is unknown. Candidates (hypotheses): `-Og` at 160 MHz, instruction-cache misses on code
that runs from flash through an 8 KB cache, session and route work, console output.

A second reading needs one more timestamp to decide. If the expensive part of the pass sits between the
owner's RX loop and `promote` (the sessions run there, `ProbeApp.cpp:307-309`), the request left late in
the pass and the drive answered within a few hundred microseconds. Otherwise the drive took about 2 ms.
A submit timestamp in `IdfTwaiDriver::submit` and one in the TX-done callback would settle it.

Other service-gap figures agree that passes were often longer than 1 ms: 3,788 to 5,191 us worst case in the
overnight soaks (`docs/reports/2026-11-03_overnight_soak.md:26`, `:30`), 4.8 ms in the motion stress run
(`2026-11-05_motion_and_stress.md:63`), 5 to 6 ms in stops (`handoff_19_33/packet_16.md:17`), 2,008 us on the
early raw path, and 1,483 us during the silent capture (`bench_closure_18_33/packet_4.md:113`). The 3 h soak
moved 1,505,129 frames each way in 11,411 s, about 132 frames/s each way. That run was limited by the
one-read-at-a-time pipeline, not by a frame limit.

**Is "about 1000 frames/s" real?** No. It appears as an estimate in `2026-11-12_capacity_configuration.md:37`
and `:321` and in `2026-11-12_library_limitations.md:245-247`. `2026-11-12_multidrive_preparation.md:58` also
says "The owner task runs every 1 ms". The traces above contradict that during SDO work. The overnight soak's
claim that the run was "bound by the drives' reply time" (`2026-11-03_overnight_soak.md:32`) is probably
overstated for the same reason. Both should be corrected after the measurement in step A below, with this
evidence and the old wording recorded.

## 2. Why the adapter keeps one SDK frame in flight

| Rule | Evidence |
| --- | --- |
| `promote` returns at once if any slot is ACTIVE; otherwise it picks the oldest QUEUED slot, checks again that the request may still be sent, marks it ACTIVE, then submits | `adapters/twai/TwaiAdapter.cpp:148-187` (`:152-154`, `:155-156`, `:161-168`, `:169-171`) |
| The SDK TX queue depth must be 1, and a second submit is refused while one is pending | `TwaiAdapter.cpp:16`; `IdfTwaiDriver.cpp:62`, `:95`, `:172-175` |
| Submit is deferred while a received frame waits to be routed, so a reply or boot-up is seen before the next frame leaves | `IdfTwaiDriver.cpp:176-180` |
| The TX-done callback must match the exact owned frame | `IdfTwaiDriver.cpp:254`; `TwaiAdapter.cpp:128-139` |
| A queued frame can be withdrawn; an ACTIVE one cannot, only deleting the node ends it | `TwaiAdapter.cpp:93-102`; `adapters/twai/README.md:142-146` |
| The "final handoff" proof depends on one outstanding frame and the same pinned core, using the SDK's idle direct path | `docs/adapters/twai_sdk.md:95-109`, `:74` |
| No-ACK retries forever; a retired frame may still have been delivered | `twai_sdk.md:35-53` |
| Owner per turn: `TX_LIMIT = 2` enqueues, `START_LIMIT = 2` new services, `RX_LIMIT = 8`, `EVENT_LIMIT = 8` | `adapters/owner/CanBusOwner.h:31-35`; `CanBusOwner.cpp:669-713` |

Why this matters, in plain words:

- The code always knows the single frame that might be on the wire. A stop or a recovery then knows exactly
  what is uncertain.
- The next frame is chosen at the last moment. A stop admitted now goes next. It does not wait behind
  frames already handed to the controller.
- A frame not yet in the SDK can be withdrawn for free. A frame in the SDK can only be ended by deleting the
  node.
- The boot-up guard works because the task decides when each frame starts.

**Hard rule or pacing?** The hard design rule is "one frame inside the SDK and controller at a time".
"One frame per pass" is only the current pacing. The TX-done event is consumed at the next `pollEvents`, and
`pollEvents` runs once per pass. Owner `TX_LIMIT` allows two enqueues per turn, but the adapter submits only
one per turn (`adapters/twai/README.md:120-123`; `docs/can_owner.md:121-122`).

## 3. Options to send more, smallest first

Bus time at 1 Mbit/s, Classical CAN, 11-bit ID. A frame with n data bytes has 47 + 8n bits including a
3-bit intermission, plus at most floor((33 + 8n) / 4) stuff bits.

| Frame | Time | Frames/s |
| --- | --- | --- |
| 8 data bytes (SDO, RPDO) | 111 to 135 us | 7,400 to 9,000 |
| 0 data bytes (SYNC) | 47 to 55 us | 18,200 to 21,300 |
| Expedited SDO exchange (2 frames) | 222 to 270 us of wire, plus the drive's reply time | |

| Option | Change size | Risk to the exact-retirement proof | Expected rate (estimate) |
| --- | --- | --- | --- |
| 0. Make the pass short: find the 2 to 3 ms, 240 MHz, `-O2` | `sdkconfig.defaults` lines plus whatever the measurement finds | None. Stack use changes with `-O2` and must be measured again | Today's 1 frame per pass, but closer to 1000 passes/s |
| a. Wake the owner task from TWAI TX-done and RX callbacks (`vTaskNotifyGiveFromISR` after leaving the driver's critical section, return its "woken" flag); `idle()` waits with `ulTaskNotifyTake(pdTRUE, 1)` | Small: driver callbacks, platform idle, host fakes, about 30 lines plus tests | Low. Still one frame in flight, same task and core; the ISR still finishes before the task runs, which is the proof's premise (`twai_sdk.md:102-109`). Do not wake on `on_error` (about 7,250 per second during a no-ACK storm, `twai_sdk.md:39-41`). Bound wakes so the idle task and console still run | One frame per (frame time + interrupt and task switch, roughly 10 to 30 us + woken pass). With a woken pass of 0.1 to 0.3 ms: about 2,500 to 5,000 frames/s. With today's 2 to 3 ms pass: about 350 to 450 frames/s, little gain. SDO read round trip falls to about the drive's reply time plus two short passes |
| b. Several owner sub-turns per tick (`pollEvents`, owner with TX/RX budgets, `promote`, repeated up to K times while a frame waits) | Small, examples only | None: same rules, same task | At most K frames per pass. A sub-turn helps only after the previous frame finished (about 0.13 ms), so without (a) it spins or wastes the pass. About K divided by the pass time, for example 1,300/s for K = 4 and a 3 ms pass |
| c. More than one frame in flight (SDK queue depth 2 or 3, per-frame ACTIVE slots matched by `done_tx_frame`, owner limits raised) | Large: driver, adapter, owner, fakes, SDK review, bench proof | High. The SDK then starts queued frames from its ISR, not from the task (the queue path, not the idle direct path the proof uses). The captured-RX guard no longer covers queued frames. A stop waits behind up to K - 1 frames it cannot overtake. No-ACK, bus-off and node deletion make all K frames uncertain | Up to the bus rate (about 7,400/s for 8-byte frames), even with slow passes |

Recommendation: do 0 and (a) together, after measuring. Keep (b) as a small follow-up if the
measurement shows that one woken pass still costs much more than a frame time. Treat (c) as a separate design
prompt with its own proof.

## 4. Is polling needed in practice?

What PDOs give today:

- Prompt 17 (Complete): observed map decoding and passive samples, `validatePdoMap`, `decodePdo`,
  `pdoMapFromReadback`, `configurePdo`, `qualifyPdo` and `pdoFreshness` (`docs/canopen_services.md:774-884`).
- Prompt 18 (Partial): a typed TPDO1 configuration subset, including the event timer `1800:05` raw 0/100
  (`docs/canopen_services.md:887-942`; `docs/reports/prompt_18.md:5`). Remaining Q17 gates stay open
  (`docs/device_conflicts.md:88`).
- Measured on the specimens: TPDO1 periods fit raw + 1 ms for 10, 100 and 200 (fitted, not documented). TPDOs
  need NMT Operational. The live maps differ from the EDS (backlog D15, `docs/backlog.md:103`).

A TPDO is drive to host. The host sends nothing to get it, so the host TX limit does not apply. The receive
side then matters instead: a 16-entry ISR ring and 8 RX per turn (`IdfTwaiDriver.h:25`; `CanBusOwner.h:31`).
At 32 axes with a 10 ms TPDO each, a 3 ms pass brings about 10 frames, close to the ring size. An overflow
inhibits TX (`TwaiAdapter.cpp:123-126`; `2026-11-12_capacity_configuration.md:428`). Faster or event-driven
passes help PDO reception too.

Why motion still polls today:

- Motion observation is a seven-field SDO cycle (`include/MotorControlCAN/profiles/lichuan_c/Position.h:109-111`;
  `2026-11-12_library_limitations.md:254-260`).
- A confirmed map does not qualify any sample (`docs/canopen_services.md:838-848`). D11 (`docs/backlog.md:99`)
  and Q17 stay open. The read soak still counts PDO frames as anomalies (S13, `docs/backlog.md:129`).
- TPDO monitoring of several axes is backlog M08 (`docs/backlog.md:193`), "after S13, D11 and Q17". Prompts 51
  and 54 trial SYNC and synchronized TPDO sampling (`docs/prompts/51_sync_producer.md:68-69`;
  `54_hardware_sync_multiaxis.md:27-30`), but none of the pending prompts moves motion completion to PDO
  samples.

Reads that stay SDO even with PDOs: identity and settings (read once, not polled); every write and its
readback (TPDO1 configuration itself requires `SDO_READBACK` evidence); fault and alarm details read on demand;
anything not mapped; and, until D11 is solved, motion completion and stop settlement.

## 5. Owner turn cost at large N

### Where the time goes

`CanBusOwner::service` calls `expire(now)` after `collect()` (`CanBusOwner.cpp:568`), after each TX report
(`:623`), after each received frame (`:665`) and once before the TX scan (`:667`). The TX scan then calls
`transmit(c, now)` for every channel (`:677-684`). `transmit` calls `expire(now)` before it even checks whether
the channel has a staged frame (`:504-508`). `expire` walks all `TOTAL_REQUESTS` slots (`:428-446`), about
2N + 4. For most slots it calls `sequence()`, which looks up the channel among N channels and checks every
peer (`:72-92`, `:42-59`). One scan therefore costs about N × (2N + 4) × N steps, roughly N³ in total.

Host measurement (x86-64, MinGW GCC 15.1, `-O3`, `capacity_n*` "turn cost": every ordinary and urgent slot
queued, one unanswered exchange per channel; three runs each, steady mean):

| Build | N = 2 | N = 8 | N = 32 | Native tests |
| --- | --- | --- | --- | --- |
| HEAD | 0.50 to 0.69 us | 4.7 to 5.0 us | 186 to 187 us | |
| A: candidate, return before `expire` in `transmit` when nothing is staged | 0.30 us | 1.4 to 1.5 us | 16.0 to 16.3 us | owner, recovery and capacity tests pass |
| B: A plus one `expire` after the scan | 0.42 to 0.45 us | 1.9 to 2.1 us | 22.2 to 23.3 us | owner, recovery and capacity tests pass |
| C (recommended): skip empty channels in the scan loop, `transmit` unchanged, one `expire` after the scan | 0.40 us | 1.8 to 3.4 us | 20.6 to 21.0 us | **all 140** (`ctest`) pass |

### Why the same work takes longer on the board

The board is not slower at large N; it is given more work per turn. The difference to the desktop is per
instruction:

- Clock: about 4 to 5 GHz against 240 MHz (160 MHz in the bench image), about 20 times (30 times).
- Work per clock: a desktop core runs several instructions per clock, out of order. The ESP32-S2/S3 Xtensa LX7
  core is in order and runs at most about one per clock.
- Caches: the desktop has tens of KB of L1 and megabytes of L2 and L3. The S2 bench image runs code from
  external flash through an 8 KB instruction cache. A miss costs many cycles. The owner's data sits in internal
  RAM (`DRAM_ATTR`), so data is less of a problem than code.

Very rough estimate, not measured: 50 to 100 times slower at 240 MHz with optimized code, and another 2 to 3
times on the bench image (160 MHz, `-Og`). Applied to the host table, that gives these per-turn times on the
board:

- N = 32: HEAD about 9 to 19 ms, C about 1 to 2 ms.
- N = 8: HEAD about 0.25 to 0.5 ms, C about 0.1 to 0.2 ms.
- N = 2: about 25 to 50 us.

So at N = 2 the owner does not explain the 2 to 3 ms bench pass; something else in the pass does. Only
`esp_cpu_get_cycle_count()` around `owner.service()` on the board gives the real figure.

### Review of the candidate fix

**What `expire()` can do.** It can finish owner slots that were never submitted (deadline, invalidated, queue
expired), all at the same `now`. For a submitted slot whose reservation is no longer valid, it calls
`SdoClient::invalidate` (`CanBusOwner.cpp:433-438`). That function retires the channel first, which clears
`queueOccupied`. It then finishes the transfer, so no cleanup abort is kept (`SdoClient.cpp:948-958`,
`:463-470`, `:501-505`). With a network route it does the same for every channel of the node, raises the node
generation, revokes PDOs and calls observers (`adapters/canopennode/Network.cpp:180-240`). Frames are staged
only by `pump` (`SdoClient.cpp:553-581`) and admission, never by `expire`. Best-effort stops are never touched
after submission (`CanBusOwner.cpp:434`, `!slot.outsideSequence`). Before submission they are guarded again by
`markerCurrent` and `checkBestEffortStop` in `eligible()` (`:467-469`). `expire` never clears uncertainty; it
only adds terminals.

**Consequences for candidate A.**

- On a channel with nothing staged, `transmit` returns false with or without `expire` (`:507-508`).
- Every frame that reaches `transport_.transmit` still passes an `expire(now)` just before its handoff
  (`:504`, `:530-536`).
- So A never sends a frame that today's code holds back, never holds back a frame that today's code sends,
  never withdraws a stop and never drops uncertainty. **It is safe for the wire.**

**What A changes.** Inside the scan, a handoff can change state. An enqueue refused by the adapter calls
`transportFailure`, and the channel becomes unresolved (`:550-551`; `SdoClient.cpp:960-971`, `:510-511`). A
refused preflight cancels the request (`:516-523`). Either change can invalidate reservations on peer channels
(`sequence()`, `peersResolved`). Today the next channel's `transmit` expires that change even when the channel
is empty. With A it waits for a later channel with a staged frame, the start loop, or the next turn (`:568`).
Three effects follow:

1. A peer's queued or in-flight work is finished or invalidated up to one turn later.
2. The node invalidation that `expire` would trigger can be delayed. In the next turn, `SdoClient::service`
   runs first and may end that transfer as `TIMEOUT` instead. The transfer still ends unresolved and with
   uncertain effect, but the node generation is not raised.
3. `select()` in the start loop can still see an active peer slot that today's code would already have cleared
   (`:463-465`). A peer best-effort stop's start or invalidation then moves by one turn.

None of these lets an unwanted frame out. But they change terminal reasons and times in rare cascades, and the
change depends on channel order. That breaks the rule "never later than today".

**Smallest safe alternative (C).** Gate empty channels in the scan loop itself and expire once after the scan:

```cpp
    for (unsigned scan = 0; scan < C::SdoClient::CHANNELS && sent < tx; ++scan) {
        const uint8_t c = uint8_t((firstChannel + scan) % C::SdoClient::CHANNELS);
        // expire() never stages a frame, so a channel with nothing staged cannot send.
        const auto* staged = services_.port(c);
        if (!staged || !staged->queueOccupied)
            continue;
        if (transmit(c, now)) {
            ++sent;
            nextChannel_ = uint8_t((c + 1) % C::SdoClient::CHANNELS);
        }
    }
    expire(now); // One expiry for the whole scan, before select() and promote.
```

Every handoff in the scan still runs `transmit`'s own `expire` first. The start loop's `transmit` is unchanged,
so it always expires after an admission. `select()` and the adapter's `promote` now see state that is at least
as fresh as today. It is actually fresher in one case: today, a refusal on the last scanned channel is not
expired before `select()` or `promote`. A frame already handed to the adapter for a reservation that became
invalid later in the same scan is then withdrawn before submission. Today it could still be submitted. That is
the conservative direction, because invalidated work must not send. It is still an observable change, so it
needs a host test and a CHANGELOG line.

Remaining cost after C is one `expire` of O(N²) per call, and one more per received frame (`:665`). At N = 32,
with 8 received frames in a turn, that adds about 50 us on the host, so several ms on the board (estimate).
Next candidates, each needing the same ordering review:

- Make `expire` linear: compute each channel's validity once per call instead of once per slot, or cache each
  slot's channel (`2026-11-12_library_limitations.md:250`).
- Move the per-frame RX expiry to once after the RX loop.

## Proposed prompt steps

**A. Measure first (bench image, example code only; no new device traffic beyond existing finite routes).**

1. Add bounded pass instrumentation to the S2 bench image:
   - passes per second;
   - minimum, maximum and a small histogram of pass duration (1, 2, 3, 5 ms, longer);
   - `esp_cpu_get_cycle_count()` around `pollEvents`, the owner part and the sessions part of `app_.service`,
     `promote`, `evidence_.service` and each bench collector;
   - the submit time and TX-done time, copied in `IdfTwaiDriver::submit` and the TX-done callback.

   Report them in the existing `s` and PONG lines.
2. Run these and record the results above for each:
   - idle;
   - one identity read set;
   - the existing position trial on node 1;
   - a 10 ms TPDO1 watch;
   - a 20 minute read soak.

   Record passes per second, pass time, the drive's reply time (TX done to reply interrupt),
   `maxServiceGapUs`, stack and heap.
3. Correct the docs with the measured figures, recording old claim, evidence and replacement:
   - `2026-11-12_multidrive_preparation.md:58`;
   - `2026-11-12_capacity_configuration.md:37`, `:321`;
   - `2026-11-12_library_limitations.md:245-247`;
   - `2026-11-03_overnight_soak.md:32`;
   - `docs/prompts/51_sync_producer.md:23-24`.

**B. Owner expiry change C (host).**

1. Implement C in `adapters/owner/CanBusOwner.cpp`, with a private comment on why it is safe.
2. Add host tests:
   - (i) a refused enqueue on one channel during the scan withdraws a peer's later staged frame, as today;
   - (ii) a peer frame handed over earlier in the same scan is withdrawn before `promote`, the new behavior,
     asserted explicitly;
   - (iii) a peer's queued and in-flight work ends `INVALIDATED` with this turn's `completedUs`;
   - (iv) a best-effort stop for that node keeps today's eligibility and outcome;
   - (v) no staged frame of an invalidated reservation reaches `transport.transmit`.
3. Run the full native suite and `capacity_n1`, `capacity_n8` and `capacity_n32`.
4. Update `docs/can_owner.md:121-128`, prompt 53's "still owed (a)", the CHANGELOG and Doxygen.

**C. Clock and optimization.** Set 240 MHz and `-O2` in the S2 bench and `probe_idf` defaults. Then:

1. Run the firmware gates.
2. Measure the stack high-water mark again.
3. Repeat step A.2.

**D. Event wake, option (a).**

1. Notify the owner task from TX-done and RX (outside the critical section; not from `on_error`); `idle()` uses
   `ulTaskNotifyTake(pdTRUE, 1)`.
2. Bound the wake rate so the idle task and the console still run.
3. Record in `docs/adapters/twai_sdk.md` that the proof premises are unchanged.
4. Host tests:
   - the driver fake shows one wake per TX-done or RX event, none on error, and the woken flag passed through;
   - the platform fake shows that `idle()` waits at most one tick;
   - the firmware fake shows that a woken pass with an equal timestamp is accepted.
5. Bench:
   - step A.2 again;
   - a back-to-back SDO read burst on both nodes (frames per second, read latency, 7-field cycle time);
   - the short regression of existing finite routes (identity, position, stop);
   - console loss under load;
   - a no-ACK check only if the fixture allows it without a power cycle; otherwise keep it as an open measurement.

**E. Only if D is not enough:** option (b) TX sub-turns, measured. Option (c) gets its own design prompt with
an SDK queue-path review and a new retirement proof.

**F. PDO path (existing backlog, unchanged order):** S13, then D11 and Q17 evidence, then M08 TPDO monitoring.
Prompt 51/54 SYNC trials supply synchronized sampling evidence.

## Not verified here

- No board measurement of pass time, owner cycles, drive reply time or frames per second exists. The 2 to
  3 ms pass is inferred from traces.
- The desktop-to-MCU ratio is an estimate.
- Patched builds A, B and C exist only as disposable scratch output. Nothing was committed.
- Host turn costs are informational and come from one machine.
- The scratch copies came from `HEAD` (`2c5d832`). They exclude the concurrent, uncommitted edits in
  this working tree, for example the test files that derive capacity counts and the prompt 52 and 53 edits.
