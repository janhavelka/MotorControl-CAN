# Prompt48 finite failure/load campaign, 15 November 2026

See [plan.json](plan.json) for pre-action bounds and [report48](../../reports/prompt_48.md)
for qualification limits. This is the existing S2/two-CL86-C fixture. No other
model, physical observer, power-cycle operator or independent CAN instrument
became available. The standing unattended authorization remains separate from
observed device behavior.

`00` checks image/resources. `01` retains six successful ordinary public state
blocks with raw diagnostics. `02` is a host argument error before port access;
it reset nothing. `03` is the one actual MCU EN restart, with original boot bytes
and same-image identity. It is not a drive restart or persistent reload.

`04` is campaign A: requested600s,200ms operation interval,10s summaries, full
accepted read set. The actual wire duration is599s because the helper uses the
remaining wall-clock budget. Later campaigns are admitted individually only
after the preceding outcome is reviewed. Nothing automatically repeats a failure.

The optional [host pause experiment](host_pause.py) requires `--execute` and a new
output directory. Its plan is30s of200ms full reads with1s summaries. After the
observed begin, Python pauses reads once at5s for3s, then resumes the same handle;
at20s it sends one `X` to stop read scheduling. The whole host window is55s and
firmware duration remains30s. No reconnect, additional X, CAN cut or motor stop
is inferred. Output zero means an end was captured; the ordinary parser and
campaign review still judge settlement, timing, counters and completeness.

Raw bytes, original verdicts and counter evidence are immutable. A corrected
offline review is a separate artifact. Software timing and bounded anomaly
windows are not full electrical wire capture or physical qualification.

The initial `04-A-replay.json`/`04-A-account.json` inspected the still-running
capture and correctly found no end yet. They are retained as partial snapshots.
The `04-A-final-*` records inspect the finished file; only those are final evidence.

The bounded live commands were:

```text
python scripts/s2_soak.py --port COM15 --serial 94A990398B40 --duration-s 600 --interval-ms 200 --summary-s 10 --reads full --max-trials 1 --reenumerate-s 0 --output docs/bench/20261115-p48/04-A-baseline
python scripts/s2_soak.py --port COM15 --serial 94A990398B40 --duration-s 120 --interval-ms 200 --summary-s 1 --reads full --max-trials 1 --reenumerate-s 0 --output docs/bench/20261115-p48/05-B-console
python scripts/s2_soak.py --port COM15 --serial 94A990398B40 --duration-s 120 --interval-ms 20 --summary-s 1 --reads full --max-trials 1 --reenumerate-s 0 --output docs/bench/20261115-p48/06-C-readload
```

These paths are historical and exclusive; do not reuse them for a new run.
The interval is the shared alternating read scheduler's interval, not a separate
rate per node. B changes summary frequency; C then changes read frequency. D has
its own [pre-action plan](07-D-plan.json) and fake tests (`host_pause_test.py`).
Both models of timing remain software measurements. `SOAK_SUM` owner counters
and owner gap are cumulative since owner setup; node counts, soak gap and
console waiting counters restart per trial. Compare owner counter deltas.

C is incomplete: one 64-byte host deficit after the console accounting boundary.
[C2's separate plan](06-C2-plan.json) repeats the same case once without
intentional concurrent host tests/builds. Its clean result is not a causal fix.
D's exact 44-byte command reply explains the two +44/-44 accounting windows;
`07-D-reviewed-account.json` corrects only the interleaving hints, preserving the
original values. The report and independent review explain the source ordering.

`08` records post-load cached resources. `09` opens the shared console without
CAN traffic; `10` runs six ordinary state blocks, then `11` records the idle
owner and final resources. [state_regression.json](state_regression.json) compares
pre/post values. [campaign_results.json](campaign_results.json) contains derived
final counts and measured software timings. Nothing in these files qualifies
physical motion or a related model.

The source hashes in `load_review.json` describe the initial checker review.
The subsequent UART accounting hint correction has a separate final source
snapshot, [final_host_sources_v2.json](final_host_sources_v2.json). V1 predates
the numbered `SOAK_NODE_STOP` edge correction; the independent review records
that intermediate mismatch, and its final follow-up confirmed all eight v2 hashes.
Original review records, captured bytes and initial verdicts stay intact.
