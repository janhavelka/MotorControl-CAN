# 02b — Profile registration, probe and `ds2` console family

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Runs after 02. Apply [canopen_shared 02](../canopen_shared/02_profile_integration_contract.md)
as written; claim seams S1, S2, S6 and S10 in the [canopen_shared table](../canopen_shared/README.md) and land
them, or extend them if another family claimed or landed them first. Seam S3's default deny lands in the S2 commit
(canopen_shared 02, S3 bullet), before any DS2 binding is admitted; only DS2's opt-in shape and its decision stay
with 04. The console items (6 and the console tests) stay BLOCKED until seam S4 lands (execution contract);
the API-level probe, refusals and owner tests do not wait. Commit tooling, core/profile and application/console
changes separately, each tested.

## Work

1. Declared model. Add `include/MotorControlCAN/profiles/lichuan_ds2/Model.h`: `Model` with the nine models of the
   [scope table](README.md#scope), `EncoderOption` (C17, R17, C23, R23; DS2C p4, IBL60 p5, ISL60 p6) and a pure
   `validateModelDeclaration`. Accept every combination the model code prints; record DS-D25 (IBL 17-bit claim
   versus C23/R23 codes) in the declaration's evidence instead of refusing. Each documented model fact used later
   (status dialect, speed-limit object, DIP node range, brake form, EDS scope) is a table lookup on `Model` with a
   ledger key, never a reply-derived value.
2. Seam S1. Append `DriveProfile::LICHUAN_DS2` with the next unused explicit nonzero value and a finite inventory
   row in `src/Discovery.cpp`: `profileId` `lichuan_ds2`, protocol CANopen, `nonChanging`, `exactModel` and
   `firmware` false, address bounds 1..127, probe object `1000:00`, scopes from the ledger. The documented DIP ranges 1..7 and 1..31 and `P0C-01` 1..255 (DS-D22)
   stay coverage facts; node IDs 128..255 are not CANopen node IDs and the binding validator refuses them.
3. Seam S2, as canopen_shared 02 "Profile resolution" defines it: `Endpoint.profile` holds the `DriveProfile` value
   and one pure lookup in `Discovery.h` also checks `Endpoint.protocol`; zero, unknown values and protocol mismatches
   refuse and nothing defaults to Lichuan. If S2 has not landed, its commit carries the Lichuan migration of that
   section. Every DS2 preparation and every Lichuan C preparation refuses a binding of the other profile before
   touching output or yielding work; test both directions, the old label 41, unknown values, a protocol mismatch,
   rebinding while a result is retained and that nothing is sent. Selection performs no I/O.
   The same commit carries S3's default deny if no family landed it: through the S2 lookup a binding with no
   configured stop shape gets `INVALID` and no TX from `admitBestEffortStop`. Test it directly on an unconfigured DS2
   binding, an unresolved DS2 route after a timeout and a rebound channel, each with no TX; Lichuan C's best-effort
   tests and DEC-2 behavior stay unchanged. If another family landed S2 and the deny, add these DS2 cases to its tests.
4. Probe. `prepareProbe` performs one explicit upload of `1000:00` through the real owner and `SdoClient`. It proves a
   responding CANopen server only, never DS2, model or firmware, and its read effect stays unknown (no `DS-DEC-1`
   yet). Abort versus malformed reply, timeout and a late identical reply follow the existing service evidence.
5. Seam S10, reviewed `SdoReuse` per binding. The owner's default markers (`1000:00`, `1018:01`-`03`) rest on
   Lichuan C DEC-1. S10 makes a non-Lichuan binding's effective default `NONE` through the S2 lookup; DS2 bindings
   keep `NONE` until a reviewed marker decision, separate from `DS-DEC-1`'s polling list (03), covers marker objects
   and their read effects. Record the choice and test, on a
   two-family owner without explicit configuration, that a timeout and then a late reply on the DS2 route send no
   marker, set no `acceptedCorrelation` and replay nothing, while the Lichuan route keeps its default marker.
6. Console and JSONL (seam S6). One `commands[]` entry `ds2` in `examples/common/ProbeConsole.cpp`; grammar and
   formatting in `examples/common/LichuanDs2Console.{h,cpp}`; `ds2 probe <node>` and `ds2 model <axis> <model>
   <encoder>` (local declaration, no I/O). Existing top-level verbs keep their Lichuan C meaning; there is no selected
   profile mode. JSONL `profile` follows S6's per-record rule (`null` without a resolved target, the target's
   `profileId` on targeted replies and refusals, the admission `profileId` on an operation's records and retained
   result, also after a rebind), revised with `ProbeConsoleText.h`, `scripts/console_jsonl.py`,
   `scripts/s2_console.py` and their tests; `profile list`, `caps` and `help` come from the inventory and claim no
   detection. BLOCKED until S4 lands.
7. Fake server. `test/support/LichuanDs2Drive.h` answers only documented objects, labelled synthetic, with both
   identity claim variants (manual 0x0100/0x0002 and the EDS values of each group) selectable per test, so no test
   bakes in one unresolved claim.
8. Multi-profile consumers. By default a Lichuan C axis and a DS2 axis run on separate interfaces (owner and
   transport each) with separate caches, results and stops. On one interface, negative tests only: reservations
   conflict on the same bus and address whatever the profile label, foreign refusal sends nothing, and the
   shared-interface positive test waits for 11's same-domain analysis and its enforced exclusions;
   `test/multidrive_contract_test.cpp` still passes. 02 landed the install rule and package entries with the first
   headers; this stage extends them only for the files it adds (`Model.h`, `Reads.h`, `src/profiles/lichuan_ds2/`
   sources in `MOTORCONTROLCAN_SOURCES`, the install file list, `test/package_test.py` `HEADERS` and archive set) and
   adds C++11 consumers of core only, Lichuan C only, DS2 only and both; `library.json` and `check_repository.py`
   `EXPORTS` stay unchanged.
9. Operation map. Add the `lichuan_ds2` family to `docs/reference/lichuan_ds2_operations.json` with the probe's
   function, tests and console row, so the checker proves the route.

## Deliver

`Model.h`, the probe in `Reads.h` and `src/profiles/lichuan_ds2/Reads.cpp`, the inventory row,
`LichuanDs2Console.{h,cpp}`, `LichuanDs2Drive.h`, `test/lichuan_ds2_discovery_test.cpp`,
`test/lichuan_ds2_owner_test.cpp`, `test/lichuan_ds2_console_test.cpp`, Python response validation for the new
profile, package and consumer entries, Doxygen for every public declaration, README scope, CHANGELOG `Unreleased`
(landed code only), `docs/profile_contract.md`, `docs/cli_contract.md`, `docs/api_cli_mapping.md`,
`docs/source_map.md`, coverage, the canopen_shared README seam table (claims and landed commits) and
`docs/reports/lichuan_ds2_canopen_02b.md`.

## Acceptance

The probe runs through the real owner, `SdoClient` and fake server in API tests, and in console tests with identical
frames once S4 has landed (otherwise the console items are BLOCKED and the stage is PARTIAL). Foreign-profile
refusals send nothing in both directions. `admitBestEffortStop` returns `INVALID` with no TX for every DS2 binding
before 04 opts in its shape. JSONL records follow S6: `lichuan_ds2` only on records that target or were
admitted for a DS2 binding, `null` without a resolved target, and Lichuan C targeted records otherwise unchanged. All consumers build in C++11 with no catalogue dependency. Checker
negative tests still fail as intended. `python scripts/verify.py --mode quick` and `--mode full`, the Arduino and IDF
firmware gates of the contract, and required CI on the exact final pushed commit pass. Hardware NOT TESTED; no DS2
frame reaches COM15. Commit and push, then hand off with the seam rows updated.
