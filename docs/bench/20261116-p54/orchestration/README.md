# Single-node typed change procedure for review

This is an offline plan, not an authorized live executor or a physical result.
The lead alone owns COM15. See [report54](../../../reports/prompt_54.md) and
[diagnostics](../diagnostics.md). The current runtime change path is ordinary
`ProbeConsole -> ProbeMotion -> AxisSession`; this procedure does not use raw
position-change `Tp`.

Print exactly one proposed phase, without port access:

```text
python docs/bench/20261116-p54/orchestration/single_node_changes.py.txt move-speed
python docs/bench/20261116-p54/orchestration/single_node_changes.py.txt move-ramps
python docs/bench/20261116-p54/orchestration/single_node_changes.py.txt velocity
```

These are three independent candidate trials. Do not execute all three as a
batch or fill missing evidence from these sample commands. The 500/500 ms
initial ramps, replacement 200/800 ms and rates 6/12 rpm are bounded experiment
choices, not device timing guarantees. Only one phase is to be selected after
review. Fresh position may differ from the reported 35981; use 38240 only while
it remains the reviewed forward endpoint with adequate travel margin.

The reported released mode 1/status 4050 is insufficient for move arming. A
separate reviewed enable must first own the acknowledged 000F, and its record
and any required stop predecessor must be retained. TY requires the exact
reviewed acceptance/completion masks, values and rule, all eight fresh identity
values, actual start position, evidence record and explicit stop budgets.
No identity, signal, power-up observation or release predecessor is guessed.
For velocity, separately establish released mode 3, then TB with maximum 12 rpm,
500/500 ms ramps, 500 ms poll and QUICK_STOP; the initial request is 6 rpm.
Use 500 ms observation spacing for TY too. This is an application choice, not a
drive timing claim. Confirm quiet RX and no competing SYNC producer before
enabling raw console traces: two 100 Hz TPDO streams can exhaust UART/evidence
capacity. This plan never reconfigures PDOs to make its diagnostics fit.

The lead's one serial session must:

1. Inspect actual PONG routes 15/image identity, caps, controller counters and
   unresolved history. Check the persistent campaign under its exclusive lock.
   Reserve all proposed motion effects before any movement. A move trial needs
   two MOVE admissions (original plus replacement) and one STOP; a velocity
   trial needs one VELOCITY, its native STOP and the separate planned STOP.
   Any enable or mode preparation is additional. Refusals do not refund.
2. Set local host intent, arm the selected kind with a complete TY2/TB2 and
   `--allow-changes`, retain `changes=1/changeRecord=<trial>`, drain its full
   END and release its route record. Keep one reader. Move immediately within
   the original arming freshness cap; never refresh timestamps in host code.
3. Use `bench_probe.Console.begin()` for the start with immutable response
   deadline and pre-handoff cutoff. Retain actual request, operation and axis
   IDs. Keep pumping, not blocking for the original terminal. `paired_motion`
   shows the existing admission/cutoff pattern; it cannot execute one-axis work.
4. At the explicit planned change time, make at most one change offer using
   the actual operation ID, only while original admission is known and it has
   no terminal. Native fresh-running/phase admission remains authoritative;
   a clock delay alone proves no running state. Refusal is retained failure,
   never an instruction to wait and retry. No new endpoint is permitted.
5. At the original host preparation-time cutoff, hand off `stop 2 fast` once,
   even after change refusal, if the stream remains healthy and before its
   original final deadline. Use a new retained response budget bounded by that
   final deadline. Do not wait for motion/change results before offering the
   planned stop. A damaged/poisoned stream sends nothing further; use the
   attendant's independent cutoff and retain the missing stop as a failure.
6. Collect the stop first, then both original/replacement results, or the
   original velocity and its retained change evidence. Keep first failure and
   stop outcome separately. Release only exact observed retained results after
   saving them. A failed or uncertain result is not permission to retry.

The existing software does not yet express this whole procedure safely:
`Campaign` validates only T-route sends and exactly-two-axis paired reservations;
there is no single-axis console reservation for these three effects. Do not log
a T line that was not sent, invent a paired second axis, or reset a ledger.
`s2_bench_console --no-terminal` has no returned-ID substitution or finite
cutoff scheduler. `bench_probe.Console` rejects bench ASCII after its handshake,
so it cannot directly consume TY/TB notices/dumps. A future narrow executor must
finish that bounded arming phase before handing the same stream to the strict
console client, or use an explicitly reviewed shared demultiplexer; it must not
relax either parser or discard received bytes. No live executor is supplied here.

A speed and a ramp change are separate trials. `ProbeMotion` retains an original
and replacement; it refuses another replacement while the original is retained.
This plan does not release an original mid-motion to evade that bound. The move
keeps its original 10 s native deadline; the velocity keeps its original 12 s stop
due and 10 s final tail. Host cutoffs include setup, so slow preparation can
legitimately leave no change opportunity. Trace loss, missing terminal, refused
change, fallback-only stop or uncertain execution prevents qualification. Raw
feedback may show a response, but owner enqueue times and sequential SDO reads
are neither wire timing nor measured mechanical interruption.

For an admitted move replacement, the original's retained REPLACED interruption
and its relation to the new operation are expected evidence; do not require the
original to report ordinary target completion. An explicit stop may intentionally
interrupt the replacement or velocity. Change qualification still needs the
exact acknowledged changed write and the native post-change observations before
the stop; a mere accepted change receipt is insufficient. Planned stop evidence
requires its own sequenced acknowledgement and observed completion. Best-effort
000B alone does not qualify standstill or successful stop.
