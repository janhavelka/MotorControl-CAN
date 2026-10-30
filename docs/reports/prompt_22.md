# Prompt 22 — control and tuning groups

Date: 2026-10-30. Execution identity: promptset version 2, existing branch
`promptset/motorcontrol-can`, upstream `origin/promptset/motorcontrol-can`.
Entry commit `3aa1817ff916f96951c8991f898d78ee30511f52` was clean, with no staged
changes. Fresh fetch showed 0 ahead/0 behind. The promptset base remains
`7878f7c`; no new branch or remote was created.

## Outcome and finite ownership

**Partial on documentary write evidence.** All 33 assigned raw read rows are
implemented and host tested. Explicit loop selection is implemented for the
reviewed 86C subset. All other assigned writes retain the precise admissible
domain, scaling, model or effect gates in [the tuning guide](../tuning.md).
No confirmed-but-unimplemented read or setting was found. A pending setting is
not advertised as unsupported hardware or closed by an unsupported stub.

The finite assignment is exactly `2500..2520:00`, plus the four existing manual
semantic accounting rows `drive_loop_mode`, `encoder_resolution`,
`current_settings`, `tuning_settings`. All retain owner 22. Current/holding is
not duplicated in 20. History/communication/PDO read/write accounting remains
with its existing 19/38 and 13/17/18 declarations; checker regressions freeze
that split. No unrelated native row was absorbed into this prompt.

## Source review and decisions

Reopened original EDS sections 2500..2520, lines 1559..1822; all 33 are scalar
U16 RW/PDO-mappable, with no default or limits. Independently rendered original
EN86/ZH86 covers, PDF p3 and p19–20. EN86 p19 ends at 2516; ZH86 p19 ends at
2505. Corrected per-row ledger page references accordingly. Original source
hashes, bytes and attributes are unchanged. Disposable renders are under
`build/p22-source-review/`; the immutable sources remain under reference/vendor.

Source-confirmed facts: loop selector 0 ineffective/1 open/2 closed; 2502 mA;
2503..2506 percent; 2507 ms. OL86-C has no documented encoder, so closed-loop
selection is rejected. CL86-C's 1000-line description is not a 2501 numeric
convention. Product-table 8 A is not a qualified peak-current bound. 250C has
no time unit, 2508 has no Boolean code set, and 251A..2520 have no reviewed
manual meaning. Missing accepted values/scales remain documentary gaps; no
operating range is inferred from the U16 representation.

Correction to prior policy: blanket `not_writable` for 2500 blocked a complete
documented selector domain. It now has a narrow conditional setting scope,
`explicit_loop_mode_86c`, while its generic descriptor stays qualification
gated. Missing physical activation timing does not invent a prerequisite or
block this explicit register transaction. Other 32 writes remain forbidden.
No new source conflict was silently resolved. No claim was physically measured.

## Implementation and independent review

- `Tuning.h` exposes `prepareControlRead`, `prepareTuningRead`, `getControl`,
  `getTuning` and `prepareControlSettings`. Five finite groups reuse existing
  caller-owned contexts and `nextDriver/advanceDriver`; no new runtime engine.
- Named native observations preserve all words and unknown values. Complete
  getters check kind, source, provenance, per-field age, skew and epochs before
  atomic output publication. Q20 upload effects remain unqualified.
- Explicit loop settings validate enum/model/fresh prior image and continuous
  owner reservation before yielding work. Only changed 2500 is written; 2501
  is read back unchanged. Actual configuration withdrawal precedes possible TX.
  ACK, matching new-epoch readback, uncertainty and physical activation remain
  separate. No hidden communication/persistent/enable/reset/stop action occurs.
- Independent review found a damaged settings context could substitute a
  descriptor or writable mask. Canonical descriptor, mask, cursor and enum/model
  guards now reject that work atomically. The review also caught the new
  header's missing install entry; source/install/export inventories now include it.
- Generator/checker guards admit exactly 33 new U16 read shapes and one
  conditional tuning setting. Generated products account 99 indexes, 202 typed
  entries and 53 manual functions; 95 implemented read shapes and 12 conditional
  setting rows do not imply 95 supported operations or physical qualification.
- README, CHANGELOG, source/API map, coverage, backlog, roadmap and package
  guide link the authoritative tuning contract. Existing stale driver-table
  language about already implemented position settings was corrected.

Delegation: `p22_sources` used `gpt-6-astra` high for independent rendered-source
and mutation-boundary review; `p22_siblings` used `gpt-6.1-sol` high for recurring
source review and the disjoint literal native tests; `p22_ledger` used
`gpt-6.1-sol` high for checker/generator/schema tests and independent coverage
review. Lead owned public contracts, canonical ledger, generated writes, build,
package integration and Git. All agents shared this branch; no worker Git,
sibling or hardware mutations occurred.

## Current sibling delta

All inspected checkouts are clean. RS/ADS/INA/FieldCore revisions match the
preceding 20/21 audit; MB85RC was refreshed for this seam.

| Checkout | HEAD | Actual relevant evidence |
| --- | --- | --- |
| MotorControl-RS | f812d7c1d364a03be86f02ae8f648dc8fca23d42 | `Tuning.h:47/52/54`, `ControlSettings.h:27/35/37`; implementations Tuning.cpp:55–95, ControlSettings.cpp:4–33; shared DriverSettings.cpp:316–428/534–574 candidate/provenance/model/intermediate-current validation; tuning_test.cpp:119–208, control_settings_test.cpp:84–163; ProbeApp.cpp:1611–1625 and ProbeConsole.cpp:2434–2449 typed callers. |
| ADS1115 | 235af3e8c722661a89a44766a4ccd903a654f6fe | ADS1115.cpp:273–278/308–328 whole-profile validation; test_basic.cpp:639/730/756; 02_owner_safe_poll/main.cpp:246–279 typed retained terminals. |
| INA228 | b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b | INA228.cpp:466–545 validates complete configuration/calibration before bind; test_basic.cpp:2523–2554 zero-I/O invalid candidate; bringup caller:1153–1158/2129–2133 public setter/calibration path. |
| MB85RC | 1bf7c293549b1038b2d8f3b7a8d655fa74201e6e | MB85RC.cpp:2017–2062 exact variant selection/invalidation and :820–874 policy; header:500–519 bulk partial-output exception; tests:857/917/1077/1152/1183/1297 and ESP-IDF caller:594–606. |
| FieldCore-node | 5ad9292dc8481cf08be9a61f662495a6474d1da0 | Rs485MotorModule.cpp:50–75/659–679 replacement/retained-work/reference invalidation, SettingsService.cpp:38–74 canonical full-candidate publication. Read only; no CAN tuning integration claimed. |

RS role names and atomic getters are retained. ESS ranges, motor/current
conventions, sensor retries and MB85RC's partial bulk-buffer exception are not
imported. No unchanged sibling suite was rerun to fill this report.

## Verification and handoff

Local tools: Python 3.12.10, GCC 15.1.0, CMake/CTest 4.0.1, Ninja 1.12.1,
Doxygen 1.13.2, clang-format 14.0.0. Windows local sanitizers/Clang are unavailable;
the existing Linux GCC/Clang sanitizer jobs remain required on the final commit.

| Local command / scope | Result |
| --- | --- |
| Strict C++11 build and focused tuning/settings/read/driver/I/O/object/native-consumer CTests | PASS, 7/7; tuning suite 7,453 checks |
| `python scripts/check_lichuan_ledger_test.py` | PASS, 73 tests |
| `python scripts/generate_lichuan_objects_test.py` | PASS, 42 tests, including malformed scope/type, deterministic generation, freshness and failure atomicity |
| `python scripts/verify.py --mode quick --build-dir .build/p22-quick` | PASS, 38/38 registered quick contracts |
| `python scripts/verify.py --mode full --build-dir .build/p22-full` | PASS, 39/39 contracts plus independent package checks |
| Full isolated package scope | PASS: source/export/install, strict independent headers, C++11/C++17/no-RTTI consumers, no Python/SDK/ledger/stack dependency; exact 22-header install inventory |
| `doxygen Doxyfile` and full extracted API inventory | PASS, 22 public headers; missing-doc/unresolved-reference negative tests remain active |
| Generated freshness, repository checks, promptset integrity and diff checks | PASS; 99 indexes, 202 typed entries, 53 manual functions, 137 protected artifacts |

Logs remain under `.build/p22-quick/20261030T132234503757Z-86676-iu73wzwg`
and `.build/p22-full/20261030T132220560872Z-56536-8l01zqjy`. They record entry
revision, dirty task paths, commands, tool versions, status and artifact paths;
the local result covers this working tree rather than pretending the entry HEAD
contains the new implementation. The initial quick run correctly failed on two
links to this report while it was still being written. Its logs are retained;
the completed report restores those links and the full integrated rerun passes.
Staged/committed provenance checks accompany the checkpoint.

Final commit/sync and exact remote job status are reported in the delivery
handoff after fresh fetch and final-commit CI inspection. Adding a header or
using an earlier green run is not remote verification.

Remaining work is precise: obtain admissible domain/scaling/effect evidence
for the 32 withheld writes and qualify the selected model/firmware physically.
Retain this prompt as Partial and return here when those facts arrive. Prompt
23 owns the complete application scheduler; its actual settings integration is
not claimed by the existing fake-service tests. No hardware traffic was issued.
