# Prompt 44: finite Python campaigns and sessions

See the [2026-11-15 independent re-audit](prompts_43_44_audit.md) for subsequent
evidence/session corrections and current integrated verification.

Started 2026-11-14 at23:00 Europe/Prague, as requested (22:00 UTC). The delayed
start was honored before repository execution. Branch `main`, upstream
`origin/main`, entry `9e036f6`; no new branch. All activity here is host/fake work;
no serial port, CAN adapter or motor was used.

## Scope and source decisions

The six-file runner family now has concrete roles: `bench_probe.py` owns Console
and shared checked helpers, `_bench_evidence.py` remains private interpretation,
`bench_scenarios.py` defines finite named plans, `bench_motion.py` is the movement/
action entry, `bench_repeat.py` compares write/verify position setup through
ordinary moves, and `bench_session.py` owns one port and all evidence files.
[Runner guidance](../runners.md) and [the scenario catalogue](../bench_scenarios.md)
define working arguments and boundaries.

[The source audit](../audit/PROMPT_44_SOURCES.md) records refreshed RS campaign
code, four sensor HIL seams and FieldCore ownership. All revisions advanced since
prompt43. Adopted finite plans, scenario-owned cleanup and exclusive evidence;
rejected RS COM13/address1 defaults, Modbus `write_rejected`, ESS I/O function
interpretation and recovery/retry hidden in cleanup. The exact EN86/EDS motion
ranges were rechecked by the campaign implementer; no native constant, controlword,
dictionary policy or vendor artifact changed.

## Implemented behavior

Six named plans cover quick/read-only, position, standalone stop, finite velocity,
method-specific homing and one complete typed driver-settings candidate. They use
public CLI operations only. Default `quick` is read-only; `--list` exists only on
`bench_scenarios`. Plans/imports/help need no serial package. Live invocations
require explicit console port/baud, selected profile/node, expected CAN bitrate,
exact library version, artifact prefix and applicable supplied fixture evidence.

Repetition accepts only1..20 position setup pairs, write then verify, stopping at
the first failure. Every move is explicit new intent; no failed write is retried.
There is one planned cleanup, no settings/save/factory-restore loop, no stored-only
policy and no restoration claim. Exact target and aggregate displacement strings
remain distinct beyond2^53. Velocity profile/ramp facts come from externally
prepared native prerequisites; setting console accel/decel would not configure
that workflow and is not inserted into velocity plans.

The session's first active upload is DEC-1 probe1000:00. Identity is the only
additional preflight read for a declared device fixture; other snapshots are
cached, including axis facts when that public cache is available. No ordinary
read-config, settings baseline or homing/velocity parameter upload bypasses Q20.
Caller model/firmware/dialect/scale/reference/I/O/stop/heartbeat/map declarations
are retained as supplied provenance, not measured identity or native permission.
The existing public typed APIs remain the final admission authority.

Each scenario owns at most one planned stop. Broken framing, reboot, disconnect,
transport interruption and failed evidence callbacks prohibit subsequent I/O.
A valid uncertain native result on an intact stream can receive a separate planned
stop; primary and stop evidence remain independent. Cleanup never performs recover,
reconnect, escalation or replay. The session itself invents no stop or restoration.

Console now validates exact motion values, native execution/completion and stop
components, with reserved stop correlation capacity. Successful motion cannot
omit its values; successful stop cannot omit its qualifying component. Best-effort
replies never qualify completion. Campaign service-turn budgets accommodate finite
durations up to the existing60-second host cutoff while remaining bounded.
Firmware gives reserved stop output priority over ordinary receipts. Client
correlation now accepts that proven delivery order while retaining bounded ID
history and rejecting reuse or a lower operation on a newer host request.

All three artifacts (`.json`, `.jsonl`, `.zip`) are exclusively reserved before
opening the port. Summary tails and the first primary failure remain bounded;
raw lines stream separately. ZIP packaging includes only summary, transcript and
SHA256 manifest with fixed metadata. Identical input bytes produce identical ZIP
bytes. Packaging/close failures remain non-passes, preserve the original failure
and invalidate any prematurely passing archive. No unrelated machine inventory
or firmware binary enters the archive.

## Review and verification

Parallel tasks used `gpt-6.1-sol` high for source audit, scenario implementation,
documentation and integration tests; `gpt-6-astra` high independently challenged
cleanup, correlation and durable evidence. Workers owned disjoint files and made
no Git or hardware changes. The lead owns shared client/session and integration.

Focused host suites currently pass: campaign plans, byte-level adversarial cases,
all six real Console/session fake paths, bounded repeat and deterministic packaging.
Predecessor client tests pass after the extensions. Tests cover wrong profile,
missing prerequisites, stale/duplicate completion, boot/disconnect, lost receipts,
lost/late stop, partial write, transport interrupt, failed artifact writes/closes,
serial-open failure, exact numbers, collisions and no replay. Normal and optimized
Python checks and serial-free help/plan checks are included.

Local commands use the existing verifier, not a second campaign-specific build
framework:

```text
python -B test/bench_campaign_plans_test.py
python -B test/bench_campaign_adversarial_test.py
python -B test/bench_session_test.py
python -O -B test/bench_campaign_adversarial_test.py
python scripts/verify.py --mode full --compiler gcc --build-dir .build/p44-full
python scripts/check_promptset.py
git diff --check
```

Results:52 new focused methods pass (12 plan,29 adversarial,11 session integration),
including optimized-Python variants and serial-free CLI subprocesses. Existing
prompt43 contracts remain passing. Full GCC15.1 verification passes142 registered
CTest contracts, separate source/install/export package consumers and strict
Doxygen1.13.2. Retained command/tool/status logs and summary are under
`.build/p44-full/20261114T222439935119Z-29336-mpa2dvlq/`. Python3.12.10 compilation,
planning integrity and whitespace checks pass. Linux sanitizers are unavailable
locally on Windows; the unchanged remote GCC/Clang jobs own that check.

The final implementation checkpoint is committed and synchronized on `main`.
Exact commit/sync and required automatic CI conclusions are checked and reported
in the final handoff, rather than treating an older green run as validation.

## Documentation, maintainability and remaining scope

Updated runners, scenarios, source map, README/index, CHANGELOG, verification,
coverage/backlog/roadmap and PROGRESS. No installed header, native dependency or
firmware source changed; native/package/Doxygen remain integration regressions.
Direct named scenarios and one file-owner context replace no native operation
engine. Shared evidence interpretation stays behind bench_probe; no new CAN driver,
registry, private command sequence or automatic restoration was added.

Physical campaign qualification remains47-48. Firmware boot epoch and SDK identity
are unavailable unless supplied; host IDs do not resolve late wire replies. Model,
firmware, homing-input, source-dialect and native capability gates remain explicit.
The later velocity/position-change scenarios in backlog F12 depend on F04 and their
native owning prompts; this increment does not claim them or surveyed drive support.
Next assigned software work after handoff is prompt45; it is not executed here.
