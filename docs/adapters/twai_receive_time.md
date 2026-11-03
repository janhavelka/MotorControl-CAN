# Conservative TWAI receive-time bounds

ESP32-S2/S3 provide no physical receive timestamp in the pinned ESP-IDF 5.5.5
driver. `receivedTimeKnown` stays false. The application shim supplies a separate
inclusive interval `[receivedEarliestUs, receivedLatestUs]` when it can prove
both endpoints. All times use the owner's expanded monotonic microsecond domain;
zero is valid. The interval concerns completion of an accepted CAN frame, not
its start bit or the time at which the device sampled its payload.

`receiveBoundsKnown` distinguishes this evidence from exact capture and unknown
time. Both time-known flags may not be true together. Processing time remains
separate. The portable core acquires no clock and includes no SDK headers.

## Lower and upper evidence

The shim checks an empty ingress boundary inside its existing same-core critical
section, with a live enabled controller, empty copied RX ring and no pending
fault, loss, bus-off or retirement. It samples `esp_timer_get_time()` **before**
checking that the hardware RX message count is zero. If the FIFO is nonempty,
the prior boundary is retained. A frame completing after a successful check
cannot precede the sampled lower bound. An in-progress frame may have started
earlier, which is why this is explicitly a completed-frame bound.

Each RX callback copies the established lower bound and samples the clock after
the SDK supplies the frame. That observation is the upper bound; it is never
promoted to exact arrival. Previously copied frames retain their original
intervals even when a later empty check establishes a newer boundary. No fixed
interrupt latency, timeout, quiet interval or service gap is assumed. A delayed
interrupt simply produces a wider interval and weaker qualification.

The driver defers an RX entry whose upper bound exceeds the transport's accepted
service-time cutoff. It leaves that entry in the ring; it neither clamps the time
nor drops the frame. The adapter supplies its previous service time to this
cutoff, which can add one cooperative turn of latency. Normal FIFO dispatch and
configured budgets still apply. Repeated deferral or a zero RX budget can consume
an operation deadline; a pre-deadline arrival cannot reopen terminal work.

Restart discards the old boundary. Negative or backwards clock evidence latches
a driver fault. Software-ring overflow follows the existing explicit fault path.

## Pinned source proof

The exact sources and original-byte hashes remain in
[twai_sources.json](twai_sources.json), at SDK commit
`b774170ff46c393eeb5e495ea37936038d3f4f4f`.

| Evidence | Pinned source | Consequence |
| --- | --- | --- |
| No hardware receive timestamp | `components/hal/twai_hal_v1.c:382-389` | Exact physical time stays unknown. |
| Optional timestamp fallback reads the ESP timer during receive parsing | `components/esp_driver_twai/esp_twai_onchip.c:651-663` | ISR observation is only an upper bound. The shim samples it explicitly. |
| SDK pops FIFO into its private buffer, then calls RX callback synchronously | `esp_twai_onchip.c:258-278` | The same-core task cannot sample an empty boundary while an older SDK buffer is waiting for its callback. |
| S2 controller zero is `TWAI`; RX count reads only the volatile counter register | `components/hal/esp32s2/include/hal/twai_ll.h:29,784-787` | A zero-count probe reads without consuming frames or clearing interrupts. |
| S3 supplies the corresponding controller-zero/count primitives | `components/hal/esp32s3/include/hal/twai_ll.h` | The same narrow proof applies under the existing S3 owner-core restriction. |
| Public node information omits RX FIFO occupancy | `esp_twai_onchip.c:557-573` | A public status query alone cannot prove this boundary. |
| ESP timer returns microseconds from a coherent system-counter snapshot | `components/esp_timer/include/esp_timer.h:217-221`, `components/esp_timer/src/esp_timer_impl_systimer.c:63-75`, `components/hal/systimer_hal.c:45-69` | ISR and task reads use the same expanded clock. HAL retries inconsistent split-register reads; no hard read-duration bound is claimed. |

This deliberately adds a private LL dependency in the application adapter.
The SDK warns that LL APIs are not public application interfaces; their use here
is confined to the pinned, read-only occupancy probe. No FIFO release, reset,
interrupt-status clear, hidden controller restart or remote command is used.
An SDK or target change requires reviewing this proof again. The existing
[same-core lifecycle proof](twai_esp32s2.md) and configuration exclusions apply.

## Network qualification

`SdoClient::receive(frame, earliestUs, latestUs, nowUs)` validates
`earliestUs <= latestUs <= nowUs` before mutation. Network snapshots and history
retain the interval separately from exact capture.

For bounds `[L,U]` and maximum age `A`, freshness is `FRESH` only when
`now-L <= A`; it is `EXPIRED` only when `now-U > A`. Between these cases it is
`UNQUALIFIED`. Producer interval unknown/disabled remains independently reported.
Post-invalidation and post-NMT-handoff qualification requires `L` strictly after
the relevant boundary. Processing-time deadlines continue to win at equality.

Nonoverlapping intervals establish ordering. Overlapping contradictory NMT
states withdraw exact-state and ordinary-service qualification, revoke dependent
PDO eligibility and retain an interval covering both observations as an ordering
fence. `nmtOrderingUncertain` makes the retained state historical. A subsequent
observation must be demonstrably later than that fence to qualify again. Legacy
exact-capture comparisons retain their existing equal-timestamp behavior.

Bounds do not prove a reply belongs to a particular SDO request, qualify a reused
PDO route or establish device sampling age. SDO retains its existing external
correlation premise and processing-time deadlines. A receive interval wholly
before its handoff is additional stale evidence and quarantines that channel.
Intervals do not become exact capture in SDO/PDO observations. TX success and a
subsequent NMT observation remain separate; the interval does not prove causation,
drive enablement, motion completion or a host-loss watchdog response.

## SDO service qualification

A checked SDO reply records service evidence (`NodeSnapshot::sdo` and
`serviceQualification`) from bounds `[L,U]` under the rule that applies to an
exact capture, with `L` standing for the receive time:

| Boundary | Exact capture `R` | Bounds `[L,U]` |
| --- | --- | --- |
| Request handoff | `R < handoff` quarantines; equality records | `U < handoff` quarantines; `L <= handoff` settles the request but records nothing |
| Node boundary after configuration | `R >= boundary` | `L >= boundary` |
| Node boundary after invalidation (strict) | `R > boundary` | `L > boundary` |
| Possible NMT transition | Current only when `R > transition` | Current only when `L > transition` |
| Ordering against retained evidence | Replaces unless `R` is older | Replaces unless `L` is older; `U` is never used |
| Service age | `now - R` | `now - L`, the oldest possible receive time |

An interval that straddles a boundary therefore does not qualify. An inverted or
future interval is rejected before any mutation. A reply that settles its request
without qualifying leaves both records unchanged. The handoff row is deliberately
stricter for bounds: `L` equal to the handoff time could include the instant of
handoff, while an exact capture at that instant is a timestamp-resolution tie the
existing contract already accepts. SDO matching, quarantine, settlement, unknown
time and passive NMT handling are unchanged. NMT already used `L` for the
boundary, transition and age rules above.

Until 2026-11-03 an interval-only reply never qualified service. On the S2
fixture, which has no exact capture, ordinary SDO work then needed a heartbeat
that reports exact NMT state, and the session's writes, enable words and
sequenced stop were refused `INVALID` (packets 4 and 11). The decision came from
two soaks on the attended fixture (COM15, two CL86-C specimens, one firmware
image): [20261103-soak-night2](../bench/20261103-soak-night2/summary.txt), 200 ms
interval for two hours, 63,531 completed steps per node, and
[20261103-soak-stress1](../bench/20261103-soak-stress1/summary.txt), 20 ms
interval, 246,197 completed steps on node 1 by its 04:27 UTC summary. Neither
recorded a timeout or an unmatched frame. Reviewing those traces, the lead found
each reply's interval was about 1.6 ms wide and started after its own handoff.
This note does not recompute that width: the retained summaries hold counters and
latency histograms, not per-frame bounds. Soaks are communication evidence only,
not electrical, sustained-load or motion qualification.

Residual risk. Bounds are host processing windows, not wire time. On the S2, `L`
is the shim's last proven-empty FIFO check and `U` is the ISR's clock sample
after the SDK supplies the frame. Neither is a hardware timestamp. `handoff` is
the owner's processing time, so a reply can follow the handoff timestamp and
still precede the actual controller submission by a short margin. Qualification
therefore keeps the existing sole-client and no-delayed-duplicate premise; it does
not prove which request a reply answers. A wider interval only delays
qualification and ages it sooner. A change of SDK, target, ISR placement or
shim boundary needs this review again.

## Verification and limits

`test/twai_driver_test.cpp` compiles the actual shim against narrow SDK fakes.
Its timing cases exercise delayed callbacks, pending hardware FIFO despite an
empty software ring, arrivals on either side of the empty check, future-frame
deferral, restart and invalid clocks. `test/network_receive_time_test.cpp` checks
age boundaries, interval ambiguity, lifecycle/NMT fences, unchanged SDO deadlines,
stale-response quarantine and every SDO qualification row above.
`test/bounded_service_test.cpp` runs the session's writes, enable words and
sequenced stop on bounds-only ingress through the real owner, and the same refusals
for unknown time and handoff-reaching intervals. These are host contracts, not
physical timing measurements.

The SDK may lose frames on hardware FIFO overrun without a public loss report;
this change does not establish complete reception. Continuous ingress may prevent
a recent empty boundary. In that case timing qualification expires conservatively.
Electrical behavior, service gaps and actual NMT/heartbeat behavior still require
the authorized fixture and retained raw traffic. No such measurement is implied
by this source proof or by a successful compile.
