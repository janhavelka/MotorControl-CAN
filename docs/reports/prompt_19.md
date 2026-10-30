# Prompt 19: typed identity, state and configuration reads

## Status and owned scope

**Partial.** All 30 assigned raw read rows and their negative contracts are
implemented and host tested. The documented non-consuming probe remains blocked
by Q20: neither the original manuals nor EDS establishes non-consuming upload
effects. `prepareProbe` rejects that candidate without modifying its context or
yielding work. This rejection does not complete the probe requirement. No
hardware qualification, supported installed model or interpreted status dialect
is claimed. Prompt 18 remains independently Blocked on Q17.

Execution identity: version 2, original base `7878f7c`, existing branch
`promptset/motorcontrol-can`, upstream `origin/promptset/motorcontrol-can`.
Entry commit `bdf833e`; entry tree/index clean, fresh fetch ahead/behind 0/0.
This assignment stops at 19. Git delivery and final verification are recorded
below after the integrated checks.

## Implemented contracts

- Installed common read phases/outcomes and concrete Lichuan contexts,
  `prepareIdentity/State/Config`, `nextRead`, `advanceRead` and checked
  `getIdentity/getStateBlock/getConfig`. The [read guide](../reads.md) owns the
  durable lifecycle, row inventory, provenance, age/skew and failure contract.
- Nine identity, fifteen state/history and six communication rows. Error-history
  count gates active subentries; no absent `1018:04` or measured following-error
  accessor. `1009/100A` stay U16, `6061` stays raw and disputed monitor access
  grants no write setter. Identity match and interpreted state remain unknown.
- Matched bounded SDO terminals reuse `readWord`; qualified PDO projection
  reuses actual-map evidence and the decoder. Whole candidates publish atomically.
  Every field owns evidence; time zero is valid. Whole identity/config reads
  require success; completed state blocks survive a later ordinary failure.
- Independent lifecycle invalidation survives deadline precedence, withdrawing
  getters after reboot/bus-off/rebinding even when the outcome is `DEADLINE`.
  Unknown upload effects and full U32 aborts remain retained evidence.
- Pure observation/PDO record types moved into installed `canopen` headers;
  the optional adapter forwards to the same types. A shared bounded
  `samePdoRoute` replaces duplicate comparisons. No second receive owner or
  node/map table, I/O, clock, heap, hidden NMT/write or retry was introduced.
- All readable rows have separate read ownership: 30 in 19, 101 in 38,
  9 in 20, 23 in 21, 33 in 22. Existing write owners and whole-object coverage
  are unchanged. The 30 raw rows have independent implemented/host-tested read
  coverage; all hardware-qualified fields remain false. Generator validation,
  generated numeric declarations and readable coverage reflect these facts.
- Core-only compiled reads preview and isolated installed/exported consumer
  exercise the production API without Python, ledger, vendor files, catalogue
  strings or optional stack. Exact package inventories include the read guide.

## Fresh source delta

All sibling inspections were read-only; no unchanged sibling suite was rerun.

| Checkout | Current revision / dirty state | Actual implementation, caller/test and CAN decision |
| --- | --- | --- |
| MotorControl-RS | `f812d7c1d364a03be86f02ae8f648dc8fca23d42`, clean | Refreshed after `4234c6b`; intervening homing/owner/console changes do not change the inspected Reads/StateCache/reads-test bytes. `include/MotorControlRS/profiles/ess_rs/Reads.h:225`, `src/profiles/ess_rs/Reads.cpp:58` and `:170`, `test/reads_test.cpp:134` and `:410` establish concrete prepare/next/advance/getters and partial state blocks. `examples/probe_cli/ProbeApp.cpp:2776` checks live generations before projection; ProbeConsole callers `:2846` and `examples/probe_cli/StateCache.h:26` preserve history versus current values. CAN getters explicitly require current epochs and per-field age, rather than relying solely on an outer caller. RS Discovery's single reviewed non-changing register does not establish Lichuan CAN read effects. |
| ADS1115 | `235af3e8c722661a89a44766a4ccd903a654f6fe`, clean, unchanged | Public validity/generation contract `include/ADS1115/ADS1115.h:78`; atomic publication `src/ADS1115.cpp:920`, token-matched result `:1079`; `examples/02_owner_safe_poll/main.cpp:246`, `test/test_basic.cpp:5305`. CAN retains explicit caller-owned evidence and unchanged failed outputs, without importing sensor polling/recovery policy. |
| INA228 | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b`, clean, unchanged | Freshness `include/INA228/INA228.h:561`, destructive diagnostic distinctions `:894`, implementation `src/INA228.cpp:2886`, tests `test/test_basic.cpp:2034` and `:2387`, bringup caller `examples/01_basic_bringup_cli/main.cpp:815`. Fresh transport does not prove physical freshness or a harmless read; CAN preserves unqualified effects. |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0`, clean, unchanged | `src/devices/rs485_motor/Rs485MotorModule.cpp:33`, `:467`, `:640` project raw reads only with binding/current evidence and retain interrupted terminals; `test/host/rodtester/motor_module.cpp:734`. No FieldCore integration or dependency added. |

The source reviewer independently reopened all five original manuals and raw
EDS sections for owned blocks. Identity: EDS lines 158–224, 234–251, 276–313,
1229–1273; state: 1823–1893 and 2021–2028, plus EN86/ZH86 monitoring tables.
Communication/history source lines remain in each canonical row. Rendered PDF
pages were used for ambiguous extraction. Original source bytes are unchanged.
The old `2000/2001` ZH86 citation said PDF page 18; the actual U16 RO model/version
table is PDF **page 17**. Both row citations were corrected and regenerated;
the native type/access claims did not change. EN86 page 17 already agreed.
Manufacturer conflicts and unbound firmware scope remain visible.

## Independent review and tests

Delegated source/schema review, live sibling review/offline example, and
independent fake-owner/provenance tests ran in parallel with disjoint ownership.
Source and boundary reviewers used `gpt-6-astra` high; sibling/example work used
`gpt-6.1-sol` high, per DELEGATION. The lead owned lifecycle, canonical ledger,
integration and Git. Review findings were checked against implementation and
fixed before verification: source-page citation, strict terminal evidence,
PDO communication/qualification timing, and lifecycle withdrawal at the deadline.

The registered `reads` suite passes **3,091 assertions**, including actual
fake-owner exchanges for every assigned row, independent expected widths/raw
values, every wrong object/width case, before-completion/wrong-context getters,
zero/future/extreme time, age/skew, history bounds, partial failure, abort bits,
reboot/configuration change, immutable failure output, actual PDO map/epoch and
qualification boundaries. Probe yields no work; ordinary reads produce uploads
only. No write, reset, save, enable, I/O reassignment or hidden NMT is produced.

## Verification and delivery

| Command | Integrated result / retained evidence |
| --- | --- |
| `python scripts/verify.py --mode quick --build-dir .build/p19-quick` | PASS: 32 registered contracts, structure and version/object freshness. `.build/p19-quick/20261030T110208083495Z-81932-4zcegicx/summary.json`. |
| `python scripts/verify.py --mode full --build-dir .build/p19-full` | PASS: 33 registered contracts, isolated exported/relocated-installed C++11/C++17/no-RTTI consumers, 18 independent public headers, optional catalogue omission and strict Doxygen/API extraction. `.build/p19-full/20261030T110259795859Z-52372-db5u8kg8/summary.json`. |
| Registered ledger / generator / repository contracts | PASS: 64 / 37 / 15 tests respectively, in the verifier's retained contract log. |
| `python scripts/check_promptset.py --check-git-ref HEAD` | PASS: 99 indexes, 202 entries, 53 manual items, 24 conflicts; original and committed provenance intact. INDEX and post-commit HEAD checks accompany delivery. |
| `git diff --check` | PASS. |

The first Release quick build found an unbounded fixture-width payload loop
under GCC `-Wstringop-overflow`. The fake-node loop now has an explicit four-byte
bound, matching the independently asserted native widths; the corrected Release
quick/full builds and all read assertions pass. This was a fixture correction,
not a weakened warning or production policy change. The failed log is retained
at `.build/p19-quick/20261030T110149999747Z-88988-kk2mqaim/11_build.log`.

Measured Windows x86-64 value sizes: `ReadContext=1944`,
`IdentityObservation=824`, `StateObservation=520`, `ConfigObservation=568` bytes.
These are caller storage costs, not embedded ABI or timing guarantees.

Local tools:
Python 3.12.10, CMake/CTest 4.0.1, Ninja 1.12.1, GCC 15.1.0, Doxygen 1.13.2.
Linux GCC/Clang sanitizers are required CI gates; they are unavailable locally
on this Windows toolchain. No physical CAN traffic or tests were performed.
The final pushed commit's automatic Windows quick and Linux GCC/Clang
ASan/UBSan workflow must be inspected before handoff; its immutable commit/run
receipt is reported in the final handoff. Adding or pushing code is not CI
execution evidence. There is no manual dispatch or workflow-policy change.

## Documentation and remaining evidence

Public fields/functions have Doxygen contracts. README, CHANGELOG,
[read guide](../reads.md), schema/generated coverage, source map, API/CLI map,
getting-started guide, index, backlog, roadmap and PROGRESS distinguish raw
read success from model, status, scale, effect and hardware qualification.
The actual preview and isolated consumer replace uncompiled lifecycle snippets.
No CLI implementation is owed here; no board or firmware documentation was
invented.

To close 19, obtain source-backed non-consuming semantics for the minimal
selected probe (or separately authorize a fixture/firmware-specific evidence
experiment), review Q20 in the canonical ledger, implement the permitted probe
path and its successful fake-owner/getter contracts, then rerun affected gates.
Do not grant that policy from RO access, an observed successful upload or another
manufacturer's identity semantics. The 30 completed raw read rows remain usable
under their documented ordinary-admission and unknown-effect policy.
