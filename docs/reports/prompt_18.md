# Prompt 18: PDO configuration evidence gate

## Status and delivered scope

**Blocked: Lichuan applicability of the standard remap is unconfirmed.** The source
audit, prerequisite review and precise evidence assessment are complete; the
typed remapping/settings implementation and its acceptance tests are not.
No remap context, always-rejecting API, hypothetical device sequence or extra
service engine was added. Passing existing tests does not complete Prompt 18.

Executed 2026-10-30 from `c5253b82c710f54715860d2628b32d549e60ba63` on
`promptset/motorcontrol-can`, upstream `origin/promptset/motorcontrol-can`.
Initial tree/index were clean; fresh fetch showed 0 ahead / 0 behind. This
continues promptset v2. No sibling mutation, real CAN traffic or hardware work.

Read the pasted Prompt 18 and repository version in full; their instructions
match. Its requirement to confirm edit/state/disable/readback behavior before
exposing remapping cannot be met from the evidence below. This follows
[COMMON's completion rule](../prompts/COMMON.md#verify-finish-and-sync):
"No guessed values, success stubs or silent scope reductions qualify."

The canonical Q17 evidence request now explicitly includes permitted states,
disable/edit/enable order, activation/readback, timer meanings and persistence.
The existing generator updated only its coverage documentation. Object facts,
permissions, generated C++ headers, original artifacts and source pins did not
change. The service guide, API/CLI coverage, backlog, roadmap, progress, README
and changelog now identify this concrete limitation.

## Primary evidence and its limits

All page numbers are one-based PDF pages. Original paths and provenance remain
in [SOURCES.json](../reference/SOURCES.json). No downloaded file was replaced.

| Fact | Reopened original evidence | Disposition |
| --- | --- | --- |
| Four receive and four transmit sets | EDS DeviceInfo; mapping records `1600..1603`, `1A00..1A03`; EN86/ZH86 pp15-16 | Documented Lichuan metadata, not a common-library maximum or observed firmware configuration. |
| Counts U8 RW 0..4; four U32 RW slots | EDS lines 587-787 and 1028-1228; rendered EN86/ZH86 mapping tables | Documented representation/access; variable/dynamic capability and device-specific activation remain unresolved. The conditional standard order is documented by CiA, as corrected below. |
| Communication COB-ID U32, transmission U8, inhibit/event U16 | EDS lines 347-586 and 788-1027, records `1400+k/1800+k`, k=0..3 | Documented representation/access only. No complete flag mask, legal edit state or readback contract. |
| Type default 255, inhibit 0, receive event 1000, transmit event 100 | Same EDS sections | Raw defaults only. No accepted type set, timing units/ranges/zero behavior or motor watchdog guarantee. |
| PDO targets and direction | Original default words and WO `6040/6060`; current generated passive layouts | Default-layout evidence does not authorize arbitrary remapping or SDO uploads of WO targets. |
| SYNC object | EDS `1005`, line 225: RW U32, default `0x80`; `1006/1007/1019` absent | Object existence and general synchronization claims do not establish required frame/mode/effect behavior. No SYNC feature is justified here. |
| Persistence | EN86 p15 save example uses `2300=2` and mentions homing/ramp/mode settings | PDO inclusion, automatic persistence, completion and activation remain unknown. No implicit save. |
| Fresh samples after changes | No applicable producer/queue retirement procedure found | Readback, time delay and host epochs cannot qualify indistinguishable old frames. Q17 stays open. |

EN86, ZH86, ZH57 and operating manuals were searched/read for communication,
mapping and SYNC procedures; the mapping tables supply no missing mutation
procedure. Inspection renders remain under `.build/p18-evidence/`. Reproduced
original SHA-256 values:

- EDS: `dd3c96e7282c012a522dddb5bbdbbf561c1f5c23716b31f12472f45829d4f52c`.
- EN86: `401df6c49c54fe56fed7c606ca45718f352235746e2faaeb65be778eef6c4b9f`.
- ZH86: `3969c914aa291662e8c77941bcfbb0559769a96b75306266370890d03e824d13`.

The current [manufacturer download index](https://www.xlichuan.com/download/23.html)
still lists the same named EDS and four 57C/86C manuals. The
[manufacturer CL86-C page](https://en.xlichuan.com/product-xq/143.html) adds broad
CiA301/402 and synchronization claims without a usable procedure. This was an
index/content recheck, not a byte-for-byte redownload comparison. No newer
applicable procedure was found; this does not prove that none exists elsewhere.

Pinned CANopenNode `9b8beed8367241e96ac03f916cd5a500bcb2cf23` does document
the standard sequence in `301/CO_PDO.h:89-103`: disable, zero count, entries,
count, enable. `CO_PDO.c:247-252` checks its mapping edit state;
`:999-1035` implements its type/timing behavior. A type write sets the stack's
`sendRequest` flag; inhibit edits require disabled PDO, with 100-us units, and
event fields use milliseconds in that implementation. These are source-confirmed
stack mechanics, not source-confirmed Lichuan firmware effects. The original
review overstated the uncertainty of the order itself: CiA independently
documents it. The [fresh audit below](#fresh-independent-audit) corrects that
claim while preserving the distinction between a standard procedure and its
Lichuan applicability. No physical claim was measured.

## Original acceptance criteria

| Item | Available prerequisite | Unfulfilled Prompt 18 work |
| --- | --- | --- |
| 1. Typed request/atomic admission | `validatePdoMap`, `validatePdoRoute`, owner identity/collision checks | Model/firmware mutation permissions, allowed states and remappable-target policy; then concrete typed request preparation. |
| 2. Documented sequence | Existing bounded SDO service/channel and CiA's conditional standard remap procedure | Confirm selected-model applicability, device-specific conditions/activation and readback contract, then implement through the same service. No Lichuan sequence is claimed tested. |
| 3. Deadline/progress/effects | SDO owns payload, identities, node epoch and retained terminal | Concrete operation context, immutable overall budget, last confirmed step and partial effects for that reviewed sequence. |
| 4. Partial failure | `invalidatePdo(slot,true,now)` revokes only the affected map/qualification and retains history | Couple invalidation to the reviewed mutation's first possible-effect boundary; retain confirmed prefix and uncertainty. |
| 5. Type/timing/SYNC | Raw source fields and defaults | Supported types, timing units/ranges/effects/persistence; required SYNC behavior if established. These setters are not independently supported. |
| 6. Confirmation versus sample provenance | `pdoMapFromReadback`, `configurePdo`, `qualifyPdo` | Publish actual remap readbacks and integrate dependent-work withdrawal. Existing passive proof is not mutation completion. |

Existing tests already cover bad count/width/direction, payload/profile limits,
WO default RPDO targets without target uploads, identifier collisions in both
configuration orders, stale readback epochs/admission times, and old same-ID/DLC
bytes with unrelated traffic kept current. Independent review found no defect
requiring changes to those paths. Failure-at-every-mutation-step, rollback/replay
absence and last-confirmed-step tests remain unperformed because their actual
device applicability is not established. They remain owned by Prompt 18.

Resume with applicable manufacturer clarification or narrowly scoped, separately
authorized fixture evidence identifying model/firmware, legal NMT/drive state,
allowed targets/flags, application of the standard sequence, activation and
readback meaning. Keep persistence effects explicit; require timer/SYNC facts
only for operations that change or consume them. Q17 owns this gate; Q01 model
binding, relevant Q16 routes, Q20 read effects and Q15 persistence remain distinct.
A single qualified subset
may proceed without resolving every model. If owner construction is complete by
then, integrate through that owner before closing this stage. Prompt 45 checks
combined paths; it does not inherit unfinished Prompt 18 implementation.

## Current sibling source audit

All six checkouts were clean before and after read-only inspection. Only RS
advanced since Prompt 17, from `4234c6b` to the revision below; homing and
interruption integration changed, while DriverSettings implementation/tests did
not. No unchanged sibling suite was rerun.

| Checkout / revision | Actual implementation, caller and focused evidence |
| --- | --- |
| MotorControl-RS `f812d7c1d364a03be86f02ae8f648dc8fca23d42` | `src/profiles/ess_rs/DriverSettings.cpp:501-730`: atomic preparation, copied request, alternating write/readback, fixed operation deadline and effect-aware per-field progress. `examples/probe_cli/ProbeApp.cpp:2097,2138-2167,2282-2290`: owner acceptance/invalidation and advancement. `test/driver_settings_test.cpp:260-325,459-563`: cancellation, partial confirmation and deadlines. |
| ADS1115 `235af3e8c722661a89a44766a4ccd903a654f6fe` | `src/ADS1115.cpp:633-739,2053-2085`: bounded writes/verification and UNKNOWN after partial effects; `examples/02_owner_safe_poll/main.cpp:225-263`; `test/test_basic.cpp:2850-2945`. Sensor recovery is not motor replay policy. |
| INA228 `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` | `src/INA228.cpp:659-690,777-839,1071-1189`: operation/config generations, PARTIAL/INDETERMINATE effects and verification; bringup caller `:299-373`; `test/test_basic.cpp:5510-5570`. |
| MB85RC `1bf7c293549b1038b2d8f3b7a8d655fa74201e6e` | `src/MB85RC.cpp:674-735,1452-1488`: accepted prefix/failing chunk; bringup caller `:1568-1606`; `test/test_basic.cpp:5470-5500`. No FRAM semantics or restoration policy transfers. |
| RV3032-C7 `aabd8d9204e75072403fefb60cf8d2679a7f56de` | `src/RV3032.cpp:634-665,730-825`: mutation and cleanup evidence; bringup caller `:185-191`; `test/test_native/test_datetime.cpp:9136,9483`. RTC cleanup does not justify PDO rollback. |
| FieldCore-node `5ad9292dc8481cf08be9a61f662495a6474d1da0` | `src/devices/rs485_motor/Rs485MotorModule.cpp:50-75`: retained-result/config-generation rules; `src/i2c/I2cTask.cpp:1297-1315` and `Ads1115Module.cpp:821-885`: route/configuration trust and effects; native motor/I2C transcript tests. No product dependency or edit. |

Reuse RS `prepare.../next.../advance...` and concrete terminal evidence when the
sequence is implementable. Keep the existing CAN receiver, fixed route storage,
SDO settlement and per-route invalidation. Four Lichuan sets do not constrain
other profiles; no N5/EPOS4/proprietary mutation support is inferred.

## Verification and handoff

Three parallel fresh-context reviewers: `gpt-6-astra` high for manufacturer/stack
evidence; `gpt-6.1-sol` high for sibling contracts; `gpt-6.1-sol` high for scope
and existing-code/test correctness. All were read-only. The lead owns ledger,
documentation, integrated checks and Git. No new production abstraction, state,
capacity, dependency or public declaration was needed; no speculative API was
frozen to fill the missing sequence.

Independent current PDO builds/tests passed: codec **630 checks**, owner
observations **490 checks**. Lead full verification passed:

```text
python scripts/verify.py --mode full --compiler gcc --build-dir .build/p18-full
```

Run `20261030T092059359850Z-85308-k6v30_lw` passed 31 non-package CTests,
the separate package/source/export/relocated consumers, independent C++11/C++17/
no-RTTI headers, generation freshness and strict Doxygen 1.13.2/API extraction.
This verifies existing software at the entry revision, not a new remap feature.
After the ledger/documentation changes, all six focused CTests passed:
`pdo_codec`, `pdo_observations`, `objects_generated`, `objects_generator_contract`,
`planning_integrity` and `ledger_validator_contract`. Direct strict Doxygen
passed again (log `.build/p18-final-documentation.log`); generator `--check`,
promptset integrity and `git diff --check` passed. Original-byte checks against
INDEX/HEAD accompany synchronization. No runtime or public header changed after
the full run. Local Linux
sanitizers and hardware were not run. Documentation/evidence-only delivery does
not require manual remote CI dispatch under the project policy.

Final independent evidence and scope reviewers found no blocking report or
coverage issue; the stack flag wording was tightened to avoid implying actual
TX. Commit and sync these checked findings on the same execution branch; final
commit/upstream comparison are verified at delivery. Prompt 18 stays Blocked.
The next independently ready numbered work is Prompt 19 when assigned; it does
not satisfy this stage's remaining requirements and is not executed here.

## Fresh independent audit

Re-read original Prompt 18 in full on 2026-10-30 and inspected the actual
`c5253b8..7649ec1` diff and current implementation. Audit entry was
`7649ec1b1c82ee80d0d585d6fd0b7f99534d0c92`, clean tree/index on the same branch
and upstream, with a fresh fetch at 0 ahead / 0 behind. Three new fresh-context
reviewers covered primary evidence (`gpt-6-astra`, high), code/edge cases and
requirements/live sibling sources (`gpt-6.1-sol`, high). All remained read-only.

**Correction:** the original rationale made the remapping order appear
stack-specific or unknown. [CiA's PDO description](https://www.can-cia.org/can-knowledge/pdo-protocol/)
documents the conditional procedure and distinguishes variable mapping in
Pre-operational from dynamic mapping, which also permits Operational. Lichuan
EN86/ZH86 p3 explicitly claims CiA301; RW mapping tables support conditional
applicability but do not establish actual edit restrictions and activation.
The lead checked the primary CiA page and retained source tables independently.
A verbatim vendor restatement of the standard is not required. The remaining
blocker is selected-model applicability, not the absence of a physical test or
knowledge of the generic sequence.

Q17 and maintained guidance now keep three obligations separate: the selected
mutation contract, settings actually changed/consumed, and post-change sample
qualification. A future established narrow remap may retain existing timing
settings and leave samples unqualified. Unknown persistence must stay explicit;
it does not justify an invented volatile-only guarantee or implicit save.
No confirmed independently implementable mutation subset was found. Existing
layout/readback/provenance code already covers the available passive subset;
adding a hypothetical profile or generic sequencer would not complete this stage.

Fresh downloads of all five retained PDFs and the EDS are byte-identical to
their originals, verified by SHA-256 in
`.build/p18-reaudit-evidence/download_comparison.json`. The current manufacturer
index HTML differs but retains the same download links. Product-page retrieval
failed local certificate verification and timed out through browsing; no fresh
product-page conclusion is claimed. CiA's inspected HTML is retained in that
ignored directory with SHA-256
`45093e79d496711d9d0edc7cc06ad21927b02c038a7201a6f06fe924a7321276`.
Original reference bytes/manifests and generated C++ remain unchanged.

Live RS, ADS1115, INA228, MB85RC, RV3032-C7 and FieldCore revisions and clean
states match the table above before/after inspection. Reviewers read current
configuration implementations, actual owner callers, focused tests and verifier
registration; no relevant behavior delta was found and no sibling suite rerun.
No defect was reproduced in atomic PDO preparation, copied readback evidence,
WO targets, count/width/route checks, lifecycle invalidation or reused-route
qualification. All six original implementation obligations stay in Prompt 18.

Fresh quick verification passed 30/30 tests in
`.build/p18-reaudit-code/verification`, run
`20261030T092921251637Z-31684-rxj82moi`. A separate 21-check adversarial harness
passed: callback-time input mutation, wrong node/map epochs, false distinct-route
claims on reused IDs, old captured bytes and retained observations after map
withdrawal. Its source/binary remain under `.build/p18-reaudit-code/`.
These checks validate existing software, not an implemented Lichuan remap.

The first full audit run, `20261030T093438261915Z-83648-3rudsty5` under
`.build/p18-reaudit-full/`, failed: `sdo_codec` and `api_ownership` exceeded
their unchanged 10-second CTest limits without output; 29 other tests passed.
The verifier stopped before package and Doxygen checks. The original logs were
retained. Both exact binaries then passed in 0.01 seconds, and five repetitions
each passed in 0.00-0.07 seconds. Their executable sections match the independently
passing quick-build binaries byte-for-byte. Review found no test sleep/hardware
wait or reproduced runtime defect. This does not explain or fix the initial
timeouts; their cause remains unresolved. Reproduction logs and binary comparison
are retained under `.build/p18-reaudit-code/`. No timeout or test was weakened.

A fresh full run, `20261030T093839659292Z-7036-cnptp10d` under
`.build/p18-reaudit-final/`, passed all 31 non-package CTests, separate package
and relocated-consumer checks, generation freshness, strict Doxygen 1.13.2 and
API extraction. Command:

```text
python scripts/verify.py --mode full --compiler gcc --build-dir .build/p18-reaudit-final
```

The passing rerun does not erase the unexplained initial failure. Promptset,
generator and diff checks passed; staged and committed reference-byte checks
accompany synchronization. No runtime source or public declaration changed.
This documentation/ledger correction requires no manual remote CI dispatch;
Linux sanitizers and hardware were not run locally. Final commit and fresh
upstream comparison are reported at delivery. Prompt 18 remains Blocked on the
scoped device evidence above, with all implementation obligations retained.
