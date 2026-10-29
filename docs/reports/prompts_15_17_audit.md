# Fresh independent audit of prompts 15-17

## Scope and baseline

Re-read the three original prompts and shared contracts in full; reviewed actual
code, callers, tests and changes from `a473e6e` through `8e60a4e`. The continuing
promptset-v2 branch is `promptset/motorcontrol-can`, upstream
`origin/promptset/motorcontrol-can`. Entry HEAD was
`8e60a4e67b2876cba2a0ff946395c4c4c3c82cd6`; tree/index were clean and fresh fetch
showed 0 ahead / 0 behind. This is an audit of completed scope, not prompt 18.

## Findings and corrections

1. **Initial attachment renewed old service evidence.** A successful SDO at
   capture time 3, followed by network attachment at 100 and age cap 5, was
   relabeled as fresh external proof at 100. Even an unknown-capture response
   could admit a subsequent write. `Network.cpp` now imports original time and
   origin from fixed pending channel evidence, without renewing it at attachment.
   An initial external confirmation uses the owner's last supplied time as a
   conservative bound. Unknown capture stays unknown. Quarantined channels do
   not provide an initial premise or reopen through monitoring attachment.
2. **Processing time was invented as capture time.** The retained fit facade
   called the three-argument receive overload using processing time twice.
   After boot-up, a queued heartbeat could appear to have been captured after
   the reset boundary. It now calls the processing-only overload. A regression
   proves that the later unknown-capture heartbeat cannot renew passive facts.
3. **SDO history could erase stronger service evidence.** An older or
   unknown-capture response overwrote a newer known external premise, disabling
   a still-serviceable requested write. Matching-response history and service
   qualification now apply their own ordering. Valid native aborts still prove
   service communication while their channel remains quarantined.

Independent review also caught and fixed a candidate migration defect: selecting
the newest service proof could lose an earlier matching SDO observation depending
on slot order. Pending SDO history is retained and imported separately from the
service proof. Tests cover both slot permutations and unknown capture on the same
channel. All pending evidence is consumed at initial anchor installation; the
shared node snapshot remains the single current owner thereafter.

## Requirement review

| Prompt | Independently checked result |
| --- | --- |
| 15 | Checked configured boot-up/heartbeat/EMCY envelopes, raw error bits, single dispatch, independent clocks, bounded history/overflow and synchronous reset/binding/controller invalidation. No heartbeat restores configuration/reference, faults or command outcomes. Processing-only callers now preserve unknown capture. |
| 16 | Native command vectors, addressed/broadcast membership, queue/possible/confirmed TX, immutable cutoffs, requested versus observed state, service age, heartbeat-disabled probing and preserved SDO uncertainty. No further confirmed NMT defect. |
| 17 | Pure layout/native-width validation, independent template/actual-map/sample evidence, failed readback, same-ID/DLC ambiguity without old-generation labels, first TPDO with unknown NMT, four/eight-set contrast and event/SDO interleaving. No PDO algorithm or permission defect found. |

Original EDS mappings and rendered EN86 PDF page 16 were reopened. Pinned
CANopenNode NMT/PDO implementations were checked against the corresponding
contracts. Source claims remain documentary: four Lichuan sets, WO RPDO targets,
default producer heartbeat zero, and unresolved mode/velocity signedness. No
source claim, generated declaration, read effect, access gate or firmware
applicability was changed. Host-loss stop behavior and live sample retirement
remain unmeasured; no hardware activity occurred.

## Refreshed sibling evidence

Read-only review covered implementations and actual callers/tests; no unchanged
sibling suite was rerun.

| Checkout | Revision / state at review | Relevant seam and delta |
| --- | --- | --- |
| MotorControl-RS | `70791727a502f7c8dc0536c0496ea236e5f4df60`, clean | Advanced during audit from dirty `6493e183` through concurrent Homing work. Inspected `examples/probe_cli/StateCache.h`, common RtuBusOwner/RtuRunner, runner/probe-app/traffic tests and workflow. Observation/owner seams have no diff from prompt-15 `a5a02b74`; history/currentness and conservative request-time age remain distinct. Dirty Homing behavior was not used as authority. |
| ADS1115 | `235af3e8c722661a89a44766a4ccd903a654f6fe`, clean unchanged | `src/ADS1115.cpp` cancellation/result/configuration handling, owner-safe-poll example, basic tests and CI. Retained effect/identity is explicit; no sensor retry policy is imported. |
| INA228 | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b`, clean unchanged | `src/INA228.cpp` conversion-origin/configuration invalidation, destructive-read/freshness tests and CI. Observation time is not delivery time. |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0`, clean unchanged | Motor binding/retention and ADS cache publication with actual module/native consumers. Application availability remains separate from retained native results. |

## Delegation and maintainability

gpt-6.1-sol/high independently reviewed network events/siblings and supplied
facade and history regressions. Separate gpt-6-astra/high reviewers challenged
NMT serviceability/broadcast and PDO provenance/actual-map boundaries. Compiled
counterexamples preceded corrections. The lead checked findings against actual
source/diffs, owns central storage/integration and all Git operations. Reviewers
rechecked the corrected implementation; worker passes were followed by integrated
verification.

Reused ObservationTime, existing channel/node storage and per-fact ordering;
added only two transient timing values per channel, consumed at attachment.
No receiver, registry, recovery engine, new public operation or generated product
was added. Windows x86-64 SdoClient is now 10,528 bytes (previously 10,400);
ObservationTime is 32 bytes. Embedded sizing/timing remains unqualified.
User contract/Doxygen and CHANGELOG accompany the fixes. Scope remains complete;
later remapping, controller owner integration and physical qualification retain
their assigned prompts.

## Verification and handoff

- Strict GCC 15.1 C++11 build in `.build/audit15-17-native` and all ten focused
  network/NMT/PDO/SDO/proof suites pass. Network events: 1,200 checks;
  NMT services: 1,709 checks; unchanged PDO observations: 490 checks.
- Actual retained `canopen_service_fit` executable passes with no controller I/O.
- `python scripts/verify.py --mode quick --compiler gcc --build-dir .build/audit15-17-quick`:
  passed 30/30 contracts; run `20261029T215635501248Z-75440-7zas_68k`.
- `python scripts/verify.py --mode full --compiler gcc --build-dir .build/audit15-17-full`:
  passed 31/31 non-package contracts plus independent source/export/installed
  consumers, C++11/C++17/no-RTTI, 15 independent public headers and strict
  Doxygen 1.13.2; run `20261029T215635504252Z-55936-ifwi7c2c`.
- Repository/version/object freshness, planning/original-byte integrity and
  `git diff --check` pass: 137 retained artifacts, 99 EDS indexes, 202 typed
  entries, 53 manual items and 24 conflicts remain accounted for. Source and
  attribute bytes were not changed; staged/committed integrity is rechecked
  around delivery.

Local tools: Python 3.12.10, CMake/CTest 4.0.1, Ninja 1.12.1, GCC 15.1.0 and
Doxygen 1.13.2. Local Clang/Linux sanitizers are unavailable; the existing exact-
commit Linux GCC/Clang sanitizer jobs provide those gates remotely.

Scratch reproductions and retained logs stay in ignored `.build/audit1517_*`
and `.build/audit15-17-*` paths. Final commit/upstream comparison and required
automatic Windows/GCC/Clang CI are verified at handoff on the exact final SHA;
no older green run is substituted. No release, sibling mutation or hardware test.
