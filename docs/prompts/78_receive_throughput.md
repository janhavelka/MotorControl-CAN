# Prompt 78 Receive throughput up to the S2's practical limit

Apply [COMMON.md](COMMON.md). Prerequisites: [54](54_hardware_sync_multiaxis.md), [55](55_self_repair.md), [56](56_host_throughput.md).

## Objective and boundaries

The user's question, verbatim (2026-11-19): "So the frequency of messages did not improve much - it went from 300hz
to 420hz, right? What about pdos and others? Is the mcu and fw able to reveive and process thousands of samples form
different devices?"; then "what could rise that number to the practical limit of the S2 ? Can we have it
implemented?"

Facts ([prompt 56 report](../reports/prompt_56.md)): 300 to 420 Hz is the owner task's pass rate during a two-node
SDO read soak, not a frame rate. An SDO read is request and response on one channel, about two passes, 4.22 ms on
image `d89fddd4`; the drive answers in about 0.2 ms. Received frames need no host request: the 30-minute 10 ms SYNC
run received 180,000 TPDOs per node plus heartbeats (about 200 frames/s) with a complete capture. Above that nothing is
measured. The receive path has fixed bounds: the driver copies each frame in its RX callback into a 16-entry FIFO
(`IdfTwaiDriver::RX_CAPACITY`; full drops the newest and latches loss), the owner routes at most `RX_LIMIT` (8) frames
per turn, and each routed frame runs a full `expire()` (O(N²)), health refresh, trace and observer call. A loss episode
takes the interface `OFFLINE` until a retired recovery (prompt 55). So the sustained rate is bounded by about
8 / (pass time) and by the per-frame owner cost, and an overload is a hard fault, not a slowdown.

Raise the loss-free receive rate toward the S2's practical limit, which is expected to be the bus (about 7400 to
9000 eight-byte frames/s at 1 Mbit/s) if the per-frame cost falls to roughly 30 to 50 us at 240 MHz (an estimate to
measure, not a claim). Measure first, then apply the levers below in order, measuring after each, and answer the user
with numbers. The bench's two drives can produce only about 2000 to 2500 frames/s (heartbeats at 1 ms and TPDOs), so
the bench proves loss-free reception up to that; above it the report extrapolates from the measured per-frame cycles
and states the extrapolation. A full-bus test needs an external generator (the PC's PCAN-USB) and is a deferred owner
step. Boundaries: one frame in flight on TX and the TWAI proof premises stay; no RX path may drop a frame
silently; diagnostics are bounded and their own cost is stated.

During the owner's absence (2026-11-20 to about 2026-12-04,
[COMMON](COMMON.md#owner-absence-20-november-to-about-4-december-2026)) every write is RAM only, read back and
restored in the same session: `1017`, `1800:02` 1/255 and `1800:05` 0/100 are allowed (COMMON rule 3); PDO maps,
COB-IDs and the `1800:03` inhibit time are not.

## Read and audit

Read the prompt 56 report, `CanBusOwner::service` (the receive loop and its per-frame `expire`), `IdfTwaiDriver`
(`rxDone`, `receive`, `takeTiming`), `TwaiAdapter::pollEvents`, the bench `TIMING` lines and `PassMeter`, the PDO
routes in `examples/s2_bench/PdoCommand.h`, the heartbeat experiment (`h`/`j`) and the reviewed `1017` and TPDO
evidence in [canopen_services.md](../canopen_services.md). Refresh MotorControl-RS HEAD and its receive-path seam
per COMMON; FieldCore-node read-only only if it changes owner integration. Apply [CONVENTIONS.md](CONVENTIONS.md),
[MULTIDRIVE.md](MULTIDRIVE.md), [DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and
[DELEGATION.md](DELEGATION.md).

## Implement

1. Load sources available without new hardware, each RAM only and restored: heartbeat producer period on both drives
   (`1017`, reviewed U16 ms; a bounded write-and-restore route for chosen periods such as 100, 10, 5, 2 and 1 ms,
   with readback), SYNC at 10 and 5 ms with both TPDO1 at type 1 (the 10 ms inhibit time `1800:03` caps each TPDO1 at
   100 Hz; changing it needs reviewed evidence first), and their combination. State the expected frame rate of each
   step from the configuration, then measure it.
2. Measure at each step: received frames per second per ID, driver RX FIFO high-water mark, RX drops and loss
   episodes, owner passes per second, pass work histogram, owner phase cycles and per-frame owner cost (add a bounded
   counter if the existing `TIMING` lines cannot separate it). Stop a step at the first loss episode and record it.
3. The levers, in this order, each measured on the bench and on the host before the next:
   1. Per-turn instead of per-frame owner work. The per-frame `expire` exists so that a frame which invalidates a
      node, route or sequence (a boot-up, a heartbeat state change, a consumed loss) ends dependent work before the
      next frame in the same batch is routed; moving it after the loop could let a stale reply complete. Keep that
      order: skip the per-frame `expire` only when the frame changed no node, route, sequence or recovery state (a
      cheap generation or dirty check), with an independent ordering review and host tests for the cascade (a
      boot-up followed by a reply in one batch). Then health refresh and presence once per turn where their
      contracts allow, trace copies bounded, and a linear `expire` (each slot's channel cached at admission).
   2. Drain what the driver buffered on each wake with a bound on turn length instead of the fixed `RX_LIMIT` of 8,
      and a larger driver FIFO (for example 64 entries; state the RAM cost). An overflow stays a loss episode.
   3. Hot receive path in IRAM: the TWAI RX callback and its copy, the owner's receive routing; then the TWAI ISR in
      IRAM (`CONFIG_TWAI_ISR_IN_IRAM`, which the shim refuses today) only if its cache-safety requirements are met and
      reviewed. Measure IRAM/DRAM cost against B01.
   4. A hardware acceptance filter derived from the configured routes, so foreign IDs raise no interrupt; documented
      as a configuration, never hiding frames the application asked for.
   5. Bench and application reporting at a fixed rate instead of every pass, so a pass is short; the console keeps
      its loss accounting.
   More than one TX frame in flight is out of scope (it needs its own proof); it does not limit reception.
   Host tests assert the observable ordering of every owner change and that no frame is dropped silently.
4. Report the highest loss-free rate with its producers, per-frame cycles before and after each lever, CPU headroom,
   the projected rate at the bus limit, and the remaining limiting factor, with evidence for each claim. Add the
   full-bus generator test to the deferred owner steps.

## Verify and exit

Host tests for every owner change; full suite with `capacity_n1`, `capacity_n8` and `capacity_n32`; firmware, stack
and documentation gates; CI on the final commit. Bench: the load steps above with their records, each restored, plus a
short identity, soak and SYNC 10 ms regression on the final image. A loss episode on the bench is a finding, not a
failure to hide; recover it with the explicit `recover()` path only.

## Documentation and delegation

Update the owner guide (receive budget, per-frame cost), the TWAI SDK guide (RX FIFO bounds), the S2 bench README, the
CHANGELOG and Doxygen in the same stage, and record `docs/reports/prompt_78.md` with the answer to the user's question.
One independent review of any owner ordering change; normal coding agents own instrumentation and tests. Commit and
sync on the current branch before handoff.
