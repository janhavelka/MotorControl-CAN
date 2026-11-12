# Prompt 56 Host throughput: faster service passes, event wake and owner turn cost

Apply [COMMON.md](COMMON.md). Prerequisites: [27](27_twai_adapter.md), [42](42_cli_motion_routes.md), [53](53_multi_axis.md).

## Objective and boundaries

The user's questions, verbatim (2026-11-12): "One frame per loop pass - what does this mean? Could i send more? Do i
need polling in practise? Can i easily send more frames per loop? Is it real anyway? You are counting the loop passes at
1ms intervals, right? It can be much faster and therefore fit more polls per 50ms, right?"; "how come mcu would become
'slower'?".

Facts ([host throughput report](../reports/2026-11-12_host_throughput.md)): the TWAI adapter keeps one frame in flight
(the exact TX retirement proof) and learns that it has left only at the next pass, so at most one frame leaves per
pass. The pass is paced by `vTaskDelay(1)` at 1000 Hz and nothing wakes the task on a TWAI event. Retained night
traces (S0001, 320 exchanges) show that during SDO work a pass ran only about every 3 ms (TX report consumed 2997 to
3002 us after handoff), so the host sent at most about 300 frames/s, not the 1000 earlier documents stated; that image
runs at 160 MHz with `-Og`. One read takes about 7 ms, so seven fields take about 49 ms. The bus itself carries 7400 to
9000 eight-byte frames/s at 1 Mbit/s. The owner turn grows about N^3 because `transmit()` repeats an O(N^2) expiry scan
per channel (host: 0.5 us at N = 2, 186 us at N = 32; an S2 is estimated 100 to 300 times slower at this build, so
roughly 10 to 20 ms per turn at N = 32).

Deliver measured pass timing, a shorter pass, an event-woken owner task and a cheaper owner turn, without weakening the
one-frame-in-flight retirement proof; then correct every document that states the old figures.

Boundaries: one frame in flight stays unless a separate design with its own proof replaces it; the owner task stays
the only receiver and keeps its core; the wake never fires on `on_error` (a no-ACK storm raises about 7250 errors/s);
stop priority, invalidation order and uncertainty rules are unchanged or strictly more conservative, and any visible
change is tested and listed in the CHANGELOG; diagnostics are bounded and their own cost is stated.

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read its owner pacing and wake seams; RS is
authoritative for ecosystem conventions. Inspect FieldCore-node read-only for task pacing and notification use. If
HEAD, dirty state and seam match the last recorded row, write "unchanged since prompt N" and do not reread. A sibling
row must name a behavior adopted or rejected, with path and symbol. This does not relax vendor evidence, safety,
uncertainty or verification rules. Read the throughput report, `CanBusOwner.cpp` (`service`, `transmit`, `expire`,
`select`), `TwaiAdapter.cpp`, `IdfTwaiDriver.cpp`, `EspProbePlatform.cpp`, the TWAI SDK proof notes
(`docs/adapters/twai_sdk.md`) and the night soak traces. Apply [CONVENTIONS.md](CONVENTIONS.md),
[MULTIDRIVE.md](MULTIDRIVE.md), [DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and
[DELEGATION.md](DELEGATION.md).

## Implement

1. Measure first (example code only): passes per second, pass time minimum, maximum and histogram, cycles per phase,
   and submit and TX-done timestamps on the bench image; runs: idle, an identity read set, the node 1 position trial, a
   10 ms TPDO1 watch and a 20 minute read soak. Then correct the documents the report lists (the multi-drive
   preparation, capacity study, limitations report, overnight soak report and prompt 51's budget).
2. Owner change "C" from the report: the service scan skips channels with nothing staged, `transmit()` is unchanged,
   and `expire(now)` runs once after the scan before `select()`; every handoff still expires first. The rejected
   variant (returning from `transmit()` before `expire`) is recorded with the reason: it delays peer invalidation by up
   to one turn. Then measure whether the per-frame expiry in the receive loop and a linear `expire` (each slot's channel
   cached at admission) are needed.
3. Build the S2 bench and shared images at 240 MHz with `-O2` (reviewed sdkconfig keys), re-measure the stack
   high-water marks and repeat step 1.
4. Event wake: the TX-done and RX callbacks notify the owner task, `idle()` waits with `ulTaskNotifyTake(pdTRUE, 1)`,
   the wake rate is bounded, and `twai_sdk.md` records that the proof premises are unchanged.
5. Only if step 4 is not enough: several owner sub-turns per wake. More than one frame in flight is out of scope and
   gets its own design prompt.

## Verify and exit

Host tests: a refusal during the scan withdraws a peer's later staged frame; a peer frame handed over earlier in the
scan is withdrawn before `promote` (asserted as the new behavior); peer work ends INVALIDATED with this turn's time; a
best-effort stop keeps its eligibility and outcome; the driver fake wakes once per TX-done or received frame and never
on error; `idle()` waits at most one tick. Run the full suite with `capacity_n1`, `capacity_n8` and `capacity_n32`, the
firmware, stack and documentation gates and CI on the actual final commit. Bench: step 1 repeated, a back-to-back read
burst on both nodes, the identity, position and stop regression, and a console-loss check.

## Documentation and delegation

Update the owner guide (pass timing, turn cost), the TWAI adapter and SDK guides, the firmware guide, prompt 53 item 1
("still owed (a)"), coverage, CHANGELOG and Doxygen in the same stage. A stronger reviewer challenges the owner
ordering change and the wake rules; normal coding agents own the instrumentation and tests. The lead integrates and
runs combined checks. Record `docs/reports/prompt_56.md`. Commit and sync on the current branch before handoff.
