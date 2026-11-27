# Ramp-law analysis of the retained captures (prompt 60, 2026-11-27)

This directory retains the analysis behind the ramp law in the
[prompt 60 report](../../reports/prompt_60.md) ("Ramp law evidence") and the
[speed units guide](../../speed_units.md). It reads retained captures only: it opens no device, sends nothing and
uses no network. The scripts use the Python standard library only (run with Python 3.12.10).

A read-only agent wrote the first version of the scripts in stage (a); slice b1 reviewed them, fixed their sign
handling, added `fit.py` (the table) and reran everything from the repository root. The stage (c) fix round counted
reverse position starts and dropped a duplicated dump. The results below come from that rerun; the differences
from the stage (a) numbers are listed in [Differences](#differences-from-the-stage-a-report).

## Inputs

- Every loose `docs/bench/**/rx.txt` (1273 files) and every `rx.txt` member of the 536 archives in
  `docs/bench/20261111-night/packed/`, `docs/bench/20261113-ladder/packed/` and `docs/bench/20261116-hil/packed/`,
  as present at `47841ca1`. Archive members are read in memory; nothing is extracted. `summary.json` field
  `inputs.sha256` is the SHA-256 of the sorted list of `path sha256` lines of these 1809 files, so a later capture
  changes it.
- Kept: the 1503 records that write `6040`, `60FF`, `6081`, `609A` or `6099`, joined into 496 node timelines. An SDO
  event already seen in the same group (same node, object, value and MCU times) is a second dump of the same
  traffic and is dropped: `20261116-p52/52-ramp-up-cached-dump` repeats trial 52020, and
  `20261116-p52/107-enable-cached-dump` repeats 106.
- For the three HALT runs, the later record that holds the standstill `6064` (named in `fit.py`, `HALT_CASES`):
  `20261105T1541Z-rearm-halt-node1-710`, `20261110T1037Z-s18-p1-node1-1508` and `20261110T1044Z-s19-p1-node2-1614`.
- Specimens: the two CL86-C drives, nodes 1 and 2. Every identity upload in the loose captures (54 from node 1, 47
  from node 2) reads `1000=00040192`, `1009=0C`, `100A=20`. Subdivision `2201` is written by separate setting
  records and is not in these timelines; it is inferred per run from the reported rate (below).

## Method

| Step | Script | What it does |
| --- | --- | --- |
| 1 | `parse.py` | Pairs each SDO request with its reply per node; keeps confirmed downloads and expedited uploads with their byte count and the request and reply times (MCU microseconds) |
| 2 | `ana.py` | Joins consecutive records (one archive, or one UTC day of loose captures, in name order, each starting less than 120 s after the previous one ends); per node, the `6064` and `606C` samples and every `6040`/`60FF` write with the motion objects' state. `6064`, `606C`, `607A` and `60FF` are sign-extended from the bytes transferred |
| 3 | `ramp.py` | Cuts each timeline into segments at the start, change and stop writes and fits a cruise line to each start or change |
| 4 | `fit.py` | The law fits, the speed changes, the HALT and quick stops; writes `summary.json` and `runs.csv` |

**Timing and resolution.** A sample is timed at the middle of its SDO request/reply window (median half width
1.1 ms, largest 4 ms); a write at its request. Counts are exact. One count is 1 / r seconds of a cruise line at r
counts/s: 3 ms at 20 rpm, 10 ms at 6 rpm, 0.2 ms at 300 rpm (at `2201=1000`). Polls are 0.5 to 5 s apart,
so a run gives one intercept, not the curve shape.

**The D intercept.** For a start from rest (`606C` = 0 at the last sample before the start), the cruise samples are
the first unbroken run of `6064` samples whose nearest `606C` (within 25 ms) lies within 0.975 |V| - 3 to
1.01 |V| + 3 rpm. A least-squares line x = a + r t goes through them, dropping leading samples while the first
residual exceeds 4 + 0.004 |r| counts. D is the time from the start write t0 to the line's crossing of the start
position x0. A linear ramp from rest lasting T after a reaction L gives D = L + T/2; a fixed acceleration per
1000 rpm (the alternative law) gives D = L + T V / 2000. Position starts begin at the controlword bit 4 rise, and
their speed carries the sign of the displacement (`607A` for a relative edge, the target minus the start position
for an absolute one), because `6081` is unsigned while `606C` is signed: both directions count;
velocity starts at word `000F`, with x0 taken before the enable words, as in stage (a). `6064` stepped by +15 and
-18 counts at the enable in the two runs checked by hand (`20261116-p52/31-velocity-change-node2` and
`S0018-n1-vc30to60`), and the in-ramp sample of the first fits a ramp from the pre-enable position, not from the
stepped one.
The law fits use |V| >= 20 rpm, where one count is at most 3 ms; slower runs are reported separately.

**Start velocity.** A ramp that starts at a start velocity Vs (the OP86 p9 figure) instead of rest gives
D = L + T/2 - (Vs/2) T/V; a three-term fit over the same position runs estimates Vs.

**Reported count rate.** The cruise rate r is compared with floor(|V| x `2201` / 60) counts/s. `2201` is taken as
the nearest 100 to 1000 x r / (0.992 x floor(|V| x 1000 / 60)); the 0.992 is the night-soak factor of the
[units re-audit](../../reports/2026-11-13_units_reaudit.md) section 3. Position runs count only when their cruise
spans at least 2000 counts (one count is then 0.05 %).

**Speed changes.** Two cruise lines at the model rate (median velocity ratio 0.99201 x floor(|V| x 1000 / 60)),
each through the last cruise sample at its speed, cross D12 after the `60FF` write. A linear change lasting Tc after
a reaction L gives D12 = L + Tc/2. The resolution adds one count at each anchor, the SDO windows and the spread of
the velocity rate ratios (0.075 %) over the extrapolation.

**Stops.** Dstop is the stop distance from the cruise line at the stop write, expressed as time at cruise speed: an
instant stop after L gives L, a linear ramp-down lasting Td gives L + Td/2. HALT runs use the last cruise sample at
the model rate and the later standstill record. Quick stops (`000B`) use the `ramp.py` line and the last two equal
samples of the stop segment; a stop whose last sample before the stop word was not at cruise speed is excluded.
The speed-change and HALT steps assume `2201=1000`; the displacements of those runs (about 1000 counts per turn, for
example 4940 counts in 5.1 s mostly at 60 rpm) exclude the higher subdivisions.

## Results

All numbers are in `summary.json`; per-run values in `runs.csv`.

| Case | Result | Basis |
| --- | --- | --- |
| Position start from rest | D = 17.25 ms + 0.5042 x T(`6083`) at 20 rpm and above (278 runs, rms 1.4 ms, largest 5.3 ms); 348 runs on both nodes (143 forward, 205 reverse), 2 to 600 rpm (three reverse runs at 600 rpm, 1000 ms), `6083` 100 to 1000 ms except one run at 2000 ms (trial 52020); the 70 runs below 20 rpm lie within 42 ms of the line | `position_start` |
| Which object | 191 runs at 20 rpm and above with `6083` != `6084`: a fit to `6083` has rms 1.4 ms, a fit to `6084` rms 172 ms | `unequal_ramps_at_or_above_20_rpm` |
| Alternative law | A fixed rate per 1000 rpm (D against T V / 2000) fits at rms 142 ms: excluded | `fixed_rate_law_D_vs_T_times_V_over_2000` |
| Start velocity | -0.18 rpm, standard error 0.02 rpm: no positive start velocity; the data fit the EN86/ZH86 p11 figure (from rest), not OP86 p9 | `start_velocity_fit` |
| Velocity start from rest | D = 465.6 ms + 0.5015 x T at 20 to 4500 rpm (229 runs, rms 7.3 ms, largest 30 ms); 460.3 ms + 0.5042 x T at 3001 rpm and above (23 runs, rms 0.6 ms, largest 1.1 ms); all 314 runs (6 to 4500 rpm, 10 of them negative, T 200 to 2000 ms, `2201` 1000 to 51200): 469.1 ms + 0.4993 x T, rms 22 ms, largest 102 ms at 6 to 15 rpm. The intercept is a start latency of about 0.465 s after word `000F`. `6083` = `6084` in every run. 4500 rpm completed once (node 2, 2000 ms); above 3000 rpm there are 23 runs | `velocity_start` |
| Ramp duration | 2 x slope: 1.0083 T in position mode, 1.003 T (20 rpm and above) and 1.0084 T (3001 rpm and above) in velocity mode | `ramp_duration_over_T*` |
| Reported count rate | r / floor(rpm x `2201` / 60): median 0.99201; velocity runs at `2201=1000` 0.99161 to 0.99235 (188 runs, 20 to 4500 rpm), at higher subdivisions 0.99197 to 0.99210 (41 runs), position runs 0.99157 to 0.99237 (194 runs) | `count_rate` |
| Change between two nonzero speeds | 7 runs on both nodes, velocity mode, `6083` = `6084`: 30 to 60 and 60 to 30 rpm at T = 500 ms, 6 to 60 rpm at T = 200 ms. D12 - T/2 = 16.5 to 18.8 ms in every run (resolution 8 to 15 ms): each change takes the full T, so a = abs(dV) / T. The old guide formula (a = V x 1000 / T with V the commanded speed) needs a reaction of 144 ms for 30 to 60 rpm and is contradicted; for 60 to 30 rpm both give T | `speed_change` |
| HALT | Trial 709 (position, 300 rpm, `6084` = 500 ms): Dstop = 17.7 ms + T/2. Trials 1507 and 1613 (velocity, 30 and 60 rpm, `6084` = 200 ms): 14.1 and 15.1 ms + T/2. `6083` = `6084` in all three | `halt` |
| Position arrival | 99 position runs reached the target; the polls bracket the arrival 0.5 to 2.5 s wide (median 1 s). 83 brackets contain the end of a `6084` ramp-down, 74 that of a `6083` one, 60 even an instant stop: not resolved | `position_arrival` |
| Quick stop (`000B`) | 170 runs on both nodes (162 in velocity mode, 8 in position mode), 6 to 60 rpm: Dstop median 5.8 ms, quartiles 4.1 and 7.2 ms; median 5.8, 6.4 and 5.7 ms with `6084` = 200, 500 and 1000 ms: the quick stop does not follow `6084` | `quick_stop` |
| Homing ramp | No retained record writes `609A` or `6099` | `homing_ramp_writes_609A_6099` |

## Differences from the stage (a) report

- **Sign handling (fixed).** The stage (a) `ramp.py` converted values a second time after `ana.py`, so a negative
  speed or position became a value near -2^32 and targets of 32768 to 65535 became negative. Its author noted that
  this corrupted runs S0018 and S0023 and did not use them: the two 30 to 60 rpm changes of the 2026-11-16 campaign,
  `S0018-n1-vc30to60` and `S0023-n2-vc30to60`, carry these numbers and are the only speed-change runs with negative
  positions. With the fix they are included: 7 speed-change runs instead of 5, with the same result. The 10 negative
  velocity starts now carry their real speed; their D values were already right.
- **Velocity law.** Stage (a) reported D = 465 ms + 0.502 x T over 314 runs with rms 13 ms. The rerun gives
  465.6 ms + 0.5015 x T, rms 7.3 ms at 20 rpm and above (229 runs) and 469.1 ms + 0.4993 x T, rms 22 ms over all 314;
  the stage (a) summary step was not retained, so its exact selection cannot be reproduced. The law (the ramp is the
  time from rest to the commanded speed, about 0.465 s after word `000F`) is the same.
- **Rate range.** Stage (a) reported 0.99197 to 0.99210 above 20 rpm. That range holds for the 41 runs at
  subdivisions above 1000; at `2201=1000` the shorter cruises spread from 0.99161 to 0.99237. The median 0.99201 is
  unchanged.
- **Ramp duration.** Stage (a) gave 1.008 to 1.012 x T; the rerun gives 1.0105 (position), 1.0084 (velocity at
  3001 rpm and above) and 1.003 (velocity at 20 rpm and above).
- **Reverse position starts and a duplicate (stage (c)).** `ramp.py` compared the sign of `606C` with the unsigned
  `6081`, so every reverse position start failed its cruise test and was dropped; and the cached re-dump of trial
  52020 counted it twice. With both fixed the position set has 348 runs instead of 144 (205 reverse), and only one
  run (52020, 2000 ms) lies above 1000 ms. The law is unchanged: 17.25 ms + 0.5042 x T against 17.05 ms +
  0.5052 x T; the position ramp duration becomes 1.0083 x T (was 1.0105).
- The HALT and quick-stop results, the speed changes and the homing count are reproduced.

## Outputs

- `summary.json`: the table above, with the input identity.
- `runs.csv`: one line per run used: `fit` (which result: `position`, `position-low` below 20 rpm, `velocity`,
  `velocity-low`, `change`, `halt`, `quick`, `quick-not-cruising`), `kind` (segment), `source` (timeline: first
  record and the number of records joined to it), node, trial numbers, `t0_s` (MCU seconds), `rpm`, `rpm_before`
  (changes), `ramp_6083_ms`, `ramp_6084_ms`, `subdivision_inferred`, cruise points, cruise span and median poll
  interval in seconds, `rate_counts_per_s`, `rate_ratio` (r / floor(rpm x `2201` / 60)), `D_ms`, `D12_ms`, `Dstop_ms`.

Not retained, because the commands below regenerate them: the intermediates `recs.json` (about 4.6 MB),
`rows.json` (1.3 MB) and `ramp.json` (0.5 MB). Also not retained from the stage (a) copy: `summ.py` (grouped medians,
superseded by `fit.py`) and `render.py` with its three PNG renders of EN86 and ZH86 p11 and OP86 p9; read those pages
in the original PDFs in [docs/reference/vendor](../../reference/vendor/).

## How to rerun

From the repository root, with the intermediates in the ignored `.build` directory:

```text
python -I docs/bench/20261127-p60-ramp-analysis/parse.py docs/bench .build/p60-ramp/recs.json
python -I docs/bench/20261127-p60-ramp-analysis/ana.py .build/p60-ramp/recs.json .build/p60-ramp/rows.json
python -I docs/bench/20261127-p60-ramp-analysis/ramp.py .build/p60-ramp/rows.json .build/p60-ramp/ramp.json
python -I docs/bench/20261127-p60-ramp-analysis/fit.py .build/p60-ramp/recs.json .build/p60-ramp/rows.json .build/p60-ramp/ramp.json docs/bench docs/bench/20261127-p60-ramp-analysis
```

Create `.build/p60-ramp` first. Two consecutive runs produce identical `summary.json` and `runs.csv`. The
directory is `-text` in `.gitattributes`; the scripts write LF line endings.

## Limits

- Drive reports only (`6064` counts, `606C`): no shaft measurement, one firmware, two specimens; nothing is
  qualified. The library's boxes lie inside the bounding boxes of the measured points (lead decision, stage (c)):
  position 2 to 300 rpm with `6083` 100 to 1000 ms (above 1000 ms there is one run), velocity 6 to 3000 rpm with
  200 to 2000 ms (4500 rpm completed once, and 4000 rpm faulted FF05 with 200 ms ramps on 2026-11-13). Inside a
  box not every corner was measured: velocity at 6 to 10 rpm only with 200 ms ramps, 60 to 300 rpm only with
  1000 ms, and above 1000 rpm almost only with 2000 ms; position cells are all populated. A move too short to reach
  its speed never shows the law: its effective acceleration is inference.
- `6083` = `6084` in every velocity start and every speed change, so which object sets the velocity-mode ramp is
  not shown.
- Deceleration is unresolved: three HALT runs (one in position mode) fit "`6084` is the time from the speed to
  rest", and the position arrivals are bracketed only by 1 s polls. Other HALT segments whose standstill lies in a
  later record (for example trial 2106 with 2107, and 5857 with 5858 at 2344 rpm, T = 2000 ms) were not analysed.
- Speed changes: 7 runs, 6 to 60 rpm, T 200 and 500 ms, all with `6083` = `6084`.
- Homing ramps (`609A`): no data.
- The 0.992 rate and the 0.3 to 1 % longer ramp are observations; their cause is unknown.
- Records are joined in name order, which can split a sequence (trial 710 sorts after 711 to 713), so a stop whose
  standstill is in a separate record needs the explicit list in `fit.py`.
