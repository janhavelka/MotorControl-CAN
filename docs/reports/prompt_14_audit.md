# Fresh independent Prompt 14 audit

Audited 2026-10-29 against original
[Prompt 14](../prompts/14_sdo_reconciliation.md) in full. Starting revision:
`54fabb961bbe092a38178407f3c3cd7bde884208`; inspected implementation diff:
`6af5283..54fabb9`. Existing branch `promptset/motorcontrol-can`, upstream
`origin/promptset/motorcontrol-can`, clean index/worktree and fresh fetch at
0 ahead / 0 behind. No branch change or sibling mutation.

## Findings and corrections

No production state-machine defect was reproduced under the documented
cooperative ownership, monotonic caller time and qualified session premises.
The independent reviewers identified two retained-test gaps and one test-order
clarification; all were corrected within this stage's scope.

1. **Segment toggle reuse was underspecified.** A 21-byte download uses segment
   acknowledgement commands `20, 30, 20`. An old first-segment ACK delivered
   after the third handoff can match the final expected ACK. The pinned
   `CO_SDOclient.c` download-segment branch checks command/toggle, not a sequence
   number or echoed written value. The existing no-duplicate premise now
   explicitly covers every initiation and segment exchange during a transfer,
   not just reuse after an entire SDO completes. A retained counterexample shows
   the unknown-capture limitation; original pre-handoff capture instead yields
   `CORRELATION_LOST`. A fresh final reply is the positive case. Neither local
   tokens nor an accurate timestamp for a duplicate arriving late on the wire
   establish its server origin. No second engine or invented recovery was added.
2. **Cancellation boundary coverage omitted segmented uploads.** The existing
   seven-boundary test now exercises downloads and both unqualified/reviewed
   uploads, including after partial FIFO progress. It checks retired staging,
   blocked cached TX, retained effects, no published partial upload value and
   quarantine after possible transmission. Review classification does not grant
   a real Lichuan read permission.
3. **One test queried time 4 before admitting at time 3.** The pure `canTransmit`
   query does not advance the stored processing clock. The test now supplies
   nondecreasing time, and Doxygen/owner guidance makes the caller obligation
   explicit. No unsafe TX behavior was reproduced from valid caller input.

The fixes change documentation and retained tests only. Runtime branches,
storage capacities, exported declarations, selected stack and dependency pins
are unchanged. The existing test fixture was extended directly rather than
introducing another framework, test engine or broad refactor.

## Requirements and source refresh

All six original requirements remain covered: explicit processing/capture
precedence; indistinguishable reply limits; permanent quarantine without sound
reopening evidence; TX/binding retirement and stable effects; reviewed read-effect
classification/partial multi-write evidence; independent proven-route capacity.
SDO settlement remains below generic motion outcomes. Exact current contracts
are [services](../canopen_services.md) and [owner](../can_owner.md).

Fresh before/after source checks found these same clean revisions:

| Checkout | Revision |
| --- | --- |
| MotorControl-RS | `a5a02b74a536fe5875e6256a32477dc850d96807` |
| ADS1115 | `235af3e8c722661a89a44766a4ccd903a654f6fe` |
| INA228 | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0` |
| MB85RC | `1bf7c293549b1038b2d8f3b7a8d655fa74201e6e` |
| RV3032-C7 | `aabd8d9204e75072403fefb60cf8d2679a7f56de` |

Reviewers reopened actual RS `RtuBusOwner.cpp`/`RtuRunner.cpp`, probe generation
checks and owner/runner tests; ADS1115/INA228 cancellation, effect and retained
result paths and their example/test callers; FieldCore `Rs485MotorModule.cpp`
invalidation and motor-module tests; narrow MB85RC/RV3032 reconciliation paths.
Relevant implementations and workflows have no delta from
[the prior exact source map](prompt_14_sources.md). Sibling suites were not rerun.

Pinned CANopenNode `9b8beed8367241e96ac03f916cd5a500bcb2cf23` callback and
download-ACK/toggle branches, SRT shared-status/sleep/unchecked-send anti-examples,
original EDS sections and EN86 PDF pages 12/17 plus ZH86 pages 12/18 were reopened.
Inspected TWAI headers still supply no ingress timestamp or remote-session
retirement guarantee. Original hashes match. The ledger remains 202 entries with
unestablished read effects; all 194 descriptors remain `UNQUALIFIED`. No vendor
fact, original artifact or generated permission changed. Q18/Q16/Q20/Q01 and
applicable Q02 remain distinct device gates; no hardware was used or qualified.

## Independent review and verification

Three fresh-context workers reviewed the actual code/diff in parallel:
`gpt-6-astra` high for state/correlation, `gpt-6.1-sol` high for independent
vectors, and `gpt-6.1-sol` high for scope/source evidence. Only the vector worker
edited its assigned test file. The lead owned declarations/docs, integration
and Git. Workers made no Git mutations or sibling changes.

Fresh independent GCC 15.1 C++11/C99 builds passed the registered lifecycle,
reconciliation, adapter, ownership, fit and multidrive tests. An additional
1,488-check upload probe exercised seven TX boundaries across cancel, transport
failure, reboot and invalidation for both read-effect classes. The independent
toggle probe reproduced the documented ambiguity and captured-stale quarantine.
Probes are retained under `.build/p14-reaudit-vectors` and
`.build/p14-reaudit-state`; they are not substitute service implementations.

The revised registered reconciliation test passes **1,731 checks**. The stronger
reviewer then inspected the actual final changes and reran the focused tests;
no further correction was found.

The lead's integrated command passed:

```text
python scripts/verify.py --mode full --compiler gcc --build-dir .build/p14-reaudit-full
```

Run `20261029T194113391581Z-47092-qa48ipu2` passed all 25 non-package CTests,
the separate package contract (source/export/relocated-install consumers),
independent C++11/C++17/no-RTTI headers, generated-file freshness and strict
Doxygen 1.13.2/API extraction. `git diff --check` and promptset integrity passed.
Windows GCC 15.1 does not supply the Linux sanitizer checks; automatic CI and
synchronization are checked on the final commit at delivery, separately from
this local working-tree run. An older green commit is not evidence for these
audit changes.

Prompt 14 remains Complete with the strengthened tests and precise qualification
premises. No confirmed runtime correction or deferred owned-scope work remains.
No later prompt, motion sequence, real controller port or reopening capability
was implemented by this audit.
