# Prompt 17: observed PDO maps and decoding

## Scope and decisions

Implements checked pure mapping/decoding, generated reviewed layout facts,
actual-map evidence acquisition and separately qualified passive samples in
the existing application owner. No configuration writes, remapping, automatic
NMT, active SYNC, controller I/O, drive action or motion completion is added.
The [service guide](../canopen_services.md#observed-pdo-maps-and-passive-samples)
owns the complete public contract; the stack decision records the narrow
passive PDO correction without a duplicate active native receiver.

Portable `canopen/Pdo.h/.cpp` validates source-backed direction, widths, layout
and profile capacities; decoding publishes complete owned candidates. Ten
explicit `review.pdo_observation` ledger records generate profile `Pdo.h`
through the existing generator/freshness gate. Types/access/effects/gates and
all original claims remain unchanged. Modes 6060/6061 and velocity 60FF remain
RAW, not resolved signed types. Default WO RPDO targets gain no SDO reads or
write permission. Count-zero PDO3/4 templates remain disabled, not active slots.

`pdoMapFromReadback` consumes complete matched successful uploads of actual
communication/mapping records, or the owner accepts an explicitly reviewed
actual configuration with provenance. SDO completion now retains admission
time and node epoch; earliest read admission and latest completion are checked
independently. Mixed old epochs or pre-boundary constituent reads cannot hide
behind a recent final result. Coherent acquisition against external writers
remains an explicit prerequisite; no read-effect qualification is invented.

The existing two node/channel slots gain eight permanently bound PDO wire
route slots, not a parallel node registry. Routes validate in the single
receiver, then publish one complete latest sample/callback and a raw diagnostic
copy into the existing four-record FIFO. Overflow cannot suppress publication
or invalidation. Trace consumers cannot steal frames. Route-local map changes
preserve unrelated stable observations; reset/controller/binding/configuration
changes revoke actual dependent PDO consumers. Nonreset NMT withdrawals preserve
map contents but revoke sample qualification.

Map confirmation and sample qualification are independent. Every route starts
unqualified; explicit scoped producer/all-ingress retirement or genuinely
distinct verified non-reused-route evidence is required. Reused routes cannot
reopen through host generation, readback, quiet time, later capture or distinct-
route claims. Unknown exact NMT permits a first configured qualified TPDO without
state inference. Qualified captures must follow the actual boundary strictly;
malformed/old/unknown-capture bytes remain diagnostic. Raw words and actual/
commanded/estimated roles carry no physical units/position validity/completion.

## Refreshed source evidence

All sibling inspection was read-only and proportional; unchanged suites were
not rerun.

| Source | Current revision / state | Relevant implementation, caller/test and delta |
| --- | --- | --- |
| MotorControl-RS | `4234c6b40db2d3d9788f8a6accfbfe7713d7adc6`, clean | Advanced from prompt-16 dirty e44110f: owner/console interruption integration, Xtensa fix and evidence committed. `examples/probe_cli/StateCache.h:24–38,54–75` retains history separately from current generation/freshness; `ProbeApp.cpp:322–353,380–413` invalidates derived values. `src/profiles/ess_rs/Codec.cpp:245–267`, `Reads.cpp:298–339` and literal codec/probe/move tests verify native widening/candidate publication. |
| ADS1115 | `235af3e8c722661a89a44766a4ccd903a654f6fe`, clean unchanged | `src/ADS1115.cpp:920–955`, owner-safe-poll caller and `test/test_basic.cpp:5312–5344` attach signed sample/configuration generation only after validation; no sensor lifecycle/retry is imported. |
| INA228 | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b`, clean unchanged | `src/INA228.cpp:1255–1329,3317–3334`, bringup owner consumer and basic boundary/retained-sample tests preserve signed20/40-bit native values and operation/configuration provenance. |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0`, clean unchanged | `Rs485MotorModule.cpp:50–75,640–679`, `Rs485Task.cpp:1131–1147`, host motor-module test:733–758 separate binding/reference validity, historical read evidence and motion eligibility. No product changes/dependencies. |

Original EDS SHA-256 remains
`dd3c96e7282c012a522dddb5bbdbbf561c1f5c23716b31f12472f45829d4f52c`.
RPDO mapping records are lines 587–787; TPDO records 1028–1228. All offer
four U32 slots and a U8 active count 0..4. Source fixture words match the prompt;
sets 3/4 have count zero and absent slot defaults. Communication objects supply
COB-ID/type/timing metadata, not live configuration or watchdog behavior.

Original EN86 PDF pages 15–16 (printed 13–14), including rendered page 16,
confirm counts/slots. Pages 11,13,16–17 retain mode U8/I8 and 60FF U32/I32
conflicts; ZH86 page 17 corroborates I32 feedback. Raw 6064/606C do not resolve
physical scale, and 60FD has no complete terminal mapping. No source conflict
was silently resolved. N5 v3.3.0/FIR-v2213 pages 127,207,244 document eight
independent sets including 1607/1A07; that is a capacity contrast, not installed
N5 support. Original hash/provenance manifests and exclusions remain intact.

Pinned CANopenNode `9b8beed8367241e96ac03f916cd5a500bcb2cf23`,
`301/CO_PDO.c` SHA-256
`4791d3acaa044fbe4c05b3b9cd4cbe76c07d9534b04adda3f3df12fdc71a2252`:
separate RPDO/TPDO attributes at 89–131; receive 463–500 accepts longer DLC and
coalesces; init 620–731 requires local OD/EM; process 742–754 requires known
Operational and 799–879 publishes fields sequentially. The checked passive
projection avoids false NMT state and partial observation writes. Native SDO
and NMT producers remain selected; no native PDO receiver is compiled with it.

Actual pinned experiment, with real RPDO init/process, OD interface and fake
port: `powershell -NoProfile -ExecutionPolicy Bypass -File .build/p17-parity/run_parity.ps1`.
Eight assertions pass: known 7/8-byte fields, signed raw feedback, longer-DLC
publication, short-DLC suppression, mailbox coalescing, non-Operational discard,
registered routing and zero TX. Configuration is C99/C++11 GCC 15.1.0,
RPDO+OD_IO only; SYNC/timers/dynamic OD disabled. `SOURCES.json`, source,
commands and `results.txt` remain in that ignored evidence directory. A pinned
CO_SYNC.h include dependency was fetched there only (SHA-256
`3e9ae95c5b548a645f691829913e2bd118bf8d1857a6e2b8fbb9288eccc954a6`);
no SYNC code ran. This is host evidence, not a device conformance result.

## Delegation, review and verification

gpt-6.1-sol/high independently inspected primary/sibling sources, implemented
the disjoint codec vectors and single-writer ledger/generator unit.
gpt-6-astra/high challenged map/sample provenance and wrote independent owner
tests. The lead owns production routing/storage, shared contracts/build and Git.
Worker results were inspected against actual source/diff and integrated tests.

Confirmed review gaps were fixed: controller failure cannot be bypassed by
supplying new qualification; replacing an already qualified proof rejects until
explicit invalidation notifies consumers; two wire routes cannot certify the
same physical PDO set concurrently; SDO epoch/earliest admission prevents stale
readback constituent promotion. Every correction has an observable regression.

Focused strict C++11 codec checks pass 630 assertions; owner/evidence checks
pass 490, including five actual bounded SDO uploads and an actual final-abort
acquisition failure without retry. Ledger tests: 59 and generator tests: 32 pass.
Normal source and installed/exported consumers use generated raw PDO facts,
without Python, references, catalogue strings or stack dependencies.

- `python scripts/verify.py --mode quick --compiler gcc --build-dir .build/p17-quick`:
  passed 30/30 contracts after the checkout correction, run
  `20261029T213813000645Z-85528-u_po9ulh`.
- `python scripts/verify.py --mode full --compiler gcc --build-dir .build/p17-full`:
  passed 31/31 non-package contracts, separate package/source/export/relocated
  consumers, 15 independent public headers, C++11/C++17/no-RTTI and strict
  Doxygen 1.13.2, run `20261029T212625344287Z-74748-wgb9k_0z`.
- Direct `doxygen Doxyfile`, generator `--check`, repository/planning integrity
  and `git diff --check` pass. INDEX and committed HEAD original-byte provenance
  are checked around delivery: 137 original artifacts, 99 indexes, 202 explicitly
  typed entries, 53 manual-only items and 24 retained conflicts.

Current Windows x86-64 storage: owner 10,400 bytes; each nested PdoRecord 816,
PdoObservation 408, PdoMapEvidence 200 and SdoCompletion 216. These sizes overlap,
are not additive and do not qualify embedded RAM/stack, timing or service gaps.
One service call adds only bounded route dispatch/qualification scans; callbacks
must remain bounded. Eight permanent route reservations can exhaust; failure
is explicit rather than recycling wire identity.

Tools: Python 3.12.10, CMake/CTest 4.0.1, Ninja 1.12.1, GCC 15.1.0 and Doxygen 1.13.2.
Local Clang/Linux sanitizers and clang-format are unavailable. Existing automatic
Windows quick plus Linux GCC/Clang ASan/UBSan full jobs verify the exact final
commit; their final status/run identity is checked and reported at handoff.
No unchanged sibling suite or physical CAN test is run. The editable ledger's
original LF treatment was restored after a Windows writer introduced CRLF;
all immutable vendor/evidence bytes and existing attribute rules are preserved.

The first integrated push `b8b9f472c43fb67267df0c2abd8b5c4155d7b3ca`
exposed a Windows checkout defect in [CI run 40958766954](https://github.com/janhavelka/MotorControl-CAN/actions/runs/40958766954).
The retained Windows `09_objects.log` reports stale generated profile `Pdo.h`:
its missing LF attribute allowed `core.autocrlf=true` to alter generated bytes.
A new Git checkout regression reproduced the exact failure before the fix.
The header now has the same explicit LF rule as the other generated headers;
the regression verifies every generated product after an actual autocrlf
checkout and reruns exact freshness. No check or CI trigger was weakened.
The original failed log archive remains at `.build/p17-windows-failed.zip`.
The corrected final commit's automatic CI is checked separately at handoff.

Branch `promptset/motorcontrol-can`, upstream `origin/promptset/motorcontrol-can`,
entry/base `6899c4f6780caf24130248901b6a39861446c22a`; entry worktree/index clean,
fresh upstream comparison 0/0. Complete changes are committed/synchronized on
that branch; exact final commit/CI status is verified and reported at handoff.
No release tag, publication, sibling edit or hardware activity.

Reused: existing service storage/receiver, generated-ledger pipeline, callbacks,
Frame/Status/native SDO and all package/check gates. New code is one pure mapping
boundary and one bounded passive route projection. There is no plugin engine,
second receiver, automatic discovery, generic health state or remapping stub.
Docs/Doxygen/user guide, source map, coverage, backlog, roadmap and PROGRESS
accompany the code. Prompt 18 owns explicit mutation sequences; real mapping,
firmware identity, physical scale and producer/queue retirement qualification
remain unmeasured. Those limits do not become passing device capabilities.
