# Independent audit of prompts 43 and 44, 2026-11-15

Entry: `0a11ea6d205c03dbb27d5b7ff29b7a0c00be4724` on `main`, tracking
`origin/main`, with a clean tree and no incoming commits after fetch. Both
original prompts were read in full. Three parallel reviewers examined the
actual client/evidence, campaign/session and source/coverage seams. The lead
checked their reproductions and diffs and owns production changes and Git.
The [source audit](../audit/PROMPTS_43_44_REAUDIT_SOURCES.md) records refreshed
RS/sensor/FieldCore revisions and actual adopted/rejected behavior.

## Confirmed findings and corrections

| Entry defect | Correction and independently exercised boundary |
| --- | --- |
| A checked `result pending` lost its admitted operation ID in the pure summary. | Preserve its admission proof and original IDs without inventing a terminal or uncertainty flag. Test both a locally tracked operation and an explicitly queried firmware-only result. |
| Requested release refusals were ignored by the live invocation and `wait(..., release=True)`. | Surface the refusal while retaining the exact primary terminal, abort and uncertainty; issue no second release or device action. |
| Fixed 8192-turn limits terminated valid long read/startup waits near 8 seconds. | Share a deadline-derived finite allowance for startup, preflight and every request, bounded by 65536. Exercise delayed replies, 10/60-second startup windows and frozen-clock termination. |
| A completed retained result could exhaust its old byte/turn quota while an independent long read waited. | End per-command charging only after bounded trailing-byte drainage. Retained counters/terminal stay unchanged, cached waits do no I/O, and incomplete trailers remain bounded even with a frozen clock. Global input/slot limits remain active. |
| A slow intent callback or final empty preflight read could hand off an already expired request. | Recheck the candidate cutoff immediately before publishing handoff; expire without a write or replay. |
| Ctrl-C could leave an empty live summary, and an idle-wait interruption could lose its unknown outcome. | Retain the known terminal or interrupted admitted/handoff evidence, publish a failure summary and exit nonzero without hidden cleanup. |
| Port/log close failures occurred after success publication; one failed close could leak the other resource. | Attempt both ownership closures before publishing success, retain separate close errors and preserve native evidence and any earlier error. |
| Summary finalization could hide a primary timeout behind a secondary file error and print completion before closing the file. | Finalize before publishing completion; report bounded primary classification/reason and separate artifact failure on stderr with execution exit 1, without reopening/retry or rewriting native evidence. Test success and timeout crossed with write/flush/close faults. |
| Evidence writes ignored short counts and did not retain write/flush failure. | Require complete flushed records, latch failure permanently and refuse later writes. Console callback failures forbid further handoff or cleanup. Counters include confirmed limit notices. |
| A failed summary close/reopen could bypass invalidation of a passing session ZIP. | Invalidate the independently writable archive before attempting summary repair; preserve the original exception and transcript. |

No new command, device permission, retry, recovery, motion engine or native
sequence was added. Firmware/model/dialect qualification stays explicit and
unknown where not evidenced. Scenario-owned cleanup, finite repetition and
primary-failure retention otherwise passed independent review. Current RS
group/single-setting additions and sensor retry policies remain outside scope.

The client and session reviewers used `gpt-6-astra` at high effort; source and
coverage review used `gpt-6.1-sol` at high effort. Workers edited disjoint tests
and the source audit. One lead owns the existing six-file responsibilities;
the shared turn-bound and checked evidence-write helpers replace duplicate
policies without introducing an execution framework.

## Verification and scope

All eight registered runner suites pass in the focused combined tree, including
the real native console producer, exact integers, offline/help isolation and fake
campaign/session failures. The 27 client, 36 client-adversarial and 33 campaign
adversarial tests also pass with optimized Python. All six runner modules compile; 16 documented offline
commands pass without serial dependencies. New regressions were observed failing
against the entry implementation before their corrections.

```text
ctest --test-dir .build/audit4344-coverage --output-on-failure -R '^bench_(console_native|console_contract|console_adversarial|probe_cli|evidence|campaign_plans|campaign_adversarial|session)$'
python -O -B test/bench_console_test.py
python -O -B test/bench_console_adversarial_test.py
python -O -B test/bench_campaign_adversarial_test.py
python scripts/verify.py --mode full --compiler gcc --build-dir .build/audit4344-full-final
python scripts/check_promptset.py
git diff --check
```

The GCC 15.1 / Python 3.12.10 full verifier passes all 142 registered contracts,
independent headers, source/export and relocated installed consumers,
C++11/C++17/no-RTTI, isolated native applications and strict Doxygen 1.13.2.
Version/catalogue freshness and actual test registration pass. Command, tool,
status and artifact evidence is retained in
`.build/audit4344-full-final/20261115T000348054869Z-22228-3xea_qar/`;
the contracts completed in 95.11 seconds. All eight affected runner suites were
also rerun on the final Python tree after the retention correction (8.46 seconds),
with the evidence above. No C++/firmware/package input changed during that final
Python correction. Earlier candidate logs remain under `.build/audit4344-full/`.

The final handoff records checkpoint/synchronization and required automatic CI
for that exact commit. Linux GCC/Clang sanitizer gates are unavailable locally
on Windows and run remotely; no previous green commit is substituted.

No serial port, controller, flashing or hardware was used for this audit.
C++ and firmware sources are unchanged; their builds remain regression gates
in the existing automatic CI, rather than new firmware qualification.
Physical campaign qualification remains assigned to prompts 47-48. Firmware
boot/session identity is still unavailable in the schema; local IDs and a fresh
client cannot establish remote queue retirement. No plan/fake result is promoted
to physical qualification. Next implementation work remains prompt 45.
