# Prompt 13 independent audit

## Scope and starting state

Re-read [the original assignment](../prompts/13_sdo_lifecycle.md) in full and
the parent/root guidance and shared contracts. Inspected the actual
`1e3df68..56da844` implementation diff, production adapter and portable values,
selected engine, retained facade, tests, package consumers, verifier and
workflow. Started clean at `56da84491c8b7fb0b58a4659a3e0f4383b6cd6d2` on
`promptset/motorcontrol-can`; fresh fetch confirmed 0 ahead / 0 behind
`origin/promptset/motorcontrol-can`.

Three parallel read-only reviews covered state/deadlines, adversarial vectors
and requirements/source/package boundaries. The state reviewer used the
available `gpt-6-astra` with high effort and fresh context; the other reviewers
used the inherited model/effort with fresh briefs. The lead independently
reproduced both findings and owns all tracked edits and Git operations.

## Confirmed findings and corrections

1. `SdoClient::receive` required a pending response and empty staging even for
   a server abort. After a segmented initiation or nonfinal reply, the next
   frame is already staged; a valid abort was ignored and `nextTx` could send
   that frame afterward. Both upload and download reproduced
   `consumed=0 terminal=0 sends_after_abort=1`. The pinned
   [CO_SDOclient.c](../../experiments/canopen_service_fit/vendor/CANopenNode/301/CO_SDOclient.c)
   callback accepts aborts in every non-IDLE state, and its
   upload/download processing handles them before normal response states.
   The adapter now accepts a full matching abort after possible transmission,
   checks its object before mutation, retires staged TX and lets the selected
   engine publish server-abort evidence. Wrong-object aborts preserve staging;
   untransmitted requests cannot acquire abort evidence. Quarantine, raw abort
   bits, unrelated channel progress and first-terminal retention survive.
2. `confirmTx` also used the pending-response flag. A valid nonfinal reply
   could therefore invalidate the newest controller handoff report before
   another handoff occurred. A five-byte download reproduced service success
   with a rejected actual TX report and `transmissionConfirmed=false`.
   Confirmation now checks live ownership, current frame, possible TX and
   expiry without requiring a pending response. The next handoff still
   supersedes the token; terminal results remain immutable. Reports never
   renew queue, response or overall cutoffs.

Added regressions first: they failed on the two original guards before the
corresponding fixes. Both directions and two segment stages exercise abort
retirement; another node continues normally. Confirmation vectors cover the
reply-before-report ordering and stale tokens after the next handoff.
Independent reviewers rebuilt the corrected actual target and found no
additional confirmed defect.

## Fresh source audit

All inspected sibling worktrees are currently clean. Historical dirty-state
facts in the [implementation report](prompt_13.md) remain unchanged there.

| Checkout | Current revision | Implementation and caller/test seam |
| --- | --- | --- |
| MotorControl-RS | `af06eac6846e10f38d0f02fd641f545f2f53f2ad` | `examples/common/RtuBusOwner.cpp` copied admission/result/release/cancelActive; `RtuRunner.cpp` cancellation; ProbeApp callers and bus_owner/runner tests |
| ADS1115 | `235af3e8c722661a89a44766a4ccd903a654f6fe` | `src/ADS1115.cpp` cancellation callback no-effect/indeterminate boundaries and lifecycle tests |
| INA228 | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` | `src/INA228.cpp` `_cancelJob`, reset invalidation and raw reset tests |
| MB85RC | `1bf7c293549b1038b2d8f3b7a8d655fa74201e6e` | `src/MB85RC.cpp` WriteCommit, retained verification/reconciliation and no-replay tests |
| RV3032-C7 | `aabd8d9204e75072403fefb60cf8d2679a7f56de` | `src/RV3032.cpp` attempted/ambiguous writes, later readback and post-commit failure tests |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0` | `include/TunnelMonitor/contracts/MotorControl.h`, `src/devices/rs485_motor/Rs485MotorModule.cpp`: binding/deadline/terminal retention and independent effect projection with native host tests |

RS advanced six commits from the earlier `bd831c8` audit: typed velocity/homing
reads and shared application/parser coverage were added, and prior documents
were committed. Direct diff confirms the inspected RtuBusOwner/RtuRunner and
their bus_owner/runner tests are unchanged. Other sibling revisions match the
implementation audit. CAN retains copied service storage and permanent channel
quarantine; neither RTU quiet recovery nor sensor readback becomes SDO reuse
proof. No sibling suite or repository mutation was performed.

Original EDS `1200sub1/sub2` at lines 329/338 still contain RW U32 defaults
`$NODEID+0x600` and `$NODEID+0x580`. Its SHA-256 remains
`dd3c96e7282c012a522dddb5bbdbbf561c1f5c23716b31f12472f45829d4f52c`.
Independent raw counting confirms 202 typed entries: 42 U8, 91 U16, 61 U32,
one I16, seven I32. All 11 engine source/license hashes match the selected
`9b8beed8367241e96ac03f916cd5a500bcb2cf23` manifest. Pinned public-example
sleeps still provide no transaction acknowledgement. No manufacturer claim,
reviewed permission, source artifact or generated catalogue changed.

## Verification

- Focused native build and five lifecycle/adapter/owner/fit/consumer CTests:
  PASS after both fixes; lifecycle executable reports **6,254 checks**.
- Independent state harness: PASS 768 command-byte states, abort-before/after
  staging and delayed-report vectors in both directions. Actual before/after
  output is retained in `.build/p13-audit-state/result-before-fix.log` and
  `result-after-fix.log`.
- Independent vector harness: PASS 3,845 protocol assertions plus 96 report,
  cutoff and byte-for-byte terminal-retention assertions, under
  `.build/p13-audit-vectors`.
- `python scripts/verify.py --mode full --compiler gcc --build-dir .build/p13-audit-full`:
  PASS 24/24 non-package CTests, independent source/export/relocated installed
  consumers, 12 independent public headers, C++11/C++17/no-RTTI, missing-package
  dependency rejection and strict Doxygen/API extraction. Corrected-tree run:
  `20261029T161621046602Z-69872-vuv9zrn3`. This full run covers the quick contract
  set and adds package/documentation checks; no second quick run was needed.
- `python scripts/check_promptset.py` and `git diff --check`: PASS. Original
  134 artifacts, 99 EDS indexes, 202 typed entries and 53 manual items remain
  intact. Staged/HEAD checks are repeated around delivery.

Local tools: Python 3.12.10, CMake/CTest 4.0.1, Ninja 1.12.1, GCC 15.1.0 and
Doxygen 1.13.2. Linux sanitizer/Clang execution is unavailable on this Windows
host; the configured final-commit CI supplies both GCC/Clang ASan/UBSan jobs.
Its exact result is checked after commit and reported in the handoff. An older
green implementation run is not treated as verification of these corrections.

## Scope, simplicity and handoff

The corrections change two state guards and retire existing staging;
they add no state, allocation, retry, dependency, codec engine or public API.
Source Doxygen, the service guide and CHANGELOG describe the corrected
ordering; portable outcome documentation now says admission rejection rather
than implying that the adapter returns Status. Coverage/PROGRESS link this
audit. Backlog/roadmap ownership and installed support are unchanged.

Core installation/export still contains only portable values/codecs, not the
adapter, native stack or vendor references. The retained proof remains one
facade over the selected engine. Common non-CANopen outcomes do not require
SDO acknowledgement. No hardware traffic, motion, firmware qualification or
channel reopening was added; reconciliation remains prompt 14. No confirmed
Prompt 13 obligation is deferred. Final commit/synchronization and CI are
checked before handoff and reported with the resulting SHA.
