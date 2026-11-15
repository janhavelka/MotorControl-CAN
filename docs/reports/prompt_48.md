# Prompt 48 failure and load qualification

The subsequent [fresh prompts 47–49 audit](audit_47_49.md) fixes request/record
correlation, measurement validation and finite host timing. It preserves the
original captures and their qualification limits; checks below describe the
original execution.

## Scope

Started on 15 November 2026 from clean, synchronized `main` at
`5f33272325ba773253d5e9cc17e54974df19c211`. This assignment is prompt 48.
One lead owns COM15 and CAN.
Parallel agents inspect source, host evidence and causal claims without hardware
access or Git mutations.

This stage remains **Partial** while physical interruption, persistence/restart
and related-model campaigns lack their prerequisites. The usable prompt47
artifacts are its verified S2 image, exact two-node read records, settled finite
velocity/quick-stop results, ordinary post-state regression and owner/resource
evidence. They permit the read-only load campaign; they do not qualify physical
motion, electrical behavior, fault injection or the remaining models.

## Plan and source audit

The [finite plan](../bench/20261115-p48/plan.json) fixes the fixture, current image,
read set, load/output variables and failure handling before live work. Save
parameters, restore factory settings and re-apply recorded configuration each
have a count cap of zero in this campaign. No drive parameter, mode, NMT, fault,
motion or PDO command is a setup/cleanup action.

The [source review](../bench/20261115-p48/source_review.json) records fresh sibling
states and exact declarations. RS advanced to
`28e5352e34dd02d9c7d8a815f7934d1e2b30f1ad`; its recorded failure/session files
match prompt47 working bytes. Its finite repeat seam was also inspected. Adopt
bounded session evidence and independent primary/cleanup outcomes; reject RS
recovery/replay, port defaults, ESS semantics and automatic restoration.
ADS1115, MCP45HVX1 and FieldCore are unchanged since47 at the recorded seams.
No sibling was changed.

The final read-only refresh found concurrent RS work at
`1c1a95e05d3a481ea90f4c6b703caccedbf1d4dd` with 12 dirty paths. The changed
`bench_probe.py` adds profile work; the six inspected failure/cleanup seams are
unchanged. The [independent review](../bench/20261115-p48/final_review.json)
records the working-file hashes separately from HEAD.

Original EN86/ZH86/OP86/OP57 pages and EDS hashes were checked. `2300=2` means
save parameters and `2300=1` restore factory settings; no documented restart
command, complete saved set, flash-completion duration, endurance, rearm rule or
host-loss stop guarantee was established. `1017` is producer heartbeat, not proof
of drive reaction to host loss. The duplicated 57C power-up text retains its model
scope uncertainty. No dictionary conflict or qualification flag is promoted.

## Executed work and evidence

Evidence lives in [the campaign directory](../bench/20261115-p48/README.md).
The current board image remains source `bf8a0d9`, ELF SHA
`9ca7701bb4f20efe9c383fd8fe4929e0314980bd2f2f1899652abcaa687b0399`, SDK literal
`v5.5.5-dirty`. No firmware change is needed for host evidence validation.

Fresh ordinary status, monitor and feedback reads on both nodes succeeded with
raw diagnostic copies and explicit terminal releases. Both reported `6041=C050`,
`2100=0015`, mode3 and velocity0, with positions16846/16845. These match the
preceding zero-velocity readback; no physical shaft observation exists.

The existing Soak collector needs 11,008 bytes in one lazy setup allocation.
The motion/console boot had 2,880 free bytes, largest 1,024. One explicit MCU EN
pulse restarted the same image inactive/silence-requested; it did not power-cycle
either drive or prove a remote SDO session was drained. All preceding work was
settled and released. The clean boot had 36,676 free bytes and largest 20,480;
the read-only Soak is allocated before shared console and motion contexts.
No allocation fallback weakens the installed core contract.

The [derived final measurements](../bench/20261115-p48/campaign_results.json)
reconcile the raw endings and both nodes' histogram/outcome counts. Durations
below are MCU elapsed times at the final end, including finite settlement/output.
The runner encoded 599/119/119/119 seconds after deducting setup from the host
budget. D encoded 30 seconds and ended on one explicit `X`.

| Campaign | Read interval / summary interval | Elapsed s | Successful typed reads node1/node2 | Successful SDO steps node1/node2 | Capture |
| --- | --- | --- | --- | --- | --- |
| A baseline | 200 ms / 10 s | 599.130202 | 1497 / 1497 | 5245 / 5245 | Complete |
| B output pressure | 200 ms / 1 s | 119.130080 | 297 / 297 | 1045 / 1045 | Complete |
| C read pressure | 20 ms / 1 s | 119.139084 | 1840 / 1839 | 6446 / 6442 | Incomplete: 64-byte diagnostic deficit |
| C2 deliberate reproduction | 20 ms / 1 s | 119.166079 | 1840 / 1840 | 6446 / 6446 | Complete; does not fix C |
| D host pause / explicit withdrawal | 200 ms / 1 s | 20.238080 | 50 / 50 | 181 / 181 | Complete numbered records; interleaved command reply |

Across these five runs the final summaries report 38,722 successful SDO steps.
All report settled ends and zero typed-read failures, CAN errors, RX loss,
queue/result rejection, bus-off or SDK TX deferral. These counters do not repair
C's incomplete diagnostic record. A/B final service-step means are
5.325-5.379 ms. The largest final step maximum
across runs is 6.807 ms. All settled histogram samples are within the 10 ms bin.
These are admission-to-terminal software measurements, not device response
limits or electrical timings. The maximum owner gap since setup is 5.323 ms;
per-trial Soak gap reaches 4.887 ms. C/C2 record 20,971/20,977 busy deferrals and
1,839/1,840 missed scheduler ticks, exposing load pressure without claiming CAN
loss. No heartbeat was observed; this is not evidence of host-loss motor behavior.

Each final soak summary reports free/minimum heap 25,100 bytes, stack watermark
8,536 and ordinary queue/active/result high-water counts one. A's initial stack
watermark was 9,576 before the workload reduced it. Console waiting-turn
counts rose from 3,305 over A to 6,355 over B and about 6,600 over C/C2. They are
not lost frames. The soak trace ring contains bounded anomaly windows, not every
healthy frame. No anomaly dump was emitted, so dump overhead was not measured.

C has one 64-byte deficit after the S2 console's byte-accounting boundary,
merging records 755/756. Independent accounting finds 116 balanced windows and
one short window. This does not identify the faulty UART/bridge/USB/host component.
Pure host tests ran near the event; that correlation is not a cause. The byte
file logs positive chunks, so gaps between chunks can be quiet periods, not proof
of host suspension. C2 repeated the same firmware/load without intentional
concurrent host tests or builds and retained every numbered record. No firmware
fix or causal conclusion follows from that pass. Simultaneous GPIO43/bridge or
USB observation remains the precise next localization step.

D paused Python reads for 3.000 seconds; 4,495 bytes were waiting on resume.
It sent one `S` and one `X` on the same handle without reconnect, replay or reset.
All 296 numbered records are contiguous. Adjacent byte-accounting windows show
+44/-44 bytes around the exact 44-byte `SOAK_STOPPING` reply. Source review shows
the pending summary line sampled its counter before that command reply was
queued ahead of it; correcting that one observed stream-position shift balances
both windows. The original accounting output is retained. A zero net difference
alone would not establish this explanation. Host pause is not a physical CAN cut;
`X` withdraws read scheduling and does not stop a motor.

The [ordinary public-path regression](../bench/20261115-p48/state_regression.json)
contains six reads before and six after, all OK/SUCCESS without uncertainty.
Both specimens still report status C050, monitor 0015, mode 3, zero velocity,
unchanged positions 16846/16845 and zero input/output monitors. Diagnostic copies
were retained with no reported trace drops; their summaries deliberately retain
`coverage_complete=false`, so they are not a full independent wire capture.
The console allocated successfully after Soak: final free heap 7,200, largest
4,352 and stack watermark 5,544 bytes. Final CAN TX/RX counts are both 38,736,
with zero errors/loss; no workload remains active. The added console waiting
counter is separate from trace loss. No new firmware or drive write ran here.

## Corrected evidence validation

Review found that a `SOAK_BEGIN` plus an early/unsettled `SOAK_END` could be
classified complete without final measurements. The checker now requires the
final summary's interface/resource/console rows and both nodes' outcome,
histogram and abort rows. It rejects a duration end before its declared duration,
keeps unsettled/internal and final node failures as failed evidence, and preserves
missing measurements as incomplete. Observed explicit-stop boundaries cannot be
satisfied by an earlier periodic summary. Absence of a firmware stop marker does
not invent one or prove the phase of an otherwise unmarked summary.

The UART accounting helper also falsely treated unnumbered `SOAK_STOPPING` as
a numbered stream record merely because it shared the `SOAK_` prefix. It now
marks that command-response interleaving as possible without changing any byte
difference, suppressing a loss or converting an accounting failure to a pass.

Tests mutate actual retained firmware records, including missing summaries,
wrong trial, short duration, explicit early stop and honest failure ends. Older
clean, anomalous and damaged captures retain their distinct verdicts. The live
runner's historical result stays with its original parser inputs; corrected
offline review is recorded separately and never rewrites raw evidence.

## Qualification limits and pending work

| Feature | Current scope | Required missing evidence |
| --- | --- | --- |
| CL86-C two-node read load | Finite current-image campaign; controller counters, software timings and bounded diagnostics | Full electrical timing/load limits and physical behavior remain unmeasured |
| Console pressure/host-read pause | Host consumer and read-scheduler behavior only | PC buffering is not a physical CAN cut or host-loss motor stop |
| Physical fault/link interruption and recovery | Unperformed | Controlled link/fault fixture, independent observation/control and exact action plan |
| Save parameters | Unperformed, count0 | Q15 selected field scope, finite save count, independent power-cycle observation and before/after reconciliation |
| Restore factory settings | Unperformed, count0 | Exact scope, route/identity effects and recoverable fixture plan; never a convenient test baseline |
| Re-apply recorded configuration | Unperformed, count0 | Validated supported record and explicit partial-failure reconciliation; no rollback promise |
| OL86-C | Unavailable fixture | Its own identity, motor, feedback, firmware and finite baseline |
| CL57-C | Unavailable fixture | Its own identity, motor, feedback, firmware and finite baseline |
| OL57-C | Unavailable fixture | Its own identity, motor, feedback, firmware and finite baseline |

No maximum measured here becomes a vendor response/stop/recovery guarantee.
Admission-to-terminal latency includes host scheduling, queue and SDK/controller
work. Owner/soak service gaps and output pressure are separate metrics. The
bounded 128-entry soak ring emits anomaly windows, not every frame of a healthy
run; ordinary pre/post captures supply diagnostic samples. An independent CAN
observer is still required for complete wire timing and electrical claims.

## Verification and handoff

The final host sources match [snapshot v2](../bench/20261115-p48/final_host_sources_v2.json).
The retained v1 snapshot predates the numbered `SOAK_NODE_STOP` edge correction;
the independent review's v1 mismatch describes that intermediate state. A final
independent hash check confirmed all eight v2 entries and 26 accounting tests.

Local verification passed using the real full verifier:

```text
python scripts/verify.py --mode full --compiler gcc --build-dir .build/p48-final
```

All 147 selected contract tests passed, followed by the installed-package consumer
and strict Doxygen gates. This includes 118 evidence-parser cases, 41 soak-runner
cases and 26 UART-accounting cases. The experiment-local pause helper separately
passed its 10 cases normally and with `python -O`. The
[verification records](../bench/20261115-p48/verification/full-verifier.json)
retain actual commands, tool versions and timings. Integrity, source/evidence
hashes, staged-byte checks and whitespace checks also passed. Raw device records
remain unchanged; no new firmware was flashed for these host-only corrections.

The final handoff records synchronization on `main` and exact-commit remote CI.
Existing failed records are retained; a later pass does not erase them. No
independent later prompt is executed here. Prompt49 can consume this limited
report, but cannot turn the unperformed campaigns or serial loss into a pass.

Maintainability: reuse the existing Soak, sole owner, serial backend, parser and
offline UART accounting. No second service engine, raw CAN sender, automatic
reconnect, motor replay or new installed API is introduced. The host-pause script
is retained experiment code, explicitly executed and bounded, not production
motion policy.
