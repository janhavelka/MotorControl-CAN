# Prompt 13 - Bounded SDO transfer lifecycle

## Scope and current state

Implemented the selected CANopenNode application adapter with bounded transfer
progress, copied admission intent, immutable deadlines, retained evidence and
persistent per-channel quarantine. No controller I/O, hardware exchange,
automatic retry, motion operation or channel reopening is implemented here.
Complete: focused contracts, independent reviews, integrated local verification
and implementation-checkpoint CI pass. The checkpoint is committed and synced;
the final documentation/build-standard closure receives its own exact-commit CI
check before handoff.

Started clean at `1e3df68215d2786252e8edacce45aaa5c039c62f` on the continuing
`promptset/motorcontrol-can` execution branch. Configured upstream is
`origin/promptset/motorcontrol-can`; fresh fetch showed 0 ahead / 0 behind.
The original assignment, COMMON/shared contracts, actual workflow/verifier and
prerequisite 5/6/11 code and reports were inspected before implementation.

## Implemented contract

| Requirement | Delivered behavior and evidence |
| --- | --- |
| One selected engine/storage | [Optional SdoClient](../../adapters/canopennode/SdoClient.h) promotes the existing channel storage; CANopenNode remains the only protocol engine. The old bridge is a compatibility facade over it, without duplicate callbacks, engine or TX queue |
| Channel isolation | Two fixed unique physical routes on one interface; one active/retained transfer per route. Duplicate/crossing IDs reject regardless of logical axis/subaxis/profile/epochs. Unrelated frames remain available to the sole application dispatcher |
| Owned admission | [Portable intent/result](../../include/MotorControlCAN/canopen/SdoTransfer.h) retain channel/binding/configuration epochs, operation/request/step identities and complete payload. Refused admission changes no ID or storage |
| Independent evidence | Local rejection, retirable software staging, possible TX, confirmed TX/ACK, matching SDO success, full abort, timeout, cancellation, transport failure, reboot and bus-off remain separate. Protocol acknowledgement is not motion completion |
| Required forms | Exact 1-32-byte expedited/segmented uploads/downloads using the selected engine; all supplied EDS scalar widths 1-4 covered. Checked unsized expedited prefix normalization, declared size, capacity, toggle, final total and canonical initiation checks; no block transfer |
| Unsupported discovered form | Explicit UNSUPPORTED result for disallowed segmented upload after initiation; selected-engine client abort staged within original queue/overall bounds. Retained result never claims an unsent abort reached the peer |
| Fixed time | Absolute exclusive admission deadline never renews. First queue cutoff remains fixed at admission; later frame staging and actual-handoff response intervals clamp to remaining overall time. Backward time rejects; subtraction before addition handles UINT64_MAX safely |
| Terminal ownership | First terminal remains immutable until explicit release. Bounded cleanup can continue after release without altering the terminal, its original deadline or quarantine |
| No unsafe reuse | Any unsuccessful possibly transmitted transfer quarantines its route, including a non-changing upload with a late-reply risk. Bus-off/reboot and transport failure invalidate applicable channels. Release, tokens, quiet time and client abort do not reopen them |
| Effects | Unqualified uploads can change state. effectPossible remains distinct from effectUncertain and service success; only explicitly reviewed non-changing reads have no changing effect. Full server abort does not roll back earlier workflow writes |

The adapter accepts only explicitly source-backed routes. Every configuration
requires an external fresh-session/local-TX/RX premise. A new C++ object or a
matching probe cannot manufacture that premise. Bounded reviewed non-changing
confirmation must already be supplied or performed under that premise before
changing work is admitted. Current Lichuan read-effect qualification remains
open; host synthetic qualification flags are not device evidence.

The staging port performs no physical I/O. `nextTx` marks possible transmission
before returning bytes because actual controller acceptance may transmit at
once. Callers must not cache/replay handed-off work after cancellation/expiry.
`confirmTx` records only the current frame's controller report. The selected
native timers receive zero elapsed time; owner-supplied microsecond queue,
response and overall cutoffs govern the lifecycle independently of native
per-segment timer resets. Work per call is explicitly bounded in Doxygen.

## Source confirmation and recurring audit

Source-confirmed: original EDS `1200sub1/sub2`, lines 329/338, are RW U32,
PDO=0 with literal defaults `$NODEID+0x600` and `$NODEID+0x580`. These are
metadata, never installed route facts. Independent counting confirms 202 typed
entries: 42 U8, 91 U16, 61 U32, one I16, seven I32, all 1-4 bytes. Original
EDS SHA-256 remains
`dd3c96e7282c012a522dddb5bbdbbf561c1f5c23716b31f12472f45829d4f52c`.
No manufacturer claim, ledger permission, firmware fact or object definition
was changed; generated freshness remains independent and active.

Primary selected-engine source at pin
`9b8beed8367241e96ac03f916cd5a500bcb2cf23` confirms command/segment bit layouts
in `301/CO_SDOserver.h`, upload/abort/timer behavior in `301/CO_SDOclient.c`
and copied callback/FIFO ownership. The adapter adds exact object filtering
before native abort callbacks, scalar width normalization for unsized `42`,
FIFO draining and owner cutoffs. Public-example sleeps supply neither matching
acknowledgement nor transaction identity. No vendor timing is inferred from
native timeout arguments or the manual's approximate enable-step interval.
No device fact was physically measured.

| Checkout | Revision and current dirty state | Actual implementation/caller/test seam and CAN difference |
| --- | --- | --- |
| MotorControl-RS | `bd831c859daa9c8c07b8d27a2dd879bb4e750511`; 17 modified documentation files plus untracked `docs/reports/2026-10-29_ess_software_contracts.md` at audit | `examples/common/RtuBusOwner.cpp` admit/result/release/collect/cancelActive/cancel, RtuRunner cancel, ProbeApp callers, bus_owner/runner tests. Inspected implementation/caller/test files remain clean. Copy intent and avoid pumping unsent work to cancel; RTU quiet recovery is not SDO reuse proof |
| ADS1115 | `235af3e8c722661a89a44766a4ccd903a654f6fe`, clean | `src/ADS1115.cpp` explicit no-effect versus indeterminate callback classification; CAN keeps service/effect facts separate and does not copy sensor retry policy |
| INA228 | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b`, clean | Reset-owned state marked suspect before CONFIG.RST and raw reset dirty-state regression; no implicit reset/recovery is imported |
| MB85RC | `1bf7c293549b1038b2d8f3b7a8d655fa74201e6e`, clean | WriteCommit, verified-write WAITING_FOR_RECONCILIATION/resume and one-write/one-read tests. CAN copies payload instead of borrowing expected data, and never treats readback as general session retirement |
| RV3032-C7 | `aabd8d9204e75072403fefb60cf8d2679a7f56de`, clean | Attempted/ambiguous calendar-write evidence, later readback and post-commit failure tests. Retain original failure; RTC readback does not authorize replay or drive reconciliation |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0`, clean | MotorControl contract, Rs485MotorModule frozen target/deadline/correlation and host motor tests. Framework remains read-only; no RTU address/native RPM or application health assumptions enter common CAN |

No behavior claim relies on the dirty RS documents; the live documentary delta
is explicitly separate from unchanged implementation paths. No sibling suite,
repository mutation or hardware action was performed.

## Review and corrections

One implementer owns production session storage, portable contracts, CMake and
Git. Independent vector construction and compatibility migration ran in
disjoint paths using the inherited available model/effort. A fresh-context
`gpt-6-astra`, high-effort reviewer independently inspected deadlines,
cancellation, effects, cleanup after release and route qualification.

Confirmed issues fixed before exit:

- Stronger review exposed a missing explicit freshness premise for bootstrap
  configuration. It is now required even when non-changing confirmation has
  not yet completed; a successful stale read cannot establish freshness.
- Lead review found first-service queue-cutoff renewal. The initial cutoff now
  remains fixed at admission; an admit-0/service-4/cutoff-5 regression proves
  the queued frame cannot escape at 5.
- Legacy timeout marker differences are translated only in the compatibility
  facade; production local timeout never becomes a valid peer abort.
- Static archive ordering initially left port symbols unresolved. The single
  port object now belongs to the same engine archive, preserving ordinary
  static linking without duplicate symbols or forced consumer flags.
- Optional adapter plus enabled tests initially duplicated a Ninja configure
  dependency. One pinned engine creation path now owns its hash/dependency
  checks; both optional build modes pass without weakening provenance.
- Per-call documentation now includes initiation, a possible abort progression
  and FIFO drain rather than understating the bounded work.
- The promoted C++ port object explicitly compiles as strict C++11 in the
  mixed C/C++ engine archive. Public Doxygen states the external no-duplicate
  premise required for successful-channel reuse; host tokens cannot prove it.

## Verification

Tools: Python 3.12.10, CMake/CTest 4.0.1, Ninja 1.12.1, MinGW GCC/G++ 15.1.0,
Doxygen 1.13.2. Local Clang/Linux sanitizer runtimes and clang-format are
unavailable; the existing CI provides GCC 13/Clang 18 ASan/UBSan. Source style
follows the existing four-space direct-function convention.

| Check | Actual local result / evidence |
| --- | --- |
| `.build/p13` strict C++11 configure/build | PASS; 5 focused contracts: lifecycle, checked adapter, retained owner, retained fit and native consumer |
| Independent production fake server | PASS 6021 checks in `test/sdo_transfer_test.cpp`: sizes/sign-preserving raw bytes, sized/unsized, 32-byte FIFO boundary, toggle/final/size faults, unsupported cleanup, route aliases, copied payload, stable terminal, every cancellation boundary, stalled/delayed TX, immutable cutoffs, UINT64_MAX, reboot, bus-off, two nodes and two channels of the same node |
| Independent selected-stack byte probe | PASS 110 checks; `.build/p13-vectors/vectors.cpp`, actual pinned C engine and independently specified frames |
| Stronger adversarial harness | PASS 66 checks; `.build/p13_deadline_review.cpp`, cleanup after release, bus-off retirement, between-segment cancellation, stale reports and mutation of caller route facts |
| Standalone retained commands | PASS service fit 1/1 in `.build/p13-fit-standalone`; owner/contrast/fit 3/3 in `.build/p13-owner-standalone` |
| Optional application adapter | PASS explicit source-pin build with BUILD_TESTING=OFF in `.build/p13-optional`; optional+tests build and lifecycle contract in `.build/p13-optional-tests` |
| Repository checker and strict Doxygen | PASS; 15 repository failure/registration tests and direct `doxygen Doxyfile` |

`python scripts/verify.py --mode full --compiler gcc --build-dir .build/p13-full`
passed 24/24 non-package CTests, the independent package contract and strict
Doxygen/API extraction (12 installed headers plus the optional adapter API).
Retained run: `20261029T154634751591Z-89236-8j_56aq0`. It builds isolated
source/install/export consumers, C++11/C++17/no-RTTI and independent headers
without the adapter, stack, Python, ledger or references in the core package.

`python scripts/verify.py --mode quick --compiler gcc --build-dir .build/p13-quick`
passed 23/23 CTests, retained as `20261029T154746140084Z-85404-9qwbhjau`.
After the final explicit C++11 setting and public reuse-premise clarification,
the same quick command passed 23/23 again, retained as
`20261029T155824186969Z-69180-5_iqb8xp`; four focused native contracts and
direct strict Doxygen also passed.
Version/object freshness, repository and source integrity passed. An altered
external upstream source intentionally failed optional CMake configuration
with its exact SHA-256 diagnostic; `.build/p13-pin-negative/negative-command.log`
retains the command and failure. Original sources and generated objects remain
unchanged. Staged and committed provenance are checked around delivery.
These are host results, not physical CAN, firmware or manufacturer capability
qualification.

## Documentation and maintainability

The [service guide](../canopen_services.md) owns forms, time precedence,
effects, cleanup, route premises and quarantine. Adapter README owns actual
optional-target commands; source Doxygen covers fields, lifetimes, time,
borrowing/overlap and failure behavior. README, CHANGELOG, package/owner guides,
source map, coverage, backlog, roadmap and PROGRESS reflect the new boundary.
No ledger row is promoted to a typed semantic function or hardware coverage.

Reused/simplified/removed: one selected engine and the old storage/port are
promoted; the duplicate fake port implementation is removed; the legacy bridge
is a facade. New abstractions are only portable owned service values, a fixed
two-channel adapter, per-frame reports and the optional CMake target. No virtual
registry, universal operation state, heap, I/O, hidden clock or retry is added.
Current Windows x86-64 sizes: adapter 1464, intent 112, completion 208, port 88,
native client 176, compatibility facade 1624, outer proof owner 2616 bytes.
Nested sizes overlap; none measures embedded RAM, stack, service gaps or timing.

## Remaining boundaries and handoff

No deferred uncertainty fix belongs to this lifecycle. Channels remain
unresolved when safe reuse is unproved. Prompt 14 owns deeper adversarial
correlation and evidence-backed reconciliation; 15/17 own events/PDO; 23/24
own full application-owner integration/recovery; 27 owns actual SDK/controller
retirement. The current port deliberately rejects unreviewed non-little-endian
targets. Lichuan source/model/firmware/read-effect/write gates remain open.

Implementation checkpoint `48c80b5ad177f428bd73d6d2cad3881e4d37b8aa` is pushed
to `origin/promptset/motorcontrol-can`; a fresh fetch confirmed 0 ahead / 0
behind. Working, staged and committed source-integrity checks passed, as did
`git diff --check`. Its [CI run 40905770119](https://github.com/janhavelka/MotorControl-CAN/actions/runs/40905770119)
passed Windows quick verification and both Linux GCC/Clang full ASan/UBSan,
package and strict Doxygen gates on that exact SHA.

The final closure commit's synchronization and exact-commit CI are checked
before handoff and reported with its resulting SHA. No firmware, physical test
or claimed device support is hidden behind the native results.
