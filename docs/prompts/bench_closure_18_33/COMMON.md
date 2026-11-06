# Contract for this bench continuation

Apply root AGENTS and [main COMMON](../COMMON.md), including its standing bench
authorization, plus [CONVENTIONS](../CONVENTIONS.md),
[DOCUMENTATION](../DOCUMENTATION.md), [MAINTAINABILITY](../MAINTAINABILITY.md),
[DELEGATION](../DELEGATION.md) and the relevant [MULTIDRIVE](../MULTIDRIVE.md) rows.
These eight packets are assignments within the existing promptset, not permission
to mark all of 18-33 complete or implement unrelated later stages.

## Every packet

1. Read its original numbered prompts in full, its named current implementation
   and tests, then the relevant reports. Inspect branch/upstream, HEAD, dirty and
   staged changes; fetch/reconcile incoming changes safely. Continue
   `promptset/motorcontrol-can`. Never overwrite or commit another session's work.
2. Refresh RS revision/dirty state and inspect the named seam; refresh only named
   I2C siblings. FieldCore is read-only where ownership changes. If the recorded
   revision, dirty state and seam match, cite the earlier row instead of repeating
   the audit. Record adopted/rejected behavior with path and symbol.
3. Check exact device facts against originals in `docs/reference/vendor/`,
   `docs/reference/SOURCES.json` and `docs/reference/lichuan_objects.json`.
   Render cited tables/diagrams when text is ambiguous. Do not edit originals or
   infer model identity from matching generic EDS values. Correct the reviewed
   ledger and regenerate through the real generator when facts actually change.
4. Inspect `.github/workflows/verify.yml`, `scripts/verify.py`,
   `scripts/check_firmware.py`, `test/CMakeLists.txt` and `docs/verification.md`.
   Use registered tests, the existing package consumers and pinned toolchains.
   Run focused checks, then the appropriate quick/full verifier and Doxygen for
   changed public APIs. Firmware changes require the affected real firmware build.
   Tests prove behavior, not merely that another helper returns the same value.
5. Prefer one concrete lifecycle and caller-owned bounded data. No new service
   engine, universal workflow language, raw production write bypass, placeholder
   driver or second receive loop. Keep exact RS intent names and native CAN facts
   distinct. Retained outcomes and uncertainty survive cancellation and recovery.

Useful existing commands (inspect their current help before adding options):

```text
python scripts/check_promptset.py
python scripts/check_lichuan_ledger.py
python scripts/verify.py --mode quick
python scripts/verify.py --mode full
python scripts/check_firmware.py --target s2
python scripts/s2_flash.py --help
python scripts/s2_bench.py --help
python scripts/s2_pdo.py --help
git diff --check
```

Find the configured native build in the verifier's retained summary, then use
`ctest --test-dir <that-build> -R <packet-test-pattern> --output-on-failure`.
Do not invent target names, SDK paths or a fixed build directory. Report missing
tools explicitly. Check exact-commit CI for substantial API/adapter/integration
changes; documentation-only checkpoints do not need a manual CI dispatch.

## Bench execution and failures

The user authorizes needed finite physical testing and repeats to investigate
bugs. Do not ask for permission again. Ask only for a physical action or
observation software cannot supply, and continue independent software meanwhile.
Re-read `docs/hardware_bench.md` and the latest retained report before running.
Current identity is ESP-Prog-2 USB 303A:1002, serial 94A990398B40, historically
COM15/115200; discover and match it anew. S2 CAN RX16/TX17/silent18, nodes1/2 at
1 Mbit/s are recorded facts to verify, not universal library defaults.

One lead holds the port; no subagent, second runner, monitor or background soak
may also control it. Check for an existing session before opening or flashing;
do not terminate another operator's process. Use existing flash/build identity
and ping checks, with fresh status/error/reference evidence. A host restart loses
local provenance; it does not reset the drives or settle their SDO sessions.

Before every write experiment record exact objects/values/order, baseline,
specimen, evidence source, state predicates, finite deadline/admission cap,
observations and failure disposition. Independently review and fake-test the
path. Keep startup/probe/cache passive. No automatic enable, stop, alarm clear,
reset, save, rollback, rearm or replay is hidden in recovery or test cleanup.
An intended restoration is a separate explicit checked action after a known
outcome; an uncertain write ends dependent work and preserves its evidence.

Use public checked services and the same owner. Evidence-only routes remain
application-local and explicitly admitted; experimental numeric acceptance is
not production capability. CAN receive times on S2 are bounded observations,
not exact physical arrival timestamps. Trace copies cannot consume normal RX.
Every capture has finite capacity, deadlines and explicit loss indicators.

Retain raw bytes, build/ELF identity, fixture, operation/step IDs, time bounds,
controller counters, service gaps and terminal effects on both success and
failure. Separate observed facts from hypotheses. Reproduce deliberately with
the same finite case when conditions allow, fix the cause, add a failing native
regression where representable, and rerun that case plus a short baseline.
Longer timeouts, quiet intervals or a later pass do not resolve correlation or
prove a fix. Never promise a definitive electrical cause without its measurement.

## Exit, delegation and records

Each numbered file is a complete assignable packet. Give workers disjoint files
or read-only briefs; the lead owns central state, public contracts, integration,
Git and hardware. Use the available coding model for settled work and an
independent stronger reviewer for side effects, provenance and stale TX. No
parallel writers in an operation/owner state machine.

Write `docs/reports/bench_closure_18_33/packet_N.md` on execution: actual starting
revision/dirty state, exact requirement-to-code/test mapping, source delta,
checks, live records, remaining gates and next measurement. Packet 1 creates a
small execution index there; later packets update their row. Update original
reports/PROGRESS/backlog/coverage only where facts changed, with links instead
of copied narratives. Include Doxygen, user guides and CHANGELOG with behavior.
Keep packet completion, original-prompt completion and hardware qualification
as separate columns. Do not premark execution in this authoring folder.

Commit and sync each checked packet and useful complete checkpoints on the same
branch. Stage only owned paths/hunks, review staged content, push normally and
verify remote inclusion with fresh fetch/ahead-behind. If incoming changes touch
the task, integrate safely and rerun affected checks. Report blockers honestly;
no forced push, stash, reset or borrowed green CI result. If assigned only one
packet, finish its handoff and stop; a range assignment proceeds to ready work.
