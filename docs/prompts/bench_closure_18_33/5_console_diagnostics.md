# Packet 5 Implement bounded diagnostics for console loss

Apply [COMMON](COMMON.md). Prerequisite [1](1_evidence_contracts.md). Originals
28-29; independent of packets2-4. Deliver runnable diagnostic software and a
finite reproduction. A definitive bridge-versus-host cause needs a second
capture path; do not promise that existing same-path evidence can identify it.

## Inspect

Read `docs/reports/2026-11-05_motion_and_stress.md` and its retained R1-R6
captures, the earlier A/B report, `examples/common/EspProbePlatform.cpp`,
`examples/s2_bench/Soak.*`, `scripts/s2_soak.py`, `s2_soak_test.py`,
`s2_soak_transcript.py`, `s2_evidence.py` and tests. Refresh RS runner/raw-evidence
validation and ADS1115 bounded owner service; do not import RS timing values.

Current evidence: keepalive did not fix loss. At one-second summaries, 20 ms
reads still lost transfers, while 200 ms reads had none in 10,791 bursts. The
earlier long-idle explanation is refuted. Whole 64-byte deletions are observed
in host captures; the exact failing component still needs independent capture.
CAN read success does not certify the completeness of the console record.

## Implement

1. Add a small offline comparison tool in `scripts/` for an independent UART byte
   stream and existing `rx.bin`, with optional timestamp metadata. Require an
   explicit capture format; support one documented binary/CSV format first,
   not a framework for every analyzer. Keep original inputs unchanged.
2. Report the first defensible aligned divergence, deletion/duplication/splice
   ranges, missing tail, corrupt lines and unalignable/ambiguous regions. Repeated
   text may admit multiple alignments; return ambiguity rather than a fabricated
   exact location. Cross-device clocks need an explicit shared marker or bounds;
   do not subtract unrelated timestamps as precise latency. Bound input size and
   memory/time; avoid an unbounded whole-capture quadratic diff.
3. Extend diagnostics only where they add an observable distinction between
   formatting rejection, console admission, UART FIFO acceptance and host receipt.
   Reuse line numbers/counters and existing trace loss handling. Keep service
   budgets and record the diagnostic overhead; UART acceptance is not host receipt.
4. Test identical data, a literal 64-byte deletion, partial-line deletion, repeated
   identical chunks, inserted/duplicated data, corruption, truncation, clock offset,
   missing markers, oversized input and unavailable external capture. Replay at
   least one retained damaged burst and one clean burst through the actual parser.
5. Run a short finite 200 ms baseline and a bounded 20 ms reproduction using the
   existing soak runner, new evidence directories and the same image/settings
   except the tested variable. Predeclare duration/caps. A short run with no loss
   is inconclusive, not a fix. Do not start another overnight campaign by default.

## Verify and finish

Register meaningful offline tests with the existing verifier. If changing
firmware, build and flash the exact tested artifact and run the short read
regression. Preserve failing evidence and report CAN versus console outcomes
separately. If an independent UART receiver/logic analyzer at GPIO43 is not
available, finish software and emit the exact connection/capture format needed;
leave bridge/USB/host attribution unresolved. Do not change host USB power policy
or fabricate another device as a substitute measurement.

Delegate analyzer test vectors separately; a reviewer challenges alignment and
causal claims. Update H04 and packet_5 with actual checks and the next measurement,
then commit and sync. No blanket retry or longer-timeout workaround closes it.
