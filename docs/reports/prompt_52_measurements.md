# Prompt 52: motion-change measurements, 2026-11-16

Offline review of retained raw `RESULT` and `TRAFFIC` records. All motion observations below are
drive reports from **node 2, one CL86-C specimen and firmware tuple**:
`1000=00040192`, `1009=0C`, `100A=20`, `1018:01..03=1000/1/0`, `2000=0400`, `2001=0200`.
The operator records explicitly say the shaft was not observed. These results establish neither
physical displacement nor family-wide qualification. Ordinary capability gates remain a separate
review decision.

The requests identify COM15 / serial `94A990398B40`, ESP32-S2, ESP-IDF 5.5.5 and the user's motion
waiver. The velocity image reports ELF `92d5f9c3e18d30af1c6d96d3cdf65c0ae895d25584ecc03e9f71179245bf1fc0`;
the position image reports `7e64e3d4f8177d96591830a3c83694b8128e3a123d6c9b7106df724ce0d51e8c`.
Both name base `6775a7ffab26bd161227258ca6e8d5d0e0165ad7` with uncommitted source changes, rather
than claiming the base alone identifies the image. Requests, flash records and original bytes are
under [20261116-p52](../bench/20261116-p52/).

## Method and limits

For each record, group successful uploads of `2100`, `6041`, `6064` and `606C` by native operation;
velocity operation 12 contains several cycles, separated by their next `2100` upload. Decode
`6064`/`606C` as signed little-endian I32. Match every download to its `TRAFFIC k=1` payload and its
`RESULT dir=1` operation/step, admission interval and acknowledgement. Download `RESULT` data is
empty; it is **not** the written value. The payload supplies that value.
For the ten trials including long case 5, all 877 acknowledged `RESULT` terminals also match exactly one
node-2 `TRAFFIC` response within their admission/completion interval, with the same index/subindex
and expedited response width/data (or download `60` acknowledgement). This checks the retained
record correlation, not physical application of a download.

Times below are S2 processing microseconds. Relative cycle times span the first `2100` admission
through the last `606C` completion, relative to the initial trigger's TX record. These are sequential
read windows, not simultaneous samples, physical event times or measured response deadlines.
Position cycles are about 500 ms apart; velocity running cycles about 5 s apart. Transitions between
them cannot determine a ramp shape or sub-cycle latency.

Raw status masks: bit 10 is `0400`, bit 12 is `1000`; `5037` has bit10=0/bit12=1, `9437` 1/1,
`4037` 0/0 and `8437` 1/0. In the velocity trial, `C437` has bit10=1 while `606C` is nonzero and
`2100=0004`: **bit 10 does not establish standstill in this observed mode-3 state**.

## Disposition of the reviewed trials

| Trial / case | Narrow observed disposition | What remains unresolved |
| --- | --- | --- |
| 52013, velocity 30 to 60 | **Works:** one acknowledged `60FF=60` during the positive run, followed by a 60-rpm report; its original quick stop settled | Rate-change latency/ramp law, tolerance, decreasing changes and HALT after a change |
| 52017, position case 1 | **Ignored for the current curve:** bare `6081=60` acknowledged while the 30-rpm move continued at reports 30/31 | Stored/next-curve effect; not a claim that the write itself was rejected |
| 52020, position case 2 | **Ignored for the current acceleration:** bare `6083=500` acknowledged during the original 2000-ms ramp; reports continued 7,15,22,30 | Other phases/rates and next-curve effect; no fitted ramp law |
| 52022, position case 3 | **No observable change to the current deceleration:** bare `6084=2000`; arrival progression matches the short-ramp comparison trials | Fine transition shape and which ramp a later moving stop would use |
| 52024, position case 4 | **Works as a rearm observation:** `001F` to `000F` cleared bit12 while the move continued to its endpoint | Does not establish a subsequent `003F`/buffered trigger, acceptance attribution or queue depth |
| 52026, position case 5 | **Works for reacceptance; an observable trajectory change:** same endpoint with `6081=60`, fresh `000F`/`003F`, bit12 cleared then set and an earlier arrival report | No 60-rpm sample was captured; the short remaining distance cannot establish the new plateau or transition shape |
| 52028, position case 6 | **Works for the selected extension:** new target 31079 with `003F`; reports pass the old 30479 endpoint and end at 31080 | Bare `607A` behavior, shortened targets, reversal and unsampled transition shape |
| 52030, position case 7 | **Works for a deferred second target:** new 32480 with bit5=0; reports decelerate near old 31880 then move beyond it, unlike case 6 | Exact stop at the first endpoint, buffer depth/full behavior and completion at the second target; this trial stopped before it |
| 52032, position case 8 | **Ignored for the current curve:** bare `607A=33840` acknowledged with `001F` held; reports end at original 33240 | Retention/activation on a subsequent command and other target relations |
| 52034, long position case 5 | **Works:** same-endpoint immediate speed change yields 56,60,60,58,60-rpm reports; moving quick stop from `003F` settles | Nonzero-speed ramp formula, exact transition latency/shape, HALT after a change and other firmware |

No listed change was refused by the drive. Acknowledgement and observed effect are distinct above.

## Velocity trial 52013

Source: [31-velocity-change-node2/rx.txt](../bench/20261116-p52/31-velocity-change-node2/rx.txt),
with its [request](../bench/20261116-p52/31-velocity-change-node2/request.json).
Operation 12 wrote target 30, ramps 500/500 and words 6/7/15. The initial `000F` TX was
`317458006`, ACK `317462010`. No second enable/controlword accompanied the rate change.

| Native operation / step | Exact node-2 request payload | TX / ACK processing time (us) |
| --- | --- | --- |
| 12 / 49: `60FF=60` | `23 FF 60 00 3C 00 00 00` | `323292008` / `323296010` |
| 13 / 1: quick stop `000B` | `2B 40 60 00 0B 00 00 00` | `329307008` / `329310010` |

| Operation / cycle purpose | Relative window (s) | `6041` (bit10/12) | `2100` | `6064` | `606C` |
| --- | --- | --- | --- | --- | --- |
| 12, first enabled/running report | 0.720–0.771 | `C037` (0/0) | `0004` | 16886 | 16 |
| 12, last cycle before change | 5.775–5.826 | `C437` (1/0) | `0004` | 19375 | 28 |
| 12, first cycle after change | 5.843–5.894 | `C037` (0/0) | `0004` | 19409 | 30 |
| 12, later running cycle | 10.898–10.949 | `C437` (1/0) | `0004` | 24315 | 60 |
| 13, first stop cycle | 11.856–11.907 | `C050` (0/0) | `0014` | 25222 | 0 |
| 13, second stop cycle | 16.911–16.962 | `C050` (0/0) | `0015` | 25222 | 0 |

The post-change count slope is `(24315-19409)/(328400010-323345010)` = about 970.5 counts/s,
using the actual `6064` completion times; that interval includes the unsampled acceleration.
It is not a cruise estimate or a physical rpm measurement. The second rate appears by the later
sample, not in the first post-ACK cycle. Bit12 stayed clear in these velocity samples.

The run retained `started=317301009`, `due=329301009`, `deadline=337301009`;
`stopAt=329302006`. The quick stop TX was 6.0 ms after due. Its retained terminal says
`state=2/outcome=1/completion=1/uncertain=0`, with two stop observations. This is the run's own
quick stop, not a HALT test. Record loss/drop counters and TEC/REC were zero; maximum service gap
was 5999 us. The summary's `TRIAL_LAST` repeats the admission position; the operation-specific raw
uploads above supply the actual running and stop observations.

## Position cases 1–4

All initial triggers were `2B 40 60 00 1F 00 00 00`; each change was one additional acknowledged
download after cycle 2. The original target and positive rate were retained throughout each trial.

| Trial / source | Original trigger TX | Change operation / step; exact payload | Change TX / ACK (us) |
| --- | --- | --- | --- |
| 52017 / [41-bare-speed-node2](../bench/20261116-p52/41-bare-speed-node2/rx.txt) | 185879967 | 18/1, `6081=60`: `23 81 60 00 3C 00 00 00` | 186439967 / 186443967 |
| 52020 / [52 cached dump](../bench/20261116-p52/52-ramp-up-cached-dump/rx.txt) | 115938967 | 10/1, `6083=500`: `23 83 60 00 F4 01 00 00` | 116499967 / 116503967 |
| 52022 / [60-position-case3](../bench/20261116-p52/60-position-case3/rx.txt) | 19807967 | 10/1, `6084=2000`: `23 84 60 00 D0 07 00 00` | 20368967 / 20372967 |
| 52024 / [68-position-case4](../bench/20261116-p52/68-position-case4/rx.txt) | 19779967 | 10/1, `6040=000F`: `2B 40 60 00 0F 00 00 00` | 20336967 / 20343967 |

Case 2's [original capture](../bench/20261116-p52/51-bare-ramp-up-node2/rx.txt) was incomplete.
The [cached composition analysis](../bench/20261116-p52/ramp-up-cached-analysis.json) joins the
original prefix to the cached dump of the **same trial 52020**, with original/cached hashes and no
repeated CAN request. Main records retain all 95 terminals and 295 traffic entries; cached
post-trial tail reports `tail=16/tailLost=1`, so that tail is not complete evidence. Nothing below
depends on that tail.

Each cell below is **native operation: `6064` / `606C` / `6041`**. The sequential windows begin at
roughly the stated relative times and last about 45 ms; exact raw times remain in the linked files.
The first column is comparable time, not a shared clock between separate trials.

| Window start after trigger | 52017: bare speed, target 26021 | 52020: bare acceleration, target 27820 | 52022: bare deceleration, target 28080 | 52024: falling edge, target 28880 |
| --- | --- | --- | --- | --- |
| 0.007 s | 16: 25221 / 0 / `5037` | 8: 26020 / 0 / `5037` | 8: 27280 / 0 / `5037` | 8: 28080 / 0 / `5037` |
| 0.506–0.507 s, before change | 17: 25355 / 30 / `5037` | 9: 26053 / 7 / `5037` | 9: 27415 / 28 / `5037` | 9: 28216 / 28 / `5037` |
| 1.007–1.008 s | 19: 25602 / 30 / `5037` | 11: 26149 / 15 / `5037` | 11: 27662 / 30 / `5037` | 11: 28464 / 30 / `4037` |
| 1.507–1.508 s | 20: 25850 / 31 / `5037` | 12: 26306 / 22 / `5037` | 12: 27910 / 30 / `5037` | 12: 28711 / 30 / `4037` |
| 2.007–2.010 s | 21: 26015 / 3 / `5037` | 13: 26524 / 30 / `5037` | 13: 28075 / 5 / `5037` | 13: 28874 / 5 / `4037` |
| 2.507–2.510 s | 22: 26020 / 0 / `9437` | 14: 26771 / 30 / `5037` | 14: 28080 / 0 / `9437` | 14: 28880 / 0 / `8437` |
| 3.007–3.012 s | 23: 26020 / 0 / `9437` | 15: 27020 / 31 / `5037` | 15: 28080 / 0 / `9437` | 15: 28880 / 0 / `8437` |
| 3.507–3.512 s | 24: 26020 / 0 / `9437` | 16: 27269 / 30 / `5037` | 16: 28080 / 0 / `9437` | 16: 28880 / 0 / `8437` |

`2100=0004` accompanies the moving rows; completed rows have `0001`. In cases 1–3 bit12 remains
set after the trigger and at completion. In case 4 the falling-edge ACK precedes the first
`4037` report, bit12 stays clear, position continues to 28880 and the final report is `8437`.
The sampled progression is inconsistent with a lasting stop at the falling edge, but cannot rule
out a brief unsampled transient. Case 1 arrives one count below its requested endpoint; the report
does not silently round that observation to an exact arrival.

Each trial then sent its explicit `000B`. Cases 1, 3 and 4 had already reported arrival; these are
stationary stops, not evidence about stopping a changed running curve. Case 2 stopped while still
moving: operation 17 sent `000B` at `119497967`, ACK `119500967`; later operation-17 feedback
reported `6064=27280`, `606C=0` twice, at 120.084 and 120.631 s. The selected released-running
latch report was `4050/0014`. All four stop contexts reported observed settlement. Main record
loss/drop counters and TEC/REC were zero; maximum service gap was 7000 us.

The 500/2000-ms comparison separates these coarse current-curve effects only. It establishes no
nonzero-speed ramp formula, stop deceleration law, exact delay, buffered capacity, same-ID PDO
freshness or physical shaft behavior. The immediate/buffered and target-replacement observations
below do not remove those limits.

## Position case 5: immediate same-endpoint speed change

Source: [76-position-case5/rx.txt](../bench/20261116-p52/76-position-case5/rx.txt), trial 52026,
same position image and node-2 firmware as cases 1–4. Initial target 29680 from 28880, 30 rpm,
ramps 500/500. The `001F` trigger was TX `19734967`, ACK `19737967`.

| Operation / step | Exact request payload | TX / ACK processing time (us) |
| --- | --- | --- |
| 10 / 1: falling edge `000F` | `2B 40 60 00 0F 00 00 00` | `20294967` / `20298967` |
| 12 / 1: `6081=60` | `23 81 60 00 3C 00 00 00` | `20793967` / `20797967` |
| 12 / 2: same `607A=29680` | `23 7A 60 00 F0 73 00 00` | `20800967` / `20804967` |
| 12 / 3: immediate trigger `003F` | `2B 40 60 00 3F 00 00 00` | `20807967` / `20811967` |
| 18 / 1: quick stop `000B` | `2B 40 60 00 0B 00 00 00` | `23299967` / `23302967` |

| Native operation | Relative cycle window (s) | `6041` (bit10/12) | `2100` | `6064` | `606C` |
| --- | --- | --- | --- | --- | --- |
| 9, before falling edge | 0.507–0.555 | `5037` (0/1) | `0004` | 29017 | 28 |
| 11, after `000F`, before new writes | 1.008–1.053 | `4037` (0/0) | `0004` | 29265 | 30 |
| 13, after `003F` | 1.509–1.554 | `5037` (0/1) | `0004` | 29560 | 30 |
| 14 | 2.009–2.054 | `9437` (1/1) | `0004` | 29679 | 0 |
| 15 | 2.509–2.557 | `9437` (1/1) | `0001` | 29679 | 0 |
| 16 / 17 | 3.010–3.055 / 3.510–3.558 | `9437` (1/1) | `0001` | 29679 | 0 |

The fresh bit12 absence precedes `003F`, followed by a fresh presence. The endpoint report is one
count below the requested target, not an exact arrival. It appears about 0.5 s earlier than the
otherwise comparable 800-count/30-rpm cases 1, 3 and 4. The interval from positions 29265 to 29560
is 295 counts in about 0.501 s, compared with roughly 247–248 counts per 0.5 s before any immediate
trigger. That is an observable change in the sampled trajectory, consistent with the speed update.
No report reaches 60 rpm: sampling and short remaining distance do not determine whether a
60-rpm plateau occurred, or whether the transition briefly stopped and restarted.

At operation 14 the sequential `6041`/feedback report indicates completion/zero while the earlier
`2100` read still says running; operation 15 subsequently reports in place. Do not merge these
sequential fields into one instantaneous drive state. The later quick stop retained predecessor
`003F` and reported settlement, but the move had already arrived: this is **stationary** stop
evidence only. Loss/drop counters and TEC/REC were zero, maximum service gap 7000 us.

## Position case 6: immediate whole-target extension

Source: [84-position-case6/rx.txt](../bench/20261116-p52/84-position-case6/rx.txt), trial 52028.
Start 29679, original target 30479, replacement target 31079; rate 30 rpm, ramps 500/500.
Initial `001F` TX/ACK were `19762967`/`19765967`.

| Operation / step | Exact request payload | TX / ACK processing time (us) |
| --- | --- | --- |
| 10 / 1: `000F` | `2B 40 60 00 0F 00 00 00` | `20319967` / `20323967` |
| 12 / 1: `607A=31079` | `23 7A 60 00 67 79 00 00` | `20820967` / `20824967` |
| 12 / 2: `003F` | `2B 40 60 00 3F 00 00 00` | `20827967` / `20831967` |
| 18 / 1: `000B` | `2B 40 60 00 0B 00 00 00` | `23321967` / `23324967` |

| Native operation | Relative cycle window (s) | `6041` (bit10/12) | `2100` | `6064` | `606C` |
| --- | --- | --- | --- | --- | --- |
| 9, before falling edge | 0.507–0.552 | `5037` (0/1) | `0004` | 29815 | 28 |
| 11, after falling edge | 1.007–1.052 | `4037` (0/0) | `0004` | 30064 | 30 |
| 13, after new edge | 1.507–1.552 | `5037` (0/1) | `0004` | 30312 | 28 |
| 14, beyond original endpoint | 2.007–2.052 | `5037` (0/1) | `0004` | 30559 | 30 |
| 15 | 2.507–2.552 | `5037` (0/1) | `0004` | 30807 | 30 |
| 16 | 3.007–3.052 | `5037` (0/1) | `0004` | 31035 | 18 |
| 17 | 3.507–3.552 | `9437` (1/1) | `0001` | 31080 | 0 |

This establishes the selected new endpoint was effective, with a final +1 count discrepancy;
the old endpoint was passed with a nonzero velocity report. No lasting stop appears in the sampled
transition. There is no observation between `607A` and `003F`, so this trial does **not** establish
that a bare target write is inert. It cannot establish shortened-ahead or reversing targets,
relative updates or another firmware's behavior. The later `000B` again follows observed arrival,
so it is stationary-stop evidence from predecessor `003F`. All 83 terminals match retained raw
responses; loss/drop counters and TEC/REC were zero, maximum service gap 7000 us.

## Position case 7: deferred whole-target extension, bit5=0

Source: [92-position-case7/rx.txt](../bench/20261116-p52/92-position-case7/rx.txt), trial 52030.
Start 31080, original target 31880, replacement 32480, rate 30 rpm, ramps 500/500. Initial `001F`
TX/ACK were `19831967`/`19834967`.

| Operation / step | Exact request payload | TX / ACK processing time (us) |
| --- | --- | --- |
| 10 / 1: `000F` | `2B 40 60 00 0F 00 00 00` | `20388967` / `20392967` |
| 12 / 1: `607A=32480` | `23 7A 60 00 E0 7E 00 00` | `20889967` / `20893967` |
| 12 / 2: deferred trigger `001F` | `2B 40 60 00 1F 00 00 00` | `20896967` / `20900967` |
| 18 / 1: quick stop `000B` | `2B 40 60 00 0B 00 00 00` | `23394967` / `23397967` |

| Native operation | Relative cycle window (s) | `6041` (bit10/12) | `2100` | `6064` | `606C` |
| --- | --- | --- | --- | --- | --- |
| 11, after falling edge | 1.007–1.052 | `4037` (0/0) | `0004` | 31463 | 28 |
| 13, after deferred edge | 1.507–1.552 | `5037` (0/1) | `0004` | 31710 | 30 |
| 14, near original target | 2.007–2.052 | `5037` (0/1) | `0004` | 31874 | 5 |
| 15, beyond original target | 2.509–2.554 | `5037` (0/1) | `0004` | 31962 | 22 |
| 16 | 3.009–3.054 | `5037` (0/1) | `0004` | 32206 | 28 |
| 17, before quick stop | 3.511–3.556 | `5037` (0/1) | `0004` | 32434 | 18 |
| 18, stop feedback 1 / 2 | 4.104–4.150 / 4.652–4.697 | `4050` (0/0) | `0014` | 32440 | 0 |

The dip to 5 near the first target, followed by 22 beyond it, distinguishes this selected deferred
trigger from case 6: its immediate trigger already reported a position 80 counts beyond the first
target at about 2.05 s with velocity 30. No sample proves an exact full stop at 31880, and no
bit15/`2100` completion sample was captured at that boundary. The selected second target is
effective, but its completion was not measured: `000B` interrupted the reported motion, leaving
32440, 40 counts short of 32480. This is a **moving quick stop from `001F` after a deferred change**,
not a moving stop from `003F`. One admitted second target cannot establish queue capacity or what
happens when full. All 97 terminals match raw responses; loss/drop counters and TEC/REC were
zero, maximum service gap 7000 us.

## Position case 8: bare target write

Source: [100-position-case8/rx.txt](../bench/20261116-p52/100-position-case8/rx.txt), trial 52032.
Start 32440, original target 33240, rate 30 rpm, ramps 500/500. Initial `001F` TX/ACK were
`19811967`/`19814967`. Operation 10/step 1 downloaded `607A=33840` as
`23 7A 60 00 30 84 00 00`, TX `20372967`, ACK `20376967`, about 0.561 s after the trigger.
No falling or new rising edge accompanied it.

| Native operation | Relative cycle window (s) | `6041` (bit10/12) | `2100` | `6064` | `606C` |
| --- | --- | --- | --- | --- | --- |
| 9, before target write | 0.507–0.552 | `5037` (0/1) | `0004` | 32575 | 30 |
| 11, after target write | 1.008–1.053 | `5037` (0/1) | `0004` | 32823 | 30 |
| 12 | 1.508–1.553 | `5037` (0/1) | `0004` | 33072 | 30 |
| 13 | 2.010–2.055 | `5037` (0/1) | `0004` | 33236 | 5 |
| 14 | 2.510–2.555 | `9437` (1/1) | `0001` | 33240 | 0 |
| 15 / 16 | 3.012–3.057 / 3.512–3.557 | `9437` (1/1) | `0001` | 33240 | 0 |

The reported endpoint stays at the original 33240, rather than the acknowledged new value 33840.
This establishes no effect on this current curve. It says nothing about which stored target a
subsequent edge would use. The later stationary `000B` TX/ACK were `23375967`/`23378967`;
stop settlement was reported. All 81 terminals match raw responses; loss/drop counters and TEC/REC
were zero, maximum service gap 7000 us.

## Longer case 5: sampled 60-rpm plateau and moving stop from `003F`

Source: [109-long-speed-change/rx.txt](../bench/20261116-p52/109-long-speed-change/rx.txt),
trial 52034, same position image/specimen. Start 33240, target 38240, initial rate 30 rpm,
ramps 500/500. The extra distance is an explicit native-count target, with no inferred shaft scale.
Initial `001F` TX/ACK were `152764967`/`152767967`. The unchanged trial cutoff was
`due=158640869`, stop end `160140869`; its eight cycles requested the explicit stop earlier.

| Operation / step | Exact request payload | TX / ACK processing time (us) |
| --- | --- | --- |
| 10 / 1: `000F` | `2B 40 60 00 0F 00 00 00` | `153321967` / `153325967` |
| 12 / 1: `6081=60` | `23 81 60 00 3C 00 00 00` | `153824967` / `153828967` |
| 12 / 2: same `607A=38240` | `23 7A 60 00 60 95 00 00` | `153831967` / `153835967` |
| 12 / 3: `003F` | `2B 40 60 00 3F 00 00 00` | `153838967` / `153842967` |
| 18 / 1: moving quick stop `000B` | `2B 40 60 00 0B 00 00 00` | `156328967` / `156331967` |

| Native operation | Relative cycle window (s) | `6041` (bit10/12) | `2100` | `6064` | `606C` |
| --- | --- | --- | --- | --- | --- |
| 9, before falling edge | 0.507–0.552 | `5037` (0/1) | `0004` | 33375 | 30 |
| 11, after falling edge | 1.009–1.054 | `4037` (0/0) | `0004` | 33623 | 30 |
| 13, after immediate edge | 1.510–1.555 | `5037` (0/1) | `0004` | 33975 | 56 |
| 14 | 2.010–2.055 | `5037` (0/1) | `0004` | 34469 | 60 |
| 15 | 2.510–2.555 | `5037` (0/1) | `0004` | 34965 | 60 |
| 16 | 3.012–3.057 | `5037` (0/1) | `0004` | 35463 | 58 |
| 17, before quick stop | 3.512–3.557 | `5037` (0/1) | `0004` | 35959 | 60 |
| 18, stop feedback 1 / 2 | 4.105–4.150 / 4.652–4.700 | `4050` (0/0) | `0014` | 35981 | 0 |

The 58–60 cruise reports and approximately 494–498-count increments per 0.5-s window establish
the selected speed change took effect beyond the short case's limited sampling. No new mode or
enable word was sent. The final moving sample is 2281 counts short of target, so this quick stop
was **during motion from `003F`**, unlike the stationary stops in cases 5 and 6. The stop retained
predecessor `003F`, `documented=0`, and three observations; state/outcome/execution/completion were
`2/1/1/1`, uncertainty clear, with no best-effort attempt. Feedback later reported the same
35981 count and zero velocity twice. The 22-count difference from the last moving sample includes
the unsampled interval before the stop; it is not a stopping-distance measurement.

All 98 terminals match retained raw responses, 304 traffic entries were retained, loss/drop
counters and TEC/REC were zero and maximum service gap was 7000 us. The preceding enable's
original capture had inconsistent duplicate output trailers; its separate
[cached composition analysis](../bench/20261116-p52/enable-cached-analysis.json) preserves both
captures and hashes without repeating a CAN request. Trial 52034 has its own fresh in-trial state
checks and complete retained records.

These initial trials close only their observed cases. They do not establish Q26's nonzero-speed
ramp formula, a changed-ramp moving HALT, decreased velocity changes, shortened/reversing targets,
buffer depth/full behavior, buffered second-target completion, another specimen/firmware or
prompt 54's finished-operation hardware qualification. The mode-3 bit10 observation above is a
confirmed limit on interpreting that bit as zero speed, not a reason to replace the separate
`606C`/monitor stop evidence. No further code-gate contradiction was established by these records.
