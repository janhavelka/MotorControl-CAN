# Retained prompt45 counterexamples

Host-only synthetic failures, not CAN captures or device qualification. Base
revision `818d36186a558e650e3b39d92dbec731a9d6d487`; tests were added before fixes.
Line numbers in original logs describe that reproduction, not later source edits.

Windows output bytes are preserved exactly in `originals.zip`; `originals.json`
records each original SHA256 and the archive hash. Neighboring log/JSON files are
readable LF copies with final blank lines removed, so repository whitespace checks
do not mistake preserved CRLF for trailing spaces. No original source or evidence
bytes were discarded; the archive is the authority for byte-for-byte comparison.

- `motion_counterexample.json` records the real fake-owner trace, tool and cause.
  `best_effort_false_unsent_baseline.log` is the failing CTest observation;
  `focused_after_fix.log` retains the focused passing rerun.
- `runner_baseline.json` records commands, baseline source hashes and environment.
  `callback-baseline-failure.log` and `callback-baseline-trace.json` retain the
  consumed-but-unpublished evidence failure and unintended cleanup on that broken
  stream. The trace's `intact` label is the **old incorrect report**, not a new claim.
- `fixture-baseline-failure.log` records malformed/deep fixture input returning
  the execution-failure code instead of pre-I/O input refusal.
- `service-interruption-baseline.json` and its log record the independent review's
  post-read/startup interruption counterexample, before the whole-interval fence.
- `write-accounting-baseline.json` and its log record interruption after short/full
  write return but before count validation. The current regression uses a one-shot
  Python line trace, restored afterward, to hit that real boundary deterministically.
- `resources.json` contains selected values from the existing stack measurement,
  tool identities and the full report hash. Unmeasured SDK/libc/indirect-call
  portions remain explicit in the [integration report](../../../reports/prompt_45.md).

The current registered tests are the maintained reproduction. From the repo root:

```text
python -B test/bench_campaign_adversarial_test.py
python -B test/bench_campaign_plans_test.py
python -B test/bench_session_test.py
ctest --test-dir <native-build> -R "^(move_absolute|axis_session|can_bus_owner|can_bus_health|can_bus_recovery)$" --output-on-failure
```

Do not restore old code into a live checkout to reproduce a failure. These files
retain the original failure; the current tests prove the corrected behavior.
