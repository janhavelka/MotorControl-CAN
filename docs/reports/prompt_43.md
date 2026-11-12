# Prompt 43: Python console and evidence primitives

Date: 2026-11-12. Execution: `main`, upstream `origin/main`; no new branch.
Entry revision was `3249ee8`; concurrent completed night-soak reporting advanced
the base to `d8fa2f3` before integration. Its evidence and unrelated editor files
were preserved. This prompt used no serial port, CAN hardware or motor activity.

## Delivered scope

`scripts/bench_probe.py` owns bounded console framing, cached handshake, request
and admitted-operation correlation, immutable deadlines and one explicit typed
probe/read. It reuses `console_jsonl.py` for the actual CAN schema. The shared
validator now rejects duplicate keys under optimized Python and finite-overflow
JSON numbers. Neither script is a CAN driver.

`scripts/_bench_evidence.py` owns pure bounded copies and summaries, preserving
decimal strings, native status/outcome/abort and uncertainty. Admission, native
terminal, serial handoff and stream integrity remain separate. Known native
results survive later damage; no timeout becomes motor-stop evidence.

Offline `--help`, `--plan-only` and `--self-test` use the standard library only.
Live use requires explicit port, serial baud, profile, existing CANopen node,
expected CAN bitrate, exact library version and new artifact paths. The default
scenario is one probe. Startup/selection performs only cached console work and
host binding selection, followed by the explicitly requested typed upload.
There is no reconnect, retry, inferred address, automatic release or cleanup
device command. [The runner guide](../runners.md) defines arguments, exit codes,
capacities and evidence interpretation.

## Audit and independent review

[Source audit](../audit/PROMPT_43_SOURCES.md) records current RS, the four
sensor sibling seams and FieldCore ownership seam. RS responsibilities and exact
CAN producer output determined the client; sensor verdict/threshold/retry policy,
RTU framing and campaign defaults were rejected. Existing manufacturer read and
uncertainty gates are unchanged; no new native object claim was introduced.

Parallel delegated work used `gpt-6.1-sol` at high effort for source audit,
pure evidence/tests and CLI/resource tests, and `gpt-6-astra` at high effort for
independent correlation review. One lead owned client, registration and Git.
The reviewer supplied hostile vectors independently from the client.

Confirmed review gaps were fixed before closure: typed admission/completion
matching; immutable retained-result lookup; pending-result regression; malformed
types and nested JSON poisoning; stale buffered records before new handoff;
split replies while other work remains pending; exact expected read fields;
contradictory success/abort/time evidence; terminal retention after trailing
damage; and permanent transcript exhaustion. All have focused regressions.

## Verification

Five Python contracts are explicitly registered in `test/CMakeLists.txt`, checked
by the repository inventory and exercised by the existing automatic CI workflow.
The native integration test consumes actual `native_probe --console-fake` output,
not copied success JSON. Its probe/identity succeed; its identity-only fake's
state reads fail with retained uncertainty, and config retains its Q20 refusal.
Separate independent vectors test successful status/monitor/feedback decoding.

Focused commands run on Python 3.12.10:

```text
python -B test/bench_console_test.py
python -B test/bench_console_adversarial_test.py
python -O -B test/bench_console_adversarial_test.py
python -B test/bench_probe_cli_test.py
python -B test/bench_evidence_test.py
python -B test/bench_console_native_test.py .build/p43-native/test/native_probe_example/native_probe.exe
python -S scripts/bench_probe.py --help
python -S scripts/bench_probe.py --plan-only
python -S scripts/bench_probe.py --self-test
python scripts/verify.py --mode full --compiler gcc --build-dir .build/p43-full-final
python scripts/check_promptset.py
git diff --check
```

The native test executable was built with CMake/Ninja and GCC 15.1.0. Python
compilation and 78 focused test methods pass (23 client/log/live-wrapper, 28
independent hostile-stream, 9 isolated CLI, 14 pure evidence and 4 real-producer
methods, with additional subcases). Optimized-Python hostile cases also pass.
The repository checker regression has 19 passing methods, including rejection
of a substituted/missing native producer argument.

Full verification passes: 134 registered CTests, the separately executed package
contract (source/install/export consumers, C++11/C++17/no-RTTI) and strict
Doxygen 1.13.2. Retained evidence is
`.build/p43-full-final/20261112T100621372522Z-12448-6gb3l1b1/`; its JSON summary
and command/version/status logs identify all artifacts. Linux sanitizers are
unavailable on this Windows host and remain covered by the unchanged remote
GCC/Clang workflow. Firmware sources did not change.

An earlier full run in `.build/p43-full/20261112T100115115015Z-46828-yevatr9a/`
correctly failed both planning tests because PROGRESS linked this report before
it existed. The report was added, both checks passed, and the fresh complete run
above passed. No check was weakened or skipped. Promptset integrity, staged
original-byte provenance (140 artifacts), Python compilation and diff checks pass.

The implementation checkpoint is committed and synchronized on `main` using the
existing automatic [verification workflow](../../.github/workflows/verify.yml).
Exact final commit, synchronization and remote job conclusions are verified and
reported in the final handoff; an older green run is not used for that claim.

## Remaining boundaries

Schema1 emits no firmware boot/session ID or firmware source commit. These stay
unknown; randomized host IDs and local epochs prove neither wire freshness nor
device retirement. Already-buffered stale records are rejected before new work;
an old same-ID reply arriving after an empty read is not distinguishable without
firmware evidence. No automatic reconnect or replay tries to resolve this limit.

Probe's DEC-1 read effect remains unknown. Other ordinary read qualification and
model/firmware gates remain owned by their existing stages; config is Q20-gated.
The live invocation is implemented against P41 routes and tested with an injected
serial module, not run physically here. `live-observed` is an evidence-origin
label, never hardware qualification. No existing bench evidence is relabeled.

Prompt44 owns campaigns, repetition, session orchestration and scenario catalogue.
This stage adds no motion runner, direct-CAN client or additional supported drive.

Documentation: runner guide, source map, README/index, verification table,
CHANGELOG, coverage/backlog/roadmap and PROGRESS are updated. Installed public
headers, Doxygen declarations, package dependencies and firmware sources are
unchanged; strict documentation/package gates remain applicable regressions.
